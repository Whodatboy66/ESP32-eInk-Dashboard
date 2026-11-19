#include "TimeService.h"

#include <Arduino.h>
#include <esp_sntp.h>
#include <time.h>

#include "../../app.h"

// NTP sync, keeping the device's time zone: configTime(0, 0, ...) would set TZ to UTC, and
// everything until the next widget set it again (log times, "today", edit mode) ran on UTC
// (seen in the diagnostics log of 2026-11-19: times jumping between 16:xx and 17:xx).
void initTime() {
  configTzTime(device_timezone[0] ? device_timezone : "UTC0", "pool.ntp.org", "time.nist.gov");
}



// Waits for the SNTP client (started by initTime) to set the clock.
bool waitForNtpSync(unsigned long timeoutMs) {
  // sntp_get_sync_status() resets COMPLETED back to RESET once read, so read it once per loop.
  unsigned long start = millis();
  bool ok = false;
  while (!(ok = sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) && millis() - start < timeoutMs)
    delay(50);
  DBGF("NTP %s after %lu ms", ok ? "synced" : "NOT synced", millis() - start);
  return ok;
}

