#pragma once
// The e-paper display: panel type, the display object (created on first use with the active pin
// map), power-up / init once per wake, and partial refresh of one layout region. Drawing goes
// through canvas() (src/ui/Canvas.h); this is refresh control.
#include "GxEPD2.h"
#include "../../configure.h"
#include "../ui/Canvas.h"
#include "../ui/Fonts.h"
#include "../ui/SparseFont.h"

#if USE_COLORDISPLAY
  #include <GxEPD2_4C.h>
  #include <epd4c/GxEPD2_750c_GDEM075F52.h>
  using DashboardPanel = GxEPD2_750c_GDEM075F52;
  using DashboardDisplay = GxEPD2_4C<DashboardPanel, DashboardPanel::HEIGHT / 2>;
#else
  #include <GxEPD2_BW.h>
  #include <epd/GxEPD2_750_T7.h>
  using DashboardPanel = GxEPD2_750_T7;
  using DashboardDisplay = GxEPD2_BW<DashboardPanel, DashboardPanel::HEIGHT>;
#endif
#include <Arduino.h>
#include <Adafruit_GFX.h>

DashboardDisplay& getDisplay();

#define display getDisplay()

#if DISPLAY_HAS_COLOR
  #define ALERT_COLOR GxEPD_RED
#else
  #define ALERT_COLOR GxEPD_BLACK
#endif

struct LayoutItem;

extern bool displayReady;
// Powers and initializes the panel once per boot (initialRefresh: full refresh on the first update).
void initDisplay(bool initialRefresh);
// Partial refresh of one layout region, drawn by drawFunc.
void updatePartial(LayoutItem* item, void (*drawFunc)(LayoutItem*));
