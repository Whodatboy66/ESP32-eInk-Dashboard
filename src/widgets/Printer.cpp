// The printer widget (layout id 64, stage 7b own code): a Bambu printer's status while printing and
// shortly after (src/services/Bambu.cpp). Three rows: name, layers, progress, bed and nozzle
// temperature; the fans and the AMS climate; time left, state and the AMS spools.
#include "../../app.h"
#include "../services/Bambu.h"
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

// An icon with its value right of it.
void iconValue(uint32_t icon, int x, int y, const char* value) {
  drawSparseChar(&MDI_22_Sparse, x, y + 2, icon, GxEPD_BLACK);
  drawSparseString(&epaperFont, x + 25, y, value, GxEPD_BLACK);
}

// The first AMS with spools (more than one AMS: only the first is shown, there's room for one).
const bambu::Ams* firstAms() {
  for (int i = 0; i < printer.amsCount; i++)
    if (printer.ams[i].trayCount > 0) return &printer.ams[i];
  return nullptr;
}

}  // namespace

void bambuWidget(LayoutItem* item) {
  char buf[40];
  const int x0 = item->PosX + 5;
  int y = item->PosY + item->RowHeight / 2 + 5;
  const bambu::Ams* ams = firstAms();

  // Row 1: name, layers, progress, bed, nozzle.
  drawSparseString(&epaperFont, x0, y, item->Option, GxEPD_BLACK);
  snprintf(buf, sizeof(buf), "%d/%d", printer.layer_num, printer.total_layer_num);
  iconValue(MDI_LAYERS, x0 + 65, y, buf);
  snprintf(buf, sizeof(buf), tr(i18n::S::PrinterDone), printer.percent);
  drawSparseString(&epaperFont, x0 + 180, y, buf, GxEPD_BLACK);
  snprintf(buf, sizeof(buf), "%.0f°", printer.bed_temp);
  iconValue(MDI_BED, x0 + 255, y, buf);
  snprintf(buf, sizeof(buf), "%.0f°", printer.nozzle_temp);
  iconValue(MDI_NOZZLE, x0 + 330, y, buf);

  // Row 2: cooling, exhaust and aux fan, the AMS's humidity and temperature.
  y += item->RowHeight;
  const struct {
    uint32_t icon;
    int raw;
  } fans[] = {{MDI_FAN_COOLING, printer.cooling_fan_speed}, {MDI_FAN_EXHAUST, printer.big_fan2_speed},
              {MDI_FAN_AUX, printer.big_fan1_speed}};
  for (int i = 0; i < 3; i++) {
    snprintf(buf, sizeof(buf), "%d%%", bambu::fanPercent(fans[i].raw));
    iconValue(fans[i].icon, x0 + 70 * i, y, buf);
  }
  if (ams) {
    snprintf(buf, sizeof(buf), "%d%%", ams->humidity);
    iconValue(MDI_HUMIDITY, x0 + 210, y, buf);
    snprintf(buf, sizeof(buf), "%.1f°", ams->temp);
    i18n::localizeNumber(buf, ui_lang);
    iconValue(MDI_THERMOMETER, x0 + 280, y, buf);
  }

  // Row 3: time left and the state; the spools of the AMS with their fill level.
  y += item->RowHeight;
  snprintf(buf, sizeof(buf), "%02u:%02u", printer.remainTime / 60, printer.remainTime % 60);
  drawSparseChar(&MDI_22_Sparse, item->PosX, y + 2, MDI_CLOCK, GxEPD_BLACK);
  drawSparseString(&epaperFont, item->PosX + 30, y, buf, GxEPD_BLACK);
  drawSparseString(&epaperFont, item->PosX, y + 20, stateText(printer.gcode_state), GxEPD_BLACK);
  if (!ams) return;
  drawSparseString(&epaperFont, item->PosX + 90, item->PosY + 59, "AMS", GxEPD_BLACK);
  for (int j = 0; j < ams->trayCount; j++) {
    const bambu::Tray& t = ams->trays[j];
    int cx = item->PosX + item->ColWidth * j + 160;
    drawSparseChar(&MDI_22_Sparse, cx - 10, y + 4, MDI_SPOOL, GxEPD_BLACK);
    snprintf(buf, sizeof(buf), "%d", j + 1);
    drawSparseStringCentered(&epaperFont, cx, y - 1, buf, GxEPD_BLACK);
    if (t.remain >= 0) snprintf(buf, sizeof(buf), "%d%%", t.remain);
    else snprintf(buf, sizeof(buf), "%s", t.tray_type[0] ? "--" : tr(i18n::S::PrinterEmpty));
    drawSparseStringCentered(&epaperFont, cx, y + item->RowHeight, buf, GxEPD_BLACK);
  }
}
