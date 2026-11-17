// Host test for src/core/Redact.h. Run: tests/host/run.sh
#include "src/core/Redact.h"
#include "tests/host/check.h"

#include <string>

static std::string red(const char* line, const char* const* secrets, size_t n) {
  char buf[256];
  std::snprintf(buf, sizeof(buf), "%s", line);
  redactSecrets(buf, sizeof(buf), secrets, n);
  return buf;
}

// Keeps the result alive while it is compared (red() returns a temporary).
#define CHECK_RED(line, secrets, n, want)          \
  do {                                             \
    std::string r_ = red(line, secrets, n);        \
    CHECK_STR(r_.c_str(), want);                   \
  } while (0)

static void test_masks_every_occurrence() {
  const char* s[] = {"hunter22"};
  CHECK_RED("pass=hunter22 again hunter22.", s, 1, "pass=*** again ***.");
}

static void test_short_or_empty_secrets_are_ignored() {
  const char* s[] = {"", "abc", nullptr};
  CHECK_RED("abc is fine", s, 3, "abc is fine");
}

static void test_several_secrets() {
  const char* s[] = {"wifiPassword1", "AKIA-12345"};
  CHECK_RED("wifi wifiPassword1 key AKIA-12345", s, 2, "wifi *** key ***");
}

static void test_ics_url_and_its_path() {
  const char* s[] = {"https://calendar.google.com/calendar/ical/me%40gmail.com/private-0a1b2c/basic.ics"};
  CHECK_RED("GET https://calendar.google.com/calendar/ical/me%40gmail.com/private-0a1b2c/basic.ics", s, 1, "GET ***");
  // Only the path logged (e.g. by a redirect message): still masked; the host stays readable.
  CHECK_RED("Location: /calendar/ical/me%40gmail.com/private-0a1b2c/basic.ics", s, 1, "Location: ***");
  CHECK_RED("[ICS] HTTP 404 for calendar.google.com", s, 1, "[ICS] HTTP 404 for calendar.google.com");
}

static void test_overlapping_secrets_longest_first() {
  const char* s[] = {"secret", "secret-extended"};
  CHECK_RED("x secret-extended y secret", s, 2, "x *** y ***");
}

static void test_line_full_to_capacity() {
  const char* s[] = {"abcd"};
  char buf[9] = "xxabcdxx";
  redactSecrets(buf, sizeof(buf), s, 1);
  CHECK_STR(buf, "xx***xx");
}

int main() {
  test_masks_every_occurrence();
  test_short_or_empty_secrets_are_ignored();
  test_several_secrets();
  test_ics_url_and_its_path();
  test_overlapping_secrets_longest_first();
  test_line_full_to_capacity();
  return finish("redact");
}
