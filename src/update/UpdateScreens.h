#pragma once
// E-paper screens of the update flow (docs/IMPLEMENTATION_PLAN.md stage 1, "Update screens").
// The first screen of a session is a full refresh, the following steps are partial refreshes.
// Texts avoid ( ) ! ' because the e-paper font has no such glyphs.

#include <Arduino.h>

#include "Manifest.h"
#include "UpdateClient.h"

namespace ota {

struct DeviceInfo {
  const char* version;     // FW_VERSION
  const char* variant;     // FW_VARIANT
  const char* lastUpdate;  // "2026-11-16" or "never"
  int batteryPercent;      // -1: no battery sensor
  bool usbPowered;
  const char* wifi;        // network name
  bool autoInstall;
};

// Double tap / setup page: device info and "Checking for updates..." (full refresh).
void showInfo(const DeviceInfo& info);

// "New version .65", What's new, and either "Tap to install - cancels in N s" or "Installing...".
void showAvailable(const Manifest& m, const char* installedVersion, bool waitForTap, uint32_t cancelMs);

// Downloading (bar in quarters), Verifying, Installing, Restarting.
void showProgress(InstallStage stage, int quarter, const char* newVersion);
void showRestarting(const char* newVersion);

// A result in plain language, e.g. "Up to date: .64" / "No Wi-Fi: try again later".
void showMessage(const char* title, const char* detail, bool fullRefresh = false);

// After the restart into the new firmware, once: "Updated to .65" with What's new (full refresh).
void showUpdated(const char* version, const char (*notes)[NOTE_LEN], int noteCount);

}  // namespace ota
