// Host test for src/widgets/DataColumns.h (stage 8 part B, the look pass): the stocks box's columns
// come from its width (they were fixed for a 430 px place). Run: tests/host/run.sh
#include <initializer_list>

#include "src/widgets/DataColumns.h"
#include "tests/host/check.h"

namespace {

void test_stock_columns() {
  for (int w : {396, 430, 596, 796}) {
    const datacols::StockColumns c = datacols::stockColumns(w);
    CHECK(c.nameX < c.nameEnd && c.nameEnd < c.priceRight && c.priceRight < c.changeRight && c.changeRight <= w);
    CHECK(c.nameEnd - c.nameX >= 100);  // "MSCI World" fits
    if (c.rangeX) CHECK(c.rangeX > c.changeRight && c.rangeX + 120 <= w);
  }
  CHECK_EQ(datacols::stockColumns(396).rangeX, 0);  // half a screen: no day range
  CHECK(datacols::stockColumns(796).rangeX > 0);    // full width: with it
}

void test_change_text() {
  char b[16];
  datacols::changeText(100.0f, 99.2f, b, sizeof(b));
  CHECK_STR(b, "+0.8 %");
  datacols::changeText(98.0f, 100.0f, b, sizeof(b));
  CHECK_STR(b, "-2.0 %");
  datacols::changeText(5.0f, 0.0f, b, sizeof(b));  // no previous close
  CHECK_STR(b, "");
}

}  // namespace

int main() {
  test_stock_columns();
  test_change_text();
  return finish("data columns");
}
