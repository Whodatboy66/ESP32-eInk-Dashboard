#pragma once
// The log of one wake, collected in RAM for the diagnostics view (docs/plans/STAGE5B_DIAGNOSTICS.md)
// and the battery history file. Plain C++, tested on the PC (tests/host/test_log_buffer.cpp).
#include <cstddef>
#include <cstdio>
#include <cstring>

// Text printed in pieces (print, println, printf) becomes whole lines, each starting with a prefix
// (the time). Full: the rest of the wake is dropped, with one "[log cut]" line.
template <size_t N>
class LogBuffer {
 public:
  // prefix(out, cap) writes the start of a new line, e.g. "09:41:20 ".
  using PrefixFn = void (*)(char* out, size_t cap);

  void append(const char* text, PrefixFn prefix) {
    for (const char* p = text; *p && !cut_; p++) {
      if (atLineStart_) {
        if (*p == '\r') continue;
        char pre[40] = "";
        if (prefix) prefix(pre, sizeof(pre));
        if (!put(pre)) return;
        atLineStart_ = false;
      }
      if (*p == '\r') continue;
      if (!putChar(*p)) return;
      if (*p == '\n') atLineStart_ = true;
    }
  }
  // A line of its own (date line, markers), not prefixed.
  void appendRaw(const char* line) {
    if (cut_) return;
    if (!atLineStart_) putChar('\n');
    atLineStart_ = true;
    put(line);
    putChar('\n');
  }

  const char* data() const { return buf_; }
  size_t size() const { return len_; }
  bool empty() const { return len_ == 0; }
  void clear() {
    len_ = 0;
    buf_[0] = '\0';
    cut_ = false;
    atLineStart_ = true;
  }

 private:
  static constexpr const char* CUT = "[log cut]\n";
  bool put(const char* s) {
    for (; *s; s++)
      if (!putChar(*s)) return false;
    return true;
  }
  bool putChar(char c) {
    if (len_ + 2 + std::strlen(CUT) >= N) {  // keep room for a newline, the cut mark and the NUL
      if (!cut_) {
        if (!atLineStart_) buf_[len_++] = '\n';
        std::memcpy(buf_ + len_, CUT, std::strlen(CUT));
        len_ += std::strlen(CUT);
        buf_[len_] = '\0';
        cut_ = true;
      }
      return false;
    }
    buf_[len_++] = c;
    buf_[len_] = '\0';
    return true;
  }
  char buf_[N] = "";
  size_t len_ = 0;
  bool atLineStart_ = true;
  bool cut_ = false;
};

// Keeps the last maxLines lines of a text (in place); returns the new length.
inline size_t keepLastLines(char* text, size_t len, int maxLines) {
  int lines = 0;
  size_t i = len;
  if (i && text[i - 1] == '\n') i--;  // the final newline doesn't start a line
  while (i > 0) {
    if (text[i - 1] == '\n' && ++lines >= maxLines) break;
    i--;
  }
  if (i == 0) return len;
  std::memmove(text, text + i, len - i);
  text[len - i] = '\0';
  return len - i;
}

// Pointers to the starts of the last `max` lines of a text (oldest first); returns how many.
inline int lastLines(const char* text, size_t len, const char** starts, int max) {
  int n = 0;
  size_t i = len;
  if (i && text[i - 1] == '\n') i--;
  while (i > 0 && n < max) {
    size_t end = i;
    while (i > 0 && text[i - 1] != '\n') i--;
    if (end > i || n == 0) starts[n++] = text + i;
    if (i > 0) i--;  // skip the newline before this line
  }
  // reverse: oldest first
  for (int a = 0, b = n - 1; a < b; a++, b--) {
    const char* t = starts[a];
    starts[a] = starts[b];
    starts[b] = t;
  }
  return n;
}
