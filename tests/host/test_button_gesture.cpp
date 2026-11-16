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

static const ButtonTiming kTiming{2000, 4000, 15000, 60};          // double tap off
static const ButtonTiming kDouble{2000, 4000, 15000, 60, 400};    // double tap on (400 ms)

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

// ---- Double tap ----

static void test_double_tap() {
  ButtonGesture g(kDouble);
  CHECK_EQ(feed(g, true, 1000, 1090), ButtonEvent::None);   // first tap, 100 ms
  CHECK_EQ(feed(g, false, 1100, 1240), ButtonEvent::None);  // 150 ms gap
  CHECK_EQ(feed(g, true, 1250, 1340), ButtonEvent::None);   // second tap
  CHECK_EQ(feed(g, false, 1350, 1600), ButtonEvent::DoubleTap);
  CHECK_EQ(g.isBusy(), false);
}

static void test_single_tap_reported_after_the_window() {
  ButtonGesture g(kDouble);
  CHECK_EQ(feed(g, true, 1000, 1090), ButtonEvent::None);
  // Released at 1100: no Tap during the 400 ms window, and the gesture counts as busy.
  CHECK_EQ(feed(g, false, 1100, 1490), ButtonEvent::None);
  CHECK_EQ(g.isBusy(), true);
  CHECK_EQ(g.update(false, 1500), ButtonEvent::Tap);
  CHECK_EQ(g.isBusy(), false);
}

static void test_taps_further_apart_are_two_single_taps() {
  ButtonGesture g(kDouble);
  CHECK_EQ(press(g, 1000, 100), ButtonEvent::None);  // still waiting for a second tap
  CHECK_EQ(feed(g, false, 1310, 2000), ButtonEvent::Tap);
  CHECK_EQ(press(g, 3000, 100), ButtonEvent::None);
  CHECK_EQ(feed(g, false, 3310, 4000), ButtonEvent::Tap);
}

static void test_tap_then_hold_is_the_hold_gesture() {
  ButtonGesture g(kDouble);
  CHECK_EQ(press(g, 1000, 100), ButtonEvent::None);
  CHECK_EQ(press(g, 1350, 2500), ButtonEvent::Reboot);
  CHECK_EQ(g.isBusy(), false);
}

static void test_tap_then_stuck_is_fault() {
  ButtonGesture g(kDouble);
  CHECK_EQ(press(g, 1000, 100), ButtonEvent::None);
  CHECK_EQ(feed(g, true, 1350, 20000), ButtonEvent::Fault);
}

static void test_double_tap_after_a_tap_that_woke_the_board() {
  // The first tap woke the board from deep sleep and was released before the firmware ran.
  ButtonGesture g(kDouble);
  g.beginAfterTap(200);
  CHECK_EQ(g.isBusy(), true);
  CHECK_EQ(feed(g, false, 200, 340), ButtonEvent::None);
  CHECK_EQ(press(g, 350, 100), ButtonEvent::DoubleTap);
}

static void test_wake_tap_alone_is_a_single_tap() {
  ButtonGesture g(kDouble);
  g.beginAfterTap(200);
  CHECK_EQ(feed(g, false, 200, 590), ButtonEvent::None);
  CHECK_EQ(g.update(false, 600), ButtonEvent::Tap);
}

static void test_begin_after_tap_without_double_tap_reports_tap() {
  ButtonGesture g(kTiming);
  g.beginAfterTap(200);
  CHECK_EQ(g.update(false, 200), ButtonEvent::Tap);
}

static void test_begin_after_tap_is_ignored_while_faulted() {
  ButtonGesture g(kDouble, /*faulted=*/true);
  g.beginAfterTap(200);
  CHECK_EQ(g.isBusy(), false);
}

static void test_wake_press_then_second_tap_is_double_tap() {
  // The wake tap was still held when the firmware ran: beginPress(0), then release, then tap.
  ButtonGesture g(kDouble);
  g.beginPress(0);
  CHECK_EQ(feed(g, true, 150, 190), ButtonEvent::None);
  CHECK_EQ(feed(g, false, 200, 340), ButtonEvent::None);
  CHECK_EQ(press(g, 350, 100), ButtonEvent::DoubleTap);
}

static void test_begin_after_tap_then_press_without_double_tap() {
  // Double tap off: the wake tap is a Tap, and a press right after it is a new press, not a double.
  ButtonGesture g(kTiming);
  g.beginAfterTap(200);
  CHECK_EQ(g.update(true, 210), ButtonEvent::Tap);
  CHECK_EQ(feed(g, true, 220, 290), ButtonEvent::None);
  CHECK_EQ(feed(g, false, 300, 500), ButtonEvent::Tap);
}

static void test_press_just_after_the_window_is_not_a_double_tap() {
  // The first poll after the window sees the press: report the pending Tap, then a new press.
  ButtonGesture g(kDouble);
  CHECK_EQ(press(g, 1000, 100), ButtonEvent::None);     // released at 1100, window ends at 1500
  CHECK_EQ(g.update(true, 1510), ButtonEvent::Tap);     // the first tap, late
  CHECK_EQ(feed(g, true, 1520, 1590), ButtonEvent::None);
  CHECK_EQ(feed(g, false, 1600, 1800), ButtonEvent::None);  // new tap pending
  CHECK_EQ(feed(g, false, 1810, 2100), ButtonEvent::Tap);
}

static void test_bounce_during_second_tap() {
  ButtonGesture g(kDouble);
  CHECK_EQ(press(g, 1000, 100), ButtonEvent::None);
  CHECK_EQ(feed(g, true, 1300, 1340), ButtonEvent::None);
  CHECK_EQ(feed(g, false, 1350, 1370), ButtonEvent::None);  // 30 ms bounce < debounce
  CHECK_EQ(feed(g, true, 1380, 1420), ButtonEvent::None);
  CHECK_EQ(feed(g, false, 1430, 1600), ButtonEvent::DoubleTap);
}

static void test_is_busy_while_pressed() {
  ButtonGesture g(kTiming);
  CHECK_EQ(g.isBusy(), false);
  g.update(true, 0);
  CHECK_EQ(g.isBusy(), true);
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
  test_double_tap();
  test_single_tap_reported_after_the_window();
  test_taps_further_apart_are_two_single_taps();
  test_tap_then_hold_is_the_hold_gesture();
  test_tap_then_stuck_is_fault();
  test_double_tap_after_a_tap_that_woke_the_board();
  test_wake_tap_alone_is_a_single_tap();
  test_begin_after_tap_without_double_tap_reports_tap();
  test_begin_after_tap_is_ignored_while_faulted();
  test_wake_press_then_second_tap_is_double_tap();
  test_is_busy_while_pressed();
  test_begin_after_tap_then_press_without_double_tap();
  test_press_just_after_the_window_is_not_a_double_tap();
  test_bounce_during_second_tap();
  if (failures) {
    std::printf("FAILED: %d check(s)\n", failures);
    return EXIT_FAILURE;
  }
  std::printf("All button gesture tests passed\n");
  return EXIT_SUCCESS;
}
