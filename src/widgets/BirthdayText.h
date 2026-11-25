#pragma once
// A birthday's name line, shortened to its box with the age always kept (device test of build 80:
// "Geburtstag Lennart Kempf t…" lost the age). Plain C++, tested on the PC
// (tests/host/test_birthday_text.cpp); the width comes from the caller's font.
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace bdaytext {

constexpr const char* ELLIPSIS = "\xE2\x80\xA6";  // …

// "Lennart Kempf" -> "Lennart K." (the last word as its first character); false for one word.
inline bool initialLast(const char* name, char* out, size_t cap) {
  const char* space = std::strrchr(name, ' ');
  if (!space || !space[1] || space == name) return false;
  size_t first = 1;
  while ((static_cast<unsigned char>(space[1 + first]) & 0xC0) == 0x80) first++;  // a whole UTF-8 character
  std::snprintf(out, cap, "%.*s%.*s.", static_cast<int>(space + 1 - name), name, static_cast<int>(first), space + 1);
  return true;
}

// The first try that fits maxW: the sentence (longFmt, e.g. "%s wird %d"; nullptr = skip it),
// "Lennart Kempf (30)", "Lennart K. (30)", then the name cut with "…" before " (30)". Without an
// age (0) the same without the parentheses. The last try may still be too wide for a tiny box.
template <class Width>
void fit(const char* name, int age, const char* longFmt, int maxW, Width width, char* out, size_t cap) {
  if (age > 0 && longFmt) {
    std::snprintf(out, cap, longFmt, name, age);
    if (width(out) <= maxW) return;
  }
  char suffix[16] = "";
  if (age > 0) std::snprintf(suffix, sizeof(suffix), " (%d)", age);
  std::snprintf(out, cap, "%s%s", name, suffix);
  if (width(out) <= maxW) return;
  char base[96];
  if (initialLast(name, base, sizeof(base))) {
    std::snprintf(out, cap, "%s%s", base, suffix);
    if (width(out) <= maxW) return;
  } else {
    std::snprintf(base, sizeof(base), "%s", name);
  }
  size_t n = std::strlen(base);
  if (!n) {
    std::snprintf(out, cap, "%s", suffix);
    return;
  }
  for (;;) {
    size_t prev = n;  // drop the last whole character
    do prev--;
    while (prev && (static_cast<unsigned char>(base[prev]) & 0xC0) == 0x80);
    while (prev && base[prev - 1] == ' ') prev--;
    if (!prev) break;  // keep at least one character
    n = prev;
    std::snprintf(out, cap, "%.*s%s%s", static_cast<int>(n), base, ELLIPSIS, suffix);
    if (width(out) <= maxW) return;
  }
  size_t one = 1;
  while ((static_cast<unsigned char>(base[one]) & 0xC0) == 0x80) one++;
  std::snprintf(out, cap, "%.*s%s%s", static_cast<int>(one), base, ELLIPSIS, suffix);
}

}  // namespace bdaytext
