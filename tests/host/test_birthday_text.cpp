// Host test for src/widgets/BirthdayText.h: a birthday line shortened to its box, the age kept
// (device test of build 80: "Geburtstag Lennart Kempf t…"). Run: tests/host/run.sh
#include <cstring>

#include "src/widgets/BirthdayText.h"
#include "tests/host/check.h"

namespace {

// 10 px per character (UTF-8 aware: "ü" and "…" are one character each).
int width(const char* s) {
  int n = 0;
  for (; *s; s++)
    if ((static_cast<unsigned char>(*s) & 0xC0) != 0x80) n++;
  return n * 10;
}

const char* fit(const char* name, int age, const char* longFmt, int maxW) {
  static char out[128];
  bdaytext::fit(name, age, longFmt, maxW, width, out, sizeof(out));
  return out;
}

void expectText(const char* got, const char* want) {
  if (std::strcmp(got, want)) std::printf("  got \"%s\", want \"%s\"\n", got, want);
  CHECK_STR(got, want);
}

void test_steps() {
  const char* LONG = "%s wird %d";
  expectText(fit("Anna", 60, LONG, 200), "Anna wird 60");                // fits as a sentence
  expectText(fit("Lennart Kempf", 30, LONG, 180), "Lennart Kempf (30)");  // the age in parentheses
  expectText(fit("Lennart Kempf", 30, LONG, 150), "Lennart K. (30)");     // the last name as initial
  expectText(fit("Lennart Kempf", 30, LONG, 100), "Lenn\xE2\x80\xA6 (30)");  // cut, the age stays
  expectText(fit("Lennart Kempf", 0, LONG, 200), "Lennart Kempf");       // no age known
  expectText(fit("Lennart Kempf", 0, LONG, 100), "Lennart K.");
  expectText(fit("Anna", 60, nullptr, 200), "Anna (60)");                // the widget's short form
}

void test_utf8_cut() {
  // "Jürgen M. (5)" is 13 characters: cut by whole characters, never inside the "ü".
  expectText(fit("Jürgen Müller", 5, nullptr, 80), "Jür\xE2\x80\xA6 (5)");
  expectText(fit("Ölaf Ärger", 0, nullptr, 70), "Ölaf Ä.");  // the initial is a whole "Ä"
}

void test_tiny_box() {
  // Narrower than anything: one character, the ellipsis and the age (the caller cuts the rest).
  expectText(fit("Lennart", 30, nullptr, 10), "L\xE2\x80\xA6 (30)");
  expectText(fit("", 30, nullptr, 100), " (30)");
}

}  // namespace

int main() {
  test_steps();
  test_utf8_cut();
  test_tiny_box();
  return finish("birthday text");
}
