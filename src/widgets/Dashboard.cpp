#include "Dashboard.h"

#include <time.h>

#include "../core/AppState.h"
#include "../services/Bambu.h"
#include "../ui/Canvas.h"
#include "Clock.h"
#include "MonthView.h"
#include "StatusBar.h"
#include "Widgets.h"

void drawStaleMark(LayoutItem* item, Service service, bool bottom) {
  if (!item) return;
  time_t interval = item->Refresh > 0 ? (time_t)item->Refresh * 60 : 3600;
  if (!isStale(serviceStates[(int)service], time(nullptr), interval)) return;
  // Short, in a corner the widget leaves free (also in Classic's small places).
  const char* text = "old";
  int16_t w = getSparseStringWidth(&epaperFont, text);
  int16_t x = item->PosX + item->Width - w - 4;
  int16_t y = bottom ? item->PosY + item->Height - 6 : item->PosY + 16;
  // A small black badge: clearly part of this widget, also right next to a neighbor's text.
  canvas().fillRect(x - 3, y - 15, w + 6, 19, GxEPD_BLACK);
  drawSparseString(&epaperFont, x, y, text, GxEPD_WHITE);
}

void drawWidget(LayoutItem* item, void (*draw)(LayoutItem*)) {
  if (!item) return;
  setCanvasClip(item->PosX, item->PosY, item->Width, item->Height);
  draw(item);
  clearCanvasClip();
}

void drawFullScreen(const FullScreenParts& p) {
  if (p.printer) drawWidget(getLayout(64), p.printView ? bambuWidget : makerWorldWidget);
  drawWidget(getLayout(1), updateClock);
  if (p.weather) drawWidget(getLayout(8), weatherWidget);
  drawWidget(getLayout(128), drawStatus);
  if (p.agenda) drawWidget(getLayout(2), gCalWidget);
  if (p.stocks) drawWidget(getLayout(4), stockWidget);
  if (p.month) drawWidget(getLayout(256), drawCalendar);
  if (p.tracking) drawWidget(getLayout(16), trackingWidget);
}
