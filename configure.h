#pragma once
// Build options of the firmware (stage 7b, own code): build number, update source, logging, the
// display, the board's pin preset, the button's timing. Settings people change are in NVS
// (src/core/ConfigManager.cpp), not here.
#include "app.h"
#include <Arduino.h>

#ifndef DEBUG
#define DEBUG               1   // set to 1 to enable debug prints
#endif

// Firmware build number: an integer that only goes up with each release. The update client only
// installs a release with a higher build. FW_VERSION (status bar, release name) follows from it.
#ifndef FW_BUILD
#define FW_BUILD            85
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
#define OTA_NIGHT_HOUR      3       // daily check from this local hour until noon (first wake after 03:00)
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

// The display: 0 = the 7.5" black and white Waveshare panel (GxEPD2_750_T7), 1 = its 4-colour sibling.
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

// =======================
// Pins (presets in src/hardware/PinPresets.h)
// =======================
#include "src/hardware/PinPresets.h"

// The board's preset: set by the sketch.yaml profile (-DDEFAULT_PIN_PRESET_...), else from the
// board the core was built for.
#if defined(DEFAULT_PIN_PRESET_ESP32_WAVESHARE)
inline constexpr PinPreset DEFAULT_PIN_PRESET = PinPreset::Esp32Waveshare;
#elif defined(DEFAULT_PIN_PRESET_ESP32)
inline constexpr PinPreset DEFAULT_PIN_PRESET = PinPreset::Esp32Default;
#elif defined(DEFAULT_PIN_PRESET_ESP32C6_FIREBEETLE)
inline constexpr PinPreset DEFAULT_PIN_PRESET = PinPreset::Esp32C6Default;
#elif defined(DEFAULT_PIN_PRESET_ESP32C6_SUPERMINI) || (CONFIG_IDF_TARGET_ESP32C6 && defined(ARDUINO_MAKERGO_C6_SUPERMINI))
inline constexpr PinPreset DEFAULT_PIN_PRESET = PinPreset::Esp32C6SuperMini;
#elif CONFIG_IDF_TARGET_ESP32C6
inline constexpr PinPreset DEFAULT_PIN_PRESET = PinPreset::Esp32C6Default;
#elif CONFIG_IDF_TARGET_ESP32
inline constexpr PinPreset DEFAULT_PIN_PRESET = PinPreset::Esp32Default;
#else
inline constexpr PinPreset DEFAULT_PIN_PRESET = PinPreset::Custom;
#endif

// Build variant = the sketch.yaml profile: the update client installs only the release file made
// for the same variant (an image for another board can't be installed).
#if defined(DEFAULT_PIN_PRESET_ESP32_WAVESHARE)
  #define FW_VARIANT "esp32_waveshare"
#elif defined(DEFAULT_PIN_PRESET_ESP32)
  #define FW_VARIANT "esp32"
#elif defined(DEFAULT_PIN_PRESET_ESP32C6_SUPERMINI) || (CONFIG_IDF_TARGET_ESP32C6 && defined(ARDUINO_MAKERGO_C6_SUPERMINI))
  #define FW_VARIANT "esp32c6_supermini"
#elif defined(DEFAULT_PIN_PRESET_ESP32C6_FIREBEETLE) || CONFIG_IDF_TARGET_ESP32C6
  #define FW_VARIANT "esp32c6_firebeetle"
#elif CONFIG_IDF_TARGET_ESP32
  #define FW_VARIANT "esp32"
#else
  #define FW_VARIANT "unknown"
#endif

extern PinConfig activePins;  // the pins in use (src/hardware/PinMapping.cpp)

inline bool hasDisplayPowerPin() { return isPinAssigned(activePins.displayPower); }
inline bool hasBatteryPin() { return isPinAssigned(activePins.battery); }
inline bool hasDemoButtonPin() { return isPinAssigned(activePins.demoButton); }
inline gpio_num_t getDemoWakeGpio() { return static_cast<gpio_num_t>(activePins.demoButton); }

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

#include "src/ui/Icons.h"
