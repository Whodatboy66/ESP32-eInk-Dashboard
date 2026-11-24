// Live status of the next flight (AeroDataBox via RapidAPI), stage 7b own code. The answer is read
// by ServiceParsers.h.
#include "Fetchers.h"

#include "HttpJson.h"
#include "ServiceParsers.h"

bool fetchFlightStatus(const char* tz) {
  if (!aerodatabox_key[0] || !liveFlight.number[0]) return false;
  // The API wants the local departure date: the calendar time in the dashboard's zone.
  setenv("TZ", (tz && tz[0]) ? tz : "UTC0", 1);
  tzset();
  struct tm lt;
  localtime_r(&liveFlight.calStart, &lt);
  char date[11], url[200];
  strftime(date, sizeof(date), "%Y-%m-%d", &lt);
  snprintf(url, sizeof(url),
           "https://aerodatabox.p.rapidapi.com/flights/Number/%s/%s?dateLocalRole=Departure&withAircraftImage=false"
           "&withLocation=false",
           liveFlight.number, date);
  JsonDocument filter;
  JsonObject f = filter[0].to<JsonObject>();
  f["status"] = true;
  for (const char* dir : {"departure", "arrival"}) {
    for (const char* t : {"scheduledTime", "revisedTime", "runwayTime"}) f[dir][t]["utc"] = true;
    f[dir]["gate"] = true;
    f[dir]["terminal"] = true;
  }
  const HttpHeader headers[] = {{"X-RapidAPI-Key", aerodatabox_key}, {"X-RapidAPI-Host", "aerodatabox.p.rapidapi.com"}};
  HttpJsonOptions opt;
  opt.filter = &filter;
  opt.headers = headers;
  opt.headerCount = 2;
  int code = 0;
  opt.httpCode = &code;
  JsonDocument doc;
  if (!httpGetJson(url, doc, opt)) {
    DBGF("[Flight] HTTP %d for %s on %s (204/404 = not found, 401/403 = key or plan)", code, liveFlight.number, date);
    return false;
  }
  if (!svc::readFlight(doc, liveFlight.calStart, liveFlight)) {
    DBGF("[Flight] No %s flight found on %s", liveFlight.number, date);
    return false;
  }
  DBGF("[Flight] %s %s, gate %s", liveFlight.number, liveFlight.status, liveFlight.depGate[0] ? liveFlight.depGate : "-");
  return true;
}
