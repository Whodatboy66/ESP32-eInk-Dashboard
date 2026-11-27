// The printer widget (layout id 64): a Bambu printer's status (src/services/Bambu.cpp), drawn in the
// design for its box size (stage 8, PrinterLayout.h): state and progress, temperatures, every AMS.
#include "../../app.h"
#include "../services/Bambu.h"
#include "Personal.h"
#include "PrinterLayout.h"
#include "Widgets.h"

namespace {

// The printer's state word in the screen language; unknown ones as they come.
const char* stateText(const char* state) {
  using i18n::S;
  static const struct {
    const char* word;
    S text;
  } STATES[] = {{"RUNNING", S::PrinterRunning}, {"PREPARE", S::PrinterPrepare}, {"PAUSE", S::PrinterPause},
                {"FINISH", S::PrinterFinish},   {"FAILED", S::PrinterFailed},   {"IDLE", S::PrinterIdle}};
  for (const auto& s : STATES)
    if (!strcmp(state, s.word)) return tr(s.text);
  return state;
}

// An icon with its value right of it; returns the x after it.
int iconValue(uint32_t icon, int x, int y, const char* value) {
  drawSparseChar(&MDI_22_Sparse, x, y + 2, icon, GxEPD_BLACK);
  drawSparseString(&epaperFont, x + 25, y, value, GxEPD_BLACK);
  return x + 25 + getSparseStringWidth(&epaperFont, value);
}

int textW(const char* s) { return getSparseStringWidth(&epaperFont, s); }

// A filament swatch, 11 x 11 at (x, y): dark colours filled, middle ones hatched, light ones an outline.
void drawSwatch(int x, int y, uint32_t color) {
  if (!color) return;
  canvas().drawRect(x, y, 11, 11, GxEPD_BLACK);
  switch (prnlayout::swatchFor(color)) {
    case prnlayout::Swatch::Filled: canvas().fillRect(x, y, 11, 11, GxEPD_BLACK); break;
    case prnlayout::Swatch::Hatched:
      for (int d = 2; d < 22; d += 3)
        for (int i = 0; i <= d; i++)
          if (i < 11 && d - i < 11) canvas().drawPixel(x + i, y + d - i, GxEPD_BLACK);
      break;
    case prnlayout::Swatch::Empty: break;
  }
}

// A bar of w x h at (x, y), filled to pct (outline only when unknown).
void drawBar(int x, int y, int w, int h, int pct) {
  canvas().drawRect(x, y, w, h, GxEPD_BLACK);
  if (pct > 0) canvas().fillRect(x, y, (w * (pct > 100 ? 100 : pct)) / 100, h, GxEPD_BLACK);
}

// "80 %", "?" (no RFID), "Leer" (empty slot).
void levelText(const bambu::Tray& t, char* out, size_t cap) {
  if (!t.tray_type[0]) snprintf(out, cap, "%s", tr(i18n::S::PrinterSlotEmpty));
  else if (t.remain >= 0) snprintf(out, cap, "%d %%", t.remain);
  else snprintf(out, cap, "?");
}

// One spool cell at (x, y), w wide: "■ PLA 80 %" in one line, or brand / fill bar / level and colour.
void drawSpool(const bambu::Tray& t, int x, int y, int w, prnlayout::CellStyle style) {
  char level[16], line[48];
  levelText(t, level, sizeof(level));
  const char* kind = t.brand[0] ? t.brand : t.tray_type;
  if (style == prnlayout::CellStyle::OneLine) {
    drawSwatch(x, y + 5, t.color);
    if (t.remain >= 0) snprintf(level, sizeof(level), "%d%%", t.remain);  // no space: "PLA 80%" fits 100 px
    snprintf(line, sizeof(line), "%s %s", t.tray_type[0] ? t.tray_type : "", level);
    drawFittedString(x + 15, y + 17, w - 19, line, GxEPD_BLACK);
    return;
  }
  if (style == prnlayout::CellStyle::FourLines) {  // brand / bar / level / swatch and colour
    drawFittedString(x, y + 17, w - 6, t.tray_type[0] ? kind : level, GxEPD_BLACK);
    if (!t.tray_type[0]) return;
    drawBar(x, y + 24, w - 10, 7, t.remain);
    drawSparseString(&epaperFont, x, y + 50, level, GxEPD_BLACK);
    drawSwatch(x, y + 61, t.color);
    drawFittedString(x + 15, y + 73, w - 21, prnlayout::colorName(t.color, ui_lang), GxEPD_BLACK);
    return;
  }
  drawSwatch(x, y + 5, t.color);
  drawFittedString(x + 15, y + 17, w - 19, t.tray_type[0] ? kind : level, GxEPD_BLACK);
  if (!t.tray_type[0]) return;
  drawBar(x, y + 24, w - 10, 7, t.remain);
  snprintf(line, sizeof(line), "%s  %s", level, prnlayout::colorName(t.color, ui_lang));
  drawFittedString(x, y + 50, w - 6, line, GxEPD_BLACK);
}

}  // namespace

// The printer box (stage 8 part B): a design per box size (PrinterLayout.h). Printing: the state,
// percentage, time left and a progress bar; idle: the state. Always: bed and nozzle temperature, the
// AMS climate and every AMS's spools with type / brand, fill level and colour.
void bambuWidget(LayoutItem* item) {
  if (!item) return;
  if (printerOff) {  // switched off and no neighbour took the box: say so calmly (no warning sign)
    const char* name = item->Option[0] ? item->Option : tr(i18n::S::WPrinter);
    drawSparseString(&Heading_22, item->PosX, item->PosY + 19, name, GxEPD_BLACK);
    drawSparseString(&epaperFont, item->PosX + getSparseStringWidth(&Heading_22, name) + 10, item->PosY + 17,
                     tr(i18n::S::PrinterOff), GxEPD_BLACK);
    return;
  }
  const bool printing = showPrintView();
  const bambu::Ams* units[bambu::MAX_AMS];
  int ams = 0;
  for (int i = 0; i < printer.amsCount; i++)
    if (printer.ams[i].trayCount > 0) units[ams++] = &printer.ams[i];
  const prnlayout::Geometry g = prnlayout::layoutFor(item->Width, item->Height, ams, printing);
  const int X = item->PosX, Y = item->PosY;
  char buf[48];

  // Head: name and state; in the low box while printing also "42 % · 1:35" on the right.
  int base = Y + g.head.y + 17;
  const char* name = item->Option[0] ? item->Option : tr(i18n::S::WPrinter);
  drawSparseString(&Heading_22, X + g.head.x, base + 2, name, GxEPD_BLACK);
  int x = X + g.head.x + getSparseStringWidth(&Heading_22, name) + 10;
  char state[24];
  if (printer.hmsCount && !g.msg.h) snprintf(state, sizeof(state), "%s (%d)", tr(i18n::S::PrinterMessage), printer.hmsCount);
  else snprintf(state, sizeof(state), "%s", stateText(printer.gcode_state));
  drawSparseString(&epaperFont, x, base, state, GxEPD_BLACK);
  if (printing && g.design == prnlayout::Design::Compact) {
    snprintf(buf, sizeof(buf), "%d %%  %u:%02u", printer.percent, printer.remainTime / 60, printer.remainTime % 60);
    drawSparseString(&epaperFont, X + g.head.x + g.head.w - textW(buf) - 4, base, buf, GxEPD_BLACK);
  }

  // Progress: the bar (low box), or percentage, layers and time left above it.
  if (g.progress.h) {
    const int px = X + g.progress.x, py = Y + g.progress.y, pw = g.progress.w - 6;
    if (g.design == prnlayout::Design::Compact) {
      drawBar(px, py, pw, prnlayout::BAR, printer.percent);
    } else {
      snprintf(buf, sizeof(buf), "%d", printer.percent);  // the big digits have no "%": that in the text font
      drawSparseString(&DigitsBold_22, px, py + 24, buf, GxEPD_BLACK);
      int lx = px + getSparseStringWidth(&DigitsBold_22, buf) + 3;
      drawSparseString(&epaperFont, lx, py + 24, "%", GxEPD_BLACK);
      lx += textW("%") + 14;
      snprintf(buf, sizeof(buf), "%d/%d", printer.layer_num, printer.total_layer_num);
      iconValue(MDI_LAYERS, lx, py + 22, buf);
      snprintf(buf, sizeof(buf), "%u:%02u", printer.remainTime / 60, printer.remainTime % 60);
      const int tw = 25 + textW(buf);
      iconValue(MDI_CLOCK, px + pw - tw, py + 22, buf);
      drawBar(px, py + prnlayout::BIG - prnlayout::BAR - 4, pw, prnlayout::BAR, printer.percent);
    }
  }

  // Temperatures and the first AMS's climate, spread over the line.
  if (g.temps.h) {
    // Bed, nozzle, AMS humidity and temperature (the narrow state column of Wide: without the last).
    const int items = !ams ? 2 : g.design == prnlayout::Design::Wide ? 3 : 4;
    const int ty = Y + g.temps.y + 17, step = (g.temps.w - 4) / items;
    int tx = X + g.temps.x;
    snprintf(buf, sizeof(buf), "%.0f°", printer.bed_temp);
    iconValue(MDI_BED, tx, ty, buf);
    snprintf(buf, sizeof(buf), "%.0f°", printer.nozzle_temp);
    iconValue(MDI_NOZZLE, tx + step, ty, buf);
    if (ams) {
      snprintf(buf, sizeof(buf), "%d %%", units[0]->humidity);
      iconValue(MDI_HUMIDITY, tx + 2 * step, ty, buf);
      if (items == 4) {
        snprintf(buf, sizeof(buf), "%.1f°", units[0]->temp);
        i18n::localizeNumber(buf, ui_lang);
        iconValue(MDI_THERMOMETER, tx + 3 * step, ty, buf);
      }
    }
  }

  // Spools: one row per AMS that fits, four cells each.
  for (int r = 0; r < g.amsRows; r++) {
    const bambu::Ams& unit = *units[r];
    const int ry = Y + g.spools.y + r * g.rowH;
    if (r > 0) canvas().drawFastHLine(X + g.spools.x, ry - 2, g.spools.w - 6, GxEPD_BLACK);
    for (int j = 0; j < unit.trayCount && j < bambu::MAX_TRAYS; j++)
      drawSpool(unit.trays[j], X + g.spools.x + j * g.cellW, ry, g.cellW, g.cell);
  }
  if (g.msg.h && printer.hmsCount) {  // the printer's message: its unit and code (see the printer)
    char msg[80];
    prnlayout::hmsLine(printer.hms, printer.hmsCount, ui_lang, msg, sizeof(msg));
    drawSparseChar(&MDI_22_Sparse, X + g.msg.x, Y + g.msg.y + 19, MDI_ALERT, GxEPD_BLACK);
    drawFittedString(X + g.msg.x + 26, Y + g.msg.y + 17, g.msg.w - 32, msg, GxEPD_BLACK);
  }
  if (g.note.h && ams > g.amsRows) {
    snprintf(buf, sizeof(buf), "+%d AMS", ams - g.amsRows);
    drawSparseString(&epaperFont, X + g.note.x, Y + g.note.y + 17, buf, GxEPD_BLACK);
  }
}
