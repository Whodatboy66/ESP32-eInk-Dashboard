#pragma once
// The ESP32-C6 SuperMini's on-board LEDs (GPIO15 LED, GPIO8 RGB): hold feedback for the button and
// print start / end pulses. Does nothing on other boards (those pins are display pins there).
#include <Arduino.h>

void forceSuperMiniLedsOff();
void flashSuperMiniStatusLed(uint8_t red, uint8_t green, uint8_t blue, uint8_t flashes, uint16_t onMs, uint16_t offMs);
void setSuperMiniStatusLed(uint8_t red, uint8_t green, uint8_t blue);
void updateSuperMiniHoldFeedback(unsigned long heldMs);
void signalPrintTransition();
void applyBoardSpecificPinSetup();
