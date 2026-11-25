#pragma once
// Problems shown in the status bar (which sources failed), kept in RTC memory across deep sleep.
#include <stdint.h>
#include <stdio.h>

#include "../i18n/Strings.h"

// One list for the flags and their texts (stage 7b): each is set when its source fails, cleared on
// its next success, kept in deep sleep. The order gives the bits: append only. The second column
// names the texts: Prob<x> (the short name) and Pt<x> (the sentence, as in the app).
#define PROBLEM_LIST(X) \
  X(WIFI, Wifi) X(TIME, Time) X(WEATHER, Weather) X(STOCKS, Stocks) X(CALENDAR, Calendar) \
  X(PRINTER, Printer) X(PRINTER_CODE, PrinterCode) X(MAKERWORLD, MakerWorld) X(TRACKING, Tracking) \
  X(FLIGHT, Flight) X(BIRTHDAYS, Birthdays)

enum ProblemBit : uint8_t {
#define PROBLEM_BIT(name, label) PROBLEM_BIT_##name,
  PROBLEM_LIST(PROBLEM_BIT)
#undef PROBLEM_BIT
  PROBLEM_COUNT
};
enum : uint16_t {
#define PROBLEM_FLAG(name, label) PROBLEM_##name = 1u << PROBLEM_BIT_##name,
  PROBLEM_LIST(PROBLEM_FLAG)
#undef PROBLEM_FLAG
};
inline const i18n::S PROBLEM_LABELS[PROBLEM_COUNT] = {
#define PROBLEM_LABEL(name, label) i18n::S::Prob##label,
  PROBLEM_LIST(PROBLEM_LABEL)
#undef PROBLEM_LABEL
};
inline const i18n::S PROBLEM_TITLES[PROBLEM_COUNT] = {
#define PROBLEM_TITLE(name, label) i18n::S::Pt##label,
  PROBLEM_LIST(PROBLEM_TITLE)
#undef PROBLEM_TITLE
};

// The status bar's problem line (build 84): the first problem's sentence ("Kalender 2 kann nicht
// gelesen werden" when exactly one calendar link failed, bit i of calFailedMask = link i + 1; with
// the link's name from stage 8: "Arbeit kann nicht gelesen werden"; linkNames may be nullptr). The
// number of problems; 0 = none (out empty).
inline int problemLine(uint16_t flags, uint8_t calFailedMask, const char* const* linkNames, i18n::Lang lang,
                       char* out, size_t cap) {
  int n = 0, first = -1;
  for (int bit = 0; bit < PROBLEM_COUNT; bit++)
    if (flags & (1u << bit)) {
      if (first < 0) first = bit;
      n++;
    }
  if (cap) out[0] = '\0';
  if (first < 0) return 0;
  const bool oneLink = calFailedMask && !(calFailedMask & (calFailedMask - 1));
  if (first == PROBLEM_BIT_CALENDAR && oneLink) {
    int link = 1;
    while (!(calFailedMask & 1u)) calFailedMask >>= 1, link++;
    if (linkNames && linkNames[link - 1] && linkNames[link - 1][0])
      snprintf(out, cap, i18n::text(i18n::S::PtCalendarNamed, lang), linkNames[link - 1]);
    else
      snprintf(out, cap, i18n::text(i18n::S::PtCalendarN, lang), link);
  } else {
    snprintf(out, cap, "%s", i18n::text(PROBLEM_TITLES[first], lang));
  }
  return n;
}
extern uint16_t problems;       // RTC memory
extern uint16_t problemsShown;  // what the status bar shows now (RTC memory)
extern uint8_t icsFailedMask;   // the calendar links that failed at the last read (bit i = link i + 1)
void setProblem(uint16_t flag, bool active);
