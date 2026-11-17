#pragma once
// PC stand-in for NVS: an in-memory key-value store (per namespace), so sample data can be stored
// the same way the firmware stores it.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include "WString.h"

class Preferences {
public:
  bool begin(const char* ns, bool = false) { ns_ = ns ? ns : ""; return true; }
  void end() {}
  bool clear() { for (auto it = store().begin(); it != store().end();) it = it->first.rfind(ns_ + "/", 0) == 0 ? store().erase(it) : std::next(it); return true; }
  int getInt(const char* k, int d = 0) { auto v = get(k); return v ? std::stoi(*v) : d; }
  unsigned getUInt(const char* k, unsigned d = 0) { auto v = get(k); return v ? (unsigned)std::stoul(*v) : d; }
  bool getBool(const char* k, bool d = false) { auto v = get(k); return v ? *v == "1" : d; }
  uint8_t getUChar(const char* k, uint8_t d = 0) { return (uint8_t)getUInt(k, d); }
  size_t getString(const char* k, char* out, size_t cap) {
    auto v = get(k);
    if (!cap) return 0;
    std::string s = v ? *v : "";
    strncpy(out, s.c_str(), cap - 1);
    out[cap - 1] = 0;
    return strlen(out);
  }
  String getString(const char* k, const String& d = String()) { auto v = get(k); return v ? String(*v) : d; }
  size_t putInt(const char* k, int v) { put(k, std::to_string(v)); return 4; }
  size_t putUInt(const char* k, unsigned v) { put(k, std::to_string(v)); return 4; }
  size_t putBool(const char* k, bool v) { put(k, v ? "1" : "0"); return 1; }
  size_t putUChar(const char* k, uint8_t v) { put(k, std::to_string(v)); return 1; }
  size_t putString(const char* k, const char* v) { put(k, v ? v : ""); return v ? strlen(v) : 0; }

private:
  static std::map<std::string, std::string>& store() { static std::map<std::string, std::string> m; return m; }
  const std::string* get(const char* k) { auto it = store().find(ns_ + "/" + k); return it == store().end() ? nullptr : &it->second; }
  void put(const char* k, const std::string& v) { store()[ns_ + "/" + k] = v; }
  std::string ns_;
};
