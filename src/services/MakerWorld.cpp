#include "Fetchers.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ctype.h>
#include <time.h>

#include "HttpJson.h"
#include "SecureClient.h"
#include "ServiceUtil.h"
#include "../widgets/MonthView.h"

// ----------------------- MakerWorld (Bambu API) -----------------------

bool fetchMakerWorldDirect() {
  if (makerworld_uid[0] == '\0') return false;

  // makerworld.com is behind a browser check; Bambu's API host serves the same data directly.
  char url[160];
  snprintf(url, sizeof(url),
           "https://api.bambulab.com/v1/design-service/published/%s/design?limit=100", makerworld_uid);

  SecureClient client;  // certificate checked with the CA bundle
  HTTPClient http;
  http.useHTTP10(true);  // no chunked encoding, so the ~1 MB answer can be parsed as a stream
  http.setTimeout(15000);
  if (!http.begin(client, url)) return false;
  http.setUserAgent("Mozilla/5.0 (ESP32 eInk Dashboard)");

  int code = http.GET();
  if (code != 200) {
    DBGF("[MakerWorld] HTTP %d", code);
    http.end();
    return false;
  }

  // Keep only the counters of each design; everything else (covers, titles, ...) is skipped.
  JsonDocument filter;
  JsonObject f = filter["hits"][0].to<JsonObject>();
  f["likeCount"] = true;
  f["downloadCount"] = true;
  f["printCount"] = true;
  f["collectionCount"] = true;
  f["boostCnt"] = true;

  unsigned long t0 = millis();
  JsonDocument doc;
  // The skipped parts of each design are nested deeper than the default limit of 10.
  PatientStreamReader reader(http.getStreamPtr());
  DeserializationError err = deserializeJson(doc, reader,
                                             DeserializationOption::Filter(filter),
                                             DeserializationOption::NestingLimit(40));
  http.end();
  if (err) {
    DBGF("[MakerWorld] JSON error %s after %u bytes", err.c_str(), (unsigned)reader.total);
    return false;
  }

  JsonArray hits = doc["hits"];
  if (hits.isNull()) return false;

  makerworld = {};
  for (JsonObject h : hits) {
    makerworld.likes       += h["likeCount"] | 0;
    makerworld.downloads   += h["downloadCount"] | 0;
    makerworld.prints      += h["printCount"] | 0;
    makerworld.collections += h["collectionCount"] | 0;
    makerworld.boosts      += h["boostCnt"] | 0;
  }
  makerworld.lastUpdated = time(nullptr);
  makerworld.available = true;
  saveMakerWorld();

  DBGF("[MakerWorld] %u designs in %lu ms: likes %d downloads %d prints %d boosts %d",
       (unsigned)hits.size(), millis() - t0, makerworld.likes, makerworld.downloads,
       makerworld.prints, makerworld.boosts);
  return true;
}

