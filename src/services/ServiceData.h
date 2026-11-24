#pragma once
// The data the services fetch and the widgets draw (stage 7b, own code). Plain C++ (no Arduino), so
// the parsers that fill it are tested on the PC (tests/host/test_service_parsers.cpp). The member
// order of the structs kept in RTC memory (CalEvents, TrackingItem, LiveFlight) is the one of the
// earlier builds.
#include <cstdint>
#include <ctime>

constexpr int MAXWEATHERDAYS = 7;
constexpr int MAX_STOCKS = 5;
constexpr int MAX_TRACKING = 5;
constexpr unsigned DATA_TITLE_MAX = 128;  // = CFG_CAL_TITLE_MAX (checked in Widgets.h)

struct StockItem {
  char name[52];  // the own label, or the symbol without a leading ^
  float price;
  float dayLow;
  float dayHigh;
  float prevClose;
  float fiftyTwoWeekHigh;
  float fiftyTwoWeekLow;
};

struct TrackingItem {
  char trackNumber[40];
  int status;            // PKGE status: 3 in transit, 4 at a pickup point, 5 delivered, 6/7 problem, 8 out for delivery
  time_t lastDate;       // the latest checkpoint
  time_t estDelivery;    // estimated delivery, 0 = unknown
};

// The next flight from the calendar and its live status (AeroDataBox).
struct LiveFlight {
  char number[12];       // "TS399", from the calendar title
  char title[DATA_TITLE_MAX];
  time_t calStart;       // departure in the calendar (identifies the flight)
  time_t calEnd;         // end of the calendar entry (expected landing without live data)
  time_t lastFetch;
  uint8_t requests;      // requests made for this flight (capped)
  bool valid;            // live data received
  char status[20];       // Expected, CheckIn, Boarding, Departed, Delayed, Arrived, Canceled, Diverted, ...
  time_t depScheduled, depActual;  // UTC; "actual" = revised or runway time when known, else 0
  time_t arrScheduled, arrActual;
  char depGate[8];
  char depTerminal[8];
};

struct CalEventItem {
  char title[DATA_TITLE_MAX];
  time_t start;
  time_t end;
  bool allDay;
};

struct CalEvents {
  int daysWithEvents[31];
  int daysCount;
  CalEventItem next24hEvents[10];  // today and tomorrow, by start
  int eventCount;
  CalEventItem nextFlight;  // the earliest flight not over yet within 7 days; start == 0: none
};

struct WeatherDay {
  float temp_max;
  float temp_min;
  int weather_code;  // WMO code (Open-Meteo)
  uint32_t icon;     // Material Design Icons code point (MDI_80_Sparse)
};

struct WeatherData {
  WeatherDay day[MAXWEATHERDAYS];
  int count;
};

struct MakerWorld {
  int likes;
  int downloads;
  int prints;
  int collections;
  int boosts;
  time_t lastUpdated;
  bool available;
};

// A place from the weather search (setup app).
struct PlaceResult {
  char name[65];    // "Berlin, DE": what goes into the weather setting
  char detail[96];  // "Berlin, Brandenburg, Deutschland"
};
