#pragma once
// Clock layout (stage 7, docs/plans/STAGE7_NEW_LOOK.md task 2): which digit size, AM/PM and the
// date fit a clock box. Plain C++, the widths come from a callback, so it is tested on the PC
// with the real fonts (tests/host/test_clock_layout.cpp). Positions are relative to the box.

namespace clockui {

enum class Digits : unsigned char { D64, D110, D180 };

constexpr int PAD = 4;           // kept free at every edge of the box
constexpr int AMPM_GAP = 6;      // between the digits and AM/PM
constexpr int DATE_GAP = 8;      // between the digits and the date line
constexpr int DATE_ASCENT = 16;  // Heading_22: from the top of the date line to its baseline
constexpr int DATE_LINE = 22;    // the date line's height (with descenders)

// Height of the digits above the baseline ('0' of src/ui/fonts/ClockDigits_*.h; the test checks).
constexpr int capHeight(Digits d) { return d == Digits::D64 ? 47 : d == Digits::D110 ? 81 : 132; }

struct Layout {
  Digits digits = Digits::D64;
  int digitsX = 0, digitsBaseline = 0;
  bool showAmPm = false;
  int amPmX = 0;
  bool showDate = false;
  int dateBaseline = 0;
};

using WidthFn = int (*)(Digits, const char* text);

// The largest digits that fit, in this order of importance: AM/PM (a 12-hour clock without it is
// ambiguous), then the date, then the digit size. amPm == nullptr: a 24-hour clock.
inline Layout layoutClock(int boxW, int boxH, const char* time, const char* amPm, WidthFn digitsWidth,
                          int amPmWidth, int dateWidth) {
  const Digits sizes[] = {Digits::D180, Digits::D110, Digits::D64};
  const bool wants[4][2] = {{true, true}, {true, false}, {false, true}, {false, false}};  // {AM/PM, date}
  for (const auto& want : wants) {
    bool withAmPm = want[0], withDate = want[1];
    if (withAmPm && !amPm) continue;
    if (withDate && dateWidth > boxW - 2 * PAD) continue;
    for (Digits d : sizes) {
      int tw = digitsWidth(d, time);
      int groupW = tw + (withAmPm ? AMPM_GAP + amPmWidth : 0);
      int blockH = capHeight(d) + (withDate ? DATE_GAP + DATE_LINE : 0);
      if (groupW > boxW - 2 * PAD || blockH > boxH - 2 * PAD) continue;
      Layout l;
      l.digits = d;
      l.digitsX = (boxW - groupW) / 2;
      l.digitsBaseline = (boxH - blockH) / 2 + capHeight(d);
      l.showAmPm = withAmPm;
      l.amPmX = l.digitsX + tw + AMPM_GAP;
      l.showDate = withDate;
      l.dateBaseline = l.digitsBaseline + DATE_GAP + DATE_ASCENT;
      return l;
    }
  }
  Layout l;  // nothing fits: the small digits, centred
  l.digitsX = (boxW - digitsWidth(Digits::D64, time)) / 2;
  l.digitsBaseline = (boxH + capHeight(Digits::D64)) / 2;
  return l;
}

}  // namespace clockui
