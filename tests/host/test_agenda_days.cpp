// Host test for src/widgets/AgendaDays.h: the days "Heute und morgen" shows, in order, and which of
// them get the holiday line (build 79 review findings 2, 3 and 11). Run: tests/host/run.sh
#include "src/widgets/AgendaDays.h"
#include "tests/host/check.h"

using agenda::Day;

namespace {

constexpr long TODAY = 20819;  // a day number

void test_entry_from_yesterday_no_duplicate() {
  // A trip that started yesterday, then two entries today; today is a holiday.
  const long days[] = {TODAY - 1, TODAY, TODAY};
  Day out[6];
  int n = agenda::plan(days, 3, TODAY, true, false, out, 6);
  CHECK_EQ(n, 2);
  CHECK(out[0].key == TODAY - 1 && !out[0].holiday);
  CHECK(out[1].key == TODAY && out[1].holiday);
}

void test_holiday_without_entries() {
  Day out[6];
  int n = agenda::plan(nullptr, 0, TODAY, false, true, out, 6);
  CHECK_EQ(n, 1);
  CHECK(out[0].key == TODAY + 1 && out[0].holiday);
  // Entries only tomorrow, holiday today: today first.
  const long days[] = {TODAY + 1};
  n = agenda::plan(days, 1, TODAY, true, false, out, 6);
  CHECK_EQ(n, 2);
  CHECK(out[0].key == TODAY && out[0].holiday);
  CHECK(out[1].key == TODAY + 1 && !out[1].holiday);
}

void test_clock_not_set() {
  Day out[6];
  CHECK_EQ(agenda::plan(nullptr, 0, 0, true, true, out, 6), 0);  // no "Neujahr" on 1 January 1970
}

void test_cap() {
  const long days[] = {TODAY, TODAY + 1};
  Day out[1];
  CHECK_EQ(agenda::plan(days, 2, TODAY, true, true, out, 1), 1);
}

}  // namespace

int main() {
  test_entry_from_yesterday_no_duplicate();
  test_holiday_without_entries();
  test_clock_not_set();
  test_cap();
  return finish("agenda days");
}
