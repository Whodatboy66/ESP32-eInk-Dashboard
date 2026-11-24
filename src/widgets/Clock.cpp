#include "Clock.h"

#include <time.h>

#include "../../app.h"
#include "Widgets.h"
#include "ClockLayout.h"
#include "../i18n/Dates.h"

static const SparseGFXfont* digitsFont(clockui::Digits d) {
  return d == clockui::Digits::D64 ? &ClockDigits_64 : d == clockui::Digits::D110 ? &ClockDigits_110 : &ClockDigits_180;
}

static int digitsWidth(clockui::Digits d, const char* text) { return getSparseStringWidth(digitsFont(d), text); }

// The clock (stage 7, rebuilt in 7b): in its own time zone (Option), else the device's.
void updateClock(LayoutItem* infoClock) {
  if (!infoClock) return;
  const char* zone = infoClock->Option[0] ? infoClock->Option : device_timezone[0] ? device_timezone : "UTC0";
  setenv("TZ", zone, 1);
  tzset();
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    DBG_ERROR("Clock: no time yet");
    return;
  }

  char timeStr[16];
  const char* amPm = nullptr;
  if (clock_24h) {
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
  } else {
    int h12 = timeinfo.tm_hour % 12 == 0 ? 12 : timeinfo.tm_hour % 12;
    snprintf(timeStr, sizeof(timeStr), "%d:%02d", h12, timeinfo.tm_min);
    amPm = timeinfo.tm_hour < 12 ? "AM" : "PM";
  }

  canvas().fillRect(infoClock->PosX, infoClock->PosY, infoClock->Width, infoClock->Height, GxEPD_WHITE);

  // Stage 7: filled Montserrat digits, AM/PM next to them, the date below (clockui::layoutClock).
  char date[48];
  i18n::formatDayHeader(timeinfo, ui_lang, date, sizeof(date));  // "Freitag, 20. November"
  if (getSparseStringWidth(&Heading_22, date) > infoClock->Width - 2 * clockui::PAD)
    i18n::formatStatusDate(timeinfo, ui_lang, date, sizeof(date));  // "Fr 20. Nov"
  clockui::Layout l = clockui::layoutClock(infoClock->Width, infoClock->Height, timeStr, amPm, digitsWidth,
                                           amPm ? getSparseStringWidth(&Heading_22, amPm) : 0,
                                           getSparseStringWidth(&Heading_22, date));
  const SparseGFXfont* digits = digitsFont(l.digits);
  int x = infoClock->PosX, y = infoClock->PosY;
  drawSparseString(digits, x + l.digitsX, y + l.digitsBaseline, timeStr, GxEPD_BLACK);
  if (l.showAmPm) drawSparseString(&Heading_22, x + l.amPmX, y + l.digitsBaseline, amPm, GxEPD_BLACK);
  if (l.showDate)
    drawSparseStringCentered(&Heading_22, x + infoClock->Width / 2, y + l.dateBaseline, date, GxEPD_BLACK);
}
