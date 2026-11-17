#pragma once
// The fonts used on the screen (sparse bitmap fonts, src/ui/fonts/). Defined once in Fonts.cpp:
// the font headers define their data as const arrays, so including them in every file that
// draws text used to put a copy of each font into the image per file.
#include "fonts/SparseFontBase.h"

extern const SparseGFXfont epaperFont;      // Montserrat 16 px, text
extern const SparseGFXfont MDI_22_Sparse;   // Material Design Icons 22 px (status bar, widgets)
extern const SparseGFXfont MDI_80_Sparse;   // Material Design Icons 80 px (weather)
extern const SparseGFXfont HighSpeed_130;   // 130 px digits (clock)
