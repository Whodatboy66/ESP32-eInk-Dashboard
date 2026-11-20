// The dashboard's over-the-air update flow (docs/OTA_UPDATES.md): triggers (double tap, setup page,
// night-time check), the interactive session with its screens, install, confirm or roll back.
#include "OtaFlow.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>
#include <stdarg.h>
#include <time.h>

#include "../../app.h"
#include "../core/AppState.h"
#include "../core/Problems.h"
#include "../hardware/BatteryMonitor.h"
#include "../hardware/EpaperDriver.h"
#include "../hardware/TouchController.h"
#include "../services/WifiService.h"
#include "UpdateClient.h"
#include "UpdatePolicy.h"
#include "UpdateScreens.h"

#include <HTTPClient.h>

using i18n::Lang;
using i18n::S;

// File-internal functions, in the order they are defined below.
static bool otaBatteryOk();
static void otaText(Lang lang, S key, char* out, size_t cap, ...);
static void otaSetAvailable(const ota::Manifest* m);
static void otaProgress(ota::InstallStage stage, int quarter);
static void otaInstallAndRestart(const ota::Manifest& m);
static bool waitForConfirmTap(uint32_t timeoutMs);
static void describeFetchFailure(const ota::FetchOutcome& f, Lang lang, char* out, size_t cap);
static void otaMarkValid(const char* why);


// Over-the-air updates (docs/OTA_UPDATES.md). Settings and results are kept in NVS ("config").
bool otaAutoInstall = true;     // setup page "Install updates automatically"
bool otaBeta = false;           // setup page "Update channel": beta = test releases too
static const ota::DeviceIds OTA_DEVICE{FW_BUILD, FW_DISPLAY, FW_CONFIG_SCHEMA};
uint32_t otaAvailBuild = 0;     // newer release seen but not installed: status-bar mark
char otaAvailVersion[16] = "";
bool otaPendingVerify = false;  // first start of a freshly installed image (not confirmed yet)
bool otaWifiOk = false;         // Wi-Fi worked during this wake (needed to confirm an update)
unsigned long otaUpdatedShownAt = 0;  // millis() when "Updated to" was drawn; 0 = not shown
int otaLastCheckDay = 0;              // ota::dayKey() of the last night-time check (NVS)
uint32_t otaBadBuild = 0;             // a build that rolled back: no auto install (NVS)
// A setup page "Check for updates now" survives the restart in no-init RTC memory.
RTC_NOINIT_ATTR uint32_t updateRequest;
#define OTA_RESULT_SCREEN_MS 6000             // result messages stay this long
#define OTA_CONFIRM_TIMEOUT_S 180             // a new image not confirmed by then: restart = rollback



// Triggers: double tap (info screen, confirm tap to install), setup page "Check for updates now"
// (installs right away) and the night-time check (installs when "Install updates automatically"
// is on, otherwise only shows "Update available" in the status bar). See docs/OTA_UPDATES.md.

static const char* const OTA_NOTE_KEYS[ota::MAX_NOTES] = { "ota_n0", "ota_n1", "ota_n2" };
static char otaSessionVersion[16] = "";

static bool otaBatteryOk() {
  int pct = batteryPercentNow();
  return ota::batteryAllowsInstall(pct >= 0, pct, usbPowered(), OTA_BATTERY_MIN_PCT);
}

// Local time in the device timezone for the update code; restores the previous TZ afterwards
// (the clock widget may use its own timezone, and later redraws rely on it).
struct LocalTimezone {
  char saved[CFG_TIMEZONE_MAX] = "";
  bool hadTz = false;
  LocalTimezone() {
    const char* tz = getenv("TZ");
    if (tz) {
      hadTz = true;
      snprintf(saved, sizeof(saved), "%s", tz);
    }
    setenv("TZ", device_timezone[0] ? device_timezone : "UTC0", 1);
    tzset();
  }
  ~LocalTimezone() {
    if (hadTz) setenv("TZ", saved, 1);
    else unsetenv("TZ");
    tzset();
  }
};

void formatDate(uint32_t t, char* out, size_t cap) {
  if (t < 1700000000UL) {
    snprintf(out, cap, "%s", tr(S::UpdNever));
    return;
  }
  LocalTimezone tz;
  time_t tt = (time_t)t;
  struct tm tmv;
  localtime_r(&tt, &tmv);
  strftime(out, cap, "%Y-%m-%d", &tmv);
}

// A text of the table, printf-formatted, in a language.
static void otaText(Lang lang, S key, char* out, size_t cap, ...) {
  va_list args;
  va_start(args, cap);
  vsnprintf(out, cap, i18n::text(key, lang), args);
  va_end(args);
}

// Results go to the setup app ("ota_result") and the screen in the screen language, and to the log
// in English: compose(lang, out, cap) writes the text in a language and is called for both.
constexpr size_t OTA_TEXT_LEN = 112;

template <typename Compose>
static void otaSaveResult(Compose compose) {
  char text[2 * OTA_TEXT_LEN + 4];  // a title, " - " and a detail
  compose(Lang::En, text, sizeof(text));
  logPrintf("[OTA] %s\n", text);
  compose(ui_lang, text, sizeof(text));
  preferences.begin("config", false);
  preferences.putString("ota_result", text);
  preferences.putUInt("ota_rtime", (uint32_t)time(nullptr));
  preferences.end();
}

// A result screen: compose(lang, title, detail) writes both (OTA_TEXT_LEN each, detail may stay "").
template <typename Compose>
static void otaShowResult(Compose compose) {
  char title[OTA_TEXT_LEN], detail[OTA_TEXT_LEN];
  otaSaveResult([&](Lang lang, char* out, size_t cap) {
    title[0] = detail[0] = '\0';
    compose(lang, title, detail);
    snprintf(out, cap, "%s%s%s", title, detail[0] ? " - " : "", detail);
  });
  title[0] = detail[0] = '\0';
  compose(ui_lang, title, detail);
  ota::showMessage(title, detail);
  delay(OTA_RESULT_SCREEN_MS);
}

// "Update available" mark in the status bar (nullptr clears it).
static void otaSetAvailable(const ota::Manifest* m) {
  uint32_t build = m ? m->build : 0;
  if (build == otaAvailBuild) return;
  otaAvailBuild = build;
  snprintf(otaAvailVersion, sizeof(otaAvailVersion), "%s", m ? m->version : "");
  preferences.begin("config", false);
  preferences.putUInt("ota_avail", otaAvailBuild);
  preferences.putString("ota_availv", otaAvailVersion);
  preferences.end();
  forceUpdateStatusBar = true;
}

static void otaProgress(ota::InstallStage stage, int quarter) {
  ota::showProgress(stage, quarter, otaSessionVersion);
}

// Downloads and activates m, then restarts into it (doesn't return). On a failure the message is
// shown, and this returns.
static void otaInstallAndRestart(const ota::Manifest& m) {
  snprintf(otaSessionVersion, sizeof(otaSessionVersion), "%s", m.version);
  if (otaPendingVerify) {  // never overwrite the fallback slot while this image is unconfirmed
    otaShowResult([](Lang l, char* t, char* d) {
      otaText(l, S::OtaPostponed, t, OTA_TEXT_LEN);
      otaText(l, S::OtaStillStarting, d, OTA_TEXT_LEN);
    });
    return;
  }

  ota::InstallResult r = ota::install(m, otaProgress);
  if (r == ota::InstallResult::Ok) {
    // Remembered for the new firmware: "Updated to" + What's new, and to notice a rollback.
    // Written only now, so an interrupted download isn't reported as a failed start later.
    preferences.begin("config", false);
    preferences.putUInt("ota_done", m.build);
    preferences.putUChar("ota_nn", m.noteCount);
    for (int i = 0; i < ota::MAX_NOTES; i++)
      preferences.putString(OTA_NOTE_KEYS[i], i < m.noteCount ? m.notes[i] : "");
    preferences.end();
    otaSaveResult([&](Lang l, char* out, size_t cap) { otaText(l, S::OtaInstalledRestarting, out, cap, m.version); });
    ota::showRestarting(m.version);
    logFlush(true);
    delay(300);
    ESP.restart();
  }

  otaSaveResult([&](Lang l, char* out, size_t cap) { otaText(l, S::OtaFailedTo, out, cap, m.version, ota::describe(r, l)); });
  ota::showMessage(tr(S::OtaFailedKept), ota::describe(r, ui_lang));
  delay(OTA_RESULT_SCREEN_MS);
}

// Waits for a tap or double tap seen by the button task.
static bool waitForConfirmTap(uint32_t timeoutMs) {
  if (!hasDemoButtonPin()) return false;
  buttonMailbox = ButtonEvent::None;
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    ButtonEvent e = buttonMailbox;
    if (e == ButtonEvent::Tap || e == ButtonEvent::DoubleTap) {
      buttonMailbox = ButtonEvent::None;
      return true;
    }
    delay(20);
  }
  return false;
}

static void describeFetchFailure(const ota::FetchOutcome& f, Lang lang, char* out, size_t cap) {
  switch (f.result) {
    case ota::FetchResult::NoRelease:
      otaText(lang, S::OtaNoRelease, out, cap, i18n::text(otaBeta ? S::UpdBeta : S::UpdStable, lang));
      break;
    case ota::FetchResult::BadSignature: otaText(lang, S::OtaBadSignature, out, cap); break;
    case ota::FetchResult::BadManifest: otaText(lang, S::OtaBadManifest, out, cap, (int)f.manifestError); break;
    default:
      if (f.httpCode == HTTPC_ERROR_TOO_LESS_RAM) otaText(lang, S::OtaNoMemory, out, cap);
      else if (f.httpCode < 0) otaText(lang, S::OtaNoGithub, out, cap);
      else otaText(lang, S::OtaGithubAnswered, out, cap, f.httpCode);
      break;
  }
}

// Interactive check: info screen, then the result or the update (double tap: after a confirm tap).
// Leaves an update screen on the display: the caller redraws the dashboard afterwards.
void runUpdateSession(OtaTrigger trigger) {
  otaConfirmByUser("update screen");
  setCpuFrequencyMhz(cpuFullMhz);
  initDisplay(true);

  preferences.begin("config", true);
  uint32_t lastUpdate = preferences.getUInt("ota_updt", 0);
  preferences.end();
  char lastUpdateText[16];
  formatDate(lastUpdate, lastUpdateText, sizeof(lastUpdateText));
  ota::DeviceInfo info{FW_VERSION, FW_VARIANT, lastUpdateText, batteryPercentNow(), usbPowered(),
                       wifi_ssid, otaAutoInstall, tr(otaBeta ? S::UpdBeta : S::UpdStable)};
  ota::showInfo(info);

  if (!wifi_ssid[0] || !startWiFiReliable(wifi_ssid, wifi_pass)) {
    otaShowResult([](Lang l, char* t, char* d) {
      otaText(l, S::OtaNoWifi, t, OTA_TEXT_LEN);
      if (!wifi_ssid[0]) otaText(l, S::OtaSetUpWifi, d, OTA_TEXT_LEN);
    });
  } else {
    otaWifiOk = true;
    wifiFailures = 0;
    setProblem(PROBLEM_WIFI, false);
    ota::Manifest m;
    ota::FetchOutcome f = ota::fetchManifest(otaBeta, m);
    if (f.result != ota::FetchResult::Ok) {
      otaShowResult([&](Lang l, char* t, char* d) {
        otaText(l, S::OtaCheckFailed, t, OTA_TEXT_LEN);
        describeFetchFailure(f, l, d, OTA_TEXT_LEN);
      });
    } else {
      switch (ota::availabilityFor(m, OTA_DEVICE)) {
        case ota::Availability::UpToDate:
          otaSetAvailable(nullptr);
          otaShowResult([](Lang l, char* t, char*) { otaText(l, S::OtaUpToDate, t, OTA_TEXT_LEN, FW_VERSION); });
          break;
        case ota::Availability::NotForThisBoard:
          otaShowResult([&](Lang l, char* t, char* d) {
            otaText(l, S::OtaNoUpdate, t, OTA_TEXT_LEN);
            otaText(l, S::OtaNoFile, d, OTA_TEXT_LEN, m.version, FW_VARIANT);
          });
          break;
        case ota::Availability::WrongDisplay:
          otaShowResult([&](Lang l, char* t, char* d) {
            otaText(l, S::OtaNoUpdate, t, OTA_TEXT_LEN);
            otaText(l, S::OtaWrongDisplay, d, OTA_TEXT_LEN, m.version, m.fileDisplay, FW_DISPLAY);
          });
          break;
        case ota::Availability::IncompatibleSettings:
          otaShowResult([&](Lang l, char* t, char* d) {
            otaText(l, S::OtaNoUpdate, t, OTA_TEXT_LEN);
            otaText(l, S::OtaBadSettings, d, OTA_TEXT_LEN, m.version);
          });
          break;
        case ota::Availability::NeedsNewerFirmware:
          otaShowResult([&](Lang l, char* t, char* d) {
            otaText(l, S::OtaNeedsUsb, t, OTA_TEXT_LEN);
            otaText(l, S::OtaNeedsNewer, d, OTA_TEXT_LEN, m.version, (unsigned)m.minBuild);
          });
          break;
        case ota::Availability::Newer:
          if (!otaBatteryOk()) {
            otaSetAvailable(&m);
            const int pct = batteryPercentNow();
            otaShowResult([pct](Lang l, char* t, char* d) {
              otaText(l, S::OtaBatteryLow, t, OTA_TEXT_LEN);
              otaText(l, S::OtaBatteryDetail, d, OTA_TEXT_LEN, pct, OTA_BATTERY_MIN_PCT);
            });
          } else if (trigger == OtaTrigger::DoubleTap) {
            ota::showAvailable(m, FW_VERSION, true, OTA_CONFIRM_MS);
            if (waitForConfirmTap(OTA_CONFIRM_MS)) {
              ota::showAvailable(m, FW_VERSION, false, 0);
              otaInstallAndRestart(m);  // returns only after a failure
            } else {
              otaSetAvailable(&m);  // no tap: back to the dashboard, the mark stays
              otaSaveResult([&](Lang l, char* out, size_t cap) { otaText(l, S::OtaNotInstalled, out, cap, m.version); });
            }
          } else {
            ota::showAvailable(m, FW_VERSION, false, 0);
            otaInstallAndRestart(m);
          }
          break;
      }
    }
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  setCpuFrequencyMhz(80);
}

// Night-time check due (OTA_NIGHT_HOUR, once per day)? Needs the real time.
bool otaNightCheckDue(int* todayKey) {
  time_t now = time(nullptr);
  if (now < 1700000000) return false;
  LocalTimezone tz;
  struct tm t;
  localtime_r(&now, &t);
  *todayKey = ota::dayKey(t.tm_year + 1900, t.tm_yday);
  return ota::nightCheckDue(t.tm_hour, *todayKey, otaLastCheckDay, OTA_NIGHT_HOUR);
}

// Night-time check, while Wi-Fi is up. Returns true if it drew an update screen (the dashboard
// must be redrawn).
bool runNightCheck(int todayKey) {
  // Once per night, also after a failure. In NVS: an install or rollback restart would reset RTC
  // memory and start the check again.
  otaLastCheckDay = todayKey;
  preferences.begin("config", false);
  preferences.putInt("ota_day", todayKey);
  preferences.end();
  ota::Manifest m;
  ota::FetchOutcome f = ota::fetchManifest(otaBeta, m);
  if (f.result != ota::FetchResult::Ok) {
    otaSaveResult([&](Lang l, char* out, size_t cap) {
      char detail[OTA_TEXT_LEN];
      describeFetchFailure(f, l, detail, sizeof(detail));
      otaText(l, S::OtaNightFailed, out, cap, detail);
    });
    return false;
  }
  ota::Availability a = ota::availabilityFor(m, OTA_DEVICE);
  bool failed = ota::failedBefore(m.build, otaBadBuild);
  switch (ota::nightAction(a, otaAutoInstall, otaBatteryOk(), failed)) {
    case ota::NightAction::Nothing:
      if (a == ota::Availability::UpToDate) otaSetAvailable(nullptr);
      otaSaveResult([a](Lang l, char* out, size_t cap) {
        otaText(l, a == ota::Availability::UpToDate ? S::OtaNightUpToDate : S::OtaNightNothing, out, cap);
      });
      return false;
    case ota::NightAction::NotifyOnly:
      otaSetAvailable(&m);
      otaSaveResult([&](Lang l, char* out, size_t cap) {
        otaText(l, failed ? S::OtaNightFailedBefore : S::OtaNightAvailable, out, cap, m.version);
      });
      return false;
    case ota::NightAction::WaitForCharge:
      otaSetAvailable(&m);
      otaSaveResult([&](Lang l, char* out, size_t cap) { otaText(l, S::OtaNightWaits, out, cap, m.version); });
      return false;
    case ota::NightAction::Install:
      initDisplay(false);
      ota::showAvailable(m, FW_VERSION, false, 0);
      otaInstallAndRestart(m);  // returns only after a failure
      return true;
  }
  return false;
}

// The bootloader starts a new image in "pending verify" state; it is confirmed here once Wi-Fi
// and a display refresh have worked. A crash or reset before that rolls back to the old version.
extern "C" bool verifyRollbackLater() { return true; }

// Called before deep sleep, after the dashboard was drawn. Without Wi-Fi the new firmware isn't
// confirmed: roll back right away with a clean restart (not through deep sleep, whose wake would
// start the old firmware on RTC memory laid out by the new one).
static esp_timer_handle_t otaHangTimer = nullptr;

// Confirms the running image and records the update. Also called from the button task: uses its
// own Preferences object, not the global one the main task may have open.
static void otaMarkValid(const char* why) {
  if (!otaPendingVerify) return;
  esp_ota_mark_app_valid_cancel_rollback();
  otaPendingVerify = false;
  if (otaHangTimer) esp_timer_stop(otaHangTimer);
  otaAvailBuild = 0;
  Preferences p;
  p.begin("config", false);
  p.putUInt("ota_updt", (uint32_t)time(nullptr));
  p.putUInt("ota_done", 0);
  p.putUInt("ota_avail", 0);
  char text[48];
  snprintf(text, sizeof(text), tr(S::UpdUpdatedTo), FW_VERSION);  // for the setup app
  p.putString("ota_result", text);
  p.putUInt("ota_rtime", (uint32_t)time(nullptr));
  p.end();
  logPrintf("[OTA] new firmware %s confirmed (%s)\n", FW_VERSION, why);
}

// The user is operating the device (a hold, setup mode, the update screen): the new firmware
// clearly runs, so a restart now must not roll it back. Not for test builds (docs/OTA_TESTS.md):
// the crash / hang tests must roll back even if the button is used meanwhile; a test build stays
// only if it passes the automatic check (Wi-Fi and display).
void otaConfirmByUser(const char* why) {
  if (ota::isTestBuild(FW_BUILD)) {
    if (otaPendingVerify) logPrintf("[OTA] test build: not confirmed by %s\n", why);
    return;
  }
  otaMarkValid(why);
}

void otaConfirmIfPending() {
  if (!otaPendingVerify) return;
  if (!otaWifiOk) {
    logPrintln("[OTA] new firmware not confirmed (no Wi-Fi): rolling back");
    logFlush(true);
    Serial.flush();
    esp_ota_mark_app_invalid_rollback_and_reboot();
    return;  // only if there is no previous version to go back to
  }
  otaMarkValid("Wi-Fi and display OK");
}

// Early at boot: is this a new, not yet confirmed image? Then a hang must not keep it forever: a
// one-shot timer restarts the board, and the bootloader rolls back (a restart before the
// confirmation always does).
void otaReadBootState() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;
  otaPendingVerify = running && esp_ota_get_state_partition(running, &state) == ESP_OK &&
                     state == ESP_OTA_IMG_PENDING_VERIFY;
  if (!otaPendingVerify) return;
  logPrintln("[OTA] new firmware, not confirmed yet");
  esp_timer_create_args_t args = {};
  args.callback = [](void*) { esp_restart(); };
  args.name = "ota_hang";
  if (esp_timer_create(&args, &otaHangTimer) == ESP_OK)
    esp_timer_start_once(otaHangTimer, OTA_CONFIRM_TIMEOUT_S * 1000000ULL);
}

// At boot, after the button: "Updated to" screen once, or the notice of a rollback.
void otaBootCheck() {
  preferences.begin("config", true);
  uint32_t doneBuild = preferences.getUInt("ota_done", 0);
  char notes[ota::MAX_NOTES][ota::NOTE_LEN] = {};
  uint8_t noteCount = preferences.getUChar("ota_nn", 0);
  if (noteCount > ota::MAX_NOTES) noteCount = ota::MAX_NOTES;
  for (int i = 0; i < noteCount; i++) preferences.getString(OTA_NOTE_KEYS[i], notes[i], ota::NOTE_LEN);
  preferences.end();
  if (!doneBuild) return;

  if (doneBuild == FW_BUILD) {
    if (!otaPendingVerify) {  // already confirmed (e.g. by a hold right away): just clean up
      preferences.begin("config", false);
      preferences.putUInt("ota_done", 0);
      preferences.end();
      return;
    }
    initDisplay(true);
    ota::showUpdated(FW_VERSION, notes, noteCount);
    otaUpdatedShownAt = millis();
    if (!otaUpdatedShownAt) otaUpdatedShownAt = 1;
    return;
  }
  if (doneBuild > FW_BUILD && esp_ota_get_last_invalid_partition() == nullptr) {
    doneBuild = 0;  // no rolled-back image: the restart into the update never happened
  }
  // An update was installed but this (older) firmware runs: the new one failed and rolled back.
  preferences.begin("config", false);
  preferences.putUInt("ota_done", 0);
  if (doneBuild > FW_BUILD) {
    otaBadBuild = doneBuild;  // not installed again automatically (no reinstall loop)
    preferences.putUInt("ota_bad", otaBadBuild);
  }
  preferences.end();
  if (doneBuild > FW_BUILD) {
    otaSaveResult([&](Lang l, char* out, size_t cap) { otaText(l, S::OtaRolledBack, out, cap, (unsigned)doneBuild, FW_VERSION); });
    initDisplay(true);
    ota::showMessage(tr(S::OtaFailedKept), tr(S::OtaDidNotStart), true);
    delay(OTA_RESULT_SCREEN_MS);  // a cold start: the dashboard is fully redrawn next
  }
}

