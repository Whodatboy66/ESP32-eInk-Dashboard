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

// Waits for SNTP (started by initTime) to set the clock. The status is read once per round: reading
// COMPLETED resets it.
bool waitForNtpSync(unsigned long timeoutMs) {
  const unsigned long t0 = millis();
  bool synced = false;
  for (; millis() - t0 < timeoutMs; delay(50))
    if ((synced = sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED)) break;
  if (!synced) synced = sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED;
  DBGF("NTP %s after %lu ms", synced ? "synced" : "NOT synced", millis() - t0);
  return synced;
}
