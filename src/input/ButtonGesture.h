#pragma once
// Button / TTP223 gesture detection: tap, hold-to-reboot, hold-to-setup, and a fault limit for
// a sensor that reports "touched" for too long (disconnected, stuck or recalibrating).
//
// Plain C++ without Arduino headers: the caller passes the pin level and the time, so the same
// logic runs at wake-up, in the awake watch task and in the host tests (tests/host).

#include <cstdint>

enum class ButtonEvent : uint8_t {
  None,    // nothing completed yet
  Tap,     // released before rebootMs
  Reboot,  // released between rebootMs and setupMs
  Setup,   // released between setupMs and faultMs
  Fault    // held for faultMs: ignored until the input reads low again
};

struct ButtonTiming {
  uint32_t rebootMs;    // hold at least this long (then release) to reboot
  uint32_t setupMs;     // hold at least this long (then release) for setup mode
  uint32_t faultMs;     // a "touch" this long is a fault, not a gesture
  uint32_t debounceMs;  // the input must stay low this long to count as released
};

class ButtonGesture {
public:
  explicit ButtonGesture(const ButtonTiming& timing, bool faulted = false)
    : timing_(timing), faulted_(faulted) {}

  // Starts a press that began before tracking did (e.g. the press that woke the board).
  void beginPress(uint32_t startMs) {
    if (faulted_) return;
    pressed_ = true;
    lowPending_ = false;
    pressStart_ = startMs;
  }

  // Call regularly (every 10-20 ms) with the current level (true = pressed / HIGH).
  ButtonEvent update(bool high, uint32_t nowMs) {
    if (faulted_) {
      if (!high) faulted_ = false;  // the sensor recovered; the next press counts again
      return ButtonEvent::None;
    }

    if (high) {
      lowPending_ = false;
      if (!pressed_) {
        pressed_ = true;
        pressStart_ = nowMs;
      }
      if (nowMs - pressStart_ >= timing_.faultMs) {
        pressed_ = false;
        faulted_ = true;
        return ButtonEvent::Fault;
      }
      return ButtonEvent::None;
    }

    if (!pressed_) return ButtonEvent::None;
    if (!lowPending_) {
      lowPending_ = true;
      lowSince_ = nowMs;
    }
    if (nowMs - lowSince_ < timing_.debounceMs) return ButtonEvent::None;

    // Released: the hold ended when the input first went low, not after the debounce.
    pressed_ = false;
    lowPending_ = false;
    const uint32_t held = lowSince_ - pressStart_;
    if (held >= timing_.setupMs) return ButtonEvent::Setup;
    if (held >= timing_.rebootMs) return ButtonEvent::Reboot;
    return ButtonEvent::Tap;
  }

  bool isPressed() const { return pressed_; }
  bool isFaulted() const { return faulted_; }
  uint32_t heldMs(uint32_t nowMs) const { return pressed_ ? nowMs - pressStart_ : 0; }

private:
  ButtonTiming timing_;
  bool faulted_;
  bool pressed_ = false;
  bool lowPending_ = false;
  uint32_t pressStart_ = 0;
  uint32_t lowSince_ = 0;
};
