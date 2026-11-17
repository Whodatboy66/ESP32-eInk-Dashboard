#pragma once
// Drawing the whole screen (a full refresh), for the device and for the PC renderer
// (tools/render), so both draw exactly the same. Each widget is clipped to its place.
struct LayoutItem;

// Which widgets have something to draw (fresh or kept data). Clock and status bar always.
struct FullScreenParts {
  bool printer = false;    // printer area: printer status or MakerWorld stats
  bool printView = false;  // printer status (printing / just finished) instead of MakerWorld
  bool weather = false;
  bool agenda = false;
  bool stocks = false;
  bool month = false;
  bool tracking = false;
};

void drawFullScreen(const FullScreenParts& parts);
#include "../core/ServiceState.h"

// A small "old" badge when the service's data is stale (ServiceState.h): top right (lists), or
// bottom right (weather: its icons fill the top).
void drawStaleMark(LayoutItem* item, Service service, bool bottom = false);
// One widget, clipped to its place.
void drawWidget(LayoutItem* item, void (*draw)(LayoutItem*));
