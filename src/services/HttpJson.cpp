#include "Fetchers.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ctype.h>
#include <time.h>

#include "HttpJson.h"
#include "SecureClient.h"
#include "../widgets/MonthView.h"

// ----------------------- Helpers -----------------------

void urlEncode(const char* in, char* out, size_t cap) {
  static const char hex[] = "0123456789ABCDEF";
  size_t o = 0;
  for (; *in && o + 4 < cap; in++) {
    unsigned char c = (unsigned char)*in;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out[o++] = (char)c;
    } else {
      out[o++] = '%';
      out[o++] = hex[c >> 4];
      out[o++] = hex[c & 0x0F];
    }
  }
  out[o] = '\0';
}

bool httpGetJson(const char* url, JsonDocument& doc, const JsonDocument* filter) {
  SecureClient client;  // certificate checked with the CA bundle
  HTTPClient http;
  http.setTimeout(10000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(client, url)) return false;
  http.setUserAgent("Mozilla/5.0 (ESP32 eInk Dashboard)");

  int code = http.GET();
  if (code != 200) {
    DBGF("[Direct] HTTP %d for %s", code, url);
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();

  DeserializationError err = filter
    ? deserializeJson(doc, body, DeserializationOption::Filter(*filter))
    : deserializeJson(doc, body);
  if (err) {
    DBGF("[Direct] JSON error %s for %s", err.c_str(), url);
    return false;
  }
  return true;
}

