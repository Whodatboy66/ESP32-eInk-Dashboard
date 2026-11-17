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

// ----------------------- Live flight status (AeroDataBox) -----------------------

// Actual/estimated gate time (what airlines and flight apps show): the revised time, else the
// runway (take-off / touchdown) time; 0 when neither is known.
static time_t actualTime(JsonObject movement) {
  time_t t = parseIsoUtc(movement["revisedTime"]["utc"] | "");
  return t ? t : parseIsoUtc(movement["runwayTime"]["utc"] | "");
}

bool fetchFlightStatus(const char* tz) {
  if (aerodatabox_key[0] == '\0' || liveFlight.number[0] == '\0') return false;

  // The API wants the local departure date; use the dashboard's timezone for the calendar time.
  setenv("TZ", (tz && tz[0]) ? tz : "UTC0", 1);
  tzset();
  struct tm lt;
  localtime_r(&liveFlight.calStart, &lt);
  char date[11];
  strftime(date, sizeof(date), "%Y-%m-%d", &lt);

  char url[200];
  snprintf(url, sizeof(url),
           "https://aerodatabox.p.rapidapi.com/flights/Number/%s/%s"
           "?dateLocalRole=Departure&withAircraftImage=false&withLocation=false",
           liveFlight.number, date);

  SecureClient client;  // certificate checked with the CA bundle
  HTTPClient http;
  http.setTimeout(15000);
  if (!http.begin(client, url)) return false;
  http.addHeader("X-RapidAPI-Key", aerodatabox_key);
  http.addHeader("X-RapidAPI-Host", "aerodatabox.p.rapidapi.com");

  int code = http.GET();
  if (code != 200) {
    DBGF("[Flight] HTTP %d for %s on %s (204/404 = flight not found, 401/403 = key or plan)",
         code, liveFlight.number, date);
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();

  JsonDocument filter;
  JsonObject f = filter[0].to<JsonObject>();
  f["status"] = true;
  for (const char* dir : { "departure", "arrival" }) {
    f[dir]["scheduledTime"]["utc"] = true;
    f[dir]["revisedTime"]["utc"] = true;
    f[dir]["runwayTime"]["utc"] = true;
    f[dir]["gate"] = true;
    f[dir]["terminal"] = true;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body, DeserializationOption::Filter(filter));
  if (err) {
    DBGF("[Flight] JSON error %s", err.c_str());
    return false;
  }

  // A flight number can fly more than once a day: take the leg closest to the calendar time.
  JsonObject best;
  time_t bestDiff = 0;
  for (JsonObject fl : doc.as<JsonArray>()) {
    time_t dep = parseIsoUtc(fl["departure"]["scheduledTime"]["utc"] | "");
    time_t diff = dep > liveFlight.calStart ? dep - liveFlight.calStart : liveFlight.calStart - dep;
    if (best.isNull() || diff < bestDiff) { best = fl; bestDiff = diff; }
  }
  if (best.isNull()) {
    DBGF("[Flight] No %s flight found on %s", liveFlight.number, date);
    return false;
  }

  JsonObject dep = best["departure"], arr = best["arrival"];
  snprintf(liveFlight.status, sizeof(liveFlight.status), "%s", best["status"] | "Unknown");
  liveFlight.depScheduled = parseIsoUtc(dep["scheduledTime"]["utc"] | "");
  liveFlight.depActual = actualTime(dep);
  liveFlight.arrScheduled = parseIsoUtc(arr["scheduledTime"]["utc"] | "");
  liveFlight.arrActual = actualTime(arr);
  snprintf(liveFlight.depGate, sizeof(liveFlight.depGate), "%s", dep["gate"] | "");
  snprintf(liveFlight.depTerminal, sizeof(liveFlight.depTerminal), "%s", dep["terminal"] | "");
  liveFlight.valid = true;

  DBGF("[Flight] %s %s, gate %s", liveFlight.number, liveFlight.status,
       liveFlight.depGate[0] ? liveFlight.depGate : "-");
  DBGF("[Flight] UTC dep sched %s rev %s rwy %s | arr sched %s rev %s rwy %s",
       dep["scheduledTime"]["utc"] | "-", dep["revisedTime"]["utc"] | "-", dep["runwayTime"]["utc"] | "-",
       arr["scheduledTime"]["utc"] | "-", arr["revisedTime"]["utc"] | "-", arr["runwayTime"]["utc"] | "-");
  return true;
}
