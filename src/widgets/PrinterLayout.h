#pragma once
// The printer box (stage 8 part B, docs/plans/STAGE8_DATA_AND_SCREENS.md task B2): its design per box
// size, like the clock and the month view, and the filament colours as words and swatches (the
// screen is black and white). Plain C++, tested on the PC (tests/host/test_printer_layout.cpp);
// Printer.cpp draws into these areas.
#include <cstdint>

#include <cstdio>

#include "../i18n/Strings.h"
#include "../services/PrinterReport.h"

namespace prnlayout {

enum class Design : uint8_t { Compact, Panel, Wide };
// "■ PLA 80%" / brand, fill bar, level + colour / brand, bar, level, ■ colour (narrow cells with room)
enum class CellStyle : uint8_t { OneLine, ThreeLines, FourLines };

struct Area {
  int x = 0, y = 0, w = 0, h = 0;  // relative to the box; h == 0: not shown
};

struct Geometry {
  Design design = Design::Compact;
  Area head;      // name, state (and while printing: "42 % · 1:35" in Compact)
  Area progress;  // printing: the percentage, time left and bar (Compact: the bar only)
  Area temps;     // bed, nozzle, the AMS's humidity
  Area spools;    // amsRows rows of four spool cells
  Area note;      // "+2 AMS" when not every AMS fits
  Area msg;       // the printer's message (HMS), where a line is left
  CellStyle cell = CellStyle::OneLine;
  int cellW = 0, rowH = 0, amsRows = 0;
};

constexpr int LINE = 24;        // a text line (epaperFont)
constexpr int BAR = 10;         // the progress bar
constexpr int BIG = 52;         // percentage and time left (Heading_22) with the bar under them
constexpr int ROW_ONE = 24;     // a row of one-line spool cells
constexpr int ROW_THREE = 60;   // a row of three-line spool cells
constexpr int ROW_FOUR = 80;    // a row of four-line spool cells
constexpr int NARROW_CELL = 130;  // narrower cells put the colour on a line of its own
constexpr int WIDE_LEFT = 260;  // Wide: the state column

// The spool rows in a column from y to h: three-line cells when every AMS fits that way, else one-line
// cells; when even those don't fit, one line less for the "+N AMS" note.
inline void placeSpools(Geometry& g, int x, int y, int w, int h, int ams) {
  if (ams <= 0) return;
  const int avail = h - y;
  g.cellW = w / 4;
  if (g.cellW < NARROW_CELL && avail >= ams * ROW_FOUR) {
    g.rowH = ROW_FOUR;
    g.cell = CellStyle::FourLines;
  } else if (avail >= ams * ROW_THREE) {
    g.rowH = ROW_THREE;
    g.cell = CellStyle::ThreeLines;
  } else {
    g.rowH = ROW_ONE;
    g.cell = CellStyle::OneLine;
  }
  int rows = avail / g.rowH;
  if (rows < ams) {
    rows = (avail - LINE) / g.rowH;
    if (rows < 1) rows = 1;
  }
  g.amsRows = rows < ams ? rows : ams;
  g.spools = Area{x, y, w, g.amsRows * g.rowH};
  if (g.amsRows < ams && y + g.spools.h + LINE <= h) g.note = Area{x, y + g.spools.h, w, LINE};
}

// The design for a box of w x h px, with `ams` AMS units, printing or idle.
inline Geometry layoutFor(int w, int h, int ams, bool printing) {
  Geometry g;
  if (w >= 600 && h < 290) {
    g.design = Design::Wide;
    g.head = Area{0, 0, WIDE_LEFT, LINE};
    int y = LINE + 2;
    if (printing) {
      g.progress = Area{0, y, WIDE_LEFT, BIG};
      y += BIG + 4;
    }
    g.temps = Area{0, y, WIDE_LEFT, LINE};
    placeSpools(g, WIDE_LEFT + 10, 0, w - WIDE_LEFT - 10, h, ams);
    return g;
  }
  g.design = h >= 290 ? Design::Panel : Design::Compact;
  g.head = Area{0, 0, w, LINE};
  int y = LINE + 2;
  if (printing) {
    const int ph = g.design == Design::Panel ? BIG : BAR;
    g.progress = Area{0, y, w, ph};
    y += ph + 4;
  }
  g.temps = Area{0, y, w, LINE};
  y += LINE + 4;
  placeSpools(g, 0, y, w, h, ams);
  if (g.design == Design::Compact) {  // one-line cells in the low box, whatever fits
    g.cell = CellStyle::OneLine;
  }
  // A line for the printer's message below everything (tall boxes).
  const int used = g.note.h ? g.note.y + g.note.h : g.spools.h ? g.spools.y + g.spools.h : y;
  if (g.design == Design::Panel && used + 4 + LINE <= h) g.msg = Area{0, used + 4, w, LINE};
  return g;
}

// ---- The idle view and the printer's messages (task B3) ----

constexpr uint8_t IDLE_MAKERWORLD = 0;  // setting prn_idle: MakerWorld's counters when an id is set
constexpr uint8_t IDLE_PRINTER = 1;     // the printer's own view (temperatures, AMS)

inline bool showIdlePrinter(bool hasMakerWorldId, uint8_t prnIdle) { return !hasMakerWorldId || prnIdle == IDLE_PRINTER; }

// "Meldung (AMS): 0700_2000_0002_0001 +1": the first message's unit (AMS units are module 0x07, the
// AMS lite 0x12) and its code, as the printer and Bambu's wiki show it; "" without messages. No text
// table: the codes' meanings aren't in the firmware (owner: look them up on the printer).
inline void hmsLine(const bambu::Hms* list, int count, i18n::Lang lang, char* out, size_t cap) {
  if (cap) out[0] = '\0';
  if (!list || count <= 0) return;
  char code[24];
  bambu::hmsCode(list[0], code, sizeof(code));
  const unsigned module = list[0].attr >> 24;
  const char* unit = module == 0x07 ? "AMS" : module == 0x12 ? "AMS lite" : nullptr;
  char more[16] = "";
  if (count > 1) std::snprintf(more, sizeof(more), " +%d", count - 1);
  if (unit)
    std::snprintf(out, cap, "%s (%s): %s%s", i18n::text(i18n::S::PrinterMessage, lang), unit, code, more);
  else
    std::snprintf(out, cap, "%s: %s%s", i18n::text(i18n::S::PrinterMessage, lang), code, more);
}

// ---- Filament colours ----

enum class Swatch : uint8_t { Filled, Hatched, Empty };

inline void rgbOf(uint32_t rgba, int& r, int& g, int& b) {
  r = (rgba >> 24) & 0xFF;
  g = (rgba >> 16) & 0xFF;
  b = (rgba >> 8) & 0xFF;
}

// Dark colours filled, middle ones hatched, light ones an outline (by luminance).
inline Swatch swatchFor(uint32_t rgba) {
  int r, g, b;
  rgbOf(rgba, r, g, b);
  const float lum = (0.2126f * r + 0.7152f * g + 0.0722f * b) / 255.0f;
  return lum < 0.35f ? Swatch::Filled : lum < 0.7f ? Swatch::Hatched : Swatch::Empty;
}

// The nearest colour word (12): grey shades by brightness, colours by hue; "" for none (0).
inline const char* colorName(uint32_t rgba, i18n::Lang lang) {
  using i18n::S;
  if (!rgba) return "";
  int r, g, b;
  rgbOf(rgba, r, g, b);
  const int mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
  const int mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
  const float sat = mx ? float(mx - mn) / mx : 0.0f, val = mx / 255.0f;
  S name;
  if (sat < 0.18f) {
    const float lum = (0.2126f * r + 0.7152f * g + 0.0722f * b) / 255.0f;
    name = lum < 0.2f ? S::ColBlack : lum > 0.85f ? S::ColWhite : S::ColGrey;
  } else {
    const float d = float(mx - mn);
    float hue = mx == r ? 60.0f * ((g - b) / d) : mx == g ? 60.0f * (2.0f + (b - r) / d) : 60.0f * (4.0f + (r - g) / d);
    if (hue < 0) hue += 360.0f;
    if (hue < 15 || hue >= 345) name = val < 0.45f ? S::ColBrown : S::ColRed;
    else if (hue < 40) name = val < 0.55f ? S::ColBrown : sat < 0.35f ? S::ColBeige : S::ColOrange;
    else if (hue < 70) name = sat < 0.35f ? S::ColBeige : S::ColYellow;
    else if (hue < 170) name = S::ColGreen;
    else if (hue < 260) name = S::ColBlue;
    else if (hue < 300) name = S::ColViolet;
    else name = S::ColPink;
  }
  return i18n::text(name, lang);
}

}  // namespace prnlayout
