#pragma once
// Where drawing goes. All drawing code (fonts, widgets, screens) draws on canvas(); refresh
// control (full / partial window, first/nextPage, power) stays on the e-paper driver.
// On the device canvas() is the e-paper display. On the PC it can be an in-memory bitmap
// (Adafruit GFX's GFXcanvas1), which renders the same screens into images: layout tests in
// stage 3 and the phone preview in stage 5 (spec §7.3: one drawing code for both).
#include <Adafruit_GFX.h>

Adafruit_GFX& canvas();

// nullptr = back to the default (the display).
void setCanvas(Adafruit_GFX* target);

// Each widget is clipped to its place: drawing outside this rectangle is dropped until
// clearCanvasClip(). Text that is too long for a small place doesn't spill into the neighbor.
void setCanvasClip(int16_t x, int16_t y, int16_t w, int16_t h);
void clearCanvasClip();
