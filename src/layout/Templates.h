#pragma once
// The built-in layout templates (docs/plans/STAGE3_WIDGETS_LAYOUT.md, decision 6).
//
//   Planner (family)        Minimal (desk clock)       Technical
//   +---------+---------+   +-------------------+      +---------+---------+
//   | Clock   | Weather |   |                   |      | Clock   | Printer |
//   +---------+---------+   |   Clock (4x2)     |      +---------+---------+
//   | Agenda  | Month   |   |                   |      | Stocks  | Tracking|
//   | (2x2)   | (2x2)   |   +---------+---------+      +---------+---------+
//   |         |         |   | Weather | Agenda  |      | Weather | Agenda  |
//   +---------+---------+   +---------+---------+      +---------+---------+
//
// Classic is today's fixed layout, in pixels (not grid-aligned), so existing devices look the same.
#include "LayoutEngine.h"

namespace layouts {

namespace templates {

inline const PixelItem CLASSIC[] = {
    {WidgetId::Clock, {10, 0, 430, 155}},
    {WidgetId::StatusBar, {440, 0, 360, 28}},
    {WidgetId::Month, {440, 25, 360, 230}},
    {WidgetId::Agenda, {440, 255, 360, 145}},
    {WidgetId::Stocks, {10, 155, 430, 130}},
    {WidgetId::Weather, {10, 285, 430, 105}},
    {WidgetId::Printer, {10, 390, 430, 90}},
    {WidgetId::Tracking, {440, 400, 360, 80}},
};
inline const Placement PLANNER[] = {
    {WidgetId::Clock, {0, 0, 2, 1}},
    {WidgetId::Weather, {2, 0, 2, 1}},
    {WidgetId::Agenda, {0, 1, 2, 2}},
    {WidgetId::Month, {2, 1, 2, 2}},
};
inline const Placement MINIMAL[] = {
    {WidgetId::Clock, {0, 0, 4, 2}},
    {WidgetId::Weather, {0, 2, 2, 1}},
    {WidgetId::Agenda, {2, 2, 2, 1}},
};
inline const Placement TECHNICAL[] = {
    {WidgetId::Clock, {0, 0, 2, 1}},
    {WidgetId::Printer, {2, 0, 2, 1}},
    {WidgetId::Stocks, {0, 1, 2, 1}},
    {WidgetId::Tracking, {2, 1, 2, 1}},
    {WidgetId::Weather, {0, 2, 2, 1}},
    {WidgetId::Agenda, {2, 2, 2, 1}},
};

}  // namespace templates

inline const Template TEMPLATES[] = {
    {"classic", "Classic", nullptr, 0, templates::CLASSIC, sizeof(templates::CLASSIC) / sizeof(PixelItem)},
    {"planner", "Planner", templates::PLANNER, sizeof(templates::PLANNER) / sizeof(Placement), nullptr, 0},
    {"minimal", "Minimal", templates::MINIMAL, sizeof(templates::MINIMAL) / sizeof(Placement), nullptr, 0},
    {"technical", "Technical", templates::TECHNICAL, sizeof(templates::TECHNICAL) / sizeof(Placement), nullptr, 0},
};
constexpr int TEMPLATE_COUNT = sizeof(TEMPLATES) / sizeof(TEMPLATES[0]);

inline const Template& classicTemplate() { return TEMPLATES[0]; }

// Index of the template with this key, -1 if there is none.
inline int templateIndex(const char* key) {
  for (int i = 0; i < TEMPLATE_COUNT; i++)
    if (key && std::strcmp(TEMPLATES[i].key, key) == 0) return i;
  return -1;
}

// The template with this key; Classic for an unknown key.
inline const Template& templateByKey(const char* key) {
  int i = templateIndex(key);
  return TEMPLATES[i < 0 ? 0 : i];
}

}  // namespace layouts
