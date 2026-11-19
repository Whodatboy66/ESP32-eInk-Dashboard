#pragma once
// The log (DBGF in configure.h, and every former Serial.print): on USB as before, secrets masked
// (security finding S6, Redact.h), and collected per wake for the diagnostics view of the setup
// app (docs/plans/STAGE5B_DIAGNOSTICS.md), stored on the userdata partition by logFlush().
#include <stddef.h>

// Secrets to mask: pointers to buffers (the settings), so later changes are covered too.
void logRegisterSecrets(const char* const* secrets, size_t count);
// printf to the log, secrets masked. Lines longer than 511 characters are cut.
void logPrintf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// Serial.print / println replacements (the same text goes to USB and the log).
#ifdef ARDUINO
#include <Arduino.h>
void logPrint(const char* s);
void logPrint(const __FlashStringHelper* s);
void logPrint(const String& s);
void logPrintln(const char* s = "");
void logPrintln(const __FlashStringHelper* s);
void logPrintln(const String& s);
#ifndef RENDER_ON_PC
#include <IPAddress.h>
void logPrintln(const IPAddress& ip);
#endif
#endif

// This wake did something worth keeping (Wi-Fi, a problem, a full refresh, setup mode): logFlush()
// stores it. Quiet clock wakes are not stored (flash wear, awake time).
void logKeepThisWake();
// Appends the collected lines to /log.txt (rotated to /log.1 at 16 KB) if logKeepThisWake() was
// called, or always with force. Call before deep sleep / restarts.
void logFlush(bool force = false);
// The stored log plus what this wake collected so far (plain text), at most cap - 1 bytes, the
// newest part. Returns the length.
size_t logReadAll(char* out, size_t cap);
// Erases the stored log.
void logClear();

// Battery history (one line "unix,millivolts,usb" per hour, last 7 days in /battery.csv).
// Call on every wake: samples only when an hour has passed (needs the clock).
void batteryHistorySample();
