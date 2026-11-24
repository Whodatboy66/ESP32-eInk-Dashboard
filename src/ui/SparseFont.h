#pragma once
// Text with the sparse bitmap fonts (src/ui/Fonts.h) on canvas(). Strings are UTF-8; a code point
// the font doesn't have (and the space, which isn't stored) leaves a gap of half the font size.
// The logic is in GlyphText.h (tested on the PC).
#include <stdint.h>

#include "Fonts.h"

const GFXglyph* findGlyph(const SparseGFXfont* font, uint32_t code);
void drawSparseChar(const SparseGFXfont* font, int16_t x, int16_t y, uint32_t code, uint16_t color);
int16_t getSparseStringWidth(const SparseGFXfont* font, const char* str);
void drawSparseString(const SparseGFXfont* font, int16_t x, int16_t y, const char* str, uint16_t color);
void drawSparseStringCentered(const SparseGFXfont* font, int16_t centerX, int16_t y, const char* str, uint16_t color);
