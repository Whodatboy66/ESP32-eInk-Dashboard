#pragma once
// Setup mode (access point "Dashboard-Setup" + captive portal + setup page).
void startAP();
// Serves the setup page while setup mode is on; restarts after AP_TIMEOUT_MS without activity.
void setupModeLoop();
