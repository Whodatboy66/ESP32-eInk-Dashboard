// Host test for src/core/WakePlan.h: the decisions of one wake, as they were in setup() (build 66).
// Run: tests/host/run.sh
#include "src/core/WakePlan.h"
#include "tests/host/check.h"

#include <initializer_list>

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
  // Stage 8: weather, stocks, calendar and parcels are due by the age of their data (kept in sleep
  // memory); all just fetched here.
  for (time_t& t : in.lastOk) t = NOW - 100;
  return in;
}

static void setAge(WakeInputs& in, Service s, int minutes) { in.lastOk[(int)s] = NOW - minutes * 60; }

static void test_quiet_wake_needs_no_wifi() {
  WakePlan p = planWake(base());
  CHECK(!p.needWiFi);
  CHECK(!p.event && !p.stocks && !p.weather && !p.tracking && !p.calendar && !p.bambu);
  CHECK(!p.makerworld && !p.ntp && !p.flightStatus && !p.eventRedraw && !p.battery);
}

// Stage 8: each data type is due when its data is as old as its own interval (defaults: calendar
// 30, weather 60, stocks 30, parcels 120 minutes), not by the wake counter.
static void test_services_due_by_age() {
  WakeInputs in = base();
  setAge(in, Service::Calendar, 31);
  setAge(in, Service::Stocks, 10);
  setAge(in, Service::Weather, 30);
  setAge(in, Service::Tracking, 121);
  WakePlan p = planWake(in);
  CHECK(p.event && p.calendar && p.tracking);  // agenda and month: one calendar download
  CHECK(!p.stocks && !p.weather);
  CHECK(p.needWiFi);
  // In the last quarter of its interval, weather rides along with the Wi-Fi that is on anyway.
  setAge(in, Service::Weather, 46);
  CHECK(planWake(in).weather);
  // Alone (nothing else due) it waits for its own time.
  in = base();
  setAge(in, Service::Weather, 46);
  CHECK(!planWake(in).weather && !planWake(in).needWiFi);
}

// After a reset the store is empty: everything on the layout once, in one Wi-Fi session.
static void test_due_after_reset() {
  WakeInputs in = base();
  for (time_t& t : in.lastOk) t = 0;
  WakePlan p = planWake(in);
  CHECK(p.event && p.calendar && p.stocks && p.weather && p.tracking && p.needWiFi);
}

// A tap (one screen), a layout change from the app, the morning after the quiet hours: everything
// fresh ("forceFetch", what bootCount = 0 meant before). The daily full redraw alone draws from the
// store and downloads nothing.
static void test_force_fetch_and_daily_full_redraw() {
  WakeInputs in = base();
  in.forceFetch = true;
  WakePlan p = planWake(in);
  CHECK(p.event && p.calendar && p.stocks && p.weather && p.tracking && p.needWiFi);
  in = base();
  in.fullRefresh = true;
  in.bootCount = 1440;
  in.bambuRefresh = 0;  // the printer has its own 10-minute poll (1440 is one of them)
  p = planWake(in);
  CHECK(!p.event && !p.calendar && !p.stocks && !p.weather && !p.tracking);
  CHECK(!p.needWiFi);
}

// Stage 8: calendar links with their own interval ("every 3 hours", "mornings only") bring the
// calendar download by themselves; the calendar's interval counts only with a link that follows it.
static void test_calendar_links_own_interval() {
  WakeInputs in = base();
  in.calendarOwnDue = true;  // e.g. the work calendar's morning read
  WakePlan p = planWake(in);
  CHECK(p.event && p.calendar && p.needWiFi);
  in = base();
  setAge(in, Service::Calendar, 31);
  in.calendarServiceLinks = false;  // every link has its own interval, none due
  p = planWake(in);
  CHECK(!p.event && !p.calendar);
}

// Stage 8 (B5): a switched-off printer is asked again after its backoff; a tap asks at once (the owner
// switched it on and wants its box back).
static void test_bambu_tap_ignores_backoff() {
  WakeInputs in = base();
  in.bootCount = 0;
  in.bambuRetryAfter = NOW + 1800;
  CHECK(!planWake(in).bambu);
  in.buttonWake = true;
  CHECK(planWake(in).bambu);
}

// Without a real clock yet everything counts as due.
static void test_due_without_clock() {
  WakeInputs in = base();
  in.now = 1000;
  in.lastNtpSync = 0;
  WakePlan p = planWake(in);
  CHECK(p.weather && p.stocks && p.calendar && p.needWiFi);
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
  setAge(in, Service::Calendar, 60);
  in.eventRefresh = 0;     // only the month view
  in.stocksRefresh = in.trackingRefresh = in.bambuRefresh = 0;
  in.hasIcsFeeds = false;
  WakePlan p = planWake(in);
  CHECK(p.calendar);       // the flag is set (the month view redraws) ...
  CHECK(!p.needWiFi);      // ... but without feeds nothing is downloaded
  in.hasIcsFeeds = true;
  CHECK(planWake(in).needWiFi);
}

static void test_forced_fetch_brings_wifi() {
  WakeInputs in = base();
  in.forceFetch = true;
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

static void test_bambu_needs_mqtt_settings_and_backs_off() {
  WakeInputs in = base();
  in.bootCount = 120;  // an idle poll is due
  in.hasPrinterMqtt = false;  // MakerWorld only: no printer connection at all
  CHECK(!planWake(in).bambu);
  in.pendingBambuRetry = true;
  CHECK(!planWake(in).bambu);
  in.hasPrinterMqtt = true;
  in.pendingBambuRetry = false;
  CHECK(planWake(in).bambu);
  in.bambuRetryAfter = in.now + 600;  // didn't answer: wait
  CHECK(!planWake(in).bambu);
  in.pendingBambuRetry = true;
  CHECK(!planWake(in).bambu);  // not even the retry
  in.now += 601;
  CHECK(planWake(in).bambu);
  CHECK_EQ(bambuBackoffSeconds(1), 600);
  CHECK_EQ(bambuBackoffSeconds(2), 1200);
  CHECK_EQ(bambuBackoffSeconds(3), 2400);
  CHECK_EQ(bambuBackoffSeconds(4), 3600);
  CHECK_EQ(bambuBackoffSeconds(200), 3600);
  CHECK_EQ(bambuBackoffSeconds(0), 0);
}

static void test_slower_clock_keeps_hourly_fetches() {
  // A 5-minute and a 15-minute clock: bootCount advances by the interval; hourly things still come
  // once per hour (at the same minute), the 4-minute printer poll becomes 5 / 15 minutes.
  CHECK_EQ(roundedRefresh(4, 5), 5);
  CHECK_EQ(roundedRefresh(60, 15), 60);
  CHECK_EQ(roundedRefresh(10, 15), 15);
  CHECK_EQ(roundedRefresh(7, 1), 7);
  // By age: an hourly calendar stays hourly with a 5- and a 15-minute clock (the download recorded
  // 20 s into its wake); a 15-minute interval with a 30-minute clock comes every 30 minutes.
  const struct { int step, interval, perDay; } cases[] = {{5, 60, 24}, {15, 60, 24}, {30, 15, 48}, {1, 30, 48}};
  for (const auto& c : cases) {
    WakeInputs in = base();
    in.stepMin = c.step;
    in.intervalMin[(int)Service::Calendar] = c.interval;
    in.lastOk[(int)Service::Calendar] = NOW + 20;
    int calendarFetches = 0, wakes = 0;
    for (int minute = c.step; minute <= 24 * 60; minute += c.step) {
      in.now = NOW + minute * 60;
      in.lastNtpSync = in.now;
      in.makerworldLastFetch = in.now;
      for (int s = 0; s < SERVICE_COUNT; s++)
        if (s != (int)Service::Calendar) in.lastOk[s] = in.now;
      WakePlan p = planWake(in);
      if (p.calendar) {
        calendarFetches++;
        in.lastOk[(int)Service::Calendar] = in.now + 20;
      }
      wakes++;
    }
    CHECK_EQ(wakes, 24 * 60 / c.step);
    if (calendarFetches != c.perDay) std::printf("  step %d, interval %d: %d fetches\n", c.step, c.interval, calendarFetches);
    CHECK_EQ(calendarFetches, c.perDay);
  }
  WakeInputs in = base();
  in.stepMin = 5;
  in.bambuRefresh = 4;
  in.isPrinting = true;
  in.bootCount = 5;
  CHECK(planWake(in).bambu);  // every 5 minutes while printing (4 rounded up)
  in.bootCount = 10;
  CHECK(planWake(in).bambu);
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
  for (time_t& t : in.lastOk) t = NOW - 7200;
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

static void test_backoff_fetches_nothing() {
  WakeInputs in = base();
  in.bootCount = 0;
  in.fullRefresh = true;
  in.forceFetch = true;
  in.wifiFailures = 1;
  in.wifiRetryAfter = NOW + 50;
  in.buttonWake = false;
  WakePlan p = planWake(in);
  CHECK(!p.needWiFi);
  // The flags say what to download; the full redraw draws every box from the store (App.cpp).
  CHECK(!p.event && !p.weather && !p.stocks && !p.calendar && !p.tracking);
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
  in.bootCount = 60;
  setAge(in, Service::Calendar, 31);  // the calendar is fetched anyway: no extra redraw
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


int main() {
  test_quiet_wake_needs_no_wifi();
  test_services_due_by_age();
  test_due_after_reset();
  test_force_fetch_and_daily_full_redraw();
  test_due_without_clock();
  test_calendar_links_own_interval();
  test_bambu_tap_ignores_backoff();
  test_widget_not_on_layout_is_never_fetched();
  test_calendar_needs_feeds_for_wifi();
  test_forced_fetch_brings_wifi();
  test_ntp_resync_hourly();
  test_makerworld_hourly_only_in_idle_view();
  test_bambu_polling();
  test_bambu_redraw_when_print_view_ends();
  test_bambu_needs_mqtt_settings_and_backs_off();
  test_slower_clock_keeps_hourly_fetches();
  test_wifi_backoff();
  test_backoff_fetches_nothing();
  test_retry_after_a_failed_fetch();
  test_wifi_backoff_sequence();
  test_night_update_check_brings_wifi();
  test_flight_countdown_redraw_every_10_wakes();
  test_flight_live_status_window();
  test_flight_fast_refresh_near_departure();
  test_flight_stops();
  return finish("wake plan");
}
