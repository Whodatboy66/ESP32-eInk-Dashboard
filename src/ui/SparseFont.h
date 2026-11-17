#pragma once
// Text with the sparse bitmap fonts (src/ui/Fonts.h), drawn on canvas(). Strings are UTF-8;
// glyphs a font doesn't have leave a half-height gap.
#include <stdint.h>

#include "Fonts.h"

const GFXglyph* findGlyph(const SparseGFXfont* font, uint32_t code);
uint32_t utf8_next(const char** str);
void drawSparseChar(const SparseGFXfont* font, int16_t x, int16_t y, uint32_t code, uint16_t color);
void drawSparseCharCenter(const SparseGFXfont* font, int16_t x, int16_t yCenter, uint32_t code, uint16_t color);
int16_t getSparseStringWidth(const SparseGFXfont* font, const char* str);
void drawSparseString(const SparseGFXfont* font, int16_t x, int16_t y, const char* str, uint16_t color);
void drawSparseStringCentered(const SparseGFXfont* font, int16_t centerX, int16_t y, const char* str, uint16_t color);
