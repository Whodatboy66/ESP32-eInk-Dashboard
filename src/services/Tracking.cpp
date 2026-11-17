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

// ----------------------- Package tracking (PKGE) -----------------------

#define TRACKING_KEEP_DELIVERED_S  86400  // delivered parcels stay visible for 1 day

static bool trackingExpired(const TrackingItem& it, time_t now) {
  return it.status == 5 && now - it.lastDate > TRACKING_KEEP_DELIVERED_S;
}

// Sort order: parcels still on their way first, then by most recent checkpoint.
static bool trackingBefore(const TrackingItem& a, const TrackingItem& b) {
  bool aDone = a.status == 5, bDone = b.status == 5;
  if (aDone != bDone) return !aDone;
  return a.lastDate > b.lastDate;
}

bool trackingDropExpired() {
  time_t now = time(nullptr);
  if (now < 1700000000) return false;  // clock not set yet
  int kept = 0;
  for (int i = 0; i < trackingCount; i++)
    if (!trackingExpired(tracking[i], now)) tracking[kept++] = tracking[i];
  bool dropped = kept != trackingCount;
  trackingCount = kept;
  return dropped;
}

bool fetchTrackingDirect() {
  if (pkge_api_key[0] == '\0') {
    trackingCount = 0;
    return false;
  }

  SecureClient client;  // certificate checked with the CA bundle
  HTTPClient http;
  http.useHTTP10(true);  // no chunked encoding, so the answer can be parsed as a stream
  http.setTimeout(15000);
  if (!http.begin(client, "https://api.pkge.net/v1/packages/list?per-page=100")) return false;
  http.addHeader("X-Api-Key", pkge_api_key);

  int code = http.GET();
  if (code != 200) {
    DBGF("[Tracking] HTTP %d (401 = wrong API key)", code);
    http.end();
    return false;
  }

  // Keep only what the widget shows; checkpoints and addresses are skipped.
  JsonDocument filter;
  JsonObject f = filter["payload"][0].to<JsonObject>();
  f["track_number"] = true;
  f["status"] = true;
  f["last_status_date"] = true;
  f["est_delivery_date_from"] = true;
  f["est_delivery_date_to"] = true;

  JsonDocument doc;
  PatientStreamReader reader(http.getStreamPtr());
  DeserializationError err = deserializeJson(doc, reader,
                                             DeserializationOption::Filter(filter),
                                             DeserializationOption::NestingLimit(40));
  http.end();
  if (err) {
    DBGF("[Tracking] JSON error %s", err.c_str());
    return false;
  }

  JsonArray list = doc["payload"];
  if (list.isNull()) {
    DBG(F("[Tracking] Unexpected answer (no payload list)"));
    return false;
  }

  // Built aside and copied at the end, so a failed fetch keeps the previous list.
  TrackingItem found[MAX_TRACKING];
  int count = 0;
  time_t now = time(nullptr);
  int seen = 0;
  for (JsonObject p : list) {
    seen++;
    TrackingItem it = {};
    snprintf(it.trackNumber, sizeof(it.trackNumber), "%s", p["track_number"] | "");
    it.status = p["status"] | 0;
    it.lastDate = parseIsoUtc(p["last_status_date"] | "");
    it.estDelivery = parseIsoUtc(p["est_delivery_date_to"] | (p["est_delivery_date_from"] | ""));
    if (trackingExpired(it, now)) continue;

    // Insert in sorted position, keeping only the first MAX_TRACKING.
    int pos = count;
    while (pos > 0 && trackingBefore(it, found[pos - 1])) pos--;
    if (pos >= MAX_TRACKING) continue;
    int last = count < MAX_TRACKING ? count : MAX_TRACKING - 1;
    for (int i = last; i > pos; i--) found[i] = found[i - 1];
    found[pos] = it;
    if (count < MAX_TRACKING) count++;
  }

  memcpy(tracking, found, sizeof(TrackingItem) * count);
  trackingCount = count;

  DBGF("[Tracking] %d packages in the account, showing %d", seen, trackingCount);
  return true;
}

