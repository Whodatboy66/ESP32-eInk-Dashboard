#pragma once
// The e-paper display (stage 7b, own code): the panel type, its object (made on first use with the
// active pins), power-up and init once per wake, partial refresh of one widget's box. Drawing goes
// through canvas() (src/ui/Canvas.h); this is the refresh control.
#include <Arduino.h>
#include <Adafruit_GFX.h>

#include "GxEPD2.h"
#include "../../configure.h"
#include "../ui/Canvas.h"
#include "../ui/Fonts.h"
#include "../ui/SparseFont.h"

#if USE_COLORDISPLAY
#include <GxEPD2_4C.h>
#include <epd4c/GxEPD2_750c_GDEM075F52.h>
using DashboardPanel = GxEPD2_750c_GDEM075F52;  // 7.5" 4 colours: half the screen per page
using DashboardDisplay = GxEPD2_4C<DashboardPanel, DashboardPanel::HEIGHT / 2>;
#define ALERT_COLOR GxEPD_RED
#else
#include <GxEPD2_BW.h>
#include <epd/GxEPD2_750_T7.h>
using DashboardPanel = GxEPD2_750_T7;  // 7.5" black / white, 800 x 480: the whole screen in one page
using DashboardDisplay = GxEPD2_BW<DashboardPanel, DashboardPanel::HEIGHT>;
#define ALERT_COLOR GxEPD_BLACK
#endif

DashboardDisplay& getDisplay();
#define display getDisplay()

struct LayoutItem;

extern bool displayReady;
// Powers and initializes the panel once per wake (initialRefresh: the first update is a full one).
void initDisplay(bool initialRefresh);
// A partial refresh of one widget's box, drawn by draw.
void updatePartial(LayoutItem* item, void (*draw)(LayoutItem*));
