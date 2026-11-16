// Host test for src/input/ButtonGesture.h (no Arduino headers needed).
// Run: tests/host/run.sh
#include "src/input/ButtonGesture.h"

#include <cstdio>
#include <cstdlib>

static int failures = 0;

#define CHECK_EQ(actual, expected)                                                        \
  do {                                                                                    \
    auto a_ = (actual);                                                                   \
    auto e_ = (expected);                                                                 \
    if (!(a_ == e_)) {                                                                    \
      std::printf("%s:%d: CHECK_EQ(%s, %s) failed: %d != %d\n", __FILE__, __LINE__,      \
                  #actual, #expected, static_cast<int>(a_), static_cast<int>(e_));       \
      ++failures;                                                                         \
    }                                                                                     \
  } while (0)

static const ButtonTiming kTiming{2000, 4000, 15000, 60};

// Feeds a constant level from `from` to `to` (inclusive) in 10 ms steps, returns the first event.
static ButtonEvent feed(ButtonGesture& g, bool high, uint32_t from, uint32_t to) {
  // Counts the offset instead of comparing times, so a range across the millis() wrap works.
  for (uint32_t d = 0; d <= to - from; d += 10) {
    ButtonEvent e = g.update(high, from + d);
    if (e != ButtonEvent::None) return e;
  }
  return ButtonEvent::None;
}

// Press from `start`, hold for `heldMs`, then release; returns the first event.
static ButtonEvent press(ButtonGesture& g, uint32_t start, uint32_t heldMs) {
  ButtonEvent e = feed(g, true, start, start + heldMs - 10);
  if (e != ButtonEvent::None) return e;
  return feed(g, false, start + heldMs, start + heldMs + 200);
}

static void test_idle_low_gives_nothing() {
  ButtonGesture g(kTiming);
  CHECK_EQ(feed(g, false, 0, 30000), ButtonEvent::None);
  CHECK_EQ(g.isPressed(), false);
}

static void test_short_tap() {
  ButtonGesture g(kTiming);
  CHECK_EQ(press(g, 1000, 300), ButtonEvent::Tap);
}

static void test_reboot_between_2_and_4_s() {
  ButtonGesture g(kTiming);
  CHECK_EQ(press(g, 1000, 2500), ButtonEvent::Reboot);
}

static void test_setup_on_release_between_4_and_15_s() {
  ButtonGesture g(kTiming);
  // Nothing fires while still held at 6 s: setup is decided on release.
  CHECK_EQ(feed(g, true, 1000, 7000), ButtonEvent::None);
  CHECK_EQ(feed(g, false, 7010, 7300), ButtonEvent::Setup);
}

static void test_held_longer_than_fault_limit_is_fault_not_gesture() {
  ButtonGesture g(kTiming);
  CHECK_EQ(feed(g, true, 0, 20000), ButtonEvent::Fault);
  CHECK_EQ(g.isFaulted(), true);
  // Still high: no further events, and the release is not counted as a gesture.
  CHECK_EQ(feed(g, true, 20010, 40000), ButtonEvent::None);
  CHECK_EQ(feed(g, false, 40010, 41000), ButtonEvent::None);
  CHECK_EQ(g.isFaulted(), false);
  // Works normally again after the release.
  CHECK_EQ(press(g, 50000, 300), ButtonEvent::Tap);
}

static void test_fault_fires_exactly_at_limit() {
  ButtonGesture g(kTiming);
  CHECK_EQ(feed(g, true, 0, 14990), ButtonEvent::None);
  CHECK_EQ(g.update(true, 15000), ButtonEvent::Fault);
}

static void test_bounce_during_hold_does_not_release() {
  ButtonGesture g(kTiming);
  CHECK_EQ(feed(g, true, 0, 1000), ButtonEvent::None);
  CHECK_EQ(feed(g, false, 1010, 1040), ButtonEvent::None);  // 40 ms low < 60 ms debounce
  CHECK_EQ(feed(g, true, 1050, 2990), ButtonEvent::None);
  // Released at 3000: counted from the press start, so this is a reboot, not a tap.
  CHECK_EQ(feed(g, false, 3000, 3200), ButtonEvent::Reboot);
}

static void test_release_time_is_when_the_level_went_low() {
  // Held 3.99 s: the debounce delay must not push it over the 4 s setup threshold.
  ButtonGesture g(kTiming);
  CHECK_EQ(press(g, 0, 3990), ButtonEvent::Reboot);
}

static void test_press_that_began_before_tracking() {
  // After a wake from deep sleep the press started at boot (t = 0), before the firmware ran.
  ButtonGesture g(kTiming);
  g.beginPress(0);
  CHECK_EQ(g.isPressed(), true);
  CHECK_EQ(g.heldMs(1500), 1500u);
  CHECK_EQ(feed(g, true, 1500, 2490), ButtonEvent::None);
  CHECK_EQ(feed(g, false, 2500, 2700), ButtonEvent::Reboot);
}

static void test_stays_faulted_when_starting_faulted_and_high() {
  // Fault remembered across deep sleep: a sensor still stuck high must not start a new press.
  ButtonGesture g(kTiming, /*faulted=*/true);
  CHECK_EQ(feed(g, true, 0, 30000), ButtonEvent::None);
  CHECK_EQ(g.isFaulted(), true);
  CHECK_EQ(g.isPressed(), false);
}

static void test_fault_clears_on_first_low_poll() {
  ButtonGesture g(kTiming, /*faulted=*/true);
  CHECK_EQ(g.update(false, 0), ButtonEvent::None);
  CHECK_EQ(g.isFaulted(), false);
}

static void test_held_ms_is_zero_when_not_pressed() {
  ButtonGesture g(kTiming);
  CHECK_EQ(g.heldMs(5000), 0u);
}

static void test_millis_wraparound() {
  ButtonGesture g(kTiming);
  const uint32_t start = 0xFFFFFFFFu - 1000u;
  CHECK_EQ(feed(g, true, start, start + 2490), ButtonEvent::None);  // wraps past 0
  CHECK_EQ(feed(g, false, start + 2500, start + 2700), ButtonEvent::Reboot);
}

int main() {
  test_idle_low_gives_nothing();
  test_short_tap();
  test_reboot_between_2_and_4_s();
  test_setup_on_release_between_4_and_15_s();
  test_held_longer_than_fault_limit_is_fault_not_gesture();
  test_fault_fires_exactly_at_limit();
  test_bounce_during_hold_does_not_release();
  test_release_time_is_when_the_level_went_low();
  test_press_that_began_before_tracking();
  test_stays_faulted_when_starting_faulted_and_high();
  test_fault_clears_on_first_low_poll();
  test_held_ms_is_zero_when_not_pressed();
  test_millis_wraparound();
  if (failures) {
    std::printf("FAILED: %d check(s)\n", failures);
    return EXIT_FAILURE;
  }
  std::printf("All button gesture tests passed\n");
  return EXIT_SUCCESS;
}
