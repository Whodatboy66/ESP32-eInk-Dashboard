#pragma once
// The over-the-air update flow of the dashboard (triggers, session, night check, confirm/rollback).
// See docs/OTA_UPDATES.md.
#include <stddef.h>
#include <stdint.h>

// What opened the update screen.
enum class OtaTrigger : uint8_t { DoubleTap, SetupPage };

// Settings (setup page, NVS "config").
extern bool otaAutoInstall;            // "Install updates automatically"
extern bool otaBeta;                   // "Update channel": beta
extern int otaLastCheckDay;            // ota::dayKey() of the last night-time check
extern uint32_t otaBadBuild;           // a build that rolled back: not auto-installed again

// A newer release seen but not installed (notify only / battery): "Update available" mark.
extern uint32_t otaAvailBuild;
extern char otaAvailVersion[16];

// State of this wake.
extern bool otaPendingVerify;          // first start of a freshly installed image (not confirmed yet)
extern bool otaWifiOk;                 // Wi-Fi worked during this wake (needed to confirm an update)
extern unsigned long otaUpdatedShownAt;  // millis() when "Updated to" was drawn; 0 = not shown

// The setup page's "Check for updates now" survives the restart in no-init RTC memory.
constexpr uint32_t UPDATE_REQUEST_MAGIC = 0x0DA7E5AF;
extern uint32_t updateRequest;

// Local date "YYYY-MM-DD" in the device timezone ("never" for 0 / no real time).
void formatDate(uint32_t t, char* out, size_t cap);

// Interactive check: info screen, then the result or the update (double tap: confirm tap).
// Leaves an update screen on the display: the caller redraws the dashboard afterwards.
void runUpdateSession(OtaTrigger trigger);
// Night-time check due (once per day in OTA_NIGHT_HOUR)? todayKey is set for runNightCheck.
bool otaNightCheckDue(int* todayKey);
// The night-time check while Wi-Fi is up; true if it drew an update screen.
bool runNightCheck(int todayKey);

// Boot: is this a new, unconfirmed image (arms the hang timer)?
void otaReadBootState();
// Boot, after the button: "Updated to" once, or the rollback notice.
void otaBootCheck();
// Before deep sleep: confirm the new image (Wi-Fi + display worked) or roll back.
void otaConfirmIfPending();
// The user is operating the device (a hold, setup mode, the update screen): the new firmware
// clearly runs, so a restart now must not roll it back.
void otaConfirmByUser(const char* why);
