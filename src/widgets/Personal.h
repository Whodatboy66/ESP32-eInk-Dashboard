#pragma once
// The personal widgets (stage 4, docs/plans/STAGE4_PERSONAL_WIDGETS.md): tasks, the note,
// birthdays. Data from src/personal/UserStore.h (local, no download).
#include <stdint.h>

struct LayoutItem;

void tasksWidget(LayoutItem* item);
void noteWidget(LayoutItem* item);
void birthdaysWidget(LayoutItem* item);

// Draws text cut to maxWidth pixels, with "…" when it was cut.
void drawFittedString(int16_t x, int16_t y, int maxWidth, const char* text, uint16_t color);
