// Renders the dashboard on the PC into an image, with the firmware's own drawing code (src/widgets,
// src/ui) drawing on an in-memory canvas (src/ui/Canvas.h). Sample data, fixed time.
//
//   tools/render/render.sh            -> docs/images/layouts/*.png
//
// Used to check layouts before the device (stage 3) and, later, for the phone preview (stage 5).
#include <Adafruit_GFX.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

#include "app.h"
#include "src/widgets/Widgets.h"
#include "src/widgets/MonthView.h"
#include "src/core/AppState.h"
#include "src/core/Problems.h"
#include "src/services/Bambu.h"
#include "src/ui/Canvas.h"
#include "src/widgets/Clock.h"
#include "src/widgets/Dashboard.h"
#include "src/widgets/Registry.h"
#include "src/widgets/StatusBar.h"

// ---- What the drawing code expects from the rest of the firmware ----
HardwareSerialStub Serial;
char wifi_ssid[CFG_WIFI_SSID_MAX] = "Home";
char wifi_pass[CFG_WIFI_PASS_MAX] = "";
char mqtt_pass[CFG_MQTT_PASS_MAX] = "";
char mqtt_sn[CFG_MQTT_SN_MAX] = "";
char mqtt_ip[CFG_MQTT_IP_MAX] = "192.168.1.50";
uint16_t mqtt_port = 8883;
char device_timezone[CFG_TIMEZONE_MAX] = "CET-1CEST,M3.5.0,M10.5.0/3";
char weather_location[CFG_WEATHER_LOC_MAX] = "Berlin, DE";
char stock_symbols[CFG_STOCKS_MAX] = "AAPL,NVDA,^IXIC";
char ics_urls[ICS_MAX_FEEDS][CFG_ICS_URL_MAX] = {"https://example.org/a.ics", "", ""};
char makerworld_uid[CFG_MW_UID_MAX] = "123";
char printer_name[CFG_PRINTER_NAME_MAX] = "X1C";
char pkge_api_key[CFG_PKGE_KEY_MAX] = "key";
char aerodatabox_key[CFG_ADB_KEY_MAX] = "";
char bday_url[CFG_ICS_URL_MAX] = "";
uint8_t user_content = USER_HAS_TASKS | USER_HAS_NOTE | USER_HAS_BIRTHDAYS;
extern int renderSample;  // tools/render/UserStoreSample.cpp
char layout_template[CFG_LAYOUT_MAX] = "classic";
char layout_boxes[CFG_LAYOUT_BOXES_MAX] = "";
char device_name[CFG_DEVICE_NAME_MAX] = "";
uint16_t widget_show_mask = 0xFFFF;
bool clock_24h = true;
ServiceState serviceStates[SERVICE_COUNT];
PinConfig activePins = makePinPreset(PinPreset::Esp32C6Default);
uint16_t bootCount = 0;
bool isPrinting = false, previousIsPrinting = false, printViewOnScreen = false, bambuAuthRejected = false;
PrinterStatus printer;
uint32_t otaAvailBuild = 0;
char otaAvailVersion[16] = "";
bool showPrintView() { return false; }

float readBattery() { return 3.95f; }
void logPrint(const char* s) { fputs(s, stderr); }
void logPrint(const String& s) { fputs(s.c_str(), stderr); }
void logPrintln(const char* s) { fprintf(stderr, "%s\n", s); }
void logPrintln(const String& s) { fprintf(stderr, "%s\n", s.c_str()); }
void logPrintf(const char* fmt, ...) {
  va_list a;
  va_start(a, fmt);
  vfprintf(stderr, fmt, a);
  va_end(a);
}

// Fixed time for repeatable images: Tue 2026-11-17 09:41:20 in Germany (08:41:20 UTC).
static time_t RENDER_TIME = 1794904880;
extern "C" time_t __wrap_time(time_t* t) {
  if (t) *t = RENDER_TIME;
  return RENDER_TIME;
}


static void writePbm(const GFXcanvas1& c, const char* path) {
  FILE* f = fopen(path, "wb");
  if (!f) {
    perror(path);
    exit(1);
  }
  fprintf(f, "P4\n%d %d\n", c.width(), c.height());
  const int rowBytes = (c.width() + 7) / 8;
  for (int y = 0; y < c.height(); y++) {
    for (int b = 0; b < rowBytes; b++) {
      uint8_t out = 0;
      for (int bit = 0; bit < 8; bit++) {
        int x = b * 8 + bit;
        bool black = x < c.width() && !c.getPixel(x, y);  // canvas bit 1 = white
        if (black) out |= 0x80 >> bit;
      }
      fputc(out, f);
    }
  }
  fclose(f);
}

// ---- Sample data ----
static void sampleData() {
  const char* icons[] = {"0", "2", "61", "3"};
  weather.count = 4;
  for (int i = 0; i < 4; i++) {
    WeatherDay& d = weather.day[i];
    d.temp_min = 2 + i;
    d.temp_max = 9 + i;
    d.weather_code = atoi(icons[i]);
    d.icon = weatherCodeToMDI(d.weather_code);
    snprintf(d.sunrise, sizeof(d.sunrise), "07:3%d", i);
    snprintf(d.sunset, sizeof(d.sunset), "16:1%d", i);
  }
  struct Ev { const char* title; int startH; int durH; bool allDay; };
  const Ev evs[] = {{"Dentist", 2, 1, false}, {"Team meeting", 5, 1, false},
                    {"Birthday Anna", 15, 24, true}, {"LH 400 FRA->JFK", 26, 8, false}};
  cal.eventCount = 0;
  for (const Ev& e : evs) {
    CalEventItem& it = cal.next24hEvents[cal.eventCount++];
    snprintf(it.title, sizeof(it.title), "%s", e.title);
    it.start = RENDER_TIME + e.startH * 3600;
    it.end = it.start + e.durH * 3600;
    it.allDay = e.allDay;
  }
  int days[] = {3, 9, 17, 18, 24};
  cal.daysCount = 0;
  for (int d : days) cal.daysWithEvents[cal.daysCount++] = d;
  cal.nextFlight = cal.next24hEvents[3];

  struct St { const char* n; float p, prev; };
  const St st[] = {{"AAPL", 231.5f, 229.0f}, {"NVDA", 141.2f, 145.9f}, {"^IXIC", 18920.0f, 18800.0f}};
  stockCount = 0;
  for (const St& s : st) {
    StockItem& it = stocks[stockCount++];
    snprintf(it.name, sizeof(it.name), "%s", s.n);
    it.price = s.p;
    it.prevClose = s.prev;
    it.dayLow = s.p * 0.98f;
    it.dayHigh = s.p * 1.01f;
    it.fiftyTwoWeekLow = s.p * 0.7f;
    it.fiftyTwoWeekHigh = s.p * 1.2f;
  }
  trackingCount = 2;
  snprintf(tracking[0].trackNumber, sizeof(tracking[0].trackNumber), "%s", "JJD000390007712345678");
  tracking[0].status = 3;
  tracking[0].lastDate = RENDER_TIME - 7200;
  tracking[0].estDelivery = RENDER_TIME + 86400;
  snprintf(tracking[1].trackNumber, sizeof(tracking[1].trackNumber), "%s", "1Z999AA10123456784");
  tracking[1].status = 5;
  tracking[1].lastDate = RENDER_TIME - 3600;
  makerworld = MakerWorld{1240, 5230, 870, 310, 42, RENDER_TIME, true};
  saveMakerWorld();  // the widget reloads its numbers from NVS
}

// render <out.pbm> [template] [hide]   hide: comma-separated widget keys switched off (Registry.cpp)
int main(int argc, char** argv) {
  const char* out = argc > 1 ? argv[1] : "classic.pbm";
  if (argc > 2 && std::strncmp(argv[2], "v1;", 3) == 0) {  // a custom layout (box string)
    snprintf(layout_template, sizeof(layout_template), "custom");
    snprintf(layout_boxes, sizeof(layout_boxes), "%s", argv[2]);
  } else if (argc > 2) {
    snprintf(layout_template, sizeof(layout_template), "%s", argv[2]);
  }
  if (argc > 4 && std::string(argv[4]) == "stale") {  // weather and calendar not updated for a day
    serviceStates[(int)Service::Weather].lastOk = RENDER_TIME - 86400;
    serviceStates[(int)Service::Calendar].lastOk = RENDER_TIME - 86400;
  }
  if (argc > 4 && std::string(argv[4]).rfind("12h", 0) == 0) clock_24h = false;
  if (argc > 4 && std::string(argv[4]) == "empty") renderSample = 1;
  if (argc > 4 && std::string(argv[4]) == "problems") {  // weather place missing, calendar failing
    weather_location[0] = '\0';
    problems = PROBLEM_CALENDAR | PROBLEM_WEATHER;
  }
  if (argc > 4 && std::string(argv[4]) == "long") renderSample = 2;
  if (argc > 4 && std::string(argv[4]) == "12h1258") RENDER_TIME += 3 * 3600 + 17 * 60;  // 12:58, widest 12 h
  if (argc > 3) {
    std::string hide = argv[3];
    for (int w = 0; w < layouts::WIDGET_COUNT; w++) {
      std::string key = widgetInfo(static_cast<layouts::WidgetId>(w)).key;
      if (("," + hide + ",").find("," + key + ",") != std::string::npos)
        widget_show_mask &= ~layouts::widgetBit(static_cast<layouts::WidgetId>(w));
    }
  }

  GFXcanvas1 screen(800, 480);
  setCanvas(&screen);
  sampleData();

  // The full refresh of the wake cycle (src/core/App.cpp), everything due.
  loadDefaultLayout();
  canvas().fillScreen(GxEPD_WHITE);
  FullScreenParts parts;
  parts.printer = parts.weather = parts.agenda = parts.stocks = parts.month = parts.tracking = true;
  drawFullScreen(parts);

  writePbm(screen, out);
  fprintf(stderr, "wrote %s\n", out);
  return 0;
}
