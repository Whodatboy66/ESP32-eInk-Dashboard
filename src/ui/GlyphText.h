#pragma once
// Text with the sparse bitmap fonts (stage 7b, own code): UTF-8 decoding, glyph lookup, widths and
// drawing through a pixel function, so it runs on the PC too (tests/host/test_glyph_text.cpp).
// SparseFont.cpp draws with it on the e-paper canvas. The fonts' data is in flash, read directly
// (the ESP32 maps flash into the address space).
#include <cstdint>

#include "fonts/SparseFontBase.h"

namespace glyphtext {

constexpr uint32_t REPLACEMENT = 0xFFFD;  // a broken UTF-8 sequence (no glyph: drawn as a gap)

// The next code point of a UTF-8 string; *s moves past it, never past the terminating zero.
// Broken input (a stray continuation byte, 0xF8-0xFF, a sequence cut off) yields one REPLACEMENT
// per broken character and always moves on, so a loop over a string ends.
inline uint32_t nextCodepoint(const char** s) {
  const unsigned char* p = reinterpret_cast<const unsigned char*>(*s);
  unsigned char lead = p[0];
  if (lead < 0x80) {
    if (lead) (*s)++;
    return lead;
  }
  int extra = (lead & 0xE0) == 0xC0 ? 1 : (lead & 0xF0) == 0xE0 ? 2 : (lead & 0xF8) == 0xF0 ? 3 : -1;
  if (extra < 0) {
    (*s)++;
    return REPLACEMENT;
  }
  uint32_t cp = lead & (0x3F >> extra);
  for (int i = 1; i <= extra; i++) {
    if ((p[i] & 0xC0) != 0x80) {  // cut off (also by the end of the string): skip what was there
      *s += i;
      return REPLACEMENT;
    }
    cp = (cp << 6) | (p[i] & 0x3F);
  }
  *s += extra + 1;
  return cp;
}

// Binary search in the font's sorted code point table.
inline const GFXglyph* findGlyph(const SparseGFXfont* font, uint32_t code) {
  int lo = 0, hi = static_cast<int>(font->glyphCount) - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    uint32_t c = font->charMap[mid];
    if (c == code) return &font->glyph[mid];
    if (c < code) lo = mid + 1;
    else hi = mid - 1;
  }
  return nullptr;
}

// How far a code point moves the pen: its advance, or half the font size if the font lacks it.
inline int advanceOf(const SparseGFXfont* font, uint32_t code) {
  const GFXglyph* g = glyphtext::findGlyph(font, code);  // qualified: SparseFont.h has a global one
  return g ? g->xAdvance : font->yAdvance / 2;
}

inline int16_t textWidth(const SparseGFXfont* font, const char* text) {
  int w = 0;
  while (*text) w += advanceOf(font, nextCodepoint(&text));
  return static_cast<int16_t>(w);
}

using PixelFn = void (*)(int16_t x, int16_t y, uint16_t color);

// One glyph with its origin at (x, y): the glyph record's offsets are added (their y offset puts
// the glyph 1 px below the usual baseline convention; every layout is tuned to that).
inline void drawGlyph(const SparseGFXfont* font, const GFXglyph* g, int16_t x, int16_t y, uint16_t color, PixelFn plot) {
  const uint8_t* bits = font->bitmap + g->bitmapOffset;
  const int rowBytes = (g->width + 7) / 8;
  for (int row = 0; row < g->height; row++) {
    const uint8_t* line = bits + row * rowBytes;
    for (int col = 0; col < g->width; col++)
      if (line[col >> 3] & (0x80 >> (col & 7))) plot(x + g->xOffset + col, y + g->yOffset + row, color);
  }
}

// A string from (x, y); returns where the pen ends.
inline int16_t drawText(const SparseGFXfont* font, int16_t x, int16_t y, const char* text, uint16_t color, PixelFn plot) {
  while (*text) {
    uint32_t code = nextCodepoint(&text);
    const GFXglyph* g = glyphtext::findGlyph(font, code);
    if (g) {
      drawGlyph(font, g, x, y, color, plot);
      x += g->xAdvance;
    } else {
      x += font->yAdvance / 2;
    }
  }
  return x;
}

}  // namespace glyphtext
