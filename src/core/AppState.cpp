#include "AppState.h"

#include <esp_attr.h>

Preferences preferences;
bool forceUpdateStatusBar = false;
bool apModeActive = false;
RTC_DATA_ATTR bool redrawAll = false;        // an update screen covered the dashboard: redraw all
uint32_t cpuFullMhz = 240;  // the board's default CPU clock, restored for Wi-Fi work

RTC_DATA_ATTR uint16_t bootCount = 0;
RTC_DATA_ATTR bool pendingBambuRetry = false;
RTC_DATA_ATTR time_t lastNtpSync = 0;
// After a failed Wi-Fi connection (router off, out of range) wait before trying again, longer
// after each failure, instead of keeping the radio on for up to a minute on every wake.
RTC_DATA_ATTR uint8_t wifiFailures = 0;
RTC_DATA_ATTR time_t wifiRetryAfter = 0;

RTC_DATA_ATTR time_t makerworldLastFetch = 0;  // MakerWorld stats: at most hourly, idle view only
