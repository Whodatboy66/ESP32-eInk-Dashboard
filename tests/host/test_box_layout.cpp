// Host test for src/layout/BoxLayout.h (custom layouts of the setup app's editor), including the
// test vectors shared with the app (tests/vectors/layouts.json, also run by tests/web/). Run:
// tests/host/run.sh
#include <ArduinoJson.h>

#include <fstream>
#include <sstream>
#include <string>

#include "src/layout/BoxLayout.h"
#include "src/layout/Templates.h"
#include "tests/host/check.h"

using namespace layouts;

static int key(const char* k) { return widgetFromKey(k, std::strlen(k)); }

static void test_parse_and_write() {
  BoxLayout l;
  CHECK(parseBoxes("v1;sb1;0,0,4,1,agenda;0,1,2,2,clock;2,1,2,2,-", l));
  CHECK(l.statusBar);
  CHECK_EQ(l.count, 3);
  CHECK_EQ(l.boxes[0].widget, (int)WidgetId::Agenda);
  CHECK_EQ(l.boxes[2].widget, -1);
  CHECK(validateBoxes(l) == BoxError::None);
  char out[BOX_STRING_MAX];
  CHECK(writeBoxes(l, out, sizeof(out)));
  CHECK_STR(out, "v1;sb1;0,0,4,1,agenda;0,1,2,2,clock;2,1,2,2,-");
  CHECK(parseBoxes("v1;sb0;0,0,4,3,notes", l));
  CHECK(!l.statusBar);
  // Damaged strings: refused (the device falls back to Classic).
  CHECK(!parseBoxes("", l));
  CHECK(!parseBoxes("v2;sb1;0,0,4,3,clock", l));
  CHECK(!parseBoxes("v1;sb1;0,0,4,3,coffee", l));
  CHECK(!parseBoxes("v1;sb1;0,0,4,clock", l));
  CHECK(!parseBoxes("v1;sb1;0,0,4,3,status", l));  // the status bar is not a box widget
  CHECK(!parseBoxes("v1;sb1;0,0,4,3,clock;junk", l));
  std::string many = "v1;sb1";
  for (int i = 0; i < 13; i++) many += ";0,0,1,1,-";
  CHECK(!parseBoxes(many.c_str(), l));
  CHECK(!writeBoxes(l, out, 8));  // doesn't fit
}

static void test_validate_rules() {
  BoxLayout l;
  l.count = 1;
  l.boxes[0] = Box{0, 0, 4, 3, (int8_t)WidgetId::Month};
  CHECK(validateBoxes(l) == BoxError::None);
  l.boxes[0] = Box{0, 0, 4, 2, -1};
  CHECK(validateBoxes(l) == BoxError::Gap);
  l.count = 2;
  l.boxes[0] = Box{0, 0, 4, 2, -1};
  l.boxes[1] = Box{0, 1, 4, 2, -1};
  CHECK(validateBoxes(l) == BoxError::Overlap);
  l.boxes[1] = Box{0, 2, 4, 2, -1};
  CHECK(validateBoxes(l) == BoxError::Outside);
  l.boxes[0] = Box{0, 0, 2, 3, (int8_t)WidgetId::Clock};
  l.boxes[1] = Box{2, 0, 2, 3, (int8_t)WidgetId::Clock};
  CHECK(validateBoxes(l) == BoxError::Duplicate);
  l.boxes[0] = Box{0, 0, 1, 3, (int8_t)WidgetId::Weather};
  l.boxes[1] = Box{1, 0, 3, 3, (int8_t)WidgetId::Agenda};
  CHECK(validateBoxes(l) == BoxError::None);
  l.boxes[0].widget = (int8_t)WidgetId::Clock;  // stage 7: the 64 px digits fit one column
  CHECK(validateBoxes(l) == BoxError::None);
  l.boxes[0].widget = (int8_t)WidgetId::Stocks;  // needs 2 columns
  CHECK(validateBoxes(l) == BoxError::TooSmall);
  l.count = 0;
  CHECK(validateBoxes(l) == BoxError::Empty);
}

static void test_build_without_status_bar_and_clock() {
  BoxLayout l;
  CHECK(parseBoxes("v1;sb0;0,0,4,1,agenda;0,1,2,2,month;2,1,2,2,-", l));
  uint16_t missing = 0;
  Result r = buildBoxes(l, ALL_WIDGETS, &missing);
  CHECK(!r.fellBack);
  CHECK_EQ(r.count, 2);  // no status bar, no clock, the empty box draws nothing
  CHECK_EQ(missing, 0);
  CHECK(r.items[0].id == WidgetId::Agenda);
  CHECK_EQ(r.items[0].rect.y, 0);
  CHECK_EQ(r.items[0].rect.h, 160);  // rows of 160 px without the status bar
  CHECK_EQ(r.items[1].rect.y, 160);
  CHECK_EQ(r.items[1].rect.h, 320);
  CHECK(validate(r));
}

static void test_missing_widget_keeps_its_box() {
  BoxLayout l;
  CHECK(parseBoxes("v1;sb1;0,0,2,1,clock;2,0,2,1,weather;0,1,4,2,agenda", l));
  uint16_t configured = ALL_WIDGETS & ~widgetBit(WidgetId::Weather);
  uint16_t missing = 0;
  Result r = buildBoxes(l, configured, &missing);
  CHECK_EQ(missing, widgetBit(WidgetId::Weather));
  CHECK_EQ(r.count, 3);  // status bar, clock, agenda: no neighbor grows into the weather box
  const Box* b = boxOf(l, WidgetId::Weather);
  CHECK(b != nullptr);
  Rect wr = boxRect(*b, true);
  CHECK_EQ(wr.x, 400);
  CHECK_EQ(wr.w, 400);
}

static void test_invalid_falls_back_to_classic() {
  BoxLayout l;
  l.count = 1;
  l.boxes[0] = Box{0, 0, 4, 2, -1};  // gap
  Result r = buildBoxes(l, ALL_WIDGETS, nullptr);
  CHECK(r.fellBack);
}

static void test_templates_as_boxes() {
  // Every built-in grid template, with every combination of widgets: its boxes are a valid custom
  // layout that draws the same rectangles.
  for (int t = 0; t < TEMPLATE_COUNT; t++) {
    if (TEMPLATES[t].pixels) continue;
    for (uint16_t mask = 0; mask < (1u << WIDGET_COUNT); mask += 7) {
      uint16_t m = mask | widgetBit(WidgetId::Clock) | widgetBit(WidgetId::StatusBar);
      Result r = buildLayout(TEMPLATES[t], m);
      BoxLayout l = boxesFromResult(r);
      CHECK(validateBoxes(l) == BoxError::None);
      Result r2 = buildBoxes(l, m, nullptr);
      CHECK_EQ(r2.count, r.count);
      CHECK_EQ(signature(r2), signature(r));
    }
  }
}

static void test_shared_vectors() {
  std::ifstream f("tests/vectors/layouts.json");
  CHECK(f.good());
  std::stringstream ss;
  ss << f.rdbuf();
  std::string text = ss.str();
  JsonDocument doc;
  CHECK(!deserializeJson(doc, text));
  // The app's widget table must match the firmware's limits.
  for (JsonObjectConst w : doc["widgets"].as<JsonArrayConst>()) {
    int id = key(w["key"] | "");
    CHECK(id > 0);
    if (id <= 0) continue;
    CHECK_EQ(limitsOf((WidgetId)id).minCols, (int)(w["minW"] | 0));
    CHECK_EQ(limitsOf((WidgetId)id).minRows, (int)(w["minH"] | 0));
  }
  int n = 0;
  for (JsonObjectConst v : doc["cases"].as<JsonArrayConst>()) {
    BoxLayout l;
    l.statusBar = v["statusBar"] | true;
    bool parsedOk = true;
    for (JsonObjectConst b : v["boxes"].as<JsonArrayConst>()) {
      if (l.count >= MAX_BOXES) {
        l.count++;  // too many: invalid
        break;
      }
      int widget = -1;
      if (!b["widget"].isNull()) {
        widget = key(b["widget"] | "");
        if (widget < 0) parsedOk = false;
      }
      l.boxes[l.count++] = Box{(uint8_t)(int)b["c"], (uint8_t)(int)b["r"], (uint8_t)(int)b["w"], (uint8_t)(int)b["h"], (int8_t)widget};
    }
    bool valid = parsedOk && l.count <= MAX_BOXES && validateBoxes(l) == BoxError::None;
    bool expected = v["valid"] | false;
    if (valid != expected) std::printf("vector \"%s\": firmware says %s\n", (const char*)(v["name"] | "?"), valid ? "valid" : "invalid");
    CHECK(valid == expected);
    n++;
  }
  CHECK(n >= 12);
}

int main() {
  test_parse_and_write();
  test_validate_rules();
  test_build_without_status_bar_and_clock();
  test_missing_widget_keeps_its_box();
  test_invalid_falls_back_to_classic();
  test_templates_as_boxes();
  test_shared_vectors();
  return finish("box layout");
}
