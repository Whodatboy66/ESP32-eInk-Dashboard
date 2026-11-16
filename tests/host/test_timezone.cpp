// Host test for src/core/Timezone.h. Run: tests/host/run.sh
#include "src/core/Timezone.h"
#include "tests/host/check.h"

#include <cstdlib>
#include <ctime>

static const char* norm(const char* in) {
  static char out[72];
  return normalizeTimezone(in, out, sizeof(out)) ? out : "<invalid>";
}

static void test_names_become_posix_rules() {
  // A bare "CET" has no offset: the C library would treat it as UTC (1 h off in German winter).
  CHECK_STR(norm("CET"), "CET-1CEST,M3.5.0,M10.5.0/3");
  CHECK_STR(norm("cet"), "CET-1CEST,M3.5.0,M10.5.0/3");
  CHECK_STR(norm(" MEZ "), "CET-1CEST,M3.5.0,M10.5.0/3");
  CHECK_STR(norm("Europe/Berlin"), "CET-1CEST,M3.5.0,M10.5.0/3");
  CHECK_STR(norm("europe/vienna"), "CET-1CEST,M3.5.0,M10.5.0/3");
  CHECK_STR(norm("Europe/London"), "GMT0BST,M3.5.0/1,M10.5.0");
  CHECK_STR(norm("EET"), "EET-2EEST,M3.5.0/3,M10.5.0/4");
  CHECK_STR(norm("America/New_York"), "EST5EDT,M3.2.0,M11.1.0");
  CHECK_STR(norm("UTC"), "UTC0");
  CHECK_STR(norm("GMT"), "GMT0");
  CHECK_STR(norm(""), "UTC0");
}

static void test_posix_rules_are_kept() {
  CHECK_STR(norm("CET-1CEST,M3.5.0,M10.5.0/3"), "CET-1CEST,M3.5.0,M10.5.0/3");
  CHECK_STR(norm("EST5EDT,M3.2.0/2,M11.1.0/2"), "EST5EDT,M3.2.0/2,M11.1.0/2");
  CHECK_STR(norm("UTC0"), "UTC0");
  CHECK_STR(norm("<+0530>-5:30"), "<+0530>-5:30");
  CHECK_STR(norm("  JST-9  "), "JST-9");
}

static void test_unusable_strings_are_rejected() {
  CHECK_STR(norm("Berlin time"), "<invalid>");
  CHECK_STR(norm("XYZ"), "<invalid>");  // a name without offset that we don't know
  CHECK_STR(norm("12"), "<invalid>");
  CHECK_STR(norm("AB1"), "<invalid>");  // names need 3+ letters
}

static void test_result_gives_german_time() {
  // 2026-01-15 12:00 UTC is 13:00 in Germany (winter), 2026-07-15 12:00 UTC is 14:00 (summer).
  setenv("TZ", norm("CET"), 1);
  tzset();
  time_t winter = 1768478400, summer = 1784116800;
  struct tm t;
  localtime_r(&winter, &t);
  CHECK_EQ(t.tm_hour, 13);
  localtime_r(&summer, &t);
  CHECK_EQ(t.tm_hour, 14);
}

int main() {
  test_names_become_posix_rules();
  test_posix_rules_are_kept();
  test_unusable_strings_are_rejected();
  test_result_gives_german_time();
  return finish("timezone");
}
