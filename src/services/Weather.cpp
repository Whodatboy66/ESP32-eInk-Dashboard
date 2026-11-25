// Weather (Open-Meteo) and the place search of the setup app (stage 7b, own code). The answers are
// read by ServiceParsers.h.
#include "Fetchers.h"

#include "HttpJson.h"
#include "ServiceParsers.h"
#include "../layout/LayoutEngine.h"

// A city's coordinates survive deep sleep, so the name is looked up once.
RTC_DATA_ATTR static float cachedLat = 0.0f;
RTC_DATA_ATTR static float cachedLon = 0.0f;
RTC_DATA_ATTR static char cachedLocation[CFG_WEATHER_LOC_MAX] = "";

static bool resolveLocation(float& lat, float& lon) {
  char city[CFG_WEATHER_LOC_MAX], country[3];
  svc::Place kind = svc::readPlaceSetting(weather_location, lat, lon, city, sizeof(city), country);
  if (kind == svc::Place::None) return false;
  if (kind == svc::Place::Coordinates) return true;
  if (!strcmp(cachedLocation, weather_location)) {
    lat = cachedLat;
    lon = cachedLon;
    return true;
  }
  char encoded[CFG_WEATHER_LOC_MAX * 3], url[320];
  svc::urlEncode(city, encoded, sizeof(encoded));
  snprintf(url, sizeof(url), "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=1&format=json%s%s",
           encoded, country[0] ? "&countryCode=" : "", country);
  JsonDocument doc;
  if (!httpGetJson(url, doc)) return false;
  if (!svc::firstPlace(doc, lat, lon)) {
    DBGF("[Weather] Location not found: %s", weather_location);
    return false;
  }
  cachedLat = lat;
  cachedLon = lon;
  snprintf(cachedLocation, sizeof(cachedLocation), "%s", weather_location);
  DBGF("[Weather] %s -> %.4f,%.4f", weather_location, lat, lon);
  return true;
}

bool fetchWeatherDirect(LayoutItem* item) {
  float lat, lon;
  if (!resolveLocation(lat, lon)) return false;
  // As many days as the widget shows (the drawing's rule, src/layout/LayoutEngine.h).
  int days = MAXWEATHERDAYS;
  if (item && item->ColWidth > 0) days = constrain(layouts::weatherDaysFor(item->Width, item->ColWidth), 1, MAXWEATHERDAYS);
  char url[320];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min&forecast_days=%d&timezone=auto",
           lat, lon, days);
  JsonDocument doc;
  if (!httpGetJson(url, doc)) return false;
  WeatherData fresh{};  // the kept forecast stays until a new one is read (stage 8 data store)
  fresh.count = svc::readForecast(doc, fresh.day, days);
  DBGF("[Weather] %d days", fresh.count);
  if (fresh.count <= 0) return false;
  weather = fresh;
  return true;
}

int searchPlaces(const char* query, PlaceResult* out, int max) {
  char encoded[CFG_WEATHER_LOC_MAX * 3], url[320];
  svc::urlEncode(query, encoded, sizeof(encoded));
  snprintf(url, sizeof(url), "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=%d&format=json&language=%s",
           encoded, max, i18n::langCode(ui_lang));
  JsonDocument doc;
  if (!httpGetJson(url, doc)) return -1;
  return svc::readPlaces(doc, out, max);
}
