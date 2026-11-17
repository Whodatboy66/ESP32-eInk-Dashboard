#pragma once
// Pin presets (configure.h) and the active pin map: which preset / custom pins are used, stored in
// NVS ("pin_preset", "pin_*"), logged at boot.
#include <Arduino.h>
#include <Preferences.h>

#include "../../configure.h"

extern PinConfig customPins;
extern PinPreset activePinPreset;

const char* pinPresetValue(PinPreset preset);
const char* pinPresetLabel(PinPreset preset);
void logPinConfiguration();
PinPreset parsePinPreset(const String& value);
void applyStoredPinSelection();
void saveCustomPinConfig(Preferences& prefs, const PinConfig& pins);
PinConfig loadCustomPinConfig(Preferences& prefs, const PinConfig& fallback);
void pinValueToText(int16_t pin, char* out, size_t outSize);
bool hasRequiredDisplayPins(const PinConfig& pins);
