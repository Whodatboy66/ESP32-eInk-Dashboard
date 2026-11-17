#pragma once
// PC stand-in for the parts of the Arduino core the drawing code uses (tools/render).
#ifndef ARDUINO
#define ARDUINO 10800
#endif
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <algorithm>
#include "Print.h"
#include "WString.h"
#include "esp_attr.h"

class __FlashStringHelper;
typedef int gpio_num_t;

#define PROGMEM
#define F(s) (s)
#define pgm_read_byte(a) (*(const uint8_t*)(a))
#define pgm_read_word(a) (*(const uint16_t*)(a))
#define pgm_read_dword(a) (*(const uint32_t*)(a))
#define pgm_read_pointer(a) (*(void* const*)(a))
typedef bool boolean;
typedef uint8_t byte;
using std::min;
using std::max;
#ifndef constrain
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#endif
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
inline unsigned long millis() { return 0; }
inline void delay(unsigned long) {}
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return 0; }

class HardwareSerialStub : public Print {
public:
  size_t write(uint8_t c) override { return fputc(c, stderr) == EOF ? 0 : 1; }
  using Print::write;
  void begin(unsigned long) {}
};
extern HardwareSerialStub Serial;

// time helpers of the ESP32 Arduino core
inline bool getLocalTime(struct tm* info, uint32_t = 5000) {
  time_t now = time(nullptr);
  localtime_r(&now, info);
  return true;
}
