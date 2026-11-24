// Host test for src/ui/GlyphText.h (stage 7b): UTF-8 decoding (also broken input), glyph lookup,
// widths and drawing of the sparse bitmap fonts, with the real screen font. Run: tests/host/run.sh
#include <cstring>
#include <set>
#include <utility>

#include "src/ui/GlyphText.h"
#include "src/ui/fonts/epaperFont.h"
#include "tests/host/check.h"

using namespace glyphtext;

namespace {

void test_utf8() {
  const char* s = "Aä€😀";
  CHECK_EQ(nextCodepoint(&s), 0x41u);
  CHECK_EQ(nextCodepoint(&s), 0xE4u);
  CHECK_EQ(nextCodepoint(&s), 0x20ACu);
  CHECK_EQ(nextCodepoint(&s), 0x1F600u);
  CHECK(*s == '\0');
}

// Stage 7b: broken UTF-8 (a lone continuation byte, 0xFF, a sequence cut off by the end of the
// string) must still move on and never read past the end. The old decoder stood still on such a
// byte: an endless loop (the device hung until the watchdog) on a broken calendar title.
void test_utf8_broken() {
  const char bad[] = {'a', (char)0x80, (char)0xFF, 'b', 0};
  const char* s = bad;
  CHECK_EQ(nextCodepoint(&s), 0x61u);
  CHECK_EQ(nextCodepoint(&s), REPLACEMENT);
  CHECK_EQ(nextCodepoint(&s), REPLACEMENT);
  CHECK_EQ(nextCodepoint(&s), 0x62u);
  CHECK(*s == '\0');
  const char cut[] = {'x', (char)0xE2, (char)0x82, 0};  // "€" without its last byte
  s = cut;
  CHECK_EQ(nextCodepoint(&s), 0x78u);
  CHECK_EQ(nextCodepoint(&s), REPLACEMENT);
  CHECK(*s == '\0');  // stopped at the end, not beyond
  const char bad2[] = {(char)0xC3, 'z', 0};  // a lead byte followed by a normal character
  s = bad2;
  CHECK_EQ(nextCodepoint(&s), REPLACEMENT);
  CHECK_EQ(nextCodepoint(&s), 0x7Au);
}

void test_lookup_and_width() {
  const GFXglyph* a = findGlyph(&epaperFont, 'A');
  CHECK(a != nullptr);
  CHECK(findGlyph(&epaperFont, 0x1F600) == nullptr);
  CHECK(findGlyph(&epaperFont, 0) == nullptr);
  // A missing glyph (also the space, not stored) advances half the font height.
  CHECK_EQ(textWidth(&epaperFont, " "), epaperFont.yAdvance / 2);
  CHECK_EQ(textWidth(&epaperFont, "AA"), 2 * a->xAdvance);
  CHECK_EQ(textWidth(&epaperFont, ""), 0);
  // Every glyph of the font is found (the table is sorted for the binary search).
  for (int i = 0; i < epaperFont.glyphCount; i++) CHECK(findGlyph(&epaperFont, epaperFont.charMap[i]) == &epaperFont.glyph[i]);
}

std::set<std::pair<int, int>> pixels;
void plot(int16_t x, int16_t y, uint16_t) { pixels.insert({x, y}); }

void test_draw() {
  pixels.clear();
  int16_t end = drawText(&epaperFont, 10, 30, "I", 0, plot);
  const GFXglyph* g = findGlyph(&epaperFont, 'I');
  CHECK_EQ(end, 10 + g->xAdvance);
  CHECK(!pixels.empty());
  for (const auto& p : pixels) {  // inside the glyph's box, 1 px below the usual baseline convention
    CHECK(p.first >= 10 + g->xOffset && p.first < 10 + g->xOffset + g->width);
    CHECK(p.second >= 30 + g->yOffset && p.second < 30 + g->yOffset + g->height);
  }
  pixels.clear();
  drawText(&epaperFont, 0, 0, "\x80\xFF", 0, plot);  // broken input: nothing drawn, no hang
  CHECK(pixels.empty());
}

}  // namespace

int main() {
  test_utf8();
  test_utf8_broken();
  test_lookup_and_width();
  test_draw();
  return finish("glyph text");
}
