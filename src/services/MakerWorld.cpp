// MakerWorld design counters (Bambu's design service; makerworld.com itself is behind a browser
// check), stage 7b own code. The answer is read by ServiceParsers.h.
#include "Fetchers.h"

#include "HttpJson.h"
#include "ServiceParsers.h"

bool fetchMakerWorldDirect() {
  if (!makerworld_uid[0]) return false;
  char url[160];
  snprintf(url, sizeof(url), "https://api.bambulab.com/v1/design-service/published/%s/design?limit=100", makerworld_uid);
  JsonDocument filter;  // only the counters of each design (covers, titles, ... skipped)
  JsonObject f = filter["hits"][0].to<JsonObject>();
  for (const char* k : {"likeCount", "downloadCount", "printCount", "collectionCount", "boostCnt"}) f[k] = true;
  HttpJsonOptions opt;
  opt.filter = &filter;
  opt.nesting = 40;  // the skipped parts are nested deeper than the default 10
  unsigned long t0 = millis();
  JsonDocument doc;
  MakerWorld totals;
  if (!httpGetJson(url, doc, opt) || !svc::readMakerWorld(doc, totals)) return false;
  totals.lastUpdated = time(nullptr);
  makerworld = totals;
  saveMakerWorld();
  DBGF("[MakerWorld] %u designs in %lu ms: likes %d downloads %d prints %d boosts %d",
       (unsigned)doc["hits"].size(), millis() - t0, makerworld.likes, makerworld.downloads, makerworld.prints,
       makerworld.boosts);
  return true;
}
