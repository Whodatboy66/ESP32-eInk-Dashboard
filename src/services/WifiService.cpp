#include "WifiService.h"

#include <WiFi.h>

#include "../../configure.h"

// Access point of the last successful connection, kept across deep sleep. Connecting with a known
// channel and BSSID skips the scan of every channel, which keeps the radio on for seconds.
RTC_DATA_ATTR uint8_t wifiBssid[6];
RTC_DATA_ATTR int32_t wifiChannel = 0;  // 0 = unknown, scan

static bool waitForWiFi(unsigned long timeoutMs) {
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeoutMs) return false;
    delay(20);
  }
  DBGF("WiFi connected in %lu ms", millis() - start);
  return true;
}

bool startWiFiReliable(const char* ssid, const char* password)
{
  DBG(F("startWiFiReliable called"));

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);   // IMPORTANT: disable during manual connect
  WiFi.setSleep(false);

  // Fast path: straight to the access point used last time.
  if (wifiChannel > 0) {
    DBGF("WiFi fast connect, channel %d", (int)wifiChannel);
    WiFi.begin(ssid, password, wifiChannel, wifiBssid);
    if (waitForWiFi(5000)) {
      WiFi.setAutoReconnect(true); // enable AFTER connection
      return true;
    }
    DBG(F("Fast connect failed, scanning"));
    wifiChannel = 0;  // the router moved or changed channel: forget it
  }

  for (int attempt = 1; attempt <= 2; attempt++)
  {
    DBGF("\nWiFi attempt %d", attempt);

    // FULL STOP FIRST
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    delay(100);

    // THEN re-init cleanly
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.setSleep(false);

    WiFi.begin(ssid, password);

    if (waitForWiFi(20000))
    {
      DBG(WiFi.localIP());
      memcpy(wifiBssid, WiFi.BSSID(), sizeof(wifiBssid));
      wifiChannel = WiFi.channel();

      WiFi.setAutoReconnect(true); // enable AFTER connection
      return true;
    }

    DBG(F("\nFailed. Retrying..."));
  }

  // Final cleanup
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);

  return false;
}

