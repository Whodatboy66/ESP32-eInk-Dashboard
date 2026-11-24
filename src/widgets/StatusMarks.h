#pragma once
// Status bar rules (stage 7, docs/plans/STAGE7_NEW_LOOK.md task 11). Plain C++, tested on the PC
// (tests/host/test_layout.cpp, tests/host/test_i18n.cpp).

namespace statusbar {

// Bars (1-4) for the Wi-Fi signal of the last connection; 0 = unknown, or the last connection failed
// (the problem mark "! WLAN" is shown then, full bars next to it would contradict it).
inline int signalBars(int rssi, bool wifiProblem = false) {
  if (rssi == 0 || wifiProblem) return 0;
  if (rssi >= -55) return 4;
  if (rssi >= -65) return 3;
  if (rssi >= -75) return 2;
  return 1;
}

constexpr int MIN_HOLIDAY_W = 60;  // less than this: the name is left out (the agenda and month show it)

// The width left for "· Heilige Drei Könige" after the date (ending at dateEnd), before the part on
// the right (starting at rightStart), when the power note (powerNoteW, 0 = none) is shown too: the
// note wins, the name is shortened.
inline int holidaySpace(int dateEnd, int powerNoteW, int rightStart) {
  int w = rightStart - 8 - dateEnd - (powerNoteW ? powerNoteW + 12 : 0);
  return w < MIN_HOLIDAY_W ? 0 : w;
}

}  // namespace statusbar
