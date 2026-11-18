#pragma once
// Shared helpers of the personal widgets (docs/plans/STAGE4_PERSONAL_WIDGETS.md): calendar dates,
// UTF-8 text cut to a number of characters, and the filter for characters the screen font can't
// show. Plain C++, tested on the PC (tests/host/test_personal.cpp).
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace personal {

// ---- Dates (local calendar days, no time zone involved) ----
struct Date {
  int y = 0, m = 0, d = 0;  // y = 0: no date
  bool valid() const { return y > 0; }
};

inline bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
inline int daysInMonth(int y, int m) {
  static const int n[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return (m == 2 && isLeap(y)) ? 29 : n[(m - 1) % 12];
}
inline bool validDate(int y, int m, int d) {
  return y >= 2000 && y <= 2199 && m >= 1 && m <= 12 && d >= 1 && d <= daysInMonth(y, m);
}

// Days since 1970-01-01 (proleptic Gregorian calendar; H. Hinnant's days_from_civil).
inline long dayNumber(const Date& t) {
  int y = t.y - (t.m <= 2 ? 1 : 0);
  long era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = static_cast<unsigned>(y - era * 400);
  unsigned mp = static_cast<unsigned>(t.m + (t.m > 2 ? -3 : 9));
  unsigned doy = (153 * mp + 2) / 5 + static_cast<unsigned>(t.d) - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<long>(doe) - 719468;
}
// 0 = Monday ... 6 = Sunday.
inline int weekday(const Date& t) {
  long n = dayNumber(t);
  return static_cast<int>(((n % 7) + 7 + 3) % 7);  // 1970-01-01 was a Thursday
}

// "YYYY-MM-DD" -> Date; empty -> no date (true); anything else -> false.
inline bool parseIsoDate(const char* s, Date& out) {
  out = Date{};
  if (!s || !*s) return true;
  int y, m, d;
  char tail;
  if (std::strlen(s) != 10 || std::sscanf(s, "%4d-%2d-%2d%c", &y, &m, &d, &tail) != 3) return false;
  if (!validDate(y, m, d)) return false;
  out = Date{y, m, d};
  return true;
}
inline void formatIsoDate(const Date& t, char* out, size_t cap) {
  if (!t.valid()) {
    if (cap) out[0] = '\0';
    return;
  }
  std::snprintf(out, cap, "%04d-%02d-%02d", t.y, t.m, t.d);
}

// ---- UTF-8 ----
// Bytes of the UTF-8 sequence starting with this lead byte (1 for a broken one).
inline int utf8Len(unsigned char c) {
  if (c < 0x80) return 1;
  if ((c & 0xE0) == 0xC0) return 2;
  if ((c & 0xF0) == 0xE0) return 3;
  if ((c & 0xF8) == 0xF0) return 4;
  return 1;
}
inline uint32_t utf8Decode(const char* s, int* len) {
  const unsigned char* u = reinterpret_cast<const unsigned char*>(s);
  int n = utf8Len(u[0]);
  for (int i = 1; i < n; i++)
    if ((u[i] & 0xC0) != 0x80) n = 1;  // broken sequence: take the byte alone
  *len = n;
  if (n == 1) return u[0];
  uint32_t cp = u[0] & (0x7F >> n);
  for (int i = 1; i < n; i++) cp = (cp << 6) | (u[i] & 0x3F);
  return cp;
}
inline size_t utf8Count(const char* s) {
  size_t n = 0;
  for (int len; *s; s += len, n++) utf8Decode(s, &len);
  return n;
}

// Copies at most maxChars characters and cap - 1 bytes, never splitting a character.
// Returns false if something was cut.
inline bool copyChars(char* dst, size_t cap, const char* src, size_t maxChars) {
  size_t used = 0, chars = 0;
  bool complete = true;
  for (int len; *src; src += len) {
    utf8Decode(src, &len);
    if (chars >= maxChars || used + static_cast<size_t>(len) >= cap) {
      complete = false;
      break;
    }
    std::memcpy(dst + used, src, static_cast<size_t>(len));
    used += static_cast<size_t>(len);
    chars++;
  }
  if (cap) dst[used] = '\0';
  return complete;
}

// Removes characters the screen font can't draw (hasGlyph), keeping spaces and, if allowed, line
// breaks; tabs become spaces, "\r\n" becomes "\n". The removed characters (each once, up to
// removedCap - 1 bytes) go to `removed` for the API answer. Works in place.
inline void filterText(char* s, bool (*hasGlyph)(uint32_t), bool keepNewlines, char* removed = nullptr,
                       size_t removedCap = 0) {
  size_t r = 0, w = 0, rem = 0;
  if (removed && removedCap) removed[0] = '\0';
  while (s[r]) {
    int len;
    uint32_t cp = utf8Decode(s + r, &len);
    bool keep;
    if (cp == '\r') {  // "\r\n" -> "\n"; a lone CR is a line break too
      if (s[r + 1] != '\n') s[w++] = keepNewlines ? '\n' : ' ';
      r += 1;
      continue;
    } else if (cp == '\t') {
      s[w++] = ' ';
      r += 1;
      continue;
    } else if (cp == '\n') {
      keep = keepNewlines;
      if (!keep) {
        s[w++] = ' ';
        r += 1;
        continue;
      }
    } else {
      keep = cp == ' ' || (cp >= 0x20 && len >= 1 && hasGlyph(cp));
    }
    if (keep) {
      std::memmove(s + w, s + r, static_cast<size_t>(len));
      w += static_cast<size_t>(len);
    } else if (removed) {
      // remember it once
      char tmp[5] = {};
      std::memcpy(tmp, s + r, static_cast<size_t>(len));
      if (!std::strstr(removed, tmp) && rem + static_cast<size_t>(len) < removedCap) {
        std::memcpy(removed + rem, tmp, static_cast<size_t>(len));
        rem += static_cast<size_t>(len);
        removed[rem] = '\0';
      }
    }
    r += static_cast<size_t>(len);
  }
  s[w] = '\0';
}

// Trims spaces at both ends, in place.
inline void trimSpaces(char* s) {
  size_t n = std::strlen(s);
  while (n && (s[n - 1] == ' ' || s[n - 1] == '\n')) s[--n] = '\0';
  size_t lead = 0;
  while (s[lead] == ' ' || s[lead] == '\n') lead++;
  if (lead) std::memmove(s, s + lead, n - lead + 1);
}

}  // namespace personal
