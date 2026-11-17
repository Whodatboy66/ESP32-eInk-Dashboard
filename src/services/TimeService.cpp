#include "TimeService.h"

#include <Arduino.h>
#include <esp_sntp.h>
#include <time.h>

#include "../../configure.h"

// NTP sync. Timezone is set per-widget (clock) via TZ strings.
void initTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
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

