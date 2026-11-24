// The touch sensor / button (stage 7b, own code): the gesture at wake-up, a watcher while awake,
// and the BOOT button as a backup way into setup mode. The gestures themselves are decided by
// src/input/ButtonGesture.h (tested on the PC).
#include "TouchController.h"

#include <driver/gpio.h>

#include "../update/OtaFlow.h"
#include "StatusLed.h"

namespace {

constexpr ButtonTiming TIMING{BUTTON_REBOOT_HOLD_MS, BUTTON_SETUP_HOLD_MS, BUTTON_FAULT_MS, BUTTON_DEBOUNCE_MS,
                              BUTTON_DOUBLE_TAP_MS};

bool pressedNow() { return digitalRead(activePins.demoButton) == HIGH; }

// The input with the internal pull-down (an unconnected pin reads "not pressed"). The sleep code
// latches the pad (pad hold): released here so pinMode() works.
void configureInput() {
  gpio_hold_dis(getDemoWakeGpio());
  pinMode(activePins.demoButton, INPUT_PULLDOWN);
}

// What a finished gesture shows and logs: the LED feedback (SuperMini) and a log line.
void announce(ButtonEvent e) {
  forceSuperMiniLedsOff();
  switch (e) {
    case ButtonEvent::DoubleTap:
      logPrintln("Button double tap -> update screen");
      break;
    case ButtonEvent::Reboot:
      logPrintln("Button held -> reboot");
      flashSuperMiniStatusLed(0, 0, 48, 2, 100, 70);  // blue
      break;
    case ButtonEvent::Setup:
      logPrintln("Button held -> setup / edit mode");
      flashSuperMiniStatusLed(48, 0, 0, 2, 90, 70);  // red
      break;
    case ButtonEvent::Fault:
      logPrintf("[Button] HIGH for %u s: sensor fault, ignored until it reads LOW\n", (unsigned)(BUTTON_FAULT_MS / 1000));
      break;
    default:
      break;
  }
}

}  // namespace

// Kept in deep sleep: the input read "pressed" for BUTTON_FAULT_MS (stuck, disconnected or
// recalibrating). While set and the input still reads HIGH, it is ignored and doesn't wake the
// board; cleared as soon as it reads LOW.
RTC_DATA_ATTR volatile bool buttonFaulted = false;
// A restart clears RTC_DATA_ATTR: the setup request lives in no-init RTC memory (with a magic value).
RTC_NOINIT_ATTR uint32_t setupRequest;
volatile bool buttonBusy = false;  // a press, its release debounce or the double-tap window is open
volatile ButtonEvent buttonMailbox = ButtonEvent::None;  // the last tap / double tap while awake

int readWakeButtonAction() {
  if (!hasDemoButtonPin()) return BUTTON_ACTION_NONE;
  configureInput();
  const bool high = pressedNow();
  const bool buttonWake = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1;
  DBGF("[Button] GPIO%d is %s at %lu ms%s", activePins.demoButton, high ? "HIGH (pressed)" : "LOW", millis(),
       buttonFaulted ? " (sensor fault flagged)" : "");
  if (!high) {
    buttonFaulted = false;
    if (!buttonWake) return BUTTON_ACTION_NONE;  // a tap woke us and is over: wait for a second one
  } else if (buttonFaulted) {
    DBG(F("[Button] still HIGH since the fault: ignored"));
    return BUTTON_ACTION_NONE;
  }
  ButtonGesture gesture(TIMING);
  if (high) gesture.beginPress(buttonWake ? 0 : millis());  // after a wake the press began at boot
  else gesture.beginAfterTap(millis());
  for (;;) {
    const uint32_t now = millis();
    const ButtonEvent e = gesture.update(pressedNow(), now);
    if (e == ButtonEvent::None) {
      if (gesture.isPressed()) updateSuperMiniHoldFeedback(gesture.heldMs(now));
      delay(10);
      continue;
    }
    announce(e);
    switch (e) {
      case ButtonEvent::DoubleTap: return BUTTON_ACTION_UPDATE;
      case ButtonEvent::Reboot: return BUTTON_ACTION_REBOOT;
      case ButtonEvent::Setup: return BUTTON_ACTION_AP;
      case ButtonEvent::Fault: buttonFaulted = true; return BUTTON_ACTION_NONE;
      default: return BUTTON_ACTION_NONE;  // a single tap: the wake itself refreshes
    }
  }
}

// While awake (Wi-Fi, downloads, drawing): holds restart or open setup at any time; taps go to the
// mailbox (update screen, confirm tap).
void buttonWatchTask(void*) {
  ButtonGesture gesture(TIMING, buttonFaulted);
  for (;;) {
    const uint32_t now = millis();
    const ButtonEvent e = gesture.update(pressedNow(), now);
    if (e == ButtonEvent::None) {
      if (gesture.isPressed()) updateSuperMiniHoldFeedback(gesture.heldMs(now));
    } else if (e == ButtonEvent::Tap || e == ButtonEvent::DoubleTap) {
      announce(e);
      buttonMailbox = e;
    } else if (e == ButtonEvent::Reboot || e == ButtonEvent::Setup) {
      buttonBusy = true;
      otaConfirmByUser(e == ButtonEvent::Reboot ? "reboot hold" : "setup hold");
      if (e == ButtonEvent::Setup) setupRequest = SETUP_REQUEST_MAGIC;
      announce(e);
      ESP.restart();
    } else {
      announce(e);  // a fault
    }
    buttonFaulted = gesture.isFaulted();
    buttonBusy = gesture.isBusy();
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

#ifdef BOOT_BUTTON_PIN
// Right after a reset: BOOT pressed within BOOT_SETUP_WINDOW_MS and held BOOT_SETUP_HOLD_MS. Without
// a press this waits the whole window (time for a USB serial monitor to attach, too).
bool waitForBootButtonSetup() {
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);  // BOOT pulls the pin LOW
  const unsigned long start = millis();
  unsigned long downSince = 0;
  bool down = false;
  for (;; delay(10)) {
    const unsigned long now = millis();
    if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
      if (!down) downSince = now;
      down = true;
      if (now - downSince >= BOOT_SETUP_HOLD_MS) return true;
    } else {
      down = false;
      if (now - start >= BOOT_SETUP_WINDOW_MS) return false;
    }
  }
}

bool isPinInActiveConfig(int16_t pin) {
  const PinConfig& p = activePins;
  for (int16_t used : {p.epdCs, p.epdDc, p.epdRst, p.epdBusy, p.epdSck, p.epdMosi, p.displayPower, p.battery, p.demoButton})
    if (pin == used) return true;
  return false;
}
#endif
