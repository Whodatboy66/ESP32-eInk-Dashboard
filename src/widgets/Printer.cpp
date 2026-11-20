// Printer widget (layout ID 64): status of the Bambu printer while printing (and shortly after),
// from the data src/services/Bambu.cpp receives over MQTT.
#include <math.h>

#include "../../app.h"
#include "../services/Bambu.h"
#include "Widgets.h"

static inline int fanPercent(int raw) {
  return (int)(ceil(raw * 10.0f / 15.0f) * 10);
}

// The printer's state word ("RUNNING", ...) in the screen language; unknown ones as they come.
static const char* printerState(const char* state) {
  using i18n::S;
  static const struct { const char* word; S text; } STATES[] = {
      {"RUNNING", S::PrinterRunning}, {"PREPARE", S::PrinterPrepare}, {"PAUSE", S::PrinterPause},
      {"FINISH", S::PrinterFinish},   {"FAILED", S::PrinterFailed},   {"IDLE", S::PrinterIdle}};
  for (const auto& s : STATES)
    if (!strcmp(state, s.word)) return tr(s.text);
  return state;
}

void drawFanStat(uint32_t icon, int x, int y, int rawSpeed) {
  char buf[8];
  sprintf(buf, "%d%%", fanPercent(rawSpeed));
  drawSparseChar(&MDI_22_Sparse, x, y+2, icon, GxEPD_BLACK);
  drawSparseString(&epaperFont, x+25, y, buf, GxEPD_BLACK);
}

void bambuWidget(LayoutItem* item)
{
  DBG( F("[Bambu] UPDATING......") );
  char buf[40];
  int margin_x=5;
  int baseX = item->PosX + margin_x;
  int X_POS = baseX;  // was uninitialized, so the first row was drawn at a random X
  int baseY = item->PosY;
  int y = baseY + item->RowHeight/2 + 5;

  drawSparseString(&epaperFont, baseX, y, item->Extra1, GxEPD_BLACK );

  // Layers
  X_POS += 65;
  drawSparseChar(&MDI_22_Sparse, X_POS, y+2, MDI_LAYERS, GxEPD_BLACK);
  sprintf(buf, "%d/%d", printer.layer_num , printer.total_layer_num );  

  X_POS+=25;
  drawSparseString(&epaperFont, X_POS, y, buf,GxEPD_BLACK );
  
  // Completed
  X_POS+=90;
  snprintf(buf, sizeof(buf), tr(i18n::S::PrinterDone), printer.percent );  
  drawSparseString(&epaperFont, X_POS, y, buf,GxEPD_BLACK );

  // Bed Temperature
  X_POS+=75;

  sprintf(buf, "%.0f°", printer.bed_temp );  
  drawSparseString(&epaperFont, X_POS+25, y, buf,GxEPD_BLACK );
  drawSparseChar(&MDI_22_Sparse, X_POS, y+2, MDI_BED, GxEPD_BLACK);

  // Noozle
   X_POS+=75;
  sprintf(buf, "%.0f°", printer.nozzle_temp );  
  drawSparseString(&epaperFont, X_POS+25, y, buf,GxEPD_BLACK );
  drawSparseChar(&MDI_22_Sparse, X_POS, y+2, MDI_NOZZLE, GxEPD_BLACK);

  y+=item->RowHeight;
  X_POS=baseX;
  drawFanStat(MDI_FAN_COOLING, X_POS,      y, printer.cooling_fan_speed);

  //Exhaust Fan
  X_POS+=70;
  drawFanStat(MDI_FAN_EXHAUST, X_POS,   y, printer.big_fan2_speed);

  //Right Aux
  X_POS+=70;
  drawFanStat(MDI_FAN_AUX, X_POS,  y, printer.big_fan1_speed);

  // Print info 1st AMS if present
  if( printer.ams[0].trayCount>0 )
  {
    DBG( F("Tray") );
    X_POS+=70;
    // AMS Humidity
    sprintf(buf, "%d%%", printer.ams[0].humidity ); 
    drawSparseString(&epaperFont, X_POS+25, y, buf, GxEPD_BLACK);
    drawSparseChar(&MDI_22_Sparse, X_POS, y+2, MDI_HUMIDITY, GxEPD_BLACK);

    // AMS Temperature
    X_POS+=70;
    sprintf(buf, "%.1f°", printer.ams[0].temp );
    i18n::localizeNumber(buf, ui_lang);
    drawSparseString(&epaperFont, X_POS+25, y, buf, GxEPD_BLACK );
    drawSparseChar(&MDI_22_Sparse, X_POS, y+2, MDI_THERMOMETER, GxEPD_BLACK);

  }

  y+=item->RowHeight;
  //Clock
  drawSparseChar(&MDI_22_Sparse,item->PosX, y+2, MDI_CLOCK, GxEPD_BLACK);
  sprintf(buf, "%02d:%02d", printer.remainTime/60, printer.remainTime%60 ); 
  drawSparseString(&epaperFont, item->PosX+30, y, buf,GxEPD_BLACK );

  drawSparseString(&epaperFont, item->PosX, y+20, printerState(printer.gcode_state), GxEPD_BLACK );

  for (int i = 0; i < printer.amsCount; i++) {

    if( printer.ams[i].trayCount>0 )
    {
      drawSparseString(&epaperFont, item->PosX+90, item->PosY+59, "AMS",GxEPD_BLACK );

      int j = 0;
      for (; j < printer.ams[i].trayCount; j++) {
        Tray& tray = printer.ams[i].trays[j];

        drawSparseChar(&MDI_22_Sparse, item->PosX+item->ColWidth*j+150, y+4, MDI_SPOOL, GxEPD_BLACK);
        sprintf(buf, "%d", j+1);  
        drawSparseStringCentered(&epaperFont, item->PosX+item->ColWidth*j+160, y-1, buf,GxEPD_BLACK );
     
        if (tray.remain >= 0) {
          sprintf(buf, "%d%%", tray.remain);
          drawSparseStringCentered(&epaperFont, item->PosX+item->ColWidth*j+160, y+item->RowHeight, buf, GxEPD_BLACK);
        } else if (tray.tray_type[0] != '\0') {
          drawSparseStringCentered(&epaperFont, item->PosX+item->ColWidth*j+160, y+item->RowHeight, "--", GxEPD_BLACK);
        } else {
          drawSparseStringCentered(&epaperFont, item->PosX+item->ColWidth*j+160, y+item->RowHeight, tr(i18n::S::PrinterEmpty), GxEPD_BLACK);
        }
      }
    }
  }

}
