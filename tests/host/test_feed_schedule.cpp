// Host test for src/core/FeedSchedule.h: when a calendar feed (birthday link, school holidays) is
// read, and that a failing link waits for its retry instead of turning Wi-Fi on every wake (review
// finding 1 of build 79). Run: tests/host/run.sh
#include "src/core/FeedSchedule.h"
#include "tests/host/check.h"

namespace {

constexpr time_t T = 1795000000;  // a real time
constexpr long DAY = 20400;       // a day number
constexpr uint32_t LINK = 0x1234, OTHER = 0x9999;

void test_failing_link_waits() {
  FeedState s;
  CHECK(feedDue(s, LINK, DAY, 7, T));  // never read: due
  feedResult(s, LINK, false, DAY, T, 6 * 3600);
  // The next wakes (every minute): not due until the retry time, even though nothing was applied.
  CHECK(!feedDue(s, LINK, DAY, 7, T + 60));
  CHECK(!feedDue(s, LINK, DAY, 7, T + 3 * 3600));
  CHECK(feedDue(s, LINK, DAY, 7, T + 6 * 3600));
}

void test_new_link_right_away() {
  FeedState s;
  feedResult(s, LINK, false, DAY, T, 6 * 3600);
  CHECK(feedDue(s, OTHER, DAY, 7, T + 60));  // another link (state changed): try now
  feedResult(s, OTHER, false, DAY, T + 60, 6 * 3600);
  CHECK(!feedDue(s, OTHER, DAY, 7, T + 120));  // and then it waits too
}

void test_interval() {
  FeedState s;
  feedResult(s, LINK, true, DAY, T, 6 * 3600);
  CHECK(!feedDue(s, LINK, DAY, 7, T + 60));
  CHECK(!feedDue(s, LINK, DAY + 6, 7, T + 6 * 86400));
  CHECK(feedDue(s, LINK, DAY + 7, 7, T + 7 * 86400));
  CHECK(feedDue(s, LINK, DAY + 1, 1, T + 86400));  // daily (the birthday link)
  CHECK(!feedDue(s, LINK, 0, 7, T));               // no date yet: never
}

// Review finding 4: the kept data belongs to the link it came from.
void test_keep_data() {
  FeedState s;
  CHECK(!keepFeedData(s, true, LINK));  // nothing applied yet
  feedResult(s, LINK, true, DAY, T, 6 * 3600);
  CHECK(keepFeedData(s, true, LINK));
  CHECK(!keepFeedData(s, true, OTHER));  // another state / link: the old list goes
  CHECK(!keepFeedData(s, false, LINK));  // switched off ("Bundesland: Aus"): it goes
  // Dropped data is read again as soon as it is wanted (not only after the week).
  dropFeedData(s);
  CHECK(feedDue(s, LINK, DAY, 7, T + 60));
}

}  // namespace

int main() {
  test_failing_link_waits();
  test_new_link_right_away();
  test_interval();
  test_keep_data();
  return finish("feed schedule");
}
