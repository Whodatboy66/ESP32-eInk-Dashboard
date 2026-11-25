#pragma once
// The data store (stage 8, docs/plans/STAGE8_DATA_AND_SCREENS.md part A): every service's last good
// result stays in sleep memory (RTC), so a screen draws without a download and a service downloads
// only when its own interval is due. The data itself: DataStore.cpp (weather, stocks, printer) and
// the services' own RTC variables (calendar, parcels, flight). Plain C++ here, tested on the PC
// (tests/host/test_data_store.cpp).
#include <cstddef>
#include <cstdint>
#include <ctime>

namespace datastore {

// The interval choices (minutes) for the data types; each divides a day.
constexpr uint16_t INTERVAL_CHOICES[] = {15, 30, 60, 120, 180, 360};
// Defaults (spec): calendar 30, weather 60, stocks 30, parcels 2 h.
constexpr uint16_t DEFAULT_CALENDAR_MIN = 30, DEFAULT_WEATHER_MIN = 60, DEFAULT_STOCKS_MIN = 30,
                   DEFAULT_TRACKING_MIN = 120;
constexpr time_t REAL_TIME = 1700000000;  // before this the clock isn't set yet

inline bool validInterval(int minutes) {
  for (uint16_t c : INTERVAL_CHOICES)
    if (c == minutes) return true;
  return false;
}

// A stored interval, or the default when it isn't one of the choices (NVS written by an older build).
inline int intervalOr(int stored, int fallback) { return validInterval(stored) ? stored : fallback; }

// "The clock interval is the upper bound for everything": a data type's interval rounded up to a
// multiple of the clock interval (stepMin), never shorter than it.
inline int effectiveInterval(int chosenMin, int stepMin) {
  if (stepMin < 1) stepMin = 1;
  if (chosenMin < stepMin) return stepMin;
  return (chosenMin + stepMin - 1) / stepMin * stepMin;
}

// Due again: never fetched, no real clock yet (or it moved back), or as old as its interval (minus
// half a clock step: a download ends some seconds into its wake). With Wi-Fi on anyway, already in the
// last quarter of the interval, so the services stay in one Wi-Fi session instead of drifting apart.
inline bool dueByAge(time_t lastOk, time_t now, int intervalMin, int stepMin, bool wifiAnyway) {
  if (lastOk == 0 || now < REAL_TIME || now < lastOk) return true;
  const long interval = effectiveInterval(intervalMin, stepMin) * 60L;
  const long age = static_cast<long>(now - lastOk);
  if (age >= interval - (stepMin < 1 ? 1 : stepMin) * 30L) return true;
  return wifiAnyway && age * 4 >= interval * 3;
}

// RTC bytes for weather, stocks and the printer state (classic ESP32: 8 KB of RTC memory, about
// 3.3 KB used before stage 8; the build script stops above 6,000 B in total).
constexpr size_t STORE_BYTES_MAX = 1400;

}  // namespace datastore
