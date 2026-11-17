#pragma once
// Helpers shared by several services: date arithmetic, ISO 8601 parsing, a patient stream reader
// for large JSON downloads.
#include <Arduino.h>
#include <WiFi.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Days since 1970-01-01 for a civil date (proleptic Gregorian).
inline long daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  long era = (y >= 0 ? y : y - 399) / 400;
  long yoe = y - era * 400;
  long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}


// "2021-07-06T13:46:25+03:00", "2026-10-15 18:05Z", "...Z" or "2021-07-06" -> UTC epoch seconds
// (0 if invalid). Seconds, fractions and the offset are optional.
inline time_t parseIsoUtc(const char* s) {
  int y, mo, d, hh = 0, mm = 0, ss = 0, n = 0;
  if (!s || sscanf(s, "%4d-%2d-%2d", &y, &mo, &d) != 3) return 0;
  time_t t = (time_t)daysFromCivil(y, mo, d) * 86400;
  if (s[10] != 'T' && s[10] != ' ') return t;
  const char* p = s + 11;
  if (sscanf(p, "%2d:%2d%n", &hh, &mm, &n) < 2) return t;
  p += n;
  if (*p == ':') { ss = atoi(p + 1); p += 3; }
  while (*p == '.' || isdigit((unsigned char)*p)) p++;  // fractional seconds
  t += hh * 3600 + mm * 60 + ss;
  if (*p == '+' || *p == '-') {
    int oh = 0, om = 0;
    sscanf(p + 1, "%2d:%2d", &oh, &om);
    t -= (oh * 3600 + om * 60) * (*p == '+' ? 1 : -1);
  }
  return t;
}


// Reader for ArduinoJson that buffers the TLS stream and tolerates pauses in a large download
// (Stream::read() returning -1 mid-transfer would otherwise end the JSON as IncompleteInput).
class PatientStreamReader {
public:
  explicit PatientStreamReader(WiFiClient* s) : stream(s) {}

  int read() {
    if (pos >= len && !fill()) return -1;
    return buf[pos++];
  }

  size_t readBytes(char* out, size_t n) {
    size_t done = 0;
    while (done < n) {
      if (pos >= len && !fill()) break;
      size_t chunk = min(n - done, len - pos);
      memcpy(out + done, buf + pos, chunk);
      pos += chunk;
      done += chunk;
    }
    return done;
  }

  size_t total = 0;

private:
  WiFiClient* stream;
  uint8_t buf[1024];
  size_t len = 0, pos = 0;

  bool fill() {
    unsigned long start = millis();
    while (millis() - start < 10000) {
      int avail = stream->available();
      if (avail > 0) {
        len = stream->read(buf, min((size_t)avail, sizeof(buf)));
        pos = 0;
        if (len > 0) { total += len; return true; }
      } else if (!stream->connected()) {
        return false;
      }
      delay(2);
    }
    return false;
  }
};

