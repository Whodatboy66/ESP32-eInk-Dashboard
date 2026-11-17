#pragma once
// Internet time (SNTP). The clock widget applies the timezone.
void initTime();
// Waits for the SNTP client (started by initTime) to set the clock.
bool waitForNtpSync(unsigned long timeoutMs);
