#include "TouchController.h"

#include <driver/gpio.h>

#include "../update/OtaFlow.h"
#include "StatusLed.h"


static constexpr ButtonTiming BUTTON_TIMING{
  BUTTON_REBOOT_HOLD_MS, BUTTON_SETUP_HOLD_MS, BUTTON_FAULT_MS, BUTTON_DEBOUNCE_MS, BUTTON_DOUBLE_TAP_MS};

// Set when the input read "pressed" for BUTTON_FAULT_MS (stuck, disconnected or recalibrating
// sensor). Kept across deep sleep: while it is set and the input still reads HIGH, the input is
// ignored and the board doesn't wake on it. Cleared as soon as the input reads LOW.
RTC_DATA_ATTR volatile bool buttonFaulted = false;

// Input with the internal pull-down: an unconnected pin reads LOW ("not pressed").
static void configureButtonInput() {
  // Before deep sleep the pad's pull-down is latched (pad hold) by the sleep code; release it so
  // pinMode() takes effect.
  gpio_hold_dis(getDemoWakeGpio());
  pinMode(activePins.demoButton, INPUT_PULLDOWN);
}

int readWakeButtonAction() {
  if (!hasDemoButtonPin()) {
    return BUTTON_ACTION_NONE;
  }

  configureButtonInput();
  bool high = digitalRead(activePins.demoButton) == HIGH;
  bool buttonWake = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1;
  DBGF("[Button] GPIO%d is %s at %lu ms%s", activePins.demoButton,
       high ? "HIGH (pressed)" : "LOW", millis(), buttonFaulted ? " (sensor fault flagged)" : "");
  if (!high) {
    buttonFaulted = false;
    // A tap woke the board and was already released: wait briefly for a second tap.
    if (!buttonWake) return BUTTON_ACTION_NONE;
  } else if (buttonFaulted) {
    DBG(F("[Button] still HIGH since the fault: ignored"));
    return BUTTON_ACTION_NONE;
  }

  ButtonGesture gesture(BUTTON_TIMING);
  if (high) {
    // After a button wake the press began at wake time, so count the boot time as held time.
    gesture.beginPress(buttonWake ? 0 : millis());
  } else {
    gesture.beginAfterTap(millis());
  }
  for (;;) {
    uint32_t now = millis();
    switch (gesture.update(digitalRead(activePins.demoButton) == HIGH, now)) {
      case ButtonEvent::None:
        if (gesture.isPressed()) updateSuperMiniHoldFeedback(gesture.heldMs(now));
        delay(10);
        break;
      case ButtonEvent::Tap:
        forceSuperMiniLedsOff();
        return BUTTON_ACTION_NONE;
      case ButtonEvent::DoubleTap:
        forceSuperMiniLedsOff();
        Serial.println("Button double tap -> update screen");
        return BUTTON_ACTION_UPDATE;
      case ButtonEvent::Reboot:
        forceSuperMiniLedsOff();
        Serial.println("Button held -> reboot");
        flashSuperMiniStatusLed(0, 0, 48, 2, 100, 70);
        return BUTTON_ACTION_REBOOT;
      case ButtonEvent::Setup:
        forceSuperMiniLedsOff();
        Serial.println("Button held -> AP config mode");
        flashSuperMiniStatusLed(48, 0, 0, 2, 90, 70);
        return BUTTON_ACTION_AP;
      case ButtonEvent::Fault:
        forceSuperMiniLedsOff();
        Serial.printf("[Button] HIGH for %u s: sensor fault, ignored until it reads LOW\n",
                      (unsigned)(BUTTON_FAULT_MS / 1000));
        buttonFaulted = true;
        return BUTTON_ACTION_NONE;
    }
  }
}

// A setup hold while awake restarts the board into setup mode. RTC_DATA_ATTR is reset by a
// restart, so the request lives in no-init RTC memory, guarded by a magic value.
RTC_NOINIT_ATTR uint32_t setupRequest;

// True while the watch task tracks a press (including the release debounce and the double-tap
// window) or is acting on a gesture (restarting). The sleep path waits for it, so a release just
// before sleep isn't lost.
volatile bool buttonBusy = false;
// The last tap / double tap seen by the watch task (ButtonEvent), for the update screen's confirm
// tap and for a double tap while the dashboard refreshes. Reset to None by the reader.
volatile ButtonEvent buttonMailbox = ButtonEvent::None;

// Watches the button while the dashboard is awake (WiFi, fetching, drawing), so a hold works at
// any time, not only from deep sleep. A short tap is ignored: the dashboard is already refreshing.
void buttonWatchTask(void*) {
  ButtonGesture gesture(BUTTON_TIMING, buttonFaulted);
  for (;;) {
    uint32_t now = millis();
    switch (gesture.update(digitalRead(activePins.demoButton) == HIGH, now)) {
      case ButtonEvent::None:
        if (gesture.isPressed()) updateSuperMiniHoldFeedback(gesture.heldMs(now));
        break;
      case ButtonEvent::Tap:
        forceSuperMiniLedsOff();
        buttonMailbox = ButtonEvent::Tap;
        break;
      case ButtonEvent::DoubleTap:
        forceSuperMiniLedsOff();
        Serial.println("Button double tap -> update screen");
        buttonMailbox = ButtonEvent::DoubleTap;
        break;
      case ButtonEvent::Reboot:
        buttonBusy = true;
        otaConfirmByUser("reboot hold");
        forceSuperMiniLedsOff();
        Serial.println("Button held -> reboot");
        flashSuperMiniStatusLed(0, 0, 48, 2, 100, 70);
        ESP.restart();
        break;
      case ButtonEvent::Setup:
        buttonBusy = true;
        otaConfirmByUser("setup hold");
        setupRequest = SETUP_REQUEST_MAGIC;
        forceSuperMiniLedsOff();
        Serial.println("Button held -> AP config mode");
        flashSuperMiniStatusLed(48, 0, 0, 2, 90, 70);
        ESP.restart();
        break;
      case ButtonEvent::Fault:
        forceSuperMiniLedsOff();
        Serial.println("[Button] held past BUTTON_FAULT_MS: sensor fault, ignored until it reads LOW");
        break;
    }
    buttonFaulted = gesture.isFaulted();
    buttonBusy = gesture.isBusy();
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

#ifdef BOOT_BUTTON_PIN
// Right after a reset: true when BOOT is pressed within BOOT_SETUP_WINDOW_MS and held for
// BOOT_SETUP_HOLD_MS (backup way into setup mode without the touch sensor). Without a press
// this waits the whole window, which also gives a USB serial monitor time to attach.
bool waitForBootButtonSetup() {
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);  // BOOT pulls the pin LOW; boards have a pull-up too
  unsigned long start = millis();
  unsigned long pressStart = 0;
  bool pressed = false;
  for (;;) {
    unsigned long now = millis();
    if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
      if (!pressed) {
        pressed = true;
        pressStart = now;
      }
      if (now - pressStart >= BOOT_SETUP_HOLD_MS) return true;
    } else {
      pressed = false;
      if (now - start >= BOOT_SETUP_WINDOW_MS) return false;
    }
    delay(10);
  }
}

bool isPinInActiveConfig(int16_t pin) {
  const PinConfig& p = activePins;
  return pin == p.epdCs || pin == p.epdDc || pin == p.epdRst || pin == p.epdBusy ||
         pin == p.epdSck || pin == p.epdMosi || pin == p.displayPower || pin == p.battery ||
         pin == p.demoButton;
}
#endif

