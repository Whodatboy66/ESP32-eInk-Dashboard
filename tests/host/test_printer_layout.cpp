// Host test for src/widgets/PrinterLayout.h (stage 8 part B): the printer box's design per size, the
// filament colour names and swatches. Run: tests/host/run.sh
#include <initializer_list>

#include "src/widgets/PrinterLayout.h"
#include "tests/host/check.h"

namespace {

using namespace prnlayout;

bool inside(const Area& a, int w, int h) { return a.h == 0 || (a.x >= 0 && a.y >= 0 && a.x + a.w <= w && a.y + a.h <= h); }
bool apart(const Area& a, const Area& b) {
  if (!a.h || !b.h) return true;
  return a.y + a.h <= b.y || b.y + b.h <= a.y || a.x + a.w <= b.x || b.x + b.w <= a.x;
}

void checkGeometry(int w, int h, int ams, bool printing) {
  const Geometry g = layoutFor(w, h, ams, printing);
  const Area parts[] = {g.head, g.progress, g.temps, g.spools, g.note, g.msg};
  for (const Area& a : parts) CHECK(inside(a, w, h));
  for (int i = 0; i < 6; i++)
    for (int j = i + 1; j < 6; j++) CHECK(apart(parts[i], parts[j]));
  CHECK(g.amsRows >= 1 || ams == 0);
  CHECK(g.cellW * 4 <= g.spools.w || g.spools.h == 0);  // four spools side by side
}

void test_designs() {
  CHECK(layoutFor(400, 150, 2, true).design == Design::Compact);
  CHECK(layoutFor(400, 300, 2, true).design == Design::Panel);
  CHECK(layoutFor(800, 150, 2, true).design == Design::Wide);
  CHECK(layoutFor(400, 450, 2, false).design == Design::Panel);
  // Compact: both AMS as one-line spool cells; Panel: three-line cells (brand, bar, level + colour).
  CHECK_EQ(layoutFor(400, 150, 2, true).amsRows, 2);
  CHECK(layoutFor(400, 150, 2, true).cell == CellStyle::OneLine);
  CHECK(layoutFor(400, 300, 2, true).cell == CellStyle::FourLines);  // narrow cells: colour on its own line
  CHECK(layoutFor(800, 150, 2, true).cell == CellStyle::ThreeLines);  // wide cells: level and colour together
  CHECK_EQ(layoutFor(400, 300, 2, true).amsRows, 2);
  // More AMS than rows fit: "+N AMS" in the note line.
  CHECK(layoutFor(400, 150, 4, true).amsRows < 4);
  // Idle: no progress block.
  CHECK_EQ(layoutFor(400, 300, 2, false).progress.h, 0);
  for (int w : {400, 600, 800})
    for (int h : {150, 300, 450})
      for (int ams : {0, 1, 2, 4})
        for (bool printing : {true, false}) checkGeometry(w, h, ams, printing);
}

void test_colour_names() {
  using i18n::Lang;
  CHECK_STR(colorName(0x000000FF, Lang::De), "Schwarz");
  CHECK_STR(colorName(0xFFFFFFFF, Lang::De), "Weiß");
  CHECK_STR(colorName(0xFF6A13FF, Lang::De), "Orange");
  CHECK_STR(colorName(0x0A2989FF, Lang::En), "Blue");
  CHECK_STR(colorName(0x898989FF, Lang::De), "Grau");
  CHECK_STR(colorName(0xC12E1FFF, Lang::De), "Rot");
  CHECK_STR(colorName(0x00AE42FF, Lang::De), "Grün");
  CHECK_STR(colorName(0, Lang::De), "");  // none
  CHECK(swatchFor(0x000000FF) == Swatch::Filled);
  CHECK(swatchFor(0x0A2989FF) == Swatch::Filled);   // dark blue
  CHECK(swatchFor(0xFF6A13FF) == Swatch::Hatched);  // orange: mid
  CHECK(swatchFor(0xFFFFFFFF) == Swatch::Empty);
}

// The idle view: MakerWorld when an id is set and the setting says so (default), else the printer.
void test_idle_view() {
  CHECK(!showIdlePrinter(true, IDLE_MAKERWORLD));
  CHECK(showIdlePrinter(true, IDLE_PRINTER));
  CHECK(showIdlePrinter(false, IDLE_MAKERWORLD));  // no id: always the printer
}

// The printer's messages (HMS): the unit and the code (no text table: see the printer / Bambu's wiki).
void test_hms_line() {
  using i18n::Lang;
  bambu::Hms ams{0x07002000u, 0x00020001u}, other{0x0C000300u, 0x00020001u};
  char line[80];
  hmsLine(&ams, 1, Lang::De, line, sizeof(line));
  CHECK_STR(line, "Meldung (AMS): 0700_2000_0002_0001");
  bambu::Hms two[2] = {other, ams};
  hmsLine(two, 2, Lang::En, line, sizeof(line));
  CHECK_STR(line, "Message: 0C00_0300_0002_0001 +1");
  hmsLine(nullptr, 0, Lang::De, line, sizeof(line));
  CHECK_STR(line, "");
  // A tall box has a line for it; the low one doesn't.
  CHECK(layoutFor(400, 450, 2, true).msg.h > 0);
  CHECK_EQ(layoutFor(400, 150, 2, true).msg.h, 0);
}

}  // namespace

int main() {
  test_idle_view();
  test_hms_line();
  test_designs();
  test_colour_names();
  return finish("printer layout");
}
