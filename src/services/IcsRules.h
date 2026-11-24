#pragma once
// Rules for calendar entries (stage 7, docs/plans/STAGE7_NEW_LOOK.md task 7). Plain C++, tested on
// the PC (tests/host/test_personal.cpp).
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace ics {

// A yearly all-day entry of a normal calendar link is a birthday (how Google stores a birthday
// created as an event), when the setting "birthdays from the calendars" is on.
inline bool countsAsBirthday(char freq, bool allDay, bool settingOn) { return settingOn && freq == 'Y' && allDay; }

// School holiday links (our feed, or a school's own calendar): only all-day entries are days off;
// a timed entry ("Elternabend 19:00") isn't.
inline bool countsAsSchoolHoliday(bool allDay) { return allDay; }

// The name of a birthday entry and its year of birth, if the title ends with one in parentheses:
// "Anna (1966)" -> "Anna", 1966. Otherwise the title as it is and year 0 (no age).
inline void birthdayName(const char* title, char* out, size_t cap, uint16_t& year) {
  year = 0;
  std::snprintf(out, cap, "%s", title ? title : "");
  size_t n = std::strlen(out);
  while (n && out[n - 1] == ' ') out[--n] = '\0';
  if (n < 7 || out[n - 1] != ')' || out[n - 6] != '(') return;
  int y = 0;
  for (size_t i = n - 5; i < n - 1; i++) {
    if (!std::isdigit(static_cast<unsigned char>(out[i]))) return;
    y = y * 10 + (out[i] - '0');
  }
  if (y < 1900 || y > 2100) return;
  year = static_cast<uint16_t>(y);
  n -= 6;
  out[n] = '\0';
  while (n && out[n - 1] == ' ') out[--n] = '\0';
}

}  // namespace ics
