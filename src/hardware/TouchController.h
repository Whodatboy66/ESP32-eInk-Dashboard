#pragma once
// The touch sensor / button (TTP223, active HIGH with pull-down) and the BOOT button backup:
// gestures at wake-up and while awake (src/input/ButtonGesture.h), setup / update requests.
#include <Arduino.h>

#include "../input/ButtonGesture.h"
#include "../../configure.h"

constexpr int BUTTON_ACTION_NONE = 0;
constexpr int BUTTON_ACTION_AP = 2;
constexpr int BUTTON_ACTION_REBOOT = 3;
constexpr int BUTTON_ACTION_UPDATE = 4;  // double tap: info and update screen

// A setup hold while awake restarts into setup mode: the request survives the restart.
constexpr uint32_t SETUP_REQUEST_MAGIC = 0x5E7AB0A7;
// "Change Wi-Fi" in the setup app: restart into the setup hotspot even when Wi-Fi works.
constexpr uint32_t SETUP_REQUEST_AP_MAGIC = 0x5E7AB0A8;
// After saving settings in edit mode: restart back into edit mode (same code and session).
constexpr uint32_t SETUP_REQUEST_EDIT_MAGIC = 0x5E7AB0A9;
extern uint32_t setupRequest;

extern volatile bool buttonFaulted;   // sensor reported "pressed" too long (RTC memory)
extern volatile bool buttonBusy;      // a gesture is being decided or acted on
extern volatile ButtonEvent buttonMailbox;  // last tap / double tap seen while awake

// The gesture at wake-up (waits for its end). BUTTON_ACTION_*.
int readWakeButtonAction();
// FreeRTOS task watching the button while the dashboard is awake.
void buttonWatchTask(void*);

#ifdef BOOT_BUTTON_PIN
// Right after a reset: BOOT pressed within BOOT_SETUP_WINDOW_MS and held BOOT_SETUP_HOLD_MS.
bool waitForBootButtonSetup();
bool isPinInActiveConfig(int16_t pin);
#endif
