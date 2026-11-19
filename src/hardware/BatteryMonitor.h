#pragma once
// Battery voltage (ADC with factory calibration, through the board's 1:2 divider), charge, USB power.
#include "BatteryCurve.h"

float readBattery();
// Battery charge in percent, -1 without a battery sensor.
// -1: no battery sensor, or no battery measured (board on USB power).
int batteryPercentNow();
// USB connected to a computer (USB-Serial/JTAG sees the host).
bool usbPowered();
