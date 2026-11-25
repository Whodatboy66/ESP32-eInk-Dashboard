// The calendar links (stage 7b, own code): download over HTTPS, feed the bytes through
// IcsParser.h, and decide for every occurrence where it goes: the agenda ("Heute und morgen"), the
// month view's marks, the pinned flight, the birthday lists, the school holidays.
#include "Fetchers.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <time.h>
#include <memory>
#include <new>

#include "HttpBody.h"
#include "IcsParser.h"
#include "IcsRules.h"
#include "SchoolHolidays.h"
#include "SecureClient.h"
#include "../core/Problems.h"
#include "../personal/UserStore.h"

static_assert(ics::TITLE_MAX == CFG_CAL_TITLE_MAX, "calendar titles: the parser's and the widgets' size");

namespace {

constexpr time_t FLIGHT_LOOKAHEAD_S = 7 * 86400;  // flights pinned up to a week ahead
// No byte for this long: the download ends. Google builds a large calendar while it answers: packed,
// the headers and the first bytes come at once and then nothing until it is built (a 1,098-entry
// work calendar: over 10 s, build 83 gave up there; device test 2026-11-25).
constexpr unsigned long IDLE_TIMEOUT_MS = 30000;
constexpr uint16_t ANSWER_TIMEOUT_MS = 30000;  // for the answer's headers (unpacked: the same wait is here)

// One reading state for all links (about 3 KB, static RAM once: the readers below are templates,
// and a static inside them would exist once per use).
ics::Unfolder unfolder;
ics::Parser parser;
char lineBuf[ics::MAX_LINE];

time_t localDay(const struct tm& t, int addDays) { return ics::localTime(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday + addDays, 0, 0, 0); }

// A link's host only: a private iCal link works like a password.
void logHttpError(const char* url, int code) {
  const char* host = strstr(url, "://");
  host = host ? host + 3 : url;
  DBGF("[ICS] HTTP %d for %.*s", code, (int)strcspn(host, "/"), host);
}

// What the last readLines() received, for the log: bytes over the air and unpacked.
struct Download {
  uint32_t wireBytes = 0, bodyBytes = 0;
  uint32_t longestPauseMs = 0;  // between two pieces of data (Google building the file)
  bool packed = false;
};
Download lastDownload;

// A packed answer is asked for only when the unpacking memory (HttpBody.h, ~43 KB) leaves TLS its
// room; otherwise the link is read unpacked, as before build 83.
bool unpackMemory() {
  return ESP.getMaxAllocHeap() > TINFL_LZ_DICT_SIZE + 1024 &&
         ESP.getFreeHeap() > httpbody::Gunzip::HEAP_BYTES + 60 * 1024;
}

// Downloads a link and calls onLine for each logical line (onLine returns false to stop). False
// without an answer 200, or when the answer ended early (its length, last chunk or gzip end not
// reached: the previous data stays). HTTP/1.1, because only then does HTTPClient ask for a packed
// answer; chunks and gzip are undone in HttpBody.h.
using LineFn = bool (*)(void* ctx, const char* line);

bool readLinesOnce(const char* url, LineFn lineFn, void* ctx, int* httpCode) {
  auto onLine = [&](const char* line) { return lineFn(ctx, line); };
  lastDownload = Download();
  const unsigned long startMs = millis();
  SecureClient client;  // certificate checked with the CA bundle
  HTTPClient http;
  http.setReuse(false);  // "Connection: close": the server ends the connection after the answer
  http.setTimeout(ANSWER_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  std::unique_ptr<httpbody::Gunzip> gz;
  if (unpackMemory()) {
    gz.reset(new (std::nothrow) httpbody::Gunzip());
    if (gz && !gz->begin()) gz.reset();
  }
  http.setAcceptEncoding(gz ? "gzip" : "identity");
  static const char* HEADERS[] = {"Content-Encoding", "Transfer-Encoding"};
  http.collectHeaders(HEADERS, 2);
  if (!http.begin(client, url)) {
    if (httpCode) *httpCode = 0;
    return false;
  }
  int code = http.GET();
  if (httpCode) *httpCode = code;
  if (code != 200) {
    logHttpError(url, code);
    http.end();
    return false;
  }
  const bool chunked = http.header("Transfer-Encoding").equalsIgnoreCase("chunked");
  const String encoding = http.header("Content-Encoding");
  const bool packed = encoding.equalsIgnoreCase("gzip");
  if ((packed && !gz) || (!packed && encoding.length() && !encoding.equalsIgnoreCase("identity"))) {
    DBGF("[ICS] answer packed as \"%s\": not readable", encoding.c_str());
    http.end();
    return false;
  }
  if (!packed) gz.reset();  // the memory back right away
  lastDownload.packed = packed;
  const int size = http.getSize();  // -1: unknown (chunks, or until the connection ends)
  unfolder.reset();
  WiFiClient* stream = http.getStreamPtr();
  if (!stream) {  // the connection ended right after the headers
    http.end();
    return false;
  }
  httpbody::Dechunker dechunk;
  bool more = true, broken = false;
  const char* why = nullptr;  // why the answer is incomplete, for the log
  auto lines = [&](const uint8_t* p, size_t n) {
    lastDownload.bodyBytes += n;
    for (size_t i = 0; i < n && more; i++) {
      unfolder.push((char)p[i]);
      if (unfolder.next(lineBuf, sizeof(lineBuf))) more = onLine(lineBuf);
    }
  };
  auto body = [&](const uint8_t* p, size_t n) {
    if (!gz) lines(p, n);
    else if (!gz->push(p, n, lines)) {
      broken = true;
      why = "packed data damaged";
    }
  };
  uint8_t buf[1024];
  unsigned long lastData = millis();
  while (more && !broken) {
    if (chunked ? dechunk.done() : (size >= 0 && lastDownload.wireBytes >= (uint32_t)size)) break;
    int avail = stream->available();
    if (avail > 0) {
      int n = stream->read(buf, avail < (int)sizeof(buf) ? avail : (int)sizeof(buf));
      if (n < 0) {
        why = "TLS read error";
        break;
      }
      if (millis() - lastData > lastDownload.longestPauseMs) lastDownload.longestPauseMs = millis() - lastData;
      lastDownload.wireBytes += n;
      if (chunked && !dechunk.push(buf, n, body)) {
        broken = true;
        if (!why) why = "chunk error";
      }
      else body(buf, n);
      lastData = millis();
    } else if (!stream->connected()) {
      why = "connection ended";
      break;
    } else if (millis() - lastData > IDLE_TIMEOUT_MS) {
      why = "no data for 30 s";
      break;
    } else {
      delay(2);
    }
  }
  if (more) {  // read to the end (not stopped by onLine): was it all there?
    const bool whole = !broken && (chunked ? dechunk.done() : size < 0 || lastDownload.wireBytes >= (uint32_t)size) &&
                       (!gz || gz->done());
    if (!whole) {
      DBGF("[ICS] answer incomplete or damaged after %lu bytes, %lu ms (%s%s)", (unsigned long)lastDownload.wireBytes,
           millis() - startMs, why ? why : "ended early", lastDownload.packed ? ", packed" : "");
      http.end();
      return false;
    }
    unfolder.finish();
    while (more && unfolder.next(lineBuf, sizeof(lineBuf))) more = onLine(lineBuf);  // the end can finish two
  }
  http.end();
  return true;
}

// One copy of the download code for all its users (a template would be compiled once per caller).
template <typename OnLine>
bool readLines(const char* url, OnLine onLine, int* httpCode = nullptr) {
  return readLinesOnce(url, [](void* ctx, const char* line) { return (*static_cast<OnLine*>(ctx))(line); }, &onLine,
                       httpCode);
}

// Downloads a link and calls onEvent for each event. False without an answer 200.
template <typename OnEvent>
bool readEvents(const char* url, OnEvent onEvent) {
  parser.reset();
  int events = 0;
  unsigned long t0 = millis();
  bool ok = readLines(url, [&](const char* line) {
    if (parser.feed(line)) {
      events++;
      onEvent(parser.event());
    }
    return true;
  });
  if (ok) {
    if (lastDownload.packed)
      DBGF("[ICS] %d events parsed in %lu ms (%lu KB, received packed: %lu KB, longest pause %lu ms)", events,
           millis() - t0, (unsigned long)(lastDownload.bodyBytes / 1024), (unsigned long)(lastDownload.wireBytes / 1024),
           (unsigned long)lastDownload.longestPauseMs);
    else
      DBGF("[ICS] %d events parsed in %lu ms (%lu KB, longest pause %lu ms)", events, millis() - t0,
           (unsigned long)(lastDownload.bodyBytes / 1024), (unsigned long)lastDownload.longestPauseMs);
  }
  return ok;
}

// An occurrence into a birthday list (name and day; the year from "Name (1966)").
void collectBirthday(personal::BirthdayList& l, const ics::Event& ev, time_t s, time_t from) {
  if (s < from) return;  // began before today (a multi-day entry): not a birthday to come
  struct tm lt;
  localtime_r(&s, &lt);
  char raw[ics::TITLE_MAX], name[personal::BDAY_NAME_BYTES];
  uint16_t born = 0;
  ics::birthdayName(ev.summary, raw, sizeof(raw), born);
  personal::copyChars(name, sizeof(name), raw, personal::BDAY_NAME_CHARS);
  personal::filterText(name, screenFontHas, false);
  personal::trimSpaces(name);
  if (!name[0]) return;
  for (int i = 0; i < l.count; i++)
    if (l.items[i].day == lt.tm_mday && l.items[i].month == lt.tm_mon + 1 && !strcmp(l.items[i].name, name)) return;
  if (l.count >= personal::MAX_BIRTHDAYS) return;
  personal::Birthday& b = l.items[l.count++];
  snprintf(b.name, sizeof(b.name), "%s", name);
  b.day = lt.tm_mday;
  b.month = lt.tm_mon + 1;
  b.year = born;
}

// "Heute und morgen": sorted by start, at most as many as fit.
void addUpcoming(const ics::Event& ev, time_t s, time_t e) {
  const int cap = sizeof(cal.next24hEvents) / sizeof(cal.next24hEvents[0]);
  int pos = cal.eventCount;
  while (pos > 0 && cal.next24hEvents[pos - 1].start > s) pos--;
  if (pos >= cap) return;
  for (int i = (cal.eventCount < cap ? cal.eventCount : cap - 1); i > pos; i--) cal.next24hEvents[i] = cal.next24hEvents[i - 1];
  CalEventItem& item = cal.next24hEvents[pos];
  snprintf(item.title, sizeof(item.title), "%s", ev.summary);
  item.start = s;
  item.end = e;
  item.allDay = ev.allDay;
  if (cal.eventCount < cap) cal.eventCount++;
}

// The month view: the days of this month an occurrence covers, from today on.
struct MonthMarks {
  time_t from, monthEnd;
  int month;  // tm_mon
  bool days[32];
};

void markDays(MonthMarks& m, time_t s, time_t e) {
  time_t cur = s > m.from ? s : m.from;
  for (int guard = 0; guard < 32 && cur < m.monthEnd && cur < e; guard++) {
    struct tm lt;
    localtime_r(&cur, &lt);
    if (lt.tm_mon == m.month) m.days[lt.tm_mday] = true;
    cur = ics::localTime(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday + 1, 0, 0, 0);
  }
}

// The earliest flight not over yet within a week: still to depart (pinned with a countdown) or
// departed but before its calendar end, so a delayed flight keeps its live status.
void notePossibleFlight(const ics::Event& ev, time_t s, time_t e, time_t now) {
  if (e <= now || s >= now + FLIGHT_LOOKAHEAD_S || !isFlightTitle(ev.summary)) return;
  if (cal.nextFlight.start && s >= cal.nextFlight.start) return;
  snprintf(cal.nextFlight.title, sizeof(cal.nextFlight.title), "%s", ev.summary);
  cal.nextFlight.start = s;
  cal.nextFlight.end = e;
  cal.nextFlight.allDay = ev.allDay;
}

bool setLocalZone(const char* tz, struct tm& now) {
  setenv("TZ", (tz && tz[0]) ? tz : "UTC0", 1);
  tzset();
  return getLocalTime(&now, 10000);
}

}  // namespace

bool fetchCalendarIcs(const char* tz, int* failed) {
  // Keep the previous data if every link fails (cal lives in RTC memory and stays on screen).
  static CalEvents previous;
  previous = cal;
  cal.eventCount = 0;
  cal.daysCount = 0;
  cal.nextFlight = {};
  if (!hasIcsFeeds()) return false;
  struct tm nowTm;
  if (!setLocalZone(tz, nowTm)) {
    DBG("[ICS] Time not synced");
    cal = previous;
    return false;
  }
  const time_t now = time(nullptr);
  const time_t from = localDay(nowTm, 0);
  const time_t upcomingTo = localDay(nowTm, 2);  // the agenda: today and tomorrow
  MonthMarks marks = {};
  marks.from = from;
  marks.monthEnd = ics::localTime(nowTm.tm_year + 1900, nowTm.tm_mon + 2, 1, 0, 0, 0);
  marks.month = nowTm.tm_mon;
  const time_t to = now + FLIGHT_LOOKAHEAD_S > marks.monthEnd ? now + FLIGHT_LOOKAHEAD_S : marks.monthEnd;
  // Birthdays in the links (stage 7): collected on the heap, saved only if every link worked.
  personal::BirthdayList* bdays = cal_birthdays ? new (std::nothrow) personal::BirthdayList() : nullptr;
  const time_t bdayTo = localDay(nowTm, personal::BDAY_HORIZON_DAYS + 1);

  bool anyOk = false;
  uint8_t failedMask = 0;
  int failures = 0;
  for (int i = 0; i < ICS_MAX_FEEDS; i++) {
    if (!ics_urls[i][0]) continue;
    const bool markMonth = (ics_month_mask & (1 << i)) != 0;
    bool ok = readEvents(ics_urls[i], [&](const ics::Event& ev) {
      const bool birthday = ics::countsAsBirthday(ev.freq, ev.allDay, cal_birthdays);
      if (birthday && bdays)
        ics::expand(ev, from, bdayTo, [&](time_t s) {
          collectBirthday(*bdays, ev, s, from);
          return true;
        });
      const time_t dur = ev.end - ev.start;
      ics::expand(ev, from, to, [&](time_t s) {
        const time_t e = s + dur;
        if (e > now && s < upcomingTo) addUpcoming(ev, s, e);  // birthdays too: they stay in the agenda
        if (markMonth && !birthday && s < marks.monthEnd) markDays(marks, s, e > s ? e : s + 1);
        notePossibleFlight(ev, s, e, now);
        return true;
      });
    });
    if (ok) anyOk = true;
    else failures++;
    if (ok) failedMask &= ~(1u << i);
    else failedMask |= 1u << i;
  }
  if (failed) *failed = failures;
  icsFailedMask = failedMask;  // the status bar names the link ("Kalender 2 …")
  if (anyOk && !failures) {
    if (bdays) saveUserCalBirthdays(*bdays);
    else if (!cal_birthdays) clearUserCalBirthdays();  // the setting was turned off
    // (no memory for the list: the stored one stays)
  }
  delete bdays;
  if (!anyOk) {
    cal = previous;
    return false;
  }
  for (int d = 1; d <= 31 && cal.daysCount < 31; d++)
    if (marks.days[d]) cal.daysWithEvents[cal.daysCount++] = d;
  if (cal.nextFlight.start) DBGF("[ICS] Next flight: %s", cal.nextFlight.title);
  DBGF("[ICS] %d upcoming, %d days marked", cal.eventCount, cal.daysCount);
  return true;
}

// Birthdays from the birthday calendar link (stage 4): every occurrence in the next 60 days.
// Kept in /bday_feed.json; on failure the last list stays.
bool fetchBirthdayFeed(const char* tz) {
  if (!bday_url[0]) return false;
  struct tm nowTm;
  if (!setLocalZone(tz, nowTm)) return false;
  personal::BirthdayList* found = new (std::nothrow) personal::BirthdayList();  // heap: little static RAM
  if (!found) return false;
  const time_t from = localDay(nowTm, 0), to = localDay(nowTm, personal::BDAY_HORIZON_DAYS + 1);
  bool ok = readEvents(bday_url, [&](const ics::Event& ev) {
    ics::expand(ev, from, to, [&](time_t s) {
      collectBirthday(*found, ev, s, from);
      return true;
    });
  });
  if (ok) {
    DBGF("[ICS] %d birthdays in the next %d days", found->count, personal::BDAY_HORIZON_DAYS);
    ok = saveUserFeedBirthdays(*found);
  }
  delete found;
  return ok;
}

// School holidays (stage 7): the all-day entries of the link from a month ago to a year ahead.
bool fetchSchoolHolidays(const char* url, const char* tz, school::SchoolList& out) {
  struct tm nowTm;
  if (!setLocalZone(tz, nowTm)) return false;
  out = school::SchoolList();
  const long today = personal::dayNumber(personal::Date{nowTm.tm_year + 1900, nowTm.tm_mon + 1, nowTm.tm_mday});
  const time_t from = localDay(nowTm, -31), to = localDay(nowTm, 400);
  bool ok = readEvents(url, [&](const ics::Event& ev) {
    if (!ics::countsAsSchoolHoliday(ev.allDay)) return;  // a school's own calendar: timed entries aren't days off
    const time_t dur = ev.end - ev.start;
    ics::expand(ev, from, to, [&](time_t s) {
      const time_t last = s + dur > s ? s + dur - 1 : s;  // DTEND is the day after the last
      struct tm a, b;
      localtime_r(&s, &a);
      localtime_r(&last, &b);
      char name[40];
      personal::copyChars(name, sizeof(name), ev.summary, 19);
      personal::filterText(name, screenFontHas, false);
      school::addRange(out, today, personal::dayNumber(personal::Date{a.tm_year + 1900, a.tm_mon + 1, a.tm_mday}),
                       personal::dayNumber(personal::Date{b.tm_year + 1900, b.tm_mon + 1, b.tm_mday}), name);
      return true;
    });
  });
  DBGF("[ICS] school holidays: %s, %d ranges", ok ? "read" : "failed", out.count);
  return ok;
}

// Setup app "Check" (stage 5): reads a link and counts its events, without changing the calendar
// data on screen. The number of events, or -1 with a reason in err (in the setup app's language).
int checkIcsLink(const char* url, char* err, size_t cap) {
  using i18n::S;
  bool calendar = false;
  int events = 0, code = 0;
  unsigned long t0 = millis();
  bool ok = readLines(url, [&](const char* line) {
    if (!strcmp(line, "BEGIN:VCALENDAR")) calendar = true;
    else if (!strcmp(line, "BEGIN:VEVENT")) events++;
    return millis() - t0 < 20000;  // a huge calendar: the count so far is enough
  }, &code);
  if (!ok) {
    if (code == 0) snprintf(err, cap, "%s", tr(S::CalBadAddress));
    else if (code == 200) snprintf(err, cap, "%s", tr(S::CalNoConnection));  // the answer ended early
    else if (code == 401 || code == 403) snprintf(err, cap, tr(S::CalRefused), code);
    else if (code == 404) snprintf(err, cap, "%s", tr(S::CalNotFound));
    else if (code < 0) snprintf(err, cap, "%s", tr(S::CalNoConnection));
    else snprintf(err, cap, tr(S::CalHttp), code);
    return -1;
  }
  if (!calendar) {
    snprintf(err, cap, "%s", tr(S::CalNotIcal));
    return -1;
  }
  return events;
}
