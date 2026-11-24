#pragma once
// Problems shown in the status bar (which sources failed), kept in RTC memory across deep sleep.
#include <stdint.h>

#include "../i18n/Strings.h"

// One list for the flags and their status-bar names (stage 7b): each is set when its source fails,
// cleared on its next success, kept in deep sleep. The order gives the bits: append only.
#define PROBLEM_LIST(X) \
  X(WIFI, ProbWifi) X(TIME, ProbTime) X(WEATHER, ProbWeather) X(STOCKS, ProbStocks) X(CALENDAR, ProbCalendar) \
  X(PRINTER, ProbPrinter) X(PRINTER_CODE, ProbPrinterCode) X(MAKERWORLD, ProbMakerWorld) X(TRACKING, ProbTracking) \
  X(FLIGHT, ProbFlight) X(BIRTHDAYS, ProbBirthdays)

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
#define PROBLEM_LABEL(name, label) i18n::S::label,
  PROBLEM_LIST(PROBLEM_LABEL)
#undef PROBLEM_LABEL
};
extern uint16_t problems;       // RTC memory
extern uint16_t problemsShown;  // what the status bar shows now (RTC memory)
void setProblem(uint16_t flag, bool active);
