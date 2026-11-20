#pragma once
// What one wake does: which widgets fetch new data, which redraw from kept data, whether Wi-Fi
// is needed. Extracted unchanged from setup() (build 66), so it can be tested on the PC
// (tests/host/test_wake_plan.cpp). The wake cycle (App) gathers the inputs, runs the plan and does
// the side effects (Wi-Fi, fetches, drawing).
//
// Refresh intervals are in minutes. bootCount counts minutes: it advances by the wake interval
// (stepMin: 1 normally, more with a slower clock or the battery saver, stage 6) at each wake; a widget
// with refresh N fetches when bootCount is a multiple of N rounded up to a multiple of the interval.
// bootCount is 0 after a button wake and every 1440 minutes (daily full refresh).
#include <cstdint>
#include <ctime>

constexpr time_t NTP_RESYNC_S = 60 * 60;            // re-sync internet time at least hourly
constexpr int BAMBU_IDLE_REFRESH_MIN = 10;          // printer poll while idle (a divisor of 60, so it
                                                    // shares the Wi-Fi session of the hourly fetches)
constexpr time_t MAKERWORLD_REFRESH_S = 60 * 60;    // MakerWorld stats at most hourly
constexpr time_t WIFI_BACKOFF_MAX_S = 30 * 60;      // longest wait while Wi-Fi keeps failing
constexpr time_t FLIGHT_LIVE_BEFORE_S = 6 * 3600;   // live flight status from 6 h before departure
constexpr time_t FLIGHT_LIVE_REFRESH_S = 30 * 60;   // then every 30 minutes until landing,
constexpr time_t FLIGHT_LIVE_FAST_S = 10 * 60;      // every 10 minutes around departure and landing
constexpr time_t FLIGHT_LIVE_AFTER_MAX_S = 3 * 3600;  // never more than 3 h past the expected landing
constexpr int FLIGHT_LIVE_MAX_REQUESTS = 30;        // at most this many AeroDataBox requests per flight
constexpr uint32_t WAKE_AFTER_MINUTE_MS = 500;      // wake this long after each minute starts

// The next flight from the calendar and its live status (liveFlight, kept in RTC memory).
struct FlightState {
  time_t nextFlightStart = 0;   // cal.nextFlight.start (0 = no flight within 7 days)
  time_t calStart = 0, calEnd = 0;
  time_t depScheduled = 0, depActual = 0, arrScheduled = 0, arrActual = 0;
  time_t lastFetch = 0;
  uint8_t requests = 0;
  bool valid = false;           // live data received
  bool finished = false;        // status Arrived / Canceled / Diverted
  bool hasNumber = false;       // a flight number was found in the title
};

struct WakeInputs {
  time_t now = 0;
  uint16_t bootCount = 0;
  int stepMin = 1;               // minutes between wakes (power::Plan::stepMin)
  bool fullRefresh = false;
  bool buttonWake = false;       // woken by the button (EXT1)
  // Refresh interval in wakes of each widget on the layout; 0 = not on the layout.
  int eventRefresh = 0, stocksRefresh = 0, weatherRefresh = 0, trackingRefresh = 0;
  int batteryRefresh = 0, calendarRefresh = 0, bambuRefresh = 0;
  bool hasIcsFeeds = false, hasAerodataboxKey = false, hasMakerworldUid = false;
  bool isPrinting = false, previousIsPrinting = false, pendingBambuRetry = false;
  // Printer status over MQTT needs the printer's address, serial number and access code (the
  // printer widget also works with MakerWorld stats only). After failures: not before this time.
  bool hasPrinterMqtt = true;
  time_t bambuRetryAfter = 0;
  bool printViewOnScreen = false;  // the printer view (not MakerWorld) is on screen
  bool showPrintView = false;      // the printer view should be shown now (printing / just finished)
  time_t makerworldLastFetch = 0, lastNtpSync = 0;
  uint8_t wifiFailures = 0;
  time_t wifiRetryAfter = 0;
  bool otaNightDue = false;        // night-time update check due (OtaFlow)
  // A failed fetch is due for its retry (ServiceState.h); calendar = agenda + month (one download).
  bool weatherRetry = false, stocksRetry = false, calendarRetry = false, trackingRetry = false;
  bool otaPendingVerify = false;   // a new firmware must reach Wi-Fi to confirm itself
  FlightState flight;
};

struct WakePlan {
  // Fetch new data (and redraw).
  bool event = false, stocks = false, weather = false, tracking = false, calendar = false;
  bool bambu = false, makerworld = false, flightStatus = false, ntp = false, otaNight = false;
  bool bambuRetry = false;   // this printer fetch is the retry after a failure
  // Redraw from kept data.
  bool battery = false;      // status bar
  bool eventRedraw = false;  // flight countdown
  bool needWiFi = false;
  bool wifiBackoff = false;  // Wi-Fi skipped: waiting after failures
  long backoffRemaining = 0; // seconds
};

// A refresh interval rounded up to a multiple of the wake interval (the clock every 5 minutes: a
// 4-minute poll becomes 5), so bootCount, which advances in steps of the interval, still hits it.
inline int roundedRefresh(int refresh, int step) {
  if (step <= 1) return refresh;
  return ((refresh + step - 1) / step) * step;
}

inline bool dueByWakes(int refresh, uint16_t bootCount, int step = 1) {
  return refresh > 0 && bootCount % roundedRefresh(refresh, step) == 0;
}

inline WakePlan planWake(const WakeInputs& in) {
  WakePlan p;
  const time_t now = in.now;
  p.event = dueByWakes(in.eventRefresh, in.bootCount, in.stepMin) || (in.eventRefresh > 0 && in.calendarRetry);
  p.stocks = dueByWakes(in.stocksRefresh, in.bootCount, in.stepMin) || (in.stocksRefresh > 0 && in.stocksRetry);
  p.weather = dueByWakes(in.weatherRefresh, in.bootCount, in.stepMin) || (in.weatherRefresh > 0 && in.weatherRetry);
  p.tracking = dueByWakes(in.trackingRefresh, in.bootCount, in.stepMin) || (in.trackingRefresh > 0 && in.trackingRetry);

  // A flight departs within 24 hours: refresh its countdown every 10 minutes (no download).
  const FlightState& f = in.flight;
  bool flightSoon = f.nextFlightStart > now && f.nextFlightStart - now < 86400;

  // Live flight status from 6 hours before departure; stops 1 h after the actual landing, and in
  // any case FLIGHT_LIVE_AFTER_MAX_S after the expected landing (scheduled arrival, else the
  // calendar end), even if "landed" is never reported.
  time_t expectedLanding = f.arrScheduled ? f.arrScheduled : (f.calEnd > f.calStart ? f.calEnd : f.calStart);
  time_t liveLimit = expectedLanding + FLIGHT_LIVE_AFTER_MAX_S;
  if (f.arrActual && f.arrActual + 3600 < liveLimit) liveLimit = f.arrActual + 3600;
  bool liveWindow = f.calStart && now >= f.calStart - FLIGHT_LIVE_BEFORE_S && now < liveLimit;
  bool flightFinished = f.valid && f.finished;
  time_t liveDep = f.depActual ? f.depActual : f.depScheduled ? f.depScheduled : f.calStart;
  time_t liveArr = f.arrActual ? f.arrActual : f.arrScheduled;
  bool nearMovement = (now > liveDep - 3600 && now < liveDep + 1800) ||
                      (liveArr && now > liveArr - 3600 && now < liveArr + 1800);
  time_t liveRefresh = nearMovement ? FLIGHT_LIVE_FAST_S : FLIGHT_LIVE_REFRESH_S;
  p.flightStatus = liveWindow && !flightFinished && f.hasNumber && in.hasAerodataboxKey &&
                   f.requests < FLIGHT_LIVE_MAX_REQUESTS &&
                   (f.lastFetch == 0 || now - f.lastFetch >= liveRefresh);

  bool eventOnLayout = in.eventRefresh > 0;
  p.eventRedraw = eventOnLayout && (flightSoon || liveWindow) && !p.event && dueByWakes(10, in.bootCount, in.stepMin);
  p.battery = dueByWakes(in.batteryRefresh, in.bootCount, in.stepMin);
  p.calendar = dueByWakes(in.calendarRefresh, in.bootCount, in.stepMin) || (in.calendarRefresh > 0 && in.calendarRetry);
  bool bambuOnLayout = in.bambuRefresh > 0;
  bool bambuMqtt = bambuOnLayout && in.hasPrinterMqtt;
  p.bambu = bambuMqtt && ((((in.isPrinting || in.previousIsPrinting) && dueByWakes(in.bambuRefresh, in.bootCount, in.stepMin)) ||
                           dueByWakes(BAMBU_IDLE_REFRESH_MIN, in.bootCount, in.stepMin)) ||
                          in.pendingBambuRetry);
  // A printer that didn't answer (usually: switched off) is asked again only after its backoff.
  if (p.bambu && in.bambuRetryAfter && now < in.bambuRetryAfter) p.bambu = false;
  p.bambuRetry = p.bambu && in.pendingBambuRetry;
  // The "print finished" window just ended: redraw so the MakerWorld view replaces the printer view.
  if (bambuMqtt && in.printViewOnScreen && !in.showPrintView) p.bambu = true;

  // MakerWorld stats: only for the idle view, at most once per MAKERWORLD_REFRESH_S.
  p.makerworld = bambuOnLayout && in.hasMakerworldUid && !in.showPrintView &&
                 (in.makerworldLastFetch == 0 || now - in.makerworldLastFetch >= MAKERWORLD_REFRESH_S);

  // Internet time is re-synced at least once per NTP_RESYNC_S: the RTC drifts during deep sleep.
  p.ntp = in.lastNtpSync == 0 || now - in.lastNtpSync >= NTP_RESYNC_S;
  p.otaNight = in.otaNightDue;

  p.needWiFi = p.event || p.stocks || p.weather || p.tracking || p.bambu || in.fullRefresh ||
               p.flightStatus || (p.calendar && in.hasIcsFeeds) || p.makerworld || p.ntp || p.otaNight ||
               in.otaPendingVerify;  // a new firmware confirms itself only after Wi-Fi worked

  // Wi-Fi failed recently: keep the previous content until the backoff ends (a button wake always
  // tries). A full refresh still redraws everything from the kept data.
  if (p.needWiFi && in.wifiFailures && now < in.wifiRetryAfter && !in.otaPendingVerify && !in.buttonWake) {
    p.wifiBackoff = true;
    p.backoffRemaining = static_cast<long>(in.wifiRetryAfter - now);
    p.needWiFi = false;
    if (!in.fullRefresh) p.weather = p.stocks = p.event = p.calendar = p.tracking = false;
    p.bambu = p.makerworld = p.flightStatus = p.otaNight = false;
  }
  return p;
}

// Wait after the n-th failed Wi-Fi connection in a row: 2, 4, 8, 16, then 30 minutes.
inline time_t wifiBackoffSeconds(uint8_t failures) {
  time_t backoff = failures >= 5 ? WIFI_BACKOFF_MAX_S : (time_t)60 << failures;
  return backoff > WIFI_BACKOFF_MAX_S ? WIFI_BACKOFF_MAX_S : backoff;
}

// Wait after the n-th failed printer connection in a row: 10, 20, 40, then 60 minutes (a printer that
// is switched off shouldn't keep Wi-Fi busy every few minutes).
inline time_t bambuBackoffSeconds(uint8_t failures) {
  if (failures == 0) return 0;
  time_t s = (time_t)600 << (failures > 3 ? 3 : failures - 1);
  return s > 3600 ? 3600 : s;
}

