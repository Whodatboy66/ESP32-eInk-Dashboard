#include "Bambu.h"
#include "LanAddress.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <math.h>

PrinterStatus printer;

RTC_DATA_ATTR bool isPrinting=false;
RTC_DATA_ATTR bool previousIsPrinting=false;
RTC_DATA_ATTR time_t printFinishedAt = 0;     // when the last print ended (0 = no recent print)
RTC_DATA_ATTR bool printViewOnScreen = false; // the printer status (not MakerWorld) is currently drawn

bool showPrintView() {
  if (isPrinting) return true;
  return printFinishedAt != 0 && time(nullptr) - printFinishedAt < PRINT_FINISHED_HOLD_S;
}
// --- MQTT ---
// This module keeps only a short-lived MQTT connection per wake cycle:
// connect → subscribe → process a few seconds of messages → disconnect.
WiFiClientSecure wifiClient;
PubSubClient client(wifiClient);

// Set once a full status report (one that carries gcode_state) has been parsed this wake.
static bool gotReport = false;
bool bambuAuthRejected = false;


void callback(char* topic, byte* payload, unsigned int length) {

  JsonDocument doc;  // ArduinoJson 7: StaticJsonDocument<N> was only an alias
  // doc clear();

  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    DBG( F("[Bambu] JSON parse failed") );
    return;
  }

  JsonObject print = doc["print"];
  if (!print) return;

  // Idle P-series printers mostly send small deltas; only a full report (the pushall answer)
  // carries gcode_state. Ignore deltas so they don't overwrite the full status with zeros.
  if (gotReport || print["gcode_state"].isNull()) return;

  // -------- Progress --------
  printer.layer_num = print["3D"]["layer_num"] | 0;
  printer.total_layer_num = print["3D"]["total_layer_num"] | 0;
  printer.percent = print["percent"].as<int>() | 0;
  printer.remainTime = print["mc_remaining_time"].as<int>() | 0;

  printer.error = print["err"].as<int>() | 0;
  
  copyJsonString(printer.gcode_state, sizeof(printer.gcode_state), print["gcode_state"]);

  // Persist printing state across deep sleep so we can refresh more frequently mid-print.
  logPrintln( printer.gcode_state );
  if( ( printer.gcode_state && (strcmp(printer.gcode_state,"RUNNING")==0||  strcmp(printer.gcode_state,"PREPARE")==0 ) || strcmp(printer.gcode_state,"PAUSE")   == 0 ) || ( printer.percent <100 && printer.percent >0 ) )
    isPrinting=true;
  else
    isPrinting=false;

  // -------- Temperatures --------
  printer.bed_temp = print["bed_temper"] | 0.0;
  printer.nozzle_temp = print["nozzle_temper"] | 0.0;

  // -------- Fans --------
  printer.fan_speed = print["fan"].as<int>();
  printer.cooling_fan_speed = print["cooling_fan_speed"].as<int>();

  printer.big_fan1_speed = print["big_fan1_speed"].as<int>();
  printer.big_fan2_speed = print["big_fan2_speed"].as<int>();
  
  // -------- Job --------
  const char* name = print["subtask_name"] | "";
  strncpy(printer.job_name, name, NAME_LEN);
  printer.job_name[NAME_LEN - 1] = '\0';

  const char* state = print["job"]["cur_stage"]["state"] | "";
  strncpy(printer.stage_state, state, sizeof(printer.stage_state));
  printer.stage_state[sizeof(printer.stage_state) - 1] = '\0';

  // -------- AMS --------
  printer.amsCount = 0;

  JsonArray amsArray = print["ams"]["ams"];

  if (!amsArray.isNull()) {

    for (JsonObject amsObj : amsArray) {
      
      if (printer.amsCount >= MAX_AMS) break;

      AMS &a = printer.ams[printer.amsCount++];
      a.trayCount = 0;

      const char* amsId = amsObj["id"] | "";
      strncpy(a.id, amsId, ID_LEN);
      a.id[ID_LEN - 1] = '\0';


      a.temp=amsObj["temp"].as<float>();
      a.humidity=amsObj["humidity_raw"].as<int>();


      JsonArray trayArray = amsObj["tray"];

      if (!trayArray.isNull()) {

        for (JsonObject trayObj : trayArray) {

          if (a.trayCount >= MAX_TRAYS) break;

          Tray &t = a.trays[a.trayCount++];

          const char* trayId = trayObj["id"] | "";
          strncpy(t.id, trayId, ID_LEN);
          t.id[ID_LEN - 1] = '\0';

          t.remain = trayObj["remain"] | 0;

          const char* weight = trayObj["weight"] | "";
          strncpy(t.weight, weight, WEIGHT_LEN);
          t.weight[WEIGHT_LEN - 1] = '\0';

          copyJsonString(t.tray_type, sizeof(t.tray_type), trayObj["tray_type"]);

          t.state=trayObj["state"].as<int>();

        }
      }
    }
  }
  gotReport = true;
  DBG(F("[Bambu] Retrived the informations"));
}

// File-local helper: establish MQTT connection + subscribe to the report topic.
static void connectMqtt( LayoutItem * infoBambu) {
  uint8_t retries = 0;
  while (!client.connected() && retries < 2 ) {
      DBG( F("[Bambu]: Connecting to MQTT...") );
      if (client.connect("ESP32_Bambu", "bblp", mqtt_pass)) {
          DBG( F("[Bambu]: connected") );
          char reportTopic[96];
          snprintf(reportTopic, sizeof(reportTopic), "device/%s/report", mqtt_sn);
          client.subscribe(reportTopic);

          // Ask the printer for its full status right away.
          char requestTopic[96];
          snprintf(requestTopic, sizeof(requestTopic), "device/%s/request", mqtt_sn);
          client.publish(requestTopic, "{\"pushing\":{\"sequence_id\":\"0\",\"command\":\"pushall\"}}");
      } else {
        DBGF("[Bambu]: MQTT fail rc=%d try %d/2\n", client.state(), retries+1);
        // rc 4/5 = bad credentials or not authorized (wrong LAN access code): retrying won't help.
        if (client.state() == MQTT_CONNECT_BAD_CREDENTIALS || client.state() == MQTT_CONNECT_UNAUTHORIZED) {
          DBG(F("[Bambu] Access code rejected - check the LAN access code in setup"));
          bambuAuthRejected = true;
          break;
        }
        retries++;
        // No TCP/TLS connection at all: the printer is off or unreachable. No retry here: the wake
        // plan asks again after a backoff (10 min up to 1 h).
        if (client.state() == MQTT_CONNECT_FAILED) {
          DBG(F("[Bambu] Printer unreachable (switched off?)"));
          break;
        }
        delay(1000);
      }
  }
  if (!client.connected())
    DBG(F("[Bambu] MQTT gave up"));

}

bool fetchBambu(  LayoutItem* infoBambu  )
{
  DBG( F("[Bambu] Fetching"));
    // The access code is a secret: only say whether it is set.
    DBGF("[Bambu] Access code %s MQTT_SN %s MQTT_IP %s MQTT_PORT %d", mqtt_pass[0] ? "set" : "MISSING", mqtt_sn, mqtt_ip, mqtt_port );

  if (mqtt_pass[0] == '\0' || mqtt_sn[0] == '\0' || mqtt_ip[0] == '\0' || mqtt_port == 0)
  {
    DBG("[Bambu]: Missing some parametes such pass, sn, ip or port");
    return false;
  }

  previousIsPrinting=isPrinting;
  // The printer only has a self-signed certificate: TLS without a certificate check, and only
  // inside the local network (the one documented exception, see LanAddress.h). A name from the
  // router's DNS is resolved here and must give a local address; the connection then goes to that
  // address (not to a second lookup of the name).
  IPAddress printerIp;
  if (!printerIp.fromString(mqtt_ip) && !WiFi.hostByName(mqtt_ip, printerIp)) {
    DBGF("[Bambu] can't resolve %s", mqtt_ip);
    return false;
  }
  if (!isPrivateIPv4(printerIp[0], printerIp[1], printerIp[2], printerIp[3])) {
    DBGF("[Bambu] %s is %s, not in the local network: no connection without a certificate check",
         mqtt_ip, printerIp.toString().c_str());
    return false;
  }
  wifiClient.setInsecure();
  // A switched-off printer must not keep the radio on for long: short connect and TLS timeouts
  // (the TLS client's default connect timeout is 30 s: each failed attempt took 30 s in the
  // diagnostics log of 2026-11-19).
  wifiClient.setConnectionTimeout(5000);
  wifiClient.setHandshakeTimeout(8);
  client.setSocketTimeout(5);
  client.setBufferSize(32768);  // a full pushall report can exceed 20 KB with an AMS attached
  gotReport = false;
  DBGF("[Bambu]: IP %s Port %d\n", mqtt_ip, mqtt_port );
  client.setServer(printerIp, mqtt_port);
  client.setCallback(callback);
  connectMqtt( infoBambu );

  bool connected = client.connected();
  if (connected) {
    unsigned long start = millis();
    while (!gotReport && millis() - start < 6000) {  // wait up to 6 seconds for the pushall answer
      client.loop();
      delay(10);
    }
    DBGF("[Bambu] Full report %s after %lu ms", gotReport ? "received" : "NOT received", millis() - start);
    if (gotReport) {
      if (isPrinting) printFinishedAt = 0;
      else if (previousIsPrinting) printFinishedAt = time(nullptr);  // print just ended
    }
  } else {
    DBG(F("[Bambu]: MQTT never connected, skipping loop"));
  }

  client.disconnect();   // <-- CLOSE MQTT connection
  wifiClient.stop();
  return connected && gotReport;
}

