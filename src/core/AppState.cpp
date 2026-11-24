#include "AppState.h"

#include <esp_attr.h>

Preferences preferences;
bool forceUpdateStatusBar = false;
char powerNote[48] = "";
RTC_DATA_ATTR uint32_t powerNoteShown = 0;
bool apModeActive = false;
RTC_DATA_ATTR school::SchoolList schoolHolidays;  // stage 7: kept until a new download replaces it
RTC_DATA_ATTR ServiceState serviceStates[SERVICE_COUNT];  // per data service: last success, retries
RTC_DATA_ATTR uint32_t layoutSignature = 0;  // layouts::signature() of the screen drawn last
RTC_DATA_ATTR bool redrawAll = false;        // an update screen covered the dashboard: redraw all
uint32_t cpuFullMhz = 240;  // the board's default CPU clock, restored for Wi-Fi work

// Kept in deep sleep (RTC memory).
RTC_DATA_ATTR uint16_t bootCount = 0;          // minutes since the last restart, in wake steps (WakePlan.h)
RTC_DATA_ATTR bool pendingBambuRetry = false;  // one quick retry of the printer at the next wake
RTC_DATA_ATTR time_t lastNtpSync = 0;
RTC_DATA_ATTR uint8_t wifiFailures = 0;        // failed connections in a row: the wait grows (WakePlan.h)
RTC_DATA_ATTR time_t wifiRetryAfter = 0;       // no Wi-Fi before this (router off, out of range)
RTC_DATA_ATTR time_t makerworldLastFetch = 0;  // MakerWorld stats: at most hourly, idle view only
