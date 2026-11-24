// Host test for src/core/Holidays.h (stage 7): Easter, the national holidays, the rules of the 16
// states, ISO week numbers. Run: tests/host/run.sh
#include <initializer_list>

#include "src/core/Holidays.h"
#include "tests/host/check.h"

using namespace holidays;

namespace {

const State ALL[] = {State::BW, State::BY, State::BE, State::BB, State::HB, State::HH, State::HE, State::MV,
                     State::NI, State::NW, State::RP, State::SL, State::SN, State::ST, State::SH, State::TH};

bool on(int y, int m, int d, State s) { return holidayOn(y, m, d, s, nullptr); }

// The states that have a holiday on that day, as a bit set over ALL (bit i = ALL[i]).
unsigned statesWith(int y, int m, int d) {
  unsigned bits = 0;
  for (int i = 0; i < 16; i++)
    if (on(y, m, d, ALL[i])) bits |= 1u << i;
  return bits;
}

unsigned bitsOf(std::initializer_list<State> list) {
  unsigned bits = 0;
  for (State s : list)
    for (int i = 0; i < 16; i++)
      if (ALL[i] == s) bits |= 1u << i;
  return bits;
}

void test_easter() {
  int m, d;
  easter(2024, m, d);
  CHECK(m == 3 && d == 31);
  easter(2025, m, d);
  CHECK(m == 4 && d == 20);
  easter(2026, m, d);
  CHECK(m == 4 && d == 5);
  easter(2027, m, d);
  CHECK(m == 3 && d == 28);
  easter(2030, m, d);
  CHECK(m == 4 && d == 21);
}

void test_national_2026() {
  const int days[][2] = {{1, 1}, {4, 3}, {4, 6}, {5, 1}, {5, 14}, {5, 25}, {10, 3}, {12, 25}, {12, 26}};
  for (const auto& md : days) {
    CHECK(statesWith(2026, md[0], md[1]) == 0xFFFFu);
  }
  Holiday h;
  CHECK(holidayOn(2026, 10, 3, State::BY, &h));
  CHECK(h.name == i18n::S::HolUnity);
  CHECK(!on(2026, 11, 23, State::BY));
  CHECK(!on(2026, 1, 1, State::None));
}

void test_state_rules() {
  CHECK(statesWith(2026, 1, 6) == bitsOf({State::BW, State::BY, State::ST}));
  CHECK(statesWith(2026, 3, 8) == bitsOf({State::BE, State::MV}));
  CHECK(statesWith(2026, 6, 4) == bitsOf({State::BW, State::BY, State::HE, State::NW, State::RP, State::SL}));  // Fronleichnam
  CHECK(statesWith(2026, 8, 15) == bitsOf({State::BY, State::SL}));
  CHECK(statesWith(2026, 9, 20) == bitsOf({State::TH}));
  CHECK(statesWith(2026, 10, 31) ==
        bitsOf({State::BB, State::HB, State::HH, State::MV, State::NI, State::SN, State::ST, State::SH, State::TH}));
  CHECK(statesWith(2026, 11, 1) == bitsOf({State::BW, State::BY, State::NW, State::RP, State::SL}));
  CHECK(statesWith(2026, 11, 18) == bitsOf({State::SN}));  // Buß- und Bettag
  CHECK(statesWith(2027, 11, 17) == bitsOf({State::SN}));
  CHECK(statesWith(2026, 4, 5) == bitsOf({State::BB}));    // Ostersonntag
  CHECK(statesWith(2026, 5, 24) == bitsOf({State::BB}));   // Pfingstsonntag
}

void test_year_list() {
  Holiday list[24];
  int n = holidaysOfYear(2026, State::BY, list, 24);
  CHECK_EQ(n, 13);  // 9 national + 6.1, Fronleichnam, 15.8, 1.11
  for (int i = 1; i < n; i++) CHECK(list[i - 1].month * 100 + list[i - 1].day < list[i].month * 100 + list[i].day);
  CHECK(list[0].month == 1 && list[0].day == 1 && list[0].year == 2026);
  CHECK_EQ(holidaysOfYear(2026, State::BY, list, 3), 3);  // cap respected
  CHECK_EQ(holidaysOfYear(2026, State::None, list, 24), 0);
}

void test_iso_week() {
  CHECK_EQ(isoWeek(2026, 11, 23), 48);
  CHECK_EQ(isoWeek(2024, 12, 30), 1);
  CHECK_EQ(isoWeek(2026, 1, 1), 1);
}

void test_iso_week_year_end() {
  CHECK_EQ(isoWeek(2027, 1, 1), 53);
  CHECK_EQ(isoWeek(2026, 12, 31), 53);
  CHECK_EQ(isoWeek(2027, 1, 4), 1);
  CHECK_EQ(isoWeek(2027, 1, 3), 53);
}

}  // namespace

int main() {
  test_easter();
  test_national_2026();
  test_state_rules();
  test_year_list();
  test_iso_week();
  test_iso_week_year_end();
  return finish("holidays");
}
