#pragma once
// The pins in use (stage 7b, own code): a preset (src/hardware/PinPresets.h) or custom pins, kept in
// NVS ("pin_preset", "pin_*"), logged at boot.
#include <Arduino.h>
#include <Preferences.h>

#include "../../configure.h"

extern PinConfig customPins;
extern PinPreset activePinPreset;

void logPinConfiguration();
void applyStoredPinSelection();  // activePins from the preset or the custom pins, board LEDs off
void saveCustomPinConfig(Preferences& prefs, const PinConfig& pins);
PinConfig loadCustomPinConfig(Preferences& prefs, const PinConfig& fallback);
