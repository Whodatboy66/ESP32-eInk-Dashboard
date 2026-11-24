#pragma once
// Birthdays (docs/plans/STAGE4_PERSONAL_WIDGETS.md): the hand-entered list (/birthdays.json, API
// {"manual": [{"name", "day", "month", "year"}]}), the ones read from a calendar link
// (/bday_feed.json, same shape under "feed"), and the upcoming list the widget shows.
// Plain C++, tested on the PC.
#include <ArduinoJson.h>

#include "../i18n/Strings.h"
#include "PersonalText.h"

namespace personal {

constexpr int MAX_BIRTHDAYS = 50;
constexpr size_t BDAY_NAME_CHARS = 32;
constexpr size_t BDAY_NAME_BYTES = 2 * BDAY_NAME_CHARS + 8;
constexpr int BDAY_HORIZON_DAYS = 60;

struct Birthday {
  char name[BDAY_NAME_BYTES] = "";
  uint8_t day = 0, month = 0;
  uint16_t year = 0;  // 0 = unknown (no age)
};

struct BirthdayList {
  Birthday items[MAX_BIRTHDAYS];
  int count = 0;
};

// Parses the array under `key` ("manual" or "feed"). Names trimmed and filtered with hasGlyph;
// entries without a name are dropped. False for broken JSON, too many entries or an impossible
// day (e.g. 31 April); 29 February is allowed. The message in `lang` (as parseTasks).
inline bool parseBirthdays(const char* json, size_t len, const char* key, BirthdayList& out, char* err,
                           size_t errCap, bool (*hasGlyph)(uint32_t) = nullptr, char* removed = nullptr,
                           size_t removedCap = 0, i18n::Lang lang = i18n::Lang::En) {
  out.count = 0;
  if (err && errCap) err[0] = '\0';
  if (removed && removedCap) removed[0] = '\0';
  JsonDocument doc;
  if (deserializeJson(doc, json, len) || !doc[key].is<JsonArrayConst>()) {
    if (err) std::snprintf(err, errCap, "%s", i18n::text(i18n::S::BdaysUnreadable, lang));
    return false;
  }
  JsonArrayConst arr = doc[key].as<JsonArrayConst>();
  if (arr.size() > static_cast<size_t>(MAX_BIRTHDAYS)) {
    if (err) std::snprintf(err, errCap, i18n::text(i18n::S::BdaysTooMany, lang), MAX_BIRTHDAYS);
    return false;
  }
  for (JsonObjectConst b : arr) {
    Birthday& e = out.items[out.count];
    e = Birthday{};
    char raw[BDAY_NAME_BYTES * 2];
    copyChars(raw, sizeof(raw), b["name"] | "", BDAY_NAME_CHARS * 2);
    if (hasGlyph) {
      char rem[32];
      filterText(raw, hasGlyph, false, rem, sizeof(rem));
      if (removed && rem[0] && std::strlen(removed) + std::strlen(rem) < removedCap) std::strcat(removed, rem);
    }
    trimSpaces(raw);
    if (!raw[0]) continue;
    copyChars(e.name, sizeof(e.name), raw, BDAY_NAME_CHARS);
    int d = b["day"] | 0, m = b["month"] | 0, y = b["year"] | 0;
    if (m < 1 || m > 12 || d < 1 || d > daysInMonth(2000, m) || (y != 0 && (y < 1900 || y > 2199))) {
      if (err) std::snprintf(err, errCap, i18n::text(i18n::S::BdayBadDate, lang), e.name);
      return false;
    }
    if (y != 0 && m == 2 && d == 29 && !isLeap(y)) {  // born on 29 February: a leap year
      if (err) std::snprintf(err, errCap, i18n::text(i18n::S::BdayBadDate, lang), e.name);
      return false;
    }
    e.day = static_cast<uint8_t>(d);
    e.month = static_cast<uint8_t>(m);
    e.year = static_cast<uint16_t>(y);
    out.count++;
  }
  return true;
}

inline size_t writeBirthdays(const BirthdayList& list, const char* key, char* out, size_t cap) {
  JsonDocument doc;
  JsonArray arr = doc[key].to<JsonArray>();
  for (int i = 0; i < list.count; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["name"] = list.items[i].name;
    o["day"] = list.items[i].day;
    o["month"] = list.items[i].month;
    o["year"] = list.items[i].year;
  }
  if (measureJson(doc) + 1 > cap) return 0;
  return serializeJson(doc, out, cap);
}

// The next date of this birthday on or after today (29 February -> 28 February in other years).
inline Date nextOccurrence(const Birthday& b, const Date& today) {
  for (int y = today.y; y <= today.y + 1; y++) {
    int d = b.day;
    if (b.month == 2 && d == 29 && !isLeap(y)) d = 28;
    Date c{y, b.month, d};
    if (dayNumber(c) >= dayNumber(today)) return c;
  }
  return Date{};
}

struct Upcoming {
  const Birthday* who;
  Date date;
  int days;     // 0 = today
  int age;      // 0 = unknown
  bool fromFeed;
};

// The birthdays in the next `horizon` days from the lists, soonest first (same name and day once;
// the hand-entered entry wins, it may know the year). `cal`: birthdays found in the normal calendar
// links (stage 7), may be nullptr. Returns how many were written.
inline int upcomingBirthdays(const BirthdayList& manual, const BirthdayList& feed, const Date& today,
                             Upcoming* out, int max, int horizon = BDAY_HORIZON_DAYS,
                             const BirthdayList* cal = nullptr) {
  if (!today.valid()) return 0;
  int n = 0;
  auto add = [&](const Birthday& b, bool fromFeed) {
    if (n >= max) return;
    Date next = nextOccurrence(b, today);
    if (!next.valid()) return;
    int days = static_cast<int>(dayNumber(next) - dayNumber(today));
    if (days > horizon) return;
    for (int i = 0; i < n; i++)
      if (out[i].who->day == b.day && out[i].who->month == b.month && std::strcmp(out[i].who->name, b.name) == 0)
        return;
    int age = b.year ? next.y - b.year : 0;
    out[n++] = Upcoming{&b, next, days, age > 0 ? age : 0, fromFeed};
  };
  for (int i = 0; i < manual.count; i++) add(manual.items[i], false);
  for (int i = 0; i < feed.count; i++) add(feed.items[i], true);
  if (cal)
    for (int i = 0; i < cal->count; i++) add(cal->items[i], true);
  // soonest first (stable)
  for (int a = 1; a < n; a++) {
    Upcoming v = out[a];
    int b = a - 1;
    while (b >= 0 && out[b].days > v.days) {
      out[b + 1] = out[b];
      b--;
    }
    out[b + 1] = v;
  }
  return n;
}

// Same entries (name, day, month, year), in any order. Stage 7: the calendar's birthday file is only
// written when this says something changed (flash wear, the calendar is read every 30-60 minutes).
inline bool sameBirthdays(const BirthdayList& a, const BirthdayList& b) {
  if (a.count != b.count) return false;
  bool used[MAX_BIRTHDAYS] = {};
  for (int i = 0; i < a.count; i++) {
    bool found = false;
    for (int j = 0; j < b.count && !found; j++) {
      if (used[j]) continue;
      const Birthday &x = a.items[i], &y = b.items[j];
      if (x.day == y.day && x.month == y.month && x.year == y.year && !std::strcmp(x.name, y.name)) found = used[j] = true;
    }
    if (!found) return false;
  }
  return true;
}

// "Heute" / "Today", "Morgen" / "Tomorrow", "in 12 T." / "in 12 d".
inline void daysLabel(int days, char* out, size_t cap, i18n::Lang lang) {
  if (days == 0) std::snprintf(out, cap, "%s", i18n::text(i18n::S::BdayToday, lang));
  else if (days == 1) std::snprintf(out, cap, "%s", i18n::text(i18n::S::BdayTomorrow, lang));
  else std::snprintf(out, cap, i18n::text(i18n::S::BdayInDays, lang), days);
}

}  // namespace personal
