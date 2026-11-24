// Parcels in the PKGE account (stage 7b, own code). The answer is read by ServiceParsers.h.
#include "Fetchers.h"

#include "HttpJson.h"
#include "ServiceParsers.h"

bool trackingDropExpired() {
  time_t now = time(nullptr);
  if (now < 1700000000) return false;  // clock not set yet
  int kept = 0;
  for (int i = 0; i < trackingCount; i++)
    if (!svc::parcelExpired(tracking[i], now)) tracking[kept++] = tracking[i];
  bool dropped = kept != trackingCount;
  trackingCount = kept;
  return dropped;
}

bool fetchTrackingDirect() {
  if (!pkge_api_key[0]) {
    trackingCount = 0;
    return false;
  }
  JsonDocument filter;  // checkpoints and addresses skipped
  JsonObject f = filter["payload"][0].to<JsonObject>();
  for (const char* k : {"track_number", "status", "last_status_date", "est_delivery_date_from", "est_delivery_date_to"})
    f[k] = true;
  const HttpHeader key = {"X-Api-Key", pkge_api_key};
  HttpJsonOptions opt;
  opt.filter = &filter;
  opt.headers = &key;
  opt.headerCount = 1;
  opt.nesting = 40;
  int code = 0;
  opt.httpCode = &code;
  JsonDocument doc;
  if (!httpGetJson("https://api.pkge.net/v1/packages/list?per-page=100", doc, opt)) {
    if (code == 401) DBG(F("[Tracking] 401: wrong API key"));
    return false;
  }
  // Built aside: a failed answer keeps the list on screen.
  TrackingItem found[MAX_TRACKING];
  int seen = 0;
  int n = svc::readParcels(doc, time(nullptr), found, MAX_TRACKING, &seen);
  if (n < 0) {
    DBG(F("[Tracking] Unexpected answer (no payload list)"));
    return false;
  }
  memcpy(tracking, found, sizeof(TrackingItem) * n);
  trackingCount = n;
  DBGF("[Tracking] %d packages in the account, showing %d", seen, trackingCount);
  return true;
}
