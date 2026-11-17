#pragma once
// Checks of the setup page input, before anything is saved (ConfigManager). The timezone check
// is in Timezone.h. Plain C++, tested on the PC (tests/host/test_config_validation.cpp).
#include <cctype>
#include <cstdio>
#include <cstring>

#include "../services/LanAddress.h"

namespace cfgdetail {

inline void trim(const char*& s, size_t& n) {
  while (n && std::isspace(static_cast<unsigned char>(*s))) { s++; n--; }
  while (n && std::isspace(static_cast<unsigned char>(s[n - 1]))) n--;
}

inline bool startsWithNoCase(const char* s, size_t n, const char* prefix) {
  size_t m = std::strlen(prefix);
  if (n < m) return false;
  for (size_t i = 0; i < m; i++)
    if (std::tolower(static_cast<unsigned char>(s[i])) != prefix[i]) return false;
  return true;
}

}  // namespace cfgdetail

// iCal feed URL: empty, or https://host/... . webcal:// and webcals:// (Apple / Outlook
// "subscribe" links) and http:// become https:// (every request uses TLS, so plain http:// could
// never work). The result goes to `out`; false if unusable.
inline bool validateIcsUrl(const char* in, char* out, size_t cap) {
  if (!in || !out || cap == 0) return false;
  size_t n = std::strlen(in);
  cfgdetail::trim(in, n);
  const char* scheme = nullptr;
  size_t skip = 0;
  if (cfgdetail::startsWithNoCase(in, n, "https://")) { scheme = "https://"; skip = 8; }
  else if (cfgdetail::startsWithNoCase(in, n, "http://")) { scheme = "https://"; skip = 7; }
  else if (cfgdetail::startsWithNoCase(in, n, "webcals://")) { scheme = "https://"; skip = 10; }
  else if (cfgdetail::startsWithNoCase(in, n, "webcal://")) { scheme = "https://"; skip = 9; }
  if (n == 0) {
    out[0] = '\0';
    return true;
  }
  if (!scheme || n <= skip) return false;  // unknown scheme, or no host
  for (size_t i = 0; i < n; i++)
    if (std::isspace(static_cast<unsigned char>(in[i])) || static_cast<unsigned char>(in[i]) < 0x20) return false;
  int w = std::snprintf(out, cap, "%s%.*s", scheme, static_cast<int>(n - skip), in + skip);
  return w > 0 && static_cast<size_t>(w) < cap;
}

// Yahoo Finance symbols: letters, digits and ^ . = - (e.g. ^IXIC, BRK-B, EURUSD=X, SAP.DE),
// separated by commas and/or spaces. Written to `out` as "A,B,C": empty entries dropped, at most
// 5 (the widget shows 5; the fetcher always used the first 5). False for any other character.
inline bool normalizeStockSymbols(const char* in, char* out, size_t cap) {
  if (!in || !out || cap == 0) return false;
  size_t o = 0;
  int count = 0;
  const char* p = in;
  out[0] = '\0';
  while (*p) {
    while (*p == ' ' || *p == ',') p++;
    const char* start = p;
    while (*p && *p != ' ' && *p != ',') {
      char c = *p;
      if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '^' || c == '.' || c == '=' || c == '-')) return false;
      p++;
    }
    size_t len = static_cast<size_t>(p - start);
    if (len == 0 || count >= 5) continue;
    if (o + (count ? 1 : 0) + len + 1 > cap) return false;
    if (count) out[o++] = ',';
    std::memcpy(out + o, start, len);
    o += len;
    out[o] = '\0';
    count++;
  }
  return true;
}

// MQTT port field: empty = 8883 (the printer's TLS port); -1 if not a port number.
inline int parseMqttPort(const char* s) {
  if (!s) return -1;
  size_t n = std::strlen(s);
  cfgdetail::trim(s, n);
  if (n == 0) return 8883;
  long v = 0;
  for (size_t i = 0; i < n; i++) {
    if (!std::isdigit(static_cast<unsigned char>(s[i]))) return -1;
    v = v * 10 + (s[i] - '0');
    if (v > 65535) return -1;
  }
  return v >= 1 ? static_cast<int>(v) : -1;
}

// Printer address: empty (no printer), a private IPv4 address, or a name (".local", or one from
// the router's DNS; its resolved address is checked when connecting). Never a public address: the
// printer's MQTT TLS has no certificate check (see LanAddress.h).
inline bool validatePrinterAddress(const char* s) {
  if (!s) return false;
  char buf[256];
  size_t n = std::strlen(s);
  cfgdetail::trim(s, n);
  if (n == 0) return true;
  if (n >= sizeof(buf)) return false;
  std::memcpy(buf, s, n);
  buf[n] = '\0';
  return isLocalNetworkHost(buf) || looksLikeHostname(buf);
}

// Settings schema (FW_CONFIG_SCHEMA): migrate stored settings that are older than this firmware.
// Newer ones (a downgrade over USB) are left alone.
inline bool configNeedsMigration(int stored, int current) {
  return stored < current;
}
