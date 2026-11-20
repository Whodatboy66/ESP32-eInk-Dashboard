#include "BatteryMonitor.h"

#include <Arduino.h>
#include <esp_attr.h>
#include <time.h>
#if SOC_USB_SERIAL_JTAG_SUPPORTED
#include <driver/usb_serial_jtag.h>
#endif

#include "../../configure.h"

// Battery: the board divides the LiPo voltage by 2 before the ADC pin.
constexpr float BAT_DIVIDER = 2.0f;
constexpr float BAT_CALIBRATION = 1.0f;   // multimeter voltage / displayed voltage, if they differ
constexpr int   BAT_SAMPLES = 16;

float readBattery() {
  if (!hasBatteryPin()) return 0.0f;
  analogReadResolution(12);
  // analogReadMilliVolts applies the chip's factory ADC calibration; average out the noise.
  uint32_t mv = 0;
  for (int i = 0; i < BAT_SAMPLES; i++) mv += analogReadMilliVolts(activePins.battery);
  return mv / (float)BAT_SAMPLES / 1000.0f * BAT_DIVIDER * BAT_CALIBRATION;
}

// USB connected to a computer (USB-Serial/JTAG sees the host). A plain USB charger isn't seen,
// but then the battery reads high while charging.
bool usbPowered() {
#if SOC_USB_SERIAL_JTAG_SUPPORTED
  return usb_serial_jtag_is_connected();
#else
  return false;
#endif
}

// Battery charge in percent; -1 without a battery sensor, or when no battery is measured there
// (the board then runs on USB power, see batteryReadingPlausible).
int batteryPercentNow() {
  if (!hasBatteryPin()) return -1;
  float v = readBattery();
  return batteryReadingPlausible(v) ? batteryPercent(v) : -1;
}

namespace {
struct QuietReading {
  uint32_t magic;
  float volts;
  time_t at;
};
constexpr uint32_t QUIET_MAGIC = 0xBA77E21E;
RTC_NOINIT_ATTR QuietReading quiet;  // survives deep sleep and the restarts of setup mode
}  // namespace

void batteryNoteQuiet(float volts) {
  time_t now = time(nullptr);
  if (now < 1700000000) return;  // needs the clock for its age
  quiet.volts = volts;
  quiet.at = now;
  quiet.magic = QUIET_MAGIC;
}

bool batteryQuiet(float& volts, time_t& at, time_t maxAgeS) {
  time_t now = time(nullptr);
  if (quiet.magic != QUIET_MAGIC || now < quiet.at || now - quiet.at > maxAgeS) return false;
  volts = quiet.volts;
  at = quiet.at;
  return true;
}

