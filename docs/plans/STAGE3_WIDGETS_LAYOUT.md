# Stage 3 plan: widget system + layout engine

Detailed plan for stage 3 of [IMPLEMENTATION_PLAN.md](../IMPLEMENTATION_PLAN.md) (spec §5, §6, Phase 3).
Builds on stage 2 ([STAGE2_FOUNDATION.md](STAGE2_FOUNDATION.md)): widgets in `src/widgets/`, drawing
through the `canvas()` seam, wake decisions in `WakePlan`.

**Done when:** widgets and the layout are switched through configuration only (setup page), and all
four templates render correctly on the device.

## Design decisions

1. **Screen model.** 800 × 480. The status bar (28 px, full width) is on every layout and not part
   of the grid. The content area below (800 × 452) is a **4 × 3 grid**: cells of 200 × 150 px
   (the last row 152). Widgets occupy whole cells: 1×1 = 200×150, 2×1 = 400×150, 2×2 = 400×300, …
2. **Static widget registry, no heap.** One `constexpr WidgetDef` table:
   `id, key ("weather"), name, min/max size in cells, refresh interval, isConfigured(config)
   (e.g. a city is set), service (what fetches its data), render(Canvas&, Rect, Theme)`.
   Every widget must draw correctly in any size between its min and max (decision 5).
3. **Templates are data.** A template = a list of placements `{widget, col, row, cols, rows}` in
   priority order. **Classic is the exception:** it keeps today's exact pixel layout (it isn't
   grid-aligned), so existing devices look the same after the update; it is defined in pixels.
4. **Layout engine (pure C++, host-tested):**
   - input: template + which widgets are enabled (configured and switched on);
   - removes disabled widgets, then **fills the gaps**: a neighbor in the same row/column grows into
     freed cells if its max size allows (right/left first, then down/up), otherwise the next
     widget moves up; deterministic, so the same config always gives the same screen;
   - **validates** every result: inside the grid, no overlaps, each widget within its min/max;
   - **fallback:** an invalid or unknown template → Classic; if Classic can't be built (should not
     happen) → clock + status bar only. The reason is logged and shown on the setup page.
   - Output: a list of pixel rectangles per widget. The same function will place custom layouts
     from the phone UI in stage 5.
5. **Size-aware widgets** (what each shows per size):

   | Widget | Min | Sizes and content |
   |---|---|---|
   | Clock | 2×1 | 2×1 large (130 px digits), 4×2 / 4×3 extra large, centred; **compact** variant at 1×1 with half-size digits (downscaled glyphs); 12/24 h; timezone from the setup page |
   | Weather | 1×1 | days = width / 100 px (1×1: 2 days, 2×1: 4, 4×1: 7); taller: sunrise/sunset line |
   | Agenda (next 24 h events) | 2×1 | lines = height / 22 px; flight highlight as today |
   | Month calendar | 2×2 | today's month grid; 4×3 shows larger cells |
   | Stocks | 1×1 | rows = height / 22 px (max 5) |
   | Tracking | 2×1 | one line per parcel, as many as fit |
   | Printer / MakerWorld | 2×1 | as today (printing view or MakerWorld stats) |

   Text that doesn't fit is cut with "..." instead of overflowing into a neighbor; every widget
   clips to its rectangle.
6. **Templates (besides Classic):**

   ```
   Planner (family)              Minimal (desk clock)          Technical
   +---------+---------+         +-------------------+         +---------+---------+
   | Clock   | Weather |         |                   |         | Clock   | Printer |
   +---------+---------+         |   Clock (4x2)     |         +---------+---------+
   | Agenda  | Month   |         |                   |         | Stocks  | Tracking|
   | (2x2)   | (2x2)   |         +---------+---------+         +---------+---------+
   |         |         |         | Weather | Agenda  |         | Weather | Agenda  |
   +---------+---------+         +---------+---------+         +---------+---------+
   ```
   Example of gap filling: Planner without a weather city → the clock grows to 4×1.
7. **Data per service with stale marker and backoff (spec §10.2).** Each service (weather, stocks,
   calendar, tracking, flight, MakerWorld, printer) keeps `lastSuccess, lastAttempt, failures` in
   RTC memory (lost only on a power cut, like today's caches).
   - Due when its interval has passed since the last success (time-based instead of today's
     wake-counter modulo, which drifts after button wakes and restarts).
   - After a failure: retry after 2, 4, 8, … minutes, capped at the interval.
   - **Shared Wi-Fi sessions:** when Wi-Fi is up for one service, others due within the next
     15 minutes come along, so the radio isn't switched on every few minutes.
   - **Stale:** no success for 2 × interval (at least 30 min) → the widget shows its last data
     with a small "old data" mark in its corner (in addition to the status-bar problem name).
8. **Render planning (pure, host-tested).** Per wake: full refresh when the layout or template
   changed, after a button wake, once a day (against ghosting), or after the setup page; otherwise
   partial refreshes of exactly the widgets whose data or minute changed (the clock every minute).
   Same behavior as today for Classic, now as one tested decision instead of scattered flags.
9. **Configuration (setup page, minimal until stage 5):** "Layout" (Classic / Planner / Minimal /
   Technical), "Clock" (24 h / 12 h), and "Show" switches per widget (default: on when its setting
   is filled in, like today). Stored in NVS (`FW_CONFIG_SCHEMA` 2, migrated from 1: defaults =
   Classic, 24 h, everything shown). Changing them redraws fully.
10. **PC renderer for verification.** The widgets and the layout engine compile on the PC against
    Adafruit GFX's `GFXcanvas1` (via the stage 2 canvas seam) with small Arduino shims. A tool
    renders every template with sample data (normal, empty, maximum, long texts, missing data)
    into PNG images under `docs/images/layouts/`. That's how I check "renders correctly" before
    the device, and **Classic is compared pixel by pixel** with the stage 2 rendering. The same
    renderer becomes the phone preview in stage 5.

## Steps

1. PC renderer for the current widgets; golden images of Classic (stage 2 state).
2. Layout model, engine, validation, fallback (pure) + tests.
3. Widget registry; widgets take a `Rect` and adapt; clock 12/24 h + compact.
4. Templates; Classic pixel-identical to the golden images.
5. Service state: time-based due, backoff, shared sessions, stale marker (pure + tests); App uses it.
6. Render planner (pure + tests); App uses it.
7. Config + setup page fields + schema 2 migration.
8. Render all templates × data sets; review the images; fix.
9. Docs (user guide: layouts, widget sizes; "how to add a widget").

## Tests (host)

- Layout: each template with all widgets / none / every single widget off; min/max sizes; gap
  filling rules; invalid templates (overlap, outside the grid, too small, unknown widget) → fallback.
- Widgets: content counts per size (weather days, agenda lines, stock rows), text cut with "...".
- Service state: due, backoff sequence, shared session window, stale threshold, clock jumps (NTP).
- Render planner: every full-refresh reason, partial sets, minute change.
- Config: migration 1 → 2, defaults, invalid values.
- Images: Classic identical to golden; every template image reviewed by eye.

## Risks

| Risk | Counter |
|---|---|
| Widgets were written for one fixed size: overlaps or cut text at other sizes | clipping per widget + rendered images for every size combination |
| The only large font is 130 px digits: no medium size for a compact clock | downscaled glyphs, judged on the rendered image; otherwise compact is dropped |
| Time-based scheduling changes when Wi-Fi turns on (battery) | shared-session window; Wi-Fi sessions per day counted in a test scenario vs. today |
| Partial refreshes over a changed layout leave ghosts | any layout change forces a full refresh (planner rule, tested) |
| Classic changes by accident | pixel comparison with the golden images |

## Out of scope

The phone layout editor and live preview (stage 5), new personal widgets (tasks, notes, birthdays:
stage 4), power profiles (stage 6).

## Result (build 68)

Done in code and rendered on the PC (`docs/images/layouts/`), not yet checked on the device.
Decisions taken during the work:

- **The engine feeds the existing layout table.** `loadLayout()` writes the engine's rectangles into
  `layout[]` with each widget's metrics from the registry, so the wake cycle's fetch / draw logic
  works for every template unchanged, and Classic is identical by construction. Checked: the
  Classic image was **pixel-identical** to the stage 2 rendering after every change (at build 67;
  from 68 on only the version text in the status bar differs).
- **No switch to time-based scheduling** (decision 7 changed): the wake counter already lines all
  hourly fetches up in one Wi-Fi session (the "shared session" the plan wanted). What was missing is
  the retry after a failure (a failed weather download waited 6 hours): now 2, 4, 8, 16, 30 minutes,
  capped at the interval, plus the "old data" mark. Cheaper on the battery than time-based fetches.
- **Compact 1×1 clock dropped:** the only large font has 130 px digits and needs 2 cells of width; a
  downscaled version would be blurry on e-paper. The clock is 2×1 at least, centered in any size.
- **Widget fixes for other sizes,** each keeping the Classic pixels: clock and weather centered
  vertically, weather shows the days that fit, stock columns squeezed below 430 px, every widget
  clipped to its place (`setCanvasClip`), 4 px left margin in grid cells (stock columns need the
  rest of the 400 px).
- **The printer drawing** moved from `src/services/Bambu.cpp` to `src/widgets/Printer.cpp`.
- **Forwarding headers removed** (`display.h`, `fetchAllInfo.h`, `bambulab.h`, `monthView.h`,
  `directFetch.h`); `app.h` stays (settings buffers).
- Name clashes found on the way: the namespace is `layouts` (the firmware has a global `layout[]`),
  and the bit helper is `widgetBit` (Arduino defines a `bit()` macro).

- **Not done from the plan, on purpose:** text is clipped at the widget edge, not cut with "..."
  (decision 5); the render planning (decision 8) is the shared `drawFullScreen()` plus the
  full-refresh rules in the wake cycle, not a separate pure function with its own tests; a fallback
  to Classic is logged, not shown on the setup page (that comes with the stage 5 UI). The layout
  signature in RTC matters once layouts change without a restart (stage 5); today every settings
  change restarts, which is a full refresh anyway.

Review (fresh context) confirmed Classic pixel-identical with its own stage-2 renderer, and the
engine without gaps or fallbacks for all 256 widget combinations in every grid template. Fixed
after it: retries stop after the 4th (2, 4, 8, 16 min), so a permanent error doesn't double the
Wi-Fi sessions (host test: one failing service, one day of wakes); a failed fetch redraws once the
data is old, so the badge actually appears; no recording without real time (power-on before NTP);
the weather fetch asks for as many days as the drawing shows (shared `weatherDaysFor()`); 12-hour
AM/PM centered with the digits, dropped where it can't fit; the badge is a short "old", where it
covers nothing (also in Classic); an empty clip draws nothing; the 1→2 migration stores the layout
defaults; status bar margin in grid layouts.

Known, left for later: agenda / flight times stay 24 h with the 12-hour clock; a 4×2 or larger clock
repaints its whole place every minute (more awake time than Classic's 430×155: limit the partial
window to the digits, stage 6); squeezed stock columns are tight ("19109.20 /18541.60").

Verification: 12 host test files (layout: every template with every combination of widgets, incl.
no gaps; service state incl. a day of a permanently failing service; wake plan retries), all
profiles compile, images reviewed: all four templates, gap filling, 12-hour clock (9:41 and 12:58),
"old" in Planner and Classic.
