#pragma once
// The month view's logic (stage 7, docs/plans/STAGE7_NEW_LOOK.md task 6): which design fits a box,
// which day gets which mark, and the highlights of the footer / side column. MonthView.cpp only
// draws what this returns. Plain C++, tested on the PC (tests/host/test_month_model.cpp).
#include <cstdio>
#include <cstring>

#include "../core/Holidays.h"
#include "../personal/Birthdays.h"
#include "../services/SchoolHolidays.h"

namespace month {

// Sheet: 1 x 1 calendar sheet. TwoWeeks: low boxes (this and next week). Compact: narrow boxes (no
// week numbers, no footer). Full: grid with week numbers and footer lines below. Wide: grid with a
// side column.
enum class Design : unsigned char { Sheet, TwoWeeks, Compact, Full, Wide };

constexpr int TITLE_H = 26;          // month title
constexpr int DOW_H = 20;            // weekday row
constexpr int LINE_H = 20;           // a footer line
constexpr int MIN_MARK_CELL_H = 34;  // number + mark row + line need this; lower cells drop the mark row
constexpr int BIG_DIGITS_CELL_H = 56;

struct Geometry {
  Design design = Design::Sheet;
  bool kw = false;   // a week-number column
  int cols = 0;      // columns of the grid (7, or 8 with week numbers)
  int cellW = 0, cellH = 0;
  int gridX = 0, gridW = 0;  // relative to the box
  int sideW = 0;             // the side column right of the grid (0 = none)
  int footerLines = 0;       // footer lines below the grid
  bool markRow = false;      // room for the dot / cake row under the number
  bool bigDigits = false;    // the 22 px digits
};

inline Geometry layoutFor(int w, int h) {
  Geometry g;
  const int head = TITLE_H + DOW_H;
  if (w < 300 && h < 200) {
    g.design = Design::Sheet;
    return g;
  }
  if (h < 200) {
    g.design = Design::TwoWeeks;
    g.kw = true;
    g.cols = 8;
    g.sideW = w >= 600 ? 240 : 0;
    g.footerLines = g.sideW ? 0 : 1;
    g.gridW = w - g.sideW;
    g.cellW = g.gridW / g.cols;
    g.cellH = (h - head - g.footerLines * LINE_H) / 2;
  } else if (w < 300) {
    g.design = Design::Compact;
    g.cols = 7;
    g.gridW = w;
    g.cellW = w / 7;
    g.cellH = (h - head) / 6;
  } else if (w >= 560) {
    g.design = Design::Wide;
    g.kw = true;
    g.cols = 8;
    g.sideW = w >= 760 ? 240 : 200;
    g.gridW = w - g.sideW;
    g.cellW = g.gridW / g.cols;
    g.cellH = (h - head) / 6;
  } else {
    g.design = Design::Full;
    g.kw = true;
    g.cols = 8;
    g.gridW = w;
    g.cellW = w / g.cols;
    g.footerLines = 0;
    for (int n = 3; n >= 1; n--)
      if ((h - head - n * LINE_H) / 6 >= MIN_MARK_CELL_H) {
        g.footerLines = n;
        break;
      }
    g.cellH = (h - head - g.footerLines * LINE_H) / 6;
  }
  g.markRow = g.cellH >= MIN_MARK_CELL_H;
  g.bigDigits = g.cellH >= BIG_DIGITS_CELL_H;
  return g;
}

// Marks of one month: bit d = day d (1..31).
struct Marks {
  uint32_t holiday = 0, school = 0, appointment = 0, birthday = 0;
  int today = 0;  // day of the month, 0 = unknown (clock not set): then no marks at all
};

inline int birthdayDay(const personal::Birthday& b, int year) {
  return (b.month == 2 && b.day == 29 && !personal::isLeap(year)) ? 28 : b.day;
}

inline Marks marksFor(int year, int mon, int today, holidays::State state, const school::SchoolList* schoolList,
                      uint32_t appointmentDays, const personal::BirthdayList* const* lists, int listCount) {
  Marks m;
  if (today <= 0 || year <= 0 || mon < 1 || mon > 12) return m;
  m.today = today;
  const int dim = personal::daysInMonth(year, mon);
  const uint32_t valid = (dim >= 31 ? 0xFFFFFFFFu : ((1u << (dim + 1)) - 1)) & ~1u;
  holidays::Holiday list[24];
  int n = holidays::holidaysOfYear(year, state, list, 24);
  for (int i = 0; i < n; i++)
    if (list[i].month == mon) m.holiday |= 1u << list[i].day;
  if (schoolList) {
    long first = personal::dayNumber(personal::Date{year, mon, 1});
    for (int i = 0; i < schoolList->count; i++)
      for (int d = 1; d <= dim; d++) {
        long dn = first + d - 1;
        if (dn >= schoolList->r[i].fromDay && dn <= schoolList->r[i].toDay) m.school |= 1u << d;
      }
  }
  m.appointment = appointmentDays & valid;
  for (int l = 0; l < listCount; l++) {
    if (!lists || !lists[l]) continue;
    for (int i = 0; i < lists[l]->count; i++) {
      const personal::Birthday& b = lists[l]->items[i];
      if (b.month == mon && b.day >= 1) m.birthday |= 1u << birthdayDay(b, year);
    }
  }
  m.birthday &= valid;
  return m;
}

enum class Kind : unsigned char { Holiday, SchoolStart, SchoolEnd, SchoolDay, Birthday };  // SchoolDay: one day off

struct Highlight {
  personal::Date date;
  Kind kind = Kind::Holiday;
  i18n::S holiday = i18n::S::Count;  // Kind::Holiday: its name (drawn in the screen language)
  char text[personal::BDAY_NAME_BYTES] = "";  // school holiday or person's name
  int age = 0;                       // birthdays: 0 = unknown
};

constexpr int HIGHLIGHT_DAYS = 60;  // how far the footer looks ahead

// The highlights from today on (60 days), soonest first; the same day: holiday, school start,
// school end, birthdays. The same person on the same day appears once.
inline int highlights(const personal::Date& today, holidays::State state, const school::SchoolList* schoolList,
                      const personal::BirthdayList* const* lists, int listCount, Highlight* out, int cap) {
  if (!today.valid() || cap <= 0) return 0;
  const long from = personal::dayNumber(today), to = from + HIGHLIGHT_DAYS;
  // Sorted straight into out (at most cap entries): no big list on the stack.
  auto key = [](const Highlight& h) { return personal::dayNumber(h.date) * 8 + static_cast<int>(h.kind); };
  int n = 0;
  auto add = [&](const Highlight& h) {
    int pos = n;
    while (pos > 0 && key(out[pos - 1]) > key(h)) pos--;
    if (pos >= cap) return;
    for (int j = (n < cap ? n : cap - 1); j > pos; j--) out[j] = out[j - 1];
    out[pos] = h;
    if (n < cap) n++;
  };
  auto dateOf = [](long dn) {
    int y, m, d;
    holidays::civilFromDays(dn, y, m, d);
    return personal::Date{y, m, d};
  };
  for (int y = today.y; y <= today.y + 1; y++) {
    holidays::Holiday list[24];
    int k = holidays::holidaysOfYear(y, state, list, 24);
    for (int i = 0; i < k; i++) {
      personal::Date d{y, list[i].month, list[i].day};
      long dn = personal::dayNumber(d);
      if (dn < from || dn > to) continue;
      Highlight h;
      h.date = d;
      h.kind = Kind::Holiday;
      h.holiday = list[i].name;
      add(h);
    }
  }
  if (schoolList) {
    for (int i = 0; i < schoolList->count; i++) {
      const school::Range& r = schoolList->r[i];
      if (r.fromDay == r.toDay) {  // one day: one line, none if it's a public holiday anyway
        if (r.fromDay < from || r.fromDay > to) continue;
        personal::Date d = dateOf(r.fromDay);
        if (holidays::holidayOn(d.y, d.m, d.d, state, nullptr)) continue;
        Highlight h;
        h.date = d;
        h.kind = Kind::SchoolDay;
        std::snprintf(h.text, sizeof(h.text), "%s", r.name);
        add(h);
        continue;
      }
      for (int edge = 0; edge < 2; edge++) {
        long dn = edge == 0 ? r.fromDay : r.toDay;
        if (dn < from || dn > to) continue;
        Highlight h;
        h.date = dateOf(dn);
        h.kind = edge == 0 ? Kind::SchoolStart : Kind::SchoolEnd;
        std::snprintf(h.text, sizeof(h.text), "%s", r.name);
        add(h);
      }
    }
  }
  for (int l = 0; l < listCount; l++) {
    if (!lists || !lists[l]) continue;
    for (int i = 0; i < lists[l]->count; i++) {
      const personal::Birthday& b = lists[l]->items[i];
      personal::Date next = personal::nextOccurrence(b, today);
      if (!next.valid() || personal::dayNumber(next) > to) continue;
      bool dup = false;
      for (int j = 0; j < n && !dup; j++)
        dup = out[j].kind == Kind::Birthday && out[j].date.m == next.m && out[j].date.d == next.d &&
              !std::strcmp(out[j].text, b.name);
      if (dup) continue;
      Highlight h;
      h.date = next;
      h.kind = Kind::Birthday;
      std::snprintf(h.text, sizeof(h.text), "%s", b.name);
      h.age = b.year && next.y > b.year ? next.y - b.year : 0;
      add(h);
    }
  }
  return n;
}

}  // namespace month
