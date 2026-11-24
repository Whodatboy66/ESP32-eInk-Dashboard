// The wake cycle. Every minute the board wakes from deep sleep, decides what to do (WakePlan),
// fetches what is due, redraws what changed and sleeps again until the next minute.
#include "App.h"

#include <Arduino.h>
#include <WiFi.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "../../app.h"
#include "../hardware/BatteryMonitor.h"
#include "../hardware/EpaperDriver.h"
#include "../hardware/PinMapping.h"
#include "../hardware/StatusLed.h"
#include "../hardware/TouchController.h"
#include "../services/Bambu.h"
#include "../services/Fetchers.h"
#include "FeedSchedule.h"
#include "../services/TimeService.h"
#include "../services/WifiService.h"
#include "../update/OtaFlow.h"
#include "../update/UpdatePolicy.h"
#include "../web/SetupServer.h"
#include "../widgets/Clock.h"
#include "../widgets/Dashboard.h"
#include "../widgets/Registry.h"
#include "../widgets/MonthView.h"
#include "../widgets/Personal.h"
#include "../personal/UserStore.h"
#include "../widgets/StatusBar.h"
#include "../widgets/Widgets.h"
#include "AppState.h"
#include "ConfigManager.h"
#include "Problems.h"
#include "WakePlan.h"
#include "PowerPlan.h"
#include "../ui/Canvas.h"
#include "../ui/SparseFont.h"

bool fullRefresh;  // this wake redraws the whole screen

// Minute the clock was drawn in this wake (redrawn if the minute changes during the network work).
static long clockDrawnMinute = -1;
// Personal widgets (stage 4): the day they were drawn for (due labels, "in 3 d"), and the birthday
// calendar link: the day it was read, and when to try again after a failure.
RTC_DATA_ATTR static long personalDrawnDay = 0;
RTC_DATA_ATTR static long tasksPurgedDay = 0;  // done tasks older than 30 days: removed once a day
// The birthday link: once a day, after a failure once an hour. School holidays (stage 7): once a
// week, after a failure after 6 hours (src/core/FeedSchedule.h).
RTC_DATA_ATTR static FeedState bdayFeed;
RTC_DATA_ATTR static FeedState schoolFeed;
// Printer (MQTT): failed connections in a row and the time before which it isn't asked again.
RTC_DATA_ATTR static uint8_t bambuFailures = 0;
RTC_DATA_ATTR static time_t bambuRetryAfter = 0;  // a changed link is read right away


// The minute on the clock (changes exactly when HH:MM does).
static long minuteNow() { return (long)(time(nullptr) / 60); }

// Records a fetch result for the retry backoff and the "old data" mark (ServiceState.h).
static void recordFetch(Service svc, bool ok, LayoutItem* item) {
  time_t interval = item && item->Refresh > 0 ? (time_t)item->Refresh * 60 : 3600;
  recordResult(serviceStates[(int)svc], ok, time(nullptr), interval);
}

// After a failed fetch the widget normally keeps its picture (no redraw). Once its data is old it
// is redrawn anyway, so the "old data" mark appears (the failed fetch is what makes data old).
static bool turnedStale(Service svc, LayoutItem* item) {
  time_t interval = item && item->Refresh > 0 ? (time_t)item->Refresh * 60 : 3600;
  return isStale(serviceStates[(int)svc], time(nullptr), interval);
}

// After a data service's download: its result for the retries and the "old" mark, its problem
// flag (also for a part that failed, e.g. one stock symbol), and whether its box is redrawn: after
// a success, on a full refresh, or once a failure made the data old.
static bool afterFetch(Service svc, bool ok, bool partFailed, uint16_t problem, LayoutItem* box) {
  recordFetch(svc, ok, box);
  setProblem(problem, !ok || partFailed);
  return ok || fullRefresh || turnedStale(svc, box);
}

// The time zone for the calendar and the flight: the clock widget's own, else the device's.
static const char* clockZone(const LayoutItem* clock) {
  return clock && clock->Option[0] ? clock->Option : device_timezone;
}

// The next flight in the calendar is followed from 6 hours before departure until it lands.
static void followNextFlight(time_t now) {
  const CalEventItem& next = cal.nextFlight;
  if (!next.start || next.start == liveFlight.calStart || next.start - now >= FLIGHT_LIVE_BEFORE_S) return;
  liveFlight = {};
  liveFlight.calStart = next.start;
  liveFlight.calEnd = next.end;
  snprintf(liveFlight.title, sizeof(liveFlight.title), "%s", next.title);
  extractFlightNumber(liveFlight.title, liveFlight.number, sizeof(liveFlight.number));
}

constexpr uint32_t LOW_POWER_MHZ = 80;
constexpr unsigned long SERIAL_BAUD = 115200;

// Power (stage 6): battery saver on at the last wake (hysteresis), the night note already shown,
// the "please charge" screen shown, the wake interval of the last wake.
RTC_DATA_ATTR static bool saverWasOn = false;
RTC_DATA_ATTR static bool nightShown = false;
RTC_DATA_ATTR static bool criticalShown = false;

// The button wakes the board from deep sleep (input HIGH). A press still going on is finished first
// by the button task (it restarts for a gesture, or flags a fault after BUTTON_FAULT_MS), so it
// isn't lost; the pin is read too, for a press the task hasn't polled yet (bounded wait).
static void armButtonWake() {
  const gpio_num_t pin = getDemoWakeGpio();
  const unsigned long t0 = millis();
  while ((buttonBusy || digitalRead(activePins.demoButton) == HIGH) && !buttonFaulted &&
         millis() - t0 < BUTTON_FAULT_MS + 1000UL)
    delay(10);
  if (digitalRead(activePins.demoButton) == HIGH) {
    // Stuck HIGH: a button wake would fire at once and keep the board awake. Timer wakes only,
    // until it reads LOW again.
    DBG(F("[Button] input stuck HIGH: button wake off for this sleep"));
    return;
  }
  buttonFaulted = false;  // it reads LOW: the sensor is fine again
  esp_sleep_enable_ext1_wakeup(1ULL << pin, ESP_EXT1_WAKEUP_ANY_HIGH);
  // The pull-down stays on in sleep (latched by the pad hold, released at the next wake), so a
  // missing sensor can't float HIGH and wake the board.
  if (rtc_gpio_is_valid_gpio(pin)) {
    rtc_gpio_pullup_dis(pin);
    rtc_gpio_pulldown_en(pin);
  }
}

// Deep sleep: display off (if it was used), button wake armed, the diagnostics log written.
// timerUs 0: no timer wake (critical battery: only the button wakes it).
[[noreturn]] static void sleepNow(uint64_t timerUs, bool displayUsed, bool hibernateDisplay, bool keepLog) {
  // hibernate() also clears the controller's buffer that partial refreshes compare against: only
  // before a full refresh. powerOff() keeps it for the next partial one.
  if (displayUsed) hibernateDisplay ? display.hibernate() : display.powerOff();
  if (hasDisplayPowerPin()) gpio_hold_en((gpio_num_t)activePins.displayPower);  // stays as it is in sleep
  if (hasDemoButtonPin()) armButtonWake();
  if (timerUs) esp_sleep_enable_timer_wakeup(timerUs);
  // Diagnostics (setup app): the battery once an hour; this wake's log if it did something.
  batteryHistorySample();
  if (keepLog) logKeepThisWake();
  logFlush();
  esp_deep_sleep_start();
  for (;;) {}  // not reached
}

// Below 5 % battery: one full-screen message, then only the button wakes the board.
static void drawCriticalScreen(float volts) {
  initDisplay(true);
  display.setFullWindow();
  display.firstPage();
  do {
    canvas().fillScreen(GxEPD_WHITE);
    drawChargeScreen(volts);
  } while (display.nextPage());
}

// One wake: boot checks, button, update requests, plan (WakePlan), Wi-Fi + fetches, drawing, sleep.
void appSetup() {
  // Most wakes only redraw the clock: 80 MHz, full speed only for Wi-Fi (TLS is CPU bound: at
  // 80 MHz the radio would stay on longer).
  cpuFullMhz = getCpuFrequencyMhz();
  setCpuFrequencyMhz(LOW_POWER_MHZ);
  Serial.begin(SERIAL_BAUD);
  const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
  const bool coldBoot = wakeCause == ESP_SLEEP_WAKEUP_UNDEFINED;
  // After a reset / flash (not a timer wake): watch the BOOT button for a few seconds (backup
  // way into setup mode). The wait also gives the USB serial monitor time to attach.
  [[maybe_unused]] bool bootButtonSetup = false;
  if (coldBoot) {
#ifdef BOOT_BUTTON_PIN
    bootButtonSetup = waitForBootButtonSetup();
#elif DEBUG
    delay(3000);  // no BOOT button watch: still time for a serial monitor
#endif
#if CONFIG_IDF_TARGET_ESP32
    delay(2000);  // the classic ESP32 after power-up / reset (not on every wake)
#endif
  }
  forceSuperMiniLedsOff();  // the SuperMini's LEDs (does nothing on other boards)

  // A full refresh once a day (bootCount counts minutes) and after a button wake.
  fullRefresh = bootCount % (60 * 24) == 0;
  const bool buttonWoke = wakeCause == ESP_SLEEP_WAKEUP_EXT1;
  if (buttonWoke) {
    fullRefresh = true;
    bootCount = 0;
  }
  // Logged after the settings are loaded: the time zone (log times) comes with them.
  const char* wakeMsg = buttonWoke ? "Wake: the button"
                        : wakeCause == ESP_SLEEP_WAKEUP_TIMER ? "Wake: timer"
                        : coldBoot ? "Start: power-up, reset or update"
                                   : "Wake: other cause";
  // An update screen covered the dashboard during the last wake: redraw (and refetch) everything.
  if (redrawAll) {
    redrawAll = false;
    fullRefresh = true;
    bootCount = 0;
  }

  configLoad();
  const uint16_t problemsAtStart = problems;  // a change is worth keeping in the diagnostics log
  // Local time for everything drawn (the clock widget used to be the only place that set it, and
  // a layout may have no clock).
  setenv("TZ", device_timezone[0] ? device_timezone : "UTC0", 1);
  tzset();
  logPrintln(wakeMsg);
  DBGF("Firmware %s (build %d, %s, %s, settings schema %d), auto update %s, channel %s", FW_VERSION,
       FW_BUILD, FW_VARIANT, FW_DISPLAY, FW_CONFIG_SCHEMA, otaAutoInstall ? "on" : "off",
       ota::channelName(otaBeta));
  otaReadBootState();
  if (coldBoot) configCheckSerialFactoryReset();  // after otaReadBootState: confirms a pending update
#ifdef OTA_TEST_CRASH
  // Test build only (docs/OTA_TESTS.md): crash in the first start after an over-the-air install,
  // before the update can confirm itself. Never crashes when flashed over USB.
  if (otaPendingVerify) {
    logPrintln("[OTA test] crashing on purpose");
    Serial.flush();
    abort();
  }
#endif

  // Setup hold while the board was awake: edit mode on the home network if Wi-Fi works, else the setup hotspot (startSetup).
  // "Change Wi-Fi" in the app asks for the hotspot directly.
  if (setupRequest == SETUP_REQUEST_AP_MAGIC) {
    setupRequest = 0;
    startAP();
    return;
  }
  // Restarted into setup: after saving in edit mode (back into it), or by a setup hold while awake.
  const uint32_t request = setupRequest;
  if (request == SETUP_REQUEST_EDIT_MAGIC || request == SETUP_REQUEST_MAGIC) {
    setupRequest = 0;
    if (request == SETUP_REQUEST_EDIT_MAGIC) startEditMode();
    else startSetup();
    return;
  }

#ifdef BOOT_BUTTON_PIN
  // Setup mode asked by holding BOOT right after a reset (works without the touch sensor).
  if (bootButtonSetup) {
    if (isPinInActiveConfig(BOOT_BUTTON_PIN)) {
      logPrintf("BOOT held, but GPIO%d is used by the pin map: ignored\n", BOOT_BUTTON_PIN);
    } else {
      logPrintln("BOOT held after reset -> AP config mode");
      startAP();
      return;
    }
  }
#endif

  // The button first: the hold time counts from the wake, as the user feels it.
  const int action = readWakeButtonAction();
  if (action == BUTTON_ACTION_AP) {
    startSetup();
    return;
  }
  if (action == BUTTON_ACTION_REBOOT) {
    otaConfirmByUser("reboot hold");
    logFlush(true);
    ESP.restart();
  }
  const bool updateScreenRequested = action == BUTTON_ACTION_UPDATE;
  // From here a watcher task follows the button while the dashboard is awake (TouchController.cpp).
  constexpr uint32_t BUTTON_TASK_STACK = 3072;
  constexpr UBaseType_t BUTTON_TASK_PRIORITY = 2;
  if (hasDemoButtonPin()) xTaskCreate(buttonWatchTask, "button", BUTTON_TASK_STACK, nullptr, BUTTON_TASK_PRIORITY, nullptr);

  // Update screen: double tap, or "Check for updates now" on the setup page (before the restart).
  bool setupPageUpdate = updateRequest == UPDATE_REQUEST_MAGIC;
  updateRequest = 0;
  if (setupPageUpdate || updateScreenRequested) {
    runUpdateSession(setupPageUpdate ? OtaTrigger::SetupPage : OtaTrigger::DoubleTap);
    fullRefresh = true;  // the update screen covered the dashboard
    bootCount = 0;
  }
  otaBootCheck();
#ifdef OTA_TEST_HANG
  // Test build only (docs/OTA_TESTS.md): hang after "Updated to" in the first start after an
  // over-the-air install; the 3-minute timer must restart the board and roll back.
  if (otaPendingVerify) {
    logPrintln("[OTA test] hanging on purpose");
    for (;;) delay(1000);
  }
#endif


  // Not set up yet (no Wi-Fi): the setup hotspot.
  if (!wifi_ssid[0] || !wifi_pass[0]) {
    startAP();
    return;
  }
  DBG(F("--- dashboard wake ---"));

  // The configured template with the configured widgets. A changed layout (template, a widget on
  // or off) is drawn with a full refresh, everything fetched, so no ghosts of the old one stay.
  layouts::Result lay = loadLayout();
  if (lay.fellBack) DBGF("Layout \"%s\" not usable: Classic used", layout_template);
  uint32_t laySig = layouts::signature(lay);
  if (laySig != layoutSignature) {
    if (layoutSignature != 0 && !fullRefresh) DBG(F("Layout changed: full refresh"));
    fullRefresh = true;
    bootCount = 0;
    layoutSignature = laySig;
  }
  // Power (stage 6, src/core/PowerPlan.h): clock interval, night quiet hours, battery saver,
  // critical battery. Measured now, before Wi-Fi: the voltage doesn't sag yet.
  power::Inputs powerIn;
  {
    time_t t = time(nullptr);
    struct tm lt;
    if (t > 1700000000 && localtime_r(&t, &lt)) powerIn.minuteOfDay = lt.tm_hour * 60 + lt.tm_min;
    else powerIn.minuteOfDay = -1;
  }
  powerIn.clockInterval = clock_interval;
  powerIn.quietOn = quiet_on;
  powerIn.quietFrom = quiet_from;
  powerIn.quietTo = quiet_to;
  powerIn.batteryPct = batteryPercentNow();
  powerIn.usb = usbPowered();
  powerIn.saverWasOn = saverWasOn;
  powerIn.criticalWasOn = criticalShown;
  powerIn.buttonWake = buttonWoke;
  const power::Plan powerPlan = power::decide(powerIn);
  if (powerPlan.saverOn != saverWasOn)
    DBGF("[Power] battery saver %s (%d %%)", powerPlan.saverOn ? "on" : "off", powerIn.batteryPct);
  saverWasOn = powerPlan.saverOn;
  {
    power::Plan shown = powerPlan;
    if (powerPlan.night) shown.mode = power::Mode::Night;  // also after a tap at night
    power::note(shown, clock_interval, quiet_to, powerNote, sizeof(powerNote), ui_lang);
  }
  if (textHash(powerNote) != powerNoteShown) forceUpdateStatusBar = true;
  // The counter advances in steps of the wake interval: keep it on a multiple of it (the step
  // changes when the saver starts or the setting changes), so hourly fetches stay hourly.
  if (powerPlan.stepMin > 1 && bootCount % powerPlan.stepMin)
    bootCount = (uint16_t)(((bootCount + powerPlan.stepMin - 1) / powerPlan.stepMin * powerPlan.stepMin) % 10080);

  if (powerPlan.mode == power::Mode::Critical) {
    bool drew = false;
    if (!criticalShown || powerIn.buttonWake) {
      DBGF("[Power] battery critical (%d %%): charge screen, battery check once an hour", powerIn.batteryPct);
      drawCriticalScreen(readBattery());
      drew = true;
      criticalShown = true;
    }
    // Once an hour only the battery is read (nothing drawn): back to the dashboard once it charged.
    sleepNow((uint64_t)power::CRITICAL_CHECK_MIN * 60 * 1000000ULL, drew, true, drew);
  }
  if (criticalShown) {  // charged again: everything fresh
    criticalShown = false;
    fullRefresh = true;
    bootCount = 0;
  }
  if (powerPlan.mode == power::Mode::Night) {
    bool drew = false;
    if (!nightShown) {  // first wake of the night: the note in the status bar, then one long sleep
      DBGF("[Power] night until %02d:%02d", quiet_to / 60, quiet_to % 60);
      LayoutItem* bar = getLayout(box::STATUS);
      if (bar) {
        initDisplay(false);
        updatePartial(bar, drawStatus);
        drew = true;
      }
      nightShown = true;
    }
    struct timeval ntv;
    gettimeofday(&ntv, nullptr);
    sleepNow((uint64_t)power::nightSleepMicros(ntv.tv_sec, ntv.tv_usec, powerPlan.nightMinutesLeft, WAKE_AFTER_MINUTE_MS),
             drew, false, drew);
  }
  if (nightShown && !powerPlan.night) {  // morning: everything fresh
    DBG(F("[Power] quiet hours over: full refresh"));
    nightShown = false;
    fullRefresh = true;
    bootCount = 0;
  }

  // The boxes on the screen (nullptr: the widget isn't on this layout).
  LayoutItem* infoClock = getLayout(box::CLOCK);
  LayoutItem* infoEvent = getLayout(box::AGENDA);
  LayoutItem* infoStocks = getLayout(box::STOCKS);
  LayoutItem* infoOpenMeteo = getLayout(box::WEATHER);
  LayoutItem* infoBambu = getLayout(box::PRINTER);
  LayoutItem* infoTracking = getLayout(box::TRACKING);
  LayoutItem* infoBattery = getLayout(box::STATUS);
  LayoutItem* infoCalendar = getLayout(box::MONTH);

  time_t nowTs = time(nullptr);
  followNextFlight(nowTs);
  // Daily update check at night (installs or only marks, see runNightCheck).
  int otaTodayKey = 0;
  bool otaNightDue = !otaPendingVerify && otaNightCheckDue(&otaTodayKey);

  // What this wake does (src/core/WakePlan.h).
  WakeInputs wake;
  wake.now = nowTs;
  wake.bootCount = bootCount;
  wake.stepMin = powerPlan.stepMin;
  wake.fullRefresh = fullRefresh;
  wake.buttonWake = buttonWoke;
  // Present = on the layout and active (loadDefaultLayout adds only active items, all with
  // refresh > 0, so this equals the old "pointer not null" checks).
  auto refreshOf = [](LayoutItem* item) { return item && item->Active ? item->Refresh : 0; };
  wake.eventRefresh = refreshOf(infoEvent);
  wake.stocksRefresh = refreshOf(infoStocks);
  wake.weatherRefresh = refreshOf(infoOpenMeteo);
  wake.trackingRefresh = refreshOf(infoTracking);
  wake.batteryRefresh = refreshOf(infoBattery);
  wake.calendarRefresh = refreshOf(infoCalendar);
  wake.bambuRefresh = refreshOf(infoBambu);
  wake.hasIcsFeeds = hasIcsFeeds();
  wake.hasAerodataboxKey = aerodatabox_key[0] != '\0';
  wake.hasMakerworldUid = makerworld_uid[0] != '\0';
  wake.isPrinting = isPrinting;
  wake.previousIsPrinting = previousIsPrinting;
  wake.pendingBambuRetry = pendingBambuRetry;
  wake.hasPrinterMqtt = mqtt_ip[0] && mqtt_sn[0] && mqtt_pass[0];
  wake.bambuRetryAfter = bambuRetryAfter;
  wake.printViewOnScreen = printViewOnScreen;
  wake.showPrintView = showPrintView();
  wake.makerworldLastFetch = makerworldLastFetch;
  wake.lastNtpSync = lastNtpSync;
  wake.wifiFailures = wifiFailures;
  wake.wifiRetryAfter = wifiRetryAfter;
  wake.otaNightDue = otaNightDue;
  wake.otaPendingVerify = otaPendingVerify;
  wake.weatherRetry = retryDue(serviceStates[(int)Service::Weather], nowTs);
  wake.stocksRetry = retryDue(serviceStates[(int)Service::Stocks], nowTs);
  wake.calendarRetry = retryDue(serviceStates[(int)Service::Calendar], nowTs);
  wake.trackingRetry = retryDue(serviceStates[(int)Service::Tracking], nowTs);
  wake.flight.nextFlightStart = cal.nextFlight.start;
  wake.flight.calStart = liveFlight.calStart;
  wake.flight.calEnd = liveFlight.calEnd;
  wake.flight.depScheduled = liveFlight.depScheduled;
  wake.flight.depActual = liveFlight.depActual;
  wake.flight.arrScheduled = liveFlight.arrScheduled;
  wake.flight.arrActual = liveFlight.arrActual;
  wake.flight.lastFetch = liveFlight.lastFetch;
  wake.flight.requests = liveFlight.requests;
  wake.flight.valid = liveFlight.valid;
  wake.flight.finished = !strcmp(liveFlight.status, "Arrived") || !strcmp(liveFlight.status, "Canceled") ||
                         !strcmp(liveFlight.status, "Diverted");
  wake.flight.hasNumber = liveFlight.number[0] != '\0';
  const WakePlan plan = planWake(wake);

  bool eventIf = plan.event;
  bool stocksIf = plan.stocks;
  bool openMeteoIf = plan.weather;
  bool trackingIf = plan.tracking;
  bool calendarIf = plan.calendar;
  bool batteryIf = plan.battery;
  bool bambuIf = plan.bambu;
  bool bambuRetry = plan.bambuRetry;
  bool makerworldDue = plan.makerworld;
  bool flightStatusDue = plan.flightStatus;
  bool eventRedraw = plan.eventRedraw;
  bool ntpDue = plan.ntp;
  otaNightDue = plan.otaNight;
  bool needWiFi = plan.needWiFi;
  // Personal widgets: redraw when the date changed; the birthday link is read once a day.
  LayoutItem* infoTasks = getLayout(box::TASKS);
  LayoutItem* infoNote = getLayout(box::NOTE);
  LayoutItem* infoBirthdays = getLayout(box::BIRTHDAYS);
  const personal::Date today = localToday();
  const long todayNum = today.valid() ? personal::dayNumber(today) : 0;
  bool personalRedraw = todayNum && todayNum != personalDrawnDay && (infoTasks || infoNote || infoBirthdays);
  if (todayNum && todayNum != tasksPurgedDay) {
    tasksPurgedDay = todayNum;
    purgeOldDoneTasks();  // also when only done tasks are left (they don't set USER_HAS_TASKS)
  }
  const uint32_t bdayHash = textHash(bday_url);
  bool bdayFeedDue = infoBirthdays && bday_url[0] && feedDue(bdayFeed, bdayHash, todayNum, 1, nowTs);
  if (bdayFeedDue && !plan.wifiBackoff) needWiFi = true;
  else bdayFeedDue = false;
  // School holidays: only for the month view, our feed for the state or the user's own link.
  char schoolUrl[CFG_ICS_URL_MAX + 96];
  const bool schoolSet = school_on && school::feedUrl(schoolUrl, sizeof(schoolUrl), OTA_REPO, holiday_state, school_url);
  const uint32_t schoolHash = schoolSet ? textHash(schoolUrl) : 0;
  // Only the list of the current state / link is drawn: switched off or changed, the old one goes.
  if (!keepFeedData(schoolFeed, schoolSet, schoolHash)) {
    if (schoolHolidays.count) {
      schoolHolidays = school::SchoolList();
      calendarIf = true;
    }
    dropFeedData(schoolFeed);
  }
  bool schoolFeedDue = schoolSet && infoCalendar && feedDue(schoolFeed, schoolHash, todayNum, 7, nowTs);
  if (schoolFeedDue && !plan.wifiBackoff) needWiFi = true;
  else schoolFeedDue = false;
  if (needWiFi) logKeepThisWake();  // stored for the diagnostics view (quiet clock wakes aren't)
  bool makerworldRedraw = false;
  // A parcel delivered more than a day ago is still on screen: remove it without a download
  // (decided on the interval alone, before a Wi-Fi backoff, as before).
  bool trackingRedraw = infoTracking && !dueByWakes(wake.trackingRefresh, bootCount) && trackingDropExpired();
  if (plan.wifiBackoff) DBGF("WiFi backoff: next attempt in %ld s", plan.backoffRemaining);

  // The clock first, from the RTC, before the (slow) downloads: it changes within a few seconds of
  // the minute. A full refresh draws it with everything else.
  if (infoClock && !fullRefresh) {
    initDisplay(false);
    updatePartial(infoClock, updateClock);
    clockDrawnMinute = minuteNow();
    display.powerOff();  // the controller keeps its RAM for the next partial update
  }

  DBGF("Plan: printer %d (printing %d, before %d) weather %d stocks %d agenda %d month %d MakerWorld %d NTP %d full %d",
       bambuIf, isPrinting, previousIsPrinting, openMeteoIf, stocksIf, eventIf, calendarIf, makerworldDue, ntpDue,
       fullRefresh);

  if (needWiFi) {
    setCpuFrequencyMhz(cpuFullMhz);  // TLS at full speed: the radio is on for a shorter time
    const bool online = startWiFiReliable(wifi_ssid, wifi_pass);
    setProblem(PROBLEM_WIFI, !online);
    if (online) {
      otaWifiOk = true;
      wifiFailures = 0;
      initTime();

      if (bambuIf) {
        const bool bambuOk = fetchBambu(infoBambu);
        // Not answering (usually switched off): one quick retry at the next wake, then ask again
        // after 10, 20, 40, then 60 minutes. A refused access code won't fix itself: no retry.
        pendingBambuRetry = !bambuOk && !bambuAuthRejected && !bambuRetry;
        if (bambuOk) {
          bambuFailures = 0;
          bambuRetryAfter = 0;
        } else {
          if (bambuFailures < 255) bambuFailures++;
          if (bambuFailures > 1) pendingBambuRetry = false;
          bambuRetryAfter = time(nullptr) + bambuBackoffSeconds(bambuFailures);
          DBGF("[Bambu] next try in %ld min", (long)(bambuBackoffSeconds(bambuFailures) / 60));
        }
        setProblem(PROBLEM_PRINTER_CODE, !bambuOk && bambuAuthRejected);
        setProblem(PROBLEM_PRINTER, !bambuOk && !bambuAuthRejected);
        signalPrintTransition();
        forceUpdateStatusBar = true;
      }
      // The data services: a failed download keeps the box's picture (afterFetch).
      if (openMeteoIf)
        openMeteoIf = afterFetch(Service::Weather, fetchWeatherDirect(infoOpenMeteo), false, PROBLEM_WEATHER, infoOpenMeteo);
      if (stocksIf) {
        int failed = 0;
        const bool ok = fetchStocksDirect(&failed);
        stocksIf = afterFetch(Service::Stocks, ok, failed > 0, PROBLEM_STOCKS, infoStocks);
      }
      if ((eventIf || calendarIf) && hasIcsFeeds()) {
        int failed = 0;
        const bool ok = fetchCalendarIcs(clockZone(infoClock), &failed);
        if (!afterFetch(Service::Calendar, ok, failed > 0, PROBLEM_CALENDAR, infoEvent ? infoEvent : infoCalendar))
          eventIf = calendarIf = false;
      }
      if (bdayFeedDue) {
        const bool ok = fetchBirthdayFeed(device_timezone);
        setProblem(PROBLEM_BIRTHDAYS, !ok);
        feedResult(bdayFeed, bdayHash, ok, todayNum, time(nullptr), 3600);  // a broken link: once an hour
        if (ok) personalRedraw = true;
      }
      if (schoolFeedDue) {
        school::SchoolList fresh;
        const bool ok = fetchSchoolHolidays(schoolUrl, device_timezone, fresh);
        if (school::apply(schoolHolidays, fresh, ok, schoolHash != schoolFeed.appliedHash)) calendarIf = true;
        // A failure: again after 6 hours, no problem mark (the month view keeps the last list).
        feedResult(schoolFeed, schoolHash, ok, todayNum, time(nullptr), 6 * 3600);
      }
      if (trackingIf)
        trackingIf = afterFetch(Service::Tracking, fetchTrackingDirect(), false, PROBLEM_TRACKING, infoTracking);
      if (flightStatusDue) {
        const bool ok = fetchFlightStatus(clockZone(infoClock));
        liveFlight.lastFetch = time(nullptr);  // also after a failure: tried again at the next interval
        liveFlight.requests++;
        setProblem(PROBLEM_FLIGHT, !ok);
        if (ok && infoEvent) eventRedraw = true;  // the new status right away
      }

      // After the printer: a print that just started shows the printer instead of MakerWorld.
      if (makerworldDue && !showPrintView()) {
        makerworldRedraw = fetchMakerWorldDirect();
        makerworldLastFetch = time(nullptr);  // also after a failure: a wrong id is tried hourly
        setProblem(PROBLEM_MAKERWORLD, !makerworldRedraw);
      }
      // The time is usually synced while the downloads ran; a due re-sync waits a little longer.
      const bool timeOk = waitForNtpSync(ntpDue ? 5000 : 1000);
      if (timeOk) lastNtpSync = time(nullptr);
      if (timeOk || ntpDue) setProblem(PROBLEM_TIME, !timeOk);

      // Last, so the dashboard data is already fetched. An auto install restarts the board.
      if (otaNightDue && runNightCheck(otaTodayKey)) redrawAll = true;

    } else {
      DBG("WiFi failed: the screen keeps its content");
      wifiFailures = wifiFailures < 255 ? wifiFailures + 1 : 255;
      wifiRetryAfter = time(nullptr) + wifiBackoffSeconds(wifiFailures);  // 2, 4, 8, 16, 30 min
      if (!fullRefresh) openMeteoIf = stocksIf = eventIf = calendarIf = trackingIf = false;
      if (bambuIf) pendingBambuRetry = true;
    }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    setCpuFrequencyMhz(LOW_POWER_MHZ);
  }


  DBGF("Minute %d since the last full refresh", bootCount);
  // The printer while printing and 5 minutes after, MakerWorld's counters otherwise.
  const bool printView = showPrintView();
  if (bambuIf || makerworldRedraw) printViewOnScreen = printView;

  // Without a status bar the problems are a mark in a corner, drawn over a widget: a change of
  // them is drawn with a full refresh (rare), not as a partial window.
  if (!infoBattery && problems != problemsShown && !fullRefresh) {
    fullRefresh = true;
    // Widgets that weren't fetched this wake are drawn from their kept data (a full refresh
    // clears the whole screen; only the drawn parts come back).
    openMeteoIf = stocksIf = eventIf = calendarIf = trackingIf = makerworldRedraw = true;
  }

  initDisplay(fullRefresh);  // does nothing if the clock was drawn early
  if (fullRefresh) {
    DBG(F("Full refresh"));

    // "Updated to" after an update restart stays at least OTA_DONE_SCREEN_MS.
    while (otaUpdatedShownAt && millis() - otaUpdatedShownAt < OTA_DONE_SCREEN_MS) delay(50);

    display.setFullWindow();
    canvas().fillScreen(GxEPD_WHITE);
    display.firstPage();

    FullScreenParts parts;
    parts.printer = bambuIf || makerworldRedraw;
    parts.printView = printView;
    parts.weather = openMeteoIf;
    parts.agenda = eventIf || eventRedraw;
    parts.stocks = stocksIf;
    parts.month = calendarIf;
    parts.tracking = trackingIf || trackingRedraw;
    do {
      drawFullScreen(parts);
    } while (display.nextPage());
    personalDrawnDay = todayNum;

    DBG(F("Finish full"));
  }
  /* PARTIAL REFRESH */
  else {
    DBG(F("Partial refresh"));
    // Each box whose data changed, in this order (the clock was drawn before the downloads: again
    // only if the minute changed meanwhile). A box not on the layout (nullptr) is skipped.
    const struct {
      LayoutItem* box;
      bool due;
      void (*draw)(LayoutItem*);
    } redraws[] = {
        {infoClock, clockDrawnMinute != minuteNow(), updateClock},
        {infoOpenMeteo, openMeteoIf, weatherWidget},
        {infoBambu, bambuIf || makerworldRedraw, printView ? bambuWidget : makerWorldWidget},
        {infoEvent, eventIf || eventRedraw, gCalWidget},
        {infoStocks, stocksIf, stockWidget},
        {infoCalendar, calendarIf, drawCalendar},
        {infoTracking, trackingIf || trackingRedraw, trackingWidget},
        {infoBattery, batteryIf || forceUpdateStatusBar || problems != problemsShown, drawStatus},
        {infoTasks, personalRedraw, tasksWidget},
        {infoNote, personalRedraw, noteWidget},
        {infoBirthdays, personalRedraw, birthdaysWidget},
    };
    for (const auto& r : redraws)
      if (r.due && r.box) updatePartial(r.box, r.draw);
    if (personalRedraw) personalDrawnDay = todayNum;
  }
  // A double tap while the dashboard was refreshing: open the update screen now. A tap may still
  // be waiting for its second tap.
  unsigned long tapWait = millis();
  while (buttonBusy && millis() - tapWait < BUTTON_DOUBLE_TAP_MS + 200UL) delay(10);
  if (buttonMailbox == ButtonEvent::DoubleTap) {
    buttonMailbox = ButtonEvent::None;
    runUpdateSession(OtaTrigger::DoubleTap);
    redrawAll = true;
  }

  // A freshly installed firmware: confirm it now that Wi-Fi and the display worked (or roll back).
  otaConfirmIfPending();

  if (powerPlan.night) nightShown = true;  // a tap at night: the morning still gets its full refresh
  bootCount = (uint16_t)((bootCount + powerPlan.stepMin) % 10080);  // minutes (multiple of every interval)
  // Repeated at the end: after a deep-sleep wake the USB monitor misses the first lines.
  DBGF("Finished wake %d: firmware %s (build %d, %s), awake %lu ms", bootCount, FW_VERSION, FW_BUILD, FW_VARIANT, millis());

  // Next wake: on the next wall-clock boundary of the interval (every 5 min: :00, :05, ...), or at
  // the end of the quiet hours after a tap at night.
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  if (tv.tv_sec <= 1700000000) DBG(F("Time not set, sleeping 60s"));
  uint64_t sleep_us = powerPlan.night
                          ? (uint64_t)power::nightSleepMicros(tv.tv_sec, tv.tv_usec, powerPlan.nightMinutesLeft, WAKE_AFTER_MINUTE_MS)
                          : (uint64_t)power::alignedSleepMicros(tv.tv_sec, tv.tv_usec, powerPlan.stepMin, WAKE_AFTER_MINUTE_MS);
  if (redrawAll) sleep_us = 1000000ULL;  // redraw the dashboard right away
  DBGF("Done after %lu ms, next wake in %llu s", millis(), sleep_us / 1000000ULL);

  bool nextBootIsFullRefresh = (bootCount % (60 * 24) == 0);
  sleepNow(sleep_us, true, nextBootIsFullRefresh, fullRefresh || problems != problemsAtStart);
}
