#include "PinMapping.h"

#include "StatusLed.h"

PinConfig activePins = makePinPreset(DEFAULT_PIN_PRESET);
PinConfig customPins = makePinPreset(PinPreset::Custom);
PinPreset activePinPreset = DEFAULT_PIN_PRESET;

const char* pinPresetValue(PinPreset preset) {
  switch (preset) {
    case PinPreset::Esp32Waveshare: return "esp32_waveshare";
    case PinPreset::Esp32Default: return "esp32";
    case PinPreset::Esp32C6Default: return "esp32_c6";
    case PinPreset::Esp32C6SuperMini: return "esp32_c6_supermini";
    case PinPreset::Custom:
    default: return "custom";
  }
}

const char* pinPresetLabel(PinPreset preset) {
  switch (preset) {
    case PinPreset::Esp32Waveshare: return "ESP32 Waveshare";
    case PinPreset::Esp32Default: return "ESP32";
    case PinPreset::Esp32C6Default: return "ESP32 C6";
    case PinPreset::Esp32C6SuperMini: return "ESP32-C6 SuperMini";
    case PinPreset::Custom:
    default: return "Custom";
  }
}

void logPinConfiguration() {
  DBGF("Pin preset: %s", pinPresetLabel(activePinPreset));
  DBGF("Pins CS=%d DC=%d RST=%d BUSY=%d SCK=%d MOSI=%d PWR=%d BAT=%d BTN=%d",
       activePins.epdCs,
       activePins.epdDc,
       activePins.epdRst,
       activePins.epdBusy,
       activePins.epdSck,
       activePins.epdMosi,
       activePins.displayPower,
       activePins.battery,
       activePins.demoButton);
}

PinPreset parsePinPreset(const String& value) {
  if (value == "esp32_waveshare") return PinPreset::Esp32Waveshare;
  if (value == "esp32") return PinPreset::Esp32Default;
  if (value == "esp32_c6") return PinPreset::Esp32C6Default;
  if (value == "esp32_c6_supermini" || value == "esp32_c6_mini") return PinPreset::Esp32C6SuperMini;
  if (value == "custom") return PinPreset::Custom;
  return DEFAULT_PIN_PRESET;
}


void applyStoredPinSelection() {
  if (activePinPreset == PinPreset::Custom) {
    setCustomPinConfig(customPins);
  } else {
    applyPinPreset(activePinPreset);
  }
  applyBoardSpecificPinSetup();
}

void saveCustomPinConfig(Preferences& prefs, const PinConfig& pins) {
  prefs.putInt("pin_epd_cs", pins.epdCs);
  prefs.putInt("pin_epd_dc", pins.epdDc);
  prefs.putInt("pin_epd_rst", pins.epdRst);
  prefs.putInt("pin_epd_busy", pins.epdBusy);
  prefs.putInt("pin_epd_sck", pins.epdSck);
  prefs.putInt("pin_epd_mosi", pins.epdMosi);
  prefs.putInt("pin_disp_power", pins.displayPower);
  prefs.putInt("pin_battery", pins.battery);
  prefs.putInt("pin_demo_btn", pins.demoButton);
}

PinConfig loadCustomPinConfig(Preferences& prefs, const PinConfig& fallback) {
  PinConfig pins = fallback;
  pins.epdCs = static_cast<int16_t>(prefs.getInt("pin_epd_cs", fallback.epdCs));
  pins.epdDc = static_cast<int16_t>(prefs.getInt("pin_epd_dc", fallback.epdDc));
  pins.epdRst = static_cast<int16_t>(prefs.getInt("pin_epd_rst", fallback.epdRst));
  pins.epdBusy = static_cast<int16_t>(prefs.getInt("pin_epd_busy", fallback.epdBusy));
  pins.epdSck = static_cast<int16_t>(prefs.getInt("pin_epd_sck", fallback.epdSck));
  pins.epdMosi = static_cast<int16_t>(prefs.getInt("pin_epd_mosi", fallback.epdMosi));
  pins.displayPower = static_cast<int16_t>(prefs.getInt("pin_disp_power", fallback.displayPower));
  pins.battery = static_cast<int16_t>(prefs.getInt("pin_battery", fallback.battery));
  pins.demoButton = static_cast<int16_t>(prefs.getInt("pin_demo_btn", fallback.demoButton));
  return pins;
}

void pinValueToText(int16_t pin, char* out, size_t outSize) {
  if (outSize == 0) return;
  if (isPinAssigned(pin)) {
    snprintf(out, outSize, "%d", pin);
  } else {
    out[0] = '\0';
  }
}

bool hasRequiredDisplayPins(const PinConfig& pins) {
  return isPinAssigned(pins.epdCs) &&
         isPinAssigned(pins.epdDc) &&
         isPinAssigned(pins.epdRst) &&
         isPinAssigned(pins.epdBusy) &&
         isPinAssigned(pins.epdSck) &&
         isPinAssigned(pins.epdMosi);
}

