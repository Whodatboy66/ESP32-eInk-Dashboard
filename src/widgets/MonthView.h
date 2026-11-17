#pragma once
#include "../../app.h"
#include "Widgets.h"

// Month grid widget (layout ID 256). The ICS calendar feeds are fetched in directFetch.cpp.
int daysInMonth(int year, int month);
void drawCalendar(LayoutItem* item);
