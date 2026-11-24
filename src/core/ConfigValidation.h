#pragma once
// Checks of the setup page input, before anything is saved (ConfigManager). The timezone check
// is in Timezone.h. Plain C++, tested on the PC (tests/host/test_config_validation.cpp).
#include <cctype>
#include <cstdio>
#include <cstring>

#include "../services/LanAddress.h"
#include "Holidays.h"

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
// Stage 7: the Bundesland for holidays ("" = off, else one of the 16 codes, upper case).
inline bool validStateCode(const char* code) {
  return code && (!code[0] || holidays::parseState(code) != holidays::State::None);
}

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
// Stocks setting: up to 5 entries separated by commas, each "SYMBOL" (Yahoo Finance: letters,
// digits, ^ . = -) or "SYMBOL:Label" (own name on the screen, up to STOCK_LABEL_CHARS characters,
// e.g. "EUNL.DE:MSCI World"). Without labels, spaces separate symbols too ("AAPL MSFT"), as before.
// Writes the cleaned list ("EUNL.DE:MSCI World,PPFB.DE:Gold"); false if something is invalid.
constexpr size_t STOCK_LABEL_CHARS = 12;
constexpr int STOCK_MAX_ENTRIES = 5;

inline bool stockSymbolChar(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '^' || c == '.' || c == '=' || c == '-';
}

inline bool normalizeStockSymbols(const char* in, char* out, size_t cap) {
  if (!in || !out || cap == 0) return false;
  size_t o = 0;
  int count = 0;
  out[0] = '\0';
  auto append = [&](const char* sym, size_t symLen, const char* label, size_t labelLen) -> bool {
    if (count >= STOCK_MAX_ENTRIES) return true;  // more than 5: ignored, as before
    size_t need = (count ? 1 : 0) + symLen + (labelLen ? 1 + labelLen : 0) + 1;
    if (o + need > cap) return false;
    if (count) out[o++] = ',';
    std::memcpy(out + o, sym, symLen);
    o += symLen;
    if (labelLen) {
      out[o++] = ':';
      std::memcpy(out + o, label, labelLen);
      o += labelLen;
    }
    out[o] = '\0';
    count++;
    return true;
  };
  const char* p = in;
  while (*p) {
    const char* start = p;
    while (*p && *p != ',') p++;
    const char* end = p;
    if (*p == ',') p++;
    while (start < end && *start == ' ') start++;
    while (end > start && end[-1] == ' ') end--;
    if (start == end) continue;
    const char* colon = static_cast<const char*>(std::memchr(start, ':', static_cast<size_t>(end - start)));
    if (colon) {  // "SYMBOL:Label"
      const char* se = colon;
      while (se > start && se[-1] == ' ') se--;
      if (se == start) return false;
      for (const char* q = start; q < se; q++)
        if (!stockSymbolChar(*q)) return false;
      const char* ls = colon + 1;
      while (ls < end && *ls == ' ') ls++;
      size_t chars = 0;
      for (const char* q = ls; q < end; q++) {
        unsigned char c = static_cast<unsigned char>(*q);
        if (c < 0x20 || c == ':') return false;
        if ((c & 0xC0) != 0x80) chars++;  // count characters, not UTF-8 bytes
      }
      if (chars == 0 || chars > STOCK_LABEL_CHARS) return false;
      if (!append(start, static_cast<size_t>(se - start), ls, static_cast<size_t>(end - ls))) return false;
    } else {  // one or more plain symbols, separated by spaces
      const char* q = start;
      while (q < end) {
        while (q < end && *q == ' ') q++;
        const char* s0 = q;
        while (q < end && *q != ' ') {
          if (!stockSymbolChar(*q)) return false;
          q++;
        }
        if (q > s0 && !append(s0, static_cast<size_t>(q - s0), nullptr, 0)) return false;
      }
    }
  }
  return true;
}

// One entry of the cleaned stocks setting.
struct StockEntry {
  char symbol[24];  // for the request
  char label[52];   // for the screen: the label, else the symbol without a leading ^
};

// Splits the cleaned setting into entries; returns how many (at most max).
inline int parseStockEntries(const char* list, StockEntry* out, int max) {
  int n = 0;
  const char* p = list;
  while (p && *p && n < max) {
    const char* end = std::strchr(p, ',');
    if (!end) end = p + std::strlen(p);
    const char* colon = static_cast<const char*>(std::memchr(p, ':', static_cast<size_t>(end - p)));
    const char* se = colon ? colon : end;
    StockEntry& e = out[n];
    std::snprintf(e.symbol, sizeof(e.symbol), "%.*s", static_cast<int>(se - p), p);
    if (colon) std::snprintf(e.label, sizeof(e.label), "%.*s", static_cast<int>(end - colon - 1), colon + 1);
    else std::snprintf(e.label, sizeof(e.label), "%s", e.symbol[0] == '^' ? e.symbol + 1 : e.symbol);
    if (e.symbol[0]) n++;
    p = *end ? end + 1 : end;
  }
  return n;
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
