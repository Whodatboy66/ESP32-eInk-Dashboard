#pragma once
// Columns of the data boxes from their width (stage 8 part B, the look pass): the stocks box used
// columns fixed for a 430 px place, empty on the right of a wider box. Plain C++, tested on the PC
// (tests/host/test_data_columns.cpp).
#include <cstdio>

namespace datacols {

struct StockColumns {
  int nameX, nameEnd;  // arrow left of nameX; the name is cut before nameEnd
  int priceRight;      // the price, right-aligned
  int changeRight;     // the day's change in percent, right-aligned
  int rangeX;          // the day's range "96,16 - 99,10" from here (0 = no room)
};

// For a box w px wide (inside its padding).
inline StockColumns stockColumns(int w) {
  StockColumns c;
  c.nameX = 30;
  c.rangeX = w >= 560 ? w - 170 : 0;
  const int right = c.rangeX ? c.rangeX - 24 : w - 4;
  c.changeRight = right;
  c.priceRight = right - 90;
  c.nameEnd = c.priceRight - 80;
  return c;
}

// The change against the previous close: "+0.8 %" (the caller localizes the decimal point); "" when
// there is no previous close.
inline void changeText(float price, float prevClose, char* out, size_t cap) {
  if (prevClose <= 0.0f) {
    if (cap) out[0] = '\0';
    return;
  }
  const float pct = (price - prevClose) / prevClose * 100.0f;
  std::snprintf(out, cap, "%+.1f %%", pct);
}

}  // namespace datacols
