#pragma once
// Status bar (layout ID 128): battery voltage, problems, "Update available", firmware version,
// print and battery icons.
struct LayoutItem;
void drawStatus(LayoutItem* item);
// Problems as a small mark in the bottom-right corner (layouts without the status bar).
void drawProblemCorner();
