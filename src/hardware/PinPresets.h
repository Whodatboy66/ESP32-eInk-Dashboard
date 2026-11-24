#pragma once
// The boards' pin presets (stage 7b, own code): which GPIO drives the display, reads the battery
// and the button. The keys are stored on the devices (NVS "pin_preset") and used by the setup app:
// they must not change. Plain C++, tested on the PC (tests/host/test_pin_presets.cpp).
#include <cstdint>
#include <cstring>

constexpr int16_t PIN_UNASSIGNED = -1;

struct PinConfig {
  int16_t epdCs, epdDc, epdRst, epdBusy, epdSck, epdMosi;  // e-paper SPI and control
  int16_t displayPower;                                    // switches the display HAT's power
  int16_t battery;                                         // battery voltage (ADC, 1:2 divider)
  int16_t demoButton;                                      // the touch sensor / button (wakes the board)
};

enum class PinPreset : uint8_t { Esp32Waveshare, Esp32Default, Esp32C6Default, Esp32C6SuperMini, Custom };

struct PresetInfo {
  PinPreset preset;
  const char* key;    // NVS and setup app
  const char* label;  // shown in the setup app
  PinConfig pins;
};

constexpr PresetInfo PIN_PRESETS[] = {
    //                                                       CS  DC RST BUSY SCK MOSI PWR BAT BTN
    {PinPreset::Esp32Waveshare, "esp32_waveshare", "ESP32 Waveshare", {15, 27, 26, 25, 13, 14, 33, 35, PIN_UNASSIGNED}},
    {PinPreset::Esp32Default, "esp32", "ESP32", {15, 27, 26, 25, 13, 14, 4, 35, 33}},
    {PinPreset::Esp32C6Default, "esp32_c6", "ESP32 C6", {1, 8, 14, 7, 23, 22, 4, 0, 2}},
    {PinPreset::Esp32C6SuperMini, "esp32_c6_supermini", "ESP32-C6 SuperMini", {4, 20, 21, 22, 7, 5, 1, PIN_UNASSIGNED, 2}},
    {PinPreset::Custom, "custom", "Custom", {PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED,
                                             PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED}},
};

static_assert(PIN_PRESETS[0].preset == PinPreset::Esp32Waveshare && PIN_PRESETS[1].preset == PinPreset::Esp32Default &&
                  PIN_PRESETS[2].preset == PinPreset::Esp32C6Default && PIN_PRESETS[3].preset == PinPreset::Esp32C6SuperMini &&
                  PIN_PRESETS[4].preset == PinPreset::Custom,
              "PIN_PRESETS in the order of PinPreset");

constexpr const PresetInfo& presetInfo(PinPreset p) {
  return PIN_PRESETS[static_cast<int>(p) < 4 ? static_cast<int>(p) : 4];
}

constexpr PinConfig makePinPreset(PinPreset p) { return presetInfo(p).pins; }
inline const char* pinPresetValue(PinPreset p) { return presetInfo(p).key; }
inline const char* pinPresetLabel(PinPreset p) { return presetInfo(p).label; }

// A stored or sent key; an unknown one gives fallback ("esp32_c6_mini": an older key of the SuperMini).
inline PinPreset parsePinPreset(const char* key, PinPreset fallback) {
  if (!key) return fallback;
  if (!std::strcmp(key, "esp32_c6_mini")) return PinPreset::Esp32C6SuperMini;
  for (const PresetInfo& p : PIN_PRESETS)
    if (!std::strcmp(key, p.key)) return p.preset;
  return fallback;
}

constexpr bool isPinAssigned(int16_t pin) { return pin >= 0; }

inline bool hasRequiredDisplayPins(const PinConfig& c) {
  return isPinAssigned(c.epdCs) && isPinAssigned(c.epdDc) && isPinAssigned(c.epdRst) && isPinAssigned(c.epdBusy) &&
         isPinAssigned(c.epdSck) && isPinAssigned(c.epdMosi);
}
