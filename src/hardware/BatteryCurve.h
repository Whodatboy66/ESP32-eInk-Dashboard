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

