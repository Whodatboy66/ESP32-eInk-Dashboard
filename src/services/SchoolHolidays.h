#pragma once
// School holidays (stage 7, docs/plans/GIFT_EDITION.md): ranges read from a calendar link (our own
// ICS per Bundesland, or one of the user's). Kept in RTC memory between wakes. Plain C++, tested on
// the PC (tests/host/test_school_holidays.cpp, tests/host/test_month_model.cpp).
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace school {

constexpr int MAX_RANGES = 8;

struct Range {
  long fromDay = 0, toDay = 0;  // personal::dayNumber of the first and the last day (inclusive)
  char name[40] = "";           // "Weihnachtsferien" (up to 19 characters)
};

struct SchoolList {
  uint8_t count = 0;
  Range r[MAX_RANGES];
};

// The link to read: the user's own link if set, else our feed for the state (published by
// .github/workflows/feeds.yml on the fixed pre-release "feeds"). False without either, or if the
// link doesn't fit.
inline bool feedUrl(char* out, size_t cap, const char* repo, const char* state, const char* custom) {
  int n;
  if (custom && custom[0]) n = std::snprintf(out, cap, "%s", custom);
  else if (state && state[0]) n = std::snprintf(out, cap, "https://github.com/%s/releases/download/feeds/ferien-%s.ics", repo, state);
  else return false;
  if (n < 0 || static_cast<size_t>(n) >= cap) {
    if (cap) out[0] = '\0';
    return false;
  }
  return true;
}

// Adds a range (first and last day, inclusive), sorted by start. Ranges that ended more than 31
// days before today are left out; the same range twice is kept once; with more than MAX_RANGES the
// nearest stay.
inline void addRange(SchoolList& l, long today, long fromDay, long toDay, const char* name) {
  if (toDay < fromDay || toDay < today - 31) return;
  for (int i = 0; i < l.count; i++)
    if (l.r[i].fromDay == fromDay && l.r[i].toDay == toDay) return;
  int pos = l.count;
  while (pos > 0 && l.r[pos - 1].fromDay > fromDay) pos--;
  if (pos >= MAX_RANGES) return;
  int last = l.count < MAX_RANGES ? l.count : MAX_RANGES - 1;
  for (int i = last; i > pos; i--) l.r[i] = l.r[i - 1];
  l.r[pos].fromDay = fromDay;
  l.r[pos].toDay = toDay;
  std::snprintf(l.r[pos].name, sizeof(l.r[pos].name), "%s", name ? name : "");
  if (l.count < MAX_RANGES) l.count++;
}

inline bool sameList(const SchoolList& a, const SchoolList& b) {
  if (a.count != b.count) return false;
  for (int i = 0; i < a.count; i++)
    if (a.r[i].fromDay != b.r[i].fromDay || a.r[i].toDay != b.r[i].toDay || std::strcmp(a.r[i].name, b.r[i].name))
      return false;
  return true;
}

// After a download: the fresh list replaces the kept one only if the download worked and found
// something (a broken or empty file keeps the last good list). sourceChanged (another state or
// link): the fresh list replaces it even when empty. True if the kept list changed.
inline bool apply(SchoolList& kept, const SchoolList& fresh, bool ok, bool sourceChanged = false) {
  if (!ok || (fresh.count == 0 && !sourceChanged) || sameList(kept, fresh)) return false;
  kept = fresh;
  return true;
}

// The school holidays going on today, or the next ones; nullptr if none are known.
inline const Range* currentOrNext(const SchoolList& l, long today) {
  for (int i = 0; i < l.count; i++)
    if (l.r[i].toDay >= today) return &l.r[i];
  return nullptr;
}

}  // namespace school
