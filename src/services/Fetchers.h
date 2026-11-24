#pragma once
// The data services (stage 7b, own code): each fetch fills the globals the widgets draw from
// (src/widgets/Widgets.h). All over verified HTTPS; the answers are read by ServiceParsers.h and
// IcsParser.h. A failed fetch keeps the data on screen.
#include "../widgets/Widgets.h"
#include "SchoolHolidays.h"

// Weather of `weather_location` ("City", "City, CC" or "lat, lon"), as many days as `item` shows.
bool fetchWeatherDirect(LayoutItem* item);
// Quotes of `stock_symbols` ("SYMBOL" or "SYMBOL:Label", comma-separated). True if any was read;
// `failed` gets the number of symbols without a quote.
bool fetchStocksDirect(int* failed = nullptr);
// Parcels of the PKGE account `pkge_api_key`: those on their way and those delivered in the last day.
bool fetchTrackingDirect();
// Removes parcels delivered more than a day ago (no download); true if any was removed.
bool trackingDropExpired();
// Live status of `liveFlight` (AeroDataBox, `aerodatabox_key`); tz gives the local departure date.
bool fetchFlightStatus(const char* tz);
// MakerWorld counters of `makerworld_uid`, stored with saveMakerWorld().
bool fetchMakerWorldDirect();

// The calendar links `ics_urls` (merged): agenda, month marks, next flight, calendar birthdays.
// True if any link was read; `failed` gets the number that failed. tz: the dashboard's zone.
bool fetchCalendarIcs(const char* tz, int* failed = nullptr);
// The birthday link `bday_url` into /bday_feed.json (stage 4).
bool fetchBirthdayFeed(const char* tz);
// The school holidays of a calendar link (stage 7).
bool fetchSchoolHolidays(const char* url, const char* tz, school::SchoolList& out);

// Setup app checks (stage 5): a calendar link's number of events (-1 and a reason in err); places
// for the weather search (-1 if the search didn't answer).
int checkIcsLink(const char* url, char* err, size_t cap);
int searchPlaces(const char* query, PlaceResult* out, int max);
