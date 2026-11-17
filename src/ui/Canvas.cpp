#include "Canvas.h"

#include "../../display.h"

namespace {
Adafruit_GFX* target = nullptr;
}

Adafruit_GFX& canvas() {
  return target ? *target : getDisplay();
}

void setCanvas(Adafruit_GFX* t) {
  target = t;
}
