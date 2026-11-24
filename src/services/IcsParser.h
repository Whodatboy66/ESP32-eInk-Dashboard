#pragma once
// iCalendar (RFC 5545) for the calendar links (stage 7b, own code): line unfolding, property
// splitting, values, one event at a time, and the expansion of repeating events into occurrences.
// Times are local in the time zone set in the environment (TZ: the dashboard's zone); TZID
// parameters are read as that zone too. Plain C++, tested on the PC (tests/host/test_ics_parser.cpp).
// src/services/Calendar.cpp downloads the links and feeds the bytes in.
//
// Supported: DTSTART / DTEND / DURATION, SUMMARY, STATUS:CANCELLED, EXDATE, and RRULE with FREQ
// (daily, weekly, monthly, yearly), INTERVAL, UNTIL, COUNT, BYDAY (weekly: days; monthly: "2TU",
// "-1FR"). Not supported: edited single occurrences (RECURRENCE-ID: they can show twice), BYMONTHDAY,
// BYSETPOS, time zone definitions.
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "../core/CivilDate.h"

namespace ics {

constexpr size_t MAX_LINE = 640;   // longer lines (descriptions) are cut; the next line is unaffected
constexpr size_t TITLE_MAX = 128;  // = CFG_CAL_TITLE_MAX
constexpr int MAX_EXDATES = 16;
constexpr int MAX_BYDAY = 7;
constexpr int MAX_STEPS = 1500;    // safety limit per repeating event

struct Event {
  char summary[TITLE_MAX] = "";
  time_t start = 0, end = 0;
  bool hasEnd = false, allDay = false, cancelled = false;
  long duration = -1;      // DURATION in seconds, -1 = none
  char freq = 0;           // 0 = once, 'D', 'W', 'M', 'Y'
  int interval = 1;
  time_t until = 0;        // 0 = none
  int count = 0;           // 0 = none
  int byDayCount = 0;
  int8_t byDayWd[MAX_BYDAY] = {};  // 0 = Monday ... 6 = Sunday
  int8_t byDayN[MAX_BYDAY] = {};   // monthly: 2 = second, -1 = last; 0 = every
  time_t exdates[MAX_EXDATES] = {};
  int exdateCount = 0;
};

// ---- Values ----

inline time_t localTime(int y, int mo, int d, int h, int mi, int s) {
  std::tm t{};
  t.tm_year = y - 1900;
  t.tm_mon = mo - 1;
  t.tm_mday = d;  // may be out of range: mktime normalizes (used by the expansion)
  t.tm_hour = h;
  t.tm_min = mi;
  t.tm_sec = s;
  t.tm_isdst = -1;
  return std::mktime(&t);
}

using civil::daysFromCivil;

inline bool digits(const char* s, int n) {
  for (int i = 0; i < n; i++)
    if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
  return true;
}

inline int number(const char* s, int n) {
  int v = 0;
  for (int i = 0; i < n; i++) v = v * 10 + (s[i] - '0');
  return v;
}

// "20261010" (a date: all day, local midnight), "20261010T140000" (local), "...Z" (UTC). 0 if not
// a date. allDay may be nullptr.
inline time_t parseDateTime(const char* v, bool* allDay) {
  if (!v || !digits(v, 8)) return 0;
  int y = number(v, 4), mo = number(v + 4, 2), d = number(v + 6, 2);
  if (mo < 1 || mo > 12 || d < 1 || d > 31) return 0;
  if (v[8] != 'T') {
    if (allDay) *allDay = true;
    return localTime(y, mo, d, 0, 0, 0);
  }
  if (!digits(v + 9, 6)) return 0;
  if (allDay) *allDay = false;
  int h = number(v + 9, 2), mi = number(v + 11, 2), s = number(v + 13, 2);
  if (v[15] == 'Z') return static_cast<time_t>(daysFromCivil(y, mo, d)) * 86400 + h * 3600 + mi * 60 + s;
  return localTime(y, mo, d, h, mi, s);
}

// "P1D", "PT1H30M", "P2W", "P1DT2H" in seconds; -1 for a negative or broken one.
inline long parseDuration(const char* v) {
  if (!v || *v == '-') return -1;
  if (*v == '+') v++;
  if (*v++ != 'P') return -1;
  long total = 0;
  bool time = false, any = false;
  while (*v) {
    if (*v == 'T') {
      time = true;
      v++;
      continue;
    }
    if (!std::isdigit(static_cast<unsigned char>(*v))) return -1;
    long n = 0;
    while (std::isdigit(static_cast<unsigned char>(*v))) n = n * 10 + (*v++ - '0');
    char unit = *v++;
    if (unit == 'W' && !time) total += n * 7 * 86400;
    else if (unit == 'D' && !time) total += n * 86400;
    else if (unit == 'H' && time) total += n * 3600;
    else if (unit == 'M' && time) total += n * 60;
    else if (unit == 'S' && time) total += n;
    else return -1;
    any = true;
  }
  return any ? total : -1;
}

// TEXT escapes: "\," "\;" "\\" stay as the character, "\n" becomes a space (one line on the screen).
inline void unescapeText(char* s) {
  char* out = s;
  for (const char* in = s; *in; in++) {
    if (*in == '\\' && in[1]) {
      in++;
      *out++ = (*in == 'n' || *in == 'N') ? ' ' : *in;
    } else {
      *out++ = *in;
    }
  }
  *out = '\0';
}

// "NAME;PARAM=...:VALUE": the name (before the first ';' or ':') and the value (after the first ':'
// that isn't inside a quoted parameter, e.g. TZID="(UTC+01:00) Amsterdam"). Cuts the line.
inline bool splitProperty(char* line, const char** name, char** value) {
  bool quoted = false;
  char* nameEnd = nullptr;
  for (char* p = line; *p; p++) {
    if (*p == '"') quoted = !quoted;
    else if (!quoted && *p == ';' && !nameEnd) nameEnd = p;
    else if (!quoted && *p == ':') {
      if (!nameEnd) nameEnd = p;
      *nameEnd = '\0';
      *p = '\0';
      *name = line;
      *value = p + 1;
      return true;
    }
  }
  return false;
}

inline int weekdayOf(const char* s) {
  static const char* const names[7] = {"MO", "TU", "WE", "TH", "FR", "SA", "SU"};
  for (int i = 0; i < 7; i++)
    if (s[0] == names[i][0] && s[1] == names[i][1]) return i;
  return -1;
}

inline void parseRrule(const char* value, Event& ev) {
  char buf[256];
  std::snprintf(buf, sizeof(buf), "%s", value);
  char* save = nullptr;
  for (char* part = strtok_r(buf, ";", &save); part; part = strtok_r(nullptr, ";", &save)) {
    char* eq = std::strchr(part, '=');
    if (!eq) continue;
    *eq = '\0';
    const char* key = part;
    char* val = eq + 1;
    if (!std::strcmp(key, "FREQ")) {
      ev.freq = !std::strcmp(val, "DAILY") ? 'D' : !std::strcmp(val, "WEEKLY") ? 'W'
              : !std::strcmp(val, "MONTHLY") ? 'M' : !std::strcmp(val, "YEARLY") ? 'Y' : 0;
    } else if (!std::strcmp(key, "INTERVAL")) {
      int n = std::atoi(val);
      ev.interval = n > 0 ? n : 1;
    } else if (!std::strcmp(key, "UNTIL")) {
      ev.until = parseDateTime(val, nullptr);
    } else if (!std::strcmp(key, "COUNT")) {
      ev.count = std::atoi(val);
    } else if (!std::strcmp(key, "BYDAY")) {
      char* save2 = nullptr;
      for (char* day = strtok_r(val, ",", &save2); day && ev.byDayCount < MAX_BYDAY; day = strtok_r(nullptr, ",", &save2)) {
        int n = std::atoi(day);  // "2TU" -> 2, "-1FR" -> -1, "MO" -> 0
        const char* name = day;
        while (*name == '-' || *name == '+' || std::isdigit(static_cast<unsigned char>(*name))) name++;
        int wd = weekdayOf(name);
        if (wd < 0) continue;
        ev.byDayWd[ev.byDayCount] = static_cast<int8_t>(wd);
        ev.byDayN[ev.byDayCount] = static_cast<int8_t>(n);
        ev.byDayCount++;
      }
    }
  }
}

// ---- Lines ----

// Bytes in, logical lines out: CR dropped, a line starting with a blank or tab continues the one
// before (RFC 5545 folding; the blank itself is removed). Call next() after every push(); after
// finish() call it until it returns false (the end can complete two lines at once).
class Unfolder {
 public:
  void reset() {
    physLen = queued = 0;
    physAny = haveLogical = false;
  }
  void push(char c) {
    if (c == '\r') return;
    if (c == '\n') {
      endPhysical();
      return;
    }
    if (physLen + 1 < MAX_LINE) phys[physLen++] = c;
    physAny = true;
  }
  // End of the data: the last line counts even without a final newline.
  void finish() {
    if (physAny) endPhysical();
    if (haveLogical) {
      emit();
      haveLogical = false;
    }
  }
  bool next(char* out, size_t cap) {
    if (!queued) return false;
    std::snprintf(out, cap, "%s", done[0]);
    if (--queued) std::memcpy(done[0], done[1], sizeof(done[0]));
    return true;
  }

 private:
  char phys[MAX_LINE] = "";
  char logical[MAX_LINE] = "";
  char done[2][MAX_LINE] = {};  // finished lines not yet taken (two only at the end)
  size_t physLen = 0;
  int queued = 0;
  bool physAny = false, haveLogical = false;

  void emit() {
    if (queued < 2) std::snprintf(done[queued++], sizeof(done[0]), "%s", logical);
  }
  void endPhysical() {
    phys[physLen] = '\0';
    if (haveLogical && (phys[0] == ' ' || phys[0] == '\t')) {
      size_t n = std::strlen(logical);
      std::snprintf(logical + n, sizeof(logical) - n, "%s", phys + 1);
    } else {
      if (haveLogical) emit();
      std::snprintf(logical, sizeof(logical), "%s", phys);
      haveLogical = true;
    }
    physLen = 0;
    physAny = false;
  }
};

// ---- Events ----

// Logical lines in, events out: feed() returns true when an event ended; event() is it then.
// Components inside an event (VALARM) are skipped: their SUMMARY / DURATION aren't the event's.
class Parser {
 public:
  void reset() {
    inEvent = false;
    depth = 0;
  }
  bool feed(const char* line) {
    if (!inEvent) {
      if (!std::strcmp(line, "BEGIN:VEVENT")) {
        ev = Event();
        inEvent = true;
        depth = 0;
      }
      return false;
    }
    if (!std::strncmp(line, "BEGIN:", 6)) {
      depth++;
      return false;
    }
    if (!std::strncmp(line, "END:", 4)) {
      if (depth > 0) {
        depth--;
        return false;
      }
      if (std::strcmp(line, "END:VEVENT")) return false;
      inEvent = false;
      if (!ev.hasEnd) ev.end = ev.duration >= 0 ? ev.start + ev.duration : ev.allDay ? ev.start + 86400 : ev.start;
      return true;
    }
    if (depth > 0) return false;
    char buf[MAX_LINE];
    std::snprintf(buf, sizeof(buf), "%s", line);
    const char* name;
    char* value;
    if (!splitProperty(buf, &name, &value)) return false;
    if (!std::strcmp(name, "DTSTART")) {
      ev.start = parseDateTime(value, &ev.allDay);
    } else if (!std::strcmp(name, "DTEND")) {
      ev.end = parseDateTime(value, nullptr);
      ev.hasEnd = ev.end != 0;
    } else if (!std::strcmp(name, "DURATION")) {
      ev.duration = parseDuration(value);
    } else if (!std::strcmp(name, "SUMMARY")) {
      unescapeText(value);
      std::snprintf(ev.summary, sizeof(ev.summary), "%s", value);
    } else if (!std::strcmp(name, "RRULE")) {
      parseRrule(value, ev);
    } else if (!std::strcmp(name, "EXDATE")) {
      char* save = nullptr;
      for (char* v = strtok_r(value, ",", &save); v && ev.exdateCount < MAX_EXDATES; v = strtok_r(nullptr, ",", &save))
        ev.exdates[ev.exdateCount++] = parseDateTime(v, nullptr);
    } else if (!std::strcmp(name, "STATUS")) {
      ev.cancelled = !std::strcmp(value, "CANCELLED");
    }
    return false;
  }
  const Event& event() const { return ev; }

 private:
  Event ev;
  bool inEvent = false;
  int depth = 0;
};

// ---- Repeats ----

inline int daysIn(int y, int m) {
  static const int n[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
  return m == 2 && leap ? 29 : n[(m - 1) % 12];
}

// The n-th (n > 0) or n-th last (n < 0) weekday wd (0 = Monday) of a month at the time of `base`;
// 0 if that month has none (a 5th Monday).
inline time_t nthWeekday(int y, int mon, int wd, int n, const std::tm& base) {
  int firstWd = static_cast<int>(((daysFromCivil(y, mon, 1) % 7) + 7 + 3) % 7);  // 0 = Monday
  int dim = daysIn(y, mon), day;
  if (n > 0) {
    day = 1 + (wd - firstWd + 7) % 7 + (n - 1) * 7;
  } else {
    int lastWd = (firstWd + dim - 1) % 7;
    day = dim - (lastWd - wd + 7) % 7 + (n + 1) * 7;
  }
  if (day < 1 || day > dim) return 0;
  return localTime(y, mon, day, base.tm_hour, base.tm_min, base.tm_sec);
}

// Calls emit(start) for every occurrence that overlaps [from, to) (a moment: from <= start < to),
// in order; emit returns false to stop. Cancelled events and events without a start have none.
// COUNT counts excluded dates too (RFC 5545). Old series jump close to the window.
template <typename Emit>
void expand(const Event& ev, time_t from, time_t to, Emit emit) {
  if (ev.cancelled || ev.start == 0) return;
  const time_t dur = ev.end - ev.start;
  int emitted = 0;
  bool stop = false;
  auto accept = [&](time_t s) {  // false: no more occurrences
    if (s < ev.start) return true;
    if ((ev.until && s > ev.until) || (ev.count && emitted >= ev.count) || s >= to) return false;
    emitted++;
    for (int i = 0; i < ev.exdateCount; i++)
      if (ev.exdates[i] == s) return true;
    time_t e = s + dur;
    if (e <= from && !(dur == 0 && s >= from)) return true;  // over before the window
    if (!emit(s)) return false;
    return true;
  };
  if (!ev.freq) {
    accept(ev.start);
    return;
  }
  std::tm base;
  localtime_r(&ev.start, &base);
  const int by = base.tm_year + 1900, bm = base.tm_mon + 1, bd = base.tm_mday;
  const int interval = ev.interval > 0 ? ev.interval : 1;
  // Without COUNT, start close to the window instead of at the first occurrence.
  const long skip = ev.count ? 0 : static_cast<long>(from - dur - ev.start);
  auto firstStep = [&](long period) { return skip > 0 ? skip / (period * interval) - 1 : 0; };
  if (ev.freq == 'D') {
    for (long k = firstStep(86400L) < 0 ? 0 : firstStep(86400L), step = 0; step < MAX_STEPS && !stop; step++, k++)
      stop = !accept(localTime(by, bm, bd + static_cast<int>(k * interval), base.tm_hour, base.tm_min, base.tm_sec));
  } else if (ev.freq == 'W') {
    const int baseWd = (base.tm_wday + 6) % 7;
    int days[7], nDays = 0;
    for (int d = 0; d < 7; d++) {  // Monday .. Sunday
      bool use = ev.byDayCount == 0 && d == baseWd;
      for (int i = 0; i < ev.byDayCount; i++) use = use || ev.byDayWd[i] == d;
      if (use) days[nDays++] = d;
    }
    for (long k = firstStep(7L * 86400L) < 0 ? 0 : firstStep(7L * 86400L), step = 0; step < MAX_STEPS && !stop; step++, k++)
      for (int i = 0; i < nDays && !stop; i++)
        stop = !accept(localTime(by, bm, bd - baseWd + static_cast<int>(k * 7 * interval) + days[i], base.tm_hour,
                                 base.tm_min, base.tm_sec));
  } else if (ev.freq == 'M') {
    for (long k = firstStep(31L * 86400L) < 0 ? 0 : firstStep(31L * 86400L), step = 0; step < MAX_STEPS && !stop; step++, k++) {
      int months = bm - 1 + static_cast<int>(k * interval), y = by + months / 12, mon = months % 12 + 1;
      if (ev.byDayCount > 0 && ev.byDayN[0] != 0) {
        time_t t = nthWeekday(y, mon, ev.byDayWd[0], ev.byDayN[0], base);
        if (t) stop = !accept(t);
        else if (localTime(y, mon, 1, 0, 0, 0) >= to) stop = true;
      } else if (bd <= daysIn(y, mon)) {
        stop = !accept(localTime(y, mon, bd, base.tm_hour, base.tm_min, base.tm_sec));
      }
    }
  } else if (ev.freq == 'Y') {
    for (long k = firstStep(366L * 86400L) < 0 ? 0 : firstStep(366L * 86400L), step = 0; step < MAX_STEPS && !stop; step++, k++) {
      int y = by + static_cast<int>(k * interval);
      if (bd > daysIn(y, bm)) continue;  // 29 February in other years
      stop = !accept(localTime(y, bm, bd, base.tm_hour, base.tm_min, base.tm_sec));
    }
  }
}

}  // namespace ics
