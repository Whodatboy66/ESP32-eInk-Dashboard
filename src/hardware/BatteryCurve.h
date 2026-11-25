#pragma once
// Battery charge from the cell voltage (typical single-cell LiPo, resting). Plain C++.
#include <stdint.h>

// Typical single-cell LiPo resting voltage -> charge, highest first.
struct BatPoint { float v; uint8_t pct; };
inline constexpr BatPoint BAT_CURVE[] = {
  {4.15f, 100}, {4.10f, 95}, {4.05f, 90}, {3.98f, 80}, {3.92f, 70}, {3.87f, 60},
  {3.83f, 50}, {3.80f, 40}, {3.77f, 30}, {3.73f, 20}, {3.68f, 10}, {3.50f, 5}, {3.30f, 0},
};

// Charge (0-100) from the battery voltage, interpolated on BAT_CURVE.
inline int batteryPercent(float v) {
  const int n = sizeof(BAT_CURVE) / sizeof(BAT_CURVE[0]);
  if (v >= BAT_CURVE[0].v) return 100;
  for (int i = 1; i < n; i++) {
    if (v >= BAT_CURVE[i].v) {
      const BatPoint& hi = BAT_CURVE[i - 1];
      const BatPoint& lo = BAT_CURVE[i];
      return lo.pct + (int)((v - lo.v) / (hi.v - lo.v) * (hi.pct - lo.pct) + 0.5f);
    }
  }
  return 0;
}

// A LiPo that runs the board reads 3.0-4.3 V. Anything else means no battery is connected (or the
// pin can't measure it: wiring, a different board), so the board runs on USB power. Below 3.0 V
// the board couldn't be running from the battery at all.
constexpr float BATTERY_MIN_RUNNING_V = 3.0f;
constexpr float BATTERY_MAX_V = 4.4f;
inline bool batteryReadingPlausible(float v) { return v >= BATTERY_MIN_RUNNING_V && v <= BATTERY_MAX_V; }

// The voltage the screen shows. With the radio on (Wi-Fi, hotspot) it sags about 0.05 V, which is
// over 15 % on the flat part of the curve: then the last reading with the radio off is shown, and
// only readings with the radio off are kept as that "quiet" reading (BatteryMonitor.cpp).
struct BatteryShown {
  float volts;
  bool keepAsQuiet;
};
inline BatteryShown batteryForScreen(bool radioOn, bool haveQuiet, float quietVolts, float liveVolts) {
  if (!radioOn) return {liveVolts, true};
  return {haveQuiet ? quietVolts : liveVolts, false};
}

