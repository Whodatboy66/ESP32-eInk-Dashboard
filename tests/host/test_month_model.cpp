// Host test for src/widgets/MonthModel.h (stage 7): the month view's design per box size, which
// day gets which mark, and the footer's highlights. Run: tests/host/run.sh
#include <cstring>

#include "src/widgets/MonthModel.h"
#include "tests/host/check.h"

using namespace month;
using holidays::State;
using personal::Date;

namespace {

school::SchoolList christmasBreak() {
  school::SchoolList l;
  l.count = 1;
  l.r[0].fromDay = personal::dayNumber(Date{2026, 12, 23});
  l.r[0].toDay = personal::dayNumber(Date{2027, 1, 9});
  std::snprintf(l.r[0].name, sizeof(l.r[0].name), "Weihnachtsferien");
  return l;
}

personal::BirthdayList annaList() {
  personal::BirthdayList b;
  b.count = 1;
  std::snprintf(b.items[0].name, sizeof(b.items[0].name), "Anna");
  b.items[0].day = 17;
  b.items[0].month = 1;
  b.items[0].year = 1967;
  return b;
}

bool has(uint32_t bits, int day) { return (bits >> day) & 1u; }

void test_layout_sizes() {
  Geometry g = layoutFor(800, 452);
  CHECK(g.design == Design::Wide);
  CHECK(g.kw);
  CHECK_EQ(g.sideW, 240);
  CHECK_EQ(g.cellW, 70);
  CHECK_EQ(g.cellH, 67);
  CHECK(g.bigDigits);

  g = layoutFor(400, 452);
  CHECK(g.design == Design::Full);
  CHECK(g.kw);
  CHECK_EQ(g.footerLines, 3);
  CHECK_EQ(g.cellW, 50);
  CHECK_EQ(g.cellH, 57);

  g = layoutFor(400, 300);
  CHECK(g.design == Design::Full);
  CHECK_EQ(g.footerLines, 2);
  CHECK_EQ(g.cellH, 35);
  CHECK(g.markRow);

  g = layoutFor(360, 230);  // Classic's month place
  CHECK(g.design == Design::Full);
  CHECK_EQ(g.footerLines, 0);
  CHECK_EQ(g.cellH, 30);
  CHECK(!g.markRow);

  g = layoutFor(200, 300);
  CHECK(g.design == Design::Compact);
  CHECK(!g.kw);
  CHECK_EQ(g.cols, 7);
  CHECK_EQ(g.cellW, 28);
  CHECK_EQ(g.cellH, 42);
  CHECK(g.markRow);

  g = layoutFor(400, 150);
  CHECK(g.design == Design::TwoWeeks);
  CHECK(g.kw);
  CHECK_EQ(g.footerLines, 1);
  CHECK_EQ(g.cellH, 42);

  g = layoutFor(800, 150);
  CHECK(g.design == Design::TwoWeeks);
  CHECK_EQ(g.sideW, 240);

  CHECK(layoutFor(200, 150).design == Design::Sheet);
}

void test_marks() {
  personal::BirthdayList anna = annaList();
  const personal::BirthdayList* lists[] = {&anna};
  Marks m = marksFor(2027, 1, 13, State::BY, nullptr, (1u << 20) | (1u << 21), lists, 1);
  CHECK(has(m.holiday, 1));
  CHECK(has(m.holiday, 6));
  CHECK(!has(m.holiday, 2));
  CHECK(has(m.birthday, 17));
  CHECK(has(m.appointment, 20) && has(m.appointment, 21) && !has(m.appointment, 13));
  CHECK_EQ(m.today, 13);
  CHECK_EQ(m.school, 0u);
}

void test_school_across_year() {
  school::SchoolList l = christmasBreak();
  Marks jan = marksFor(2027, 1, 13, State::BY, &l, 0, nullptr, 0);
  for (int d = 1; d <= 9; d++) CHECK(has(jan.school, d));
  CHECK(!has(jan.school, 10));
  Marks dec = marksFor(2026, 12, 13, State::BY, &l, 0, nullptr, 0);
  CHECK(!has(dec.school, 22));
  for (int d = 23; d <= 31; d++) CHECK(has(dec.school, d));
}

void test_marks_without_date() {
  school::SchoolList l = christmasBreak();
  personal::BirthdayList anna = annaList();
  const personal::BirthdayList* lists[] = {&anna};
  Marks m = marksFor(2027, 1, 0, State::BY, &l, 1u << 5, lists, 1);  // clock not set
  CHECK_EQ(m.today, 0);
  CHECK_EQ(m.holiday, 0u);
  CHECK_EQ(m.school, 0u);
  CHECK_EQ(m.appointment, 0u);
  CHECK_EQ(m.birthday, 0u);
  Highlight h[4];
  CHECK_EQ(highlights(Date{}, State::BY, &l, lists, 1, h, 4), 0);
}

void test_footer_order() {
  school::SchoolList l = christmasBreak();
  personal::BirthdayList anna = annaList();
  const personal::BirthdayList* lists[] = {&anna, &anna};  // the same person in two lists: once
  Highlight h[8];
  int n = highlights(Date{2027, 1, 4}, State::BY, &l, lists, 2, h, 8);
  CHECK_EQ(n, 3);
  CHECK(h[0].kind == Kind::Holiday && h[0].date.d == 6 && h[0].holiday == i18n::S::HolEpiphany);
  CHECK(h[1].kind == Kind::SchoolEnd && h[1].date.d == 9);
  CHECK_STR(h[1].text, "Weihnachtsferien");
  CHECK(h[2].kind == Kind::Birthday && h[2].date.d == 17 && h[2].age == 60);
  CHECK_STR(h[2].text, "Anna");
  CHECK_EQ(highlights(Date{2027, 1, 4}, State::BY, &l, lists, 2, h, 2), 2);  // cap respected
}

void test_footer_next_year() {
  Highlight h[3];
  int n = highlights(Date{2026, 12, 28}, State::BY, nullptr, nullptr, 0, h, 3);
  CHECK_EQ(n, 2);  // Neujahr and Heilige Drei Könige 2027
  CHECK(h[0].holiday == i18n::S::HolNewYear && h[0].date.y == 2027);
  // Before the start of the school holidays: the start is a highlight.
  school::SchoolList l = christmasBreak();
  n = highlights(Date{2026, 12, 20}, State::None, &l, nullptr, 0, h, 3);
  CHECK_EQ(n, 2);
  CHECK(h[0].kind == Kind::SchoolStart && h[0].date.d == 23);
  CHECK(h[1].kind == Kind::SchoolEnd && h[1].date.y == 2027);
}

// Review finding 5: a one-day school holiday is one footer line; on a public holiday of the same
// day (Buß- und Bettag in Sachsen) it isn't listed again.
void test_one_day_school_holiday() {
  school::SchoolList l;
  l.count = 1;
  l.r[0].fromDay = l.r[0].toDay = personal::dayNumber(Date{2026, 11, 18});
  std::snprintf(l.r[0].name, sizeof(l.r[0].name), "Buß- und Bettag");
  Highlight h[8];
  auto on18 = [&](int n, Kind& kind) {  // entries on 18 November (Christmas etc. come later)
    int c = 0;
    for (int i = 0; i < n; i++)
      if (h[i].date.m == 11 && h[i].date.d == 18) {
        c++;
        kind = h[i].kind;
      }
    return c;
  };
  Kind k = Kind::Birthday;
  CHECK_EQ(on18(highlights(Date{2026, 11, 16}, State::BY, &l, nullptr, 0, h, 8), k), 1);
  CHECK(k == Kind::SchoolDay);
  CHECK_EQ(on18(highlights(Date{2026, 11, 16}, State::SN, &l, nullptr, 0, h, 8), k), 1);
  CHECK(k == Kind::Holiday);
}

}  // namespace

int main() {
  test_layout_sizes();
  test_marks();
  test_school_across_year();
  test_marks_without_date();
  test_footer_order();
  test_footer_next_year();
  test_one_day_school_holiday();
  return finish("month model");
}
