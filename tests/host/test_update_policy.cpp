// Host test for src/update/UpdatePolicy.h. Run: tests/host/run.sh
#include "src/update/UpdatePolicy.h"
#include "tests/host/check.h"

using namespace ota;

static void test_availability() {
  // remote build, min build, file for this board, current build
  CHECK(availability(65, 0, true, 64) == Availability::Newer);
  CHECK(availability(64, 0, true, 64) == Availability::UpToDate);
  CHECK(availability(63, 0, true, 64) == Availability::UpToDate);  // never a downgrade
  CHECK(availability(65, 0, false, 64) == Availability::NotForThisBoard);
  CHECK(availability(64, 0, false, 64) == Availability::UpToDate);  // nothing newer anyway
  CHECK(availability(70, 66, true, 64) == Availability::NeedsNewerFirmware);
  CHECK(availability(70, 64, true, 64) == Availability::Newer);    // min_build is inclusive
}

static void test_test_builds() {
  // Test builds (90001-90005) count as newer than every release ...
  CHECK(isTestBuild(90005));
  CHECK(!isTestBuild(69));
  CHECK(availability(90005, 0, true, 69) == Availability::Newer);
  // ... and a device left on one gets back to any regular release (restore, the next release).
  CHECK(availability(66, 0, true, 90005) == Availability::Newer);
  CHECK(availability(69, 0, true, 90004) == Availability::Newer);
  CHECK(availability(69, 0, false, 90005) == Availability::NotForThisBoard);
  CHECK(availability(69, 70, true, 90005) == Availability::Newer);  // a test build is past any min_build
  // Between test builds the normal rule applies.
  CHECK(availability(90005, 0, true, 90005) == Availability::UpToDate);
  CHECK(availability(90004, 0, true, 90005) == Availability::UpToDate);
  CHECK(availability(0, 0, true, 90005) == Availability::UpToDate);  // no real build number
}

static void test_battery_guard() {
  // battery sensor, percent, USB, minimum
  CHECK(batteryAllowsInstall(true, 30, false, 30));
  CHECK(!batteryAllowsInstall(true, 29, false, 30));
  CHECK(batteryAllowsInstall(true, 5, true, 30));     // USB connected
  CHECK(batteryAllowsInstall(false, 0, false, 30));   // no battery sensor: powered by USB
}

static void test_night_check_due() {
  // hour, today, last check day, night hour
  CHECK(nightCheckDue(3, dayKey(2026, 320), 0, 3));
  CHECK(!nightCheckDue(3, dayKey(2026, 320), dayKey(2026, 320), 3));  // once per night
  CHECK(nightCheckDue(3, dayKey(2026, 321), dayKey(2026, 320), 3));
  CHECK(!nightCheckDue(2, dayKey(2026, 321), dayKey(2026, 320), 3));
  CHECK(!nightCheckDue(4, dayKey(2026, 321), dayKey(2026, 320), 3));
  CHECK(nightCheckDue(3, dayKey(2027, 0), dayKey(2026, 364), 3));     // new year
}

static void test_night_action_skips_a_build_that_rolled_back() {
  // A release that failed after its restart is not auto-installed again (no reinstall loop);
  // it stays marked as available, a double tap can still install it.
  CHECK(nightAction(Availability::Newer, true, true, /*failedBefore=*/true) == NightAction::NotifyOnly);
  CHECK(nightAction(Availability::Newer, true, true, false) == NightAction::Install);
  CHECK(failedBefore(65, 65));
  CHECK(!failedBefore(66, 65));  // the next build is the fix
  CHECK(!failedBefore(65, 0));
  // Only that exact build: a failed test build 90004 must not block the real builds below it.
  CHECK(!failedBefore(67, 90004));
}

static void test_night_action() {
  CHECK(nightAction(Availability::Newer, true, true) == NightAction::Install);
  CHECK(nightAction(Availability::Newer, true, false) == NightAction::WaitForCharge);
  CHECK(nightAction(Availability::Newer, false, true) == NightAction::NotifyOnly);
  CHECK(nightAction(Availability::Newer, false, false) == NightAction::NotifyOnly);
  CHECK(nightAction(Availability::UpToDate, true, true) == NightAction::Nothing);
  CHECK(nightAction(Availability::NotForThisBoard, true, true) == NightAction::Nothing);
  CHECK(nightAction(Availability::NeedsNewerFirmware, true, true) == NightAction::Nothing);
}

static void test_safe_names_and_urls() {
  CHECK(isSafeName("ESP32C6-FireBeetle.bin"));
  CHECK(isSafeName("firmware-.65"));
  CHECK(!isSafeName(""));
  CHECK(!isSafeName("../evil.bin"));
  CHECK(!isSafeName("a/b.bin"));
  CHECK(!isSafeName("a b.bin"));
  CHECK(!isSafeName("x?y=1"));

  char url[160];
  CHECK(buildAssetUrl(url, sizeof(url), "Owner/Repo", "firmware-.65", "C6.bin"));
  CHECK_STR(url, "https://github.com/Owner/Repo/releases/download/firmware-.65/C6.bin");
  CHECK(!buildAssetUrl(url, sizeof(url), "Owner/Repo", "../x", "C6.bin"));
  CHECK(!buildAssetUrl(url, sizeof(url), "Owner/Repo", "t", "a/b"));
  CHECK(!buildAssetUrl(url, 20, "Owner/Repo", "firmware-.65", "C6.bin"));  // too small
}

static void test_allowed_hosts() {
  CHECK(isAllowedHost("github.com"));
  CHECK(isAllowedHost("release-assets.githubusercontent.com"));
  CHECK(isAllowedHost("objects.githubusercontent.com"));
  CHECK(!isAllowedHost("githubusercontent.com.evil.net"));
  CHECK(!isAllowedHost("evilgithub.com"));
  CHECK(!isAllowedHost("evil-githubusercontent.com"));
  CHECK(!isAllowedHost(""));
  CHECK(!isAllowedHost(nullptr));
}

static void test_channel_urls() {
  char url[160];
  CHECK(manifestUrl(url, sizeof(url), "Owner/Repo", false, "manifest.json"));
  CHECK_STR(url, "https://github.com/Owner/Repo/releases/latest/download/manifest.json");
  CHECK(manifestUrl(url, sizeof(url), "Owner/Repo", true, "manifest.sig"));
  CHECK_STR(url, "https://github.com/Owner/Repo/releases/download/beta/manifest.sig");
  CHECK(!manifestUrl(url, 30, "Owner/Repo", true, "manifest.sig"));
  CHECK_STR(channelName(false), "stable");
  CHECK_STR(channelName(true), "beta");
}

static void test_sha256_hex() {
  CHECK(isSha256Hex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
  CHECK(!isSha256Hex("0123456789ABCDEF0123456789abcdef0123456789abcdef0123456789abcdef"));
  CHECK(!isSha256Hex("0123"));
  CHECK(!isSha256Hex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdeg"));

  const uint8_t digest[32] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
  char hex[65];
  toHex(digest, sizeof(digest), hex, sizeof(hex));
  CHECK_STR(hex, "0123456789abcdef000000000000000000000000000000000000000000000000");
}

static void test_sanitize_note() {
  char out[72];
  sanitizeNote("Fixes (calendar) & weather!", out, sizeof(out));
  CHECK_STR(out, "Fixes calendar + weather.");
  sanitizeNote("  leading spaces ", out, sizeof(out));
  CHECK_STR(out, "leading spaces");
  sanitizeNote("It's \"quoted\"; ok", out, sizeof(out));
  CHECK_STR(out, "Its quoted, ok");
  sanitizeNote("Line\nbreak\ttab", out, sizeof(out));
  CHECK_STR(out, "Line break tab");

  char small[8];
  sanitizeNote("abcdefghijk", small, sizeof(small));
  CHECK_STR(small, "abcdefg");
  // Truncation never cuts a UTF-8 character in half ("é" = C3 A9).
  sanitizeNote("abcdef\xC3\xA9xyz", small, sizeof(small));
  CHECK_STR(small, "abcdef");
}

static void test_progress_steps() {
  // Progress bar redraws at 0, 25, 50, 75, 100 %.
  CHECK_EQ(progressStep(0, 1000), 0);
  CHECK_EQ(progressStep(249, 1000), 0);
  CHECK_EQ(progressStep(250, 1000), 1);
  CHECK_EQ(progressStep(999, 1000), 3);
  CHECK_EQ(progressStep(1000, 1000), 4);
  CHECK_EQ(progressStep(5, 0), 0);
}

int main() {
  test_availability();
  test_test_builds();
  test_battery_guard();
  test_night_check_due();
  test_night_action();
  test_night_action_skips_a_build_that_rolled_back();
  test_safe_names_and_urls();
  test_allowed_hosts();
  test_channel_urls();
  test_sha256_hex();
  test_sanitize_note();
  test_progress_steps();
  return finish("update policy");
}
