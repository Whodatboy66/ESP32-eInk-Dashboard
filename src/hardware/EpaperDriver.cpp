// The e-paper display (see EpaperDriver.h).
#include "EpaperDriver.h"

#include <SPI.h>
#include <new>

#include "../widgets/StatusBar.h"
#include "../widgets/Widgets.h"

namespace {
// The display object needs the pins from the settings: made on first use, in static storage.
alignas(DashboardDisplay) uint8_t storage[sizeof(DashboardDisplay)];
DashboardDisplay* instance = nullptr;
constexpr uint32_t SERIAL_DIAG_BAUD = 115200;  // GxEPD2's own diagnostics on the serial port
constexpr uint16_t RESET_PULSE_MS = 2;
}  // namespace

bool displayReady = false;

DashboardDisplay& getDisplay() {
  if (!instance)
    instance = new (storage) DashboardDisplay(DashboardPanel(activePins.epdCs, activePins.epdDc, activePins.epdRst, activePins.epdBusy));
  return *instance;
}

void initDisplay(bool initialRefresh) {
  if (displayReady) return;
  if (hasDisplayPowerPin()) {  // some HATs need their power switched on (the FireBeetle: IO4)
    pinMode(activePins.displayPower, OUTPUT);
    digitalWrite(activePins.displayPower, HIGH);
    delay(50);
  }
  SPI.begin(activePins.epdSck, -1, activePins.epdMosi, activePins.epdCs);
  display.init(SERIAL_DIAG_BAUD, initialRefresh, RESET_PULSE_MS, false);
  display.setRotation(0);
  displayReady = true;
}

void updatePartial(LayoutItem* item, void (*draw)(LayoutItem*)) {
  if (!item) return;
  display.setPartialWindow(item->PosX, item->PosY, item->Width, item->Height);
  display.firstPage();
  do {
    canvas().fillRect(item->PosX, item->PosY, item->Width, item->Height, GxEPD_WHITE);
    setCanvasClip(item->PosX, item->PosY, item->Width, item->Height);
    draw(item);
    // Without a status bar the problem mark sits in the bottom-right corner over a widget: a redraw
    // of that widget keeps it (clipped to this box).
    if (!getLayout(box::STATUS)) drawProblemCorner();
    clearCanvasClip();
  } while (display.nextPage());
}
