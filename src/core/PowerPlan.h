#pragma once
// Power decisions of one wake (stage 6, docs/plans/STAGE6_POWER.md): the clock interval, night quiet
// hours, the automatic battery saver and the critical level, when to wake next, the note for the
// status bar, and the battery life estimate shown in the setup app. Plain C++, tested on the PC
// (tests/host/test_power_plan.cpp).
#include <cstdint>
#include <cstdio>
#include <ctime>

#include "../i18n/Strings.h"

namespace power {

constexpr int CLOCK_CHOICES[] = {1, 2, 5, 10, 15, 30};  // minutes; all divide 60 and 1440
constexpr int CLOCK_CHOICE_COUNT = sizeof(CLOCK_CHOICES) / sizeof(CLOCK_CHOICES[0]);
constexpr int SAVER_INTERVAL_MIN = 15;
constexpr int SAVER_ON_PCT = 20;    // below: battery saver
constexpr int SAVER_OFF_PCT = 30;   // above (or on a computer's USB): back to normal
constexpr int CRITICAL_PCT = 5;     // below: "please charge" screen, a battery check once an hour
constexpr int CRITICAL_OFF_PCT = 10;  // above: the dashboard comes back (no flapping around 5 %)
constexpr int CRITICAL_CHECK_MIN = 60;
constexpr int DEFAULT_QUIET_FROM = 23 * 60;
constexpr int DEFAULT_QUIET_TO = 6 * 60;
constexpr int DEFAULT_BATTERY_MAH = 3000;

inline bool validClockInterval(int m) {
  for (int c : CLOCK_CHOICES)
    if (c == m) return true;
  return false;
}

// Is this minute of the day inside the quiet window [from, to)? It may wrap past midnight;
// from == to means no window.
inline bool inQuietWindow(int minuteOfDay, int from, int to) {
  if (from == to) return false;
  if (from < to) return minuteOfDay >= from && minuteOfDay < to;
  return minuteOfDay >= from || minuteOfDay < to;
}

// Minutes from minuteOfDay until target (0-1439), 1-1440 (target == now: a whole day).
inline int minutesUntil(int minuteOfDay, int target) {
  int d = (target - minuteOfDay + 1440) % 1440;
  return d == 0 ? 1440 : d;
}

enum class Mode : uint8_t { Normal, Night, Saver, Critical };

struct Inputs {
  int minuteOfDay = 0;        // local time (-1: clock not set)
  int clockInterval = 1;      // chosen, minutes
  bool quietOn = true;
  int quietFrom = DEFAULT_QUIET_FROM, quietTo = DEFAULT_QUIET_TO;
  int batteryPct = -1;        // -1: no battery measured (USB power)
  bool usb = false;           // a computer's USB is seen
  bool saverWasOn = false;    // last wake (RTC): hysteresis
  bool criticalWasOn = false; // the "please charge" screen is shown (RTC): hysteresis
  bool buttonWake = false;
};

struct Plan {
  Mode mode = Mode::Normal;
  int stepMin = 1;            // wake interval while awake hours
  bool saverOn = false;       // to remember for the next wake
  bool night = false;         // inside the quiet window (and not woken by the button)
  int nightMinutesLeft = 0;   // until the window ends
};

inline Plan decide(const Inputs& in) {
  Plan p;
  int interval = validClockInterval(in.clockInterval) ? in.clockInterval : 1;
  bool battery = in.batteryPct >= 0 && !in.usb;  // running on a battery we can measure
  // Saver with hysteresis: on below 20 %, off above 30 % (or USB, or no battery reading).
  p.saverOn = battery && (in.batteryPct < SAVER_ON_PCT || (in.saverWasOn && in.batteryPct < SAVER_OFF_PCT));
  p.stepMin = p.saverOn && interval < SAVER_INTERVAL_MIN ? SAVER_INTERVAL_MIN : interval;
  if (battery && (in.batteryPct < CRITICAL_PCT || (in.criticalWasOn && in.batteryPct < CRITICAL_OFF_PCT))) {
    p.mode = Mode::Critical;
    return p;
  }
  if (in.minuteOfDay >= 0 && in.quietOn && inQuietWindow(in.minuteOfDay, in.quietFrom, in.quietTo)) {
    p.night = true;
    p.nightMinutesLeft = minutesUntil(in.minuteOfDay, in.quietTo);
    if (!in.buttonWake) {
      p.mode = Mode::Night;
      return p;
    }
  }
  p.mode = p.saverOn ? Mode::Saver : Mode::Normal;
  return p;
}

// Deep sleep until wakeAfterMs after the next wall-clock boundary of `step` minutes (every 5 min:
// :00, :05, ...). Without real time: 60 s.
inline int64_t alignedSleepMicros(time_t sec, long usec, int step, uint32_t wakeAfterMs) {
  if (sec <= 1700000000) return 60LL * 1000000LL;
  if (step < 1) step = 1;
  int64_t periodUs = (int64_t)step * 60 * 1000000LL;
  int64_t intoUs = (int64_t)(sec % (step * 60)) * 1000000LL + usec;
  return periodUs - intoUs + (int64_t)wakeAfterMs * 1000LL;
}

// Deep sleep until wakeAfterMs after the end of the night window (minutes left from now).
inline int64_t nightSleepMicros(time_t sec, long usec, int minutesLeft, uint32_t wakeAfterMs) {
  int64_t intoMinuteUs = (int64_t)(sec % 60) * 1000000LL + usec;
  return (int64_t)minutesLeft * 60 * 1000000LL - intoMinuteUs + (int64_t)wakeAfterMs * 1000LL;
}

// "23:00" <-> minutes of the day.
inline void formatHm(int minutes, char* out, size_t cap) {
  std::snprintf(out, cap, "%02d:%02d", (minutes / 60) % 24, minutes % 60);
}
inline bool parseHm(const char* s, int& minutes) {
  int h, m;
  char tail;
  if (!s || std::sscanf(s, "%d:%d%c", &h, &m, &tail) != 2 || h < 0 || h > 23 || m < 0 || m > 59) return false;
  minutes = h * 60 + m;
  return true;
}

// The note in the status bar: "Nacht bis 06:00" / "Night until 06:00", "Akku-Sparen: alle 15 Min." /
// "Battery saver: every 15 min", "Uhr alle 5 Min." / "Clock every 5 min"; empty when nothing special.
inline void note(const Plan& p, int chosenInterval, int quietTo, char* out, size_t cap, i18n::Lang lang) {
  if (cap) out[0] = '\0';
  char hm[8];
  if (p.mode == Mode::Night) {
    formatHm(quietTo, hm, sizeof(hm));
    std::snprintf(out, cap, i18n::text(i18n::S::NoteNight, lang), hm);
  } else if (p.saverOn) {
    std::snprintf(out, cap, i18n::text(i18n::S::NoteSaver, lang), p.stepMin);
  } else if (chosenInterval > 1 && validClockInterval(chosenInterval)) {
    std::snprintf(out, cap, i18n::text(i18n::S::NoteClock, lang), chosenInterval);
  }
}

// ---- Battery life estimate (to be replaced by measured values, stage 6 measurements) ----
// Calibrated to the only real number so far (diagnostics log of 2026-11-20, FireBeetle C6 + 7.5"
// HAT, every minute, no quiet hours, 3000 mAh): about 3 % in 13 hours, i.e. about 20 days per charge.
// The split between the parts is a guess until the current measurements of stage 6.
struct CostModel {
  double clockWakeMAs = 250.0;     // one clock wake + partial refresh: about 3.5 s at 70 mA
  double wifiHourMAs = 3500.0;     // the hourly Wi-Fi wake: about 27 s at 130 mA
  double sleepMA = 0.1;            // deep sleep: board, display off, touch sensor
  double usable = 0.85;            // usable share of the capacity
};

// Quiet hours per day (0 without a window).
inline double quietHours(bool on, int from, int to) {
  if (!on || from == to) return 0.0;
  return ((to - from + 1440) % 1440) / 60.0;
}

// Estimated days on one charge with this clock interval.
inline double estimateDays(int intervalMin, double quietH, int capacityMah, const CostModel& c = CostModel()) {
  if (intervalMin < 1) intervalMin = 1;
  double activeH = 24.0 - quietH;
  double perActiveHour = (60.0 / intervalMin) * c.clockWakeMAs + c.wifiHourMAs;
  double perDayMAs = activeH * perActiveHour + 24.0 * 3600.0 * c.sleepMA;
  double capacityMAs = capacityMah * 3600.0 * c.usable;
  return capacityMAs / perDayMAs;
}

}  // namespace power
