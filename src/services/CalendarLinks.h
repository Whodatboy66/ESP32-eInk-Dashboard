#pragma once
// Calendar links (stage 8, docs/plans/STAGE8_DATA_AND_SCREENS.md task A4): each link has its own
// interval, and a link that isn't read in a refresh keeps its agenda entries, its marks in the month
// and its flight (a large work calendar needn't be read every time). Plain C++, tested on the PC
// (tests/host/test_calendar_links.cpp); Calendar.cpp reads the links, App.cpp decides which.
#include <cstdint>
#include <ctime>

#include "../core/DataStore.h"
#include "ServiceData.h"

namespace icslinks {

// How often a link is read (setting `ics_every`, per link).
constexpr uint8_t EVERY_CALENDAR = 0;  // with the calendar's interval (default)
constexpr uint8_t EVERY_3H = 1;        // every 3 hours
constexpr uint8_t EVERY_MORNING = 2;   // once a day, the first refresh from 06:00
constexpr int MORNING_FROM_MIN = 6 * 60;

inline long localDay(time_t t) {
  std::tm lt{};
  localtime_r(&t, &lt);
  return (lt.tm_year + 1900L) * 400 + lt.tm_yday;  // only compared for order
}

// Due now: with the calendar, when the calendar is (calendarDue); every 3 hours by age; mornings only
// on a new local day from 06:00 (the first wake after the quiet hours, when they end later). A link
// never read (lastRead 0, after a reset) is due at once.
inline bool linkDue(uint8_t every, time_t lastRead, time_t now, bool calendarDue, int stepMin) {
  if (every == EVERY_CALENDAR) return calendarDue;
  if (lastRead == 0 || now < datastore::REAL_TIME) return true;
  if (every == EVERY_3H) return datastore::dueByAge(lastRead, now, 180, stepMin, false);
  std::tm lt{};
  localtime_r(&now, &lt);
  return localDay(lastRead) < localDay(now) && lt.tm_hour * 60 + lt.tm_min >= MORNING_FROM_MIN;
}

// The agenda after a refresh: kept entries of links not in `replaced` that haven't ended, and the
// fresh entries of the links in `replaced` (read now, or dropped: changed address, removed), by start,
// at most cap. Returns the number written to out.
inline int mergeAgenda(const CalEventItem* kept, int keptCount, uint8_t replaced, const CalEventItem* fresh,
                       int freshCount, time_t now, CalEventItem* out, int cap) {
  int n = 0;
  auto insert = [&](const CalEventItem& e) {
    int pos = n;
    while (pos > 0 && out[pos - 1].start > e.start) pos--;
    if (pos >= cap) return;
    for (int j = (n < cap ? n : cap - 1); j > pos; j--) out[j] = out[j - 1];
    out[pos] = e;
    if (n < cap) n++;
  };
  for (int i = 0; i < keptCount; i++)
    if (!(replaced & (1u << kept[i].link)) && kept[i].end > now) insert(kept[i]);
  for (int i = 0; i < freshCount; i++)
    if (replaced & (1u << fresh[i].link)) insert(fresh[i]);
  return n;
}

// The month's marked days (bit d = day d): the links that mark the month (ics_month_mask).
inline uint32_t mergedMonthDays(const uint32_t* perLink, uint8_t monthMask) {
  uint32_t days = 0;
  for (int i = 0; i < 3; i++)
    if (monthMask & (1u << i)) days |= perLink[i];
  return days;
}

// The pinned flight: the kept one while its link isn't replaced and it isn't over, or the fresh one
// (from the links read now) when that departs earlier. start == 0: none.
inline CalEventItem chooseFlight(const CalEventItem& kept, uint8_t replaced, const CalEventItem& fresh, time_t now) {
  const bool keepOld = kept.start && !(replaced & (1u << kept.link)) && kept.end > now;
  if (fresh.start && (!keepOld || fresh.start < kept.start)) return fresh;
  if (keepOld) return kept;
  return CalEventItem{};
}

}  // namespace icslinks
