// Host test for src/core/DataStore.h (stage 8): the data kept in sleep memory between wakes and when
// a service is due again. Run: tests/host/run.sh
#include "src/core/DataStore.h"
#include "src/services/PrinterReport.h"
#include "src/services/ServiceData.h"
#include "tests/host/check.h"

#include <ctime>

namespace {

// Weather, stocks and the printer state move into RTC memory (8 KB on the classic ESP32, about 3.3 KB
// used before stage 8): their share stays within the store's budget.
void test_store_sizes() {
  const size_t bytes = sizeof(WeatherData) + sizeof(StockItem) * MAX_STOCKS + sizeof(int) + sizeof(bambu::PrinterStatus);
  std::printf("  store: weather %zu + stocks %zu + printer %zu = %zu B (budget %zu)\n", sizeof(WeatherData),
              sizeof(StockItem) * MAX_STOCKS, sizeof(bambu::PrinterStatus), bytes, datastore::STORE_BYTES_MAX);
  CHECK(bytes <= datastore::STORE_BYTES_MAX);
}

using datastore::dueByAge;
using datastore::effectiveInterval;
const time_t NOW = 1789000000;  // a real time (2026-09-10)

// "The clock interval is the upper bound for everything": rounded up to a multiple of it.
void test_effective_interval() {
  CHECK_EQ(effectiveInterval(30, 1), 30);
  CHECK_EQ(effectiveInterval(15, 30), 30);  // never more often than the clock
  CHECK_EQ(effectiveInterval(60, 15), 60);
  CHECK_EQ(effectiveInterval(60, 45), 90);
  for (int choice : datastore::INTERVAL_CHOICES) CHECK_EQ(1440 % choice, 0);  // a day holds whole intervals
}

void test_due_by_age() {
  CHECK(dueByAge(NOW - (29 * 60 + 31), NOW, 30, 1, false));  // within half a clock step of 30 min
  CHECK(!dueByAge(NOW - 29 * 60, NOW, 30, 1, false));
  CHECK(dueByAge(0, NOW, 30, 1, false));                     // never fetched
  // A 30-minute clock: the last download ended 40 s into its wake; the next wake 30 min later is due
  // (a 30 s tolerance would push it to the wake after, 30 min late).
  CHECK(dueByAge(NOW - (29 * 60 + 20), NOW, 30, 30, false));
}

// Time-based intervals mustn't split the hourly Wi-Fi session into several: a service due within the
// last quarter of its interval comes along when Wi-Fi is on anyway.
void test_ride_along() {
  CHECK(!dueByAge(NOW - 23 * 60, NOW, 30, 1, false));
  CHECK(dueByAge(NOW - 23 * 60, NOW, 30, 1, true));
  CHECK(!dueByAge(NOW - 20 * 60, NOW, 30, 1, true));
}

// The settings accept only the choices (the app offers them; the API checks).
void test_valid_interval() {
  for (int m : {15, 30, 60, 120, 180, 360}) CHECK(datastore::validInterval(m));
  for (int m : {0, 10, 45, 90, 720}) CHECK(!datastore::validInterval(m));
  CHECK_EQ(datastore::intervalOr(45, 60), 60);  // a stored value that isn't a choice: the default
  CHECK_EQ(datastore::intervalOr(180, 60), 180);
}

// The clock not set yet (after a reset, before NTP): due, no arithmetic on a nonsense time.
void test_due_without_clock() {
  CHECK(dueByAge(NOW - 60, 1000, 30, 1, false));
  CHECK(dueByAge(NOW + 3600, NOW, 30, 1, false));  // clock moved back: fetch, so lastOk catches up
}

}  // namespace

int main() {
  test_store_sizes();
  test_effective_interval();
  test_due_by_age();
  test_ride_along();
  test_due_without_clock();
  test_valid_interval();
  return finish("data store");
}
