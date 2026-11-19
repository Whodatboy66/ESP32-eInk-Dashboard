#pragma once
// The widgets the layout engine places (src/layout/): what each one is in the layout table, its
// metrics, and when it counts as configured (its setting is filled in).
#include <stdint.h>

#include "../layout/Templates.h"

struct WidgetInfo {
  layouts::WidgetId id;
  const char* key;           // setup page / NVS ("weather")
  const char* name;          // shown on the setup page
  int layoutId;              // ID in the layout table (getLayout())
  const char* description;
  int rowHeight, colWidth;   // text line height / column width the drawing code uses
  int refresh;               // refresh interval in wakes (minutes)
  const char* hint;          // what to set up when its settings are missing (box layouts, the app)
};

// A widget placed in a box layout whose settings are missing: its box shows the hint instead.
struct MissingHint {
  layouts::WidgetId id;
  layouts::Rect rect;
};
extern MissingHint missingHints[layouts::WIDGET_COUNT];
extern int missingHintCount;

const WidgetInfo& widgetInfo(layouts::WidgetId id);
// Widgets whose setting is filled in (status bar, clock and month view always).
uint16_t configuredWidgets();
// Widgets shown: configured and switched on (status bar and clock always).
uint16_t enabledWidgets();
// Builds the layout table (layout[], Widgets.h) for the configured template and widgets.
layouts::Result loadLayout();
// The same for a layout that isn't saved (the setup app's exact preview): template key, box string
// (for "custom") and the widgets switched on (templates).
layouts::Result loadLayoutFrom(const char* tpl, const char* boxes, uint16_t showMask);
