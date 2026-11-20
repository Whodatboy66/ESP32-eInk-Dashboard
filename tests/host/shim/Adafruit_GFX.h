#pragma once
// Host tests: the part of Adafruit GFX the font headers use (the glyph table), so a test can
// measure text with the screen font (tests/host/test_i18n.cpp). Same layout as the library's.
#include <cstdint>
#define PROGMEM
typedef struct {
  uint16_t bitmapOffset;
  uint8_t width, height, xAdvance;
  int8_t xOffset, yOffset;
} GFXglyph;
