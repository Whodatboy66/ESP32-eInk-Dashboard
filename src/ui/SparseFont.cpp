// The sparse bitmap fonts on the e-paper canvas (src/ui/Canvas.h), through GlyphText.h.
#include "SparseFont.h"

#include "Canvas.h"
#include "GlyphText.h"

namespace {
void plotOnCanvas(int16_t x, int16_t y, uint16_t color) { canvas().drawPixel(x, y, color); }
}  // namespace

const GFXglyph* findGlyph(const SparseGFXfont* font, uint32_t code) { return glyphtext::findGlyph(font, code); }

void drawSparseChar(const SparseGFXfont* font, int16_t x, int16_t y, uint32_t code, uint16_t color) {
  if (const GFXglyph* g = glyphtext::findGlyph(font, code)) glyphtext::drawGlyph(font, g, x, y, color, plotOnCanvas);
}

int16_t getSparseStringWidth(const SparseGFXfont* font, const char* str) { return glyphtext::textWidth(font, str); }

void drawSparseString(const SparseGFXfont* font, int16_t x, int16_t y, const char* str, uint16_t color) {
  glyphtext::drawText(font, x, y, str, color, plotOnCanvas);
}

void drawSparseStringCentered(const SparseGFXfont* font, int16_t centerX, int16_t y, const char* str, uint16_t color) {
  drawSparseString(font, centerX - getSparseStringWidth(font, str) / 2, y, str, color);
}
