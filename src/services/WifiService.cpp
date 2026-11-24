// Wi-Fi station connect (stage 7b, own code): straight to the access point of the last connection
// (channel and BSSID kept in deep sleep: no scan of every channel), else two full attempts.
#include "WifiService.h"

#include <WiFi.h>

#include "../../configure.h"

RTC_DATA_ATTR uint8_t wifiBssid[6];
RTC_DATA_ATTR int32_t wifiChannel = 0;  // 0 = unknown: scan
RTC_DATA_ATTR int8_t lastRssi = 0;     // the status bar's Wi-Fi bars

namespace {

constexpr unsigned long FAST_TIMEOUT_MS = 5000, FULL_TIMEOUT_MS = 20000;

bool waitConnected(unsigned long timeoutMs) {
  for (unsigned long start = millis(); WiFi.status() != WL_CONNECTED; delay(20))
    if (millis() - start > timeoutMs) return false;
  return true;
}

void stationMode() {
  WiFi.persistent(false);  // before mode(): the driver picks its storage there; ours is NVS, no flash copy
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);  // on only after a connection
  WiFi.setSleep(false);
}

bool connected(unsigned long t0) {
  memcpy(wifiBssid, WiFi.BSSID(), sizeof(wifiBssid));
  wifiChannel = WiFi.channel();
  lastRssi = (int8_t)WiFi.RSSI();
  WiFi.setAutoReconnect(true);
  DBGF("WiFi connected in %lu ms (%d dBm)", millis() - t0, (int)lastRssi);
  return true;
}

}  // namespace

bool startWiFiReliable(const char* ssid, const char* password) {
  const unsigned long t0 = millis();
  stationMode();
  if (wifiChannel > 0) {
    DBGF("WiFi fast connect, channel %d", (int)wifiChannel);
    WiFi.begin(ssid, password, wifiChannel, wifiBssid);
    if (waitConnected(FAST_TIMEOUT_MS)) return connected(t0);
    DBG(F("Fast connect failed, scanning"));
    wifiChannel = 0;  // the router moved or changed its channel
  }
  for (int attempt = 1; attempt <= 2; attempt++) {
    DBGF("WiFi attempt %d", attempt);
    WiFi.disconnect(true, true);  // a clean start for each attempt
    WiFi.mode(WIFI_OFF);
    delay(100);
    stationMode();
    WiFi.begin(ssid, password);
    if (waitConnected(FULL_TIMEOUT_MS)) return connected(t0);
  }
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  lastRssi = 0;  // no bars after a failed connection
  return false;
}
