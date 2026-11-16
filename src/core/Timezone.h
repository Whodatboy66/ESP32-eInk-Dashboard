#pragma once
// Setup page "Timezone": the C library needs a POSIX TZ rule (name + offset + DST rule, e.g.
// "CET-1CEST,M3.5.0,M10.5.0/3" for Germany). A bare name like "CET" has no offset and is silently
// treated as UTC, which puts the clock 1 h (summer 2 h) behind in Germany. So common names are
// turned into their rule, POSIX rules are kept, and anything else is rejected.
//
// Plain C++ without Arduino headers, tested on the PC (tests/host/test_timezone.cpp).

#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace tzdetail {

struct Alias {
  const char* name;
  const char* rule;
};

constexpr const char* CENTRAL_EU = "CET-1CEST,M3.5.0,M10.5.0/3";
constexpr const char* EASTERN_EU = "EET-2EEST,M3.5.0/3,M10.5.0/4";

// Case-insensitive. Mostly European zones, plus the usual US ones.
inline const Alias* aliases(size_t* count) {
  static const Alias table[] = {
    {"CET", CENTRAL_EU}, {"CEST", CENTRAL_EU}, {"MEZ", CENTRAL_EU}, {"MESZ", CENTRAL_EU},
    {"Germany", CENTRAL_EU}, {"Deutschland", CENTRAL_EU}, {"Berlin", CENTRAL_EU},
    {"Europe/Berlin", CENTRAL_EU}, {"Europe/Vienna", CENTRAL_EU}, {"Europe/Zurich", CENTRAL_EU},
    {"Europe/Paris", CENTRAL_EU}, {"Europe/Amsterdam", CENTRAL_EU}, {"Europe/Brussels", CENTRAL_EU},
    {"Europe/Luxembourg", CENTRAL_EU}, {"Europe/Rome", CENTRAL_EU}, {"Europe/Madrid", CENTRAL_EU},
    {"Europe/Copenhagen", CENTRAL_EU}, {"Europe/Oslo", CENTRAL_EU}, {"Europe/Stockholm", CENTRAL_EU},
    {"Europe/Warsaw", CENTRAL_EU}, {"Europe/Prague", CENTRAL_EU}, {"Europe/Budapest", CENTRAL_EU},
    {"EET", EASTERN_EU}, {"EEST", EASTERN_EU}, {"Europe/Helsinki", EASTERN_EU},
    {"Europe/Athens", EASTERN_EU}, {"Europe/Bucharest", EASTERN_EU},
    {"WET", "WET0WEST,M3.5.0/1,M10.5.0"}, {"Europe/Lisbon", "WET0WEST,M3.5.0/1,M10.5.0"},
    {"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0"}, {"Europe/Dublin", "IST-1GMT0,M10.5.0,M3.5.0/1"},
    {"UK", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"UTC", "UTC0"}, {"GMT", "GMT0"}, {"Z", "UTC0"},
    {"America/New_York", "EST5EDT,M3.2.0,M11.1.0"}, {"America/Chicago", "CST6CDT,M3.2.0,M11.1.0"},
    {"America/Denver", "MST7MDT,M3.2.0,M11.1.0"}, {"America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"},
  };
  *count = sizeof(table) / sizeof(table[0]);
  return table;
}

inline bool equalsIgnoreCase(const char* a, size_t n, const char* b) {
  if (std::strlen(b) != n) return false;
  for (size_t i = 0; i < n; i++) {
    if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
      return false;
  }
  return true;
}

// Looks like a POSIX rule: a name of 3+ letters (or <...>), then an offset starting with +, - or a
// digit. The rest (DST name, rules) is left to the C library.
inline bool looksLikePosix(const char* s, size_t n) {
  size_t i = 0;
  if (n && s[0] == '<') {
    const void* close = std::memchr(s, '>', n);
    if (!close) return false;
    i = static_cast<size_t>(static_cast<const char*>(close) - s) + 1;
    if (i < 3) return false;
  } else {
    while (i < n && std::isalpha(static_cast<unsigned char>(s[i]))) i++;
    if (i < 3) return false;
  }
  if (i < n && (s[i] == '+' || s[i] == '-')) i++;
  return i < n && std::isdigit(static_cast<unsigned char>(s[i]));
}

}  // namespace tzdetail

// Writes the POSIX rule for `in` to `out`; false if `in` can't be used. Empty input = "UTC0".
inline bool normalizeTimezone(const char* in, char* out, size_t cap) {
  if (!in || !out || cap == 0) return false;
  while (*in == ' ' || *in == '\t') in++;
  size_t n = std::strlen(in);
  while (n && (in[n - 1] == ' ' || in[n - 1] == '\t' || in[n - 1] == '\r' || in[n - 1] == '\n')) n--;

  const char* result = nullptr;
  if (n == 0) {
    result = "UTC0";
  } else {
    size_t count = 0;
    const tzdetail::Alias* table = tzdetail::aliases(&count);
    for (size_t i = 0; i < count && !result; i++) {
      if (tzdetail::equalsIgnoreCase(in, n, table[i].name)) result = table[i].rule;
    }
  }
  if (result) {
    int w = std::snprintf(out, cap, "%s", result);
    return w > 0 && static_cast<size_t>(w) < cap;
  }
  if (!tzdetail::looksLikePosix(in, n) || n >= cap) return false;
  std::memcpy(out, in, n);
  out[n] = '\0';
  return true;
}
