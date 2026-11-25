#include "Problems.h"

#include <esp_attr.h>

RTC_DATA_ATTR uint16_t problems = 0;
RTC_DATA_ATTR uint16_t problemsShown = 0;
RTC_DATA_ATTR uint8_t icsFailedMask = 0;

void setProblem(uint16_t flag, bool active) {
  problems = active ? (problems | flag) : (problems & ~flag);
}

