#pragma once
// Shared helpers of the fetchers: URL encoding and "GET a URL, parse JSON" over verified HTTPS.
#include <ArduinoJson.h>
#include <stddef.h>

void urlEncode(const char* in, char* out, size_t cap);
// GET url (verified TLS, redirects followed) and parse the JSON answer (optionally filtered).
bool httpGetJson(const char* url, JsonDocument& doc, const JsonDocument* filter = nullptr);
