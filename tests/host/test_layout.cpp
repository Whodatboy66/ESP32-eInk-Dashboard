// Host test for src/layout/LayoutEngine.h and src/layout/Templates.h. Run: tests/host/run.sh
#include "src/layout/LayoutEngine.h"
#include "src/layout/Templates.h"
#include "src/widgets/StatusMarks.h"
#include "tests/host/check.h"

using namespace layouts;

static uint16_t all() { return ALL_WIDGETS; }
static uint16_t without(uint16_t mask, WidgetId id) { return mask & ~widgetBit(id); }

static const Item* find(const Result& r, WidgetId id) {
  for (int i = 0; i < r.count; i++)
    if (r.items[i].id == id) return &r.items[i];
  return nullptr;
}

static bool sameRect(const Rect& a, Rect b) { return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h; }

static void test_grid_geometry() {
  // 4 x 3 cells of 200 x 150 below the 28 px status bar; the last row is 152 px.
  CHECK(sameRect(cellRect({0, 0, 1, 1}), {0, 28, 200, 150}));
  CHECK(sameRect(cellRect({2, 1, 2, 2}), {400, 178, 400, 302}));
  CHECK(sameRect(cellRect({0, 0, 4, 3}), {0, 28, 800, 452}));
}

static void test_status_bar_always_on_top() {
  for (int t = 0; t < TEMPLATE_COUNT; t++) {
    Result r = buildLayout(TEMPLATES[t], all());
    const Item* s = find(r, WidgetId::StatusBar);
    CHECK(s != nullptr);
    // Grid templates: full width; Classic keeps today's bar right of the clock.
    if (s) CHECK(sameRect(s->rect, TEMPLATES[t].pixels ? Rect{440, 0, 360, 28} : STATUS_BAR_RECT));
  }
}

static void test_every_template_is_valid_with_all_widgets() {
  for (int t = 0; t < TEMPLATE_COUNT; t++) {
    Result r = buildLayout(TEMPLATES[t], all());
    CHECK(!r.fellBack);
    CHECK(validate(r));
  }
}

static void test_classic_is_todays_layout() {
  Result r = buildLayout(templateByKey("classic"), all());
  CHECK(sameRect(find(r, WidgetId::Clock)->rect, {10, 0, 430, 155}));
  CHECK(sameRect(find(r, WidgetId::StatusBar)->rect, {440, 0, 360, 28}));
  CHECK(sameRect(find(r, WidgetId::Month)->rect, {440, 25, 360, 230}));
  CHECK(sameRect(find(r, WidgetId::Agenda)->rect, {440, 255, 360, 145}));
  CHECK(sameRect(find(r, WidgetId::Stocks)->rect, {10, 155, 430, 130}));
  CHECK(sameRect(find(r, WidgetId::Weather)->rect, {10, 285, 430, 105}));
  CHECK(sameRect(find(r, WidgetId::Printer)->rect, {10, 390, 430, 90}));
  CHECK(sameRect(find(r, WidgetId::Tracking)->rect, {440, 400, 360, 80}));
  // Classic keeps today's behavior: a disabled widget leaves its place empty.
  Result r2 = buildLayout(templateByKey("classic"), without(all(), WidgetId::Stocks));
  CHECK(find(r2, WidgetId::Stocks) == nullptr);
  CHECK(sameRect(find(r2, WidgetId::Weather)->rect, {10, 285, 430, 105}));
}

static void test_planner_with_all_widgets() {
  Result r = buildLayout(templateByKey("planner"), all());
  CHECK(sameRect(find(r, WidgetId::Clock)->rect, cellRect({0, 0, 2, 1})));
  CHECK(sameRect(find(r, WidgetId::Weather)->rect, cellRect({2, 0, 2, 1})));
  CHECK(sameRect(find(r, WidgetId::Agenda)->rect, cellRect({0, 1, 2, 2})));
  CHECK(sameRect(find(r, WidgetId::Month)->rect, cellRect({2, 1, 2, 2})));
  CHECK(find(r, WidgetId::Stocks) == nullptr);  // not part of this template
}

static void test_gap_filled_by_the_neighbor_in_the_row() {
  // Planner without a weather city: the clock grows to the full width.
  Result r = buildLayout(templateByKey("planner"), without(all(), WidgetId::Weather));
  CHECK(sameRect(find(r, WidgetId::Clock)->rect, cellRect({0, 0, 4, 1})));
  CHECK(validate(r));
}

static void test_gap_filled_downwards() {
  // Minimal without weather and agenda: the clock fills the whole screen.
  Result r = buildLayout(templateByKey("minimal"), without(without(all(), WidgetId::Weather), WidgetId::Agenda));
  CHECK(sameRect(find(r, WidgetId::Clock)->rect, cellRect({0, 0, 4, 3})));
}

static void test_no_gaps_left_in_grid_templates() {
  // Every single widget switched off, in every grid template: the content area stays covered.
  for (int t = 0; t < TEMPLATE_COUNT; t++) {
    if (TEMPLATES[t].pixels) continue;
    for (int w = 0; w < WIDGET_COUNT; w++) {
      if (static_cast<WidgetId>(w) == WidgetId::Clock || static_cast<WidgetId>(w) == WidgetId::StatusBar) continue;
      Result r = buildLayout(TEMPLATES[t], without(all(), static_cast<WidgetId>(w)));
      CHECK(validate(r));
      CHECK_EQ(coveredCells(r), GRID_COLS * GRID_ROWS);
    }
  }
}

static void test_only_clock() {
  for (int t = 0; t < TEMPLATE_COUNT; t++) {
    if (TEMPLATES[t].pixels) continue;
    Result r = buildLayout(TEMPLATES[t], widgetBit(WidgetId::Clock) | widgetBit(WidgetId::StatusBar));
    CHECK(sameRect(find(r, WidgetId::Clock)->rect, cellRect({0, 0, 4, 3})));
  }
}

static void test_min_and_max_sizes_respected() {
  for (int t = 0; t < TEMPLATE_COUNT; t++) {
    if (TEMPLATES[t].pixels) continue;
    for (uint16_t mask = 0; mask < (1u << WIDGET_COUNT); mask++) {
      uint16_t m = mask | widgetBit(WidgetId::Clock) | widgetBit(WidgetId::StatusBar);
      Result r = buildLayout(TEMPLATES[t], m);
      CHECK(validate(r));  // validate() checks bounds, overlaps, min/max per widget
      CHECK(!r.fellBack);
      CHECK_EQ(coveredCells(r), GRID_COLS * GRID_ROWS);  // no gaps, whatever is switched off
    }
  }
}

static void test_invalid_template_falls_back_to_classic() {
  static const Placement overlapping[] = {{WidgetId::Clock, {0, 0, 2, 2}}, {WidgetId::Weather, {1, 1, 2, 1}}};
  Template bad{"bad", "Bad", overlapping, 2, nullptr, 0};
  Result r = buildLayout(bad, all());
  CHECK(r.fellBack);
  CHECK(sameRect(find(r, WidgetId::Clock)->rect, {10, 0, 430, 155}));  // Classic

  static const Placement outside[] = {{WidgetId::Clock, {3, 0, 2, 1}}};
  Template bad2{"bad2", "Bad2", outside, 1, nullptr, 0};
  CHECK(buildLayout(bad2, all()).fellBack);

  static const Placement tooSmall[] = {{WidgetId::Stocks, {0, 0, 1, 1}}, {WidgetId::Clock, {1, 0, 3, 3}},
                                       {WidgetId::Weather, {0, 1, 1, 2}}};
  Template bad3{"bad3", "Bad3", tooSmall, 3, nullptr, 0};
  CHECK(buildLayout(bad3, all()).fellBack);  // stocks need 2 cells of width (the month: 1 x 1 since stage 7)
}

static void test_unknown_template_key() {
  CHECK(templateByKey("nonsense").key == templateByKey("classic").key);
  CHECK(templateIndex("technical") >= 0);
  CHECK_EQ(templateIndex("nonsense"), -1);
}

static void test_signature_changes_with_layout() {
  Result a = buildLayout(templateByKey("planner"), all());
  Result b = buildLayout(templateByKey("planner"), without(all(), WidgetId::Weather));
  Result c = buildLayout(templateByKey("planner"), all());
  CHECK(signature(a) != signature(b));
  CHECK_EQ(signature(a), signature(c));
}

static void test_weather_days_for_width() {
  // One day per 100 px column; the last one only needs its 80 px icon.
  CHECK_EQ(weatherDaysFor(430, 100), 4);  // Classic
  CHECK_EQ(weatherDaysFor(396, 100), 4);  // 2x1 cell minus the margin
  CHECK_EQ(weatherDaysFor(196, 100), 2);  // 1x1
  CHECK_EQ(weatherDaysFor(796, 100), 8);  // 4x1 (the fetch caps at 7)
  CHECK_EQ(weatherDaysFor(50, 100), 1);
}

// Stage 7: list widgets get a heading in the grid layouts, not in Classic (its places stay) and
// not in boxes too low for a heading and some lines.
static void test_heading_rule() {
  CHECK(!layouts::wantsHeading(true, 145));   // Classic's agenda
  CHECK(layouts::wantsHeading(false, 150));   // a 2 x 1 grid box
  CHECK(layouts::wantsHeading(false, 100));
  CHECK(!layouts::wantsHeading(false, 90));
}

// Stage 7: Wi-Fi bars from the signal of the last connection; the space for a holiday's name.
static void test_status_marks() {
  CHECK_EQ(statusbar::signalBars(0), 0);     // unknown: no icon
  CHECK_EQ(statusbar::signalBars(-50), 4);
  CHECK_EQ(statusbar::signalBars(-55), 4);
  CHECK_EQ(statusbar::signalBars(-62), 3);
  CHECK_EQ(statusbar::signalBars(-72), 2);
  CHECK_EQ(statusbar::signalBars(-88), 1);
  CHECK_EQ(statusbar::signalBars(-50, true), 0);  // the last connection failed: no bars next to "! WLAN"
  CHECK_EQ(statusbar::signalBars(-50, false), 4);
  // The holiday name gets what is left between the date and the right part, after the power note.
  CHECK_EQ(statusbar::holidaySpace(100, 0, 600), 492);
  CHECK_EQ(statusbar::holidaySpace(100, 150, 600), 330);
  CHECK_EQ(statusbar::holidaySpace(100, 450, 600), 0);  // too little: no name rather than overlap
}

int main() {
  test_status_marks();
  test_heading_rule();
  test_weather_days_for_width();
  test_grid_geometry();
  test_status_bar_always_on_top();
  test_every_template_is_valid_with_all_widgets();
  test_classic_is_todays_layout();
  test_planner_with_all_widgets();
  test_gap_filled_by_the_neighbor_in_the_row();
  test_gap_filled_downwards();
  test_no_gaps_left_in_grid_templates();
  test_only_clock();
  test_min_and_max_sizes_respected();
  test_invalid_template_falls_back_to_classic();
  test_unknown_template_key();
  test_signature_changes_with_layout();
  return finish("layout");
}
