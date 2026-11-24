// Host test for src/widgets/ClockLayout.h (stage 7): which digit size, AM/PM and the date fit a
// clock box, measured with the real fonts. Run: tests/host/run.sh
#include <cstring>

#include <Adafruit_GFX.h>
#include "src/ui/fonts/ClockDigits_110.h"
#include "src/ui/fonts/ClockDigits_180.h"
#include "src/ui/fonts/ClockDigits_64.h"
#include "src/ui/fonts/Heading_22.h"
#include "src/widgets/ClockLayout.h"
#include "tests/host/check.h"

using namespace clockui;

namespace {

const SparseGFXfont& fontOf(Digits d) {
  return d == Digits::D64 ? ClockDigits_64 : d == Digits::D110 ? ClockDigits_110 : ClockDigits_180;
}

const GFXglyph* glyph(const SparseGFXfont& f, char c) {
  for (int i = 0; i < f.glyphCount; i++)
    if (f.charMap[i] == (uint32_t)(unsigned char)c) return &f.glyph[i];
  return nullptr;
}

int textWidth(const SparseGFXfont& f, const char* s) {
  int w = 0;
  for (; *s; s++) {
    const GFXglyph* g = glyph(f, *s);
    w += g ? g->xAdvance : f.yAdvance / 2;
  }
  return w;
}

int digitsWidth(Digits d, const char* s) { return textWidth(fontOf(d), s); }

// The layout stays inside the box (with the 4 px padding).
void checkInside(const Layout& l, int w, int h, const char* time, int amPmW, int dateW) {
  int tw = digitsWidth(l.digits, time);
  CHECK(l.digitsX >= PAD);
  CHECK(l.digitsX + tw <= w - PAD);
  if (l.showAmPm) CHECK(l.amPmX + amPmW <= w - PAD);
  CHECK(l.digitsBaseline - capHeight(l.digits) >= PAD);
  if (l.showDate) {
    CHECK(l.dateBaseline <= h - PAD);
    CHECK(dateW <= w - 2 * PAD);
  } else {
    CHECK(l.digitsBaseline <= h - PAD);
  }
}

const int AMPM_W = textWidth(Heading_22, "AM");
const int DATE_W = textWidth(Heading_22, "Donnerstag, 24. September");  // the longest German date

void test_cap_heights_match_fonts() {
  CHECK_EQ(capHeight(Digits::D64), glyph(ClockDigits_64, '0')->height);
  CHECK_EQ(capHeight(Digits::D110), glyph(ClockDigits_110, '0')->height);
  CHECK_EQ(capHeight(Digits::D180), glyph(ClockDigits_180, '0')->height);
}

void test_clock_2x1_12h() {
  Layout l = layoutClock(400, 150, "10:28", "AM", digitsWidth, AMPM_W, DATE_W);
  CHECK(l.digits == Digits::D110);
  CHECK(l.showAmPm);
  CHECK(l.showDate);
  checkInside(l, 400, 150, "10:28", AMPM_W, DATE_W);
}

void test_clock_classic() {
  Layout l = layoutClock(430, 155, "23:59", nullptr, digitsWidth, AMPM_W, DATE_W);
  CHECK(l.digits == Digits::D110);
  CHECK(!l.showAmPm);
  CHECK(l.showDate);
  checkInside(l, 430, 155, "23:59", AMPM_W, DATE_W);
}

void test_clock_full() {
  Layout l = layoutClock(800, 300, "20:08", nullptr, digitsWidth, AMPM_W, DATE_W);
  CHECK(l.digits == Digits::D180);
  CHECK(l.showDate);
  checkInside(l, 800, 300, "20:08", AMPM_W, DATE_W);
}

void test_clock_1x1() {
  Layout l = layoutClock(200, 150, "20:08", nullptr, digitsWidth, AMPM_W, DATE_W);
  CHECK(l.digits == Digits::D64);
  CHECK(!l.showDate);  // the long date doesn't fit 200 px
  checkInside(l, 200, 150, "20:08", AMPM_W, DATE_W);
  int shortDate = textWidth(Heading_22, "Mo, 23. Nov");
  l = layoutClock(200, 150, "20:08", nullptr, digitsWidth, AMPM_W, shortDate);
  CHECK(l.showDate);
  checkInside(l, 200, 150, "20:08", AMPM_W, shortDate);
}

// AM/PM matters more than the digit size: a box where "12:58 PM" fits only with the small
// digits gets the small digits with AM/PM; where it fits with none, AM/PM is dropped.
void test_clock_too_narrow_ampm() {
  int w = digitsWidth(Digits::D110, "12:58") + 2 * PAD + 2;  // D110 alone fits, with AM/PM not
  Layout l = layoutClock(w, 150, "12:58", "PM", digitsWidth, AMPM_W, DATE_W);
  CHECK(l.digits == Digits::D64);
  CHECK(l.showAmPm);
  int tiny = digitsWidth(Digits::D64, "12:58") + 2 * PAD + 2;
  l = layoutClock(tiny, 150, "12:58", "PM", digitsWidth, AMPM_W, DATE_W);
  CHECK(l.digits == Digits::D64);
  CHECK(!l.showAmPm);
  checkInside(l, tiny, 150, "12:58", AMPM_W, DATE_W);
}

}  // namespace

int main() {
  test_cap_heights_match_fonts();
  test_clock_2x1_12h();
  test_clock_classic();
  test_clock_full();
  test_clock_1x1();
  test_clock_too_narrow_ampm();
  return finish("clock layout");
}
