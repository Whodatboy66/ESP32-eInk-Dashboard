// Host test for src/core/ServiceState.h. Run: tests/host/run.sh
#include "src/core/ServiceState.h"
#include "tests/host/check.h"

static const time_t NOW = 1794904880;
static const time_t HOUR = 3600;

static void test_success_resets() {
  ServiceState s{};
  recordResult(s, false, NOW, HOUR);
  recordResult(s, true, NOW + 100, HOUR);
  CHECK_EQ(s.failures, 0);
  CHECK_EQ(s.lastOk, NOW + 100);
  CHECK(!retryDue(s, NOW + 10 * HOUR));
}

static void test_retry_after_failure_backs_off() {
  ServiceState s{};
  recordResult(s, false, NOW, 6 * HOUR);      // first failure: retry after 2 min
  CHECK(!retryDue(s, NOW + 119));
  CHECK(retryDue(s, NOW + 120));
  recordResult(s, false, NOW + 120, 6 * HOUR);  // second: 4 min
  CHECK(!retryDue(s, NOW + 120 + 239));
  CHECK(retryDue(s, NOW + 120 + 240));
  recordResult(s, false, NOW + 360, 6 * HOUR);  // third: 8 min
  CHECK_EQ(s.retryAt - (NOW + 360), 8 * 60);
  recordResult(s, false, NOW + 840, 6 * HOUR);  // fourth: 16 min
  CHECK_EQ(s.retryAt - (NOW + 840), 16 * 60);
  // After that only the regular schedule: a permanent error (a misspelled city) must not keep
  // Wi-Fi on every few minutes.
  recordResult(s, false, NOW + 1800, 6 * HOUR);
  CHECK(!retryDue(s, NOW + 100 * HOUR));
}

static void test_permanent_failure_costs_few_extra_sessions_per_day() {
  // A 6-hour service that always fails, one wake per minute for a day: count the Wi-Fi sessions
  // its retries add (the regular fetches every 6 h are not counted).
  ServiceState s{};
  int retries = 0;
  for (int minute = 0; minute < 24 * 60; minute++) {
    time_t now = NOW + minute * 60;
    bool regular = minute % 360 == 0;
    bool retry = retryDue(s, now);
    if (retry && !regular) retries++;
    if (regular || retry) recordResult(s, false, now, 6 * HOUR);
  }
  CHECK(retries <= 4 * 4);  // at most 4 retries after each of the 4 regular attempts
  CHECK(retries >= 4);
}

static void test_retry_never_later_than_the_normal_interval() {
  ServiceState s{};
  for (int i = 0; i < 4; i++) recordResult(s, false, NOW, 10 * 60);  // 10-minute service
  CHECK_EQ(s.retryAt - NOW, 10 * 60);  // 16 min capped at 10
}

static void test_stale() {
  ServiceState s{};
  CHECK(!isStale(s, NOW, HOUR));               // never fetched: "not available", not "old"
  recordResult(s, true, NOW, HOUR);
  CHECK(!isStale(s, NOW + 2 * HOUR, HOUR));    // two intervals: still fine
  CHECK(isStale(s, NOW + 2 * HOUR + 1, HOUR));
  recordResult(s, true, NOW, 5 * 60);          // short interval: at least 30 minutes
  CHECK(!isStale(s, NOW + 30 * 60, 5 * 60));
  CHECK(isStale(s, NOW + 30 * 60 + 1, 5 * 60));
}

static void test_no_real_time_yet() {
  // Right after power-on, before NTP: the clock says 1970. Nothing is recorded, nothing is old.
  ServiceState s{};
  recordResult(s, true, 1000, HOUR);
  CHECK_EQ(s.lastOk, 0);
  recordResult(s, false, 1000, HOUR);
  CHECK_EQ(s.failures, 0);
  s.lastOk = NOW;
  CHECK(!isStale(s, 5000, HOUR));
}

static void test_clock_jump_back_is_not_stale() {
  ServiceState s{};
  recordResult(s, true, NOW, HOUR);
  CHECK(!isStale(s, NOW - 5000, HOUR));        // NTP moved the clock back
}

int main() {
  test_success_resets();
  test_retry_after_failure_backs_off();
  test_retry_never_later_than_the_normal_interval();
  test_permanent_failure_costs_few_extra_sessions_per_day();
  test_no_real_time_yet();
  test_stale();
  test_clock_jump_back_is_not_stale();
  return finish("service state");
}
