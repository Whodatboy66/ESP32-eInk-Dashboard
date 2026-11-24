#pragma once
// When a calendar feed is read (the birthday link daily, the school holidays weekly). Kept in RTC
// memory. A failing link waits for its retry time; only another link (a changed setting) is tried
// at once. (Build 79 review: the retry time used to be cleared on every wake while a link failed,
// so Wi-Fi went on every minute.) Plain C++, tested on the PC (tests/host/test_feed_schedule.cpp).
#include <cstdint>
#include <ctime>

struct FeedState {
  long day = 0;              // day number of the last successful read
  time_t retryAt = 0;        // after a failure: not before this
  uint32_t appliedHash = 0;  // link of the data on the device (0 = none)
  uint32_t triedHash = 0;    // link of the last attempt
};

// Due: a link is set (hash != 0 is the caller's business), there is a date, and the interval has
// passed or the link changed, and no retry wait is running for this link.
inline bool feedDue(FeedState& s, uint32_t hash, long today, int everyDays, time_t now) {
  if (hash != s.triedHash) s.retryAt = 0;  // another link: try right away
  if (!today) return false;
  return (today - s.day >= everyDays || hash != s.appliedHash) && now >= s.retryAt;
}

inline void feedResult(FeedState& s, uint32_t hash, bool ok, long today, time_t now, time_t retryS) {
  s.triedHash = hash;
  if (ok) {
    s.day = today;
    s.appliedHash = hash;
    s.retryAt = 0;
  } else {
    s.retryAt = now + retryS;
  }
}

// The data on the device belongs to the current link (and the feed is wanted at all). Otherwise it
// is dropped: switched off, or another state / link whose data hasn't arrived yet.
inline bool keepFeedData(const FeedState& s, bool wanted, uint32_t hash) {
  return wanted && hash != 0 && hash == s.appliedHash;
}

// The data was dropped: read the link again as soon as it is wanted.
inline void dropFeedData(FeedState& s) { s.appliedHash = 0; }
