#pragma once
// Layout engine (stage 3, docs/plans/STAGE3_WIDGETS_LAYOUT.md): places the enabled widgets of a
// template on the 800 x 480 screen. Grid templates use 4 x 3 cells below a full-width status bar;
// disabled widgets leave no gaps (neighbors grow into the free cells); every result is validated,
// an invalid template falls back to Classic. Classic is defined in pixels (today's exact layout).
// Plain C++, tested on the PC (tests/host/test_layout.cpp).
#include <cstdint>
#include <cstring>

namespace layouts {

enum class WidgetId : uint8_t { StatusBar, Clock, Weather, Agenda, Month, Stocks, Tracking, Printer, Tasks, Notes, Birthdays };
constexpr int WIDGET_COUNT = 11;
constexpr uint16_t widgetBit(WidgetId id) { return static_cast<uint16_t>(1u << static_cast<int>(id)); }
constexpr uint16_t ALL_WIDGETS = static_cast<uint16_t>((1u << WIDGET_COUNT) - 1);

constexpr int SCREEN_W = 800, SCREEN_H = 480;
constexpr int STATUS_H = 28;
constexpr int GRID_COLS = 4, GRID_ROWS = 3;
constexpr int CELL_W = SCREEN_W / GRID_COLS;                   // 200
constexpr int CELL_H = (SCREEN_H - STATUS_H) / GRID_ROWS;      // 150 (the last row takes the rest)

struct Rect {
  int16_t x, y, w, h;
};
constexpr Rect STATUS_BAR_RECT{0, 0, SCREEN_W, STATUS_H};

struct Span {
  uint8_t col, row, cols, rows;
};

// Size range of a widget in cells (what it can draw well).
struct Limits {
  uint8_t minCols, minRows, maxCols, maxRows;
};

inline const Limits& limitsOf(WidgetId id) {
  static const Limits table[WIDGET_COUNT] = {
      {4, 1, 4, 1},  // StatusBar (not in the grid)
      {2, 1, 4, 3},  // Clock: the 130 px digits need 2 cells of width
      {1, 1, 4, 3},  // Weather: one day per 100 px
      {2, 1, 4, 3},  // Agenda
      {2, 2, 4, 3},  // Month: header + 6 week rows
      {2, 1, 4, 3},  // Stocks: fixed columns up to 400 px
      {2, 1, 4, 3},  // Tracking
      {2, 1, 4, 3},  // Printer / MakerWorld: fixed columns up to 400 px
      {2, 1, 4, 3},  // Tasks: checkbox, title, due date
      {1, 1, 4, 3},  // Note: wrapped text
      {2, 1, 4, 3},  // Birthdays: label, name, age
  };
  return table[static_cast<int>(id)];
}

struct Placement {
  WidgetId id;
  Span span;
};
struct PixelItem {
  WidgetId id;
  Rect rect;
};
// A template: grid placements in priority order, or (Classic) fixed pixel rectangles.
struct Template {
  const char* key;
  const char* name;
  const Placement* cells;
  uint8_t cellCount;
  const PixelItem* pixels;
  uint8_t pixelCount;
};

struct Item {
  WidgetId id;
  Rect rect;
  Span span;   // grid templates only
  bool grid;
};
struct Result {
  Item items[WIDGET_COUNT] = {};
  int count = 0;
  bool fellBack = false;  // the template was invalid: Classic was used
};

inline Rect cellRect(Span s) {
  int16_t x = static_cast<int16_t>(s.col * CELL_W);
  int16_t y = static_cast<int16_t>(STATUS_H + s.row * CELL_H);
  int16_t w = static_cast<int16_t>(s.cols * CELL_W);
  int16_t h = static_cast<int16_t>(s.rows * CELL_H);
  if (s.row + s.rows == GRID_ROWS) h = static_cast<int16_t>(SCREEN_H - y);  // the last row takes the rest
  return Rect{x, y, w, h};
}

inline const Template& classicTemplate();  // Templates.h

// Weather days that fit a width: one per column, the last one only needs its 80 px icon.
// Used by the fetch (how many days to ask for) and the drawing, so both agree.
inline int weatherDaysFor(int width, int colWidth) {
  if (colWidth <= 0) return 1;
  int days = (width - 80) / colWidth + 1;
  return days < 1 ? 1 : days;
}

namespace engine {

inline bool inGrid(const Span& s) {
  return s.cols >= 1 && s.rows >= 1 && s.col + s.cols <= GRID_COLS && s.row + s.rows <= GRID_ROWS;
}

inline bool withinLimits(WidgetId id, const Span& s) {
  const Limits& l = limitsOf(id);
  return s.cols >= l.minCols && s.rows >= l.minRows && s.cols <= l.maxCols && s.rows <= l.maxRows;
}

struct Grid {
  int8_t cell[GRID_ROWS][GRID_COLS];
  Grid() { std::memset(cell, -1, sizeof(cell)); }
  bool free(int col, int row, int cols, int rows) const {
    for (int r = row; r < row + rows; r++)
      for (int c = col; c < col + cols; c++)
        if (c < 0 || r < 0 || c >= GRID_COLS || r >= GRID_ROWS || cell[r][c] != -1) return false;
    return true;
  }
  void mark(const Span& s, int8_t v) {
    for (int r = s.row; r < s.row + s.rows; r++)
      for (int c = s.col; c < s.col + s.cols; c++) cell[r][c] = v;
  }
};

// One growth step into free cells: right, left, down, up (in that order).
inline bool grow(Grid& g, Item& it, int8_t idx) {
  const Limits& l = limitsOf(it.id);
  Span& s = it.span;
  if (s.cols < l.maxCols && g.free(s.col + s.cols, s.row, 1, s.rows)) {
    s.cols++;
  } else if (s.cols < l.maxCols && g.free(s.col - 1, s.row, 1, s.rows)) {
    s.col--;
    s.cols++;
  } else if (s.rows < l.maxRows && g.free(s.col, s.row + s.rows, s.cols, 1)) {
    s.rows++;
  } else if (s.rows < l.maxRows && g.free(s.col, s.row - 1, s.cols, 1)) {
    s.row--;
    s.rows++;
  } else {
    return false;
  }
  g.mark(s, idx);
  return true;
}

inline Result fromPixels(const Template& t, uint16_t enabled) {
  Result r;
  for (int i = 0; i < t.pixelCount && r.count < WIDGET_COUNT; i++) {
    if (!(enabled & widgetBit(t.pixels[i].id))) continue;
    r.items[r.count++] = Item{t.pixels[i].id, t.pixels[i].rect, Span{0, 0, 0, 0}, false};
  }
  return r;
}

}  // namespace engine

// Checks a result: every grid widget inside the grid, within its size limits, no overlaps;
// pixel items inside the screen; each widget at most once.
inline bool validate(const Result& r) {
  engine::Grid g;
  uint16_t seen = 0;
  for (int i = 0; i < r.count; i++) {
    const Item& it = r.items[i];
    if (seen & widgetBit(it.id)) return false;
    seen |= widgetBit(it.id);
    if (it.rect.x < 0 || it.rect.y < 0 || it.rect.w <= 0 || it.rect.h <= 0 ||
        it.rect.x + it.rect.w > SCREEN_W || it.rect.y + it.rect.h > SCREEN_H) {
      return false;
    }
    if (!it.grid) continue;
    if (!engine::inGrid(it.span) || !engine::withinLimits(it.id, it.span)) return false;
    if (!g.free(it.span.col, it.span.row, it.span.cols, it.span.rows)) return false;
    g.mark(it.span, static_cast<int8_t>(i));
  }
  return true;
}

// Grid cells covered by widgets (12 = no gaps).
inline int coveredCells(const Result& r) {
  int n = 0;
  for (int i = 0; i < r.count; i++)
    if (r.items[i].grid) n += r.items[i].span.cols * r.items[i].span.rows;
  return n;
}

// Builds the layout of `t` for the enabled widgets (widgetBit(WidgetId) mask).
inline Result buildLayout(const Template& t, uint16_t enabled) {
  if (t.pixels) {
    Result r = engine::fromPixels(t, enabled);
    if (validate(r)) return r;
    return Result{};  // Classic itself is broken: nothing sensible to fall back to
  }
  Result r;
  if (enabled & widgetBit(WidgetId::StatusBar))
    r.items[r.count++] = Item{WidgetId::StatusBar, STATUS_BAR_RECT, Span{0, 0, 0, 0}, false};
  engine::Grid g;
  bool ok = true;
  for (int i = 0; i < t.cellCount && ok; i++) {
    const Placement& p = t.cells[i];
    if (!(enabled & widgetBit(p.id)) || p.id == WidgetId::StatusBar) continue;
    if (!engine::inGrid(p.span) || !engine::withinLimits(p.id, p.span) ||
        !g.free(p.span.col, p.span.row, p.span.cols, p.span.rows) || r.count >= WIDGET_COUNT) {
      ok = false;
      break;
    }
    g.mark(p.span, static_cast<int8_t>(r.count));
    r.items[r.count++] = Item{p.id, cellRect(p.span), p.span, true};
  }
  if (ok) {
    // Fill the gaps: neighbors grow into free cells, one step at a time, in template order.
    bool grew = true;
    while (grew) {
      grew = false;
      for (int i = 0; i < r.count; i++)
        if (r.items[i].grid && engine::grow(g, r.items[i], static_cast<int8_t>(i))) grew = true;
    }
    for (int i = 0; i < r.count; i++)
      if (r.items[i].grid) r.items[i].rect = cellRect(r.items[i].span);
    if (validate(r)) return r;
  }
  Result fallback = buildLayout(classicTemplate(), enabled);
  fallback.fellBack = true;
  return fallback;
}

// Identifies a layout (template + which widgets where): a change means a full refresh.
inline uint32_t signature(const Result& r) {
  uint32_t h = 2166136261u;  // FNV-1a
  auto add = [&h](uint32_t v) {
    for (int i = 0; i < 4; i++) {
      h ^= (v >> (8 * i)) & 0xFF;
      h *= 16777619u;
    }
  };
  for (int i = 0; i < r.count; i++) {
    const Item& it = r.items[i];
    add(static_cast<uint32_t>(it.id));
    add(static_cast<uint32_t>(static_cast<uint16_t>(it.rect.x)) << 16 | static_cast<uint16_t>(it.rect.y));
    add(static_cast<uint32_t>(static_cast<uint16_t>(it.rect.w)) << 16 | static_cast<uint16_t>(it.rect.h));
  }
  return h;
}

}  // namespace layouts

// The templates (and classicTemplate(), used by buildLayout) belong to the engine: always included.
#include "Templates.h"
