#pragma once
// Custom layouts (stage 5, docs/plans/STAGE5_SETUP_APP.md decisions 6 and 7): boxes on the 4 x 3
// grid, made with the setup app's editor. Stored as a short string (NVS key "lay_box"):
//
//   v1;sb1;0,0,4,1,agenda;0,1,2,2,clock;2,1,2,2,-
//
// "sb1" / "sb0" = status bar on / off; each box "col,row,cols,rows,widget" ("-" = empty).
// Unlike the built-in templates, empty boxes stay empty (no gap filling), the clock and the status
// bar are optional, and a widget whose settings are missing keeps its box (it shows a hint).
// Plain C++, tested on the PC (tests/host/test_box_layout.cpp, tests/vectors/layouts.json).
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "LayoutEngine.h"

namespace layouts {

constexpr int MAX_BOXES = GRID_COLS * GRID_ROWS;
constexpr size_t BOX_STRING_MAX = 200;

struct Box {
  uint8_t c, r, w, h;
  int8_t widget;  // WidgetId, -1 = empty
};

struct BoxLayout {
  bool statusBar = true;
  uint8_t count = 0;
  Box boxes[MAX_BOXES] = {};
};

// Widget key -> id (-1 if unknown). The status bar is not a box widget.
inline int widgetFromKey(const char* key, size_t len) {
  for (int i = 1; i < WIDGET_COUNT; i++)
    if (std::strlen(WIDGET_KEYS[i]) == len && std::strncmp(WIDGET_KEYS[i], key, len) == 0) return i;
  return -1;
}

enum class BoxError : uint8_t { None, Empty, TooMany, Outside, Overlap, Gap, Duplicate, TooSmall, UnknownWidget };

inline const char* boxErrorText(BoxError e) {
  switch (e) {
    case BoxError::None: return "";
    case BoxError::Empty: return "The layout has no boxes.";
    case BoxError::TooMany: return "At most 12 boxes.";
    case BoxError::Outside: return "A box reaches outside the screen.";
    case BoxError::Overlap: return "Two boxes overlap.";
    case BoxError::Gap: return "Part of the screen is not covered by a box.";
    case BoxError::Duplicate: return "A widget can be shown only once.";
    case BoxError::TooSmall: return "A widget is too large for its box.";
    case BoxError::UnknownWidget: return "Unknown widget.";
  }
  return "Invalid layout.";
}

// Inside the grid, no overlaps, all 12 cells covered, each widget once and within its minimum size.
inline BoxError validateBoxes(const BoxLayout& l) {
  if (l.count == 0) return BoxError::Empty;
  if (l.count > MAX_BOXES) return BoxError::TooMany;
  int8_t cell[GRID_ROWS][GRID_COLS];
  std::memset(cell, -1, sizeof(cell));
  uint16_t used = 0;
  for (int i = 0; i < l.count; i++) {
    const Box& b = l.boxes[i];
    if (b.w < 1 || b.h < 1 || b.c + b.w > GRID_COLS || b.r + b.h > GRID_ROWS) return BoxError::Outside;
    for (int r = b.r; r < b.r + b.h; r++)
      for (int c = b.c; c < b.c + b.w; c++) {
        if (cell[r][c] != -1) return BoxError::Overlap;
        cell[r][c] = static_cast<int8_t>(i);
      }
  }
  for (int r = 0; r < GRID_ROWS; r++)
    for (int c = 0; c < GRID_COLS; c++)
      if (cell[r][c] == -1) return BoxError::Gap;
  for (int i = 0; i < l.count; i++) {
    const Box& b = l.boxes[i];
    if (b.widget < 0) continue;
    if (b.widget == 0 || b.widget >= WIDGET_COUNT) return BoxError::UnknownWidget;
    WidgetId id = static_cast<WidgetId>(b.widget);
    if (used & widgetBit(id)) return BoxError::Duplicate;
    used |= widgetBit(id);
    const Limits& lim = limitsOf(id);
    if (b.w < lim.minCols || b.h < lim.minRows) return BoxError::TooSmall;
  }
  return BoxError::None;
}

// Parses the stored string; false if it isn't one (the caller falls back to Classic).
inline bool parseBoxes(const char* s, BoxLayout& out) {
  out = BoxLayout{};
  if (!s || std::strncmp(s, "v1;", 3) != 0) return false;
  s += 3;
  if (std::strncmp(s, "sb1", 3) == 0) out.statusBar = true;
  else if (std::strncmp(s, "sb0", 3) == 0) out.statusBar = false;
  else return false;
  s += 3;
  while (*s == ';') {
    s++;
    if (!*s) break;
    if (out.count >= MAX_BOXES) return false;
    int v[4];
    for (int k = 0; k < 4; k++) {
      char* end;
      long n = std::strtol(s, &end, 10);
      if (end == s || *end != ',' || n < 0 || n > 9) return false;
      v[k] = static_cast<int>(n);
      s = end + 1;
    }
    size_t len = std::strcspn(s, ";");
    int widget;
    if (len == 1 && *s == '-') widget = -1;
    else if ((widget = widgetFromKey(s, len)) < 0) return false;
    out.boxes[out.count++] = Box{static_cast<uint8_t>(v[0]), static_cast<uint8_t>(v[1]), static_cast<uint8_t>(v[2]),
                                 static_cast<uint8_t>(v[3]), static_cast<int8_t>(widget)};
    s += len;
  }
  return *s == '\0';
}

inline bool writeBoxes(const BoxLayout& l, char* out, size_t cap) {
  int n = std::snprintf(out, cap, "v1;sb%d", l.statusBar ? 1 : 0);
  for (int i = 0; i < l.count && n > 0 && static_cast<size_t>(n) < cap; i++) {
    const Box& b = l.boxes[i];
    n += std::snprintf(out + n, cap - n, ";%d,%d,%d,%d,%s", b.c, b.r, b.w, b.h,
                       b.widget < 0 ? "-" : WIDGET_KEYS[b.widget]);
  }
  return n > 0 && static_cast<size_t>(n) < cap;
}

// Pixel rectangle of a box: below the status bar, or from the top without it (rows of 160 px).
inline Rect boxRect(const Box& b, bool statusBar) {
  const int top = statusBar ? STATUS_H : 0;
  const int cellH = (SCREEN_H - top) / GRID_ROWS;
  int16_t x = static_cast<int16_t>(b.c * CELL_W);
  int16_t y = static_cast<int16_t>(top + b.r * cellH);
  int16_t w = static_cast<int16_t>(b.w * CELL_W);
  int16_t h = static_cast<int16_t>(b.r + b.h == GRID_ROWS ? SCREEN_H - y : b.h * cellH);
  return Rect{x, y, w, h};
}

// The layout to draw: the status bar (if on) and one item per box with a widget. Widgets whose
// settings are missing (not in `configured`) are left out of the result and reported in *missing
// (their boxes show a hint). An invalid layout falls back to Classic (fellBack).
inline Result buildBoxes(const BoxLayout& l, uint16_t configured, uint16_t* missing) {
  if (missing) *missing = 0;
  if (validateBoxes(l) != BoxError::None) {
    Result fb = buildLayout(classicTemplate(), configured);
    fb.fellBack = true;
    return fb;
  }
  Result r;
  if (l.statusBar) r.items[r.count++] = Item{WidgetId::StatusBar, STATUS_BAR_RECT, Span{0, 0, 0, 0}, false};
  for (int i = 0; i < l.count && r.count < WIDGET_COUNT; i++) {
    const Box& b = l.boxes[i];
    if (b.widget < 0) continue;
    WidgetId id = static_cast<WidgetId>(b.widget);
    if (!(configured & widgetBit(id))) {
      if (missing) *missing |= widgetBit(id);
      continue;
    }
    r.items[r.count++] = Item{id, boxRect(b, l.statusBar), Span{b.c, b.r, b.w, b.h}, true};
  }
  return r;
}

// The box of a widget in a layout (for its hint), nullptr if it has none.
inline const Box* boxOf(const BoxLayout& l, WidgetId id) {
  for (int i = 0; i < l.count; i++)
    if (l.boxes[i].widget == static_cast<int8_t>(id)) return &l.boxes[i];
  return nullptr;
}

// A built-in grid template as boxes (the editor starts from it): its result after gap filling;
// cells it leaves free become empty 1 x 1 boxes.
inline BoxLayout boxesFromResult(const Result& r) {
  BoxLayout l;
  l.statusBar = false;
  bool covered[GRID_ROWS][GRID_COLS] = {};
  for (int i = 0; i < r.count; i++) {
    const Item& it = r.items[i];
    if (it.id == WidgetId::StatusBar) {
      l.statusBar = true;
      continue;
    }
    if (!it.grid || l.count >= MAX_BOXES) continue;
    l.boxes[l.count++] = Box{it.span.col, it.span.row, it.span.cols, it.span.rows, static_cast<int8_t>(it.id)};
    for (int rr = it.span.row; rr < it.span.row + it.span.rows; rr++)
      for (int c = it.span.col; c < it.span.col + it.span.cols; c++) covered[rr][c] = true;
  }
  for (int rr = 0; rr < GRID_ROWS; rr++)
    for (int c = 0; c < GRID_COLS; c++)
      if (!covered[rr][c] && l.count < MAX_BOXES)
        l.boxes[l.count++] = Box{static_cast<uint8_t>(c), static_cast<uint8_t>(rr), 1, 1, -1};
  return l;
}

}  // namespace layouts
