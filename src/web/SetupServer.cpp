// Setup mode: the "Dashboard-Setup" access point with a captive portal, or edit mode on the home
// network, both serving the setup app (web/) and its API (Api.cpp). Opened at the first start, with
// a 4 s hold, or BOOT after RST.
#include "SetupServer.h"

#include <Arduino.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <SPI.h>
#include <WebServer.h>
#include <WiFi.h>

#include "../../app.h"
#include "../core/AppState.h"
#include "../core/ConfigManager.h"
#include "../core/Timezone.h"
#include "../core/ConfigValidation.h"
#include "../hardware/EpaperDriver.h"
#include "../hardware/PinMapping.h"
#include "../hardware/TouchController.h"
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

// Edit mode across a restart (settings saved): code and session survive a software restart in
// RTC memory that isn't cleared at boot.
struct EditSession {
  uint32_t magic;
  char code[7];
  char token[33];
  uint8_t rotations;
};
constexpr uint32_t EDIT_SESSION_MAGIC = 0xED17C0DE;
RTC_NOINIT_ATTR static EditSession keptSession;

void setupScheduleRestartSameMode(uint32_t delayMs) {
  if (setupKind == SetupKind::Home) {
    keptSession.magic = EDIT_SESSION_MAGIC;
    memcpy(keptSession.code, loginCode, sizeof(keptSession.code));
    memcpy(keptSession.token, sessionToken, sizeof(keptSession.token));
    keptSession.rotations = codeRotations;
    setupRequest = SETUP_REQUEST_EDIT_MAGIC;
  } else if (setupKind == SetupKind::Ap) {
    setupRequest = SETUP_REQUEST_AP_MAGIC;
  }
  setupScheduleRestart(delayMs);
}
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
        logPrintln("Edit mode: too many wrong codes -> leaving");
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
  // For the setup app: in its language.
  if (st == WL_NO_SSID_AVAIL) snprintf(err, cap, "%s", tr(i18n::S::WifiNotFound));
  else if (st == WL_CONNECT_FAILED) snprintf(err, cap, "%s", tr(i18n::S::WifiWrongPass));
  else snprintf(err, cap, "%s", tr(i18n::S::WifiNoConnection));
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


// Routes of both modes: the setup app and its API (the old page /basic is gone since stage 7b).
static void registerRoutes(bool hotspot) {
  server.on("/", HTTP_GET, handleApp);
  registerApiRoutes(server);
  if (hotspot) {
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
  logKeepThisWake();
  setCpuFrequencyMhz(cpuFullMhz);
  apModeActive = true;
  setupKind = SetupKind::Ap;
  apLastActivity = millis();
  loadApPassword();

  logPrintln("Start AP");
  // WPA2 with the per-device password shown on the display (the hotspot was open before stage 5).
  if (WiFi.softAP(AP_SSID, apPassword)) {
    logPrintf("AP started OK: %s\n", WiFi.softAPIP().toString().c_str());
  } else {
    logPrintln("AP failed!");
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
  logKeepThisWake();
  setCpuFrequencyMhz(cpuFullMhz);
  logPrintln("Edit mode: connecting to Wi-Fi");
  if (!startWiFiReliable(wifi_ssid, wifi_pass)) {
    logPrintln("Edit mode: no Wi-Fi -> setup hotspot");
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
  if (keptSession.magic == EDIT_SESSION_MAGIC) {  // back after saving settings: same code, same session
    keptSession.magic = 0;
    memcpy(loginCode, keptSession.code, sizeof(loginCode));
    loginCode[sizeof(loginCode) - 1] = '\0';
    memcpy(sessionToken, keptSession.token, sizeof(sessionToken));
    sessionToken[sizeof(sessionToken) - 1] = '\0';
    codeRotations = keptSession.rotations;
    wrongCodes = 0;
  } else {
    newLoginCode();
  }
  registerRoutes(false);
  String ip = WiFi.localIP().toString();
  logPrintf("Edit mode: http://%s.local or http://%s\n", hostName, ip.c_str());
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
    logPrintln("Setup: restarting");
    logFlush(true);
    delay(100);
    ESP.restart();
  }
  // Opened by mistake or forgotten (already configured): back to the dashboard after a while
  // instead of keeping the radio on and draining the battery.
  unsigned long timeout = setupKind == SetupKind::Home ? EDIT_TIMEOUT_MS : AP_TIMEOUT_MS;
  if (wifi_ssid[0] != '\0' && wifi_pass[0] != '\0' && millis() - apLastActivity > timeout) {
    logPrintln("Setup mode timed out -> restarting");
    logFlush(true);
    ESP.restart();
  }
}
