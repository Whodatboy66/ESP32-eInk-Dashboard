#include "Registry.h"

#include <stdio.h>
#include <string.h>

#include "../../app.h"
#include "../layout/BoxLayout.h"
#include "Widgets.h"

using layouts::WidgetId;

namespace {
// Indexed by WidgetId. Metrics and refresh as in the original fixed layout.
using i18n::S;
const WidgetInfo WIDGETS[layouts::WIDGET_COUNT] = {
    {WidgetId::StatusBar, layouts::WIDGET_KEYS[0], S::WStatusBar, 128, "Battery", 0, 0, 60, NO_HINT},
    {WidgetId::Clock, layouts::WIDGET_KEYS[1], S::WClock, 1, "Clock", 10, 10, 1, NO_HINT},
    {WidgetId::Weather, layouts::WIDGET_KEYS[2], S::WWeather, 8, "OpenMeteo", 0, 100, 360, S::HintWeather},
    {WidgetId::Agenda, layouts::WIDGET_KEYS[3], S::WAgenda, 2, "Cal Events", 22, 10, 60, S::HintAgenda},
    {WidgetId::Month, layouts::WIDGET_KEYS[4], S::WMonth, 256, "Calendar", 28, 210, 60, NO_HINT},
    {WidgetId::Stocks, layouts::WIDGET_KEYS[5], S::WStocks, 4, "Stocks", 22, 10, 60, S::HintStocks},
    {WidgetId::Tracking, layouts::WIDGET_KEYS[6], S::WTracking, 16, "Tracking", 22, 0, 120, S::HintTracking},
    {WidgetId::Printer, layouts::WIDGET_KEYS[7], S::WPrinter, 64, "Bambu", 22, 70, 4, S::HintPrinter},
    // Personal widgets (stage 4): local data, drawn on full refreshes and when the date changes.
    {WidgetId::Tasks, layouts::WIDGET_KEYS[8], S::WTasks, 512, "Tasks", 22, 0, 1440, S::HintTasks},
    {WidgetId::Notes, layouts::WIDGET_KEYS[9], S::WNote, 1024, "Note", 22, 0, 1440, S::HintNote},
    {WidgetId::Birthdays, layouts::WIDGET_KEYS[10], S::WBirthdays, 2048, "Birthdays", 22, 0, 1440, S::HintBirthdays},
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
  // Printer status (MQTT) or MakerWorld stats: either is enough for the widget.
  if (mqtt_ip[0] || makerworld_uid[0]) m |= layouts::widgetBit(WidgetId::Printer);
  if (user_content & USER_HAS_TASKS) m |= layouts::widgetBit(WidgetId::Tasks);
  if (user_content & USER_HAS_NOTE) m |= layouts::widgetBit(WidgetId::Notes);
  if ((user_content & USER_HAS_BIRTHDAYS) || bday_url[0]) m |= layouts::widgetBit(WidgetId::Birthdays);
  return m;
}

uint16_t enabledWidgets() {
  uint16_t always = layouts::widgetBit(WidgetId::StatusBar) | layouts::widgetBit(WidgetId::Clock);
  return configuredWidgets() & (widget_show_mask | always);
}

MissingHint missingHints[layouts::WIDGET_COUNT];
int missingHintCount = 0;

// Fills the layout table from an engine result.
static void fillTable(const layouts::Result& r) {
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
}

layouts::Result loadLayoutFrom(const char* tpl, const char* boxes, uint16_t showMask) {
  missingHintCount = 0;
  layouts::Result r;
  layouts::BoxLayout box;
  if (tpl && strcmp(tpl, "custom") == 0) {
    // The setup app's boxes: what the user placed, no gap filling; clock and status bar optional.
    // A widget without its settings keeps its box and shows a hint there.
    if (layouts::parseBoxes(boxes, box)) {
      uint16_t missing = 0;
      r = layouts::buildBoxes(box, configuredWidgets(), &missing);
      for (int w = 0; w < layouts::WIDGET_COUNT && !r.fellBack; w++) {
        WidgetId id = static_cast<WidgetId>(w);
        const layouts::Box* b = layouts::boxOf(box, id);
        if (!(missing & layouts::widgetBit(id)) || !b) continue;
        missingHints[missingHintCount++] = MissingHint{id, layouts::boxRect(*b, box.statusBar)};
      }
    } else {
      uint16_t always = layouts::widgetBit(WidgetId::StatusBar) | layouts::widgetBit(WidgetId::Clock);
      r = layouts::buildLayout(layouts::classicTemplate(), configuredWidgets() & (showMask | always));
      r.fellBack = true;
    }
  } else {
    uint16_t always = layouts::widgetBit(WidgetId::StatusBar) | layouts::widgetBit(WidgetId::Clock);
    r = layouts::buildLayout(layouts::templateByKey(tpl), configuredWidgets() & (showMask | always));
  }
  fillTable(r);
  return r;
}

layouts::Result loadLayout() {
  return loadLayoutFrom(layout_template, layout_boxes, widget_show_mask);
}
