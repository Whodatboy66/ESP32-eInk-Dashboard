#pragma once
// The widgets' shared declarations (stage 7b, own code): one drawing place per widget on the screen
// (LayoutItem, filled from the layout by Registry.cpp), the data globals the services fill
// (src/services/ServiceData.h), the drawing functions.
#include "../../app.h"
#include <time.h>

#include "../services/ServiceData.h"

static_assert(DATA_TITLE_MAX == CFG_CAL_TITLE_MAX, "calendar titles: one size everywhere");

constexpr int MAXLAYOUTS = 15;

// The widgets' layout ids (Registry.cpp: WidgetInfo::layoutId), one bit each.
namespace box {
constexpr uint16_t CLOCK = 1, AGENDA = 2, STOCKS = 4, WEATHER = 8, TRACKING = 16, PRINTER = 64, STATUS = 128,
                   MONTH = 256, TASKS = 512, NOTE = 1024, BIRTHDAYS = 2048;
}

// Where a widget draws on the screen.
struct LayoutItem {
  int ID;          // the widget's layout id (Registry.h): 1 clock, 2 agenda, 256 month, ...
  int PosY;
  int PosX;
  int Width;
  int Height;
  int RowHeight;   // line height of list widgets
  int ColWidth;    // column width (weather: one day)
  int Refresh;     // minutes between downloads of its data
  char Option[64]; // the clock: a time zone (empty = the device's); the printer: its name
  bool Active;
  bool Grid;       // a box of the grid layouts (not Classic's pixel places): list widgets get a heading
};

// A list widget's heading (Heading_22) with a line under it, in the grid layouts and boxes of at
// least 100 px (layouts::wantsHeading). Returns the area below it for the content (the item itself
// if no heading is drawn).
LayoutItem headed(const LayoutItem& item, const char* title);

// ---- The data (filled by src/services/, drawn by the widgets) ----
extern StockItem stocks[MAX_STOCKS];
extern int stockCount;
extern CalEvents cal;                       // RTC memory
extern TrackingItem tracking[MAX_TRACKING];  // RTC memory
extern int trackingCount;                    // RTC memory
extern WeatherData weather;
extern LiveFlight liveFlight;                // RTC memory
extern MakerWorld makerworld;                // kept in NVS (saveMakerWorld / loadMakerWorld)
extern uint16_t bootCount;

// ---- The layout table ----
extern LayoutItem layout[MAXLAYOUTS];
extern int layoutCount;
LayoutItem* getLayout(uint16_t id);  // nullptr if the widget isn't on the screen
int findLayoutIndexByID(int id);
void loadDefaultLayout();

// ---- Widgets ----
void gCalWidget(LayoutItem*);  // "Heute und morgen"
void stockWidget(LayoutItem*);
void weatherWidget(LayoutItem*);
void trackingWidget(LayoutItem*);
void makerWorldWidget(LayoutItem*);
void saveMakerWorld();
void loadMakerWorld();

// "9:00-10:30" (or "9:00 AM-10:30 AM"), the agenda's time column.
void formatTimeRange(time_t start, time_t end, char* buf, size_t bufLen);

// True for calendar titles that look like a flight: "TS399 PUJ->YUL", "AC 871 YUL to YYZ",
// "Flight to Toronto (AC 871)".
bool isFlightTitle(const char* title);
// The flight number ("TS399", spaces removed) of a title; false if there is none.
bool extractFlightNumber(const char* title, char* out, size_t outSize);
