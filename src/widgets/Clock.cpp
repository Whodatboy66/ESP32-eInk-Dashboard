#include "Clock.h"

#include <time.h>

#include "../../app.h"
#include "Widgets.h"

// Clock widget: uses `infoClock->Extra1` as TZ string (e.g. "EST5EDT,...").
void updateClock(LayoutItem* infoClock) {
  if (!infoClock) return;
  const char* tzValue = infoClock->Extra1[0] != '\0'
    ? infoClock->Extra1
    : (device_timezone[0] != '\0' ? device_timezone : "UTC0");
  setenv("TZ", tzValue, 1);

  tzset();

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    DBG_ERROR("Failed to obtain time");
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

  int centerX = infoClock->PosX + infoClock->Width / 2;
  // Vertically centered: the original 155 px place had the baseline 110 px down.
  int baseline = infoClock->PosY + 110 + (infoClock->Height - 155) / 2;

  if (!amPm) {
    drawSparseStringCentered(&HighSpeed_130, centerX, baseline, timeStr, GxEPD_BLACK);
    return;
  }
  // 12 h: digits and AM/PM centered together. In a place too narrow for both ("12:58" in a 2x1
  // cell: the digits fill it, and they reach from top to bottom), only the time is shown.
  int digitsW = getSparseStringWidth(&HighSpeed_130, timeStr);
  int suffixW = getSparseStringWidth(&epaperFont, amPm);
  int groupW = digitsW + 6 + suffixW;
  if (groupW <= infoClock->Width - 4) {
    int x = centerX - groupW / 2;
    drawSparseString(&HighSpeed_130, x, baseline, timeStr, GxEPD_BLACK);
    drawSparseString(&epaperFont, x + digitsW + 6, baseline, amPm, GxEPD_BLACK);
  } else {
    drawSparseStringCentered(&HighSpeed_130, centerX, baseline, timeStr, GxEPD_BLACK);
  }
}


