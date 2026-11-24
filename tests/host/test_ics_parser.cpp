// Host test for src/services/IcsParser.h (stage 7b): iCalendar lines, values, events and repeats,
// in the dashboard's time zone (Germany). Run: tests/host/run.sh
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "src/services/IcsParser.h"
#include "tests/host/check.h"

using ics::Event;

namespace {

time_t local(int y, int mo, int d, int h = 0, int mi = 0) {
  std::tm t{};
  t.tm_year = y - 1900;
  t.tm_mon = mo - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = mi;
  t.tm_isdst = -1;
  return std::mktime(&t);
}

// Feeds a whole calendar text through the unfolder and the parser; returns the finished events.
std::vector<Event> parseAll(const char* text) {
  std::vector<Event> out;
  ics::Unfolder u;
  ics::Parser p;
  char line[ics::MAX_LINE];
  auto take = [&]() {
    while (u.next(line, sizeof(line)))
      if (p.feed(line)) out.push_back(p.event());
  };
  for (const char* c = text; *c; c++) {
    u.push(*c);
    take();
  }
  u.finish();
  take();  // all lines that finish() completed
  return out;
}

std::vector<time_t> expand(const Event& ev, time_t from, time_t to) {
  std::vector<time_t> out;
  ics::expand(ev, from, to, [&](time_t s) {
    out.push_back(s);
    return true;
  });
  return out;
}

void test_unfold_and_split() {
  auto evs = parseAll(
      "BEGIN:VCALENDAR\r\nBEGIN:VEVENT\r\nSUMMARY:Lange Zeile\r\n  weiter\r\n\tund mehr\r\n"
      "DTSTART:20261010T100000\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n");
  CHECK_EQ((int)evs.size(), 1);
  CHECK_STR(evs[0].summary, "Lange Zeile weiterund mehr");  // one leading blank of each continuation removed
  char l1[] = "DTSTART;TZID=\"(UTC+01:00) Amsterdam, Berlin\":20261010T100000";
  const char* name;
  char* value;
  CHECK(ics::splitProperty(l1, &name, &value));
  CHECK_STR(name, "DTSTART");
  CHECK_STR(value, "20261010T100000");  // stage 7b: the colon inside the quotes isn't the separator
  char l2[] = "no colon here";
  CHECK(!ics::splitProperty(l2, &name, &value));
}

void test_values() {
  bool allDay = false;
  CHECK(ics::parseDateTime("20261010", &allDay) == local(2026, 10, 10) && allDay);
  CHECK(ics::parseDateTime("20261010T140500", &allDay) == local(2026, 10, 10, 14, 5) && !allDay);
  CHECK(ics::parseDateTime("20261010T120000Z", &allDay) == local(2026, 10, 10, 14, 0));  // CEST = UTC+2
  CHECK(ics::parseDateTime("garbage", &allDay) == 0);
  CHECK_EQ(ics::parseDuration("PT1H30M"), 5400L);
  CHECK_EQ(ics::parseDuration("P1D"), 86400L);
  CHECK_EQ(ics::parseDuration("P2W"), 14L * 86400L);
  CHECK_EQ(ics::parseDuration("P1DT2H"), 93600L);
  CHECK_EQ(ics::parseDuration("-PT15M"), -1L);  // a negative duration (alarms) isn't an event length
  CHECK_EQ(ics::parseDuration("X"), -1L);
  char t[] = "a\\, b\\; c\\\\d\\ne";
  ics::unescapeText(t);
  CHECK_STR(t, "a, b; c\\d e");
}

void test_event_fields() {
  auto evs = parseAll(
      "BEGIN:VEVENT\nSUMMARY:Arzt\\, Dr. M\nDTSTART;TZID=Europe/Berlin:20261110T090000\nDTEND;TZID=Europe/Berlin:20261110T093000\n"
      "END:VEVENT\n"
      "BEGIN:VEVENT\nSUMMARY:Urlaub\nDTSTART;VALUE=DATE:20261224\nEND:VEVENT\n"
      "BEGIN:VEVENT\nSUMMARY:Abgesagt\nDTSTART:20261111T100000\nSTATUS:CANCELLED\nEND:VEVENT\n"
      "BEGIN:VEVENT\nSUMMARY:Mit Dauer\nDTSTART:20261112T100000\nDURATION:PT45M\nEND:VEVENT\n"
      "BEGIN:VEVENT\nSUMMARY:Ohne Ende\nDTSTART:20261113T100000\nEND:VEVENT\n");
  CHECK_EQ((int)evs.size(), 5);
  CHECK_STR(evs[0].summary, "Arzt, Dr. M");
  CHECK(evs[0].start == local(2026, 11, 10, 9) && evs[0].end == local(2026, 11, 10, 9, 30));
  CHECK(evs[1].allDay && evs[1].end == evs[1].start + 86400);  // all-day without an end: one day
  CHECK(evs[2].cancelled);
  CHECK(evs[3].end == evs[3].start + 45 * 60);  // stage 7b: DURATION gives the end
  CHECK(evs[4].end == evs[4].start);            // no end, not all-day: a moment
}

void test_daily_and_count_until() {
  Event ev{};
  ev.start = local(2026, 11, 1, 8);
  ev.end = ev.start + 3600;
  ev.freq = 'D';
  ev.interval = 2;
  auto s = expand(ev, local(2026, 11, 1), local(2026, 11, 8));
  CHECK_EQ((int)s.size(), 4);  // 1, 3, 5, 7 Nov
  CHECK(s[3] == local(2026, 11, 7, 8));
  ev.count = 2;
  CHECK_EQ((int)expand(ev, local(2026, 11, 1), local(2026, 12, 1)).size(), 2);
  ev.count = 0;
  ev.until = local(2026, 11, 5, 8);
  CHECK_EQ((int)expand(ev, local(2026, 11, 1), local(2026, 12, 1)).size(), 3);  // UNTIL inclusive
}

void test_weekly_byday() {
  Event ev{};
  ev.start = local(2026, 11, 4, 18);  // a Wednesday
  ev.end = ev.start + 3600;
  ev.freq = 'W';
  ev.interval = 1;
  ics::parseRrule("FREQ=WEEKLY;BYDAY=MO,WE,FR", ev);
  auto s = expand(ev, local(2026, 11, 1), local(2026, 11, 10));
  CHECK_EQ((int)s.size(), 3);  // Wed 4, Fri 6, Mon 9 (Mon 2 is before the start)
  CHECK(s[0] == local(2026, 11, 4, 18) && s[1] == local(2026, 11, 6, 18) && s[2] == local(2026, 11, 9, 18));
}

void test_monthly() {
  Event ev{};
  ev.start = local(2026, 1, 13, 19);  // 2nd Tuesday of January 2026
  ev.end = ev.start + 7200;
  ev.interval = 1;
  ics::parseRrule("FREQ=MONTHLY;BYDAY=2TU", ev);
  auto s = expand(ev, local(2026, 11, 1), local(2027, 1, 1));
  CHECK_EQ((int)s.size(), 2);
  CHECK(s[0] == local(2026, 11, 10, 19) && s[1] == local(2026, 12, 8, 19));
  Event last{};
  last.start = local(2026, 1, 30, 12);
  last.end = last.start;
  last.interval = 1;
  ics::parseRrule("FREQ=MONTHLY;BYDAY=-1FR", last);
  s = expand(last, local(2026, 11, 1), local(2026, 12, 1));
  CHECK((int)s.size() == 1 && s[0] == local(2026, 11, 27, 12));
  Event d31{};
  d31.start = local(2026, 1, 31, 9);
  d31.end = d31.start;
  d31.interval = 1;
  d31.freq = 'M';
  s = expand(d31, local(2026, 2, 1), local(2026, 6, 1));
  CHECK_EQ((int)s.size(), 2);  // March and May (no 31st in February and April)
}

void test_yearly_and_exdate() {
  Event ev{};
  ev.start = local(2024, 2, 29);
  ev.end = ev.start + 86400;
  ev.allDay = true;
  ev.freq = 'Y';
  ev.interval = 1;
  CHECK_EQ((int)expand(ev, local(2025, 1, 1), local(2028, 12, 31)).size(), 1);  // only 2028
  Event w{};
  w.start = local(2026, 11, 2, 7);
  w.end = w.start + 600;
  w.freq = 'W';
  w.interval = 1;
  w.count = 3;
  w.exdates[w.exdateCount++] = local(2026, 11, 9, 7);
  auto s = expand(w, local(2026, 11, 1), local(2027, 1, 1));
  CHECK_EQ((int)s.size(), 2);  // COUNT counts the excluded one too (RFC 5545): 2 and 16 Nov
  CHECK(s[1] == local(2026, 11, 16, 7));
}

// An old repeating entry (daily since 2015, no COUNT) still reaches today's window: the expansion
// jumps close to the window instead of walking 4,000 days (the step limit).
void test_old_series_jumps() {
  Event ev{};
  ev.start = local(2015, 3, 1, 6, 30);
  ev.end = ev.start + 1800;
  ev.freq = 'D';
  ev.interval = 1;
  auto s = expand(ev, local(2026, 11, 23), local(2026, 11, 25));
  CHECK_EQ((int)s.size(), 2);
  CHECK(s[0] == local(2026, 11, 23, 6, 30));
}

void test_broken_input() {
  auto evs = parseAll("BEGIN:VEVENT\nDTSTART:nonsense\nSUMMARY:x\nEND:VEVENT\nEND:VEVENT\nBEGIN:VEVENT\n");
  CHECK_EQ((int)evs.size(), 1);
  CHECK(evs[0].start == 0);  // no start: the caller skips it
  // A very long line (a description) doesn't break the next one.
  std::string big = "BEGIN:VEVENT\nDESCRIPTION:" + std::string(3000, 'x') + "\nSUMMARY:Nach langer Zeile\nDTSTART:20261110T100000\nEND:VEVENT\n";
  evs = parseAll(big.c_str());
  CHECK((int)evs.size() == 1 && !std::strcmp(evs[0].summary, "Nach langer Zeile"));
}

// Stage 7b: an alarm inside an entry (BEGIN:VALARM) has its own SUMMARY / DURATION; they are not
// the entry's (the old reader showed the alarm text as the title).
void test_alarm_inside_event() {
  auto evs = parseAll(
      "BEGIN:VEVENT\nSUMMARY:Zahnarzt\nDTSTART:20261110T100000\nDTEND:20261110T110000\n"
      "BEGIN:VALARM\nACTION:EMAIL\nSUMMARY:Erinnerung\nDURATION:PT5M\nTRIGGER:-PT15M\nEND:VALARM\n"
      "END:VEVENT\n");
  CHECK_EQ((int)evs.size(), 1);
  CHECK_STR(evs[0].summary, "Zahnarzt");
  CHECK(evs[0].end == local(2026, 11, 10, 11));
}

// Review finding (build 80): a file without the final line break keeps its last entry; finish()
// can complete two lines at once (the one pending and the last one).
void test_no_final_newline() {
  auto evs = parseAll("BEGIN:VCALENDAR\r\nBEGIN:VEVENT\r\nSUMMARY:Letzter\r\nDTSTART:20261110T100000\r\nEND:VEVENT\r\nEND:VCALENDAR");
  CHECK_EQ((int)evs.size(), 1);
  evs = parseAll("BEGIN:VEVENT\nSUMMARY:Ohne Ende\nDTSTART:20261110T100000\nEND:VEVENT");
  CHECK_EQ((int)evs.size(), 1);
}

}  // namespace

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  test_unfold_and_split();
  test_values();
  test_event_fields();
  test_daily_and_count_until();
  test_weekly_byday();
  test_monthly();
  test_yearly_and_exdate();
  test_old_series_jumps();
  test_broken_input();
  test_alarm_inside_event();
  test_no_final_newline();
  return finish("ics parser");
}
