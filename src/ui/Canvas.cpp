#include "Canvas.h"

#ifndef RENDER_ON_PC
#include "../hardware/EpaperDriver.h"
#endif

namespace {

Adafruit_GFX* target = nullptr;

// Forwards drawing to the real target, dropping everything outside the clip rectangle. Filled
// rectangles and lines are clipped and forwarded as such (fast on the display); everything else
// (text, circles, ...) arrives here pixel by pixel through Adafruit_GFX's defaults.
class ClipCanvas : public Adafruit_GFX {
public:
  ClipCanvas() : Adafruit_GFX(800, 480) {}
  Adafruit_GFX* out = nullptr;
  int16_t cx = 0, cy = 0, cw = 0, ch = 0;
  bool active = false;

  void drawPixel(int16_t x, int16_t y, uint16_t c) override {
    if (x >= cx && y >= cy && x < cx + cw && y < cy + ch) out->drawPixel(x, y, c);
  }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) override {
    int16_t x2 = x + w, y2 = y + h;
    if (x < cx) x = cx;
    if (y < cy) y = cy;
    if (x2 > cx + cw) x2 = cx + cw;
    if (y2 > cy + ch) y2 = cy + ch;
    if (x2 > x && y2 > y) out->fillRect(x, y, x2 - x, y2 - y, c);
  }
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) override { fillRect(x, y, w, 1, c); }
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) override { fillRect(x, y, 1, h, c); }
  void fillScreen(uint16_t c) override { fillRect(0, 0, _width, _height, c); }
};

ClipCanvas clip;

Adafruit_GFX& base() {
#ifdef RENDER_ON_PC
  return *target;
#else
  return target ? *target : getDisplay();
#endif
}

}  // namespace

Adafruit_GFX& canvas() {
  if (clip.active) return clip;
  return base();
}

void setCanvas(Adafruit_GFX* t) {
  target = t;
}

void setCanvasClip(int16_t x, int16_t y, int16_t w, int16_t h) {
  clip.out = &base();
  clip.cx = x;
  clip.cy = y;
  clip.cw = w;
  clip.ch = h;
  clip.active = true;  // an empty rectangle draws nothing (not: unclipped)
}

void clearCanvasClip() {
  clip.active = false;
}
