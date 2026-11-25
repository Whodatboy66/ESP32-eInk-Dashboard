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

// ---- Birthday titles ----
namespace detail {

// Case-insensitive (ASCII) test: does s begin with `lower` (written in lower case)?
inline bool startsNoCase(const char* s, const char* lower) {
  for (; *lower; s++, lower++)
    if (std::tolower(static_cast<unsigned char>(*s)) != static_cast<unsigned char>(*lower)) return false;
  return true;
}

// Characters between the birthday word and the name: "Geburtstag: Anna", "Birthday - Tom".
inline size_t separatorLen(const char* s) {
  if (*s == ' ' || *s == ':' || *s == '-' || *s == ',') return 1;
  if (!std::strncmp(s, "\xE2\x80\x93", 3)) return 3;  // en dash
  return 0;
}

inline void trimEnd(char* s, size_t& n) {
  while (n && separatorLen(s + n - 1) == 1) s[--n] = '\0';
}

// Drops the word "birthday" from a title: the cake symbol already says it. A word that is only
// the beginning of another ("Geburtstagsfeier") stays; so does a title that would be empty.
inline void dropBirthdayWord(char* s) {
  static const char* const BEFORE[] = {"geburtstag von", "birthday of", "geburtstag", "birthday", "geb.", "bday"};
  static const char* const AFTER[] = {" hat geburtstag", "'s birthday", "\xE2\x80\x99s birthday",
                                      " geburtstag", " birthday"};
  size_t n = std::strlen(s);
  for (const char* w : BEFORE) {
    const size_t len = std::strlen(w);
    if (n <= len || !startsNoCase(s, w) || !separatorLen(s + len)) continue;
    size_t skip = len;
    while (size_t sep = separatorLen(s + skip)) skip += sep;
    if (skip < n) {
      std::memmove(s, s + skip, n - skip + 1);
      n -= skip;
    }
    break;
  }
  for (const char* w : AFTER) {
    const size_t len = std::strlen(w);
    if (n <= len || !startsNoCase(s + n - len, w)) continue;
    size_t keep = n - len;
    trimEnd(s, keep);
    if (keep) {
      s[keep] = '\0';
      n = keep;
    }
    break;
  }
}

}  // namespace detail

// The name of a birthday entry and its year of birth, if the title ends with one in parentheses,
// without the word "birthday": "Anna (1966)" -> "Anna", 1966; "Geburtstag Lennart Kempf" ->
// "Lennart Kempf", 0 (no age).
inline void birthdayName(const char* title, char* out, size_t cap, uint16_t& year) {
  year = 0;
  std::snprintf(out, cap, "%s", title ? title : "");
  size_t n = std::strlen(out);
  while (n && out[n - 1] == ' ') out[--n] = '\0';
  bool hasYear = n >= 7 && out[n - 1] == ')' && out[n - 6] == '(';
  int y = 0;
  for (size_t i = n - 5; hasYear && i < n - 1; i++) {
    if (!std::isdigit(static_cast<unsigned char>(out[i]))) hasYear = false;
    else y = y * 10 + (out[i] - '0');
  }
  if (hasYear && y >= 1900 && y <= 2100) {
    year = static_cast<uint16_t>(y);
    n -= 6;
    out[n] = '\0';
    while (n && out[n - 1] == ' ') out[--n] = '\0';
  }
  detail::dropBirthdayWord(out);
}

}  // namespace ics
