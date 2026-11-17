// ESP32 ePaper desk dashboard (fork of VoIPshare/ESP32-eInk-Dashboard).
// The firmware lives in src/: core/ (wake cycle, settings, logging), hardware/, services/,
// widgets/, ui/, web/ (setup page), update/ (over-the-air updates), input/.
// Overview: docs/plans/STAGE2_FOUNDATION.md.
#include "src/core/App.h"
#include "src/web/SetupServer.h"

void setup() {
  appSetup();  // one wake; ends in deep sleep, or in setup mode
}

void loop() {
  setupModeLoop();  // only does something while the setup page is open
}
