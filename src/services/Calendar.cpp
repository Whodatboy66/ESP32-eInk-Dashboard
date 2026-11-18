#include "Fetchers.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ctype.h>
#include <time.h>
#include <new>

#include "HttpJson.h"
#include "SecureClient.h"
#include "ServiceUtil.h"
#include "../widgets/MonthView.h"
#include "../personal/UserStore.h"

// ----------------------- Calendar (.ics) -----------------------

#define ICS_MAX_EXDATES  16
#define ICS_MAX_BYDAY     7
#define ICS_MAX_STEPS  1500   // safety limit per recurring event
#define ICS_FLIGHT_LOOKAHEAD_S  (7 * 86400)  // flights pinned up to a week ahead

struct IcsEvent {
  char summary[CFG_CAL_TITLE_MAX];
  time_t start;
  time_t end;
  bool hasEnd;
  bool allDay;
  bool cancelled;

  // RRULE subset: FREQ, INTERVAL, UNTIL, COUNT, BYDAY
  char freq;            // 0 = none, 'D', 'W', 'M', 'Y'
  int interval;
  time_t until;         // 0 = none
  int count;            // 0 = none
  int byDayCount;
  int8_t byDayWd[ICS_MAX_BYDAY];  // 0 = Monday ... 6 = Sunday
  int8_t byDayN[ICS_MAX_BYDAY];   // 0 = every, 2 = second, -1 = last (MONTHLY only)

  time_t exdates[ICS_MAX_EXDATES];
  int exdateCount;
};

struct IcsWindow {
  time_t now;
  time_t from;       // local midnight today
  time_t to;         // max(now + 24h, first day of next month)
  time_t monthEnd;
  int month;         // tm_mon of today
  bool days[32];
  personal::BirthdayList* collect;  // birthday feed: occurrences go here, not into cal
};

static time_t makeLocal(int y, int mon, int d, int hh, int mm, int ss) {
  struct tm t = {};
  t.tm_year = y - 1900;
  t.tm_mon = mon - 1;
  t.tm_mday = d;
  t.tm_hour = hh;
  t.tm_min = mm;
  t.tm_sec = ss;
  t.tm_isdst = -1;
  return mktime(&t);
}

// Parses "20261010", "20261010T140000" (local / TZID) or "20261010T140000Z" (UTC).
static time_t parseIcsValue(const char* v, bool* allDay) {
  int y = 0, mon = 0, d = 0, hh = 0, mm = 0, ss = 0;
  if (sscanf(v, "%4d%2d%2d", &y, &mon, &d) != 3) return 0;
  if (v[8] != 'T') {
    if (allDay) *allDay = true;
    return makeLocal(y, mon, d, 0, 0, 0);
  }
  if (allDay) *allDay = false;
  sscanf(v + 9, "%2d%2d%2d", &hh, &mm, &ss);
  if (v[15] == 'Z') {
    return (time_t)daysFromCivil(y, mon, d) * 86400 + hh * 3600 + mm * 60 + ss;
  }
  // TZID times are treated as the dashboard's own timezone.
  return makeLocal(y, mon, d, hh, mm, ss);
}

static int weekdayIndex(const char* s) {
  static const char* names[] = { "MO", "TU", "WE", "TH", "FR", "SA", "SU" };
  for (int i = 0; i < 7; i++)
    if (s[0] == names[i][0] && s[1] == names[i][1]) return i;
  return -1;
}

static void parseRrule(const char* value, IcsEvent& ev) {
  char buf[256];
  snprintf(buf, sizeof(buf), "%s", value);
  char* save = nullptr;
  for (char* part = strtok_r(buf, ";", &save); part; part = strtok_r(nullptr, ";", &save)) {
    if (!strncmp(part, "FREQ=", 5)) {
      const char* f = part + 5;
      if (!strcmp(f, "DAILY")) ev.freq = 'D';
      else if (!strcmp(f, "WEEKLY")) ev.freq = 'W';
      else if (!strcmp(f, "MONTHLY")) ev.freq = 'M';
      else if (!strcmp(f, "YEARLY")) ev.freq = 'Y';
    } else if (!strncmp(part, "INTERVAL=", 9)) {
      ev.interval = max(1, atoi(part + 9));
    } else if (!strncmp(part, "UNTIL=", 6)) {
      ev.until = parseIcsValue(part + 6, nullptr);
    } else if (!strncmp(part, "COUNT=", 6)) {
      ev.count = atoi(part + 6);
    } else if (!strncmp(part, "BYDAY=", 6)) {
      char* save2 = nullptr;
      for (char* day = strtok_r(part + 6, ",", &save2); day && ev.byDayCount < ICS_MAX_BYDAY; day = strtok_r(nullptr, ",", &save2)) {
        int n = atoi(day);  // "2TU" -> 2, "-1FR" -> -1, "MO" -> 0
        const char* name = day;
        while (*name == '-' || *name == '+' || isdigit((unsigned char)*name)) name++;
        int wd = weekdayIndex(name);
        if (wd < 0) continue;
        ev.byDayWd[ev.byDayCount] = (int8_t)wd;
        ev.byDayN[ev.byDayCount] = (int8_t)n;
        ev.byDayCount++;
      }
    }
  }
}

// Undo iCalendar TEXT escaping: "\," "\;" "\\" and "\n".
static void unescapeText(char* s) {
  char* out = s;
  for (char* in = s; *in; in++) {
    if (*in == '\\' && in[1]) {
      in++;
      *out++ = (*in == 'n' || *in == 'N') ? ' ' : *in;
    } else {
      *out++ = *in;
    }
  }
  *out = '\0';
}

static void addUpcoming(const IcsEvent& ev, time_t s, time_t e) {
  const int cap = sizeof(cal.next24hEvents) / sizeof(cal.next24hEvents[0]);
  int n = cal.eventCount;
  int pos = n;
  while (pos > 0 && cal.next24hEvents[pos - 1].start > s) pos--;
  if (pos >= cap) return;

  int last = (n < cap) ? n : cap - 1;
  for (int i = last; i > pos; i--) cal.next24hEvents[i] = cal.next24hEvents[i - 1];

  CalEventItem& item = cal.next24hEvents[pos];
  snprintf(item.title, sizeof(item.title), "%s", ev.summary);
  item.start = s;
  item.end = e;
  item.allDay = ev.allDay;
  if (n < cap) cal.eventCount++;
}

static void markDays(IcsWindow& w, time_t s, time_t e) {
  time_t cur = max(s, w.from);
  for (int guard = 0; guard < 32 && cur < w.monthEnd; guard++) {
    struct tm lt;
    localtime_r(&cur, &lt);
    if (lt.tm_mon == w.month) w.days[lt.tm_mday] = true;

    lt.tm_mday += 1;
    lt.tm_hour = lt.tm_min = lt.tm_sec = 0;
    lt.tm_isdst = -1;
    cur = mktime(&lt);
    if (cur >= e) break;
  }
}

// Returns false once occurrences are past the window (stop expanding).
static bool emitOccurrence(const IcsEvent& ev, IcsWindow& w, time_t s) {
  if (s >= w.to) return false;
  time_t dur = ev.end - ev.start;
  time_t e = s + dur;
  if (e <= w.from && !(dur == 0 && s >= w.from)) return true;

  for (int i = 0; i < ev.exdateCount; i++)
    if (ev.exdates[i] == s) return true;

  if (w.collect) {  // birthday feed: name and day of every occurrence in the window
    personal::BirthdayList& l = *w.collect;
    struct tm lt;
    localtime_r(&s, &lt);
    char name[personal::BDAY_NAME_BYTES];
    personal::copyChars(name, sizeof(name), ev.summary, personal::BDAY_NAME_CHARS);
    personal::filterText(name, screenFontHas, false);
    personal::trimSpaces(name);
    if (!name[0] || s < w.from) return true;
    for (int i = 0; i < l.count; i++)
      if (l.items[i].day == lt.tm_mday && l.items[i].month == lt.tm_mon + 1 && !strcmp(l.items[i].name, name))
        return true;
    if (l.count < personal::MAX_BIRTHDAYS) {
      personal::Birthday& b = l.items[l.count++];
      snprintf(b.name, sizeof(b.name), "%s", name);
      b.day = lt.tm_mday;
      b.month = lt.tm_mon + 1;
      b.year = 0;  // feeds rarely say the year of birth reliably
    }
    return true;
  }

  if (e > w.now && s < w.now + 86400) addUpcoming(ev, s, e);
  if (s < w.monthEnd) markDays(w, s, e > s ? e : s + 1);

  // Remember the earliest flight not yet over within the next week: still to depart (pinned with a
  // countdown) or departed but before its calendar end, so a delayed flight keeps its live status.
  if (e > w.now && s < w.now + ICS_FLIGHT_LOOKAHEAD_S &&
      (cal.nextFlight.start == 0 || s < cal.nextFlight.start) && isFlightTitle(ev.summary)) {
    snprintf(cal.nextFlight.title, sizeof(cal.nextFlight.title), "%s", ev.summary);
    cal.nextFlight.start = s;
    cal.nextFlight.end = e;
    cal.nextFlight.allDay = ev.allDay;
  }
  return true;
}

static time_t nthWeekdayOfMonth(int y, int mon, int wd, int n, const struct tm& base) {
  int dim = daysInMonth(y, mon);
  struct tm first = {};
  first.tm_year = y - 1900; first.tm_mon = mon - 1; first.tm_mday = 1; first.tm_hour = 12; first.tm_isdst = -1;
  mktime(&first);
  int firstWd = (first.tm_wday + 6) % 7;  // Monday = 0
  int day;
  if (n > 0) {
    day = 1 + (wd - firstWd + 7) % 7 + (n - 1) * 7;
  } else {
    int lastWd = (firstWd + dim - 1) % 7;
    day = dim - (lastWd - wd + 7) % 7 + (n + 1) * 7;
  }
  if (day < 1 || day > dim) return 0;
  return makeLocal(y, mon, day, base.tm_hour, base.tm_min, base.tm_sec);
}

static void expandEvent(const IcsEvent& ev, IcsWindow& w) {
  if (ev.cancelled || ev.start == 0) return;

  if (!ev.freq) {
    emitOccurrence(ev, w, ev.start);
    return;
  }

  struct tm base;
  localtime_r(&ev.start, &base);
  const int by = base.tm_year + 1900, bm = base.tm_mon + 1, bd = base.tm_mday;
  const int interval = ev.interval > 0 ? ev.interval : 1;
  const time_t dur = ev.end - ev.start;
  int emitted = 0;

  auto accept = [&](time_t t) -> bool {  // false = stop
    if (t < ev.start) return true;
    if (ev.until && t > ev.until) return false;
    if (ev.count && emitted >= ev.count) return false;
    emitted++;
    return emitOccurrence(ev, w, t);
  };

  // Without COUNT, jump close to the window instead of walking from the first occurrence.
  long skipSeconds = ev.count ? 0 : (long)(w.from - dur - ev.start);

  if (ev.freq == 'D') {
    long k = skipSeconds > 0 ? skipSeconds / (86400L * interval) - 1 : 0;
    for (int step = 0; step < ICS_MAX_STEPS; step++, k++) {
      if (k < 0) continue;
      if (!accept(makeLocal(by, bm, bd + (int)(k * interval), base.tm_hour, base.tm_min, base.tm_sec))) return;
    }
  } else if (ev.freq == 'W') {
    int baseWd = (base.tm_wday + 6) % 7;
    int days[ICS_MAX_BYDAY];
    int nDays = 0;
    for (int d = 0; d < 7; d++) {  // weekdays in order Monday..Sunday
      bool use = ev.byDayCount == 0 ? d == baseWd : false;
      for (int i = 0; i < ev.byDayCount; i++) if (ev.byDayWd[i] == d) use = true;
      if (use) days[nDays++] = d;
    }
    long k = skipSeconds > 0 ? skipSeconds / (7L * 86400L * interval) - 1 : 0;
    for (int step = 0; step < ICS_MAX_STEPS; step++, k++) {
      if (k < 0) continue;
      for (int i = 0; i < nDays; i++) {
        int mday = bd - baseWd + (int)(k * 7 * interval) + days[i];
        if (!accept(makeLocal(by, bm, mday, base.tm_hour, base.tm_min, base.tm_sec))) return;
      }
    }
  } else if (ev.freq == 'M') {
    long k = skipSeconds > 0 ? skipSeconds / (31L * 86400L * interval) - 1 : 0;
    for (int step = 0; step < ICS_MAX_STEPS; step++, k++) {
      if (k < 0) continue;
      int months = bm - 1 + (int)(k * interval);
      int y = by + months / 12, mon = months % 12 + 1;
      if (ev.byDayCount > 0 && ev.byDayN[0] != 0) {
        time_t t = nthWeekdayOfMonth(y, mon, ev.byDayWd[0], ev.byDayN[0], base);
        if (t && !accept(t)) return;
        if (!t && makeLocal(y, mon, 1, 0, 0, 0) >= w.to) return;
      } else if (bd <= daysInMonth(y, mon)) {
        if (!accept(makeLocal(y, mon, bd, base.tm_hour, base.tm_min, base.tm_sec))) return;
      }
    }
  } else if (ev.freq == 'Y') {
    long k = skipSeconds > 0 ? skipSeconds / (366L * 86400L * interval) - 1 : 0;
    for (int step = 0; step < ICS_MAX_STEPS; step++, k++) {
      if (k < 0) continue;
      int y = by + (int)(k * interval);
      if (bd > daysInMonth(y, bm)) continue;  // Feb 29 in non-leap years
      if (!accept(makeLocal(y, bm, bd, base.tm_hour, base.tm_min, base.tm_sec))) return;
    }
  }
}

// Buffered line reader over the HTTP stream, with RFC 5545 line unfolding.
class IcsLineReader {
public:
  explicit IcsLineReader(WiFiClient* s) : stream(s) {}

  // Returns false at end of stream.
  bool next(char* out, size_t cap) {
    if (!havePending && !readPhysical(pending, sizeof(pending))) return false;
    havePending = false;
    snprintf(out, cap, "%s", pending);

    // Continuation lines start with a space or tab.
    while (readPhysical(pending, sizeof(pending))) {
      if (pending[0] == ' ' || pending[0] == '\t') {
        size_t len = strlen(out);
        snprintf(out + len, cap - len, "%s", pending + 1);
      } else {
        havePending = true;
        break;
      }
    }
    return true;
  }

private:
  WiFiClient* stream;
  uint8_t buf[1024];
  size_t bufLen = 0, bufPos = 0;
  char pending[320];
  bool havePending = false;

  int readByte() {
    if (bufPos < bufLen) return buf[bufPos++];
    unsigned long start = millis();
    while (millis() - start < 10000) {
      int avail = stream->available();
      if (avail > 0) {
        bufLen = stream->read(buf, min((size_t)avail, sizeof(buf)));
        bufPos = 0;
        if (bufLen > 0) return buf[bufPos++];
      } else if (!stream->connected()) {
        return -1;
      }
      delay(2);
    }
    return -1;
  }

  bool readPhysical(char* out, size_t cap) {
    size_t n = 0;
    int c;
    bool any = false;
    while ((c = readByte()) >= 0) {
      any = true;
      if (c == '\n') break;
      if (c == '\r') continue;
      if (n + 1 < cap) out[n++] = (char)c;
    }
    out[n] = '\0';
    return any;
  }
};

// Downloads one feed and adds its occurrences to `cal` / `w.days`.
static bool readIcsFeed(const char* url, IcsWindow& w) {
  SecureClient client;  // certificate checked with the CA bundle
  HTTPClient http;
  http.useHTTP10(true);  // no chunked encoding, so the raw stream is the file
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(client, url)) return false;

  int code = http.GET();
  if (code != 200) {
    // A private iCal link works like a password: log only its host.
    const char* host = strstr(url, "://");
    host = host ? host + 3 : url;
    DBGF("[ICS] HTTP %d for %.*s", code, (int)strcspn(host, "/"), host);
    http.end();
    return false;
  }

  IcsLineReader reader(http.getStreamPtr());
  static char line[640];
  static IcsEvent ev;
  bool inEvent = false;
  int eventsSeen = 0;
  unsigned long t0 = millis();

  while (reader.next(line, sizeof(line))) {
    if (!inEvent) {
      if (!strcmp(line, "BEGIN:VEVENT")) {
        memset(&ev, 0, sizeof(ev));
        ev.interval = 1;
        inEvent = true;
      }
      continue;
    }

    if (!strcmp(line, "END:VEVENT")) {
      inEvent = false;
      eventsSeen++;
      if (!ev.hasEnd) ev.end = ev.allDay ? ev.start + 86400 : ev.start;
      expandEvent(ev, w);
      continue;
    }

    char* colon = strchr(line, ':');
    if (!colon) continue;
    *colon = '\0';
    const char* name = line;
    char* value = colon + 1;

    if (!strncmp(name, "DTSTART", 7)) {
      ev.start = parseIcsValue(value, &ev.allDay);
    } else if (!strncmp(name, "DTEND", 5)) {
      ev.end = parseIcsValue(value, nullptr);
      ev.hasEnd = true;
    } else if (!strncmp(name, "SUMMARY", 7)) {
      unescapeText(value);
      snprintf(ev.summary, sizeof(ev.summary), "%s", value);
    } else if (!strcmp(name, "RRULE")) {
      parseRrule(value, ev);
    } else if (!strncmp(name, "EXDATE", 6)) {
      char* save = nullptr;
      for (char* v = strtok_r(value, ",", &save); v && ev.exdateCount < ICS_MAX_EXDATES; v = strtok_r(nullptr, ",", &save))
        ev.exdates[ev.exdateCount++] = parseIcsValue(v, nullptr);
    } else if (!strcmp(name, "STATUS")) {
      ev.cancelled = !strcmp(value, "CANCELLED");
    }
  }
  http.end();

  DBGF("[ICS] %d events parsed in %lu ms", eventsSeen, millis() - t0);
  return true;
}

// Birthdays from the birthday calendar link (stage 4): every event in the next 60 days, yearly
// repeats expanded. Kept in /bday_feed.json; on failure the last list stays.
bool fetchBirthdayFeed(const char* tz) {
  if (!bday_url[0]) return false;
  setenv("TZ", (tz && tz[0]) ? tz : "UTC0", 1);
  tzset();
  struct tm nowTm;
  if (!getLocalTime(&nowTm, 10000)) return false;
  personal::BirthdayList* found = new (std::nothrow) personal::BirthdayList();  // heap: little static RAM
  if (!found) return false;
  IcsWindow w = {};
  w.now = time(nullptr);
  w.from = makeLocal(nowTm.tm_year + 1900, nowTm.tm_mon + 1, nowTm.tm_mday, 0, 0, 0);
  w.monthEnd = w.from;  // no marked days
  w.to = makeLocal(nowTm.tm_year + 1900, nowTm.tm_mon + 1, nowTm.tm_mday + personal::BDAY_HORIZON_DAYS + 1, 0, 0, 0);
  w.month = nowTm.tm_mon;
  w.collect = found;
  bool ok = readIcsFeed(bday_url, w);
  if (ok) {
    DBGF("[ICS] %d birthdays in the next %d days", found->count, personal::BDAY_HORIZON_DAYS);
    ok = saveUserFeedBirthdays(*found);
  }
  delete found;
  return ok;
}

bool fetchCalendarIcs(const char* tz, int* failed) {
  // Keep the previous data if every feed fails (cal lives in RTC memory and stays on screen).
  static CalEvents previous;
  previous = cal;
  cal.eventCount = 0;
  cal.daysCount = 0;
  cal.nextFlight = {};
  if (!hasIcsFeeds()) return false;

  setenv("TZ", (tz && tz[0]) ? tz : "UTC0", 1);
  tzset();

  struct tm nowTm;
  if (!getLocalTime(&nowTm, 10000)) {
    DBG("[ICS] Time not synced");
    return false;
  }

  IcsWindow w = {};
  w.now = time(nullptr);
  w.from = makeLocal(nowTm.tm_year + 1900, nowTm.tm_mon + 1, nowTm.tm_mday, 0, 0, 0);
  w.monthEnd = makeLocal(nowTm.tm_year + 1900, nowTm.tm_mon + 2, 1, 0, 0, 0);
  w.to = max(w.now + ICS_FLIGHT_LOOKAHEAD_S, w.monthEnd);
  w.month = nowTm.tm_mon;

  // All feeds go into the same window, so upcoming events and marked days are merged.
  bool anyOk = false;
  int failures = 0;
  for (int i = 0; i < ICS_MAX_FEEDS; i++) {
    if (ics_urls[i][0] == '\0') continue;
    if (readIcsFeed(ics_urls[i], w)) anyOk = true;
    else failures++;
  }
  if (failed) *failed = failures;

  if (!anyOk) {
    cal = previous;
    return false;
  }

  for (int d = 1; d <= 31 && cal.daysCount < 31; d++)
    if (w.days[d]) cal.daysWithEvents[cal.daysCount++] = d;
  if (cal.nextFlight.start)
    DBGF("[ICS] Next flight: %s", cal.nextFlight.title);

  DBGF("[ICS] %d upcoming, %d days marked", cal.eventCount, cal.daysCount);
  return anyOk;
}

