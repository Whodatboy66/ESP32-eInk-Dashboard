#pragma once
// The services' answers to the dashboard's data (stage 7b, own code): Open-Meteo (weather, places),
// Yahoo Finance (quotes), PKGE (parcels), AeroDataBox (flight status), MakerWorld (design counters).
// Plain C++ with ArduinoJson, tested on the PC with recorded answers
// (tests/host/test_service_parsers.cpp); the download code is in Weather.cpp, Stocks.cpp, ...
#include <ArduinoJson.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "../core/CivilDate.h"
#include "ServiceData.h"

namespace svc {

inline int digitsAt(const char* s, int n) {
  int v = 0;
  for (int i = 0; i < n; i++) {
    if (!std::isdigit(static_cast<unsigned char>(s[i]))) return -1;
    v = v * 10 + (s[i] - '0');
  }
  return v;
}

// "2021-07-06T13:46:25+03:00", "2026-10-15 18:05Z", "2026-10-15" (UTC midnight) to UTC seconds;
// seconds, fractions and the offset are optional. 0 if it isn't such a time.
inline time_t parseIsoUtc(const char* s) {
  if (!s || std::strlen(s) < 10 || s[4] != '-' || s[7] != '-') return 0;
  int y = digitsAt(s, 4), mo = digitsAt(s + 5, 2), d = digitsAt(s + 8, 2);
  if (y < 0 || mo < 1 || mo > 12 || d < 1 || d > 31) return 0;
  time_t t = static_cast<time_t>(civil::daysFromCivil(y, mo, d)) * 86400;
  if (s[10] != 'T' && s[10] != ' ') return t;
  const char* p = s + 11;
  int h = digitsAt(p, 2), mi = p[2] == ':' ? digitsAt(p + 3, 2) : -1;
  if (h < 0 || mi < 0) return t;
  p += 5;
  int sec = 0;
  if (*p == ':' && digitsAt(p + 1, 2) >= 0) {
    sec = digitsAt(p + 1, 2);
    p += 3;
  }
  if (*p == '.')
    while (*++p && std::isdigit(static_cast<unsigned char>(*p))) {}
  t += h * 3600 + mi * 60 + sec;
  if (*p == '+' || *p == '-') {
    int oh = digitsAt(p + 1, 2), om = p[3] == ':' ? digitsAt(p + 4, 2) : digitsAt(p + 3, 2);
    if (oh >= 0) t -= (*p == '+' ? 1 : -1) * (oh * 3600 + (om > 0 ? om : 0) * 60);
  }
  return t;
}

// Percent-encoding of everything but A-Z a-z 0-9 - _ . ~ (also UTF-8 bytes); never cut inside an escape.
inline void urlEncode(const char* in, char* out, size_t cap) {
  static const char hex[] = "0123456789ABCDEF";
  size_t o = 0;
  for (; *in; in++) {
    unsigned char c = static_cast<unsigned char>(*in);
    bool plain = std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~';
    if (o + (plain ? 1 : 3) >= cap) break;
    if (plain) {
      out[o++] = static_cast<char>(c);
    } else {
      out[o++] = '%';
      out[o++] = hex[c >> 4];
      out[o++] = hex[c & 0x0F];
    }
  }
  if (cap) out[o] = '\0';
}

// ---- Weather (Open-Meteo) ----

// WMO weather code to the icon (Material Design Icons, MDI_80_Sparse).
inline uint32_t weatherIcon(int code) {
  switch (code) {
    case 0: return 0xF0599;                              // sunny
    case 1: case 2: return 0xF0595;                      // partly cloudy
    case 45: case 48: return 0xF0591;                    // fog
    case 51: case 53: case 55: return 0xF0597;           // drizzle
    case 61: case 63: case 65: case 80: case 81: case 82: return 0xF0596;  // rain, showers
    case 71: case 73: case 75: case 85: case 86: return 0xF0598;           // snow
    case 95: case 96: case 99: return 0xF067E;           // thunderstorm
    default: return 0xF0590;                             // cloudy (3) and anything else
  }
}

enum class Place { None, Coordinates, Name };

// The weather setting: "48.14, 11.58" (coordinates), "City" or "City, CC" (CC: a 2-letter country).
inline Place readPlaceSetting(const char* setting, float& lat, float& lon, char* city, size_t cityCap, char country[3]) {
  country[0] = '\0';
  if (!setting || !setting[0]) return Place::None;
  if (std::sscanf(setting, "%f , %f", &lat, &lon) == 2) return Place::Coordinates;
  std::snprintf(city, cityCap, "%s", setting);
  if (char* comma = std::strchr(city, ',')) {
    *comma = '\0';
    const char* cc = comma + 1;
    while (*cc == ' ') cc++;
    size_t n = std::strlen(cc);
    while (n && cc[n - 1] == ' ') n--;
    if (n == 2) {
      country[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(cc[0])));
      country[1] = static_cast<char>(std::toupper(static_cast<unsigned char>(cc[1])));
      country[2] = '\0';
    }
  }
  for (size_t n = std::strlen(city); n && city[n - 1] == ' '; n--) city[n - 1] = '\0';
  return city[0] ? Place::Name : Place::None;
}

inline bool firstPlace(JsonVariantConst doc, float& lat, float& lon) {
  JsonVariantConst r = doc["results"][0];
  if (r.isNull() || r["latitude"].isNull() || r["longitude"].isNull()) return false;
  lat = r["latitude"].as<float>();
  lon = r["longitude"].as<float>();
  return true;
}

inline int readPlaces(JsonVariantConst doc, PlaceResult* out, int max) {
  int n = 0;
  for (JsonVariantConst r : doc["results"].as<JsonArrayConst>()) {
    if (n >= max) break;
    const char* name = r["name"] | "";
    if (!name[0]) continue;
    const char* cc = r["country_code"] | "";
    const char* admin = r["admin1"] | "";
    const char* country = r["country"] | "";
    std::snprintf(out[n].name, sizeof(out[n].name), cc[0] ? "%s, %s" : "%s", name, cc);
    std::snprintf(out[n].detail, sizeof(out[n].detail), "%s%s%s%s%s", name, admin[0] ? ", " : "", admin,
                  country[0] ? ", " : "", country);
    n++;
  }
  return n;
}

// The daily forecast (weather_code, temperature_2m_max / _min); at most max days.
inline int readForecast(JsonVariantConst doc, WeatherDay* out, int max) {
  JsonArrayConst codes = doc["daily"]["weather_code"], tmax = doc["daily"]["temperature_2m_max"],
                 tmin = doc["daily"]["temperature_2m_min"];
  int n = 0;
  for (size_t i = 0; i < codes.size() && n < max; i++) {
    WeatherDay& w = out[n++];
    w.weather_code = codes[i] | 0;
    w.temp_max = tmax[i] | 0.0f;
    w.temp_min = tmin[i] | 0.0f;
    w.icon = weatherIcon(w.weather_code);
  }
  return n;
}

// ---- Stocks (Yahoo Finance chart API) ----

inline bool readQuote(JsonVariantConst doc, StockItem& s) {
  JsonVariantConst m = doc["chart"]["result"][0]["meta"];
  if (m.isNull() || m["regularMarketPrice"].isNull()) return false;
  s.price = m["regularMarketPrice"] | 0.0f;
  s.prevClose = m["chartPreviousClose"] | (m["previousClose"] | 0.0f);
  s.dayHigh = m["regularMarketDayHigh"] | 0.0f;
  s.dayLow = m["regularMarketDayLow"] | 0.0f;
  s.fiftyTwoWeekHigh = m["fiftyTwoWeekHigh"] | 0.0f;
  s.fiftyTwoWeekLow = m["fiftyTwoWeekLow"] | 0.0f;
  return true;
}

// ---- Parcels (PKGE) ----

constexpr time_t KEEP_DELIVERED_S = 86400;  // a delivered parcel stays a day

inline bool parcelExpired(const TrackingItem& p, time_t now) { return p.status == 5 && now - p.lastDate > KEEP_DELIVERED_S; }

// Parcels on their way first, then the newest checkpoint first.
inline bool parcelBefore(const TrackingItem& a, const TrackingItem& b) {
  bool aDone = a.status == 5, bDone = b.status == 5;
  if (aDone != bDone) return !aDone;
  return a.lastDate > b.lastDate;
}

// The account's parcels, sorted, without the expired ones, at most max. -1 if the answer has no list.
inline int readParcels(JsonVariantConst doc, time_t now, TrackingItem* out, int max, int* seen) {
  JsonArrayConst list = doc["payload"].as<JsonArrayConst>();
  if (list.isNull()) return -1;
  int n = 0, all = 0;
  for (JsonVariantConst p : list) {
    all++;
    TrackingItem it = {};
    std::snprintf(it.trackNumber, sizeof(it.trackNumber), "%s", p["track_number"] | "");
    it.status = p["status"] | 0;
    it.lastDate = parseIsoUtc(p["last_status_date"] | "");
    it.estDelivery = parseIsoUtc(p["est_delivery_date_to"] | (p["est_delivery_date_from"] | ""));
    if (parcelExpired(it, now)) continue;
    int pos = n;
    while (pos > 0 && parcelBefore(it, out[pos - 1])) pos--;
    if (pos >= max) continue;
    for (int i = (n < max ? n : max - 1); i > pos; i--) out[i] = out[i - 1];
    out[pos] = it;
    if (n < max) n++;
  }
  if (seen) *seen = all;
  return n;
}

// ---- Flight status (AeroDataBox) ----

// The gate time airlines show: the revised time, else the runway time; 0 if neither is known.
inline time_t actualTime(JsonVariantConst movement) {
  time_t t = parseIsoUtc(movement["revisedTime"]["utc"] | "");
  return t ? t : parseIsoUtc(movement["runwayTime"]["utc"] | "");
}

// The leg of the flight number nearest to the calendar's departure (a number can fly twice a day).
inline bool readFlight(JsonVariantConst doc, time_t calStart, LiveFlight& f) {
  JsonVariantConst best;
  time_t bestDiff = 0;
  for (JsonVariantConst leg : doc.as<JsonArrayConst>()) {
    time_t dep = parseIsoUtc(leg["departure"]["scheduledTime"]["utc"] | "");
    time_t diff = dep > calStart ? dep - calStart : calStart - dep;
    if (best.isNull() || diff < bestDiff) {
      best = leg;
      bestDiff = diff;
    }
  }
  if (best.isNull()) return false;
  JsonVariantConst dep = best["departure"], arr = best["arrival"];
  std::snprintf(f.status, sizeof(f.status), "%s", best["status"] | "Unknown");
  f.depScheduled = parseIsoUtc(dep["scheduledTime"]["utc"] | "");
  f.depActual = actualTime(dep);
  f.arrScheduled = parseIsoUtc(arr["scheduledTime"]["utc"] | "");
  f.arrActual = actualTime(arr);
  std::snprintf(f.depGate, sizeof(f.depGate), "%s", dep["gate"] | "");
  std::snprintf(f.depTerminal, sizeof(f.depTerminal), "%s", dep["terminal"] | "");
  f.valid = true;
  return true;
}

// ---- MakerWorld (Bambu design service): the counters of all designs added up ----

inline bool readMakerWorld(JsonVariantConst doc, MakerWorld& m) {
  JsonArrayConst hits = doc["hits"].as<JsonArrayConst>();
  if (hits.isNull()) return false;
  m = MakerWorld();
  for (JsonVariantConst h : hits) {
    m.likes += h["likeCount"] | 0;
    m.downloads += h["downloadCount"] | 0;
    m.prints += h["printCount"] | 0;
    m.collections += h["collectionCount"] | 0;
    m.boosts += h["boostCnt"] | 0;
  }
  m.available = true;
  return true;
}

}  // namespace svc
