#include "EpaperDriver.h"

#include "../widgets/StatusBar.h"

#include <SPI.h>
#include <new>

#include "../widgets/Widgets.h"

namespace {
alignas(DashboardDisplay) uint8_t displayStorage[sizeof(DashboardDisplay)];
DashboardDisplay* displayInstance = nullptr;
}

DashboardDisplay& getDisplay() {
  if (displayInstance == nullptr) {
    displayInstance = new (displayStorage) DashboardDisplay(
      DashboardPanel(activePins.epdCs, activePins.epdDc, activePins.epdRst, activePins.epdBusy));
  }
  return *displayInstance;
}

bool displayReady = false;
// Powers and initializes the panel once per boot.
void initDisplay(bool initialRefresh) {
  if (displayReady) return;
  if (hasDisplayPowerPin()) {
    // Not all hats need it; IO4 powers some e-paper boards.
    pinMode(activePins.displayPower, OUTPUT);
    digitalWrite(activePins.displayPower, HIGH);
    delay(50);
  }
  SPI.begin(activePins.epdSck, -1, activePins.epdMosi, activePins.epdCs);
  display.init(115200, initialRefresh, 2, false);
  display.setRotation(0);
  displayReady = true;
}

void updatePartial(LayoutItem* item, void (*drawFunc)(LayoutItem*)) {
  if (!item) return;

  display.setPartialWindow(item->PosX, item->PosY, item->Width, item->Height);
  display.firstPage();
  do {
    canvas().fillRect(item->PosX, item->PosY, item->Width, item->Height, GxEPD_WHITE);
    setCanvasClip(item->PosX, item->PosY, item->Width, item->Height);
    drawFunc(item);
    // Without a status bar the problem mark sits in the bottom-right corner, over a widget:
    // redrawing that widget must keep the mark (clipped to this window).
    if (!getLayout(128)) drawProblemCorner();
    clearCanvasClip();
  } while (display.nextPage());
}

