#pragma once
// Public holidays of the German states (stage 7, docs/plans/GIFT_EDITION.md): calculated on the
// device, nothing downloaded. Plain C++, tested on the PC (tests/host/test_holidays.cpp,
// tests/host/test_config_validation.cpp for the state codes).
#include <cstring>

#include "../i18n/Strings.h"

namespace holidays {

enum class State : unsigned char { None, BW, BY, BE, BB, HB, HH, HE, MV, NI, NW, RP, SL, SN, ST, SH, TH };
constexpr int STATE_COUNT = 16;

struct StateInfo {
  State state;
  const char* code;
  const char* name;  // the official name, the same in both languages
};

inline const StateInfo* stateTable() {
  static const StateInfo table[STATE_COUNT] = {
      {State::BW, "BW", "Baden-Württemberg"}, {State::BY, "BY", "Bayern"},
      {State::BE, "BE", "Berlin"},            {State::BB, "BB", "Brandenburg"},
      {State::HB, "HB", "Bremen"},            {State::HH, "HH", "Hamburg"},
      {State::HE, "HE", "Hessen"},            {State::MV, "MV", "Mecklenburg-Vorpommern"},
      {State::NI, "NI", "Niedersachsen"},     {State::NW, "NW", "Nordrhein-Westfalen"},
      {State::RP, "RP", "Rheinland-Pfalz"},   {State::SL, "SL", "Saarland"},
      {State::SN, "SN", "Sachsen"},           {State::ST, "ST", "Sachsen-Anhalt"},
      {State::SH, "SH", "Schleswig-Holstein"}, {State::TH, "TH", "Thüringen"},
  };
  return table;
}

// "" or an unknown code: None (no holidays).
inline State parseState(const char* code) {
  if (!code) return State::None;
  for (int i = 0; i < STATE_COUNT; i++)
    if (!std::strcmp(stateTable()[i].code, code)) return stateTable()[i].state;
  return State::None;
}

inline const char* stateCode(State s) {
  for (int i = 0; i < STATE_COUNT; i++)
    if (stateTable()[i].state == s) return stateTable()[i].code;
  return "";
}

inline const char* stateName(State s) {
  for (int i = 0; i < STATE_COUNT; i++)
    if (stateTable()[i].state == s) return stateTable()[i].name;
  return "";
}

// ---- Calendar arithmetic (proleptic Gregorian, days since 1970-01-01) ----

inline long daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const long yoe = y - era * 400;
  const long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

inline void civilFromDays(long z, int& y, int& m, int& d) {
  z += 719468;
  const long era = (z >= 0 ? z : z - 146096) / 146097;
  const long doe = z - era * 146097;
  const long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const long mp = (5 * doy + 2) / 153;
  d = (int)(doy - (153 * mp + 2) / 5 + 1);
  m = (int)(mp < 10 ? mp + 3 : mp - 9);
  y = (int)(yoe + era * 400 + (m <= 2));
}

// 1 = Monday ... 7 = Sunday.
inline int isoWeekday(long dayNumber) { return (int)(((dayNumber % 7) + 7 + 3) % 7) + 1; }

// ISO 8601 week number (1..53): the week with the year's first Thursday is week 1.
inline int isoWeek(int year, int month, int day) {
  long n = daysFromCivil(year, month, day);
  long thursday = n - isoWeekday(n) + 4;
  int ty, tm, td;
  civilFromDays(thursday, ty, tm, td);
  return (int)((thursday - daysFromCivil(ty, 1, 1)) / 7 + 1);
}

// Easter Sunday (Gregorian; the anonymous Gregorian algorithm).
inline void easter(int year, int& month, int& day) {
  int a = year % 19, b = year / 100, c = year % 100, d = b / 4, e = b % 4;
  int f = (b + 8) / 25, g = (b - f + 1) / 3, h = (19 * a + b - d - g + 15) % 30;
  int i = c / 4, k = c % 4, l = (32 + 2 * e + 2 * i - h - k) % 7;
  int m = (a + 11 * h + 22 * l) / 451;
  month = (h + l - 7 * m + 114) / 31;
  day = (h + l - 7 * m + 114) % 31 + 1;
}

// ---- The holidays ----

struct Holiday {
  short year = 0;
  unsigned char month = 0, day = 0;
  i18n::S name = i18n::S::Count;
};

namespace detail {

constexpr unsigned stateBit(State s) { return 1u << static_cast<int>(s); }
constexpr unsigned ALL = 0x1FFFEu;  // bits 1..16: every state

enum class When : unsigned char { Fixed, Easter, Repentance };

struct Rule {
  When when;
  signed char month, day;  // Fixed: the date; Easter: day = offset from Easter Sunday
  unsigned states;
  i18n::S name;
};

// Statewide holidays only (local ones such as the Augsburger Friedensfest are left out; Mariä
// Himmelfahrt counts for all of Bayern, decided 2026-11-23).
inline const Rule* rules(int& count) {
  using i18n::S;
  static const Rule table[] = {
      {When::Fixed, 1, 1, ALL, S::HolNewYear},
      {When::Fixed, 1, 6, stateBit(State::BW) | stateBit(State::BY) | stateBit(State::ST), S::HolEpiphany},
      {When::Fixed, 3, 8, stateBit(State::BE) | stateBit(State::MV), S::HolWomensDay},
      {When::Easter, 0, -2, ALL, S::HolGoodFriday},
      {When::Easter, 0, 0, stateBit(State::BB), S::HolEaster},
      {When::Easter, 0, 1, ALL, S::HolEasterMonday},
      {When::Fixed, 5, 1, ALL, S::HolLabour},
      {When::Easter, 0, 39, ALL, S::HolAscension},
      {When::Easter, 0, 49, stateBit(State::BB), S::HolWhitSunday},
      {When::Easter, 0, 50, ALL, S::HolWhitMonday},
      {When::Easter, 0, 60,
       stateBit(State::BW) | stateBit(State::BY) | stateBit(State::HE) | stateBit(State::NW) | stateBit(State::RP) | stateBit(State::SL),
       S::HolCorpusChristi},
      {When::Fixed, 8, 15, stateBit(State::BY) | stateBit(State::SL), S::HolAssumption},
      {When::Fixed, 9, 20, stateBit(State::TH), S::HolChildrensDay},
      {When::Fixed, 10, 3, ALL, S::HolUnity},
      {When::Fixed, 10, 31,
       stateBit(State::BB) | stateBit(State::HB) | stateBit(State::HH) | stateBit(State::MV) | stateBit(State::NI) | stateBit(State::SN) |
           stateBit(State::ST) | stateBit(State::SH) | stateBit(State::TH),
       S::HolReformation},
      {When::Fixed, 11, 1, stateBit(State::BW) | stateBit(State::BY) | stateBit(State::NW) | stateBit(State::RP) | stateBit(State::SL),
       S::HolAllSaints},
      {When::Repentance, 11, 0, stateBit(State::SN), S::HolRepentance},
      {When::Fixed, 12, 25, ALL, S::HolChristmas},
      {When::Fixed, 12, 26, ALL, S::HolBoxing},
  };
  count = static_cast<int>(sizeof(table) / sizeof(table[0]));
  return table;
}

inline void dateOf(const Rule& r, int year, int& month, int& day) {
  if (r.when == When::Fixed) {
    month = r.month;
    day = r.day;
  } else if (r.when == When::Easter) {
    int em, ed;
    easter(year, em, ed);
    int y;
    civilFromDays(daysFromCivil(year, em, ed) + r.day, y, month, day);
  } else {  // Buß- und Bettag: the Wednesday between 16 and 22 November
    month = 11;
    day = 16;
    while (isoWeekday(daysFromCivil(year, 11, day)) != 3) day++;
  }
}

}  // namespace detail

// The holidays of a year in a state, sorted by date; at most cap are written. Returns how many.
inline int holidaysOfYear(int year, State s, Holiday* out, int cap) {
  if (s == State::None || cap <= 0) return 0;
  int count;
  const detail::Rule* r = detail::rules(count);
  Holiday all[24];
  int n = 0;
  for (int i = 0; i < count && n < 24; i++) {
    if (!(r[i].states & detail::stateBit(s))) continue;
    int m, d;
    detail::dateOf(r[i], year, m, d);
    Holiday h;
    h.year = static_cast<short>(year);
    h.month = static_cast<unsigned char>(m);
    h.day = static_cast<unsigned char>(d);
    h.name = r[i].name;
    int j = n++;
    while (j > 0 && all[j - 1].month * 100 + all[j - 1].day > m * 100 + d) {
      all[j] = all[j - 1];
      j--;
    }
    all[j] = h;
  }
  if (n > cap) n = cap;
  for (int i = 0; i < n; i++) out[i] = all[i];
  return n;
}

// Is that day a holiday in the state? The holiday in *found (may be nullptr).
inline bool holidayOn(int year, int month, int day, State s, Holiday* found) {
  if (s == State::None) return false;
  int count;
  const detail::Rule* r = detail::rules(count);
  for (int i = 0; i < count; i++) {
    if (!(r[i].states & detail::stateBit(s))) continue;
    int m, d;
    detail::dateOf(r[i], year, m, d);
    if (m == month && d == day) {
      if (found) {
        found->year = static_cast<short>(year);
        found->month = static_cast<unsigned char>(m);
        found->day = static_cast<unsigned char>(d);
        found->name = r[i].name;
      }
      return true;
    }
  }
  return false;
}

}  // namespace holidays
