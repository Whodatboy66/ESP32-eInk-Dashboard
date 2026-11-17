#pragma once
#include <string>
#include "Print.h"
class String {
public:
  String(const char* s = "") : s_(s ? s : "") {}
  String(const std::string& s) : s_(s) {}
  const char* c_str() const { return s_.c_str(); }
  unsigned int length() const { return (unsigned)s_.size(); }
  bool operator==(const char* o) const { return s_ == o; }
  String& operator+=(const char* o) { s_ += o; return *this; }
  String& operator+=(const String& o) { s_ += o.s_; return *this; }
  String& operator+=(char c) { s_ += c; return *this; }
  char operator[](unsigned i) const { return s_[i]; }
private:
  std::string s_;
};
inline size_t Print::print(const String& s) { return write(s.c_str()); }
