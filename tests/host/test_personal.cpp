// Host test for src/personal/ (tasks, note, birthdays). Run: tests/host/run.sh
#include <string>

#include "src/personal/Birthdays.h"
#include "src/personal/Notes.h"
#include "src/personal/Tasks.h"
#include "tests/host/check.h"

using namespace personal;

// The screen font in this test: ASCII letters, digits, basic punctuation, umlauts (not emoji, not ✓).
static bool fontHas(uint32_t cp) {
  return (cp >= 0x21 && cp <= 0x7E) || cp == 0xE4 || cp == 0xF6 || cp == 0xFC || cp == 0xDF || cp == 0xC4;
}

static void test_dates() {
  CHECK_EQ(dayNumber(Date{1970, 1, 1}), 0);
  CHECK_EQ(dayNumber(Date{2026, 11, 17}) - dayNumber(Date{2026, 11, 16}), 1);
  CHECK_EQ(dayNumber(Date{2027, 1, 1}) - dayNumber(Date{2026, 12, 31}), 1);
  CHECK_EQ(weekday(Date{2026, 11, 17}), 1);  // a Tuesday
  CHECK_EQ(weekday(Date{2026, 11, 22}), 6);  // Sunday
  Date d;
  CHECK(parseIsoDate("2026-02-28", d) && d.valid());
  CHECK(!parseIsoDate("2026-02-29", d));  // not a leap year
  CHECK(parseIsoDate("2028-02-29", d));
  CHECK(!parseIsoDate("2026-13-01", d));
  CHECK(!parseIsoDate("tomorrow", d));
  CHECK(parseIsoDate("", d) && !d.valid());
}

static void test_utf8_and_filter() {
  CHECK_EQ(utf8Count("Grüße"), 5u);
  char out[8];
  CHECK(!copyChars(out, sizeof(out), "äöüäöü", 10));  // 7 bytes room: 3 umlauts (6 bytes)
  CHECK_STR(out, "äöü");
  CHECK(!copyChars(out, sizeof(out), "abcdef", 3));
  CHECK_STR(out, "abc");
  char s[64] = "Milch ✓ kaufen\r\nmorgen\tfrüh";
  char removed[16];
  filterText(s, fontHas, true, removed, sizeof(removed));
  CHECK_STR(s, "Milch  kaufen\nmorgen früh");
  CHECK_STR(removed, "✓");
  char t[32] = "a\nb";
  filterText(t, fontHas, false);
  CHECK_STR(t, "a b");
}

static void test_tasks_parse_and_limits() {
  TaskList list;
  char err[96], removed[16];
  const char* json =
      R"({"tasks":[{"title":"  Buy milk ","due":"2026-11-20","prio":1,"done":false},)"
      R"({"title":"   ","due":""},{"title":"Call ✓ Ben","prio":7,"done":true}]})";
  CHECK(parseTasks(json, std::strlen(json), list, err, sizeof(err), fontHas, removed, sizeof(removed)));
  CHECK_EQ(list.count, 2);  // the empty title is dropped
  CHECK_STR(list.items[0].title, "Buy milk");
  CHECK_EQ(list.items[0].due.d, 20);
  CHECK_EQ(list.items[1].prio, 3);  // clamped
  CHECK(list.items[1].done);
  CHECK_STR(list.items[1].title, "Call  Ben");
  CHECK_STR(removed, "✓");

  std::string many = "{\"tasks\":[";
  for (int i = 0; i < 31; i++) many += std::string(i ? "," : "") + "{\"title\":\"t\"}";
  many += "]}";
  CHECK(!parseTasks(many.c_str(), many.size(), list, err, sizeof(err)));
  CHECK(std::strstr(err, "30") != nullptr);

  const char* badDate = R"({"tasks":[{"title":"x","due":"2026-02-30"}]})";
  CHECK(!parseTasks(badDate, std::strlen(badDate), list, err, sizeof(err)));
  CHECK(!parseTasks("nope", 4, list, err, sizeof(err)));

  std::string longTitle = "{\"tasks\":[{\"title\":\"" + std::string(100, 'x') + "\"}]}";
  CHECK(parseTasks(longTitle.c_str(), longTitle.size(), list, err, sizeof(err)));
  CHECK_EQ(utf8Count(list.items[0].title), TASK_TITLE_CHARS);
}

static void test_tasks_round_trip() {
  TaskList a, b;
  char err[64], buf[2048];
  const char* json = R"({"tasks":[{"title":"Öl wechseln","due":"2026-12-01","prio":2,"done":false}]})";
  CHECK(parseTasks(json, std::strlen(json), a, err, sizeof(err)));
  size_t n = writeTasks(a, buf, sizeof(buf));
  CHECK(n > 0);
  CHECK(parseTasks(buf, n, b, err, sizeof(err)));
  CHECK_EQ(b.count, 1);
  CHECK_STR(b.items[0].title, "Öl wechseln");
  CHECK_EQ(b.items[0].due.m, 12);
  CHECK_EQ(writeTasks(a, buf, 10), 0u);  // doesn't fit
}

static void test_task_order_and_labels() {
  Date today{2026, 11, 17};
  TaskList l;
  auto add = [&](const char* t, Date due, uint8_t prio, bool done = false) {
    Task& x = l.items[l.count++];
    copyChars(x.title, sizeof(x.title), t, 60);
    x.due = due;
    x.prio = prio;
    x.done = done;
  };
  add("no date low", Date{}, 3);
  add("next week", Date{2026, 11, 24}, 2);
  add("overdue", Date{2026, 11, 10}, 3);
  add("done", Date{2026, 11, 1}, 1, true);
  add("today high", Date{2026, 11, 17}, 1);
  add("today normal", Date{2026, 11, 17}, 2);
  add("no date high", Date{}, 1);
  int order[MAX_TASKS];
  int n = openTaskOrder(l, today, order, MAX_TASKS);
  CHECK_EQ(n, 6);  // done is not shown
  const char* expected[] = {"overdue", "today high", "today normal", "next week", "no date high", "no date low"};
  for (int i = 0; i < n && i < 6; i++) CHECK_STR(l.items[order[i]].title, expected[i]);

  char lab[16];
  bool over;
  dueLabel(l.items[2], today, lab, sizeof(lab), &over);
  CHECK_STR(lab, "10.11.");
  CHECK(over);
  dueLabel(l.items[4], today, lab, sizeof(lab), &over);
  CHECK_STR(lab, "today");
  CHECK(!over);
  Task t;
  t.due = Date{2026, 11, 18};
  dueLabel(t, today, lab, sizeof(lab), &over);
  CHECK_STR(lab, "tomorrow");
  t.due = Date{2026, 11, 20};
  dueLabel(t, today, lab, sizeof(lab), &over);
  CHECK_STR(lab, "Fri");
  dueLabel(l.items[1], today, lab, sizeof(lab), &over);
  CHECK_STR(lab, "24.11.");
  dueLabel(l.items[0], today, lab, sizeof(lab), &over);
  CHECK_STR(lab, "");
}

static void test_done_tasks_are_removed_after_30_days() {
  Date today{2026, 11, 20};
  TaskList stored, edited;
  char err[64];
  const char* json = R"({"tasks":[{"title":"old","done":true,"doneAt":"2026-10-20"},)"
                     R"({"title":"recent","done":true,"doneAt":"2026-11-01"},{"title":"open"},)"
                     R"({"title":"just done","done":true}]})";
  CHECK(parseTasks(json, std::strlen(json), stored, err, sizeof(err)));
  CHECK(stored.items[0].doneAt.valid());
  CHECK(!stored.items[3].doneAt.valid());
  // The app sends the list back without the dates: they come from the stored list, new ones today.
  const char* fromApp = R"({"tasks":[{"title":"old","done":true},{"title":"recent","done":true},)"
                        R"({"title":"open","done":false},{"title":"just done","done":true}]})";
  CHECK(parseTasks(fromApp, std::strlen(fromApp), edited, err, sizeof(err)));
  stampDoneDates(edited, stored, today);
  CHECK_EQ(edited.items[0].doneAt.m, 10);
  CHECK_EQ(edited.items[3].doneAt.d, 20);  // today
  CHECK(!edited.items[2].doneAt.valid());
  CHECK_EQ(purgeDoneTasks(edited, today), 1);  // "old": 31 days
  CHECK_EQ(edited.count, 3);
  CHECK_STR(edited.items[0].title, "recent");
  CHECK_EQ(purgeDoneTasks(edited, Date{2026, 12, 1}), 1);  // "recent": 30 days on 1 December
  CHECK_EQ(purgeDoneTasks(edited, Date{}), 0);  // no date: nothing
  char buf[1024];
  size_t n = writeTasks(edited, buf, sizeof(buf));
  CHECK(std::string(buf, n).find("\"doneAt\":\"2026-11-20\"") != std::string::npos);
}

static void test_note() {
  char note[NOTE_BYTES], err[64], removed[16];
  const char* json = R"({"note":"Wi-Fi for guests:\r\nSunflower-42 ✓"})";
  CHECK(parseNote(json, std::strlen(json), note, sizeof(note), err, sizeof(err), fontHas, removed, sizeof(removed)));
  CHECK_STR(note, "Wi-Fi for guests:\nSunflower-42");
  std::string tooLong = "{\"note\":\"" + std::string(401, 'a') + "\"}";
  CHECK(!parseNote(tooLong.c_str(), tooLong.size(), note, sizeof(note), err, sizeof(err)));
  char buf[64];
  size_t n = writeNote("a\nb", buf, sizeof(buf));
  CHECK(n > 0);
  CHECK(parseNote(buf, n, note, sizeof(note), err, sizeof(err)));
  CHECK_STR(note, "a\nb");
}

static void test_wrap_cases() {
  auto w = [](const char*, int len) { return len * 10; };
  TextLine lines[8];
  bool cut;
  const char* text = "Bins out on Thursday\n\nWi-Fi: Sunflower";
  int n = wrapText(text, 100, w, lines, 8, &cut);
  CHECK_EQ(n, 6);
  CHECK(!cut);
  CHECK(std::string(text + lines[5].start, lines[5].len) == "Sunflower");
  n = wrapText(text, 100, w, lines, 2, &cut);
  CHECK_EQ(n, 2);
  CHECK(cut);
  const char* longWord = "Donaudampfschifffahrt";
  n = wrapText(longWord, 100, w, lines, 8, &cut);
  CHECK_EQ(n, 3);
  CHECK_EQ(lines[0].len, 10);
  CHECK_EQ(lines[2].len, 1);
  n = wrapText("", 100, w, lines, 8, &cut);
  CHECK_EQ(n, 0);
}

static void test_birthdays() {
  BirthdayList manual, feed;
  char err[96];
  const char* json =
      R"({"manual":[{"name":"Anna","day":17,"month":11,"year":1990},{"name":"Joe","day":21,"month":11},)"
      R"({"name":"Leap","day":29,"month":2,"year":2004},{"name":"Silvester","day":31,"month":12}]})";
  CHECK(parseBirthdays(json, std::strlen(json), "manual", manual, err, sizeof(err)));
  CHECK_EQ(manual.count, 4);
  const char* bad = R"({"manual":[{"name":"X","day":31,"month":4}]})";
  CHECK(!parseBirthdays(bad, std::strlen(bad), "manual", manual, err, sizeof(err)));
  const char* badLeap = R"({"manual":[{"name":"X","day":29,"month":2,"year":2003}]})";
  CHECK(!parseBirthdays(badLeap, std::strlen(badLeap), "manual", feed, err, sizeof(err)));
  CHECK(parseBirthdays(json, std::strlen(json), "manual", manual, err, sizeof(err)));

  const char* feedJson = R"({"feed":[{"name":"Anna","day":17,"month":11},{"name":"Mia","day":29,"month":11}]})";
  CHECK(parseBirthdays(feedJson, std::strlen(feedJson), "feed", feed, err, sizeof(err)));

  Upcoming up[10];
  int n = upcomingBirthdays(manual, feed, Date{2026, 11, 17}, up, 10);
  CHECK_EQ(n, 4);  // Anna (once), Joe, Mia, Silvester; Leap is too far away
  CHECK_STR(up[0].who->name, "Anna");
  CHECK_EQ(up[0].days, 0);
  CHECK_EQ(up[0].age, 36);
  CHECK(!up[0].fromFeed);  // the hand-entered one wins
  CHECK_STR(up[1].who->name, "Joe");
  CHECK_EQ(up[1].age, 0);
  CHECK_STR(up[2].who->name, "Mia");
  CHECK(up[2].fromFeed);
  CHECK_STR(up[3].who->name, "Silvester");
  CHECK_EQ(up[3].days, 44);

  // Across the year end, and 29 February in a year without it.
  n = upcomingBirthdays(manual, feed, Date{2027, 1, 20}, up, 10);
  CHECK_EQ(n, 1);
  CHECK_STR(up[0].who->name, "Leap");
  CHECK_EQ(up[0].date.d, 28);
  CHECK_EQ(up[0].age, 23);
  n = upcomingBirthdays(manual, feed, Date{2026, 12, 31}, up, 10);
  CHECK_STR(up[0].who->name, "Silvester");
  CHECK_EQ(up[0].days, 0);

  char lab[16];
  daysLabel(0, lab, sizeof(lab));
  CHECK_STR(lab, "Today");
  daysLabel(1, lab, sizeof(lab));
  CHECK_STR(lab, "Tomorrow");
  daysLabel(12, lab, sizeof(lab));
  CHECK_STR(lab, "in 12 d");

  char buf[4096];
  size_t len = writeBirthdays(manual, "manual", buf, sizeof(buf));
  BirthdayList again;
  CHECK(parseBirthdays(buf, len, "manual", again, err, sizeof(err)));
  CHECK_EQ(again.count, manual.count);
  CHECK_EQ(again.items[0].year, 1990);
}

int main() {
  test_dates();
  test_utf8_and_filter();
  test_tasks_parse_and_limits();
  test_tasks_round_trip();
  test_task_order_and_labels();
  test_done_tasks_are_removed_after_30_days();
  test_note();
  test_wrap_cases();
  test_birthdays();
  return finish("personal");
}
