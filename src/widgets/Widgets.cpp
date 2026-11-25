// The data widgets (stage 7b, own code): "Heute und morgen", stocks, weather, parcels, MakerWorld,
// plus the layout table and the data globals (Widgets.h). Places and sizes as in the earlier builds.
#include "Widgets.h"

#include <Preferences.h>

#include "../core/CivilDate.h"
#include "../core/Holidays.h"
#include "../i18n/Dates.h"
#include "../personal/PersonalText.h"
#include "../services/Bambu.h"
#include "AgendaDays.h"
#include "Dashboard.h"
#include "FlightText.h"
#include "MonthView.h"
#include "Personal.h"
#include "Registry.h"

// ---- Data ----
LayoutItem layout[MAXLAYOUTS];
int layoutCount = 0;
StockItem stocks[MAX_STOCKS];
int stockCount = 0;
WeatherData weather;
MakerWorld makerworld;
// Kept in deep sleep: the agenda and the flight countdown redraw without a download; delivered
// parcels expire between downloads.
RTC_DATA_ATTR LiveFlight liveFlight;
RTC_DATA_ATTR CalEvents cal;
RTC_DATA_ATTR TrackingItem tracking[MAX_TRACKING];
RTC_DATA_ATTR int trackingCount = 0;

namespace {

constexpr int16_t ICON_TEXT_GAP = 24;  // a plane icon, then the text

int16_t drawText(int16_t x, int16_t y, const char* text) {
  drawSparseString(&epaperFont, x, y, text, GxEPD_BLACK);
  return x + getSparseStringWidth(&epaperFont, text);
}

// "Keine Daten" in two lines in the middle of the box.
void drawUnavailable(const LayoutItem* item) {
  int cx = item->PosX + item->Width / 2, cy = item->PosY + item->Height / 2;
  drawSparseStringCentered(&epaperFont, cx, cy - 8, tr(i18n::S::Unavailable1), GxEPD_BLACK);
  drawSparseStringCentered(&epaperFont, cx, cy + 10, tr(i18n::S::Unavailable2), GxEPD_BLACK);
}

// A small plane (about 18 x 14 px) pointing right, centred on a text line with baseline y.
void drawPlaneIcon(int16_t x, int16_t y) {
  int16_t cy = y - 6;
  canvas().fillRect(x + 1, cy - 1, 15, 3, GxEPD_BLACK);                           // body
  canvas().fillTriangle(x + 16, cy - 1, x + 16, cy + 1, x + 18, cy, GxEPD_BLACK);  // nose
  canvas().fillTriangle(x + 6, cy, x + 11, cy, x + 5, cy - 7, GxEPD_BLACK);        // wings
  canvas().fillTriangle(x + 6, cy, x + 11, cy, x + 5, cy + 7, GxEPD_BLACK);
  canvas().fillTriangle(x + 1, cy, x + 4, cy, x + 1, cy - 4, GxEPD_BLACK);         // tail
}

bool isLiveFlight(const CalEventItem& ev) {
  return liveFlight.valid && ev.start == liveFlight.calStart && isFlightTitle(ev.title);
}

// One agenda entry at baseline y: time (or "Ganztägig") and title; a flight with its plane, the
// countdown to the real departure, and a second line with the live status. Returns the next y.
int drawEntry(const LayoutItem* item, const CalEventItem& ev, int y, time_t now) {
  const bool live = isLiveFlight(ev);
  char when[32], line[CFG_CAL_TITLE_MAX + 64];
  if (live && liveFlight.depScheduled && liveFlight.arrScheduled)  // the real times, delays included
    formatTimeRange(liveFlight.depActual ? liveFlight.depActual : liveFlight.depScheduled,
                    liveFlight.arrActual ? liveFlight.arrActual : liveFlight.arrScheduled, when, sizeof(when));
  else if (ev.allDay)
    snprintf(when, sizeof(when), "%s", tr(i18n::S::AllDay));
  else
    formatTimeRange(ev.start, ev.end, when, sizeof(when));
  int n = snprintf(line, sizeof(line), "%s %s", when, ev.title);
  int16_t x = item->PosX;
  const bool flight = isFlightTitle(ev.title);
  if (flight) {
    drawPlaneIcon(x, y);
    x += ICON_TEXT_GAP;
    time_t departs = ev.start;
    if (liveFlight.valid && ev.start == liveFlight.calStart)
      departs = liveFlight.depActual ? liveFlight.depActual : liveFlight.depScheduled;
    if (departs > now && n > 0 && n < (int)sizeof(line)) {
      char countdown[24];
      i18n::formatCountdown((long)(departs - now), ui_lang, countdown, sizeof(countdown));
      snprintf(line + n, sizeof(line) - n, "  %s", countdown);
    }
  }
  drawText(x, y, line);
  if (flight && liveFlight.valid && ev.start == liveFlight.calStart && y + item->RowHeight <= item->PosY + item->Height) {
    y += item->RowHeight;
    char status[96];
    flighttext::liveStatus(liveFlight, ui_lang, status, sizeof(status));
    drawText(x, y, status);
  }
  return y + item->RowHeight;
}

const char* parcelStatus(int status) {
  using i18n::S;
  switch (status) {
    case -1: case 0: case 1: return tr(S::TrackChecking);
    case 2: return tr(S::TrackNoInfo);
    case 3: return tr(S::TrackInTransit);
    case 4: return tr(S::TrackPickup);
    case 5: return tr(S::TrackDelivered);
    case 6: return tr(S::TrackNotDelivered);
    case 7: return tr(S::TrackError);
    case 8: return tr(S::TrackPreparing);
    case 9: return tr(S::TrackEnded);
    default: return tr(S::TrackUnknown);
  }
}

uint32_t parcelIcon(int status) {
  if (status == 3 || status == 8) return MDI_TRUCK;  // in transit, out for delivery
  if (status == 5) return MDI_PACKAGE_DELIVERED;
  if (status == 6 || status == 7) return MDI_ALERT;
  return MDI_PACKAGE;
}

Preferences makerWorldPrefs;

}  // namespace

// ---- Shared ----

LayoutItem headed(const LayoutItem& item, const char* title) {
  if (!layouts::wantsHeading(!item.Grid, item.Height)) return item;
  drawSparseString(&Heading_22, item.PosX, item.PosY + 20, title, GxEPD_BLACK);
  canvas().drawFastHLine(item.PosX, item.PosY + 26, item.Width - 8, GxEPD_BLACK);
  LayoutItem body = item;
  body.PosY += layouts::HEADING_H;
  body.Height -= layouts::HEADING_H;
  return body;
}

void loadDefaultLayout() { loadLayout(); }  // the configured template or boxes (Registry.cpp)

int findLayoutIndexByID(int id) {
  for (int i = 0; i < layoutCount; i++)
    if (layout[i].ID == id) return i;
  return -1;
}

LayoutItem* getLayout(uint16_t id) {
  int i = findLayoutIndexByID(id);
  return i >= 0 ? &layout[i] : nullptr;
}

void formatTimeRange(time_t start, time_t end, char* buf, size_t bufLen) {
  if (!buf || !bufLen) return;
  struct tm a, b;
  localtime_r(&start, &a);
  localtime_r(&end, &b);
  snprintf(buf, bufLen, "%02d:%02d-%02d:%02d", a.tm_hour, a.tm_min, b.tm_hour, b.tm_min);
}

bool isFlightTitle(const char* title) { return flighttext::isFlightTitle(title); }
bool extractFlightNumber(const char* title, char* out, size_t outSize) {
  return flighttext::extractFlightNumber(title, out, outSize);
}

// ---- "Heute und morgen" ----

void gCalWidget(LayoutItem* item) {
  if (!item) return;
  LayoutItem body = headed(*item, tr(i18n::S::HeadAgenda));
  item = &body;
  const time_t now = time(nullptr);
  int y = item->PosY + 16;  // the first baseline
  bool firstLine = true;

  // A flight later than tomorrow (not in the list) is pinned on top with a countdown.
  const CalEventItem& flight = cal.nextFlight;
  bool listed = false;
  for (int i = 0; i < cal.eventCount; i++) listed = listed || cal.next24hEvents[i].start == flight.start;
  if (flight.start > now && !listed) {
    char countdown[24], line[CFG_CAL_TITLE_MAX + 40];
    i18n::formatCountdown((long)(flight.start - now), ui_lang, countdown, sizeof(countdown));
    snprintf(line, sizeof(line), "%s  %s", flight.title, countdown);
    drawPlaneIcon(item->PosX, y);
    drawText(item->PosX + ICON_TEXT_GAP, y, line);
    y += item->RowHeight;
    firstLine = false;
  }

  // The days of the entries, plus today / tomorrow when they are public holidays (AgendaDays.h);
  // such a day starts with "★ Heilige Drei Könige". Day numbers, not "+24 h" (DST).
  auto dayKey = [](time_t at) {
    struct tm lt;
    localtime_r(&at, &lt);
    return personal::dayNumber(personal::Date{lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday});
  };
  const bool clockSet = now > 1700000000;  // before NTP: no holidays (no "Neujahr" in 1970)
  const holidays::State state = (holidays_on && clockSet) ? holidays::parseState(holiday_state) : holidays::State::None;
  auto holidayOf = [&](long key, holidays::Holiday* h) {
    int hy, hm, hd;
    civil::civilFromDays(key, hy, hm, hd);
    return holidays::holidayOn(hy, hm, hd, state, h);
  };
  const long today = clockSet ? dayKey(now) : 0;
  const int ne = cal.eventCount < 10 ? cal.eventCount : 10;
  long evDays[10];
  for (int i = 0; i < ne; i++) evDays[i] = dayKey(cal.next24hEvents[i].start);
  agenda::Day days[12];
  const int nd = agenda::plan(evDays, ne, today, today && holidayOf(today, nullptr), today && holidayOf(today + 1, nullptr), days, 12);
  const int bottom = item->PosY + item->Height;
  bool full = false;
  for (int d = 0; d < nd && !full; d++) {
    if (!firstLine) y += 5;  // a small gap between days
    firstLine = false;
    if (y > bottom) break;
    int dy, dm, dd;
    civil::civilFromDays(days[d].key, dy, dm, dd);
    struct tm t = {};
    t.tm_year = dy - 1900;
    t.tm_mon = dm - 1;
    t.tm_mday = dd;
    t.tm_wday = (civil::weekdayMonday0(days[d].key) + 1) % 7;  // tm: 0 = Sunday
    char header[40];
    i18n::formatDayHeader(t, ui_lang, header, sizeof(header));
    drawText(item->PosX, y, header);
    y += item->RowHeight + 5;
    holidays::Holiday h;
    if (days[d].holiday && y <= bottom && holidayOf(days[d].key, &h)) {
      drawHolidayStar(item->PosX, y - 10);
      drawText(item->PosX + 14, y, tr(h.name));
      y += item->RowHeight;
    }
    for (int i = 0; i < ne && !full; i++) {
      if (evDays[i] != days[d].key) continue;
      if (y > bottom) full = true;  // stop before spilling into the box below
      else y = drawEntry(item, cal.next24hEvents[i], y, now);
    }
  }
  drawStaleMark(item, Service::Calendar);
}

// ---- Stocks ----

void stockWidget(LayoutItem* item) {
  if (!item) return;
  LayoutItem body = headed(*item, tr(i18n::S::WStocks));
  item = &body;
  if (stockCount <= 0) {
    drawUnavailable(item);
    return;
  }
  // Columns as in the original 430 px place, squeezed proportionally in a narrower one.
  auto col = [item](int x) { return item->Width >= 430 ? x : x * item->Width / 430; };
  const int priceX = col(152);
  int y = item->PosY + item->RowHeight;
  char buf[32];
  for (int i = 0; i < stockCount; i++) {
    const StockItem& s = stocks[i];
    uint32_t arrow = s.price > s.prevClose ? MDI_ARROW_UP : s.price < s.prevClose ? MDI_ARROW_DOWN : MDI_EQUAL;
    drawSparseChar(&MDI_22_Sparse, item->PosX, y, arrow, GxEPD_BLACK);
    drawFittedString(item->PosX + 30, y, priceX - 30 - 6, s.name, GxEPD_BLACK);  // "..." before the price
    snprintf(buf, sizeof(buf), "%.2f", s.price);
    i18n::localizeNumber(buf, ui_lang);
    drawText(item->PosX + priceX, y, buf);
    snprintf(buf, sizeof(buf), "%7.2f / ", s.dayHigh);
    i18n::localizeNumber(buf, ui_lang);
    drawText(item->PosX + col(240), y, buf);
    snprintf(buf, sizeof(buf), "%7.2f", s.dayLow);
    i18n::localizeNumber(buf, ui_lang);
    drawText(item->PosX + col(330), y, buf);
    y += item->RowHeight;
  }
  drawStaleMark(item, Service::Stocks);
}

// ---- Weather ----

void weatherWidget(LayoutItem* item) {
  if (!item) return;
  if (weather.count <= 0) {
    drawUnavailable(item);
    return;
  }
  // Vertically centred (the original 105 px place had the icons 63 px down). With room (grid
  // boxes, 150 px) the weekday goes above each icon: label, icon and temperatures need about 120 px.
  const bool dayLabels = item->Height >= 125;
  const int y = dayLabels ? item->PosY + 85 + (item->Height - 119) / 2 : item->PosY + 63 + (item->Height - 105) / 2;
  const time_t now = time(nullptr);
  int days = layouts::weatherDaysFor(item->Width, item->ColWidth);  // one per ColWidth
  if (days > weather.count) days = weather.count;
  char buf[32];
  for (int i = 0; i < days; i++) {
    const WeatherDay& w = weather.day[i];
    const int x = item->PosX + i * item->ColWidth, cx = x + 40;
    if (dayLabels && now > 1700000000) {  // "Fr" / "Fri"; the first day is today
      struct tm d;
      localtime_r(&now, &d);
      d.tm_mday += i;
      d.tm_hour = 12;
      d.tm_isdst = -1;
      mktime(&d);
      drawSparseStringCentered(&epaperFont, cx, y - 73, i18n::weekdayShort(d.tm_wday, ui_lang), GxEPD_BLACK);
    }
    drawSparseChar(&MDI_80_Sparse, x, y, w.icon, GxEPD_BLACK);
    snprintf(buf, sizeof(buf), "%.0f° / %.0f°", w.temp_max, w.temp_min);
    drawSparseStringCentered(&epaperFont, cx, y + 30, buf, GxEPD_BLACK);
  }
  drawStaleMark(item, Service::Weather, true);
}

// ---- Parcels ----

void trackingWidget(LayoutItem* item) {
  if (!item) return;
  LayoutItem body = headed(*item, tr(i18n::S::HeadTracking));
  item = &body;
  int y = item->PosY + 18;
  if (trackingCount == 0) {
    drawText(item->PosX, y, tr(i18n::S::TrackNone));
    return;
  }
  for (int i = 0; i < trackingCount && y <= item->PosY + item->Height; i++) {
    const TrackingItem& t = tracking[i];
    drawSparseChar(&MDI_22_Sparse, item->PosX, y + 2, parcelIcon(t.status), GxEPD_BLACK);
    // Number (its last 8 characters are enough to recognize it), status and arrival, 10 px apart.
    size_t len = strlen(t.trackNumber);
    int16_t x = drawText(item->PosX + 28, y, len > 8 ? t.trackNumber + len - 8 : t.trackNumber) + 10;
    x = drawText(x, y, parcelStatus(t.status)) + 10;
    if (t.status != 5 && t.estDelivery > 0) {
      struct tm et;
      localtime_r(&t.estDelivery, &et);
      char date[16], eta[48];
      i18n::formatShortDate(et, ui_lang, date, sizeof(date));
      snprintf(eta, sizeof(eta), "%s %s", tr(i18n::S::TrackEta), date);
      drawText(x, y, eta);
    }
    y += item->RowHeight;
  }
  drawStaleMark(item, Service::Tracking);
}

// ---- MakerWorld ----

void makerWorldWidget(LayoutItem* item) {
  if (!item) return;
  loadMakerWorld();
  // No user id (the stored counters stay in NVS after it is removed: device test 2026-11-25), or no
  // counters yet: the printer's own view (temperatures, AMS, spools, state).
  if (!makerworld_uid[0] || !makerworld.available) {
    bambuWidget(item);
    return;
  }
  // Two columns of label and number; the number right-aligned at the end of its half.
  const struct {
    i18n::S label;
    long value;
  } stats[] = {{i18n::S::MwLikes, makerworld.likes},   {i18n::S::MwDownloads, makerworld.downloads},
               {i18n::S::MwPrints, makerworld.prints}, {i18n::S::MwCollections, makerworld.collections},
               {i18n::S::MwBoosts, makerworld.boosts}};
  const int half = 196;
  for (int i = 0; i < 5; i++) {
    int x = item->PosX + (i % 2) * (half + 4), y = item->PosY + 19 + (i / 2) * item->RowHeight;
    if (y > item->PosY + item->Height) break;
    char num[12];
    snprintf(num, sizeof(num), "%ld", stats[i].value);
    drawText(x, y, tr(stats[i].label));
    drawText(x + half - 8 - getSparseStringWidth(&epaperFont, num), y, num);
  }
}

// The counters in NVS ("ePaper"): shown between the hourly downloads.
void saveMakerWorld() {
  makerWorldPrefs.begin("ePaper", false);
  makerWorldPrefs.putInt("mw_likes", makerworld.likes);
  makerWorldPrefs.putInt("mw_downloads", makerworld.downloads);
  makerWorldPrefs.putInt("mw_prints", makerworld.prints);
  makerWorldPrefs.putInt("mw_collections", makerworld.collections);
  makerWorldPrefs.putInt("mw_boosts", makerworld.boosts);
  makerWorldPrefs.putBool("mw_available", makerworld.available);
  makerWorldPrefs.end();
}

void loadMakerWorld() {
  makerWorldPrefs.begin("ePaper", true);
  makerworld.likes = makerWorldPrefs.getInt("mw_likes", 0);
  makerworld.downloads = makerWorldPrefs.getInt("mw_downloads", 0);
  makerworld.prints = makerWorldPrefs.getInt("mw_prints", 0);
  makerworld.collections = makerWorldPrefs.getInt("mw_collections", 0);
  makerworld.boosts = makerWorldPrefs.getInt("mw_boosts", 0);
  makerworld.available = makerWorldPrefs.getBool("mw_available", false);
  makerWorldPrefs.end();
}
