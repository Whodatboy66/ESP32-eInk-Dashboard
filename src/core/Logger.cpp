#include "Logger.h"

#include <Arduino.h>
#include <esp_attr.h>
#include <stdarg.h>
#include <time.h>

#include <mutex>
#include <new>

#include "../../configure.h"
#include "../hardware/BatteryMonitor.h"
#include "../personal/UserStore.h"
#include "LogBuffer.h"
#include "Redact.h"

namespace {
const char* const* registered = nullptr;
size_t registeredCount = 0;

// On the heap, made at the first log line (the classic ESP32 has little static RAM left); without
// it (no memory) the log still goes to USB.
using WakeLog = LogBuffer<4096>;
WakeLog* wakeLogP = nullptr;
WakeLog* wakeLog() {
  if (!wakeLogP) wakeLogP = new (std::nothrow) WakeLog();
  return wakeLogP;
}
bool keep = false;
RTC_DATA_ATTR long storedLogDay = 0;       // date of the last stored lines (a date line when it changes)
RTC_DATA_ATTR time_t lastBatterySample = 0;

constexpr const char* LOG_FILE = "/log.txt";
constexpr const char* LOG_OLD = "/log.1";
constexpr size_t LOG_MAX = 16 * 1024;
constexpr const char* BATTERY_FILE = "/battery.csv";
constexpr int BATTERY_KEEP = 7 * 24;

bool clockSet(time_t now) { return now > 1700000000; }

// "09:41:20 ", or "+12.345 s " before the clock is set. A date line first when the day changed.
void linePrefix(char* out, size_t cap) {
  time_t now = time(nullptr);
  if (!clockSet(now)) {
    snprintf(out, cap, "+%lu.%03lu s ", millis() / 1000, millis() % 1000);
    return;
  }
  struct tm lt;
  localtime_r(&now, &lt);
  snprintf(out, cap, "%02d:%02d:%02d ", lt.tm_hour, lt.tm_min, lt.tm_sec);
}

// Every output: masked, to USB, into this wake's log. The button task logs too: one writer at a
// time (a mutex, not a critical section: the time prefix, localtime_r, may block).
void emit(const char* text) {
  static std::mutex lock;
  std::lock_guard<std::mutex> guard(lock);
  static char masked[512];
  snprintf(masked, sizeof(masked), "%s", text);
  redactSecrets(masked, sizeof(masked), registered, registeredCount);
  Serial.print(masked);
  if (WakeLog* w = wakeLog()) w->append(masked, linePrefix);
}
}  // namespace

void logRegisterSecrets(const char* const* secrets, size_t count) {
  registered = secrets;
  registeredCount = count;
}

void logPrintf(const char* fmt, ...) {
  char line[512];
  va_list args;
  va_start(args, fmt);
  vsnprintf(line, sizeof(line), fmt, args);
  va_end(args);
  emit(line);
}

void logPrint(const char* s) { emit(s ? s : ""); }
void logPrint(const __FlashStringHelper* s) { emit(reinterpret_cast<const char*>(s)); }
void logPrint(const String& s) { emit(s.c_str()); }
void logPrintln(const char* s) {
  emit(s ? s : "");
  emit("\n");
}
void logPrintln(const __FlashStringHelper* s) { logPrintln(reinterpret_cast<const char*>(s)); }
void logPrintln(const String& s) { logPrintln(s.c_str()); }
void logPrintln(const IPAddress& ip) { logPrintln(ip.toString()); }

void logKeepThisWake() { keep = true; }

void logFlush(bool force) {
  WakeLog* w = wakeLog();
  if (!w) return;
  if ((!keep && !force) || w->empty()) {
    w->clear();
    return;
  }
  if (userFileSize(LOG_FILE) + w->size() > LOG_MAX) userFileRotate(LOG_FILE, LOG_OLD);
  // A date line when the day changed, so the stored times can be read (also after a rotation).
  time_t now = time(nullptr);
  if (clockSet(now)) {
    struct tm lt;
    localtime_r(&now, &lt);
    long day = (lt.tm_year + 1900) * 1000L + lt.tm_yday;
    if (day != storedLogDay || userFileSize(LOG_FILE) == 0) {
      char line[40];
      int n = snprintf(line, sizeof(line), "--- %04d-%02d-%02d ---\n", lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
      appendUserFile(LOG_FILE, line, n);
      storedLogDay = day;
    }
  }
  appendUserFile(LOG_FILE, w->data(), w->size());
  w->clear();
  keep = false;
}

size_t logReadAll(char* out, size_t cap) {
  if (!cap) return 0;
  // Oldest first: the previous file, the current one, then this wake's lines; the newest part
  // wins when it doesn't all fit.
  size_t len = 0;
  out[0] = '\0';
  auto add = [&](const char* text, size_t n) {
    if (n >= cap) {
      text += n - (cap - 1);
      n = cap - 1;
    }
    if (len + n >= cap) {  // drop the oldest part
      size_t drop = len + n - (cap - 1);
      memmove(out, out + drop, len - drop);
      len -= drop;
    }
    memcpy(out + len, text, n);
    len += n;
    out[len] = '\0';
  };
  for (const char* path : {LOG_OLD, LOG_FILE}) {
    char* buf = static_cast<char*>(malloc(LOG_MAX + 1));
    if (!buf) break;
    size_t n = readUserFile(path, buf, LOG_MAX + 1);
    if (n) add(buf, n);
    free(buf);
  }
  if (WakeLog* w = wakeLog()) add(w->data(), w->size());
  return len;
}

void logClear() {
  removeUserFile(LOG_FILE);
  removeUserFile(LOG_OLD);
  if (WakeLog* w = wakeLog()) w->clear();
  storedLogDay = 0;
}

void batteryHistorySample() {
  time_t now = time(nullptr);
  if (!clockSet(now) || !hasBatteryPin()) return;
  if (lastBatterySample && now - lastBatterySample < 3600 && now >= lastBatterySample) return;
  lastBatterySample = now;
  char line[48];
  int mv = (int)(readBattery() * 1000.0f + 0.5f);
  int n = snprintf(line, sizeof(line), "%ld,%d,%d\n", (long)now, mv, usbPowered() ? 1 : 0);
  appendUserFile(BATTERY_FILE, line, n);
  // Keep the last 7 days: trimmed when the file has grown a day past that.
  if (userFileSize(BATTERY_FILE) > (BATTERY_KEEP + 24) * 20) {
    char* buf = static_cast<char*>(malloc(8 * 1024));
    if (!buf) return;
    size_t len = readUserFile(BATTERY_FILE, buf, 8 * 1024);
    if (len) {
      len = keepLastLines(buf, len, BATTERY_KEEP);
      writeUserFile(BATTERY_FILE, buf, len);
    }
    free(buf);
  }
}
