#pragma once
// State shared by the wake cycle, the setup page and the update flow.
#include <Preferences.h>
#include <stdint.h>
#include <time.h>

#include "ServiceState.h"

extern Preferences preferences;       // NVS, namespace "config" (open only briefly)
extern bool forceUpdateStatusBar;     // redraw the status bar in this wake
extern bool apModeActive;             // setup mode (access point + web server) is running
extern ServiceState serviceStates[SERVICE_COUNT];  // RTC: per data service (ServiceState.h)
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
