#pragma once
// Flights in calendar titles and their live status line (stage 7b, own code). Plain C++, tested on
// the PC (tests/host/test_flight_text.cpp); drawn by the agenda (Widgets.cpp), the number is used for
// the AeroDataBox request (src/services/Flight.cpp).
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "../i18n/Strings.h"
#include "../services/ServiceData.h"

namespace flighttext {

inline bool upper(char c) { return std::isupper(static_cast<unsigned char>(c)) != 0; }
inline bool digit(char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }
inline bool alnum(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0; }

// A three-letter word at p ("FRA" followed by no letter or digit).
inline bool airportAt(const char* p) { return upper(p[0]) && upper(p[1]) && upper(p[2]) && !alnum(p[3]); }

// A flight number at the start of a word: a 2-character airline code (letters and digits, not two
// digits), an optional blank, 1-4 digits ("TS399", "AC 871", "U2 1234"). out gets it without the
// blank (may be nullptr).
inline bool findNumber(const char* t, char* out, size_t cap) {
  for (const char* p = t; *p; p++) {
    if (p != t && alnum(p[-1])) continue;
    char a = p[0], b = p[1];
    if (!(upper(a) || digit(a)) || !(upper(b) || digit(b)) || (digit(a) && digit(b))) continue;
    const char* q = p + 2;
    if (*q == ' ') q++;
    int n = 0;
    while (digit(q[n])) n++;
    if (n >= 1 && n <= 4 && !alnum(q[n])) {
      if (out && cap) std::snprintf(out, cap, "%c%c%.*s", a, b, n, q);
      return true;
    }
  }
  return false;
}

// Two airport codes with a separator: "PUJ->YUL", "YUL - YYZ", "YUL→YYZ", "YUL to YYZ", "YUL/YYZ".
inline bool hasRoute(const char* t) {
  for (const char* p = t; *p; p++) {
    if ((p != t && alnum(p[-1])) || !airportAt(p)) continue;
    const char* q = p + 3;
    bool separated = false;
    for (;;) {
      if (*q == ' ' || *q == '-' || *q == '>' || *q == '/') {
        q++;
      } else if (static_cast<unsigned char>(q[0]) == 0xE2 && static_cast<unsigned char>(q[1]) == 0x86 &&
                 static_cast<unsigned char>(q[2]) == 0x92) {
        q += 3;  // →
      } else if (q[0] == 't' && q[1] == 'o' && q[2] == ' ') {
        q += 3;
      } else {
        break;
      }
      separated = true;
    }
    if (separated && airportAt(q)) return true;
  }
  return false;
}

// A flight: "flight" in the title (any case), a ✈, or a flight number together with a route.
inline bool isFlightTitle(const char* t) {
  if (!t || !*t) return false;
  if (strcasestr(t, "flight") || std::strstr(t, "\xE2\x9C\x88")) return true;
  return findNumber(t, nullptr, 0) && hasRoute(t);
}

inline bool extractFlightNumber(const char* title, char* out, size_t cap) { return title && findNumber(title, out, cap); }

// The line under a flight: "Verspätet +45m, Gate B7, T2", "Gelandet 20:52", "Annulliert", ... The
// real departure and arrival times are on the flight's own line.
inline void liveStatus(const LiveFlight& f, i18n::Lang lang, char* buf, size_t n) {
  using i18n::S;
  auto text = [lang](S key) { return i18n::text(key, lang); };
  char arr[8] = "";
  time_t at = f.arrActual ? f.arrActual : f.arrScheduled;
  std::tm lt;
  if (at && localtime_r(&at, &lt)) std::strftime(arr, sizeof(arr), "%H:%M", &lt);
  const char* st = f.status;
  if (!std::strcmp(st, "Canceled")) { std::snprintf(buf, n, "%s", text(S::FlightCanceled)); return; }
  if (!std::strcmp(st, "CanceledUncertain")) { std::snprintf(buf, n, "%s", text(S::FlightMaybeCanceled)); return; }
  if (!std::strcmp(st, "Diverted")) { std::snprintf(buf, n, "%s", text(S::FlightDiverted)); return; }
  if (!std::strcmp(st, "Arrived")) { std::snprintf(buf, n, text(S::FlightLanded), arr); return; }
  if (!std::strcmp(st, "Departed") || !std::strcmp(st, "EnRoute") || !std::strcmp(st, "Approaching")) {
    std::snprintf(buf, n, text(S::FlightInAir), arr);
    return;
  }
  const long delayMin = (f.depActual && f.depScheduled) ? static_cast<long>(f.depActual - f.depScheduled) / 60 : 0;
  S label = !std::strcmp(st, "Expected") ? S::FlightOnTime : !std::strcmp(st, "CheckIn") ? S::FlightCheckIn
          : !std::strcmp(st, "Boarding") ? S::FlightBoarding : !std::strcmp(st, "GateClosed") ? S::FlightGateClosed
          : S::FlightScheduled;
  if (!std::strcmp(st, "Delayed") || delayMin >= 5) label = S::FlightDelayed;
  size_t len = static_cast<size_t>(std::snprintf(buf, n, "%s", text(label)));
  auto append = [&](const char* fmt, const char* arg) {
    if (len < n) len += static_cast<size_t>(std::snprintf(buf + len, n - len, fmt, arg));
  };
  if (delayMin >= 5 && len < n) len += static_cast<size_t>(std::snprintf(buf + len, n - len, " +%ldm", delayMin));
  if (f.depGate[0]) append(text(S::FlightGate), f.depGate);
  if (f.depTerminal[0]) {
    bool numeric = true;
    for (const char* c = f.depTerminal; *c; c++) numeric = numeric && digit(*c);
    append(numeric ? ", T%s" : text(S::FlightTerminal), f.depTerminal);
  }
}

}  // namespace flighttext
