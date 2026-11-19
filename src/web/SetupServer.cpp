// Setup mode: the "Dashboard-Setup" access point with a captive portal and the setup page
// (settings, pins, firmware updates). Opened at the first start, with a 4 s hold, or BOOT after RST.
#include "SetupServer.h"

#include <Arduino.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <SPI.h>
#include <WebServer.h>
#include <WiFi.h>
#include <stdarg.h>

#include "../../app.h"
#include "../core/AppState.h"
#include "../core/ConfigManager.h"
#include "../core/Timezone.h"
#include "../core/ConfigValidation.h"
#include "../hardware/EpaperDriver.h"
#include "../hardware/PinMapping.h"
#include "../update/OtaFlow.h"
#include "../update/UpdatePolicy.h"
#include "../services/TimeService.h"
#include "../services/WifiService.h"
#include "../ui/Canvas.h"
#include "../widgets/Dashboard.h"
#include "../widgets/Registry.h"
#include "Api.h"
#include "SetupMode.h"
#include "SetupScreen.h"
#include "generated/WebApp.h"

// Functions of this file, in the order they are defined below.
void appendFormat(String& out, const char* fmt, ...);
String buildPage();
void handleConfig();
void handleUpdateNow();
void handleSave();
int16_t readPinField(const String& name);
static void copyToCfg(char* dest, size_t cap, const String& s);
static String htmlAttr(const char* s);

static void copyToCfg(char* dest, size_t cap, const String& s) {
  if (!dest || cap == 0) return;
  strncpy(dest, s.c_str(), cap - 1);
  dest[cap - 1] = '\0';
}


#define AP_SSID "Dashboard-Setup"
#define AP_TIMEOUT_MS (5UL * 60UL * 1000UL)   // leave setup mode after 5 min without activity
static unsigned long apLastActivity = 0;     // millis() of AP start / last setup page request

WebServer server(80);
DNSServer dnsServer;

// ---- Setup mode state (stage 5, src/web/SetupMode.h) ----
SetupKind setupKind = SetupKind::Off;
static uint32_t restartAt = 0;        // millis() of a scheduled restart, 0 = none
static bool dashboardPending = false;  // draw the dashboard after the answer (layout / data saved)
static bool homeScreenPending = false; // new code: redraw the edit-mode screen
static char apPassword[12] = "";
static char hostName[40] = "dashboard";
static char sessionToken[33] = "";
static char loginCode[7] = "";
static uint8_t wrongCodes = 0;
static uint8_t codeRotations = 0;
static bool codeOnScreen = false;  // edit mode: the code screen is shown (not the dashboard)
#define EDIT_TIMEOUT_MS (10UL * 60UL * 1000UL)  // edit mode on the home network: 10 min without use

void setupNoteActivity() { apLastActivity = millis(); }
void setupScheduleRestart(uint32_t delayMs) { restartAt = millis() + delayMs; if (!restartAt) restartAt = 1; }
void setupScheduleDashboard() { dashboardPending = true; }
const char* setupHostName() { return hostName; }

static void randomText(char* out, size_t len, const char* alphabet) {
  size_t n = strlen(alphabet);
  for (size_t i = 0; i < len; i++) out[i] = alphabet[esp_random() % n];
  out[len] = '\0';
}

// The hotspot password: made once per device (and after a factory reset), shown on the screen.
static void loadApPassword() {
  Preferences p;
  p.begin("config", false);
  String saved = p.getString("ap_pass", "");
  if (saved.length() >= 8 && saved.length() < sizeof(apPassword)) {
    snprintf(apPassword, sizeof(apPassword), "%s", saved.c_str());
  } else {
    randomText(apPassword, 8, "abcdefghjkmnpqrstuvwxyz23456789");  // no 0/o, 1/l/i
    p.putString("ap_pass", apPassword);
  }
  p.end();
}

// "Kitchen display" -> "kitchen-display" (the .local address).
static void makeHostName() {
  size_t n = 0;
  bool dash = false;
  for (const char* s = device_name; *s && n < sizeof(hostName) - 8; s++) {
    char c = *s;
    if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      hostName[n++] = c;
      dash = false;
    } else if (n && !dash) {
      hostName[n++] = '-';
      dash = true;
    }
  }
  while (n && hostName[n - 1] == '-') n--;
  hostName[n] = '\0';
  if (!n) snprintf(hostName, sizeof(hostName), "dashboard");
}

static void newLoginCode() {
  randomText(loginCode, 6, "0123456789");
  wrongCodes = 0;
}

bool setupLogin(const char* code, String& cookie) {
  if (!codeOnScreen) {  // the dashboard covers the code: show it again, don't count this one
    homeScreenPending = true;
    return false;
  }
  if (!loginCode[0] || strcmp(code, loginCode) != 0) {
    delay(1000);  // slows guessing down
    if (++wrongCodes >= 5) {  // guessing: a new code on the screen; after 3 new codes, stop
      newLoginCode();
      homeScreenPending = true;
      if (++codeRotations >= 3) {
        Serial.println("Edit mode: too many wrong codes -> leaving");
        setupScheduleRestart(500);
      }
    }
    return false;
  }
  randomText(sessionToken, 32, "0123456789abcdef");
  cookie = String("s=") + sessionToken + "; Path=/; HttpOnly; SameSite=Strict";
  return true;
}

bool setupSessionOk() {
  if (!sessionToken[0]) return false;
  String c = server.header("Cookie");
  String want = String("s=") + sessionToken;
  int at = c.indexOf(want);
  if (at < 0) return false;
  int end = at + want.length();
  return (at == 0 || c[at - 1] == ' ' || c[at - 1] == ';') && (end == (int)c.length() || c[end] == ';');
}

bool setupHostOk() {
  String host = server.header("Host");
  int colon = host.indexOf(':');
  if (colon >= 0) host = host.substring(0, colon);
  host.toLowerCase();
  return host == WiFi.localIP().toString() || host == String(hostName) + ".local";
}

bool setupFromHotspot() {
  IPAddress ap = WiFi.softAPIP();
  if (server.client().localIP() != ap) return false;
  String host = server.header("Host");
  int colon = host.indexOf(':');
  if (colon >= 0) host = host.substring(0, colon);
  return host == ap.toString();
}

bool setupOnline() { return WiFi.status() == WL_CONNECTED; }

bool setupStaConnect(const char* ssid, const char* pass, char* err, size_t cap) {
  if (setupOnline() && WiFi.SSID() == ssid && !pass) return true;
  if (!pass) pass = strcmp(ssid, wifi_ssid) == 0 ? wifi_pass : "";
  if (setupKind == SetupKind::Ap) WiFi.mode(WIFI_AP_STA);  // the hotspot stays up
  WiFi.disconnect(false);
  delay(200);
  WiFi.begin(ssid, pass);
  unsigned long t0 = millis();
  wl_status_t st = WL_IDLE_STATUS;
  while (millis() - t0 < 15000) {
    st = WiFi.status();
    if (st == WL_CONNECTED) break;
    // A failed earlier attempt leaves its status for a moment: trust a failure only after 1.5 s.
    if ((st == WL_NO_SSID_AVAIL || st == WL_CONNECT_FAILED) && millis() - t0 > 1500) break;
    delay(200);
    dnsServer.processNextRequest();
  }
  if (st == WL_CONNECTED) {
    initTime();  // for the birthdays' "today" and dates in the checks
    return true;
  }
  if (st == WL_NO_SSID_AVAIL) snprintf(err, cap, "Network not found: is it in range?");
  else if (st == WL_CONNECT_FAILED) snprintf(err, cap, "Wrong password");
  else snprintf(err, cap, "No connection: check the password and that the network is in range.");
  WiFi.disconnect(false);
  return false;
}

bool setupEnsureOnline() {
  if (setupOnline()) return true;
  if (!wifi_ssid[0]) return false;
  char err[80];
  return setupStaConnect(wifi_ssid, nullptr, err, sizeof(err));
}

// The setup app (web/, compiled in by tools/web/build.py), gzipped.
static void handleApp() {
  setupNoteActivity();
  server.sendHeader("Content-Encoding", "gzip");
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html; charset=utf-8", reinterpret_cast<const char*>(WEB_APP_GZ), WEB_APP_GZ_LEN);
}

// The dashboard with the saved layout, drawn in setup mode (no restart after a layout change).
static void drawDashboardNow() {
  loadLayout();
  if (hasDisplayPowerPin()) {
    pinMode(activePins.displayPower, OUTPUT);
    digitalWrite(activePins.displayPower, HIGH);
    delay(50);
  }
  SPI.begin(activePins.epdSck, -1, activePins.epdMosi, activePins.epdCs);
  display.init(115200, true, 2, false);
  display.setRotation(0);
  display.setFullWindow();
  FullScreenParts parts;
  parts.printer = parts.weather = parts.agenda = parts.stocks = parts.month = parts.tracking = true;
  display.firstPage();
  do {
    canvas().fillScreen(GxEPD_WHITE);
    drawFullScreen(parts);
  } while (display.nextPage());
  display.hibernate();
  redrawAll = true;  // the next normal wake draws everything fresh
  codeOnScreen = false;  // edit mode: the next login attempt brings the code back
}


void appendFormat(String& out, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  va_list argsCopy;
  va_copy(argsCopy, args);
  int needed = vsnprintf(nullptr, 0, fmt, argsCopy);
  va_end(argsCopy);
  if (needed <= 0) {
    va_end(args);
    return;
  }

  char* buffer = new char[needed + 1];
  vsnprintf(buffer, needed + 1, fmt, args);
  va_end(args);
  out += buffer;
  delete[] buffer;
}

// Escape a stored value for use inside a value="..." attribute.
static String htmlAttr(const char* s) {
  String out;
  for (; *s; s++) {
    switch (*s) {
      case '"': out += "&quot;"; break;
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      default:  out += *s; break;
    }
  }
  return out;
}

static String otaStatusHtml();

/* --------------------------------------------------
   HTML PAGE (stored in flash to save RAM)
   -------------------------------------------------- */

String buildPage()
{
  // The saved network is pre-selected: otherwise the first (strongest) network in the scan was
  // selected, and saving the page for any other change silently switched the device to it (with
  // the saved password), so Wi-Fi, internet time and all updates stopped working.
  String options;
  options.reserve(3072);
  bool savedListed = false;
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n && i < 48; i++)
  {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) continue;
    bool dup = false;  // the same network from several access points: list it once
    for (int j = 0; j < i && !dup; j++) dup = WiFi.SSID(j) == ssid;
    if (dup) continue;
    bool isSaved = wifi_ssid[0] && ssid == wifi_ssid;
    savedListed = savedListed || isSaved;
    options += "<option value=\"";
    options += htmlAttr(ssid.c_str());
    options += "\"";
    if (isSaved) options += " selected";
    options += ">";
    options += htmlAttr(ssid.c_str());
    options += "</option>";
  }
  if (wifi_ssid[0] && !savedListed) {
    // Saved network not in range right now: keep it selectable (and selected).
    String saved = String("<option value=\"") + htmlAttr(wifi_ssid) + "\" selected>" + htmlAttr(wifi_ssid) +
                   " (saved, not found now)</option>";
    options = saved + options;
  }

  String pinPresetOptions;
  pinPresetOptions.reserve(768);
  const PinPreset presets[] = {
    PinPreset::Esp32Waveshare,
    PinPreset::Esp32Default,
    PinPreset::Esp32C6Default,
    PinPreset::Esp32C6SuperMini,
    PinPreset::Custom
  };
  for (PinPreset preset : presets) {
    pinPresetOptions += "<option value='";
    pinPresetOptions += pinPresetValue(preset);
    pinPresetOptions += "'";
    if (preset == activePinPreset) pinPresetOptions += " selected";
    pinPresetOptions += ">";
    pinPresetOptions += pinPresetLabel(preset);
    pinPresetOptions += "</option>";
  }

  String pinPresetJs;
  pinPresetJs.reserve(1400);
  appendFormat(pinPresetJs, R"rawliteral(
const presetPins = {
  "esp32_waveshare": { epdCs:%d, epdDc:%d, epdRst:%d, epdBusy:%d, epdSck:%d, epdMosi:%d, displayPower:%d, battery:%d, demoButton:%d },
  "esp32": { epdCs:%d, epdDc:%d, epdRst:%d, epdBusy:%d, epdSck:%d, epdMosi:%d, displayPower:%d, battery:%d, demoButton:%d },
  "esp32_c6": { epdCs:%d, epdDc:%d, epdRst:%d, epdBusy:%d, epdSck:%d, epdMosi:%d, displayPower:%d, battery:%d, demoButton:%d },
  "esp32_c6_supermini": { epdCs:%d, epdDc:%d, epdRst:%d, epdBusy:%d, epdSck:%d, epdMosi:%d, displayPower:%d, battery:%d, demoButton:%d }
};
let previousPinPreset = "%s";
  )rawliteral",
    makePinPreset(PinPreset::Esp32Waveshare).epdCs,
    makePinPreset(PinPreset::Esp32Waveshare).epdDc,
    makePinPreset(PinPreset::Esp32Waveshare).epdRst,
    makePinPreset(PinPreset::Esp32Waveshare).epdBusy,
    makePinPreset(PinPreset::Esp32Waveshare).epdSck,
    makePinPreset(PinPreset::Esp32Waveshare).epdMosi,
    makePinPreset(PinPreset::Esp32Waveshare).displayPower,
    makePinPreset(PinPreset::Esp32Waveshare).battery,
    makePinPreset(PinPreset::Esp32Waveshare).demoButton,
    makePinPreset(PinPreset::Esp32Default).epdCs,
    makePinPreset(PinPreset::Esp32Default).epdDc,
    makePinPreset(PinPreset::Esp32Default).epdRst,
    makePinPreset(PinPreset::Esp32Default).epdBusy,
    makePinPreset(PinPreset::Esp32Default).epdSck,
    makePinPreset(PinPreset::Esp32Default).epdMosi,
    makePinPreset(PinPreset::Esp32Default).displayPower,
    makePinPreset(PinPreset::Esp32Default).battery,
    makePinPreset(PinPreset::Esp32Default).demoButton,
    makePinPreset(PinPreset::Esp32C6Default).epdCs,
    makePinPreset(PinPreset::Esp32C6Default).epdDc,
    makePinPreset(PinPreset::Esp32C6Default).epdRst,
    makePinPreset(PinPreset::Esp32C6Default).epdBusy,
    makePinPreset(PinPreset::Esp32C6Default).epdSck,
    makePinPreset(PinPreset::Esp32C6Default).epdMosi,
    makePinPreset(PinPreset::Esp32C6Default).displayPower,
    makePinPreset(PinPreset::Esp32C6Default).battery,
    makePinPreset(PinPreset::Esp32C6Default).demoButton,
    makePinPreset(PinPreset::Esp32C6SuperMini).epdCs,
    makePinPreset(PinPreset::Esp32C6SuperMini).epdDc,
    makePinPreset(PinPreset::Esp32C6SuperMini).epdRst,
    makePinPreset(PinPreset::Esp32C6SuperMini).epdBusy,
    makePinPreset(PinPreset::Esp32C6SuperMini).epdSck,
    makePinPreset(PinPreset::Esp32C6SuperMini).epdMosi,
    makePinPreset(PinPreset::Esp32C6SuperMini).displayPower,
    makePinPreset(PinPreset::Esp32C6SuperMini).battery,
    makePinPreset(PinPreset::Esp32C6SuperMini).demoButton,
    pinPresetValue(activePinPreset)
  );

  char displayPowerValue[12];
  char batteryValue[12];
  char demoButtonValue[12];
  pinValueToText(customPins.displayPower, displayPowerValue, sizeof(displayPowerValue));
  pinValueToText(customPins.battery, batteryValue, sizeof(batteryValue));
  pinValueToText(customPins.demoButton, demoButtonValue, sizeof(demoButtonValue));

  String customPinInputs;
  customPinInputs.reserve(2200);
  appendFormat(customPinInputs, R"rawliteral(
    <div id="customPinFields" class="pin-fields" style="display:%s;">
      <div class="note">Custom mode lets you override every display-related pin. Leave optional fields empty to disable them.</div>

      <label>EPD CS</label>
      <input type="number" name="pin_epd_cs" value="%d" placeholder="Chip select">

      <label>EPD DC</label>
      <input type="number" name="pin_epd_dc" value="%d" placeholder="Data/command">

      <label>EPD RST</label>
      <input type="number" name="pin_epd_rst" value="%d" placeholder="Reset">

      <label>EPD BUSY</label>
      <input type="number" name="pin_epd_busy" value="%d" placeholder="Busy">

      <label>EPD SCK</label>
      <input type="number" name="pin_epd_sck" value="%d" placeholder="SPI clock">

      <label>EPD MOSI</label>
      <input type="number" name="pin_epd_mosi" value="%d" placeholder="SPI MOSI">

      <label>Display Power Pin (optional)</label>
      <input type="number" name="pin_display_power" value="%s" placeholder="Leave empty if unused">

      <label>Battery Pin (optional)</label>
      <input type="number" name="pin_battery" value="%s" placeholder="Leave empty if unused">

      <label>Button Pin (optional)</label>
      <input type="number" name="pin_demo_button" value="%s" placeholder="Leave empty if unused">
    </div>
  )rawliteral",
    activePinPreset == PinPreset::Custom ? "block" : "none",
    customPins.epdCs,
    customPins.epdDc,
    customPins.epdRst,
    customPins.epdBusy,
    customPins.epdSck,
    customPins.epdMosi,
    displayPowerValue,
    batteryValue,
    demoButtonValue
  );

  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 Calendar Setup</title>

<style>
body{
  font-family: Arial, Helvetica, sans-serif;
  background:#f4f6f8;
  margin:0;
}

.container{
  max-width:420px;
  margin:40px auto;
  background:white;
  padding:25px;
  border-radius:12px;
  box-shadow:0 6px 18px rgba(0,0,0,0.1);
}

h2{
  text-align:center;
  margin-bottom:25px;
  color:#333;
}

label{
  font-weight:600;
  font-size:14px;
}

input, select{
  width:100%;
  padding:10px;
  margin-top:6px;
  margin-bottom:18px;
  border:1px solid #ccc;
  border-radius:8px;
  font-size:14px;
}

button{
  width:100%;
  padding:12px;
  border:none;
  border-radius:10px;
  background:#007bff;
  color:white;
  font-size:16px;
  font-weight:bold;
  cursor:pointer;
}

button:hover{
  background:#0056b3;
}

.scanBtn{
  background:#28a745;
  margin-bottom:15px;
}

.footer{
  text-align:center;
  margin-top:15px;
  font-size:12px;
  color:#777;
}

.note{
  margin:14px 0;
  padding:10px 12px;
  border-radius:10px;
  background:#eef6ff;
  color:#335;
  font-size:13px;
  line-height:1.4;
}

.pin-fields{
  margin-top:8px;
  border-top:1px solid #e4e9ee;
  padding-top:14px;
}
</style>

<script>
  )rawliteral";

html += pinPresetJs;

html += R"rawliteral(
function setPinField(name, value){
  const field = document.getElementsByName(name)[0];
  if(!field) return;
  field.value = value >= 0 ? value : "";
}

function applyPresetValues(presetKey){
  const pins = presetPins[presetKey];
  if(!pins) return;
  setPinField("pin_epd_cs", pins.epdCs);
  setPinField("pin_epd_dc", pins.epdDc);
  setPinField("pin_epd_rst", pins.epdRst);
  setPinField("pin_epd_busy", pins.epdBusy);
  setPinField("pin_epd_sck", pins.epdSck);
  setPinField("pin_epd_mosi", pins.epdMosi);
  setPinField("pin_display_power", pins.displayPower);
  setPinField("pin_battery", pins.battery);
  setPinField("pin_demo_button", pins.demoButton);
}

function toggleCustomPins(){
  const preset = document.getElementById("pin_preset").value;
  const customPins = document.getElementById("customPinFields");
  customPins.style.display = preset === "custom" ? "block" : "none";
  if(preset === "custom" && previousPinPreset !== "custom"){
    applyPresetValues(previousPinPreset);
  }
  previousPinPreset = preset;
}

function validateForm(){
  const ssid = document.getElementById("wifi_ssid").value;
  if(ssid.length === 0){
    alert("WiFi SSID cannot be empty");
    return false;
  }
  if(document.getElementById("pin_preset").value === "custom"){
    const required = ["pin_epd_cs","pin_epd_dc","pin_epd_rst","pin_epd_busy","pin_epd_sck","pin_epd_mosi"];
    for(const fieldId of required){
      const field = document.getElementsByName(fieldId)[0];
      if(!field || field.value.trim().length === 0){
        alert("Please fill all required custom display pins.");
        return false;
      }
    }
  }
  return true;
}
</script>
</head>

<body>

<div class="container">
  <h2>ESP32 Calendar Setup</h2>

  <form action="/save" method="POST" onsubmit="return validateForm();">

    <label>Available WiFi Networks</label>
    <select id="wifi_ssid" name="wifi_ssid">
)rawliteral";

html += options;

html += R"rawliteral(
    </select>

    <button class="scanBtn" type="button" onclick="location.reload()">Scan Again</button>

    <label>WiFi Password</label>
    <input  name="wifi_pass" placeholder="Leave empty to keep the saved password">

    <label>Timezone</label>
    <input name="device_timezone" value=")rawliteral";

html += htmlAttr(device_timezone);

html += R"rawliteral(" list="tz_presets" placeholder="CET-1CEST,M3.5.0,M10.5.0/3">
    <datalist id="tz_presets">
      <option value="CET-1CEST,M3.5.0,M10.5.0/3">Germany, Austria, Switzerland, most of Central Europe</option>
      <option value="GMT0BST,M3.5.0/1,M10.5.0">United Kingdom</option>
      <option value="EET-2EEST,M3.5.0/3,M10.5.0/4">Finland, Greece, Romania</option>
      <option value="EST5EDT,M3.2.0,M11.1.0">US Eastern</option>
      <option value="PST8PDT,M3.2.0,M11.1.0">US Pacific</option>
      <option value="UTC0">UTC</option>
    </datalist>
    <div class="note">A POSIX timezone rule including summer time, e.g. <b>CET-1CEST,M3.5.0,M10.5.0/3</b> for Germany. Names like CET or Europe/Berlin are turned into that rule when you save.</div>

    <hr>

    <label>Weather Location</label>
    <input name="weather_location" value=")rawliteral";

html += htmlAttr(weather_location);

html += R"rawliteral(" placeholder="London, GB   or   51.51,-0.13">
    <div class="note">City name (optionally followed by a 2-letter country code) or latitude,longitude. Forecast from Open-Meteo.</div>

    <label>Stock Symbols</label>
    <input name="stock_symbols" value=")rawliteral";

html += htmlAttr(stock_symbols);

html += R"rawliteral(" placeholder="AAPL,NVDA,^IXIC">
    <div class="note">Up to 5 Yahoo Finance symbols, comma separated.</div>

    <label>Calendar iCal URLs (up to 3, merged)</label>
)rawliteral";

for (int i = 0; i < ICS_MAX_FEEDS; i++) {
  html += "    <input name=\"";
  html += ICS_PREF_KEYS[i];
  html += "\" value=\"";
  html += htmlAttr(ics_urls[i]);
  html += "\" placeholder=\"https://calendar.google.com/calendar/ical/.../basic.ics\">\n";
}

html += R"rawliteral(
    <div class="note">Google Calendar: Settings &rarr; your calendar &rarr; Integrate calendar &rarr; public or secret address in iCal format.</div>

    <label>Birthday calendar iCal URL (optional)</label>
    <input name="bday_url" value=")rawliteral";

html += htmlAttr(bday_url);

html += R"rawliteral(" placeholder="https://calendar.google.com/calendar/ical/.../basic.ics">
    <div class="note">Shown by the Birthdays widget (next 60 days), not in the agenda. Birthdays can also be typed in through the setup app.</div>

    <hr>

    <label>Printer Name</label>
    <input name="printer_name" value=")rawliteral";

html += htmlAttr(printer_name);

html += R"rawliteral(" placeholder="Shown as the printer widget title">

    <label>Bambu Printer IP</label>
    <input name="mqtt_ip" value=")rawliteral";

html += htmlAttr(mqtt_ip);

html += R"rawliteral(" placeholder="192.168.1.100">

    <label>Bambu MQTT Port</label>
    <input name="mqtt_port" value=")rawliteral";

html += String(mqtt_port ? mqtt_port : 8883);

html += R"rawliteral(" placeholder="8883">

    <label>Bambu Printer Serial Number</label>
    <input name="mqtt_sn" value=")rawliteral";

html += htmlAttr(mqtt_sn);

html += R"rawliteral(" placeholder="Serial number (printer Settings &rarr; Device)">

    <label>Bambu LAN Access Code</label>
    <input name="mqtt_pass" placeholder="Leave empty to keep the saved code">
    <div class="note">Printer screen: Settings &rarr; WLAN / Network &rarr; Access Code. Leave the IP empty to hide the printer widget.</div>

    <label>MakerWorld User ID</label>
    <input name="makerworld_uid" value=")rawliteral";

html += htmlAttr(makerworld_uid);

html += R"rawliteral(" placeholder="Digits from your MakerWorld profile URL">
    <div class="note">Shown in the printer area when no print is running (and 5 minutes after a print ends). Your profile URL looks like makerworld.com/en/@name &mdash; the numeric ID is in the URL of your published models list.</div>

    <hr>

    <label>PKGE API Key (package tracking)</label>
    <input name="pkge_api_key" placeholder=")rawliteral";

html += pkge_api_key[0] ? "Saved - leave empty to keep, - to remove" : "Paste the API key of your PKGE Business account";

html += R"rawliteral(">
    <div class="note">Add your packages on business.pkge.net; the dashboard shows the ones still on their way (and those delivered in the last day).</div>

    <label>AeroDataBox API Key (live flight status)</label>
    <input name="aerodatabox_key" placeholder=")rawliteral";

html += aerodatabox_key[0] ? "Saved - leave empty to keep, - to remove" : "Your X-RapidAPI-Key (optional)";

html += R"rawliteral(">
    <div class="note">Optional. On the day of a flight in your calendar, shows delays, gate and departed/landed. Free key: rapidapi.com &rarr; AeroDataBox &rarr; Basic plan.</div>

    <hr>

    <label>Pin Preset</label>
    <select id="pin_preset" name="pin_preset" onchange="toggleCustomPins()">
)rawliteral";

html += pinPresetOptions;

html += R"rawliteral(
    </select>
)rawliteral";

html += customPinInputs;

html += R"rawliteral(
)rawliteral";


html += R"rawliteral(
    <hr>

    <label>Layout</label>
    <select name="lay_tpl">)rawliteral";

for (int i = 0; i < layouts::TEMPLATE_COUNT; i++) {
  html += "<option value=\"";
  html += layouts::TEMPLATES[i].key;
  html += "\"";
  if (strcmp(layouts::TEMPLATES[i].key, layout_template) == 0) html += " selected";
  html += ">";
  html += layouts::TEMPLATES[i].name;
  html += "</option>";
}
// A layout made in the setup app: kept when this page is saved (handleSave ignores "custom").
if (strcmp(layout_template, "custom") == 0) html += "<option value=\"custom\" selected>Own layout (setup app)</option>";

html += R"rawliteral(</select>
    <div class="note">Classic: the original screen. Planner: clock, weather, upcoming events and month. Minimal: a large clock with weather and events. Technical: clock, printer, stocks, parcels, weather and events. A widget that is switched off or not set up leaves no gap.</div>

    <label>Clock</label>
    <select name="clk_24h">
      <option value="1")rawliteral";

html += clock_24h ? " selected" : "";

html += R"rawliteral(>24 hours (14:30)</option>
      <option value="0")rawliteral";

html += clock_24h ? "" : " selected";

html += R"rawliteral(>12 hours (2:30 PM)</option>
    </select>

    <label>Show</label>
)rawliteral";

for (int w = 0; w < layouts::WIDGET_COUNT; w++) {
  layouts::WidgetId id = static_cast<layouts::WidgetId>(w);
  if (id == layouts::WidgetId::StatusBar || id == layouts::WidgetId::Clock) continue;
  html += "    <label><input type=\"checkbox\" name=\"show_";
  html += widgetInfo(id).key;
  html += "\" value=\"1\" style=\"width:auto\"";
  if (widget_show_mask & layouts::widgetBit(id)) html += " checked";
  html += "> ";
  html += widgetInfo(id).name;
  html += "</label>\n";
}

html += R"rawliteral(
    <div class="note">A widget only appears when its setting above is filled in (and when the layout has a place for it).</div>

    <hr>

    <label>Firmware updates</label>
    <div class="note">)rawliteral";

html += otaStatusHtml();

html += R"rawliteral(</div>
    <label><input type="checkbox" name="ota_auto" value="1" style="width:auto")rawliteral";

html += otaAutoInstall ? " checked" : "";

html += R"rawliteral(> Install updates automatically</label>
    <div class="note">On: a new version is installed at night by itself. Off: the status bar shows "Update available"; install it with a double tap on the button, or below.</div>

    <label>Update channel</label>
    <select name="ota_channel">
      <option value="stable")rawliteral";

html += otaBeta ? "" : " selected";

html += R"rawliteral(>Stable (recommended)</option>
      <option value="beta")rawliteral";

html += otaBeta ? " selected" : "";

html += R"rawliteral(>Beta: also test releases</option>
    </select>
    <div class="note">Beta gets test releases first (and every stable release). Use it only on a device you can reach.</div>

    <button type="submit">Save Configuration</button>

  </form>

  <form action="/factory-reset" method="POST" onsubmit="return confirm('Erase ALL settings (Wi-Fi, calendars, keys, pins) and restart into setup mode?');">
    <button type="submit" class="scanBtn">Factory reset</button>
  </form>
  <div class="note">Erases every setting and starts again with the first setup. Without the setup page: tap RST, then type <b>factory-reset</b> into a USB serial monitor within 3 seconds.</div>

  <form action="/update" method="POST">
    <button type="submit" class="scanBtn">Check for updates now</button>
  </form>
  <div class="note">The device restarts, connects to your Wi-Fi and installs a newer version right away if there is one (it needs 30 % battery or USB). The screen shows the progress and the result.</div>

  <div class="footer">
    Device will restart after saving
  </div>
</div>

</body>
</html>
)rawliteral";

  return html;
}

void handleConfig() {
  apLastActivity = millis();  // someone is using the page: restart the timeout

  server.send(200, "text/html", buildPage());
}

// Setup page: installed version and the last update result.
static String otaStatusHtml() {
  preferences.begin("config", true);
  String result = preferences.getString("ota_result", "");
  uint32_t resultTime = preferences.getUInt("ota_rtime", 0);
  uint32_t updated = preferences.getUInt("ota_updt", 0);
  preferences.end();
  char date[16], line[96];
  String out;
  snprintf(line, sizeof(line), "Installed: %s (%s, %s), %s channel. ", FW_VERSION, FW_VARIANT, FW_DISPLAY,
           ota::channelName(otaBeta));
  out += line;
  formatDate(updated, date, sizeof(date));
  out += "Last update: ";
  out += date;
  out += ".";
  if (result.length()) {
    formatDate(resultTime, date, sizeof(date));
    out += "<br>Last check";
    if (strcmp(date, "never") != 0) {
      out += " (";
      out += date;
      out += ")";
    }
    out += ": ";
    out += htmlAttr(result.c_str());
  }
  if (otaAvailBuild > FW_BUILD) {
    out += "<br><b>Version ";
    out += htmlAttr(otaAvailVersion);
    out += " is available.</b>";
  }
  return out;
}

// "Check for updates now": restart, connect to Wi-Fi, check and install (the screen shows it).
void handleUpdateNow() {
  apLastActivity = millis();
  if (wifi_ssid[0] == '\0') {
    server.send(400, "text/plain", "Save your Wi-Fi settings first.");
    return;
  }
  updateRequest = UPDATE_REQUEST_MAGIC;
  server.send(200, "text/html",
    "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<title>Checking for updates</title></head><body style=\"font-family:Arial,Helvetica,sans-serif;text-align:center;padding:30px\">"
    "<h2>Checking for updates</h2><p>The device restarts and connects to your Wi-Fi. "
    "Its screen shows the result; a newer version is installed right away.</p>"
    "<p>You can close this page.</p></body></html>");
  delay(1500);
  ESP.restart();
}

void handleSave() {
  // Check every field first: if one is wrong, nothing is saved and the page says what to fix.
  String errors;
  char tzRule[CFG_TIMEZONE_MAX];
  if (!normalizeTimezone(server.arg("device_timezone").c_str(), tzRule, sizeof(tzRule)))
    errors += "Timezone \"" + server.arg("device_timezone") +
              "\" not recognized: use a rule like CET-1CEST,M3.5.0,M10.5.0/3 (Germany) or a name like CET or Europe/Berlin.\n";
  char icsClean[ICS_MAX_FEEDS][CFG_ICS_URL_MAX];
  for (int i = 0; i < ICS_MAX_FEEDS; i++)
    if (!validateIcsUrl(server.arg(ICS_PREF_KEYS[i]).c_str(), icsClean[i], sizeof(icsClean[i])))
      errors += "Calendar link " + String(i + 1) + " is not an https://, http:// or webcal:// address.\n";
  char bdayClean[CFG_ICS_URL_MAX];
  if (!validateIcsUrl(server.arg("bday_url").c_str(), bdayClean, sizeof(bdayClean)))
    errors += "Birthday calendar link is not an https://, http:// or webcal:// address.\n";
  char stocksClean[CFG_STOCKS_MAX];
  if (!normalizeStockSymbols(server.arg("stock_symbols").c_str(), stocksClean, sizeof(stocksClean)))
    errors += "Stock symbols: Yahoo Finance symbols (letters, digits, ^ . = -) separated by commas, e.g. AAPL,NVDA,^IXIC.\n";
  int port = parseMqttPort(server.arg("mqtt_port").c_str());
  bool printerSet = server.arg("mqtt_ip").length() > 0;
  if (port < 0) {
    if (printerSet) errors += "Bambu MQTT port must be a number from 1 to 65535 (usually 8883).\n";
    else port = 8883;  // no printer: the port doesn't matter
  }
  if (!validatePrinterAddress(server.arg("mqtt_ip").c_str()))
    errors += "Bambu printer address: an address in your home network (e.g. 192.168.1.100) or the printer's name there, not a public address.\n";
  PinPreset preset = parsePinPreset(server.arg("pin_preset"));
  PinConfig pins = customPins;
  pins.epdCs = readPinField("pin_epd_cs");
  pins.epdDc = readPinField("pin_epd_dc");
  pins.epdRst = readPinField("pin_epd_rst");
  pins.epdBusy = readPinField("pin_epd_busy");
  pins.epdSck = readPinField("pin_epd_sck");
  pins.epdMosi = readPinField("pin_epd_mosi");
  pins.displayPower = readPinField("pin_display_power");
  pins.battery = readPinField("pin_battery");
  pins.demoButton = readPinField("pin_demo_button");
  if (preset == PinPreset::Custom && !hasRequiredDisplayPins(pins))
    errors += "Custom pins: CS, DC, RST, BUSY, SCK and MOSI are required.\n";
  if (errors.length()) {
    server.send(400, "text/plain", "Nothing was saved:\n\n" + errors + "\nGo back, correct it and save again.");
    return;
  }

  // Empty (no network found in the scan) keeps the saved network, like the empty password does.
  if (server.arg("wifi_ssid").length() > 0)
    copyToCfg(wifi_ssid, sizeof(wifi_ssid), server.arg("wifi_ssid"));
  // Empty password fields keep the saved value, so re-opening setup doesn't require retyping them.
  if (server.arg("wifi_pass").length() > 0)
    copyToCfg(wifi_pass, sizeof(wifi_pass), server.arg("wifi_pass"));
  copyToCfg(device_timezone, sizeof(device_timezone), tzRule);
  copyToCfg(weather_location, sizeof(weather_location), server.arg("weather_location"));
  copyToCfg(stock_symbols, sizeof(stock_symbols), stocksClean);
  for (int i = 0; i < ICS_MAX_FEEDS; i++)
    copyToCfg(ics_urls[i], sizeof(ics_urls[i]), icsClean[i]);
  copyToCfg(bday_url, sizeof(bday_url), bdayClean);
  copyToCfg(makerworld_uid, sizeof(makerworld_uid), server.arg("makerworld_uid"));
  copyToCfg(printer_name, sizeof(printer_name), server.arg("printer_name"));
  // Empty keeps the saved key; a single "-" removes it (hides the tracking widget).
  if (server.arg("pkge_api_key") == "-") pkge_api_key[0] = '\0';
  else if (server.arg("pkge_api_key").length() > 0)
    copyToCfg(pkge_api_key, sizeof(pkge_api_key), server.arg("pkge_api_key"));
  if (server.arg("aerodatabox_key") == "-") aerodatabox_key[0] = '\0';
  else if (server.arg("aerodatabox_key").length() > 0)
    copyToCfg(aerodatabox_key, sizeof(aerodatabox_key), server.arg("aerodatabox_key"));
  if (server.arg("mqtt_pass").length() > 0)
    copyToCfg(mqtt_pass, sizeof(mqtt_pass), server.arg("mqtt_pass"));
  copyToCfg(mqtt_sn, sizeof(mqtt_sn), server.arg("mqtt_sn"));
  copyToCfg(mqtt_ip, sizeof(mqtt_ip), server.arg("mqtt_ip"));
  mqtt_port = port;
  activePinPreset = preset;
  customPins = pins;
  applyStoredPinSelection();
  otaAutoInstall = server.hasArg("ota_auto");  // an unticked checkbox isn't sent
  otaBeta = server.arg("ota_channel") == "beta";
  if (layouts::templateIndex(server.arg("lay_tpl").c_str()) >= 0)
    copyToCfg(layout_template, sizeof(layout_template), server.arg("lay_tpl"));
  clock_24h = server.arg("clk_24h") != "0";
  widget_show_mask = 0xFFFF;
  for (int w = 0; w < layouts::WIDGET_COUNT; w++) {
    layouts::WidgetId id = static_cast<layouts::WidgetId>(w);
    if (id == layouts::WidgetId::StatusBar || id == layouts::WidgetId::Clock) continue;
    if (!server.hasArg(String("show_") + widgetInfo(id).key)) widget_show_mask &= ~layouts::widgetBit(id);
  }

  Serial.printf("MQTT P: %u SN: %s IP %s\n", mqtt_port, mqtt_sn, mqtt_ip);
  configSave();

server.send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Saved</title>

<style>
body{
  margin:0;
  font-family: Arial, Helvetica, sans-serif;
  display:flex;
  justify-content:center;
  align-items:center;
  height:100vh;
  color:black;
}

.card{
  background: rgba(255,255,255,0.1);
  backdrop-filter: blur(10px);
  padding:30px;
  border-radius:16px;
  text-align:center;
  box-shadow:0 8px 25px rgba(0,0,0,0.2);
  max-width:320px;
}

h2{
  margin-bottom:10px;
}

p{
  margin-top:5px;
  font-size:14px;
  opacity:0.9;
}

.spinner{
  margin:20px auto;
  width:40px;
  height:40px;
  border:4px solid rgba(255,255,255,0.3);
  border-top:4px solid white;
  border-radius:50%;
  animation: spin 1s linear infinite;
}

@keyframes spin{
  100% { transform: rotate(360deg); }
}

.countdown{
  margin-top:10px;
  font-size:13px;
  opacity:0.8;
}
</style>

<script>
let seconds = 3;
function updateCountdown(){
  document.getElementById("count").innerText = seconds;
  if(seconds > 0){
    seconds--;
    setTimeout(updateCountdown, 1000);
  }
}
window.onload = updateCountdown;
</script>
</head>

<body>

<div class="card">
  <h2>✅ Configuration Saved</h2>
  <p>Your device will reboot shortly</p>

  <div class="spinner"></div>

  <div class="countdown">
    Rebooting in <span id="count">3</span> seconds...
  </div>
</div>

</body>
</html>
)rawliteral");

delay(3000);
ESP.restart();

}


static bool hotspotOnly() {
  if (setupFromHotspot()) return true;
  server.send(403, "text/plain", "Only from the setup hotspot");
  return false;
}

// Routes of both modes. The old page (/basic) has no session check: hotspot only.
static void registerRoutes(bool hotspot) {
  server.on("/", HTTP_GET, handleApp);
  registerApiRoutes(server);
  if (hotspot) {
    // The old page: only over the hotspot (after a Wi-Fi test the device is in the home network too).
    server.on("/basic", []() { if (hotspotOnly()) handleConfig(); });
    server.on("/save", HTTP_POST, []() { if (hotspotOnly()) handleSave(); });
    server.on("/update", HTTP_POST, []() { if (hotspotOnly()) handleUpdateNow(); });
    server.on("/factory-reset", HTTP_POST, []() {
      if (!hotspotOnly()) return;
      server.send(200, "text/html",
        "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<title>Factory reset</title></head><body style=\"font-family:Arial,Helvetica,sans-serif;text-align:center;padding:30px\">"
        "<h2>Factory reset</h2><p>All settings are erased. The device restarts into setup mode: join "
        AP_SSID " again with the new password shown on the display.</p></body></html>");
      delay(1500);
      configFactoryReset();
    });
    // OS connectivity checks (Android /generate_204, Apple /hotspot-detect.html, Windows
    // /connecttest.txt) and any other URL get redirected to the app.
    server.onNotFound([]() {
      server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
      server.send(302, "text/plain", "");
    });
  } else {
    server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  }
  server.begin();
}

// Setup hold: the app on the home network when Wi-Fi works (edit mode), else the hotspot.
void startSetup() {
  if (wifi_ssid[0] && wifi_pass[0]) startEditMode();
  else startAP();
}

void startAP() {
  otaConfirmByUser("setup mode");
  setCpuFrequencyMhz(cpuFullMhz);
  apModeActive = true;
  setupKind = SetupKind::Ap;
  apLastActivity = millis();
  loadApPassword();

  Serial.println("Start AP");
  // WPA2 with the per-device password shown on the display (the hotspot was open before stage 5).
  if (WiFi.softAP(AP_SSID, apPassword)) {
    Serial.printf("AP started OK: %s\n", WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("AP failed!");
  }

  // Captive portal: answer every DNS lookup with our IP so phones/laptops pop up the setup app.
  dnsServer.start(53, "*", WiFi.softAPIP());
  registerRoutes(true);
  DBG(F("AP and setup app started"));

  drawSetupScreenAp(AP_SSID, apPassword);
  DBG(F("Display configured"));
}

// Edit mode (stage 5, plan A.8): the app on the home network for 10 minutes, behind the code shown
// on the screen. Without a connection: the hotspot instead.
void startEditMode() {
  otaConfirmByUser("edit mode");
  setCpuFrequencyMhz(cpuFullMhz);
  Serial.println("Edit mode: connecting to Wi-Fi");
  if (!startWiFiReliable(wifi_ssid, wifi_pass)) {
    Serial.println("Edit mode: no Wi-Fi -> setup hotspot");
    WiFi.disconnect(true);
    startAP();
    return;
  }
  apModeActive = true;
  setupKind = SetupKind::Home;
  apLastActivity = millis();
  initTime();
  makeHostName();
  if (MDNS.begin(hostName)) MDNS.addService("http", "tcp", 80);
  newLoginCode();
  registerRoutes(false);
  String ip = WiFi.localIP().toString();
  Serial.printf("Edit mode: http://%s.local or http://%s\n", hostName, ip.c_str());
  char host[48];
  snprintf(host, sizeof(host), "%s.local", hostName);
  drawSetupScreenHome(host, ip.c_str(), loginCode);
  codeOnScreen = true;
}


int16_t readPinField(const String& name) {
  if (!server.hasArg(name)) return PIN_UNASSIGNED;
  String value = server.arg(name);
  value.trim();
  if (value.length() == 0) return PIN_UNASSIGNED;
  return static_cast<int16_t>(value.toInt());
}


// Serves the setup app while setup or edit mode is on (called from loop()).
void setupModeLoop() {
  if (!apModeActive && wifi_ssid[0] != '\0' && wifi_pass[0] != '\0') return;
  if (setupKind == SetupKind::Ap) dnsServer.processNextRequest();
  server.handleClient();

  if (dashboardPending) {  // a layout or personal data was saved: show it (no restart)
    dashboardPending = false;
    drawDashboardNow();
  }
  if (homeScreenPending) {  // too many wrong codes: a new code on the screen
    homeScreenPending = false;
    char host[48];
    snprintf(host, sizeof(host), "%s.local", hostName);
    drawSetupScreenHome(host, WiFi.localIP().toString().c_str(), loginCode);
    codeOnScreen = true;
  }
  if (restartAt && (long)(millis() - restartAt) >= 0) {
    Serial.println("Setup: restarting");
    delay(100);
    ESP.restart();
  }
  // Opened by mistake or forgotten (already configured): back to the dashboard after a while
  // instead of keeping the radio on and draining the battery.
  unsigned long timeout = setupKind == SetupKind::Home ? EDIT_TIMEOUT_MS : AP_TIMEOUT_MS;
  if (wifi_ssid[0] != '\0' && wifi_pass[0] != '\0' && millis() - apLastActivity > timeout) {
    Serial.println("Setup mode timed out -> restarting");
    ESP.restart();
  }
}
