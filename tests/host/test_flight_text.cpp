// Host test for src/widgets/FlightText.h (stage 7b): flights in calendar titles and the live status
// line under a flight. Run: tests/host/run.sh
#include <cstdlib>
#include <cstring>
#include <string>

#include "src/widgets/FlightText.h"
#include "tests/host/check.h"

using i18n::Lang;
using i18n::S;

namespace {

void test_titles() {
  for (const char* t : {"TS399 PUJ->YUL", "AC 871 YUL to YYZ", "LH 400 FRA- JFK", "Flight to Toronto (AC 871)",
                        "\xE2\x9C\x88 Berlin", "EW 7 CGN\xE2\x86\x92PMI", "U2 1234 BER/LGW"})
    CHECK(flighttext::isFlightTitle(t));
  for (const char* t : {"Meeting ABC to DEF", "BMW X5 Service", "Zahnarzt 2026", "", "AB 12345 FRA-JFK", "12 345 FRA-JFK"})
    CHECK(!flighttext::isFlightTitle(t));
  CHECK(!flighttext::isFlightTitle(nullptr));
}

void test_numbers() {
  char n[12];
  CHECK(flighttext::extractFlightNumber("Flight to Toronto (AC 871)", n, sizeof(n)));
  CHECK_STR(n, "AC871");
  CHECK(flighttext::extractFlightNumber("TS399 PUJ->YUL", n, sizeof(n)));
  CHECK_STR(n, "TS399");
  CHECK(flighttext::extractFlightNumber("U2 1234 BER/LGW", n, sizeof(n)));
  CHECK_STR(n, "U21234");
  CHECK(!flighttext::extractFlightNumber("Arzt um 9", n, sizeof(n)));
  CHECK(!flighttext::extractFlightNumber(nullptr, n, sizeof(n)));
}

std::string fmt(S key, Lang l, const char* arg = nullptr) {
  char b[96];
  if (arg) std::snprintf(b, sizeof(b), i18n::text(key, l), arg);
  else std::snprintf(b, sizeof(b), "%s", i18n::text(key, l));
  return b;
}

// CHECK_STR keeps a pointer to the expected text: hold it in a string for the whole check.
void expectText(const char* actual, const std::string& expected) { CHECK_STR(actual, expected.c_str()); }

void test_live_line() {
  LiveFlight f{};
  f.depScheduled = 1795000000;
  f.depActual = f.depScheduled + 45 * 60;
  f.arrScheduled = 1795030000;
  std::snprintf(f.status, sizeof(f.status), "Delayed");
  std::snprintf(f.depGate, sizeof(f.depGate), "B7");
  std::snprintf(f.depTerminal, sizeof(f.depTerminal), "2");
  char b[96];
  flighttext::liveStatus(f, Lang::En, b, sizeof(b));
  expectText(b, (fmt(S::FlightDelayed, Lang::En) + " +45m" + fmt(S::FlightGate, Lang::En, "B7") + ", T2"));
  std::snprintf(f.depTerminal, sizeof(f.depTerminal), "B");  // a terminal with a letter: written out
  flighttext::liveStatus(f, Lang::De, b, sizeof(b));
  expectText(b, fmt(S::FlightDelayed, Lang::De) + " +45m" + fmt(S::FlightGate, Lang::De, "B7") +
                    fmt(S::FlightTerminal, Lang::De, "B"));
  // On time ("Expected") with less than 5 minutes difference: no delay.
  f.depActual = f.depScheduled + 120;
  f.depGate[0] = f.depTerminal[0] = '\0';
  std::snprintf(f.status, sizeof(f.status), "Expected");
  flighttext::liveStatus(f, Lang::En, b, sizeof(b));
  expectText(b, fmt(S::FlightOnTime, Lang::En));
  // Landed: with the arrival time (local), cancelled: only the word.
  std::snprintf(f.status, sizeof(f.status), "Arrived");
  f.arrActual = 1795031520;  // 2026-11-18 19:52 UTC = 20:52 in Germany
  flighttext::liveStatus(f, Lang::En, b, sizeof(b));
  expectText(b, fmt(S::FlightLanded, Lang::En, "20:52"));
  std::snprintf(f.status, sizeof(f.status), "Canceled");
  flighttext::liveStatus(f, Lang::De, b, sizeof(b));
  expectText(b, fmt(S::FlightCanceled, Lang::De));
  std::snprintf(f.status, sizeof(f.status), "Something new");  // unknown: "scheduled"
  flighttext::liveStatus(f, Lang::En, b, sizeof(b));
  expectText(b, fmt(S::FlightScheduled, Lang::En));
}

}  // namespace

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  test_titles();
  test_numbers();
  test_live_line();
  return finish("flight text");
}
