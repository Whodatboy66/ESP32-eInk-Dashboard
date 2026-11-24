// Host test for src/services/SchoolHolidays.h (stage 7): the feed link, collecting ranges, keeping
// the last good list. Run: tests/host/run.sh
#include <cstring>

#include "src/personal/PersonalText.h"
#include "src/services/SchoolHolidays.h"
#include "tests/host/check.h"

using personal::Date;
using personal::dayNumber;

namespace {

void test_feed_url() {
  char u[160];
  CHECK(school::feedUrl(u, sizeof(u), "Whodatboy66/ESP32-eInk-Dashboard", "BY", ""));
  CHECK_STR(u, "https://github.com/Whodatboy66/ESP32-eInk-Dashboard/releases/download/feeds/ferien-BY.ics");
  CHECK(school::feedUrl(u, sizeof(u), "a/b", "BY", "https://school.example/ferien.ics"));  // own link wins
  CHECK_STR(u, "https://school.example/ferien.ics");
  CHECK(!school::feedUrl(u, sizeof(u), "a/b", "", ""));  // no state, no own link
  CHECK(school::feedUrl(u, sizeof(u), "a/b", "", "https://x.example/f.ics"));  // own link without state
  CHECK(!school::feedUrl(u, 20, "a/b", "BY", ""));  // too small: no cut link
}

void test_add_range() {
  school::SchoolList l;
  const long today = dayNumber(Date{2026, 11, 23});
  school::addRange(l, today, dayNumber(Date{2026, 12, 23}), dayNumber(Date{2027, 1, 9}), "Weihnachtsferien");
  school::addRange(l, today, dayNumber(Date{2026, 10, 31}), dayNumber(Date{2026, 11, 6}), "Herbstferien");
  school::addRange(l, today, dayNumber(Date{2026, 12, 23}), dayNumber(Date{2027, 1, 9}), "Weihnachtsferien");  // twice
  CHECK_EQ(l.count, 2);
  CHECK(l.r[0].fromDay < l.r[1].fromDay);  // sorted
  CHECK_STR(l.r[1].name, "Weihnachtsferien");
  // Long ago: not kept (more than 31 days before today).
  school::addRange(l, today, dayNumber(Date{2026, 7, 30}), dayNumber(Date{2026, 9, 14}), "Sommerferien");
  CHECK_EQ(l.count, 2);
  // More than 8: the nearest stay.
  school::SchoolList many;
  for (int i = 0; i < 12; i++) {
    long from = today + 10 + i * 30;
    char name[24];
    std::snprintf(name, sizeof(name), "F%d", i);
    school::addRange(many, today, from, from + 5, name);
  }
  CHECK_EQ(many.count, school::MAX_RANGES);
  CHECK_STR(many.r[0].name, "F0");
  CHECK_STR(many.r[7].name, "F7");
  school::addRange(many, today, today + 1, today + 2, "Soon");  // nearer than all: in, the farthest out
  CHECK_STR(many.r[0].name, "Soon");
  CHECK_STR(many.r[7].name, "F6");
}

// Review Focus 4: a failed or empty download keeps the last good list.
void test_school_apply_keeps_old() {
  school::SchoolList kept, fresh, empty;
  const long today = dayNumber(Date{2026, 11, 23});
  school::addRange(kept, today, today + 30, today + 46, "Weihnachtsferien");
  school::addRange(fresh, today, today + 31, today + 46, "Weihnachtsferien");
  CHECK(!school::apply(kept, fresh, false));  // failed: nothing changes
  CHECK_EQ(kept.r[0].fromDay, today + 30);
  CHECK(!school::apply(kept, empty, true));   // a file without any holiday: keep the old ones
  CHECK_EQ(kept.count, 1);
  CHECK(school::apply(kept, fresh, true));    // a good new list replaces it
  CHECK_EQ(kept.r[0].fromDay, today + 31);
  CHECK(!school::apply(kept, fresh, true));   // the same again: no change
  // Another state or link: its list replaces the old one, even when empty.
  CHECK(school::apply(kept, empty, true, true));
  CHECK_EQ(kept.count, 0);
  CHECK(!school::apply(kept, fresh, false, true));  // but not after a failed download
}

void test_current_or_next() {
  school::SchoolList l;
  const long today = dayNumber(Date{2027, 1, 4});
  school::addRange(l, today, dayNumber(Date{2026, 12, 23}), dayNumber(Date{2027, 1, 9}), "Weihnachtsferien");
  school::addRange(l, today, dayNumber(Date{2027, 2, 15}), dayNumber(Date{2027, 2, 19}), "Winterferien");
  const school::Range* r = school::currentOrNext(l, today);
  CHECK(r && !std::strcmp(r->name, "Weihnachtsferien"));
  r = school::currentOrNext(l, dayNumber(Date{2027, 1, 10}));
  CHECK(r && !std::strcmp(r->name, "Winterferien"));
  CHECK(school::currentOrNext(l, dayNumber(Date{2027, 3, 1})) == nullptr);
}

}  // namespace

int main() {
  test_feed_url();
  test_add_range();
  test_school_apply_keeps_old();
  test_current_or_next();
  return finish("school holidays");
}
