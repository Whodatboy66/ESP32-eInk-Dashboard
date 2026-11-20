#pragma once
// Weekday and month names and the date formats on the screen, per language (instead of strftime,
// whose names are English only). Plain C++, tested on the PC (tests/host/test_i18n.cpp).
#include <cstdio>
#include <ctime>

#include "Strings.h"

namespace i18n {

namespace names {
// Index: tm_wday (0 = Sunday) and tm_mon (0 = January).
inline const char* const WEEKDAY_SHORT[2][7] = {{"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"},
                                                {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"}};
inline const char* const WEEKDAY_LONG[2][7] = {
    {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"},
    {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"}};
inline const char* const MONTH_SHORT[2][12] = {
    {"Jan", "Feb", "Mär", "Apr", "Mai", "Jun", "Jul", "Aug", "Sep", "Okt", "Nov", "Dez"},
    {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"}};
inline const char* const MONTH_LONG[2][12] = {
    {"Januar", "Februar", "März", "April", "Mai", "Juni", "Juli", "August", "September", "Oktober",
     "November", "Dezember"},
    {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October",
     "November", "December"}};
// The month view's header row, Monday first.
inline const char* const WEEKDAY_INITIAL[2][7] = {{"Mo", "Di", "Mi", "Do", "Fr", "Sa", "So"},
                                                  {"M", "T", "W", "T", "F", "S", "S"}};
inline int li(Lang lang) { return lang == Lang::En ? 1 : 0; }
}  // namespace names

inline const char* weekdayShort(int wday, Lang lang) { return names::WEEKDAY_SHORT[names::li(lang)][(wday % 7 + 7) % 7]; }
inline const char* weekdayLong(int wday, Lang lang) { return names::WEEKDAY_LONG[names::li(lang)][(wday % 7 + 7) % 7]; }
inline const char* monthShort(int mon, Lang lang) { return names::MONTH_SHORT[names::li(lang)][(mon % 12 + 12) % 12]; }
inline const char* monthLong(int mon, Lang lang) { return names::MONTH_LONG[names::li(lang)][(mon % 12 + 12) % 12]; }
// 0 = Monday ... 6 = Sunday.
inline const char* weekdayInitial(int mondayFirst, Lang lang) {
  return names::WEEKDAY_INITIAL[names::li(lang)][(mondayFirst % 7 + 7) % 7];
}

// Status bar: "Fr 20. Nov" / "Fri 20 Nov".
inline void formatStatusDate(const std::tm& t, Lang lang, char* out, size_t cap) {
  std::snprintf(out, cap, lang == Lang::En ? "%s %d %s" : "%s %d. %s", weekdayShort(t.tm_wday, lang), t.tm_mday,
                monthShort(t.tm_mon, lang));
}

// Agenda day header: "Freitag, 20. November" / "Friday, 20 November".
inline void formatDayHeader(const std::tm& t, Lang lang, char* out, size_t cap) {
  std::snprintf(out, cap, lang == Lang::En ? "%s, %d %s" : "%s, %d. %s", weekdayLong(t.tm_wday, lang), t.tm_mday,
                monthLong(t.tm_mon, lang));
}

// Month view: "November 2026" (mon: 0 = January).
inline void formatMonthTitle(int year, int mon, Lang lang, char* out, size_t cap) {
  std::snprintf(out, cap, "%s %d", monthLong(mon, lang), year);
}

// A short date: "21.11." / "Nov 21".
inline void formatShortDate(const std::tm& t, Lang lang, char* out, size_t cap) {
  if (lang == Lang::En) std::snprintf(out, cap, "%s %d", monthShort(t.tm_mon, lang), t.tm_mday);
  else std::snprintf(out, cap, "%d.%d.", t.tm_mday, t.tm_mon + 1);
}

// Countdown to a flight: "in 45m", "in 3h05m", "in 2T 4h" / "in 2d 4h".
inline void formatCountdown(long secs, Lang lang, char* out, size_t cap) {
  if (secs < 3600) std::snprintf(out, cap, text(S::CountdownMin, lang), secs / 60);
  else if (secs < 86400) std::snprintf(out, cap, text(S::CountdownHours, lang), secs / 3600, (secs % 3600) / 60);
  else std::snprintf(out, cap, text(S::CountdownDays, lang), secs / 86400, (secs % 86400) / 3600);
}

}  // namespace i18n
