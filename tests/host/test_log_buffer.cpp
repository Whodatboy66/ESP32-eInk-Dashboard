// Host test for src/core/LogBuffer.h (diagnostics log). Run: tests/host/run.sh
#include <string>

#include "src/core/LogBuffer.h"
#include "tests/host/check.h"

static int tick = 0;
static void prefix(char* out, size_t cap) { std::snprintf(out, cap, "T%d ", tick++); }

static void test_lines_from_pieces() {
  tick = 0;
  LogBuffer<256> b;
  b.append("Wi-Fi ", prefix);
  b.append("connected\n", prefix);
  b.append("[Weather] ok\r\n[ICS] 3 events\n", prefix);
  CHECK_STR(b.data(), "T0 Wi-Fi connected\nT1 [Weather] ok\nT2 [ICS] 3 events\n");
  b.appendRaw("--- 2026-11-19 ---");
  b.append("next", prefix);
  CHECK_STR(b.data(), "T0 Wi-Fi connected\nT1 [Weather] ok\nT2 [ICS] 3 events\n--- 2026-11-19 ---\nT3 next");
  b.appendRaw("[end]");  // ends the open line first
  CHECK(std::string(b.data()).find("T3 next\n[end]\n") != std::string::npos);
  b.clear();
  CHECK(b.empty());
}

static void test_full_buffer_is_cut_once() {
  tick = 0;
  LogBuffer<64> b;
  for (int i = 0; i < 20; i++) b.append("0123456789\n", prefix);
  std::string s = b.data();
  CHECK(s.size() < 64);
  CHECK(s.find("[log cut]\n") != std::string::npos);
  CHECK_EQ(s.find("[log cut]"), s.rfind("[log cut]"));  // once
  size_t before = b.size();
  b.append("more\n", prefix);
  CHECK_EQ(b.size(), before);  // nothing after the cut
}

static void test_keep_last_lines() {
  char t[64] = "a\nb\nc\nd\n";
  size_t n = keepLastLines(t, std::strlen(t), 2);
  CHECK_STR(t, "c\nd\n");
  CHECK_EQ(n, 4u);
  char u[64] = "a\nb\n";
  CHECK_EQ(keepLastLines(u, std::strlen(u), 5), 4u);  // fewer lines: unchanged
  CHECK_STR(u, "a\nb\n");
  char w[64] = "x\ny";  // no final newline
  keepLastLines(w, std::strlen(w), 1);
  CHECK_STR(w, "y");
}

static void test_last_lines() {
  const char* t = "one\ntwo\nthree\n";
  const char* s[8];
  int n = lastLines(t, std::strlen(t), s, 2);
  CHECK_EQ(n, 2);
  CHECK(std::strncmp(s[0], "two\n", 4) == 0);
  CHECK(std::strncmp(s[1], "three\n", 6) == 0);
  n = lastLines(t, std::strlen(t), s, 8);
  CHECK_EQ(n, 3);
  CHECK(s[0] == t);
  CHECK_EQ(lastLines("", 0, s, 8), 0);
}

int main() {
  test_lines_from_pieces();
  test_full_buffer_is_cut_once();
  test_keep_last_lines();
  test_last_lines();
  return finish("log buffer");
}
