// The pins in use (see PinMapping.h).
#include "PinMapping.h"

#include "StatusLed.h"

PinConfig activePins = makePinPreset(DEFAULT_PIN_PRESET);
PinConfig customPins = makePinPreset(PinPreset::Custom);
PinPreset activePinPreset = DEFAULT_PIN_PRESET;

namespace {

// The custom pins in NVS: the keys are stored on the devices and must not change.
struct PinField {
  const char* key;
  int16_t PinConfig::*pin;
};
constexpr PinField FIELDS[] = {
    {"pin_epd_cs", &PinConfig::epdCs},     {"pin_epd_dc", &PinConfig::epdDc},
    {"pin_epd_rst", &PinConfig::epdRst},   {"pin_epd_busy", &PinConfig::epdBusy},
    {"pin_epd_sck", &PinConfig::epdSck},   {"pin_epd_mosi", &PinConfig::epdMosi},
    {"pin_disp_power", &PinConfig::displayPower}, {"pin_battery", &PinConfig::battery},
    {"pin_demo_btn", &PinConfig::demoButton},
};

}  // namespace

void logPinConfiguration() {
  const PinConfig& p = activePins;
  DBGF("Pin preset: %s", pinPresetLabel(activePinPreset));
  DBGF("Pins CS=%d DC=%d RST=%d BUSY=%d SCK=%d MOSI=%d PWR=%d BAT=%d BTN=%d", p.epdCs, p.epdDc, p.epdRst, p.epdBusy,
       p.epdSck, p.epdMosi, p.displayPower, p.battery, p.demoButton);
}

void applyStoredPinSelection() {
  activePins = activePinPreset == PinPreset::Custom ? customPins : makePinPreset(activePinPreset);
  applyBoardSpecificPinSetup();
}

void saveCustomPinConfig(Preferences& prefs, const PinConfig& pins) {
  for (const PinField& f : FIELDS) prefs.putInt(f.key, pins.*f.pin);
}

PinConfig loadCustomPinConfig(Preferences& prefs, const PinConfig& fallback) {
  PinConfig pins = fallback;
  for (const PinField& f : FIELDS) pins.*f.pin = static_cast<int16_t>(prefs.getInt(f.key, fallback.*f.pin));
  return pins;
}
