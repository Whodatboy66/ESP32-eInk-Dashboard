#pragma once
// Wi-Fi station connect: fast path to the access point used last time (channel + BSSID kept in RTC
// memory), otherwise two full attempts with a scan.
bool startWiFiReliable(const char* ssid, const char* password);
