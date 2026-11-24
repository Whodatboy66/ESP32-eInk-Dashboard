// Month view (layout ID 256), stage 7 (docs/plans/STAGE7_NEW_LOOK.md task 10, design in
// docs/plans/GIFT_EDITION.md): one filled mark (today), everything else thin. Every mark has its
// own place in a day cell, so nothing overlaps: ① the number (today: small filled rounded square,
// public holiday: the same as a frame), ② the mark row (appointment dot, birthday cake), ③ the
// school holiday line on the cell's bottom edge. What goes where is decided in MonthModel.h.
#include "MonthView.h"

#include <time.h>

#include "../core/AppState.h"
#include "../i18n/Dates.h"
#include "../personal/UserStore.h"
#include "MonthModel.h"
#include "Personal.h"

int daysInMonth(int year, int month) { return personal::daysInMonth(year, month); }

namespace {

using month::Geometry;
using month::Highlight;
using month::Kind;
using month::Marks;

// 9 x 9 symbols, drawn from these patterns ('#' = black).
const char* const CAKE[9] = {"..#.#.#..", "..#.#.#..", ".#######.", ".#.....#.", ".#######.",
                             "#########", "#.......#", "#.......#", "#########"};
const char* const STAR[9] = {"....#....", "....#....", "...###...", "#########", ".#######.",
                             "..#####..", "..##.##..", ".##...##.", ".#.....#."};

void drawPattern(int x, int y, const char* const* rows, uint16_t color) {
  for (int r = 0; r < 9; r++)
    for (int c = 0; c < 9; c++)
      if (rows[r][c] == '#') canvas().drawPixel(x + c, y + r, color);
}

// The symbol of a highlight, 9 x 9, its top-left corner at (x, y).
void drawSymbol(Kind k, int x, int y) {
  if (k == Kind::Holiday) drawPattern(x, y, STAR, GxEPD_BLACK);
  else if (k == Kind::Birthday) drawPattern(x, y, CAKE, GxEPD_BLACK);
  else canvas().fillRect(x, y + 3, 9, 3, GxEPD_BLACK);  // school holidays: a short line
}

// "Mi 6. Jan  Heilige Drei Könige", "Weihnachtsferien bis Sa 9. Jan", "So 17. Jan  Anna wird 60".
void highlightText(const Highlight& h, char* out, size_t cap, bool withDate) {
  std::tm t{};
  t.tm_year = h.date.y - 1900;
  t.tm_mon = h.date.m - 1;
  t.tm_mday = h.date.d;
  t.tm_wday = (personal::weekday(h.date) + 1) % 7;  // tm: 0 = Sunday
  char date[24];
  i18n::formatStatusDate(t, ui_lang, date, sizeof(date));
  char what[96];
  using i18n::S;
  if (h.kind == Kind::Holiday) snprintf(what, sizeof(what), "%s", tr(h.holiday));
  else if (h.kind == Kind::Birthday && h.age > 0) snprintf(what, sizeof(what), tr(S::HlBirthdayAge), h.text, h.age);
  else snprintf(what, sizeof(what), "%s", h.text);
  if ((h.kind == Kind::SchoolStart || h.kind == Kind::SchoolEnd) && !withDate) {
    // Side column: the date is on the line above.
    snprintf(out, cap, tr(h.kind == Kind::SchoolStart ? S::HlSchoolStarts : S::HlSchoolEnds), what);
  } else if (h.kind == Kind::SchoolStart || h.kind == Kind::SchoolEnd) {
    snprintf(out, cap, tr(h.kind == Kind::SchoolStart ? S::HlSchoolFrom : S::HlSchoolUntil), what, date);
  } else if (withDate) {
    snprintf(out, cap, "%s  %s", date, what);
  } else {
    snprintf(out, cap, "%s", what);
  }
}

// The data the month view draws from: holidays and school holidays only when switched on.
struct Inputs {
  holidays::State state;
  const school::SchoolList* school;
  const personal::BirthdayList* lists[3];
  personal::Date today;
};

Inputs inputs(const std::tm& now) {
  Inputs in;
  in.state = holidays_on ? holidays::parseState(holiday_state) : holidays::State::None;
  in.school = school_on ? &schoolHolidays : nullptr;
  in.lists[0] = &userBirthdays();
  in.lists[1] = &userFeedBirthdays();
  in.lists[2] = &userCalBirthdays();
  in.today = personal::Date{now.tm_year + 1900, now.tm_mon + 1, now.tm_mday};
  return in;
}

uint32_t appointmentBits() {
  uint32_t bits = 0;
  for (int i = 0; i < cal.daysCount; i++)
    if (cal.daysWithEvents[i] >= 1 && cal.daysWithEvents[i] <= 31) bits |= 1u << cal.daysWithEvents[i];
  return bits;
}

Marks marksOf(const Inputs& in, int year, int mon, bool currentMonth) {
  // Appointment marks (cal.daysWithEvents) exist only for the current month.
  return month::marksFor(year, mon, currentMonth ? in.today.d : 1, in.state, in.school,
                         currentMonth ? appointmentBits() : 0, in.lists, 3);
}

bool has(uint32_t bits, int day) { return (bits >> day) & 1u; }

// One day cell at (x, y), cellW x cellH.
void drawDay(const Geometry& g, int x, int y, int day, bool weekend, bool isToday, const Marks& m) {
  char num[4];
  snprintf(num, sizeof(num), "%d", day);
  const SparseGFXfont* font = g.bigDigits ? (weekend ? &DigitsBold_22 : &Digits_22)
                                          : (weekend ? &DigitsBold_16 : &epaperFont);
  const int boxH = g.bigDigits ? 28 : 20;
  int textW = getSparseStringWidth(font, num);
  int boxW = textW + 10 < boxH + 2 ? boxH + 2 : textW + 10;
  if (boxW > g.cellW - 2) boxW = g.cellW - 2;
  int cx = x + g.cellW / 2;
  int boxTop = g.markRow ? y + 3 : y + (g.cellH - 3 - boxH) / 2;
  int baseline = boxTop + (g.bigDigits ? 22 : 15);
  // ① the number
  bool holiday = has(m.holiday, day);
  if (isToday) {
    canvas().fillRoundRect(cx - boxW / 2, boxTop, boxW, boxH, 4, GxEPD_BLACK);
    drawSparseStringCentered(font, cx, baseline, num, GxEPD_WHITE);
  } else {
    if (holiday) canvas().drawRoundRect(cx - boxW / 2, boxTop, boxW, boxH, 4, GxEPD_BLACK);
    drawSparseStringCentered(font, cx, baseline, num, GxEPD_BLACK);
  }
  // ② the mark row
  if (g.markRow) {
    bool dot = has(m.appointment, day), cake = has(m.birthday, day);
    int rowY = boxTop + boxH + 2;  // top of the 9 px row
    if (dot && cake) {
      canvas().fillCircle(cx - 7, rowY + 4, 2, GxEPD_BLACK);
      drawPattern(cx - 1, rowY, CAKE, GxEPD_BLACK);
    } else if (dot) {
      canvas().fillCircle(cx, rowY + 4, 2, GxEPD_BLACK);
    } else if (cake) {
      drawPattern(cx - 4, rowY, CAKE, GxEPD_BLACK);
    }
  }
  // ③ school holidays: the whole width, so neighbouring days join into one line
  if (has(m.school, day)) canvas().fillRect(x, y + g.cellH - 3, g.cellW, 2, GxEPD_BLACK);
}

// Highlights as lines (footer) or as two-line entries (side column).
void drawFooter(const Inputs& in, int x, int y, int w, int lines) {
  Highlight h[3];
  int n = month::highlights(in.today, in.state, in.school, in.lists, 3, h, lines > 3 ? 3 : lines);
  for (int i = 0; i < n; i++) {
    int base = y + i * month::LINE_H + 15;
    drawSymbol(h[i].kind, x, base - 10);
    char text[160];
    highlightText(h[i], text, sizeof(text), true);
    drawFittedString(x + 14, base, w - 16, text, GxEPD_BLACK);
  }
}

void drawSide(const Inputs& in, int x, int y, int w, int h) {
  drawSparseString(&Heading_22, x, y + 20, tr(i18n::S::HeadUpcoming), GxEPD_BLACK);
  canvas().drawFastHLine(x, y + 26, w - 8, GxEPD_BLACK);
  const int entryH = 2 * month::LINE_H + 2;
  int cap = (h - 34) / entryH;
  if (cap > 10) cap = 10;
  Highlight list[10];
  int n = month::highlights(in.today, in.state, in.school, in.lists, 3, list, cap);
  for (int i = 0; i < n; i++) {
    int top = y + 34 + i * entryH;
    std::tm t{};
    t.tm_year = list[i].date.y - 1900;
    t.tm_mon = list[i].date.m - 1;
    t.tm_mday = list[i].date.d;
    t.tm_wday = (personal::weekday(list[i].date) + 1) % 7;
    char date[24], text[160];
    i18n::formatStatusDate(t, ui_lang, date, sizeof(date));
    drawSymbol(list[i].kind, x, top + 5);
    drawSparseString(&epaperFont, x + 14, top + 15, date, GxEPD_BLACK);
    highlightText(list[i], text, sizeof(text), false);
    drawFittedString(x + 14, top + 15 + month::LINE_H, w - 22, text, GxEPD_BLACK);
  }
}

// Title and weekday row of a grid; returns the y of the first row.
int drawHeader(const LayoutItem* item, const Geometry& g, const char* title) {
  int gx = item->PosX + g.gridX;
  drawSparseStringCentered(&Heading_22, gx + g.gridW / 2, item->PosY + 20, title, GxEPD_BLACK);
  int dowBase = item->PosY + month::TITLE_H + 15;
  int col0 = gx;
  if (g.kw) {
    drawSparseStringCentered(&epaperFont, col0 + g.cellW / 2, dowBase, tr(i18n::S::MonthWeekCol), GxEPD_BLACK);
    col0 += g.cellW;
  }
  for (int i = 0; i < 7; i++) {
    char label[8];
    snprintf(label, sizeof(label), "%s", i18n::weekdayInitial(i, ui_lang));
    if (g.design == month::Design::Compact) label[1] = '\0';  // one letter in narrow boxes
    drawSparseStringCentered(&epaperFont, col0 + i * g.cellW + g.cellW / 2, dowBase, label, GxEPD_BLACK);
  }
  return item->PosY + month::TITLE_H + month::DOW_H;
}

void drawWeekNumber(const Geometry& g, int x, int y, long mondayDay) {
  int yy, mm, dd;
  holidays::civilFromDays(mondayDay, yy, mm, dd);
  char kw[4];
  snprintf(kw, sizeof(kw), "%d", holidays::isoWeek(yy, mm, dd));
  const int boxH = g.bigDigits ? 28 : 20;  // on the baseline of the day numbers (drawDay)
  int boxTop = g.markRow ? y + 3 : y + (g.cellH - 3 - boxH) / 2;
  drawSparseStringCentered(&epaperFont, x + g.cellW / 2, boxTop + (g.bigDigits ? 20 : 15), kw, GxEPD_BLACK);
}

void drawMonthGrid(LayoutItem* item, const Geometry& g, const Inputs& in) {
  char title[24];
  i18n::formatMonthTitle(in.today.y, in.today.m - 1, ui_lang, title, sizeof(title));
  int y0 = drawHeader(item, g, title);
  Marks m = marksOf(in, in.today.y, in.today.m, true);
  const int dim = personal::daysInMonth(in.today.y, in.today.m);
  const int start = personal::weekday(personal::Date{in.today.y, in.today.m, 1});  // 0 = Monday
  const long first = personal::dayNumber(personal::Date{in.today.y, in.today.m, 1});
  const int dayX = item->PosX + g.gridX + (g.kw ? g.cellW : 0);
  for (int r = 0; r < 6; r++) {
    int firstDayOfRow = r * 7 - start + 1;
    if (firstDayOfRow > dim) break;
    int y = y0 + r * g.cellH;
    if (g.kw) drawWeekNumber(g, item->PosX + g.gridX, y, first + firstDayOfRow - 1);
    for (int c = 0; c < 7; c++) {
      int d = firstDayOfRow + c;
      if (d < 1 || d > dim) continue;
      drawDay(g, dayX + c * g.cellW, y, d, c >= 5, d == m.today, m);
    }
  }
  const int rowsUsed = (start + dim + 6) / 7;  // 4 to 6: the footer follows the last week
  if (g.footerLines) drawFooter(in, item->PosX + g.gridX, y0 + rowsUsed * g.cellH + 2, g.gridW, g.footerLines);
  if (g.sideW) drawSide(in, item->PosX + g.gridW + 12, item->PosY, g.sideW - 12, item->Height);
}

// Low boxes: this week and the next, days of two months possible.
void drawTwoWeeks(LayoutItem* item, const Geometry& g, const Inputs& in) {
  char title[24];
  i18n::formatMonthTitle(in.today.y, in.today.m - 1, ui_lang, title, sizeof(title));
  int y0 = drawHeader(item, g, title);
  const long today = personal::dayNumber(in.today);
  const long monday = today - personal::weekday(in.today);
  int nextY, nextM, nd;
  holidays::civilFromDays(monday + 13, nextY, nextM, nd);
  Marks cur = marksOf(in, in.today.y, in.today.m, true);
  Marks other = (nextM != in.today.m) ? marksOf(in, nextY, nextM, false) : cur;
  int prevY, prevM, pd;
  holidays::civilFromDays(monday, prevY, prevM, pd);
  Marks before = (prevM != in.today.m) ? marksOf(in, prevY, prevM, false) : cur;
  const int dayX = item->PosX + g.gridX + g.cellW;  // after the KW column
  for (int r = 0; r < 2; r++) {
    int y = y0 + r * g.cellH;
    drawWeekNumber(g, item->PosX + g.gridX, y, monday + r * 7);
    for (int c = 0; c < 7; c++) {
      long dn = monday + r * 7 + c;
      int yy, mm, dd;
      holidays::civilFromDays(dn, yy, mm, dd);
      const Marks& m = mm == in.today.m ? cur : (dn < today ? before : other);
      drawDay(g, dayX + c * g.cellW, y, dd, c >= 5, dn == today, m);
    }
  }
  if (g.footerLines) drawFooter(in, item->PosX + g.gridX, y0 + 2 * g.cellH + 2, g.gridW, g.footerLines);
  if (g.sideW) drawSide(in, item->PosX + g.gridW + 12, item->PosY, g.sideW - 12, item->Height);
}

// 1 x 1: a calendar sheet.
void drawSheet(LayoutItem* item, const Inputs& in, const std::tm& now) {
  int cx = item->PosX + item->Width / 2;
  drawSparseStringCentered(&Heading_22, cx, item->PosY + 22, i18n::weekdayLong(now.tm_wday, ui_lang), GxEPD_BLACK);
  char num[4];
  snprintf(num, sizeof(num), "%d", now.tm_mday);
  drawSparseStringCentered(&ClockDigits_64, cx, item->PosY + 22 + 8 + 47, num, GxEPD_BLACK);
  char line[48];
  snprintf(line, sizeof(line), "%s · %s %d", i18n::monthLong(now.tm_mon, ui_lang), tr(i18n::S::MonthWeekCol),
           holidays::isoWeek(in.today.y, in.today.m, in.today.d));
  drawSparseStringCentered(&epaperFont, cx, item->PosY + 22 + 8 + 47 + 22, line, GxEPD_BLACK);
  Highlight h[1];
  if (month::highlights(in.today, in.state, in.school, in.lists, 3, h, 1) == 1) {
    char text[160];
    highlightText(h[0], text, sizeof(text), true);
    int y = item->PosY + item->Height - 8;
    drawSymbol(h[0].kind, item->PosX + 2, y - 10);
    drawFittedString(item->PosX + 16, y, item->Width - 20, text, GxEPD_BLACK);
  }
}

}  // namespace

void drawHolidayStar(int x, int y) { drawPattern(x, y, STAR, GxEPD_BLACK); }

void drawCalendar(LayoutItem* item) {
  if (!item) return;
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  if (now < 1700000000) return;  // clock not set yet: nothing sensible to draw
  Inputs in = inputs(t);
  Geometry g = month::layoutFor(item->Width, item->Height);
  switch (g.design) {
    case month::Design::Sheet: drawSheet(item, in, t); break;
    case month::Design::TwoWeeks: drawTwoWeeks(item, g, in); break;
    default: drawMonthGrid(item, g, in); break;
  }
}
