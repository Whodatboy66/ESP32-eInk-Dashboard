#pragma once
// PC stand-in: the "display" is an in-memory 1-bit canvas; refresh control does nothing.
#include <Adafruit_GFX.h>
#include "GxEPD2.h"
template <typename Panel, int PAGE_HEIGHT>
class GxEPD2_BW : public GFXcanvas1 {
public:
  explicit GxEPD2_BW(Panel) : GFXcanvas1(Panel::WIDTH, Panel::HEIGHT) {}
  void init(unsigned long, bool, int, bool) {}
  void setFullWindow() {}
  void setPartialWindow(int16_t, int16_t, int16_t, int16_t) {}
  void firstPage() {}
  bool nextPage() { return false; }
  void hibernate() {}
  void powerOff() {}
};
