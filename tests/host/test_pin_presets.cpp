// Host test for src/hardware/PinPresets.h (stage 7b): the boards' pin presets and their keys in NVS
// and the setup app. Run: tests/host/run.sh
#include "src/hardware/PinPresets.h"
#include "tests/host/check.h"

namespace {

void test_keys_round_trip() {
  const PinPreset all[] = {PinPreset::Esp32Waveshare, PinPreset::Esp32Default, PinPreset::Esp32C6Default,
                           PinPreset::Esp32C6SuperMini, PinPreset::Custom};
  for (PinPreset p : all) CHECK(parsePinPreset(pinPresetValue(p), PinPreset::Custom) == p);
  CHECK_STR(pinPresetValue(PinPreset::Esp32C6Default), "esp32_c6");  // stored on the devices: must not change
  CHECK_STR(pinPresetValue(PinPreset::Esp32Waveshare), "esp32_waveshare");
  CHECK_STR(pinPresetLabel(PinPreset::Esp32C6SuperMini), "ESP32-C6 SuperMini");
  CHECK(parsePinPreset("esp32_c6_mini", PinPreset::Custom) == PinPreset::Esp32C6SuperMini);  // an older key
  CHECK(parsePinPreset("nonsense", PinPreset::Esp32C6Default) == PinPreset::Esp32C6Default);  // unknown: the default
  CHECK(parsePinPreset(nullptr, PinPreset::Esp32Default) == PinPreset::Esp32Default);
}

void test_pins() {
  PinConfig c6 = makePinPreset(PinPreset::Esp32C6Default);  // FireBeetle 2 C6 + Waveshare HAT
  CHECK(c6.epdCs == 1 && c6.epdDc == 8 && c6.epdRst == 14 && c6.epdBusy == 7 && c6.epdSck == 23 &&
        c6.epdMosi == 22 && c6.displayPower == 4 && c6.battery == 0 && c6.demoButton == 2);
  CHECK(hasRequiredDisplayPins(c6));
  PinConfig custom = makePinPreset(PinPreset::Custom);
  CHECK(!hasRequiredDisplayPins(custom) && !isPinAssigned(custom.battery));
  CHECK(!isPinAssigned(makePinPreset(PinPreset::Esp32Waveshare).demoButton));
  custom.epdCs = 5;
  CHECK(!hasRequiredDisplayPins(custom));
}

}  // namespace

int main() {
  test_keys_round_trip();
  test_pins();
  return finish("pin presets");
}
