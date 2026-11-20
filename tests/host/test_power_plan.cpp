// Host test for src/core/PowerPlan.h (clock interval, quiet hours, battery saver). Run: tests/host/run.sh
#include "src/core/PowerPlan.h"
#include "tests/host/check.h"

using namespace power;

static void test_quiet_window() {
  CHECK(inQuietWindow(23 * 60, 23 * 60, 6 * 60));      // start: inside
  CHECK(inQuietWindow(2 * 60, 23 * 60, 6 * 60));       // after midnight
  CHECK(!inQuietWindow(6 * 60, 23 * 60, 6 * 60));      // end: outside
  CHECK(!inQuietWindow(12 * 60, 23 * 60, 6 * 60));
  CHECK(inQuietWindow(13 * 60, 12 * 60, 14 * 60));     // not wrapping
  CHECK(!inQuietWindow(14 * 60, 12 * 60, 14 * 60));
  CHECK(!inQuietWindow(5, 300, 300));                  // from == to: off
  CHECK_EQ(minutesUntil(23 * 60, 6 * 60), 7 * 60);
  CHECK_EQ(minutesUntil(5 * 60 + 59, 6 * 60), 1);
  CHECK_EQ(minutesUntil(360, 360), 1440);
}

static void test_modes() {
  Inputs in;
  in.minuteOfDay = 12 * 60;
  in.batteryPct = 70;
  Plan p = decide(in);
  CHECK(p.mode == Mode::Normal);
  CHECK_EQ(p.stepMin, 1);

  in.minuteOfDay = 23 * 60 + 30;  // night
  p = decide(in);
  CHECK(p.mode == Mode::Night);
  CHECK_EQ(p.nightMinutesLeft, 6 * 60 + 30);
  in.buttonWake = true;  // a tap at night shows the dashboard
  p = decide(in);
  CHECK(p.mode == Mode::Normal);
  CHECK(p.night);
  in.buttonWake = false;
  in.quietOn = false;  // switched off
  CHECK(decide(in).mode == Mode::Normal);
  in.quietOn = true;
  in.minuteOfDay = -1;  // clock not set: never night
  CHECK(decide(in).mode == Mode::Normal);
}

static void test_saver_hysteresis() {
  Inputs in;
  in.minuteOfDay = 12 * 60;
  in.clockInterval = 1;
  in.batteryPct = 19;
  Plan p = decide(in);
  CHECK(p.saverOn);
  CHECK(p.mode == Mode::Saver);
  CHECK_EQ(p.stepMin, 15);
  in.saverWasOn = true;
  in.batteryPct = 25;  // between 20 and 30: stays on
  CHECK(decide(in).saverOn);
  in.batteryPct = 30;  // back to normal
  CHECK(!decide(in).saverOn);
  in.saverWasOn = false;
  in.batteryPct = 25;  // coming from above: stays off
  CHECK(!decide(in).saverOn);
  in.batteryPct = 10;
  in.usb = true;  // a computer's USB: no saver
  CHECK(!decide(in).saverOn);
  in.usb = false;
  in.batteryPct = -1;  // no battery measured (USB power): no saver
  CHECK(!decide(in).saverOn);
  in.batteryPct = 10;
  in.clockInterval = 30;  // a longer chosen interval stays
  CHECK_EQ(decide(in).stepMin, 30);
}

static void test_critical() {
  Inputs in;
  in.minuteOfDay = 12 * 60;
  in.batteryPct = 4;
  CHECK(decide(in).mode == Mode::Critical);
  in.minuteOfDay = 1 * 60;  // also at night
  CHECK(decide(in).mode == Mode::Critical);
  in.criticalWasOn = true;
  in.batteryPct = 8;  // recovering a little without charging: stays (no flapping around 5 %)
  CHECK(decide(in).mode == Mode::Critical);
  in.batteryPct = 10;  // charged: back
  CHECK(decide(in).mode != Mode::Critical);
  in.criticalWasOn = false;
  in.batteryPct = 4;
  in.usb = true;
  CHECK(decide(in).mode != Mode::Critical);
  in.usb = false;
  in.batteryPct = -1;  // not plausible: never critical
  CHECK(decide(in).mode != Mode::Critical);
}

static void test_aligned_sleep() {
  const time_t t = 1795000000;  // a real date; 1795000000 % 300 == 100 s into a 5-minute block
  CHECK_EQ(t % 300, 100);
  CHECK_EQ(alignedSleepMicros(t, 0, 5, 500), (int64_t)(200 * 1000000LL + 500000));
  CHECK_EQ(alignedSleepMicros(t, 0, 1, 500), (int64_t)((60 - t % 60) * 1000000LL + 500000));
  // Exactly on the boundary: the next one, a whole period later.
  time_t on = t - t % 900;
  CHECK_EQ(alignedSleepMicros(on, 0, 15, 0), (int64_t)900 * 1000000LL);
  CHECK_EQ(alignedSleepMicros(1000, 0, 5, 500), (int64_t)60 * 1000000LL);  // no real time
  // Night: until the end of the window.
  CHECK_EQ(nightSleepMicros(t, 0, 420, 500), (int64_t)(420 * 60 - t % 60) * 1000000LL + 500000);
}

static void test_hm_and_notes() {
  int m;
  CHECK(parseHm("23:00", m) && m == 1380);
  CHECK(parseHm("6:05", m) && m == 365);
  CHECK(!parseHm("24:00", m));
  CHECK(!parseHm("12:60", m));
  CHECK(!parseHm("noon", m));
  char hm[8];
  formatHm(365, hm, sizeof(hm));
  CHECK_STR(hm, "06:05");

  char n[48];
  Plan p;
  p.mode = Mode::Night;
  note(p, 1, 360, n, sizeof(n));
  CHECK_STR(n, "Night until 06:00");
  p = Plan();
  p.mode = Mode::Saver;
  p.saverOn = true;
  p.stepMin = 15;
  note(p, 1, 360, n, sizeof(n));
  CHECK_STR(n, "Battery saver: every 15 min");
  p = Plan();
  note(p, 5, 360, n, sizeof(n));
  CHECK_STR(n, "Clock every 5 min");
  note(p, 1, 360, n, sizeof(n));
  CHECK_STR(n, "");
}

static void test_estimate() {
  double q = quietHours(true, 23 * 60, 6 * 60);
  CHECK(q > 6.99 && q < 7.01);
  CHECK(quietHours(false, 23 * 60, 6 * 60) == 0.0);
  double prev = 0;
  for (int c : CLOCK_CHOICES) {  // a longer interval lasts longer
    double d = estimateDays(c, 0.0, 3000);
    CHECK(d > prev);
    prev = d;
  }
  CHECK(estimateDays(1, 7.0, 3000) > estimateDays(1, 0.0, 3000));  // quiet hours help
  CHECK(estimateDays(1, 0.0, 6000) > estimateDays(1, 0.0, 3000) * 1.99);
  // Every minute, no quiet hours, 3000 mAh: about 2 weeks with these costs (sanity of the model).
  double d1 = estimateDays(1, 0.0, 3000);
  CHECK(d1 > 7 && d1 < 30);
}

int main() {
  test_quiet_window();
  test_modes();
  test_saver_hysteresis();
  test_critical();
  test_aligned_sleep();
  test_hm_and_notes();
  test_estimate();
  return finish("power plan");
}
