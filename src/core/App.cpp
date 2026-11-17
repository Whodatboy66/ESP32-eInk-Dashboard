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
#include "../services/TimeService.h"
#include "../services/WifiService.h"
#include "../update/OtaFlow.h"
#include "../update/UpdatePolicy.h"
#include "../web/SetupServer.h"
#include "../widgets/Clock.h"
#include "../widgets/MonthView.h"
#include "../widgets/StatusBar.h"
#include "../widgets/Widgets.h"
#include "AppState.h"
#include "ConfigManager.h"
#include "Problems.h"
#include "WakePlan.h"

bool fullRefresh;  // this wake redraws the whole screen

// Minute the clock was drawn in this wake (redrawn if the minute changes during the network work).
static long clockDrawnMinute = -1;

// Minutes since the epoch: changes exactly when the displayed HH:MM changes.
static long currentMinute() {
  return (long)(time(nullptr) / 60);
}

// One wake: boot checks, button, update requests, plan (WakePlan), Wi-Fi + fetches, drawing, sleep.
void appSetup() {
  // Most wakes only redraw the clock: run them at 80 MHz, and go back to full speed only when
  // Wi-Fi is needed (TLS handshakes are CPU bound, so the radio would stay on longer at 80 MHz).
  cpuFullMhz = getCpuFrequencyMhz();
  setCpuFrequencyMhz(80);

  Serial.begin(115200);
  bool coldBoot = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED;
  // After a reset / flash (not a timer wake): watch the BOOT button for a few seconds (backup
  // way into setup mode). The wait also gives the USB serial monitor time to attach.
  [[maybe_unused]] bool bootButtonSetup = false;
#ifdef BOOT_BUTTON_PIN
  if (coldBoot) bootButtonSetup = waitForBootButtonSetup();
#elif DEBUG
  if (coldBoot) delay(3000);
#endif
#if CONFIG_IDF_TARGET_ESP32
  // Only after power-up / reset: waking from deep sleep needs no settle time, and this
  // ran on every minute's wake.
  if (coldBoot) delay(2000);
#endif

  if (activePinPreset == PinPreset::Esp32C6SuperMini) {
    forceSuperMiniLedsOff();
  }

  fullRefresh = (bootCount % (60 * 24) == 0);


  int wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0: Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1: Serial.println("Wakeup caused by external signal using RTC_CNTL"); fullRefresh=true; bootCount=0; break;
    case ESP_SLEEP_WAKEUP_TIMER: Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP: Serial.println("Wakeup caused by ULP program"); break;
    default: Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
  }
  // An update screen covered the dashboard during the last wake: redraw (and refetch) everything.
  if (redrawAll) {
    redrawAll = false;
    fullRefresh = true;
    bootCount = 0;
  }

  configLoad();
  DBGF("Firmware %s (build %d, %s, %s, settings schema %d), auto update %s, channel %s", FW_VERSION,
       FW_BUILD, FW_VARIANT, FW_DISPLAY, FW_CONFIG_SCHEMA, otaAutoInstall ? "on" : "off",
       ota::channelName(otaBeta));
  otaReadBootState();
  if (coldBoot) configCheckSerialFactoryReset();  // after otaReadBootState: confirms a pending update
#ifdef OTA_TEST_CRASH
  // Test build only (docs/OTA_TESTS.md): crash in the first start after an over-the-air install,
  // before the update can confirm itself. Never crashes when flashed over USB.
  if (otaPendingVerify) {
    Serial.println("[OTA test] crashing on purpose");
    Serial.flush();
    abort();
  }
#endif

  // Setup mode asked by a setup hold while the board was awake.
  if (setupRequest == SETUP_REQUEST_MAGIC) {
    setupRequest = 0;
    startAP();
    return;
  }

#ifdef BOOT_BUTTON_PIN
  // Setup mode asked by holding BOOT right after a reset (works without the touch sensor).
  if (bootButtonSetup) {
    if (isPinInActiveConfig(BOOT_BUTTON_PIN)) {
      Serial.printf("BOOT held, but GPIO%d is used by the pin map: ignored\n", BOOT_BUTTON_PIN);
    } else {
      Serial.println("BOOT held after reset -> AP config mode");
      startAP();
      return;
    }
  }
#endif

  // Check the button as early as possible, so the hold time matches what the user feels.
  bool updateScreenRequested = false;
  switch (readWakeButtonAction()) {
    case BUTTON_ACTION_AP:
      startAP();
      return;
    case BUTTON_ACTION_REBOOT:
      otaConfirmByUser("reboot hold");
      ESP.restart();
      return;
    case BUTTON_ACTION_UPDATE:
      updateScreenRequested = true;
      break;
    case BUTTON_ACTION_NONE:
    default:
      break;
  }
  if (hasDemoButtonPin()) {
    xTaskCreate(buttonWatchTask, "button", 3072, nullptr, 2, nullptr);
  }

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
    Serial.println("[OTA test] hanging on purpose");
    for (;;) delay(1000);
  }
#endif


  // Start an AP and server to configure it
  if (wifi_ssid[0] == '\0' || wifi_pass[0] == '\0') {
    startAP();
    return;
  }

  if (activePinPreset == PinPreset::Esp32C6SuperMini) {
    forceSuperMiniLedsOff();
  }

  DBG(F("============= STARTING ============="));

  loadDefaultLayout();
  LayoutItem* infoClock = getLayout(1);
  LayoutItem* infoEvent = getLayout(2);
  LayoutItem* infoStocks = getLayout(4);
  LayoutItem* infoOpenMeteo = getLayout(8);
  LayoutItem* infoBambu = getLayout(64);
  LayoutItem* infoTracking = getLayout(16);
  LayoutItem* infoBattery = getLayout(128);
  LayoutItem* infoCalendar = getLayout(256);

  time_t nowTs = time(nullptr);
  // Live flight status: follow the next flight from 6 hours before its departure until it lands.
  if (cal.nextFlight.start && cal.nextFlight.start != liveFlight.calStart &&
      cal.nextFlight.start - nowTs < FLIGHT_LIVE_BEFORE_S) {
    liveFlight = {};
    liveFlight.calStart = cal.nextFlight.start;
    liveFlight.calEnd = cal.nextFlight.end;
    snprintf(liveFlight.title, sizeof(liveFlight.title), "%s", cal.nextFlight.title);
    extractFlightNumber(liveFlight.title, liveFlight.number, sizeof(liveFlight.number));
  }
  // Daily update check at night (installs or only marks, see runNightCheck).
  int otaTodayKey = 0;
  bool otaNightDue = !otaPendingVerify && otaNightCheckDue(&otaTodayKey);

  // What this wake does (src/core/WakePlan.h).
  WakeInputs wake;
  wake.now = nowTs;
  wake.bootCount = bootCount;
  wake.fullRefresh = fullRefresh;
  wake.buttonWake = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1;
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
  wake.printViewOnScreen = printViewOnScreen;
  wake.showPrintView = showPrintView();
  wake.makerworldLastFetch = makerworldLastFetch;
  wake.lastNtpSync = lastNtpSync;
  wake.wifiFailures = wifiFailures;
  wake.wifiRetryAfter = wifiRetryAfter;
  wake.otaNightDue = otaNightDue;
  wake.otaPendingVerify = otaPendingVerify;
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
  bool makerworldRedraw = false;
  // A parcel delivered more than a day ago is still on screen: remove it without a download
  // (decided on the interval alone, before a Wi-Fi backoff, as before).
  bool trackingRedraw = infoTracking && !dueByWakes(wake.trackingRefresh, bootCount) && trackingDropExpired();
  if (plan.wifiBackoff) DBGF("WiFi backoff: next attempt in %ld s", plan.backoffRemaining);

  // Draw the clock right away from the RTC, before any (slow) network work, so it changes
  // within a couple of seconds of the minute. Full-refresh boots draw it with everything else.
  if (!fullRefresh && infoClock) {
    initDisplay(false);
    updatePartial(infoClock, updateClock);
    clockDrawnMinute = currentMinute();
    display.powerOff();  // keeps controller RAM; the panel is powered again for the next update
  }

  DBGF("Bambu %d Print %d Prev %d Weather %d Stocks %d Events %d Calendar %d MW %d NTP %d Full %d",
       bambuIf, isPrinting, previousIsPrinting, openMeteoIf, stocksIf, eventIf, calendarIf,
       makerworldDue, ntpDue, fullRefresh);

  if (needWiFi) {
    setCpuFrequencyMhz(cpuFullMhz);
    if (startWiFiReliable(wifi_ssid, wifi_pass)) {
      DBG("WiFi connected.");
      otaWifiOk = true;
      wifiFailures = 0;
      setProblem(PROBLEM_WIFI, false);
      initTime();

      if (bambuIf)
      {
        bool bambuOk = fetchBambu(infoBambu);
        // Retry once at the next wake; after that wait for the next scheduled poll (the printer is
        // probably off). A rejected access code won't fix itself: don't retry it at all.
        pendingBambuRetry = !bambuOk && !bambuAuthRejected && !bambuRetry;
        setProblem(PROBLEM_PRINTER_CODE, !bambuOk && bambuAuthRejected);
        setProblem(PROBLEM_PRINTER, !bambuOk && !bambuAuthRejected);
        signalPrintTransition();
        forceUpdateStatusBar = true;
      }

      // Weather, stocks and calendar come straight from Open-Meteo, Yahoo Finance and the iCal feeds.
      // On a failed fetch keep the old content on screen instead of drawing "unavailable".
      if (openMeteoIf) {
        bool ok = fetchWeatherDirect(infoOpenMeteo);
        setProblem(PROBLEM_WEATHER, !ok);
        if (!ok && !fullRefresh) openMeteoIf = false;
      }
      if (stocksIf) {
        int failed = 0;
        bool ok = fetchStocksDirect(&failed);
        setProblem(PROBLEM_STOCKS, !ok || failed > 0);
        if (!ok && !fullRefresh) stocksIf = false;
      }
      if ((eventIf || calendarIf) && hasIcsFeeds()) {
        const char* tz = (infoClock && infoClock->Extra1[0]) ? infoClock->Extra1 : device_timezone;
        int failed = 0;
        bool ok = fetchCalendarIcs(tz, &failed);
        setProblem(PROBLEM_CALENDAR, !ok || failed > 0);
        if (!ok && !fullRefresh) eventIf = calendarIf = false;
      }
      if (trackingIf) {
        bool ok = fetchTrackingDirect();
        setProblem(PROBLEM_TRACKING, !ok);
        if (!ok && !fullRefresh) trackingIf = false;
      }
      if (flightStatusDue) {
        const char* tz = (infoClock && infoClock->Extra1[0]) ? infoClock->Extra1 : device_timezone;
        bool ok = fetchFlightStatus(tz);
        liveFlight.lastFetch = time(nullptr);  // also after a failure: retried at the next interval
        liveFlight.requests++;
        setProblem(PROBLEM_FLIGHT, !ok);
        if (ok && infoEvent) eventRedraw = true;  // show the new status right away
      }

      // Re-check after the Bambu fetch: a print may have just started.
      if (makerworldDue && !showPrintView()) {
        makerworldRedraw = fetchMakerWorldDirect();
        makerworldLastFetch = time(nullptr);  // also after a failure, so a bad ID retries hourly, not every wake
        setProblem(PROBLEM_MAKERWORLD, !makerworldRedraw);
      }

      // Usually already done while the fetches ran; wait a bit longer when a re-sync is due.
      if (waitForNtpSync(ntpDue ? 5000 : 1000)) {
        lastNtpSync = time(nullptr);
        setProblem(PROBLEM_TIME, false);
      } else if (ntpDue) {
        setProblem(PROBLEM_TIME, true);
      }

      // Last, so the dashboard data is already fetched. An auto install restarts the board.
      if (otaNightDue && runNightCheck(otaTodayKey)) redrawAll = true;

    } else {
      DBG("WiFi FAILED → keeping the previous content");
      setProblem(PROBLEM_WIFI, true);
      if (wifiFailures < 255) wifiFailures++;
      wifiRetryAfter = time(nullptr) + wifiBackoffSeconds(wifiFailures);  // 2, 4, 8, 16, 30 min
      if (!fullRefresh) openMeteoIf = stocksIf = eventIf = calendarIf = trackingIf = false;
      if (bambuIf) pendingBambuRetry = true;
    }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    setCpuFrequencyMhz(80);
  }


  DBGF("Boot Count %d", bootCount);

  // Printer status while printing and for 5 minutes after; MakerWorld stats otherwise.
  bool printView = showPrintView();
  if (bambuIf || makerworldRedraw) printViewOnScreen = printView;

  // Start SPI and initialize display after deciding refresh mode (no-op if the early clock did it).
  initDisplay(fullRefresh);

  DBGF("What refresh %d", fullRefresh);
  /* FullREFRESH */
  if (fullRefresh) {
    DBG(F("Do the full refresh"));

    // "Updated to" after an update restart stays at least OTA_DONE_SCREEN_MS.
    while (otaUpdatedShownAt && millis() - otaUpdatedShownAt < OTA_DONE_SCREEN_MS) delay(50);

    display.setFullWindow();
    canvas().fillScreen(GxEPD_WHITE);
    display.firstPage();

    do {
      if (bambuIf || makerworldRedraw)
      {
        if (printView) bambuWidget(infoBambu);
        else           makerWorldWidget(infoBambu);
      }

      updateClock(infoClock);

      if (openMeteoIf) weatherWidget(infoOpenMeteo);

      if (infoBattery)
        drawStatus(infoBattery);
      DBG("HERE AFTER BATTERY");

      if (eventIf || eventRedraw) gCalWidget(infoEvent);
      if (stocksIf) stockWidget(infoStocks);
      if (calendarIf) drawCalendar(infoCalendar);
      if (trackingIf || trackingRedraw) trackingWidget(infoTracking);
    } while (display.nextPage());

    DBG(F("Finish full"));
  }
  /* PARTIAL REFRESH */
  else {
    DBG(F("Do Partial refresh"));

    // Already drawn before the network work; redraw only if the minute changed meanwhile.
    if (clockDrawnMinute != currentMinute())
      updatePartial(infoClock, updateClock);

    if (openMeteoIf)
      updatePartial(infoOpenMeteo, weatherWidget);

    if (bambuIf || makerworldRedraw)
      updatePartial(infoBambu, printView ? bambuWidget : makerWorldWidget);

    if (eventIf || eventRedraw)
      updatePartial(infoEvent, gCalWidget);

    if (stocksIf)
      updatePartial(infoStocks, stockWidget);

    if (calendarIf)
      updatePartial(infoCalendar, drawCalendar);

    if (trackingIf || trackingRedraw)
      updatePartial(infoTracking, trackingWidget);

    if (infoBattery && (batteryIf || forceUpdateStatusBar || problems != problemsShown))
      updatePartial(infoBattery, drawStatus);
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

  bootCount++;
  DBGF("Finished Boot %d\n", bootCount);

  // Wake WAKE_AFTER_MINUTE_MS after the next minute boundary (sub-second precision).
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  if (tv.tv_sec <= 1700000000) DBG(F("Time not set, sleeping 60s"));
  uint64_t sleep_us = (uint64_t)sleepMicros(tv.tv_sec, tv.tv_usec);
  if (redrawAll) sleep_us = 1000000ULL;  // redraw the dashboard right away
  DBGF("Awake %lu ms, sleeping %llu ms", millis(), sleep_us / 1000ULL);

  // hibernate() clears the controller's internal frame buffer used for partial refresh XOR.
  // Only use it when the next boot will be a full refresh (controller RAM is reloaded anyway).
  // For partial-refresh boots, powerOff() keeps the controller RAM intact.
  // bootCount was already incremented, so it is the next boot's value.
  bool nextBootIsFullRefresh = (bootCount % (60 * 24) == 0);
  if (nextBootIsFullRefresh) {
    display.hibernate();
  } else {
    display.powerOff();
  }

  if (hasDisplayPowerPin()) {
    gpio_hold_en((gpio_num_t)activePins.displayPower);
  }

  if (hasDemoButtonPin()) {
    const gpio_num_t buttonGpio = getDemoWakeGpio();
    // A press in progress: let the button task finish it (it restarts the board for a gesture, or
    // flags a fault after BUTTON_FAULT_MS), instead of sleeping and losing it. The pin is checked
    // too, for a press the task hasn't polled yet. Bounded in case the task isn't running.
    unsigned long waitStart = millis();
    while ((buttonBusy || digitalRead(activePins.demoButton) == HIGH) && !buttonFaulted &&
           millis() - waitStart < BUTTON_FAULT_MS + 1000UL) {
      delay(10);
    }
    if (digitalRead(activePins.demoButton) == HIGH) {
      // Stuck HIGH: a button wake would fire at once and keep the board awake. Timer wakes only,
      // until the input reads LOW again.
      DBG(F("[Button] input stuck HIGH: button wake off for this sleep"));
    } else {
      buttonFaulted = false;  // reads LOW: the sensor is fine again (the task may not have seen it yet)
      esp_sleep_enable_ext1_wakeup(
        (1ULL << buttonGpio),
        ESP_EXT1_WAKEUP_ANY_HIGH  // Wake when pin goes HIGH
      );
      // Keep the pull-down during deep sleep, so a missing sensor can't float HIGH and wake the
      // board. The sleep code latches it with the pad hold (released in configureButtonInput()).
      if (rtc_gpio_is_valid_gpio(buttonGpio)) {
        rtc_gpio_pullup_dis(buttonGpio);
        rtc_gpio_pulldown_en(buttonGpio);
      }
    }
  }

  esp_sleep_enable_timer_wakeup(sleep_us);

  esp_deep_sleep_start();
}
