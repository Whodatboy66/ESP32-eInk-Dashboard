// Download and parse JSON (see HttpJson.h).
#include "HttpJson.h"

#include <HTTPClient.h>
#include <WiFi.h>

#include "../../app.h"
#include "SecureClient.h"

namespace {

// Feeds ArduinoJson from the TLS stream in chunks and waits through pauses in a long download
// (Stream::read() returning -1 in the middle would otherwise end the JSON as IncompleteInput).
class StreamFeed {
 public:
  explicit StreamFeed(WiFiClient* s) : stream(s) {}
  int read() { return (pos < len || refill()) ? buf[pos++] : -1; }
  size_t readBytes(char* out, size_t n) {
    size_t done = 0;
    while (done < n && (pos < len || refill())) {
      size_t chunk = len - pos < n - done ? len - pos : n - done;
      memcpy(out + done, buf + pos, chunk);
      pos += chunk;
      done += chunk;
    }
    return done;
  }
  size_t total = 0;

 private:
  WiFiClient* stream;
  uint8_t buf[512];
  size_t len = 0, pos = 0;
  bool refill() {
    for (unsigned long start = millis(); millis() - start < 10000; delay(2)) {
      int avail = stream->available();
      if (avail > 0) {
        const int got = stream->read(buf, (size_t)avail < sizeof(buf) ? (size_t)avail : sizeof(buf));
        if (got < 0) return false;  // a TLS read error: never a negative length
        len = (size_t)got;
        pos = 0;
        if (len > 0) {
          total += len;
          return true;
        }
      } else if (!stream->connected()) {
        return false;
      }
    }
    return false;
  }
};

// The host of a URL for the log (paths and queries may hold keys or user ids).
void logFailure(const char* what, const char* url, int code) {
  const char* host = strstr(url, "://");
  host = host ? host + 3 : url;
  DBGF("[HTTP] %s %d for %.*s", what, code, (int)strcspn(host, "/?"), host);
}

}  // namespace

bool httpGetJson(const char* url, JsonDocument& doc, const HttpJsonOptions& opt) {
  if (opt.httpCode) *opt.httpCode = 0;
  SecureClient client;  // certificate checked with the CA bundle
  HTTPClient http;
  http.useHTTP10(true);  // no chunked encoding: the stream is the body
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(client, url)) return false;
  http.setUserAgent("Mozilla/5.0 (ESP32 eInk Dashboard)");
  for (int i = 0; i < opt.headerCount; i++) http.addHeader(opt.headers[i].name, opt.headers[i].value);
  int code = http.GET();
  if (opt.httpCode) *opt.httpCode = code;
  if (code != 200) {
    logFailure("HTTP", url, code);
    http.end();
    return false;
  }
  WiFiClient* stream = http.getStreamPtr();
  if (!stream) {  // the connection ended right after the headers (an empty answer, Wi-Fi gone)
    DBG(F("[HTTP] no answer body"));
    http.end();
    return false;
  }
  StreamFeed feed(stream);
  DeserializationError err =
      opt.filter ? deserializeJson(doc, feed, DeserializationOption::Filter(*opt.filter),
                                   DeserializationOption::NestingLimit(opt.nesting))
                 : deserializeJson(doc, feed, DeserializationOption::NestingLimit(opt.nesting));
  http.end();
  if (err) {
    DBGF("[HTTP] JSON %s after %u bytes", err.c_str(), (unsigned)feed.total);
    return false;
  }
  return true;
}
