// The setup app's JSON API (docs/SETUP_API.md): state, settings, layout, preview, checks, personal
// data (stage 4), device actions. Served in setup mode (hotspot) and in edit mode on the home
// network (session from the code on the screen).
#include "Api.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WebServer.h>

#include <memory>
#include <new>
#include <WiFi.h>
#include <new>

#include "../../app.h"
#include "../core/AppState.h"
#include "../core/ConfigManager.h"
#include "../core/ConfigValidation.h"
#include "../core/LogBuffer.h"
#include "../core/PowerPlan.h"
#include "../core/Logger.h"
#include "../core/Problems.h"
#include "../core/ServiceState.h"
#include "../core/Timezone.h"
#include "../hardware/BatteryMonitor.h"
#include "../hardware/PinMapping.h"
#include "../hardware/TouchController.h"
#include "../layout/BoxLayout.h"
#include "../personal/UserStore.h"
#include "../services/Fetchers.h"
#include "../ui/Canvas.h"
#include "../update/OtaFlow.h"
#include "../widgets/Dashboard.h"
#include "../widgets/Registry.h"
#include "SetupMode.h"

using layouts::WidgetId;

namespace {

constexpr size_t MAX_BODY = 8 * 1024;
WebServer* srv = nullptr;

// ---------- answers ----------
void sendJson(int code, const JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  srv->sendHeader("Cache-Control", "no-store");
  srv->send(code, "application/json", out);
}

void sendError(int code, const char* message) {
  JsonDocument doc;
  doc["ok"] = false;
  doc["error"] = message;
  sendJson(code, doc);
}

void sendOk(const char* removed = nullptr) {
  JsonDocument doc;
  doc["ok"] = true;
  if (removed && removed[0]) doc["removed"] = removed;
  sendJson(200, doc);
}

void sendOffline() {
  JsonDocument doc;
  doc["ok"] = false;
  doc["offline"] = true;
  doc["error"] = "The device has no internet connection: save or test your Wi-Fi first.";
  sendJson(200, doc);
}

// Every request. Hotspot: only from the hotspot itself (after a Wi-Fi test the device is also in
// the home network, which must not reach the setup without the hotspot password) and for its
// address (no DNS rebinding). Home network: our own address, and a session except for the login.
// Only requests that pass keep setup mode open.
bool allow(bool needSession = true) {
  if (setupKind == SetupKind::Ap) {
    if (!setupFromHotspot()) {
      sendError(403, "Only from the setup hotspot");
      return false;
    }
  } else if (setupKind == SetupKind::Home) {
    if (!setupHostOk()) {
      sendError(403, "Wrong address");
      return false;
    }
    if (needSession && !setupSessionOk()) {
      JsonDocument doc;
      doc["ok"] = false;
      doc["login"] = true;
      sendJson(401, doc);
      return false;
    }
    if (!needSession) return true;  // the login notes activity only when it succeeds
  }
  setupNoteActivity();
  return true;
}

// POST / PUT: JSON only (a page from another site can't send that without a preflight, which the
// device never answers).
bool jsonType() {
  if (srv->header("Content-Type").startsWith("application/json")) return true;
  sendError(415, "JSON expected");
  return false;
}

// The body of a POST / PUT, at most MAX_BODY. Copied into `body` (WebServer::arg() returns a
// temporary String: a pointer into it would dangle).
bool readBody(String& body) {
  if (!jsonType()) return false;
  if (!srv->hasArg("plain")) {
    sendError(400, "Empty request");
    return false;
  }
  body = srv->arg("plain");
  if (body.length() > MAX_BODY) {
    sendError(413, "Too much data");
    return false;
  }
  return true;
}

bool readJson(JsonDocument& doc) {
  String body;
  if (!readBody(body)) return false;
  if (deserializeJson(doc, body) || !doc.is<JsonObject>()) {  // strings are copied into doc
    sendError(400, "The request could not be read.");
    return false;
  }
  return true;
}

// ---------- state ----------
struct ProblemText {
  uint16_t flag;
  const char* key;
  const char* text;
  const char* fix;
};
const ProblemText PROBLEM_TEXTS[] = {
    {PROBLEM_WIFI, "wifi", "The Wi-Fi network can't be reached", "Check the network and password on the Device tab."},
    {PROBLEM_TIME, "time", "The clock could not be set from the internet", "It usually fixes itself; check the Wi-Fi if it stays."},
    {PROBLEM_WEATHER, "weather", "The weather can't be loaded", "Check the place on the Data tab."},
    {PROBLEM_STOCKS, "stocks", "Some stock prices can't be loaded", "Check the stock symbols on the Data tab."},
    {PROBLEM_CALENDAR, "calendar", "A calendar can't be read", "Check the calendar links on the Data tab."},
    {PROBLEM_PRINTER, "printer", "The printer doesn't answer", "Is it switched on? Check its address on the Data tab."},
    {PROBLEM_PRINTER_CODE, "printer_code", "The printer refused the access code", "Copy the access code from the printer's screen."},
    {PROBLEM_MAKERWORLD, "makerworld", "MakerWorld can't be loaded", "Check the MakerWorld user ID on the Data tab."},
    {PROBLEM_TRACKING, "tracking", "Parcel tracking can't be loaded", "Check the tracking key on the Data tab."},
    {PROBLEM_FLIGHT, "flight", "The flight status can't be loaded", "Check the flight key on the Data tab."},
    {PROBLEM_BIRTHDAYS, "birthdays", "The birthday calendar can't be read", "Check the birthday calendar link on the Data tab."},
};

void secret(JsonObject o, const char* key, const char* value) { o[key].to<JsonObject>()["set"] = value[0] != '\0'; }

void secretLink(JsonObject o, const char* value) {
  o["set"] = value[0] != '\0';
  if (value[0]) {  // the host only: the rest of a private link works like a password
    const char* host = strstr(value, "://");
    host = host ? host + 3 : value;
    o["hint"] = String(host).substring(0, strcspn(host, "/:?"));
  }
}

void addBoxes(JsonArray arr, const layouts::BoxLayout& l) {
  for (int i = 0; i < l.count; i++) {
    JsonObject b = arr.add<JsonObject>();
    b["c"] = l.boxes[i].c;
    b["r"] = l.boxes[i].r;
    b["w"] = l.boxes[i].w;
    b["h"] = l.boxes[i].h;
    if (l.boxes[i].widget < 0) b["widget"] = nullptr;
    else b["widget"] = layouts::WIDGET_KEYS[l.boxes[i].widget];
  }
}

void handleState() {
  if (!allow()) return;
  JsonDocument doc;
  JsonObject dev = doc["device"].to<JsonObject>();
  dev["name"] = device_name[0] ? device_name : "Dashboard";
  dev["version"] = FW_VERSION;
  dev["build"] = FW_BUILD;
  dev["variant"] = FW_VARIANT;
  dev["mode"] = setupKind == SetupKind::Home ? "home" : "ap";
  dev["firstRun"] = wifi_ssid[0] == '\0';
  // The last reading with Wi-Fi off (the radio makes the voltage sag now); live if there's none.
  {
    float qv;
    time_t qat;
    if (hasBatteryPin() && batteryQuiet(qv, qat, 3 * 3600)) {
      dev["battery"] = batteryReadingPlausible(qv) ? batteryPercent(qv) : -1;
      dev["batteryAt"] = (long)qat;
    } else {
      dev["battery"] = batteryPercentNow();
      dev["batteryAt"] = nullptr;
    }
  }
  dev["usb"] = false;  // no USB detection yet (stage 6)
  JsonObject wifi = dev["wifi"].to<JsonObject>();
  wifi["ssid"] = wifi_ssid;
  wifi["connected"] = setupOnline();
  wifi["rssi"] = setupOnline() ? WiFi.RSSI() : 0;
  time_t now = time(nullptr);
  dev["time"] = now > 1700000000 ? (long)now : 0;
  JsonArray probs = dev["problems"].to<JsonArray>();
  for (const ProblemText& p : PROBLEM_TEXTS) {
    if (!(problems & p.flag)) continue;
    JsonObject o = probs.add<JsonObject>();
    o["key"] = p.key;
    o["text"] = p.text;
    o["fix"] = p.fix;
  }
  {
    Preferences prefs;
    prefs.begin("config", true);
    String result = prefs.getString("ota_result", "");
    prefs.end();
    if (result.length()) dev["lastUpdate"] = result;
    else dev["lastUpdate"] = nullptr;
  }
  if (otaAvailBuild > FW_BUILD) dev["updateAvailable"] = otaAvailVersion;
  else dev["updateAvailable"] = nullptr;

  layouts::Result current = loadLayout();
  if (current.fellBack) dev["layoutFallback"] = "The saved layout could not be used: Classic is shown.";
  else dev["layoutFallback"] = nullptr;

  JsonObject s = doc["settings"].to<JsonObject>();
  s["device_name"] = device_name;
  s["wifi_ssid"] = wifi_ssid;
  secret(s, "wifi_pass", wifi_pass);
  s["timezone"] = device_timezone;
  s["weather_loc"] = weather_location;
  s["stocks"] = stock_symbols;
  JsonArray ics = s["ics"].to<JsonArray>();
  for (int i = 0; i < ICS_MAX_FEEDS; i++) secretLink(ics.add<JsonObject>(), ics_urls[i]);
  secretLink(s["bday_url"].to<JsonObject>(), bday_url);
  secret(s, "pkge_key", pkge_api_key);
  secret(s, "adb_key", aerodatabox_key);
  s["mqtt_ip"] = mqtt_ip;
  s["mqtt_sn"] = mqtt_sn;
  secret(s, "mqtt_pass", mqtt_pass);
  s["mqtt_port"] = mqtt_port;
  s["printer_name"] = printer_name;
  s["makerworld_uid"] = makerworld_uid;
  s["ota_auto"] = otaAutoInstall;
  s["ota_beta"] = otaBeta;
  s["clock_24h"] = clock_24h;
  s["clock_interval"] = clock_interval;
  s["quiet_enabled"] = quiet_on;
  char hm[8];
  power::formatHm(quiet_from, hm, sizeof(hm));
  s["quiet_from"] = hm;
  power::formatHm(quiet_to, hm, sizeof(hm));
  s["quiet_to"] = hm;
  s["battery_mah"] = battery_mah;
  s["pin_preset"] = pinPresetValue(activePinPreset);
  JsonObject pins = s["pins"].to<JsonObject>();
  pins["epd_cs"] = customPins.epdCs;
  pins["epd_dc"] = customPins.epdDc;
  pins["epd_rst"] = customPins.epdRst;
  pins["epd_busy"] = customPins.epdBusy;
  pins["epd_sck"] = customPins.epdSck;
  pins["epd_mosi"] = customPins.epdMosi;
  pins["display_power"] = customPins.displayPower;
  pins["battery"] = customPins.battery;
  pins["demo_button"] = customPins.demoButton;

  JsonArray presets = doc["pinPresets"].to<JsonArray>();
  const PinPreset all[] = {PinPreset::Esp32C6Default, PinPreset::Esp32C6SuperMini, PinPreset::Esp32Waveshare,
                           PinPreset::Esp32Default, PinPreset::Custom};
  for (PinPreset p : all) {
    JsonObject o = presets.add<JsonObject>();
    o["key"] = pinPresetValue(p);
    o["name"] = pinPresetLabel(p);
  }

  JsonObject lay = doc["layout"].to<JsonObject>();
  lay["template"] = layout_template;
  layouts::BoxLayout boxes;
  bool custom = strcmp(layout_template, "custom") == 0 && layouts::parseBoxes(layout_boxes, boxes);
  const layouts::Template& tpl = layouts::templateByKey(layout_template);
  if (!custom && !tpl.pixels) boxes = layouts::boxesFromResult(layouts::buildLayout(tpl, enabledWidgets()));
  if (!custom && tpl.pixels) {
    boxes = layouts::BoxLayout{};
    lay["template"] = custom ? "custom" : tpl.key;
  }
  lay["statusBar"] = boxes.count ? boxes.statusBar : true;
  addBoxes(lay["boxes"].to<JsonArray>(), boxes);
  JsonArray show = lay["show"].to<JsonArray>();
  for (int w = 2; w < layouts::WIDGET_COUNT; w++)
    if (widget_show_mask & layouts::widgetBit(static_cast<WidgetId>(w))) show.add(layouts::WIDGET_KEYS[w]);

  uint16_t configured = configuredWidgets();
  JsonArray widgets = doc["widgets"].to<JsonArray>();
  for (int w = 1; w < layouts::WIDGET_COUNT; w++) {
    WidgetId id = static_cast<WidgetId>(w);
    JsonObject o = widgets.add<JsonObject>();
    o["key"] = layouts::WIDGET_KEYS[w];
    o["name"] = widgetInfo(id).name;
    o["minW"] = layouts::limitsOf(id).minCols;
    o["minH"] = layouts::limitsOf(id).minRows;
    bool ok = configured & layouts::widgetBit(id);
    o["configured"] = ok;
    o["hint"] = ok ? "" : widgetInfo(id).hint;
  }

  JsonArray structures = doc["structures"].to<JsonArray>();
  for (int t = 0; t < layouts::TEMPLATE_COUNT; t++) {
    const layouts::Template& tt = layouts::TEMPLATES[t];
    if (tt.pixels) continue;
    JsonObject o = structures.add<JsonObject>();
    o["key"] = tt.key;
    o["name"] = tt.name;
    addBoxes(o["boxes"].to<JsonArray>(),
             layouts::boxesFromResult(layouts::buildLayout(tt, layouts::ALL_WIDGETS)));
  }

  // Power (stage 6): the note shown now and the battery life estimate for each clock choice.
  JsonObject pw = doc["power"].to<JsonObject>();
  {
    // Setup / edit mode doesn't run the wake's power decision: the same decision for "now".
    power::Inputs pin;
    time_t t = time(nullptr);
    struct tm lt;
    pin.minuteOfDay = (t > 1700000000 && localtime_r(&t, &lt)) ? lt.tm_hour * 60 + lt.tm_min : -1;
    pin.clockInterval = clock_interval;
    pin.quietOn = quiet_on;
    pin.quietFrom = quiet_from;
    pin.quietTo = quiet_to;
    float qv;
    time_t qat;
    pin.batteryPct = (hasBatteryPin() && batteryQuiet(qv, qat, 3 * 3600))
                         ? (batteryReadingPlausible(qv) ? batteryPercent(qv) : -1)
                         : batteryPercentNow();
    pin.usb = usbPowered();
    pin.buttonWake = true;  // the screen shows the dashboard now
    power::Plan pp = power::decide(pin);
    if (pp.night) pp.mode = power::Mode::Night;
    char note[48];
    power::note(pp, clock_interval, quiet_to, note, sizeof(note));
    pw["note"] = note;
    pw["saver"] = pp.saverOn;
    pw["night"] = pp.night;
    pw["stepMin"] = pp.stepMin;
  }
  pw["estimateNote"] = "Estimate from the first measurements (2026-11-20); real current measurements follow.";
  JsonArray est = pw["estimates"].to<JsonArray>();
  double qh = power::quietHours(quiet_on, quiet_from, quiet_to);
  double base = power::estimateDays(1, qh, battery_mah);
  for (int c : power::CLOCK_CHOICES) {
    JsonObject e = est.add<JsonObject>();
    double d = power::estimateDays(c, qh, battery_mah);
    e["interval"] = c;
    e["days"] = (int)(d + 0.5);
    e["gainPercent"] = (int)((d / base - 1.0) * 100.0 + 0.5);
  }
  pw["daysWithoutQuietHours"] = (int)(power::estimateDays(clock_interval, 0.0, battery_mah) + 0.5);
  pw["daysWithQuietHours"] = (int)(power::estimateDays(clock_interval, power::quietHours(true, quiet_from, quiet_to), battery_mah) + 0.5);

  JsonObject grid = doc["grid"].to<JsonObject>();
  grid["cols"] = layouts::GRID_COLS;
  grid["rows"] = layouts::GRID_ROWS;
  grid["cellW"] = layouts::CELL_W;
  grid["cellH"] = layouts::CELL_H;
  grid["statusH"] = layouts::STATUS_H;
  sendJson(200, doc);
}

// ---------- settings ----------
// A secret field: string = new value, null = remove, missing = keep. Returns false if the value
// is not a string / null.
enum class SecretOp { Keep, Set, Clear, Bad };
SecretOp secretOp(JsonVariantConst v) {
  if (v.isUnbound()) return SecretOp::Keep;
  if (v.isNull()) return SecretOp::Clear;
  if (v.is<const char*>()) return SecretOp::Set;
  return SecretOp::Bad;
}

bool fits(const char* s, size_t cap) { return strlen(s) < cap; }

void copyStr(char* dst, size_t cap, const char* src) {
  strncpy(dst, src ? src : "", cap - 1);
  dst[cap - 1] = '\0';
}

void handleSettings() {
  if (!allow()) return;
  JsonDocument in;
  if (!readJson(in)) return;
  JsonDocument errDoc;
  JsonObject errors = errDoc.to<JsonObject>();
  auto fail = [&](const char* field, const char* text) { errors[field] = text; };

  const bool firstRun = wifi_ssid[0] == '\0';  // before this save
  // 1. Check every field first: nothing is saved if one is wrong.
  char name[CFG_DEVICE_NAME_MAX], ssid[CFG_WIFI_SSID_MAX], tz[CFG_TIMEZONE_MAX], loc[CFG_WEATHER_LOC_MAX],
      stocks[CFG_STOCKS_MAX], ics[ICS_MAX_FEEDS][CFG_ICS_URL_MAX], bday[CFG_ICS_URL_MAX],
      mqttIp[CFG_MQTT_IP_MAX], mqttSn[CFG_MQTT_SN_MAX], pname[CFG_PRINTER_NAME_MAX], mw[CFG_MW_UID_MAX];
  int port = mqtt_port;
  JsonObjectConst o = in.as<JsonObjectConst>();

  auto plainText = [&](const char* key, char* out, size_t cap, const char* current, const char* label) {
    copyStr(out, cap, current);
    JsonVariantConst v = o[key];
    if (v.isUnbound()) return;
    if (!v.is<const char*>() || !fits(v.as<const char*>(), cap)) {
      char msg[80];
      snprintf(msg, sizeof(msg), "%s: at most %u characters.", label, (unsigned)(cap - 1));
      fail(key, msg);
      return;
    }
    copyStr(out, cap, v.as<const char*>());
  };
  plainText("device_name", name, sizeof(name), device_name, "Name");
  plainText("wifi_ssid", ssid, sizeof(ssid), wifi_ssid, "Wi-Fi name");
  plainText("weather_loc", loc, sizeof(loc), weather_location, "Place");
  plainText("mqtt_sn", mqttSn, sizeof(mqttSn), mqtt_sn, "Serial number");
  plainText("printer_name", pname, sizeof(pname), printer_name, "Printer name");
  plainText("makerworld_uid", mw, sizeof(mw), makerworld_uid, "MakerWorld user ID");

  // Power (stage 6).
  int clockInt = clock_interval, qFrom = quiet_from, qTo = quiet_to, batMah = battery_mah;
  if (!o["clock_interval"].isUnbound()) {
    clockInt = o["clock_interval"] | 0;
    if (!power::validClockInterval(clockInt)) fail("clock_interval", "Choose 1, 2, 5, 10, 15 or 30 minutes.");
  }
  if (!o["quiet_from"].isUnbound() && !power::parseHm(o["quiet_from"] | "", qFrom))
    fail("quiet_from", "A time like 23:00.");
  if (!o["quiet_to"].isUnbound() && !power::parseHm(o["quiet_to"] | "", qTo))
    fail("quiet_to", "A time like 06:00.");
  if (!o["battery_mah"].isUnbound()) {
    batMah = o["battery_mah"] | 0;
    if (batMah < 100 || batMah > 20000) fail("battery_mah", "Between 100 and 20000 mAh.");
  }

  copyStr(tz, sizeof(tz), device_timezone);
  if (!o["timezone"].isUnbound() &&
      !normalizeTimezone(o["timezone"] | "", tz, sizeof(tz)))
    fail("timezone", "Time zone not recognized: use a name like Europe/Berlin or CET, or a rule like CET-1CEST,M3.5.0,M10.5.0/3.");

  copyStr(stocks, sizeof(stocks), stock_symbols);
  if (!o["stocks"].isUnbound() && !normalizeStockSymbols(o["stocks"] | "", stocks, sizeof(stocks)))
    fail("stocks", "Yahoo Finance symbols (letters, digits, ^ . = -) separated by commas, e.g. AAPL,NVDA,^IXIC.");

  for (int i = 0; i < ICS_MAX_FEEDS; i++) copyStr(ics[i], sizeof(ics[i]), ics_urls[i]);
  if (!o["ics"].isUnbound()) {
    JsonArrayConst arr = o["ics"].as<JsonArrayConst>();
    for (int i = 0; i < ICS_MAX_FEEDS && i < (int)arr.size(); i++) {
      SecretOp op = secretOp(arr[i]);
      if (op == SecretOp::Clear) ics[i][0] = '\0';
      else if (op == SecretOp::Set && !validateIcsUrl(arr[i].as<const char*>(), ics[i], sizeof(ics[i])))
        fail(i == 0 ? "ics.0" : i == 1 ? "ics.1" : "ics.2", "Not an https://, http:// or webcal:// address.");
    }
  }
  copyStr(bday, sizeof(bday), bday_url);
  SecretOp bop = secretOp(o["bday_url"]);
  if (bop == SecretOp::Clear) bday[0] = '\0';
  else if (bop == SecretOp::Set && !validateIcsUrl(o["bday_url"].as<const char*>(), bday, sizeof(bday)))
    fail("bday_url", "Not an https://, http:// or webcal:// address.");

  copyStr(mqttIp, sizeof(mqttIp), mqtt_ip);
  if (!o["mqtt_ip"].isUnbound()) {
    const char* ip = o["mqtt_ip"] | "";
    if (!fits(ip, sizeof(mqttIp)) || !validatePrinterAddress(ip))
      fail("mqtt_ip", "An address in your home network (e.g. 192.168.1.100) or the printer's name there.");
    else copyStr(mqttIp, sizeof(mqttIp), ip);
  }
  if (!o["mqtt_port"].isUnbound()) {
    long p = o["mqtt_port"].is<const char*>() ? parseMqttPort(o["mqtt_port"].as<const char*>()) : (long)(o["mqtt_port"] | -1);
    if (p < 1 || p > 65535) {
      if (mqttIp[0]) fail("mqtt_port", "A number from 1 to 65535 (usually 8883).");
      else p = 8883;
    }
    if (p >= 1 && p <= 65535) port = (int)p;
  }

  struct SecretField {
    const char* key;
    char* target;
    size_t cap;
  };
  SecretField secrets[] = {{"wifi_pass", wifi_pass, sizeof(wifi_pass)},
                           {"mqtt_pass", mqtt_pass, sizeof(mqtt_pass)},
                           {"pkge_key", pkge_api_key, sizeof(pkge_api_key)},
                           {"adb_key", aerodatabox_key, sizeof(aerodatabox_key)}};
  for (const SecretField& f : secrets) {
    SecretOp op = secretOp(o[f.key]);
    if (op == SecretOp::Bad || (op == SecretOp::Set && !fits(o[f.key].as<const char*>(), f.cap)))
      fail(f.key, "Too long or not text.");
  }

  PinPreset preset = activePinPreset;
  PinConfig pins = customPins;
  if (!o["pin_preset"].isUnbound()) {
    const char* key = o["pin_preset"] | "";
    preset = parsePinPreset(String(key));
    if (strcmp(pinPresetValue(preset), key) != 0) fail("pin_preset", "Unknown board preset.");
  }
  if (o["pins"].is<JsonObjectConst>()) {
    JsonObjectConst p = o["pins"];
    auto pin = [&](const char* k, int16_t cur) -> int16_t {
      if (p[k].isUnbound()) return cur;
      if (p[k].isNull()) return PIN_UNASSIGNED;
      int v = p[k] | -1;
      return v < 0 || v > 48 ? PIN_UNASSIGNED : (int16_t)v;
    };
    pins.epdCs = pin("epd_cs", pins.epdCs);
    pins.epdDc = pin("epd_dc", pins.epdDc);
    pins.epdRst = pin("epd_rst", pins.epdRst);
    pins.epdBusy = pin("epd_busy", pins.epdBusy);
    pins.epdSck = pin("epd_sck", pins.epdSck);
    pins.epdMosi = pin("epd_mosi", pins.epdMosi);
    pins.displayPower = pin("display_power", pins.displayPower);
    pins.battery = pin("battery", pins.battery);
    pins.demoButton = pin("demo_button", pins.demoButton);
  }
  if (preset == PinPreset::Custom && !hasRequiredDisplayPins(pins))
    fail("pins", "Custom pins: CS, DC, RST, BUSY, SCK and MOSI are required.");

  if (errors.size()) {
    JsonDocument out;
    out["ok"] = false;
    out["error"] = "Nothing was saved: please correct the marked fields.";
    out["errors"] = errors;
    sendJson(400, out);
    return;
  }

  // 2. Apply.
  copyStr(device_name, sizeof(device_name), name);
  if (ssid[0]) copyStr(wifi_ssid, sizeof(wifi_ssid), ssid);
  copyStr(weather_location, sizeof(weather_location), loc);
  copyStr(mqtt_sn, sizeof(mqtt_sn), mqttSn);
  copyStr(printer_name, sizeof(printer_name), pname);
  copyStr(makerworld_uid, sizeof(makerworld_uid), mw);
  copyStr(device_timezone, sizeof(device_timezone), tz);
  copyStr(stock_symbols, sizeof(stock_symbols), stocks);
  for (int i = 0; i < ICS_MAX_FEEDS; i++) copyStr(ics_urls[i], sizeof(ics_urls[i]), ics[i]);
  copyStr(bday_url, sizeof(bday_url), bday);
  copyStr(mqtt_ip, sizeof(mqtt_ip), mqttIp);
  mqtt_port = port;
  for (const SecretField& f : secrets) {
    SecretOp op = secretOp(o[f.key]);
    if (op == SecretOp::Clear) f.target[0] = '\0';
    else if (op == SecretOp::Set && o[f.key].as<const char*>()[0]) copyStr(f.target, f.cap, o[f.key].as<const char*>());
  }
  if (o["ota_auto"].is<bool>()) otaAutoInstall = o["ota_auto"];
  if (o["ota_beta"].is<bool>()) otaBeta = o["ota_beta"];
  if (o["clock_24h"].is<bool>()) clock_24h = o["clock_24h"];
  clock_interval = (uint8_t)clockInt;
  if (o["quiet_enabled"].is<bool>()) quiet_on = o["quiet_enabled"];
  quiet_from = (uint16_t)qFrom;
  quiet_to = (uint16_t)qTo;
  battery_mah = (uint16_t)batMah;
  activePinPreset = preset;
  customPins = pins;
  applyStoredPinSelection();
  configSave();

  // Back into the same mode, so the app reconnects and editing goes on ("Done" ends it). The
  // first start (no Wi-Fi saved before) ends with the dashboard; "finish" / "stay" decide it.
  bool stay = in["stay"].is<bool>() ? in["stay"].as<bool>() : !(in["finish"] | firstRun);
  JsonDocument out;
  out["ok"] = true;
  out["restart"] = true;
  out["stay"] = stay;
  sendJson(200, out);
  if (stay) setupScheduleRestartSameMode(3000);  // 3 s: time for the app to send the layout too
  else setupScheduleRestart(3000);
}

// ---------- layout ----------
// Reads a layout request into template key + box string + show mask. False (answer sent) if bad.
bool parseLayoutRequest(const JsonDocument& in, char* tpl, size_t tplCap, char* boxes, size_t boxCap, uint16_t& mask) {
  const char* key = in["template"] | "";
  mask = widget_show_mask;
  boxes[0] = '\0';
  if (!strcmp(key, "custom")) {
    layouts::BoxLayout l;
    l.statusBar = in["statusBar"] | true;
    JsonArrayConst arr = in["boxes"].as<JsonArrayConst>();
    if (arr.size() > (size_t)layouts::MAX_BOXES) {
      sendError(400, layouts::boxErrorText(layouts::BoxError::TooMany));
      return false;
    }
    for (JsonObjectConst b : arr) {
      int widget = -1;
      if (!b["widget"].isNull()) {
        const char* wk = b["widget"] | "";
        widget = layouts::widgetFromKey(wk, strlen(wk));
        if (widget < 0) {
          sendError(400, layouts::boxErrorText(layouts::BoxError::UnknownWidget));
          return false;
        }
      }
      int c = b["c"] | -1, r = b["r"] | -1, w = b["w"] | 0, h = b["h"] | 0;
      if (c < 0 || r < 0 || c > 9 || r > 9 || w < 1 || h < 1 || w > 9 || h > 9) {
        sendError(400, layouts::boxErrorText(layouts::BoxError::Outside));
        return false;
      }
      l.boxes[l.count++] = layouts::Box{(uint8_t)c, (uint8_t)r, (uint8_t)w, (uint8_t)h, (int8_t)widget};
    }
    layouts::BoxError e = layouts::validateBoxes(l);
    if (e != layouts::BoxError::None) {
      sendError(400, layouts::boxErrorText(e));
      return false;
    }
    if (!layouts::writeBoxes(l, boxes, boxCap)) {
      sendError(400, "The layout is too long.");
      return false;
    }
    copyStr(tpl, tplCap, "custom");
    return true;
  }
  if (layouts::templateIndex(key) < 0) {
    sendError(400, "Unknown layout.");
    return false;
  }
  copyStr(tpl, tplCap, key);
  if (in["show"].is<JsonArrayConst>()) {
    mask = layouts::widgetBit(WidgetId::StatusBar) | layouts::widgetBit(WidgetId::Clock);
    for (JsonVariantConst v : in["show"].as<JsonArrayConst>()) {
      const char* wk = v | "";
      int id = layouts::widgetFromKey(wk, strlen(wk));
      if (id > 0) mask |= layouts::widgetBit(static_cast<WidgetId>(id));
    }
  }
  return true;
}

void handleLayout() {
  if (!allow()) return;
  JsonDocument in;
  if (!readJson(in)) return;
  char tpl[CFG_LAYOUT_MAX], boxes[CFG_LAYOUT_BOXES_MAX];
  uint16_t mask;
  if (!parseLayoutRequest(in, tpl, sizeof(tpl), boxes, sizeof(boxes), mask)) return;
  copyStr(layout_template, sizeof(layout_template), tpl);
  if (boxes[0]) copyStr(layout_boxes, sizeof(layout_boxes), boxes);
  widget_show_mask = mask;
  configSave();
  sendOk();
  setupScheduleDashboard();
}

// 1-bit BMP of the layout in the request, drawn by the device (same code as the screen).
void handlePreview() {
  if (!allow()) return;
  JsonDocument in;
  if (!readJson(in)) return;
  char tpl[CFG_LAYOUT_MAX], boxes[CFG_LAYOUT_BOXES_MAX];
  uint16_t mask;
  if (!parseLayoutRequest(in, tpl, sizeof(tpl), boxes, sizeof(boxes), mask)) return;

  GFXcanvas1* img = new (std::nothrow) GFXcanvas1(800, 480);
  if (!img || !img->getBuffer()) {
    delete img;
    sendError(503, "Not enough memory for the preview right now: try again.");
    return;
  }
  // The app's current (maybe unsaved) 12 / 24-hour choice, only for this picture.
  const bool saved24h = clock_24h;
  if (in["clock_24h"].is<bool>()) clock_24h = in["clock_24h"];
  setCanvas(img);
  img->fillScreen(GxEPD_WHITE);
  loadLayoutFrom(tpl, boxes, mask);
  FullScreenParts parts;
  parts.printer = parts.weather = parts.agenda = parts.stocks = parts.month = parts.tracking = true;
  drawFullScreen(parts);
  setCanvas(nullptr);
  clock_24h = saved24h;
  loadLayout();  // back to the saved layout

  // BMP: 14 + 40 byte headers, 2-color palette (0 = black, 1 = white), rows bottom-up, 100 bytes each.
  const uint32_t rowBytes = 100, imageBytes = rowBytes * 480, offset = 14 + 40 + 8;
  uint8_t head[62] = {};
  auto put32 = [&](int at, uint32_t v) {
    for (int i = 0; i < 4; i++) head[at + i] = (v >> (8 * i)) & 0xFF;
  };
  head[0] = 'B';
  head[1] = 'M';
  put32(2, offset + imageBytes);
  put32(10, offset);
  put32(14, 40);
  put32(18, 800);
  put32(22, 480);
  head[26] = 1;   // planes
  head[28] = 1;   // bits per pixel
  put32(34, imageBytes);
  put32(46, 2);   // colors used
  // palette: black, white (B, G, R, 0)
  head[58] = head[59] = head[60] = 0xFF;
  srv->setContentLength(offset + imageBytes);
  srv->sendHeader("Cache-Control", "no-store");
  srv->send(200, "image/bmp", "");
  srv->sendContent((const char*)head, sizeof(head));
  const uint8_t* buf = img->getBuffer();  // canvas bit 1 = white, MSB first, 100 bytes per row
  uint8_t chunk[rowBytes * 8];  // on the stack: 800 bytes
  for (int y = 479; y >= 0;) {
    size_t n = 0;
    for (int k = 0; k < 8 && y >= 0; k++, y--) {
      memcpy(chunk + n, buf + y * rowBytes, rowBytes);
      n += rowBytes;
    }
    srv->sendContent((const char*)chunk, n);
  }
  delete img;
}

// ---------- network ----------
void handleNetworks() {
  if (!allow()) return;
  int n = WiFi.scanNetworks();
  JsonDocument doc;
  JsonArray arr = doc["networks"].to<JsonArray>();
  // strongest first, each name once
  for (int pass = 0; pass < n && arr.size() < 30; pass++) {
    int best = -1;
    for (int i = 0; i < n; i++) {
      if (WiFi.SSID(i).length() == 0) continue;
      bool listed = false;
      for (JsonObject o : arr)
        if (WiFi.SSID(i) == (const char*)o["ssid"]) listed = true;
      if (listed) continue;
      if (best < 0 || WiFi.RSSI(i) > WiFi.RSSI(best)) best = i;
    }
    if (best < 0) break;
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = WiFi.SSID(best);
    o["rssi"] = WiFi.RSSI(best);
    o["secure"] = WiFi.encryptionType(best) != WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  sendJson(200, doc);
}

void handleWifiTest() {
  if (!allow()) return;
  JsonDocument in;
  if (!readJson(in)) return;
  const char* ssid = in["ssid"] | "";
  // No "pass": the saved password (same network); "" = an open network.
  const char* pass = in["pass"].is<const char*>() ? in["pass"].as<const char*>() : nullptr;
  if (!ssid[0]) {
    sendError(400, "Choose a network first.");
    return;
  }
  if (setupKind == SetupKind::Home) {
    // The device is connected with the saved network right now; reconnecting would cut off this
    // very request (and a wrong password would leave edit mode unreachable).
    JsonDocument out;
    if (strcmp(ssid, wifi_ssid) != 0) {
      sendError(400, "Another network can only be tested in setup mode: use \"Change Wi-Fi\".");
      return;
    }
    out["ok"] = !pass || strcmp(pass, wifi_pass) == 0;
    if (!(out["ok"] | false)) out["error"] = "Not the saved password: a new one can be tested in setup mode (\"Change Wi-Fi\").";
    sendJson(200, out);
    return;
  }
  char err[80] = "";
  bool ok = setupStaConnect(ssid, pass, err, sizeof(err));
  JsonDocument out;
  out["ok"] = ok;
  if (!ok) out["error"] = err;
  sendJson(200, out);
}

void handleCheckCalendar() {
  if (!allow()) return;
  JsonDocument in;
  if (!readJson(in)) return;
  char url[CFG_ICS_URL_MAX];
  if (in["index"].is<int>()) {
    int i = in["index"];
    if (i < 0 || i >= ICS_MAX_FEEDS || !ics_urls[i][0]) {
      sendError(400, "No saved link there.");
      return;
    }
    copyStr(url, sizeof(url), ics_urls[i]);
  } else if (in["bday"] | false) {
    if (!bday_url[0]) {
      sendError(400, "No saved link there.");
      return;
    }
    copyStr(url, sizeof(url), bday_url);
  } else if (!validateIcsUrl(in["url"] | "", url, sizeof(url)) || !url[0]) {
    sendError(400, "Not an https://, http:// or webcal:// address.");
    return;
  }
  if (!setupEnsureOnline()) {
    sendOffline();
    return;
  }
  char err[96] = "";
  int events = checkIcsLink(url, err, sizeof(err));
  JsonDocument out;
  out["ok"] = events >= 0;
  if (events >= 0) out["events"] = events;
  else out["error"] = err;
  sendJson(200, out);
}

void handlePlaces() {
  if (!allow()) return;
  String q = srv->arg("q");
  q.trim();
  if (q.length() < 2) {
    JsonDocument out;
    out["places"].to<JsonArray>();
    sendJson(200, out);
    return;
  }
  if (!setupEnsureOnline()) {
    sendOffline();
    return;
  }
  PlaceResult found[5];
  int n = searchPlaces(q.c_str(), found, 5);
  if (n < 0) {
    sendError(502, "The place search didn't answer: try again or type the place.");
    return;
  }
  JsonDocument out;
  JsonArray arr = out["places"].to<JsonArray>();
  for (int i = 0; i < n; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["name"] = found[i].name;
    o["detail"] = found[i].detail;
  }
  sendJson(200, out);
}

// ---------- personal data (stage 4) ----------
void sendFile(const char* path, const char* emptyJson) {
  constexpr size_t CAP = 16 * 1024;
  std::unique_ptr<char[]> buf(new (std::nothrow) char[CAP]);  // heap: little static RAM
  size_t n = buf ? readUserFile(path, buf.get(), CAP) : 0;
  srv->sendHeader("Cache-Control", "no-store");
  srv->send(200, "application/json", n ? buf.get() : emptyJson);
}

void handleGetTasks() {
  if (allow()) sendFile(USER_TASKS_FILE, "{\"tasks\":[]}");
}
void handleGetNote() {
  if (allow()) sendFile(USER_NOTE_FILE, "{\"note\":\"\"}");
}

void handlePutTasks() {
  if (!allow()) return;
  String bodyStr;
  if (!readBody(bodyStr)) return;
  const char* body = bodyStr.c_str();
  size_t len = bodyStr.length();
  std::unique_ptr<personal::TaskList> listP(new (std::nothrow) personal::TaskList());
  if (!listP) {
    sendError(503, "Not enough memory right now: try again.");
    return;
  }
  personal::TaskList& list = *listP;
  char err[96], removed[32];
  if (!personal::parseTasks(body, len, list, err, sizeof(err), screenFontHas, removed, sizeof(removed))) {
    sendError(400, err);
    return;
  }
  // Done tasks keep the date they were ticked off (new ones: today) and go after 30 days.
  personal::Date today = localToday();
  personal::stampDoneDates(list, userTasks(), today);
  personal::purgeDoneTasks(list, today);
  if (!saveUserTasks(list)) {
    sendError(500, "The tasks could not be saved.");
    return;
  }
  sendOk(removed);
  setupScheduleDashboard();
}

void handlePutNote() {
  if (!allow()) return;
  String bodyStr;
  if (!readBody(bodyStr)) return;
  const char* body = bodyStr.c_str();
  size_t len = bodyStr.length();
  std::unique_ptr<char[]> noteP(new (std::nothrow) char[personal::NOTE_BYTES]);
  if (!noteP) {
    sendError(503, "Not enough memory right now: try again.");
    return;
  }
  char* note = noteP.get();
  char err[96], removed[32];
  if (!personal::parseNote(body, len, note, personal::NOTE_BYTES, err, sizeof(err), screenFontHas, removed, sizeof(removed))) {
    sendError(400, err);
    return;
  }
  if (!saveUserNote(note)) {
    sendError(500, "The note could not be saved.");
    return;
  }
  sendOk(removed);
  setupScheduleDashboard();
}

void handleGetBirthdays() {
  if (!allow()) return;
  JsonDocument doc;
  const personal::BirthdayList& manual = userBirthdays();
  JsonArray m = doc["manual"].to<JsonArray>();
  for (int i = 0; i < manual.count; i++) {
    JsonObject o = m.add<JsonObject>();
    o["name"] = manual.items[i].name;
    o["day"] = manual.items[i].day;
    o["month"] = manual.items[i].month;
    o["year"] = manual.items[i].year;
  }
  std::unique_ptr<personal::Upcoming[]> up(new (std::nothrow) personal::Upcoming[personal::MAX_BIRTHDAYS]);
  int n = up ? personal::upcomingBirthdays(manual, userFeedBirthdays(), localToday(), up.get(), personal::MAX_BIRTHDAYS) : 0;
  JsonArray u = doc["upcoming"].to<JsonArray>();
  for (int i = 0; i < n; i++) {
    JsonObject o = u.add<JsonObject>();
    char date[11];
    personal::formatIsoDate(up[i].date, date, sizeof(date));
    o["name"] = up[i].who->name;
    o["date"] = date;
    o["days"] = up[i].days;
    o["age"] = up[i].age;
    o["fromFeed"] = up[i].fromFeed;
  }
  sendJson(200, doc);
}

void handlePutBirthdays() {
  if (!allow()) return;
  String bodyStr;
  if (!readBody(bodyStr)) return;
  const char* body = bodyStr.c_str();
  size_t len = bodyStr.length();
  std::unique_ptr<personal::BirthdayList> listP(new (std::nothrow) personal::BirthdayList());
  if (!listP) {
    sendError(503, "Not enough memory right now: try again.");
    return;
  }
  personal::BirthdayList& list = *listP;
  char err[96], removed[32];
  if (!personal::parseBirthdays(body, len, "manual", list, err, sizeof(err), screenFontHas, removed, sizeof(removed))) {
    sendError(400, err);
    return;
  }
  if (!saveUserBirthdays(list)) {
    sendError(500, "The birthdays could not be saved.");
    return;
  }
  sendOk(removed);
  setupScheduleDashboard();
}

// ---------- diagnostics (build 72, docs/plans/STAGE5B_DIAGNOSTICS.md) ----------
constexpr size_t LOG_READ_MAX = 24 * 1024;

void handleDiagnostics() {
  if (!allow()) return;
  JsonDocument doc;
  time_t now = time(nullptr);
  doc["time"] = now > 1700000000 ? (long)now : 0;
  doc["version"] = FW_VERSION;
  doc["bootCount"] = bootCount;

  // The last 200 log lines, oldest first.
  std::unique_ptr<char[]> text(new (std::nothrow) char[LOG_READ_MAX]);
  JsonArray log = doc["log"].to<JsonArray>();
  if (text) {
    size_t len = logReadAll(text.get(), LOG_READ_MAX);
    std::unique_ptr<const char*[]> startsP(new (std::nothrow) const char*[200]);
    const char** starts = startsP.get();
    int n = starts ? lastLines(text.get(), len, starts, 200) : 0;
    for (int i = 0; i < n; i++) {
      const char* end = strchr(starts[i], '\n');
      size_t l = end ? (size_t)(end - starts[i]) : strlen(starts[i]);
      log.add(String(starts[i]).substring(0, l));
    }
  }

  // Battery now and the hourly history ("unix,millivolts,usb").
  JsonObject bat = doc["battery"].to<JsonObject>();
  bat["sensor"] = hasBatteryPin();
  if (hasBatteryPin()) {
    float v = readBattery();
    bat["mv"] = (int)(v * 1000.0f + 0.5f);
    bat["plausible"] = batteryReadingPlausible(v);
    bat["percent"] = batteryPercentNow();
    float qv;
    time_t qat;
    if (batteryQuiet(qv, qat, 3 * 3600)) {  // last reading with Wi-Fi off (now it sags under the radio)
      bat["quietMv"] = (int)(qv * 1000.0f + 0.5f);
      bat["quietAt"] = (long)qat;
      bat["quietPercent"] = batteryReadingPlausible(qv) ? batteryPercent(qv) : -1;
    }
  }
  bat["usb"] = usbPowered();
  bat["minMv"] = (int)(BATTERY_MIN_RUNNING_V * 1000);
  JsonArray hist = bat["history"].to<JsonArray>();
  {
    std::unique_ptr<char[]> csv(new (std::nothrow) char[8 * 1024]);
    size_t len = csv ? readUserFile("/battery.csv", csv.get(), 8 * 1024) : 0;
    for (char* line = csv.get(); len && line && *line;) {
      char* next = strchr(line, '\n');
      if (next) *next = '\0';
      long t = 0;
      int mv = 0, usb = 0;
      if (sscanf(line, "%ld,%d,%d", &t, &mv, &usb) == 3) {
        JsonArray p = hist.add<JsonArray>();
        p.add(t);
        p.add(mv);
        p.add(usb);
      }
      line = next ? next + 1 : nullptr;
    }
  }

  // Services: last success, failures, next retry, old data.
  struct Svc {
    Service s;
    const char* key;
    const char* name;
    WidgetId widget;
  };
  const Svc svcs[] = {{Service::Weather, "weather", "Weather", WidgetId::Weather},
                      {Service::Stocks, "stocks", "Stocks", WidgetId::Stocks},
                      {Service::Calendar, "calendar", "Calendar", WidgetId::Agenda},
                      {Service::Tracking, "tracking", "Parcels", WidgetId::Tracking}};
  JsonArray services = doc["services"].to<JsonArray>();
  for (const Svc& v : svcs) {
    const ServiceState& st = serviceStates[(int)v.s];
    JsonObject o = services.add<JsonObject>();
    o["key"] = v.key;
    o["name"] = v.name;
    o["lastOk"] = (long)st.lastOk;
    o["failures"] = st.failures;
    o["retryAt"] = st.failures ? (long)st.retryAt : 0;
    o["stale"] = isStale(st, now, (time_t)widgetInfo(v.widget).refresh * 60);
  }
  JsonArray probs = doc["problems"].to<JsonArray>();
  for (const ProblemText& p : PROBLEM_TEXTS)
    if (problems & p.flag) probs.add(p.key);
  JsonObject wifi = doc["wifi"].to<JsonObject>();
  wifi["failures"] = wifiFailures;
  wifi["retryAfter"] = wifiFailures ? (long)wifiRetryAfter : 0;
  wifi["rssi"] = setupOnline() ? WiFi.RSSI() : 0;
  doc["lastTimeSync"] = (long)lastNtpSync;
  JsonObject heap = doc["heap"].to<JsonObject>();
  heap["free"] = ESP.getFreeHeap();
  heap["minFree"] = ESP.getMinFreeHeap();
  heap["largest"] = ESP.getMaxAllocHeap();
  sendJson(200, doc);
}

void handleLogText() {
  if (!allow()) return;
  std::unique_ptr<char[]> text(new (std::nothrow) char[LOG_READ_MAX]);
  if (!text) {
    sendError(503, "Not enough memory right now: try again.");
    return;
  }
  size_t len = logReadAll(text.get(), LOG_READ_MAX);
  srv->sendHeader("Cache-Control", "no-store");
  srv->sendHeader("Content-Disposition", "attachment; filename=\"dashboard-log.txt\"");
  srv->send_P(200, "text/plain; charset=utf-8", text.get(), len);  // send_P: content with a length
}

void handleLogClear() {
  if (!allow() || !jsonType()) return;
  logClear();
  sendOk();
}

// ---------- device ----------
void handleLogin() {
  if (!allow(false)) return;
  if (setupKind != SetupKind::Home) {
    sendOk();
    return;
  }
  JsonDocument in;
  if (!readJson(in)) return;
  String cookie;
  if (!setupLogin(in["code"] | "", cookie)) {
    sendError(400, "Wrong code: type the 6 digits shown on the display.");
    return;
  }
  setupNoteActivity();
  srv->sendHeader("Set-Cookie", cookie);
  sendOk();
}

void handleUpdate() {
  if (!allow() || !jsonType()) return;
  if (!wifi_ssid[0]) {
    sendError(400, "Save your Wi-Fi settings first.");
    return;
  }
  updateRequest = UPDATE_REQUEST_MAGIC;
  sendOk();
  setupScheduleRestart(1500);
}

void handleFactoryReset() {
  if (!allow() || !jsonType()) return;
  sendOk();
  delay(1500);
  configFactoryReset();  // erases everything and restarts
}

void handleFinish() {
  if (!allow() || !jsonType()) return;
  JsonDocument in;
  bool toAp = false;
  if (srv->hasArg("plain") && srv->arg("plain").length() && !deserializeJson(in, srv->arg("plain")))
    toAp = in["ap"] | false;
  if (toAp) setupRequest = SETUP_REQUEST_AP_MAGIC;  // "Change Wi-Fi": restart into the hotspot
  sendOk();
  setupScheduleRestart(1000);
}

}  // namespace

void registerApiRoutes(WebServer& server) {
  srv = &server;
  static const char* headers[] = {"Content-Type", "Cookie", "Host", "Origin"};
  server.collectHeaders(headers, sizeof(headers) / sizeof(headers[0]));
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/settings", HTTP_POST, handleSettings);
  server.on("/api/layout", HTTP_POST, handleLayout);
  server.on("/api/preview.bmp", HTTP_POST, handlePreview);
  server.on("/api/networks", HTTP_GET, handleNetworks);
  server.on("/api/wifi-test", HTTP_POST, handleWifiTest);
  server.on("/api/check-calendar", HTTP_POST, handleCheckCalendar);
  server.on("/api/places", HTTP_GET, handlePlaces);
  server.on("/api/tasks", HTTP_GET, handleGetTasks);
  server.on("/api/tasks", HTTP_PUT, handlePutTasks);
  server.on("/api/notes", HTTP_GET, handleGetNote);
  server.on("/api/notes", HTTP_PUT, handlePutNote);
  server.on("/api/birthdays", HTTP_GET, handleGetBirthdays);
  server.on("/api/birthdays", HTTP_PUT, handlePutBirthdays);
  server.on("/api/login", HTTP_POST, handleLogin);
  server.on("/api/update", HTTP_POST, handleUpdate);
  server.on("/api/factory-reset", HTTP_POST, handleFactoryReset);
  server.on("/api/finish", HTTP_POST, handleFinish);
  server.on("/api/diagnostics", HTTP_GET, handleDiagnostics);
  server.on("/api/log.txt", HTTP_GET, handleLogText);
  server.on("/api/log/clear", HTTP_POST, handleLogClear);
}
