#pragma once
// Per data service (weather, stocks, calendar, tracking): when it last worked, retries after a
// failure, and whether its data on screen is old (docs/plans/STAGE3_WIDGETS_LAYOUT.md, decision 7).
// Kept in RTC memory (lost only on a power cut, like the data caches). Plain C++, tested on the PC
// (tests/host/test_service_state.cpp).
#include <cstdint>
#include <ctime>

enum class Service : uint8_t { Weather, Stocks, Calendar, Tracking };
constexpr int SERVICE_COUNT = 4;

struct ServiceState {
  time_t lastOk = 0;      // last successful fetch (0 = never)
  time_t retryAt = 0;     // after a failure: try again from here
  uint8_t failures = 0;   // failures in a row
};

constexpr int SERVICE_MAX_RETRIES = 4;
constexpr time_t SERVICE_REAL_TIME = 1700000000;  // before this the clock isn't set yet

// After a fetch. A failure is retried after 2, 4, 8 and 16 minutes (never later than the service's
// normal interval); after that only the regular schedule tries again, so a permanent error (a
// misspelled city, a wrong key) doesn't keep Wi-Fi on every few minutes. Without real time (right
// after power-on, before NTP) nothing is recorded.
inline void recordResult(ServiceState& s, bool ok, time_t now, time_t intervalS) {
  if (now <= SERVICE_REAL_TIME) return;
  if (ok) {
    s.lastOk = now;
    s.failures = 0;
    s.retryAt = 0;
    return;
  }
  if (s.failures < 255) s.failures++;
  if (s.failures > SERVICE_MAX_RETRIES) {
    s.retryAt = 0;
    return;
  }
  time_t wait = (time_t)60 << s.failures;
  if (intervalS > 0 && wait > intervalS) wait = intervalS;
  s.retryAt = now + wait;
}

inline bool retryDue(const ServiceState& s, time_t now) {
  return s.failures && s.retryAt && now >= s.retryAt;
}

// The data shown is old: no success for two intervals (at least 30 minutes). Never fetched is not
// "old" (the widget shows "not available"); a clock moved back by NTP is not either.
inline bool isStale(const ServiceState& s, time_t now, time_t intervalS) {
  if (!s.lastOk || now <= s.lastOk || now <= SERVICE_REAL_TIME) return false;
  time_t limit = 2 * intervalS < 30 * 60 ? 30 * 60 : 2 * intervalS;
  return now - s.lastOk > limit;
}
