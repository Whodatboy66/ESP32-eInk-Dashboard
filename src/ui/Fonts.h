#pragma once
// The fonts used on the screen (sparse bitmap fonts, src/ui/fonts/). Defined once in Fonts.cpp:
// the font headers define their data as const arrays, so including them in every file that
// draws text used to put a copy of each font into the image per file.
#include "fonts/SparseFontBase.h"

extern const SparseGFXfont epaperFont;      // Montserrat 16 px, text
extern const SparseGFXfont MDI_22_Sparse;   // Material Design Icons 22 px (status bar, widgets)
extern const SparseGFXfont MDI_80_Sparse;   // Material Design Icons 80 px (weather)
extern const SparseGFXfont ClockDigits_64;   // Montserrat Bold digits 64 px (small clocks, the 1 x 1 calendar sheet)
extern const SparseGFXfont ClockDigits_110;  // Montserrat Bold digits 110 px (the clock in 2 x 1 and Classic)
extern const SparseGFXfont ClockDigits_180;  // Montserrat Bold digits 180 px (large clocks)
extern const SparseGFXfont Heading_22;       // Montserrat SemiBold 22 px (headings, date under the clock, AM/PM)
extern const SparseGFXfont DigitsBold_16;    // Montserrat Bold 16 px digits (weekend days in the month view)
extern const SparseGFXfont Digits_22;        // Montserrat Medium 22 px digits (the big month view)
extern const SparseGFXfont DigitsBold_22;    // Montserrat Bold 22 px digits (its weekend days)
