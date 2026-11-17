// Renderer for the sparse bitmap fonts (src/ui/fonts/): only the glyphs a font needs, looked up
// by Unicode code point. Draws on canvas() (src/ui/Canvas.h): the e-paper display on the device.
#include "SparseFont.h"

#include "Canvas.h"

#include <GxEPD2.h>

const GFXglyph* findGlyph(const SparseGFXfont* font, uint32_t code) {
  int low = 0;
  int high = font->glyphCount - 1;

  while (low <= high) {
    int mid = (low + high) / 2;
    uint32_t val = pgm_read_dword(&font->charMap[mid]);

    if (val == code) return &font->glyph[mid];
    if (val < code) low = mid + 1;
    else high = mid - 1;
  }
  return nullptr;
}
// Read next Unicode codepoint from UTF-8 string.
// Returns a codepoint and advances `*str`.
uint32_t utf8_next(const char** str) {
  const unsigned char* s = (const unsigned char*)(*str);
  // if (!s[1]) return 0;

  uint32_t codepoint = 0;

  if (*s < 0x80) {  // 1 byte
    codepoint = *s;
    (*str)++;
  } else if ((*s & 0xE0) == 0xC0) {  // 2 bytes
    codepoint = s[0] & 0x1F;
    codepoint <<= 6;
    codepoint |= s[1] & 0x3F;
    (*str) += 2;
  } else if ((*s & 0xF0) == 0xE0) {  // 3 bytes
    codepoint = s[0] & 0x0F;
    codepoint <<= 6;
    codepoint |= s[1] & 0x3F;
    codepoint <<= 6;
    codepoint |= s[2] & 0x3F;
    (*str) += 3;
  } else if ((*s & 0xF8) == 0xF0) {  // 4 bytes
    codepoint = s[0] & 0x07;
    codepoint <<= 6;
    codepoint |= s[1] & 0x3F;
    codepoint <<= 6;
    codepoint |= s[2] & 0x3F;
    codepoint <<= 6;
    codepoint |= s[3] & 0x3F;
    (*str) += 4;
  }
  return codepoint;
}

// Draw a single character from any SparseGFXfont.
// `x,y` are baseline-ish coordinates (matches Adafruit_GFX glyph conventions).
void drawSparseChar(const SparseGFXfont* font, int16_t x, int16_t y, uint32_t code, uint16_t color) {
  const GFXglyph* glyph = findGlyph(font, code);
  if (!glyph) return;

  uint16_t bo = pgm_read_word(&glyph->bitmapOffset);
  uint8_t w = pgm_read_byte(&glyph->width);
  uint8_t h = pgm_read_byte(&glyph->height);
  int8_t xo = pgm_read_byte(&glyph->xOffset);
  int8_t yo = pgm_read_byte(&glyph->yOffset);

  const uint8_t* bitmap = font->bitmap;
  uint8_t rowBytes = (w + 7) / 8;

  for (uint8_t yy = 0; yy < h; yy++) {
    for (uint8_t xx = 0; xx < w; xx++) {
      if (pgm_read_byte(&bitmap[bo + yy * rowBytes + xx / 8]) & (0x80 >> (xx & 7))) {
        canvas().drawPixel(x + xo + xx, y + yo + yy, color);
      }
    }
  }
}

void drawSparseCharCenter(const SparseGFXfont* font, int16_t x, int16_t yCenter, uint32_t code, uint16_t color) {
  const GFXglyph* glyph = findGlyph(font, code);
  if (!glyph) return;

  uint16_t bo = pgm_read_word(&glyph->bitmapOffset);
  uint8_t w = pgm_read_byte(&glyph->width);
  uint8_t h = pgm_read_byte(&glyph->height);
  int8_t xo = pgm_read_byte(&glyph->xOffset);
  int8_t yo = pgm_read_byte(&glyph->yOffset);

  const uint8_t* bitmap = font->bitmap;
  uint8_t rowBytes = (w + 7) / 8;

  // Shift baseline so glyph center = yCenter
  int16_t baselineY = yCenter - (yo + h / 2);

  for (uint8_t yy = 0; yy < h; yy++) {
    for (uint8_t xx = 0; xx < w; xx++) {
      if (pgm_read_byte(&bitmap[bo + yy * rowBytes + xx / 8]) & (0x80 >> (xx & 7))) {
        canvas().drawPixel(x + xo + xx, baselineY + yo + yy, color);
      }
    }
  }
}

int16_t getSparseStringWidth(const SparseGFXfont* font, const char* str) {
  int16_t width = 0;

  const char* p = str;
  while (*p) {
    uint32_t code = utf8_next(&p);
    const GFXglyph* glyph = findGlyph(font, code);

    if (glyph) {
      width += pgm_read_byte(&glyph->xAdvance);
    } else {
      width += font->yAdvance / 2;
    }
  }

  return width;
}

void drawSparseString(const SparseGFXfont* font, int16_t x, int16_t y, const char* str, uint16_t color) {
  int16_t cursorX = x;
  int16_t cursorY = y;

  while (*str) {
    uint32_t code = utf8_next(&str);  // decode next Unicode codepoint
    drawSparseChar(font, cursorX, cursorY, code, color);

    const GFXglyph* glyph = findGlyph(font, code);
    if (glyph) {
      cursorX += pgm_read_byte(&glyph->xAdvance);
    } else {
      cursorX += font->yAdvance / 2;
    }
  }
}

void drawSparseStringCentered(const SparseGFXfont* font,
                              int16_t centerX, int16_t y, const char* str, uint16_t color) {

  int16_t textWidth = getSparseStringWidth(font, str);
  int16_t startX = centerX - (textWidth / 2);

  drawSparseString(font, startX, y, str, color);
}
