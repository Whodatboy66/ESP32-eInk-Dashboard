// Host test for src/services/ServiceParsers.h (stage 7b): the services' answers (Open-Meteo, Yahoo
// Finance, PKGE, AeroDataBox, MakerWorld) to the dashboard's data, with recorded answers.
// Run: tests/host/run.sh
#include <ArduinoJson.h>

#include <cstring>

#include "src/services/ServiceParsers.h"
#include "tests/host/check.h"

namespace {

JsonDocument json(const char* text) {
  JsonDocument d;
  deserializeJson(d, text);
  return d;
}

void test_time_and_url() {
  CHECK(svc::parseIsoUtc("2021-07-06T13:46:25+03:00") == svc::parseIsoUtc("2021-07-06T10:46:25Z"));
  CHECK_EQ((long)svc::parseIsoUtc("2026-10-15 18:05Z"), 1792087500L);
  CHECK_EQ((long)svc::parseIsoUtc("2026-10-15"), 1792022400L);
  CHECK_EQ((long)svc::parseIsoUtc("2026-10-15T18:05:30.123-02:30"), 1792087500L + 30 + 2 * 3600 + 1800);
  CHECK_EQ((long)svc::parseIsoUtc("nonsense"), 0L);
  CHECK_EQ((long)svc::parseIsoUtc(nullptr), 0L);
  char out[64];
  svc::urlEncode("São Paulo & Co/1", out, sizeof(out));
  CHECK_STR(out, "S%C3%A3o%20Paulo%20%26%20Co%2F1");
  svc::urlEncode("abcdefghij", out, 6);  // never cut inside an escape
  CHECK(std::strlen(out) < 6);
}

void test_weather() {
  float lat = 0, lon = 0;
  char city[64], cc[3];
  CHECK(svc::readPlaceSetting("48.14, 11.58", lat, lon, city, sizeof(city), cc) == svc::Place::Coordinates);
  CHECK(lat > 48.13f && lon < 11.59f);
  CHECK(svc::readPlaceSetting("München , de", lat, lon, city, sizeof(city), cc) == svc::Place::Name);
  CHECK_STR(city, "München");
  CHECK_STR(cc, "DE");
  CHECK(svc::readPlaceSetting("Paris", lat, lon, city, sizeof(city), cc) == svc::Place::Name);
  CHECK_STR(cc, "");
  CHECK(svc::readPlaceSetting("", lat, lon, city, sizeof(city), cc) == svc::Place::None);

  JsonDocument geo = json(R"({"results":[{"name":"München","latitude":48.137,"longitude":11.575,"country_code":"DE",
                              "admin1":"Bayern","country":"Deutschland"},{"name":"Munich","latitude":1,"longitude":2,
                              "country_code":"US","country":"USA"}]})");
  CHECK(svc::firstPlace(geo, lat, lon) && lat > 48.1f);
  PlaceResult places[3];
  CHECK_EQ(svc::readPlaces(geo, places, 3), 2);
  CHECK_STR(places[0].name, "München, DE");
  CHECK_STR(places[0].detail, "München, Bayern, Deutschland");
  CHECK_STR(places[1].detail, "Munich, USA");
  JsonDocument none = json(R"({"generationtime_ms":0.5})");
  CHECK(!svc::firstPlace(none, lat, lon));

  JsonDocument fc = json(R"({"daily":{"time":["2026-11-17","2026-11-18","2026-11-19"],"weather_code":[0,61,95],
                             "temperature_2m_max":[9.4,10.1,11],"temperature_2m_min":[2.2,3,4.6]}})");
  WeatherDay days[MAXWEATHERDAYS];
  CHECK_EQ(svc::readForecast(fc, days, 2), 2);
  CHECK(days[0].weather_code == 0 && days[0].icon == 0xF0599 && days[0].temp_max > 9.3f);
  CHECK(days[1].icon == 0xF0596);
  CHECK_EQ(svc::readForecast(none, days, 7), 0);
  CHECK_EQ((long)svc::weatherIcon(95), 0xF067EL);
  CHECK_EQ((long)svc::weatherIcon(1234), 0xF0590L);  // unknown: cloudy
}

void test_stocks() {
  JsonDocument q = json(R"({"chart":{"result":[{"meta":{"regularMarketPrice":98.12,"chartPreviousClose":97.6,
                           "regularMarketDayHigh":99.1,"regularMarketDayLow":96.16,"fiftyTwoWeekHigh":101,
                           "fiftyTwoWeekLow":80}}]}})");
  StockItem s{};
  CHECK(svc::readQuote(q, s));
  CHECK(s.price > 98.1f && s.prevClose > 97.5f && s.dayLow < 96.2f && s.fiftyTwoWeekHigh > 100.9f);
  JsonDocument prevOnly = json(R"({"chart":{"result":[{"meta":{"regularMarketPrice":5,"previousClose":4}}]}})");
  CHECK(svc::readQuote(prevOnly, s) && s.prevClose > 3.9f);  // chartPreviousClose missing
  JsonDocument err = json(R"({"chart":{"result":null,"error":{"code":"Not Found"}}})");
  CHECK(!svc::readQuote(err, s));
}

void test_parcels() {
  const time_t now = svc::parseIsoUtc("2026-11-17T09:00:00Z");
  JsonDocument p = json(R"({"code":200,"payload":[
      {"track_number":"JJD000390007712345678","status":5,"last_status_date":"2026-11-15T10:00:00+01:00"},
      {"track_number":"1Z999AA10123456784","status":3,"last_status_date":"2026-11-16T08:00:00Z",
       "est_delivery_date_from":"2026-11-18","est_delivery_date_to":"2026-11-19"},
      {"track_number":"LX123","status":5,"last_status_date":"2026-11-17T07:00:00Z"},
      {"track_number":"RR1","status":8,"last_status_date":"2026-11-17T06:00:00Z","est_delivery_date_from":"2026-11-17"}]})");
  TrackingItem out[MAX_TRACKING];
  int seen = 0;
  int n = svc::readParcels(p, now, out, MAX_TRACKING, &seen);
  CHECK_EQ(seen, 4);
  CHECK_EQ(n, 3);  // the one delivered two days ago is gone (kept for a day)
  CHECK_STR(out[0].trackNumber, "RR1");  // on their way first, newest checkpoint first
  CHECK_STR(out[1].trackNumber, "1Z999AA10123456784");
  CHECK(out[1].estDelivery == svc::parseIsoUtc("2026-11-19"));  // the end of the window
  CHECK_STR(out[2].trackNumber, "LX123");  // delivered today: still shown, last
  JsonDocument bad = json(R"({"code":401,"payload":"Unauthorized"})");
  CHECK_EQ(svc::readParcels(bad, now, out, MAX_TRACKING, &seen), -1);
}

void test_flight() {
  LiveFlight f{};
  f.calStart = svc::parseIsoUtc("2026-11-18T12:50:00Z");
  JsonDocument d = json(R"([
      {"status":"Arrived","departure":{"scheduledTime":{"utc":"2026-11-18 06:00Z"}},"arrival":{"scheduledTime":{"utc":"2026-11-18 09:00Z"}}},
      {"status":"Delayed","departure":{"scheduledTime":{"utc":"2026-11-18 13:00Z"},"revisedTime":{"utc":"2026-11-18 13:40Z"},
        "gate":"B42","terminal":"1"},"arrival":{"scheduledTime":{"utc":"2026-11-18 21:00Z"},"runwayTime":{"utc":"2026-11-18 21:35Z"}}}])");
  CHECK(svc::readFlight(d, f.calStart, f));
  CHECK_STR(f.status, "Delayed");  // the leg nearest to the calendar time
  CHECK(f.depActual == svc::parseIsoUtc("2026-11-18T13:40:00Z"));
  CHECK(f.arrActual == svc::parseIsoUtc("2026-11-18T21:35:00Z"));  // no revised time: the runway time
  CHECK_STR(f.depGate, "B42");
  CHECK(f.valid);
  LiveFlight g{};
  JsonDocument empty = json("[]");
  CHECK(!svc::readFlight(empty, 0, g) && !g.valid);
}

void test_makerworld() {
  JsonDocument d = json(R"({"total":2,"hits":[{"likeCount":10,"downloadCount":100,"printCount":7,"collectionCount":3,"boostCnt":1},
                           {"likeCount":5,"downloadCount":20,"printCount":1}]})");
  MakerWorld m{};
  CHECK(svc::readMakerWorld(d, m));
  CHECK(m.likes == 15 && m.downloads == 120 && m.prints == 8 && m.collections == 3 && m.boosts == 1 && m.available);
  JsonDocument bad = json(R"({"message":"not found"})");
  CHECK(!svc::readMakerWorld(bad, m));
}

}  // namespace

int main() {
  test_time_and_url();
  test_weather();
  test_stocks();
  test_parcels();
  test_flight();
  test_makerworld();
  return finish("service parsers");
}
