#include "StatusLed.h"

#include <esp32-hal-rgb-led.h>

#include "../../bambulab.h"
#include "PinMapping.h"

void forceSuperMiniLedsOff() {
  // GPIO8 / GPIO15 are the SuperMini's LEDs only: on other boards they are display pins
  // (FireBeetle C6: GPIO8 = e-paper DC; ESP32: GPIO15 = CS), so leave them alone there.
  if (activePinPreset != PinPreset::Esp32C6SuperMini) return;
  pinMode(15, OUTPUT);
  digitalWrite(15, LOW);
  rgbLedWrite(8, 0, 0, 0);
  delay(2);
  rgbLedWrite(8, 0, 0, 0);
}

void flashSuperMiniStatusLed(uint8_t red, uint8_t green, uint8_t blue, uint8_t flashes, uint16_t onMs, uint16_t offMs) {
  if (activePinPreset != PinPreset::Esp32C6SuperMini) return;

  for (uint8_t i = 0; i < flashes; ++i) {
    rgbLedWrite(8, red, green, blue);
    delay(onMs);
    forceSuperMiniLedsOff();
    if (i + 1 < flashes) {
      delay(offMs);
    }
  }
}

void setSuperMiniStatusLed(uint8_t red, uint8_t green, uint8_t blue) {
  if (activePinPreset != PinPreset::Esp32C6SuperMini) return;
  rgbLedWrite(8, red, green, blue);
}

void updateSuperMiniHoldFeedback(unsigned long heldMs) {
  if (activePinPreset != PinPreset::Esp32C6SuperMini) return;

  if (heldMs < BUTTON_REBOOT_HOLD_MS) {
    // Green stays on while the button is being held.
    setSuperMiniStatusLed(0, 48, 0);
    return;
  }

  if (heldMs < BUTTON_SETUP_HOLD_MS) {
    // Reboot threshold reached (release now to reboot): flash cyan.
    const unsigned long phase = (heldMs - BUTTON_REBOOT_HOLD_MS) % 320UL;
    setSuperMiniStatusLed(0, 48, phase < 120UL ? 48 : 0);
    return;
  }

  // Setup threshold reached: flash white.
  const unsigned long phase = (heldMs - BUTTON_SETUP_HOLD_MS) % 180UL;
  if (phase < 110UL) {
    setSuperMiniStatusLed(48, 48, 48);
  } else {
    setSuperMiniStatusLed(0, 48, 0);
  }
}

void signalPrintTransition() {
  if (activePinPreset != PinPreset::Esp32C6SuperMini) return;

  if (!previousIsPrinting && isPrinting) {
    // Green pulse when a print starts.
    flashSuperMiniStatusLed(0, 48, 0, 2, 140, 90);
  } else if (previousIsPrinting && !isPrinting) {
    // Blue pulse when a print finishes or leaves the printing state.
    flashSuperMiniStatusLed(0, 0, 48, 3, 120, 80);
  }
}

void applyBoardSpecificPinSetup() {
  if (activePinPreset == PinPreset::Esp32C6SuperMini) {
    // SuperMini board LEDs: GPIO15 simple LED and GPIO8 WS2812 RGB LED.
    forceSuperMiniLedsOff();
  }
}

