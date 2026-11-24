#pragma once
// The icons the widgets draw: Material Design Icons code points (stage 7b, own list). They must be
// in MDI_22_Sparse (tools/fonts/fonts.txt); weather icons (80 px) are in src/services/ServiceParsers.h.
#include <cstdint>

// Printer
constexpr uint32_t MDI_FAN_COOLING = 0xF1797;
constexpr uint32_t MDI_FAN_EXHAUST = 0xF0D43;
constexpr uint32_t MDI_FAN_AUX = 0xF0210;
constexpr uint32_t MDI_HUMIDITY = 0xF058E;
constexpr uint32_t MDI_THERMOMETER = 0xF050F;
constexpr uint32_t MDI_CLOCK = 0xF1452;
constexpr uint32_t MDI_LAYERS = 0xF0328;
constexpr uint32_t MDI_BED = 0xF0697;
constexpr uint32_t MDI_NOZZLE = 0xF0E5B;
constexpr uint32_t MDI_SPOOL = 0xF1294;
constexpr uint32_t MDI_PRINT_ON = 0xF0E5B;  // status bar: printing (the nozzle)
constexpr uint32_t MDI_POWER_ON = 0xF06A5;
constexpr uint32_t MDI_POWER_OFF = 0xF06A6;
// Status bar and problems
constexpr uint32_t MDI_ALERT = 0xF0026;
constexpr uint32_t MDI_BATTERY_FULL = 0xF0079;
constexpr uint32_t MDI_BATTERY_10 = 0xF007A;  // ... MDI_BATTERY_10 + 8 = battery-90
// Stocks
constexpr uint32_t MDI_ARROW_UP = 0xF0737;
constexpr uint32_t MDI_ARROW_DOWN = 0xF072E;
constexpr uint32_t MDI_EQUAL = 0xF01FC;
// Parcels
constexpr uint32_t MDI_PACKAGE = 0xF053E;
constexpr uint32_t MDI_TRUCK = 0xF0788;
constexpr uint32_t MDI_PACKAGE_DELIVERED = 0xF1B52;
