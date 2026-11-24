#pragma once
// Wi-Fi station connect: fast path to the access point used last time (channel + BSSID kept in RTC
// memory), otherwise two full attempts with a scan.
bool startWiFiReliable(const char* ssid, const char* password);

#include <stdint.h>
// Signal of the last successful connection in dBm (0 = none yet), for the status bar's Wi-Fi bars.
extern int8_t lastRssi;
