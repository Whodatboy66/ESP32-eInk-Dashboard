// Renders the dashboard on the PC into an image, with the firmware's own drawing code (src/widgets,
// src/ui) drawing on an in-memory canvas (src/ui/Canvas.h). Sample data, fixed time.
//
//   tools/render/render.sh            -> docs/images/layouts/*.png (German)
//   tools/render/render.sh out en     -> out/*.png in English
//   tools/render/render.sh out de screens -> also the setup, update and charge screens
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
#include "src/services/ServiceParsers.h"
#include "src/personal/PersonalText.h"
#include "src/core/PowerPlan.h"
#include "src/core/Problems.h"
#include "src/services/Bambu.h"
#include "src/ui/Canvas.h"
#include "src/widgets/Clock.h"
#include "src/widgets/Dashboard.h"
#include "src/widgets/Registry.h"
#include "src/widgets/StatusBar.h"
#include "src/update/UpdateScreens.h"
#include "src/web/SetupScreen.h"

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
char holiday_state[4] = "BY";
bool holidays_on = true, school_on = true;
char school_url[CFG_ICS_URL_MAX] = "";
uint8_t ics_month_mask = 1;
uint16_t data_interval[4] = {60, 30, 30, 120};
char ics_names[ICS_MAX_FEEDS][CFG_ICS_NAME_MAX] = {"Familie", "Arbeit", ""};
uint8_t ics_every[ICS_MAX_FEEDS] = {0, 1, 0};
bool cal_birthdays = true;
school::SchoolList schoolHolidays;
int8_t lastRssi = -60;  // three Wi-Fi bars
uint8_t user_content = USER_HAS_TASKS | USER_HAS_NOTE | USER_HAS_BIRTHDAYS;
extern int renderSample;  // tools/render/UserStoreSample.cpp
char layout_template[CFG_LAYOUT_MAX] = "classic";
char layout_boxes[CFG_LAYOUT_BOXES_MAX] = "";
char device_name[CFG_DEVICE_NAME_MAX] = "";
char powerNote[48] = "";
uint32_t powerNoteShown = 0;
uint16_t widget_show_mask = 0xFFFF;
bool clock_24h = true;
i18n::Lang ui_lang = i18n::Lang::De;
ServiceState serviceStates[SERVICE_COUNT];
PinConfig activePins = makePinPreset(PinPreset::Esp32C6Default);
uint16_t bootCount = 0;
bool isPrinting = false, previousIsPrinting = false, printViewOnScreen = false, bambuAuthRejected = false;
uint32_t otaAvailBuild = 0;
char otaAvailVersion[16] = "";
bool showPrintView() { return false; }
// The e-paper display of the screens that do their own refresh (setup, update): an 800 x 480
// bitmap here; they draw on canvas(), which is the image being written.
DashboardDisplay& getDisplay() {
  static DashboardDisplay d(DashboardPanel(0, 0, 0, 0));
  return d;
}

float readBattery() { return 3.95f; }
void batteryNoteQuiet(float) {}
float batteryForDisplay() { return 3.95f; }
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
    d.icon = svc::weatherIcon(d.weather_code);
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
  const St st[] = {{"MSCI World", 98.12f, 97.6f}, {"Nasdaq CC", 14.85f, 14.95f}, {"Gold (phys.)", 52.31f, 51.9f}};
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
  // Stage 7: Bavaria's Christmas school holidays 2026/27 (as the feed has them).
  schoolHolidays = school::SchoolList();
  schoolHolidays.count = 1;
  schoolHolidays.r[0].fromDay = personal::dayNumber(personal::Date{2026, 12, 24});
  schoolHolidays.r[0].toDay = personal::dayNumber(personal::Date{2027, 1, 8});
  snprintf(schoolHolidays.r[0].name, sizeof(schoolHolidays.r[0].name), "Weihnachtsferien");
  saveMakerWorld();  // the widget reloads its numbers from NVS
}

// The screens outside the dashboard, with sample values (the longest texts where it matters).
static bool drawScreen(const std::string& name) {
  using i18n::S;
  ota::Manifest m;
  snprintf(m.version, sizeof(m.version), ".78");
  m.noteCount = 3;
  snprintf(m.notes[0], ota::NOTE_LEN, "German and English for the screen and the setup app");
  snprintf(m.notes[1], ota::NOTE_LEN, "Status bar: date, weather weekdays");
  snprintf(m.notes[2], ota::NOTE_LEN, "Agenda for today and tomorrow");
  char detail[112];
  if (name == "setup-ap") drawSetupScreenAp("Dashboard-Setup", "k7m2p9x4");
  else if (name == "setup-home") drawSetupScreenHome("kuechen-display.local", "192.168.178.123", "482913");
  else if (name == "update-info")
    ota::showInfo(ota::DeviceInfo{".77", "esp32c6_firebeetle", "2026-11-17", 82, true, "Home Network 5G", false,
                                  tr(S::UpdStable)});
  else if (name == "update-available") ota::showAvailable(m, ".77", true, 30000);
  else if (name == "update-progress") ota::showProgress(ota::InstallStage::Downloading, 2, ".78");
  else if (name == "update-failed") {
    snprintf(detail, sizeof(detail), tr(S::OtaNeedsNewer), ".78", 75u);
    ota::showMessage(tr(S::OtaCheckFailed), detail);
  } else if (name == "update-battery") {
    snprintf(detail, sizeof(detail), tr(S::OtaBatteryDetail), 18, 30);
    ota::showMessage(tr(S::OtaBatteryLow), detail);
  } else if (name == "update-done") ota::showUpdated(".78", m.notes, m.noteCount);
  else if (name == "charge") drawChargeScreen(3.42f);
  else return false;
  return true;
}

// render <out.pbm> [template] [hide] [variant] [lang]
//   template: a template key, a box string (v1;...) or screen:<name> (drawScreen)
//   hide: comma-separated widget keys switched off (Registry.cpp); lang: de (default) or en
int main(int argc, char** argv) {
  const char* out = argc > 1 ? argv[1] : "classic.pbm";
  if (argc > 5) ui_lang = i18n::parseLang(argv[5]);
  if (argc > 2 && std::strncmp(argv[2], "screen:", 7) == 0) {
    GFXcanvas1 screen(800, 480);
    setCanvas(&screen);
    screen.fillScreen(GxEPD_WHITE);
    if (!drawScreen(argv[2] + 7)) {
      fprintf(stderr, "unknown screen %s\n", argv[2] + 7);
      return 1;
    }
    writePbm(screen, out);
    return 0;
  }
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
  if (argc > 4 && std::string(argv[4]) == "jan27") RENDER_TIME = 1799052080;  // Mon 2027-01-04 09:41, stage 7
  if (argc > 4 && std::string(argv[4]).rfind("holiday", 0) == 0) RENDER_TIME = 1799224880;  // Wed 2027-01-06 09:41
  if (argc > 4 && std::string(argv[4]) == "night") {  // the status bar's note at night
    power::Plan p;
    p.mode = power::Mode::Night;
    power::note(p, 1, power::DEFAULT_QUIET_TO, powerNote, sizeof(powerNote), ui_lang);
  }
  if (argc > 4 && (std::string(argv[4]) == "saver" || std::string(argv[4]) == "holiday-saver")) {
    power::Plan p;
    p.saverOn = true;
    p.stepMin = power::SAVER_INTERVAL_MIN;
    power::note(p, 1, power::DEFAULT_QUIET_TO, powerNote, sizeof(powerNote), ui_lang);
  }
  if (argc > 4 && std::string(argv[4]) == "problems") {  // weather place missing, calendar failing
    weather_location[0] = '\0';
    problems = PROBLEM_CALENDAR | PROBLEM_WEATHER;
    icsFailedMask = 0x02;  // calendar link 2
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
  if (argc > 4 && std::string(argv[4]) == "holiday") {  // review case: a trip that began before today
    for (int i = cal.eventCount; i > 0; i--) cal.next24hEvents[i] = cal.next24hEvents[i - 1];
    CalEventItem& trip = cal.next24hEvents[0];
    snprintf(trip.title, sizeof(trip.title), "%s", "Skiurlaub");
    trip.start = RENDER_TIME - 2 * 86400;
    trip.end = RENDER_TIME + 3 * 86400;
    trip.allDay = true;
    cal.eventCount++;
  }

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
