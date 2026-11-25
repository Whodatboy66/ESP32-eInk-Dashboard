// Host test for src/services/CalendarLinks.h (stage 8): each calendar link has its own interval, and
// a link not read in this refresh keeps its entries, marks and flight. Run: tests/host/run.sh
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "src/services/CalendarLinks.h"
#include "tests/host/check.h"

namespace {

using namespace icslinks;

// 2026-11-25 (a Wednesday) 10:00 UTC; the tests run in UTC.
const time_t DAY = 1795000000 - 1795000000 % 86400;
time_t at(int dayOffset, int hour, int minute = 0) { return DAY + dayOffset * 86400L + hour * 3600L + minute * 60L; }

CalEventItem item(const char* title, time_t start, time_t end, uint8_t link) {
  CalEventItem e{};
  std::snprintf(e.title, sizeof(e.title), "%s", title);
  e.start = start;
  e.end = end;
  e.link = link;
  return e;
}

void test_link_due() {
  // Every 3 hours (rounded up to the clock step).
  CHECK(!linkDue(EVERY_3H, at(0, 7), at(0, 9, 59), false, 1));
  CHECK(linkDue(EVERY_3H, at(0, 7), at(0, 10), false, 1));
  CHECK(linkDue(EVERY_3H, 0, at(0, 10), false, 1));  // never read
  // Mornings only: the first refresh from 06:00 on a new day, once.
  CHECK(linkDue(EVERY_MORNING, at(-1, 6, 30), at(0, 6), false, 1));
  CHECK(!linkDue(EVERY_MORNING, at(-1, 6, 30), at(0, 5, 59), false, 1));
  CHECK(!linkDue(EVERY_MORNING, at(0, 6), at(0, 14), true, 1));  // read today: not again
  CHECK(linkDue(EVERY_MORNING, 0, at(0, 14), false, 1));         // never read (after a reset): now
  // With the calendar: whatever the calendar's own interval says.
  CHECK(linkDue(EVERY_CALENDAR, at(0, 9), at(0, 9, 5), true, 1));
  CHECK(!linkDue(EVERY_CALENDAR, at(0, 9), at(0, 9, 5), false, 1));
}

// Link 1 (bit 0) is read, link 2 (bit 1) isn't: link 2's entries stay, link 1's are replaced; ended
// ones go; by start; at most cap.
void test_merge_keeps_unread_links() {
  const time_t now = at(0, 10);
  CalEventItem kept[] = {item("old 1", at(0, 11), at(0, 12), 0), item("work A", at(0, 13), at(0, 14), 1),
                         item("work ended", at(0, 8), at(0, 9), 1), item("work B", at(1, 9), at(1, 10), 1)};
  CalEventItem fresh[] = {item("new 1", at(0, 12), at(0, 13), 0)};
  CalEventItem out[10];
  int n = mergeAgenda(kept, 4, 0x01, fresh, 1, now, out, 10);
  CHECK_EQ(n, 3);
  CHECK_STR(out[0].title, "new 1");
  CHECK_STR(out[1].title, "work A");
  CHECK_STR(out[2].title, "work B");
  CHECK_EQ(mergeAgenda(kept, 4, 0x01, fresh, 1, now, out, 2), 2);  // the cap
}

// A link whose address changed or that was removed: its kept entries and days go (replaced by nothing).
void test_link_changed_drops_kept() {
  const time_t now = at(0, 10);
  CalEventItem kept[] = {item("work A", at(0, 13), at(0, 14), 1), item("family", at(0, 15), at(0, 16), 0)};
  CalEventItem out[10];
  int n = mergeAgenda(kept, 2, 0x02, nullptr, 0, now, out, 10);  // link 2 dropped, nothing new
  CHECK_EQ(n, 1);
  CHECK_STR(out[0].title, "family");
  uint32_t days[3] = {1u << 3, 1u << 7, 1u << 9};
  CHECK_EQ(mergedMonthDays(days, 0x07), (1u << 3) | (1u << 7) | (1u << 9));
  CHECK_EQ(mergedMonthDays(days, 0x01), 1u << 3);  // only links that mark the month
}

// The kept flight of an unread link stays while it isn't over; a fresh earlier one wins.
void test_flight_choice() {
  const time_t now = at(0, 10);
  CalEventItem kept = item("LH 400", at(2, 9), at(2, 17), 1);
  CalEventItem fresh = item("LH 1", at(1, 9), at(1, 11), 0);
  CHECK_STR(chooseFlight(kept, 0x01, fresh, now).title, "LH 1");
  CalEventItem none{};
  CHECK_STR(chooseFlight(kept, 0x01, none, now).title, "LH 400");
  CHECK_EQ(chooseFlight(kept, 0x02, none, now).start, 0);  // its link was read again: no flight there now
  CalEventItem over = item("old", at(-1, 9), at(-1, 11), 1);
  CHECK_EQ(chooseFlight(over, 0x01, none, now).start, 0);
}

}  // namespace

int main() {
  setenv("TZ", "UTC0", 1);
  tzset();
  test_link_due();
  test_merge_keeps_unread_links();
  test_link_changed_drops_kept();
  test_flight_choice();
  return finish("calendar links");
}
