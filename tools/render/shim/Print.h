#pragma once
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
class String;
class Print {
public:
  virtual ~Print() {}
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t* b, size_t n) { size_t i = 0; while (i < n && write(b[i])) i++; return i; }
  size_t write(const char* s) { return s ? write((const uint8_t*)s, strlen(s)) : 0; }
  size_t print(const char* s) { return write(s); }
  size_t print(const String& s);
  size_t print(int v) { char b[16]; snprintf(b, sizeof(b), "%d", v); return write(b); }
  size_t print(unsigned v) { char b[16]; snprintf(b, sizeof(b), "%u", v); return write(b); }
  size_t print(long v) { char b[24]; snprintf(b, sizeof(b), "%ld", v); return write(b); }
  size_t print(unsigned long v) { char b[24]; snprintf(b, sizeof(b), "%lu", v); return write(b); }
  size_t print(double v) { char b[32]; snprintf(b, sizeof(b), "%.2f", v); return write(b); }
  size_t println() { return write("\n"); }
  template <typename T> size_t println(T v) { size_t n = print(v); return n + println(); }
  size_t printf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    char b[512]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof(b), fmt, a); va_end(a); return write(b);
  }
};
