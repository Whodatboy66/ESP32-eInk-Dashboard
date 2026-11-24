#include "Personal.h"

#include <stdio.h>
#include <string.h>

#include "../personal/UserStore.h"
#include "../ui/Canvas.h"
#include "../ui/SparseFont.h"
#include "Widgets.h"

using namespace personal;

namespace {

constexpr int LINE_H = 22;     // text line height (as the agenda)
constexpr int FIRST_LINE = 18; // baseline of the first line below the top

int textWidth(const char* s, int len) {
  char buf[256];
  if (len >= (int)sizeof(buf)) len = sizeof(buf) - 1;
  memcpy(buf, s, len);
  buf[len] = '\0';
  return getSparseStringWidth(&epaperFont, buf);
}

// Black box with white text (overdue date, "Today").
int drawInverted(int16_t xRight, int16_t y, const char* text) {
  int w = getSparseStringWidth(&epaperFont, text);
  int x = xRight - w;
  canvas().fillRect(x - 3, y - 15, w + 6, 19, GxEPD_BLACK);
  drawSparseString(&epaperFont, x, y, text, GxEPD_WHITE);
  return w + 6;
}

int linesThatFit(const LayoutItem* item) {
  int n = (item->Height - FIRST_LINE + LINE_H - 4) / LINE_H;
  return n < 1 ? 1 : n;
}

}  // namespace

void drawFittedString(int16_t x, int16_t y, int maxWidth, const char* text, uint16_t color) {
  if (maxWidth <= 0) return;
  if (getSparseStringWidth(&epaperFont, text) <= maxWidth) {
    drawSparseString(&epaperFont, x, y, text, color);
    return;
  }
  static const char* const ELLIPSIS = "\xE2\x80\xA6";  // …
  int ellW = getSparseStringWidth(&epaperFont, ELLIPSIS);
  char buf[256];
  size_t used = 0;
  for (const char* p = text; *p;) {
    int len;
    utf8Decode(p, &len);
    if (used + len + 4 >= sizeof(buf)) break;
    memcpy(buf + used, p, len);
    buf[used + len] = '\0';
    if (getSparseStringWidth(&epaperFont, buf) + ellW > maxWidth) {
      buf[used] = '\0';
      break;
    }
    used += len;
    p += len;
  }
  while (used && buf[used - 1] == ' ') buf[--used] = '\0';
  strcat(buf, ELLIPSIS);
  drawSparseString(&epaperFont, x, y, buf, color);
}

void tasksWidget(LayoutItem* item) {
  if (!item) return;
  LayoutItem body = headed(*item, tr(i18n::S::WTasks));
  item = &body;
  const TaskList& list = userTasks();
  Date today = localToday();
  int order[MAX_TASKS];
  int open = openTaskOrder(list, today, order, MAX_TASKS);
  int y = item->PosY + FIRST_LINE;
  const int x = item->PosX, right = item->PosX + item->Width - 16;  // clear of the neighbor
  if (open == 0) {
    drawSparseString(&epaperFont, x, y, tr(list.count ? i18n::S::TasksAllDone : i18n::S::TasksNone), GxEPD_BLACK);
    return;
  }
  int maxLines = linesThatFit(item);
  int shown = open <= maxLines ? open : maxLines - 1;  // the last line says how many more
  for (int i = 0; i < shown; i++) {
    const Task& t = list.items[order[i]];
    canvas().drawRect(x + 1, y - 13, 13, 13, GxEPD_BLACK);
    if (t.prio == PRIO_HIGH) canvas().fillRect(x + 4, y - 10, 7, 7, GxEPD_BLACK);  // high priority: filled
    char label[16];
    bool overdue;
    dueLabel(t, today, label, sizeof(label), &overdue, ui_lang);
    int labelW = 0;
    if (label[0]) {
      if (overdue) {
        labelW = drawInverted(right, y, label);
      } else {
        labelW = getSparseStringWidth(&epaperFont, label);
        drawSparseString(&epaperFont, right - labelW, y, label, GxEPD_BLACK);
      }
      labelW += 8;
    }
    drawFittedString(x + 20, y, right - labelW - (x + 20), t.title, GxEPD_BLACK);
    y += LINE_H;
  }
  if (shown < open) {
    char more[24];
    snprintf(more, sizeof(more), tr(i18n::S::TasksMore), open - shown);
    drawSparseString(&epaperFont, x + 20, y, more, GxEPD_BLACK);
  }
}

void noteWidget(LayoutItem* item) {
  if (!item) return;
  LayoutItem body = headed(*item, tr(i18n::S::WNote));
  item = &body;
  const char* text = userNote();
  int x = item->PosX, y = item->PosY + FIRST_LINE;
  int maxWidth = item->Width - 8;
  if (!text[0]) {
    drawSparseString(&epaperFont, x, y, tr(i18n::S::NoteNone), GxEPD_BLACK);
    return;
  }
  constexpr int MAX_LINES = 20;
  TextLine lines[MAX_LINES];
  int maxLines = linesThatFit(item);
  if (maxLines > MAX_LINES) maxLines = MAX_LINES;
  bool cut = false;
  int n = wrapText(text, maxWidth, textWidth, lines, maxLines, &cut);
  for (int i = 0; i < n; i++) {
    char buf[256];
    int len = lines[i].len < (int)sizeof(buf) - 4 ? lines[i].len : (int)sizeof(buf) - 4;
    memcpy(buf, text + lines[i].start, len);
    buf[len] = '\0';
    if (cut && i == n - 1) {
      // Text left over: the last line ends with "…".
      strcat(buf, " \xE2\x80\xA6");
      drawFittedString(x, y, maxWidth, buf, GxEPD_BLACK);
    } else {
      drawSparseString(&epaperFont, x, y, buf, GxEPD_BLACK);
    }
    y += LINE_H;
  }
}

void birthdaysWidget(LayoutItem* item) {
  if (!item) return;
  LayoutItem body = headed(*item, tr(i18n::S::WBirthdays));
  item = &body;
  Date today = localToday();
  Upcoming up[MAX_BIRTHDAYS];
  int n = upcomingBirthdays(userBirthdays(), userFeedBirthdays(), today, up, MAX_BIRTHDAYS, BDAY_HORIZON_DAYS,
                            &userCalBirthdays());
  int x = item->PosX, y = item->PosY + FIRST_LINE;
  int right = item->PosX + item->Width - 6;
  if (n == 0) {
    drawSparseString(&epaperFont, x, y, tr(today.valid() ? i18n::S::BdayNone : i18n::S::WBirthdays), GxEPD_BLACK);
    return;
  }
  int maxLines = linesThatFit(item);
  const int labelCol = 92;  // "Tomorrow", "in 12 T." fit
  for (int i = 0; i < n && i < maxLines; i++) {
    char label[16];
    if (up[i].days <= 1 || up[i].days > 14) {
      if (up[i].days <= 1) daysLabel(up[i].days, label, sizeof(label), ui_lang);
      else snprintf(label, sizeof(label), "%d.%d.", up[i].date.d, up[i].date.m);
    } else {
      daysLabel(up[i].days, label, sizeof(label), ui_lang);
    }
    if (up[i].days == 0) {
      int w = getSparseStringWidth(&epaperFont, label);
      drawInverted(x + 3 + w, y, label);
    } else {
      drawSparseString(&epaperFont, x, y, label, GxEPD_BLACK);
    }
    char name[96];
    if (up[i].age > 0) snprintf(name, sizeof(name), "%s (%d)", up[i].who->name, up[i].age);
    else snprintf(name, sizeof(name), "%s", up[i].who->name);
    drawFittedString(x + labelCol, y, right - (x + labelCol), name, GxEPD_BLACK);
    y += LINE_H;
  }
}
