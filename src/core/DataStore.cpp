// The data store (stage 8): weather, stocks and the printer state in RTC memory, kept between the
// wakes (lost on a reset: then everything on the screen is due once). Before stage 8 they lived in
// normal RAM and were downloaded again whenever a screen showed them. A widget with nothing yet
// shows its "no data" text (weather.count == 0, stockCount == 0, an empty printer state).
#include "DataStore.h"

#include <esp_attr.h>

#include "../services/PrinterReport.h"
#include "../services/ServiceData.h"

RTC_DATA_ATTR WeatherData weather;
RTC_DATA_ATTR StockItem stocks[MAX_STOCKS];
RTC_DATA_ATTR int stockCount = 0;
RTC_DATA_ATTR bambu::PrinterStatus printer;
