// Host test for src/hardware/BatteryCurve.h. Run: tests/host/run.sh
#include "src/hardware/BatteryCurve.h"
#include "tests/host/check.h"

int main() {
  CHECK_EQ(batteryPercent(4.20f), 100);   // above the top point
  CHECK_EQ(batteryPercent(4.15f), 100);
  CHECK_EQ(batteryPercent(3.77f), 30);    // the update battery guard threshold
  CHECK_EQ(batteryPercent(3.785f), 35);   // halfway between 3.77 (30) and 3.80 (40)
  CHECK_EQ(batteryPercent(3.30f), 0);
  CHECK_EQ(batteryPercent(3.00f), 0);     // below the bottom point
  CHECK_EQ(batteryPercent(0.0f), 0);      // no battery / not measured
  // Never goes down while the voltage goes up.
  int last = 0;
  for (int mv = 3000; mv <= 4300; mv += 5) {
    int pct = batteryPercent(mv / 1000.0f);
    CHECK(pct >= last);
    last = pct;
  }
  // No battery measured (nothing connected, a pin that can't measure it): the board runs on USB,
  // so the update battery guard must not refuse (it reported "battery too low" on USB power).
  CHECK(!batteryReadingPlausible(0.0f));
  CHECK(!batteryReadingPlausible(0.4f));
  CHECK(!batteryReadingPlausible(2.9f));
  CHECK(!batteryReadingPlausible(6.6f));   // floating input, x2 divider
  CHECK(batteryReadingPlausible(3.0f));
  CHECK(batteryReadingPlausible(3.7f));
  CHECK(batteryReadingPlausible(4.25f));   // charging
  // The screen's reading: with the radio on the voltage sags ~0.05 V (device test 2026-11-25: the
  // exact preview in edit mode showed 25 %, the dashboard a minute later 41 %), so the last reading
  // with the radio off is used and the sagged one isn't kept as "quiet".
  BatteryShown s = batteryForScreen(false, false, 0.0f, 3.80f);
  CHECK(s.volts == 3.80f && s.keepAsQuiet);   // radio off: live, and kept
  s = batteryForScreen(true, true, 3.80f, 3.75f);
  CHECK(s.volts == 3.80f && !s.keepAsQuiet);  // radio on: the quiet one
  s = batteryForScreen(true, false, 0.0f, 3.75f);
  CHECK(s.volts == 3.75f && !s.keepAsQuiet);  // radio on, no quiet reading yet: live, not kept
  return finish("battery curve");
}
