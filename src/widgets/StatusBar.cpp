#include "StatusBar.h"
#include "StatusMarks.h"
#include "Personal.h"
#include "../core/Holidays.h"
#include "../services/WifiService.h"

#include <time.h>

#include "../../app.h"
#include "../core/AppState.h"
#include "../core/Problems.h"
#include "../i18n/Dates.h"
#include "../hardware/BatteryCurve.h"
#include "../hardware/BatteryMonitor.h"
#include "../services/Bambu.h"
#include "../update/OtaFlow.h"
#include "Widgets.h"
#include "../ui/Canvas.h"

// The failing sources' names in flag order (PROBLEM_LIST, Problems.h); returns how many.
static int activeProblems(const char* names[PROBLEM_COUNT]) {
  int n = 0;
  for (int bit = 0; bit < PROBLEM_COUNT; bit++)
    if (problems & (1u << bit)) names[n++] = tr(PROBLEM_LABELS[bit]);
  return n;
}

// "⚠ WLAN Wetter +2": the alert icon, then the names that fit before maxX; the rest as "+N" (room
// for it is kept unless the name is the last one).
static void drawProblems(int16_t x, int16_t y, int16_t maxX) {
  problemsShown = problems;
  const char* names[PROBLEM_COUNT];
  const int n = activeProblems(names);
  if (!n) return;
  drawSparseChar(&MDI_22_Sparse, x, y, MDI_ALERT, ALERT_COLOR);
  x += 24;
  for (int i = 0; i < n; i++) {
    char rest[8];
    snprintf(rest, sizeof(rest), "+%d", n - i);
    const int16_t w = getSparseStringWidth(&epaperFont, names[i]);
    const int16_t keep = i + 1 < n ? getSparseStringWidth(&epaperFont, rest) + 6 : 0;
    if (x + w + keep > maxX) {
      drawSparseString(&epaperFont, x, y, rest, ALERT_COLOR);
      return;
    }
    drawSparseString(&epaperFont, x, y, names[i], ALERT_COLOR);
    x += w + 8;
  }
}

void drawStatus(LayoutItem* item) {

  float voltage = 0.0f;
  char buf[40] = "";
  // A full-width bar (grid layouts) starts with the date ("Fr 20. Nov" / "Fri 20 Nov"): the clock
  // widget has no room for it below its digits. Classic keeps its own date in the agenda.
  holidays::Holiday holiday;
  bool isHoliday = false;
  if (item->Width >= 600) {
    time_t now = time(nullptr);
    struct tm lt;
    if (now > 1700000000 && localtime_r(&now, &lt)) {
      i18n::formatStatusDate(lt, ui_lang, buf, sizeof(buf));
      // Stage 7: a public holiday's name after the date ("Mi 6. Jan · Heilige Drei Könige").
      isHoliday = holidays_on && holidays::holidayOn(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
                                                     holidays::parseState(holiday_state), &holiday);
    }
  }
  // Right end: the battery icon and, right of it, the charge in percent (the voltage is in the log
  // and Diagnostics); without a measured battery "USB" (the board runs on USB power).
  char pct[12] = "";
  if (hasBatteryPin()) {
    voltage = batteryForDisplay();  // with Wi-Fi on (edit mode preview): the last reading without it
    if (batteryReadingPlausible(voltage)) snprintf(pct, sizeof(pct), "%d %%", batteryPercent(voltage));
    else snprintf(pct, sizeof(pct), "USB");
  }
  const int16_t pctW = pct[0] ? getSparseStringWidth(&epaperFont, pct) + 4 : 0;

  char versionBuf[48];
  if (otaAvailBuild > FW_BUILD)  // notify only: a newer release waits for a double tap
    snprintf(versionBuf, sizeof(versionBuf), tr(i18n::S::UpdateAvailable), FW_VERSION);
  else
    snprintf(versionBuf, sizeof(versionBuf), "v%s", FW_VERSION);

  canvas().fillRect(item->PosX, item->PosY, item->Width, item->Height, GxEPD_WHITE);

  int16_t leftX = item->PosX;
  if (buf[0]) {
    drawSparseString(&epaperFont, item->PosX, item->PosY + 16, buf, GxEPD_BLACK);
    leftX += getSparseStringWidth(&epaperFont, buf) + 10;
  }

  // Icons are drawn right-to-left in fixed 22px slots so they never overlap
  // regardless of which subset is active.
  constexpr int16_t ICON_STEP = 22;
  constexpr int16_t RIGHT_PAD = 4;

  constexpr int16_t WIFI_W = 24;  // the Wi-Fi bars (15 px) with gaps to the version and the battery
  const int bars = statusbar::signalBars(lastRssi, (problems & PROBLEM_WIFI) != 0);
  int16_t iconReserve = RIGHT_PAD + pctW;
  if (hasBatteryPin())                  iconReserve += ICON_STEP;
  if (bars)                             iconReserve += WIFI_W;
  if (isPrinting || previousIsPrinting) iconReserve += ICON_STEP;

  int16_t versionWidth = getSparseStringWidth(&epaperFont, versionBuf);
  int16_t versionX = item->PosX + item->Width - iconReserve - versionWidth;
  int16_t minVersionX = item->PosX + (buf[0] ? getSparseStringWidth(&epaperFont, buf) + 8 : 0);
  if (versionX < minVersionX) versionX = minVersionX;
  drawSparseString(&epaperFont, versionX, item->PosY + 16, versionBuf, GxEPD_BLACK);

  if (isHoliday) {
    int powerW = powerNote[0] ? getSparseStringWidth(&epaperFont, powerNote) : 0;
    int space = statusbar::holidaySpace(leftX, powerW, versionX);
    if (space) {
      char name[64];
      snprintf(name, sizeof(name), "· %s", tr(holiday.name));
      int w = getSparseStringWidth(&epaperFont, name);
      drawFittedString(leftX, item->PosY + 16, space, name, GxEPD_BLACK);
      leftX += (w < space ? w : space) + 12;
    }
  }

  // Power note (stage 6): "Nacht bis 06:00", "Akku-Sparen: alle 15 Min.", "Uhr alle 5 Min.".
  powerNoteShown = textHash(powerNote);
  if (powerNote[0] && leftX + getSparseStringWidth(&epaperFont, powerNote) < versionX - 8) {
    drawSparseString(&epaperFont, leftX, item->PosY + 16, powerNote, GxEPD_BLACK);
    leftX += getSparseStringWidth(&epaperFont, powerNote) + 12;
  }

  drawProblems(leftX, item->PosY + 16, versionX - 8);

  // Right to left: percent, battery icon, printer icon.
  int16_t ix = item->PosX + item->Width - RIGHT_PAD;
  if (pct[0]) {
    ix -= pctW;
    drawSparseString(&epaperFont, ix + 4, item->PosY + 16, pct, GxEPD_BLACK);
  }

  if (hasBatteryPin()) {
    int percentage = batteryPercent(voltage);
    if (batteryReadingPlausible(voltage)) DBGF("[Battery] %.2f V, %d%%", voltage, percentage);
    else DBGF("[Battery] %.2f V: no battery measured (USB power)", voltage);
    // battery-10 .. battery-90 follow each other (Icons.h), then the full one.
    int tenths = constrain((percentage + 5) / 10, 1, 10);
    uint32_t charCode = tenths == 10 ? MDI_BATTERY_FULL : MDI_BATTERY_10 + (tenths - 1);
    ix -= ICON_STEP;
    drawSparseChar(&MDI_22_Sparse, ix, item->PosY + 16, charCode, GxEPD_BLACK);
  }
  if (bars) {  // four bars, 3 px wide, 4 to 13 px high: filled up to the strength, else outlined
    ix -= WIFI_W;
    for (int i = 0; i < 4; i++) {
      int h = 4 + 3 * i, x = ix + 6 + i * 4, y = item->PosY + 17 - h;
      if (i < bars) canvas().fillRect(x, y, 3, h, GxEPD_BLACK);
      else canvas().drawRect(x, y, 3, h, GxEPD_BLACK);
    }
  }
  if (isPrinting || previousIsPrinting) {
    ix -= ICON_STEP;
    drawSparseChar(&MDI_22_Sparse, ix, item->PosY + 16, MDI_PRINT_ON, GxEPD_BLACK);
  }
}

// Without a status bar (box layouts): a small black mark in the bottom-right corner, "! Weather +1",
// only while there is a problem.
void drawProblemCorner() {
  problemsShown = problems;
  const char* names[PROBLEM_COUNT];
  const int n = activeProblems(names);
  if (!n) return;
  char text[32];
  if (n > 1) snprintf(text, sizeof(text), "%s +%d", names[0], n - 1);
  else snprintf(text, sizeof(text), "%s", names[0]);
  const int16_t w = getSparseStringWidth(&epaperFont, text) + 26;
  const int16_t x = 800 - w - 6, y = 480 - 6;  // the bottom-right corner, white on black
  canvas().fillRect(x - 4, y - 19, w + 8, 23, GxEPD_BLACK);
  drawSparseChar(&MDI_22_Sparse, x, y + 2, MDI_ALERT, GxEPD_WHITE);
  drawSparseString(&epaperFont, x + 24, y, text, GxEPD_WHITE);
}
