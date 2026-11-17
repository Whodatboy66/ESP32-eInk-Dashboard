#pragma once
// Problems shown in the status bar (which sources failed), kept in RTC memory across deep sleep.
#include <stdint.h>

// Problems listed in the top bar. Each flag is set when that source fails, cleared on its
// next success, and kept across deep sleep.
enum : uint16_t {
  PROBLEM_WIFI         = 1 << 0,
  PROBLEM_TIME         = 1 << 1,
  PROBLEM_WEATHER      = 1 << 2,
  PROBLEM_STOCKS       = 1 << 3,
  PROBLEM_CALENDAR     = 1 << 4,
  PROBLEM_PRINTER      = 1 << 5,
  PROBLEM_PRINTER_CODE = 1 << 6,
  PROBLEM_MAKERWORLD   = 1 << 7,
  PROBLEM_TRACKING     = 1 << 8,
  PROBLEM_FLIGHT       = 1 << 9,
};
inline const char* const PROBLEM_LABELS[] = {
  "WiFi", "Time", "Weather", "Stocks", "Calendar", "Printer", "Access code", "MakerWorld", "Tracking", "Flight"
};
extern uint16_t problems;       // RTC memory
extern uint16_t problemsShown;  // what the status bar shows now (RTC memory)
void setProblem(uint16_t flag, bool active);
