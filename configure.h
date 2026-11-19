#pragma once
#include "app.h"
#include <Arduino.h>

#ifndef DEBUG
#define DEBUG               1   // set to 1 to enable debug prints
#endif

// Firmware build number: an integer that only goes up with each release. The update client only
// installs a release with a higher build. FW_VERSION (status bar, release name) follows from it.
#ifndef FW_BUILD
#define FW_BUILD            72
#endif
#define FW_STR_(x)          #x
#define FW_STR(x)           FW_STR_(x)
#ifndef FW_VERSION
#define FW_VERSION          "." FW_STR(FW_BUILD)
#endif

// Over-the-air updates (docs/OTA_UPDATES.md): releases of this GitHub repository.
#ifndef OTA_REPO
#define OTA_REPO            "Whodatboy66/ESP32-eInk-Dashboard"
#endif
#define OTA_BATTERY_MIN_PCT 30      // below this, no install unless USB is connected
#define OTA_NIGHT_HOUR      3       // daily check during this local hour (03:00-03:59)
#define OTA_CONFIRM_MS      15000   // "Tap to install" waits this long, then cancels
#define OTA_DONE_SCREEN_MS  10000   // "Updated to ..." stays at least this long after the restart

// Everything logged goes through src/core/Logger.h: secrets masked, on USB, and kept for the
// setup app's diagnostics view.
#include "src/core/Logger.h"

#if DEBUG
  #define DBG(x)            logPrintln(x)
  #define DBGF(fmt, ...)    logPrintf(fmt "\n", ##__VA_ARGS__)
  #define DBGV(label, val)  do { logPrint(label "="); logPrintln(String(val)); } while(0)
  #define DBG_SEP()         logPrintln("---")
#else
  #define DBG(x)
  #define DBGF(fmt, ...)
  #define DBGV(label, val)
  #define DBG_SEP()
#endif

#define DBG_ERROR(x)         logPrintln(x)

// To configure a different display
#define USE_COLORDISPLAY      0

// Display id, checked by the update client: only a release file for the same display is
// installed (the manifest lists it per file).
#if USE_COLORDISPLAY
  #define FW_DISPLAY          "color750"
#else
  #define FW_DISPLAY          "bw750"
#endif

// Settings (NVS) schema version. Raise it when the stored settings change in a way older
// firmware can't read; the new firmware migrates older schemas. The update client doesn't
// install a release whose schema is lower than the device's (it couldn't read the settings).
#define FW_CONFIG_SCHEMA      3   // 2: layout template, shown widgets, clock format (stage 3); 3: custom layout, device name (stage 5)

#define textSpace 14

// =======================
// Pins
// =======================

inline constexpr int16_t PIN_UNASSIGNED = -1;

struct PinConfig {
  int16_t epdCs;
  int16_t epdDc;
  int16_t epdRst;
  int16_t epdBusy;
  int16_t epdSck;
  int16_t epdMosi;
  int16_t displayPower;
  int16_t battery;
  int16_t demoButton;
};

enum class PinPreset : uint8_t {
  Esp32Waveshare,
  Esp32Default,
  Esp32C6Default,
  Esp32C6SuperMini,
  Custom
};

inline constexpr PinConfig makePinPreset(PinPreset preset) {
  switch (preset) {
    case PinPreset::Esp32Waveshare:
      return PinConfig{15, 27, 26, 25, 13, 14, 33, 35, PIN_UNASSIGNED};
    case PinPreset::Esp32C6Default:
      return PinConfig{1, 8, 14, 7, 23, 22, 4, 0, 2};
    case PinPreset::Esp32C6SuperMini:
      return PinConfig{4, 20, 21, 22, 7, 5, 1, PIN_UNASSIGNED, 2};
    case PinPreset::Esp32Default:
      return PinConfig{15, 27, 26, 25, 13, 14, 4, 35, 33};
    case PinPreset::Custom:
    default:
      return PinConfig{
        PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED,
        PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED,
        PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED
      };
  }
}

// Example override before the display is first touched:
//   applyPinPreset(PinPreset::Esp32Default);
//   setCustomPinConfig({15, 27, 26, 25, 13, 14, 4, 35, PIN_UNASSIGNED});
#if defined(DEFAULT_PIN_PRESET_ESP32_WAVESHARE)
inline constexpr PinPreset DEFAULT_PIN_PRESET = PinPreset::Esp32Waveshare;
#elif defined(DEFAULT_PIN_PRESET_ESP32)
inline constexpr PinPreset DEFAULT_PIN_PRESET = PinPreset::Esp32Default;
#elif defined(DEFAULT_PIN_PRESET_ESP32C6_FIREBEETLE)
inline constexpr PinPreset DEFAULT_PIN_PRESET = PinPreset::Esp32C6Default;
#elif defined(DEFAULT_PIN_PRESET_ESP32C6_SUPERMINI)
inline constexpr PinPreset DEFAULT_PIN_PRESET = PinPreset::Esp32C6SuperMini;
#elif CONFIG_IDF_TARGET_ESP32C6
  #if defined(ARDUINO_DFROBOT_FIREBEETLE_2_ESP32C6)
inline constexpr PinPreset DEFAULT_PIN_PRESET = PinPreset::Esp32C6Default;
  #elif defined(ARDUINO_MAKERGO_C6_SUPERMINI)
inline constexpr PinPreset DEFAULT_PIN_PRESET = PinPreset::Esp32C6SuperMini;
  #else
inline constexpr PinPreset DEFAULT_PIN_PRESET = PinPreset::Esp32C6Default;
  #endif
#elif CONFIG_IDF_TARGET_ESP32
inline constexpr PinPreset DEFAULT_PIN_PRESET = PinPreset::Esp32Default;
#else
inline constexpr PinPreset DEFAULT_PIN_PRESET = PinPreset::Custom;
#endif

// Build variant = the sketch.yaml profile. The update client only installs the release file
// made for the same variant, so an image for another board can never be installed.
#if defined(DEFAULT_PIN_PRESET_ESP32_WAVESHARE)
  #define FW_VARIANT "esp32_waveshare"
#elif defined(DEFAULT_PIN_PRESET_ESP32)
  #define FW_VARIANT "esp32"
#elif defined(DEFAULT_PIN_PRESET_ESP32C6_FIREBEETLE)
  #define FW_VARIANT "esp32c6_firebeetle"
#elif defined(DEFAULT_PIN_PRESET_ESP32C6_SUPERMINI)
  #define FW_VARIANT "esp32c6_supermini"
#elif CONFIG_IDF_TARGET_ESP32C6 && defined(ARDUINO_MAKERGO_C6_SUPERMINI)
  #define FW_VARIANT "esp32c6_supermini"
#elif CONFIG_IDF_TARGET_ESP32C6
  #define FW_VARIANT "esp32c6_firebeetle"
#elif CONFIG_IDF_TARGET_ESP32
  #define FW_VARIANT "esp32"
#else
  #define FW_VARIANT "unknown"
#endif

extern PinConfig activePins;

inline void applyPinPreset(PinPreset preset) {
  activePins = makePinPreset(preset);
}

inline void setCustomPinConfig(const PinConfig& customPins) {
  activePins = customPins;
}

inline bool isPinAssigned(int16_t pin) {
  return pin >= 0;
}

inline bool hasDisplayPowerPin() {
  return isPinAssigned(activePins.displayPower);
}

inline bool hasBatteryPin() {
  return isPinAssigned(activePins.battery);
}

inline bool hasDemoButtonPin() {
  return isPinAssigned(activePins.demoButton);
}

inline gpio_num_t getDemoWakeGpio() {
  return static_cast<gpio_num_t>(activePins.demoButton);
}

// The button input is active-HIGH (TTP223 default, or a push button to 3V3) with the internal
// pull-down, so a missing or disconnected sensor reads "not pressed".
#define BUTTON_REBOOT_HOLD_MS 2000    // hold 2 seconds (then release) to reboot the dashboard
#define BUTTON_SETUP_HOLD_MS  4000    // hold 4 seconds (then release) to open setup mode
#define BUTTON_FAULT_MS       15000   // a "touch" this long is a sensor fault: ignored, no gesture
#define BUTTON_DEBOUNCE_MS    60      // the input must read low this long to count as released
#define BUTTON_DOUBLE_TAP_MS  400     // a second tap within this time = double tap (update screen)

// Backup way into setup mode when the touch sensor is missing or broken: tap RST, then within
// BOOT_SETUP_WINDOW_MS press and hold the board's BOOT button for BOOT_SETUP_HOLD_MS.
// (Holding BOOT *while* tapping RST starts the ROM download mode instead.)
#if CONFIG_IDF_TARGET_ESP32C6
  #define BOOT_BUTTON_PIN     9
#elif CONFIG_IDF_TARGET_ESP32
  #define BOOT_BUTTON_PIN     0
#endif
#define BOOT_SETUP_WINDOW_MS  3000
#define BOOT_SETUP_HOLD_MS    1000

inline constexpr uint32_t MDI_FAN_COOLING  = 0x000F1797;
inline constexpr uint32_t MDI_FAN_EXHAUST  = 0x000F0D43;
inline constexpr uint32_t MDI_FAN_AUX      = 0x000F0210;
inline constexpr uint32_t MDI_HUMIDITY     = 0x000F058E;
inline constexpr uint32_t MDI_THERMOMETER  = 0x000F050F;
inline constexpr uint32_t MDI_CLOCK        = 0x000F1452;
inline constexpr uint32_t MDI_LAYERS       = 0x000F0328;
inline constexpr uint32_t MDI_BED          = 0x000F0697;
inline constexpr uint32_t MDI_NOZZLE       = 0x000F0E5B;
inline constexpr uint32_t MDI_SPOOL        = 0x000F1294;
inline constexpr uint32_t MDI_ALERT        = 0x000F0026;
inline constexpr uint32_t MDI_POWER_ON     = 0x000F06A5;
inline constexpr uint32_t MDI_POWER_OFF    = 0x000F06A6;
inline constexpr uint32_t MDI_PRINT_ON     = 0x000F0E5B;
