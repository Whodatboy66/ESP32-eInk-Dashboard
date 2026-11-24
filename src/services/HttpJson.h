#pragma once
// "GET a URL, parse its JSON" over verified HTTPS (stage 7b, own code). The answer is parsed straight
// from the stream (a filter keeps only what's needed), so large answers (MakerWorld: about 1 MB)
// never sit in memory as a whole.
#include <ArduinoJson.h>
#include <stdint.h>

struct HttpHeader {
  const char* name;
  const char* value;
};

struct HttpJsonOptions {
  const JsonDocument* filter = nullptr;  // only these fields
  const HttpHeader* headers = nullptr;   // e.g. an API key
  int headerCount = 0;
  uint8_t nesting = 10;                  // deeper answers (skipped parts) need more
  int* httpCode = nullptr;               // the HTTP status (0 = no connection)
};

bool httpGetJson(const char* url, JsonDocument& doc, const HttpJsonOptions& opt = HttpJsonOptions());
