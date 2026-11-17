#include "BatteryMonitor.h"

#include <Arduino.h>
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

// Battery charge in percent, -1 without a battery sensor.
int batteryPercentNow() {
  return hasBatteryPin() ? batteryPercent(readBattery()) : -1;
}

