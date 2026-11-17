#include "Registry.h"

#include <stdio.h>
#include <string.h>

#include "../../app.h"
#include "Widgets.h"

using layouts::WidgetId;

namespace {
// Indexed by WidgetId. Metrics and refresh as in the original fixed layout.
const WidgetInfo WIDGETS[layouts::WIDGET_COUNT] = {
    {WidgetId::StatusBar, "status", "Status bar", 128, "Battery", 0, 0, 60},
    {WidgetId::Clock, "clock", "Clock", 1, "Clock", 10, 10, 1},
    {WidgetId::Weather, "weather", "Weather", 8, "OpenMeteo", 0, 100, 360},
    {WidgetId::Agenda, "agenda", "Upcoming events", 2, "Cal Events", 22, 10, 60},
    {WidgetId::Month, "month", "Month calendar", 256, "Calendar", 28, 210, 60},
    {WidgetId::Stocks, "stocks", "Stocks", 4, "Stocks", 22, 10, 60},
    {WidgetId::Tracking, "tracking", "Package tracking", 16, "Tracking", 22, 0, 120},
    {WidgetId::Printer, "printer", "Printer / MakerWorld", 64, "Bambu", 22, 70, 4},
};
}  // namespace

const WidgetInfo& widgetInfo(WidgetId id) {
  return WIDGETS[static_cast<int>(id)];
}

uint16_t configuredWidgets() {
  uint16_t m = layouts::widgetBit(WidgetId::StatusBar) | layouts::widgetBit(WidgetId::Clock) | layouts::widgetBit(WidgetId::Month);
  if (weather_location[0]) m |= layouts::widgetBit(WidgetId::Weather);
  if (hasIcsFeeds()) m |= layouts::widgetBit(WidgetId::Agenda);
  if (stock_symbols[0]) m |= layouts::widgetBit(WidgetId::Stocks);
  if (pkge_api_key[0]) m |= layouts::widgetBit(WidgetId::Tracking);
  if (mqtt_ip[0]) m |= layouts::widgetBit(WidgetId::Printer);
  return m;
}

uint16_t enabledWidgets() {
  uint16_t always = layouts::widgetBit(WidgetId::StatusBar) | layouts::widgetBit(WidgetId::Clock);
  return configuredWidgets() & (widget_show_mask | always);
}

layouts::Result loadLayout() {
  layouts::Result r = layouts::buildLayout(layouts::templateByKey(layout_template), enabledWidgets());
  layoutCount = 0;
  for (int i = 0; i < r.count && layoutCount < MAXLAYOUTS; i++) {
    const WidgetInfo& w = widgetInfo(r.items[i].id);
    LayoutItem& l = layout[layoutCount++];
    memset(&l, 0, sizeof(l));
    l.ID = w.layoutId;
    l.PosX = r.items[i].rect.x;
    l.PosY = r.items[i].rect.y;
    l.Width = r.items[i].rect.w;
    l.Height = r.items[i].rect.h;
    if (r.items[i].id == WidgetId::StatusBar && r.items[i].rect.x == 0) {
      l.PosX += 4;  // full-width bar: keep text and icons off the panel edge
      l.Width -= 8;
    }
    if (r.items[i].grid) {
      // Grid cells touch each other and the frame: keep a few pixels to the left. (Stocks and the
      // printer view use fixed columns up to 400 px, so this stays small.)
      constexpr int PAD_LEFT = 4;
      l.PosX += PAD_LEFT;
      l.Width -= PAD_LEFT;
    }
    l.RowHeight = w.rowHeight;
    l.ColWidth = w.colWidth;
    l.Refresh = w.refresh;
    l.Active = true;
    snprintf(l.Description, sizeof(l.Description), "%s", w.description);
    // Clock Extra1 left empty: updateClock() then uses the Timezone from the setup page.
    if (r.items[i].id == WidgetId::Printer)
      snprintf(l.Extra1, sizeof(l.Extra1), "%s", printer_name[0] ? printer_name : "Bambu");
  }
  return r;
}
