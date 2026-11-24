// The ESP32-C6 SuperMini's LEDs (stage 7b, own code): GPIO15 a plain LED, GPIO8 an RGB LED. Feedback
// while the button is held and pulses when a print starts or ends. Other boards: nothing (those
// pins are display pins there: FireBeetle C6 GPIO8 = DC, ESP32 GPIO15 = CS).
#include "StatusLed.h"

#include <esp32-hal-rgb-led.h>

#include "../services/Bambu.h"
#include "PinMapping.h"

namespace {

constexpr uint8_t PLAIN_LED = 15, RGB_LED = 8;
constexpr uint8_t ON = 48;  // brightness of a colour channel (of 255)

bool superMini() { return activePinPreset == PinPreset::Esp32C6SuperMini; }

// on for onMs out of every periodMs, starting at the phase's beginning
bool blink(unsigned long sinceMs, unsigned long periodMs, unsigned long onMs) { return sinceMs % periodMs < onMs; }

}  // namespace

void forceSuperMiniLedsOff() {
  if (!superMini()) return;
  pinMode(PLAIN_LED, OUTPUT);
  digitalWrite(PLAIN_LED, LOW);
  rgbLedWrite(RGB_LED, 0, 0, 0);
  delay(2);
  rgbLedWrite(RGB_LED, 0, 0, 0);  // twice: the first write after a wake is sometimes lost
}

void setSuperMiniStatusLed(uint8_t red, uint8_t green, uint8_t blue) {
  if (superMini()) rgbLedWrite(RGB_LED, red, green, blue);
}

void flashSuperMiniStatusLed(uint8_t red, uint8_t green, uint8_t blue, uint8_t flashes, uint16_t onMs, uint16_t offMs) {
  if (!superMini()) return;
  for (uint8_t i = 0; i < flashes; i++) {
    rgbLedWrite(RGB_LED, red, green, blue);
    delay(onMs);
    forceSuperMiniLedsOff();
    if (i + 1 < flashes) delay(offMs);
  }
}

// Held: green; past the restart hold: green with cyan flashes; past the setup hold: white flashes.
void updateSuperMiniHoldFeedback(unsigned long heldMs) {
  if (!superMini()) return;
  if (heldMs < BUTTON_REBOOT_HOLD_MS)
    setSuperMiniStatusLed(0, ON, 0);
  else if (heldMs < BUTTON_SETUP_HOLD_MS)
    setSuperMiniStatusLed(0, ON, blink(heldMs - BUTTON_REBOOT_HOLD_MS, 320, 120) ? ON : 0);
  else if (blink(heldMs - BUTTON_SETUP_HOLD_MS, 180, 110))
    setSuperMiniStatusLed(ON, ON, ON);
  else
    setSuperMiniStatusLed(0, ON, 0);
}

// Two green pulses when a print starts, three blue ones when it ends.
void signalPrintTransition() {
  if (!previousIsPrinting && isPrinting) flashSuperMiniStatusLed(0, ON, 0, 2, 140, 90);
  else if (previousIsPrinting && !isPrinting) flashSuperMiniStatusLed(0, 0, ON, 3, 120, 80);
}

void applyBoardSpecificPinSetup() { forceSuperMiniLedsOff(); }
