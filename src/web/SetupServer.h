#pragma once
// Setup mode: access point "Dashboard-Setup" + captive portal, or edit mode on the home network;
// both serve the setup app (web/, src/web/Api.cpp).
void startAP();
// Edit mode on the home network (app behind a code shown on the screen); the hotspot if Wi-Fi fails.
void startEditMode();
// Setup hold: edit mode when Wi-Fi is set up, else the hotspot.
void startSetup();
// Serves the setup page while setup mode is on; restarts after AP_TIMEOUT_MS without activity.
void setupModeLoop();
