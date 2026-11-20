#include "StatusBar.h"

#include "../../app.h"
#include "../core/Problems.h"
#include "../hardware/BatteryCurve.h"
#include "../hardware/BatteryMonitor.h"
#include "../services/Bambu.h"
#include "../update/OtaFlow.h"
#include "Widgets.h"
#include "../ui/Canvas.h"

// Alert icon followed by the names of the failing sources, e.g. "[!] WiFi Weather".
// Names that don't fit before maxX are summarized as "+N".
static void drawProblems(int16_t x, int16_t y, int16_t maxX) {
  problemsShown = problems;
  if (!problems) return;

  drawSparseChar(&MDI_22_Sparse, x, y, MDI_ALERT, ALERT_COLOR);
  x += 24;

  const int count = sizeof(PROBLEM_LABELS) / sizeof(PROBLEM_LABELS[0]);
  int remaining = __builtin_popcount(problems);
  for (int i = 0; i < count; i++) {
    if (!(problems & (1 << i))) continue;
    int16_t w = getSparseStringWidth(&epaperFont, PROBLEM_LABELS[i]);
    char more[8];
    snprintf(more, sizeof(more), "+%d", remaining);
    // Keep room for a "+N" summary unless this is the last name.
    int16_t reserve = remaining > 1 ? getSparseStringWidth(&epaperFont, more) + 6 : 0;
    if (x + w + reserve > maxX) {
      drawSparseString(&epaperFont, x, y, more, ALERT_COLOR);
      return;
    }
    drawSparseString(&epaperFont, x, y, PROBLEM_LABELS[i], ALERT_COLOR);
    x += w + 8;
    remaining--;
  }
}

void drawStatus(LayoutItem* item) {
  DBG(F("Battery Draw"));

  float voltage = 0.0f;
  char buf[20] = "";
  if (hasBatteryPin()) {
    voltage = readBattery();
    batteryNoteQuiet(voltage);  // drawn with Wi-Fi off: the reading the setup app shows
    // No battery measured: the board runs on USB (a voltage there would only confuse).
    if (batteryReadingPlausible(voltage)) snprintf(buf, sizeof(buf), "%.2f V", voltage);
    else snprintf(buf, sizeof(buf), "USB");
  }

  char versionBuf[48];
  if (otaAvailBuild > FW_BUILD)  // notify only: a newer release waits for a double tap
    snprintf(versionBuf, sizeof(versionBuf), "Update available  v%s", FW_VERSION);
  else
    snprintf(versionBuf, sizeof(versionBuf), "v%s", FW_VERSION);

  canvas().fillRect(item->PosX, item->PosY, item->Width, item->Height, GxEPD_WHITE);

  int16_t leftX = item->PosX;
  if (hasBatteryPin()) {
    drawSparseString(&epaperFont, item->PosX, item->PosY + 16, buf, GxEPD_BLACK);
    leftX += getSparseStringWidth(&epaperFont, buf) + 10;
  }

  // Icons are drawn right-to-left in fixed 22px slots so they never overlap
  // regardless of which subset is active.
  constexpr int16_t ICON_STEP = 22;
  constexpr int16_t RIGHT_PAD = 4;

  int16_t iconReserve = RIGHT_PAD;
  if (hasBatteryPin())                  iconReserve += ICON_STEP;
  if (isPrinting || previousIsPrinting) iconReserve += ICON_STEP;

  int16_t versionWidth = getSparseStringWidth(&epaperFont, versionBuf);
  int16_t versionX = item->PosX + item->Width - iconReserve - versionWidth;
  int16_t minVersionX = item->PosX + (hasBatteryPin() ? getSparseStringWidth(&epaperFont, buf) + 8 : 0);
  if (versionX < minVersionX) versionX = minVersionX;
  drawSparseString(&epaperFont, versionX, item->PosY + 16, versionBuf, GxEPD_BLACK);

  drawProblems(leftX, item->PosY + 16, versionX - 8);

  // Draw icons right to left
  int16_t ix = item->PosX + item->Width - RIGHT_PAD;

  if (hasBatteryPin()) {
    int percentage = batteryPercent(voltage);
    if (batteryReadingPlausible(voltage)) DBGF("[Battery] %.2f V, %d%%", voltage, percentage);
    else DBGF("[Battery] %.2f V: no battery measured (USB power)", voltage);
    // MDI battery-10 .. battery-90 are 0xF007A .. 0xF0082, full battery is 0xF0079.
    int tenths = constrain((percentage + 5) / 10, 1, 10);
    uint32_t charCode = tenths == 10 ? 0xF0079 : 0xF007A + (tenths - 1);
    ix -= ICON_STEP;
    drawSparseChar(&MDI_22_Sparse, ix, item->PosY + 16, charCode, GxEPD_BLACK);
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
  if (!problems) return;
  const int count = sizeof(PROBLEM_LABELS) / sizeof(PROBLEM_LABELS[0]);
  const char* first = "";
  for (int i = 0; i < count; i++)
    if (problems & (1 << i)) {
      first = PROBLEM_LABELS[i];
      break;
    }
  char text[32];
  int n = __builtin_popcount(problems);
  if (n > 1) snprintf(text, sizeof(text), "%s +%d", first, n - 1);
  else snprintf(text, sizeof(text), "%s", first);
  int16_t w = getSparseStringWidth(&epaperFont, text) + 26;
  int16_t x = 800 - w - 6, y = 480 - 6;
  canvas().fillRect(x - 4, y - 19, w + 8, 23, GxEPD_BLACK);
  drawSparseChar(&MDI_22_Sparse, x, y + 2, MDI_ALERT, GxEPD_WHITE);
  drawSparseString(&epaperFont, x + 24, y, text, GxEPD_WHITE);
}
