#pragma once
#include "../../app.h"
#include "Widgets.h"

// Month grid widget (layout ID 256). The ICS calendar feeds are fetched in src/services/Calendar.cpp.
int daysInMonth(int year, int month);
void drawCalendar(LayoutItem* item);
// The 9 x 9 holiday star of the month view (also used by the agenda), top-left corner at (x, y).
void drawHolidayStar(int x, int y);
