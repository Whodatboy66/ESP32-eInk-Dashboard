#include "Logger.h"

#include <Arduino.h>
#include <stdarg.h>

#include "Redact.h"

namespace {
const char* const* registered = nullptr;
size_t registeredCount = 0;
}

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
  redactSecrets(line, sizeof(line), registered, registeredCount);
  Serial.print(line);
}
