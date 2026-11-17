// Host test for src/core/WakePlan.h: the decisions of one wake, as they were in setup() (build 66).
// Run: tests/host/run.sh
#include "src/core/WakePlan.h"
#include "tests/host/check.h"

static const time_t NOW = 1789000000;  // 2026-09-10, a real time

// A typical wake: every widget on the layout (default refresh intervals), nothing due by time.
static WakeInputs base() {
  WakeInputs in{};
  in.now = NOW;
  in.bootCount = 7;              // not a multiple of any refresh interval
  in.eventRefresh = 60;
  in.stocksRefresh = 60;
  in.weatherRefresh = 360;
  in.trackingRefresh = 120;
  in.batteryRefresh = 60;
  in.calendarRefresh = 60;
  in.bambuRefresh = 4;
  in.hasIcsFeeds = true;
  in.hasAerodataboxKey = true;
  in.hasMakerworldUid = true;
  in.makerworldLastFetch = NOW - 100;
  in.lastNtpSync = NOW - 100;
  return in;
}

static void test_quiet_wake_needs_no_wifi() {
  WakePlan p = planWake(base());
  CHECK(!p.needWiFi);
  CHECK(!p.event && !p.stocks && !p.weather && !p.tracking && !p.calendar && !p.bambu);
  CHECK(!p.makerworld && !p.ntp && !p.flightStatus && !p.eventRedraw && !p.battery);
}

static void test_refresh_intervals_count_wakes() {
  WakeInputs in = base();
  in.bootCount = 120;  // multiple of 60 and 120 (and 4, 10), not of 360
  WakePlan p = planWake(in);
  CHECK(p.event && p.stocks && p.tracking && p.battery && p.calendar);
  CHECK(!p.weather);
  CHECK(p.bambu);      // 120 % 10 == 0: idle printer poll
  CHECK(p.needWiFi);
  in.bootCount = 360;
  CHECK(planWake(in).weather);
}

static void test_widget_not_on_layout_is_never_fetched() {
  WakeInputs in = base();
  in.bootCount = 0;
  in.stocksRefresh = 0;   // no stock symbols: no stocks widget
  in.bambuRefresh = 0;    // no printer
  WakePlan p = planWake(in);
  CHECK(!p.stocks);
  CHECK(!p.bambu);
  CHECK(!p.makerworld);   // MakerWorld lives in the printer area
}

static void test_calendar_needs_feeds_for_wifi() {
  WakeInputs in = base();
  in.bootCount = 60;
  in.eventRefresh = 0;     // only the month view
  in.stocksRefresh = in.trackingRefresh = in.bambuRefresh = 0;
  in.hasIcsFeeds = false;
  WakePlan p = planWake(in);
  CHECK(p.calendar);       // the flag is set (the month view redraws) ...
  CHECK(!p.needWiFi);      // ... but without feeds nothing is downloaded
  in.hasIcsFeeds = true;
  CHECK(planWake(in).needWiFi);
}

static void test_full_refresh_always_brings_wifi() {
  WakeInputs in = base();
  in.fullRefresh = true;
  CHECK(planWake(in).needWiFi);
}

static void test_ntp_resync_hourly() {
  WakeInputs in = base();
  in.lastNtpSync = 0;
  CHECK(planWake(in).ntp);
  in.lastNtpSync = NOW - 3599;
  CHECK(!planWake(in).ntp);
  in.lastNtpSync = NOW - 3600;
  WakePlan p = planWake(in);
  CHECK(p.ntp && p.needWiFi);
}

static void test_makerworld_hourly_only_in_idle_view() {
  WakeInputs in = base();
  in.makerworldLastFetch = 0;
  CHECK(planWake(in).makerworld);
  in.makerworldLastFetch = NOW - 3600;
  CHECK(planWake(in).makerworld);
  in.showPrintView = true;  // printing: the printer view is shown, not MakerWorld
  CHECK(!planWake(in).makerworld);
  in.showPrintView = false;
  in.hasMakerworldUid = false;
  CHECK(!planWake(in).makerworld);
}

static void test_bambu_polling() {
  WakeInputs in = base();
  in.isPrinting = true;
  in.bootCount = 8;          // multiple of the 4-minute printing interval, not of 10
  CHECK(planWake(in).bambu);
  in.isPrinting = false;
  in.previousIsPrinting = true;  // just finished: still polled at the printing interval
  CHECK(planWake(in).bambu);
  in.previousIsPrinting = false;
  CHECK(!planWake(in).bambu);    // idle: only every 10 wakes
  in.pendingBambuRetry = true;
  WakePlan p = planWake(in);
  CHECK(p.bambu && p.bambuRetry);
}

static void test_bambu_redraw_when_print_view_ends() {
  WakeInputs in = base();
  in.printViewOnScreen = true;
  in.showPrintView = false;  // the 5 minutes after the print are over
  CHECK(planWake(in).bambu);
}

static void test_wifi_backoff() {
  WakeInputs in = base();
  in.bootCount = 120;
  in.wifiFailures = 2;
  in.wifiRetryAfter = NOW + 100;
  WakePlan p = planWake(in);
  CHECK(!p.needWiFi);
  CHECK(p.wifiBackoff);
  CHECK_EQ(p.backoffRemaining, 100);
  CHECK(!p.event && !p.stocks && !p.calendar && !p.tracking && !p.bambu);
  CHECK(p.battery);  // the status bar still redraws (no download)

  in.buttonWake = true;  // a button wake always tries
  CHECK(planWake(in).needWiFi);
  in.buttonWake = false;
  in.otaPendingVerify = true;  // a new firmware must reach Wi-Fi to confirm itself
  CHECK(planWake(in).needWiFi);
  in.otaPendingVerify = false;
  in.wifiRetryAfter = NOW;     // backoff over
  CHECK(planWake(in).needWiFi);
}

static void test_backoff_keeps_full_refresh_widgets() {
  WakeInputs in = base();
  in.bootCount = 0;
  in.fullRefresh = true;
  in.wifiFailures = 1;
  in.wifiRetryAfter = NOW + 50;
  WakePlan p = planWake(in);
  CHECK(!p.needWiFi);
  // A full refresh redraws everything from the kept data, so the flags stay.
  CHECK(p.event && p.weather && p.stocks && p.calendar && p.tracking);
  CHECK(!p.bambu && !p.makerworld && !p.flightStatus);
}

static void test_retry_after_a_failed_fetch() {
  WakeInputs in = base();  // nothing due by the wake counter
  in.weatherRetry = true;
  WakePlan p = planWake(in);
  CHECK(p.weather && p.needWiFi);
  CHECK(!p.stocks && !p.calendar && !p.tracking);
  in.weatherRetry = false;
  in.calendarRetry = true;   // agenda and month share one download
  p = planWake(in);
  CHECK(p.event && p.calendar && p.needWiFi);
  in.calendarRetry = false;
  in.stocksRetry = in.trackingRetry = true;
  p = planWake(in);
  CHECK(p.stocks && p.tracking);
  // Not on the layout: no retry either.
  in.stocksRefresh = 0;
  CHECK(!planWake(in).stocks);
  // Wi-Fi backoff wins: the retry waits too.
  in = base();
  in.weatherRetry = true;
  in.wifiFailures = 1;
  in.wifiRetryAfter = NOW + 60;
  CHECK(!planWake(in).weather);
}

static void test_wifi_backoff_sequence() {
  CHECK_EQ(wifiBackoffSeconds(1), 120);
  CHECK_EQ(wifiBackoffSeconds(2), 240);
  CHECK_EQ(wifiBackoffSeconds(3), 480);
  CHECK_EQ(wifiBackoffSeconds(4), 960);
  CHECK_EQ(wifiBackoffSeconds(5), 1800);
  CHECK_EQ(wifiBackoffSeconds(200), 1800);
}

static void test_night_update_check_brings_wifi() {
  WakeInputs in = base();
  in.otaNightDue = true;
  WakePlan p = planWake(in);
  CHECK(p.otaNight && p.needWiFi);
}

// ---- Flights ----

static WakeInputs flightBase() {
  WakeInputs in = base();
  in.flight.nextFlightStart = NOW + 2 * 3600;  // departs in 2 h
  in.flight.calStart = NOW + 2 * 3600;
  in.flight.calEnd = NOW + 5 * 3600;
  in.flight.hasNumber = true;
  return in;
}

static void test_flight_countdown_redraw_every_10_wakes() {
  WakeInputs in = flightBase();
  in.bootCount = 30;  // multiple of 10, not of 60
  CHECK(planWake(in).eventRedraw);
  in.bootCount = 31;
  CHECK(!planWake(in).eventRedraw);
  in.bootCount = 60;  // the calendar is fetched anyway: no extra redraw
  CHECK(!planWake(in).eventRedraw);
}

static void test_flight_live_status_window() {
  WakeInputs in = flightBase();
  WakePlan p = planWake(in);
  CHECK(p.flightStatus && p.needWiFi);  // never fetched yet
  in.flight.lastFetch = NOW - 600;      // 10 min ago, departure in 2 h: normal 30 min interval
  CHECK(!planWake(in).flightStatus);
  in.flight.lastFetch = NOW - 1800;
  CHECK(planWake(in).flightStatus);

  in.flight.calStart = NOW + 7 * 3600;  // more than 6 h ahead: not followed yet
  in.flight.lastFetch = 0;
  CHECK(!planWake(in).flightStatus);
}

static void test_flight_fast_refresh_near_departure() {
  WakeInputs in = flightBase();
  in.flight.calStart = NOW + 30 * 60;  // departure in 30 min
  in.flight.lastFetch = NOW - 600;     // 10 min ago: due at the fast interval
  CHECK(planWake(in).flightStatus);
  in.flight.lastFetch = NOW - 599;
  CHECK(!planWake(in).flightStatus);
}

static void test_flight_stops() {
  WakeInputs in = flightBase();
  in.flight.valid = true;
  in.flight.finished = true;  // Arrived / Canceled / Diverted
  CHECK(!planWake(in).flightStatus);

  in = flightBase();
  in.flight.requests = 30;    // safety cap
  CHECK(!planWake(in).flightStatus);

  in = flightBase();
  in.hasAerodataboxKey = false;
  CHECK(!planWake(in).flightStatus);

  in = flightBase();
  in.flight.hasNumber = false;
  CHECK(!planWake(in).flightStatus);

  // Landed 1 h ago: the live window ends (actual landing + 1 h).
  in = flightBase();
  in.flight.calStart = NOW - 5 * 3600;
  in.flight.calEnd = NOW - 2 * 3600;
  in.flight.arrActual = NOW - 3600;
  CHECK(!planWake(in).flightStatus);
  in.flight.arrActual = NOW - 3599;
  CHECK(planWake(in).flightStatus);

  // Never reported as landed: at most 3 h after the expected landing (calendar end).
  in = flightBase();
  in.flight.calStart = NOW - 8 * 3600;
  in.flight.calEnd = NOW - 3 * 3600;
  CHECK(!planWake(in).flightStatus);
  in.flight.calEnd = NOW - 3 * 3600 + 1;
  CHECK(planWake(in).flightStatus);
}

static void test_sleep_until_next_minute() {
  // 12:00:20.250 -> wake at 12:01:00.500
  CHECK_EQ(sleepMicros(NOW - (NOW % 60) + 20, 250000), (60 - 20) * 1000000LL - 250000 + 500000);
  // No real time yet: 60 s.
  CHECK_EQ(sleepMicros(1000, 0), 60 * 1000000LL);
}

int main() {
  test_quiet_wake_needs_no_wifi();
  test_refresh_intervals_count_wakes();
  test_widget_not_on_layout_is_never_fetched();
  test_calendar_needs_feeds_for_wifi();
  test_full_refresh_always_brings_wifi();
  test_ntp_resync_hourly();
  test_makerworld_hourly_only_in_idle_view();
  test_bambu_polling();
  test_bambu_redraw_when_print_view_ends();
  test_wifi_backoff();
  test_backoff_keeps_full_refresh_widgets();
  test_retry_after_a_failed_fetch();
  test_wifi_backoff_sequence();
  test_night_update_check_brings_wifi();
  test_flight_countdown_redraw_every_10_wakes();
  test_flight_live_status_window();
  test_flight_fast_refresh_near_departure();
  test_flight_stops();
  test_sleep_until_next_minute();
  return finish("wake plan");
}
