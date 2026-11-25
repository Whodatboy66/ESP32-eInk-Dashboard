#pragma once
// Battery voltage (ADC with factory calibration, through the board's 1:2 divider), charge, USB power.
#include "BatteryCurve.h"

#include <time.h>

float readBattery();
// Battery charge in percent, -1 without a battery sensor.
// -1: no battery sensor, or no battery measured (board on USB power).
int batteryPercentNow();
// USB connected to a computer (USB-Serial/JTAG sees the host).
bool usbPowered();

// The last reading taken while the board was nearly idle (Wi-Fi off: the dashboard's status bar,
// the hourly battery history). With Wi-Fi on (setup / edit mode, ~100 mA) the voltage sags by a few
// hundredths of a volt, which is up to ~10 % on the flat part of the LiPo curve, so the setup app
// shows this one. Kept in RTC memory across deep sleep and restarts.
void batteryNoteQuiet(float volts);
// The voltage for the status bar: live with the radio off (and kept as the quiet reading), the last
// quiet reading while Wi-Fi or the hotspot is on (BatteryCurve.h, batteryForScreen).
float batteryForDisplay();
// The quiet reading and its time; false if there is none (or it's older than maxAgeS).
bool batteryQuiet(float& volts, time_t& at, time_t maxAgeS);
