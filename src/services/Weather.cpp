#include "Fetchers.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ctype.h>
#include <time.h>

#include "HttpJson.h"
#include "SecureClient.h"
#include "../layout/LayoutEngine.h"
#include "../widgets/MonthView.h"

// Coordinates resolved from a city name survive deep sleep, so geocoding runs only once.
RTC_DATA_ATTR static float cachedLat = 0.0f;
RTC_DATA_ATTR static float cachedLon = 0.0f;
RTC_DATA_ATTR static char cachedLocation[CFG_WEATHER_LOC_MAX] = "";

// ----------------------- Weather (Open-Meteo) -----------------------

static bool resolveLocation(float& lat, float& lon) {
  if (weather_location[0] == '\0') return false;

  if (sscanf(weather_location, "%f , %f", &lat, &lon) == 2) return true;

  if (strcmp(cachedLocation, weather_location) == 0) {
    lat = cachedLat;
    lon = cachedLon;
    return true;
  }

  // "City" or "City, CC" where CC is a 2-letter country code.
  char city[CFG_WEATHER_LOC_MAX];
  char country[3] = "";
  snprintf(city, sizeof(city), "%s", weather_location);
  char* comma = strchr(city, ',');
  if (comma) {
    *comma = '\0';
    const char* cc = comma + 1;
    while (*cc == ' ') cc++;
    if (strlen(cc) == 2) {
      country[0] = (char)toupper((unsigned char)cc[0]);
      country[1] = (char)toupper((unsigned char)cc[1]);
      country[2] = '\0';
    }
  }
  for (int i = (int)strlen(city) - 1; i >= 0 && city[i] == ' '; i--) city[i] = '\0';

  char encoded[CFG_WEATHER_LOC_MAX * 3];
  urlEncode(city, encoded, sizeof(encoded));

  char url[320];
  snprintf(url, sizeof(url),
           "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=1&format=json%s%s",
           encoded, country[0] ? "&countryCode=" : "", country);

  JsonDocument doc;
  if (!httpGetJson(url, doc)) return false;

  JsonObject r = doc["results"][0];
  if (r.isNull()) {
    DBGF("[Weather] Location not found: %s", weather_location);
    return false;
  }
  lat = r["latitude"] | 0.0f;
  lon = r["longitude"] | 0.0f;

  cachedLat = lat;
  cachedLon = lon;
  snprintf(cachedLocation, sizeof(cachedLocation), "%s", weather_location);
  DBGF("[Weather] %s -> %.4f,%.4f", weather_location, lat, lon);
  return true;
}

bool fetchWeatherDirect(LayoutItem* item) {
  weather.count = 0;

  float lat, lon;
  if (!resolveLocation(lat, lon)) return false;

  int days = MAXWEATHERDAYS;
  // As many days as the widget shows (same rule as the drawing, src/layout/LayoutEngine.h).
  if (item && item->ColWidth > 0) days = constrain(layouts::weatherDaysFor(item->Width, item->ColWidth), 1, MAXWEATHERDAYS);

  char url[320];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min"
           "&forecast_days=%d&timezone=auto",
           lat, lon, days);

  JsonDocument doc;
  if (!httpGetJson(url, doc)) return false;

  JsonArray codes = doc["daily"]["weather_code"];
  JsonArray tmax = doc["daily"]["temperature_2m_max"];
  JsonArray tmin = doc["daily"]["temperature_2m_min"];
  if (codes.isNull()) return false;

  for (size_t i = 0; i < codes.size() && weather.count < days; i++) {
    WeatherDay& wd = weather.day[weather.count++];
    wd.sunrise[0] = '\0';
    wd.sunset[0] = '\0';
    wd.weather_code = codes[i] | 0;
    wd.temp_max = tmax[i] | 0.0f;
    wd.temp_min = tmin[i] | 0.0f;
    wd.icon = weatherCodeToMDI(wd.weather_code);
  }
  DBGF("[Weather] %d days", weather.count);
  return weather.count > 0;
}

// Place search for the setup app (stage 5): Open-Meteo's geocoding, up to `max` places, with names
// in the app's language ("Berlin, Deutschland").
int searchPlaces(const char* query, PlaceResult* out, int max) {
  char encoded[CFG_WEATHER_LOC_MAX * 3];
  urlEncode(query, encoded, sizeof(encoded));
  char url[320];
  snprintf(url, sizeof(url),
           "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=%d&format=json&language=%s", encoded,
           max, i18n::langCode(ui_lang));
  JsonDocument doc;
  if (!httpGetJson(url, doc)) return -1;
  int n = 0;
  for (JsonObjectConst r : doc["results"].as<JsonArrayConst>()) {
    if (n >= max) break;
    const char* name = r["name"] | "";
    const char* cc = r["country_code"] | "";
    if (!name[0]) continue;
    snprintf(out[n].name, sizeof(out[n].name), cc[0] ? "%s, %s" : "%s", name, cc);
    const char* admin = r["admin1"] | "";
    const char* country = r["country"] | "";
    snprintf(out[n].detail, sizeof(out[n].detail), "%s%s%s%s%s", name, admin[0] ? ", " : "", admin,
             country[0] ? ", " : "", country);
    n++;
  }
  return n;
}
