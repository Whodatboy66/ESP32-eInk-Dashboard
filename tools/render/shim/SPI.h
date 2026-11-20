#pragma once
// PC stand-in: SPI is not used when rendering.
struct SPIClass {
  void begin(int = -1, int = -1, int = -1, int = -1) {}
};
inline SPIClass SPI;
