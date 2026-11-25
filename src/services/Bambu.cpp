// The printer's MQTT connection (see Bambu.h).
#include "Bambu.h"

#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "LanAddress.h"

RTC_DATA_ATTR bool isPrinting = false;
RTC_DATA_ATTR bool previousIsPrinting = false;
RTC_DATA_ATTR bool printViewOnScreen = false;
RTC_DATA_ATTR static time_t printFinishedAt = 0;  // when the last print ended (0 = none recently)
bool bambuAuthRejected = false;

namespace {

WiFiClientSecure tls;
PubSubClient mqtt(tls);
bool gotReport = false;  // a full report arrived in this wake

constexpr unsigned long REPORT_WAIT_MS = 6000;  // the answer to "pushall"
constexpr uint16_t MQTT_BUFFER = 32768;         // a full report with an AMS exceeds 20 KB

void onMessage(char*, byte* payload, unsigned int length) {
  if (gotReport) return;
  JsonDocument doc;
  if (deserializeJson(doc, payload, length)) {
    DBG(F("[Bambu] JSON parse failed"));
    return;
  }
  if (!bambu::readReport(doc["print"], printer)) return;  // a small update, not the full report
  isPrinting = bambu::isPrinting(printer);
  gotReport = true;
  DBGF("[Bambu] %s, %d %%", printer.gcode_state, printer.percent);
}

// Up to two attempts; a refused access code or an unreachable printer ends it at once.
bool connect() {
  for (int attempt = 1; attempt <= 2; attempt++) {
    if (mqtt.connect("ESP32_Bambu", "bblp", mqtt_pass)) {
      char topic[96];
      snprintf(topic, sizeof(topic), "device/%s/report", mqtt_sn);
      mqtt.subscribe(topic);
      snprintf(topic, sizeof(topic), "device/%s/request", mqtt_sn);
      mqtt.publish(topic, "{\"pushing\":{\"sequence_id\":\"0\",\"command\":\"pushall\"}}");  // the full status, now
      return true;
    }
    int state = mqtt.state();
    DBGF("[Bambu] MQTT failed rc=%d (try %d/2)", state, attempt);
    if (state == MQTT_CONNECT_BAD_CREDENTIALS || state == MQTT_CONNECT_UNAUTHORIZED) {
      DBG(F("[Bambu] Access code rejected: check the LAN access code"));
      bambuAuthRejected = true;
      return false;
    }
    if (state == MQTT_CONNECT_FAILED) {  // no TCP/TLS at all: off or unreachable; the wake plan backs off
      DBG(F("[Bambu] Printer unreachable (switched off?)"));
      return false;
    }
    delay(1000);
  }
  return false;
}

}  // namespace

bool showPrintView() {
  return isPrinting || (printFinishedAt != 0 && time(nullptr) - printFinishedAt < PRINT_FINISHED_HOLD_S);
}

bool fetchBambu(LayoutItem*) {
  DBGF("[Bambu] access code %s, serial %s, address %s:%d", mqtt_pass[0] ? "set" : "MISSING", mqtt_sn, mqtt_ip, mqtt_port);
  if (!mqtt_pass[0] || !mqtt_sn[0] || !mqtt_ip[0] || !mqtt_port) return false;
  previousIsPrinting = isPrinting;
  bambuAuthRejected = false;
  // The printer has a self-signed certificate: TLS without a check, so only inside the local
  // network (the one exception, LanAddress.h). A name is resolved here once and must be local; the
  // connection goes to that address.
  IPAddress ip;
  if (!ip.fromString(mqtt_ip) && !WiFi.hostByName(mqtt_ip, ip)) {
    DBGF("[Bambu] can't resolve %s", mqtt_ip);
    return false;
  }
  if (!isPrivateIPv4(ip[0], ip[1], ip[2], ip[3])) {
    DBGF("[Bambu] %s is %s, not in the local network: no connection without a certificate check", mqtt_ip,
         ip.toString().c_str());
    return false;
  }
  tls.setInsecure();
  tls.setConnectionTimeout(5000);  // a switched-off printer mustn't keep the radio on (default: 30 s)
  tls.setHandshakeTimeout(8);
  mqtt.setSocketTimeout(5);
  mqtt.setBufferSize(MQTT_BUFFER);
  mqtt.setServer(ip, mqtt_port);
  mqtt.setCallback(onMessage);
  gotReport = false;
  bool connected = connect();
  if (connected) {
    unsigned long start = millis();
    while (!gotReport && millis() - start < REPORT_WAIT_MS) {
      mqtt.loop();
      delay(10);
    }
    DBGF("[Bambu] full report %s after %lu ms", gotReport ? "received" : "NOT received", millis() - start);
    if (gotReport) {
      if (isPrinting) printFinishedAt = 0;
      else if (previousIsPrinting) printFinishedAt = time(nullptr);  // the print just ended
    }
  }
  mqtt.disconnect();
  tls.stop();
  return connected && gotReport;
}
