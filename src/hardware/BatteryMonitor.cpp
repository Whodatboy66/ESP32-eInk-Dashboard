#include "BatteryMonitor.h"

#include <Arduino.h>
#include <esp_attr.h>
#include <time.h>
#if SOC_USB_SERIAL_JTAG_SUPPORTED
#include <driver/usb_serial_jtag.h>
#endif

#include "../../configure.h"

// The LiPo's voltage reaches the ADC pin through the board's 1:2 divider. analogReadMilliVolts()
// applies the chip's factory calibration; 16 readings average the noise out.
namespace {
constexpr float DIVIDER = 2.0f;
constexpr int SAMPLES = 16;
}  // namespace

float readBattery() {
  if (!hasBatteryPin()) return 0.0f;
  analogReadResolution(12);
  uint32_t sum = 0;
  for (int i = 0; i < SAMPLES; i++) sum += analogReadMilliVolts(activePins.battery);
  return sum * DIVIDER / SAMPLES / 1000.0f;
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

