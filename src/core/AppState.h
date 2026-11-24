#pragma once
// State shared by the wake cycle, the setup page and the update flow.
#include <Preferences.h>
#include <stdint.h>
#include <time.h>

#include "ServiceState.h"
#include "../services/SchoolHolidays.h"

extern Preferences preferences;       // NVS, namespace "config" (open only briefly)
extern bool forceUpdateStatusBar;     // redraw the status bar in this wake
// Power note for the status bar ("Night until 06:00", "Battery saver: every 15 min", ...) and the
// one shown now (RTC: a change redraws the bar). Stage 6.
extern char powerNote[48];
extern uint32_t powerNoteShown;
inline uint32_t textHash(const char* s) {
  uint32_t h = 2166136261u;  // FNV-1a
  for (; *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
  return h;
}
extern bool apModeActive;             // setup mode (access point + web server) is running
extern ServiceState serviceStates[SERVICE_COUNT];  // RTC: per data service (ServiceState.h)
extern school::SchoolList schoolHolidays;          // RTC: the school holidays (stage 7), read weekly
extern uint32_t layoutSignature;      // layout drawn last (RTC): a change means a full refresh
extern bool redrawAll;                // an update screen covered the dashboard: redraw all (RTC)
extern uint32_t cpuFullMhz;           // the board's default CPU clock, restored for Wi-Fi work

// RTC memory: kept across deep sleep.
extern uint16_t bootCount;            // wakes since the last full refresh cycle
extern bool pendingBambuRetry;
extern time_t lastNtpSync;
// After a failed Wi-Fi connection wait before trying again, longer after each failure.
extern uint8_t wifiFailures;
extern time_t wifiRetryAfter;
extern time_t makerworldLastFetch;
