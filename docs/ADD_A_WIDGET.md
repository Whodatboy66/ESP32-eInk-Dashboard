# How to add a widget

The screen is built from widgets placed by the layout engine (`src/layout/`). A widget is a
drawing function plus a few facts the engine and the wake cycle need.

1. **Data.** A fetch function in `src/services/` (verified HTTPS through `SecureClient`, see
   `HttpJson.h`) that fills a data struct (`src/widgets/Widgets.h`). If it can fail, give it a
   `Service` in `src/core/ServiceState.h` for retries and the "old data" mark. Data typed in by the
   user (like tasks) goes into `src/personal/` (stored with `UserStore`) with a route in
   `src/web/Api.cpp` ([SETUP_API.md](SETUP_API.md)).
2. **Drawing.** `void myWidget(LayoutItem* item)` in `src/widgets/`: draw with `canvas()` and the
   sparse fonts (`src/ui/SparseFont.h`) inside `item->PosX/PosY/Width/Height`. Drawing is clipped
   to that rectangle; use the size to decide how much to show (rows = `Height / RowHeight`, …).
   Call `drawStaleMark(item, Service::...)` at the end if the data can get old.

   **Texts** (German and English): user content (titles, names) is drawn as it is; every other text
   comes from the text table. Add a line per text to `I18N_TEXTS` in `src/i18n/Strings.h`:
   `X(MyKey, "German", "English")` (a screen text before `ApiOffline`, the setup app's texts after
   it), and draw `tr(i18n::S::MyKey)` (`app.h`, the chosen language). A text with printf
   conversions has the same ones in both languages, in the same order (`snprintf(buf, n,
   tr(i18n::S::MyKey), value)`). Dates and weekday / month names: `src/i18n/Dates.h` (not
   `strftime`, whose names are English only); prices with `i18n::localizeNumber()` (German decimal
   comma). German is usually longer: render both languages (step 6), and if the text sits in a
   fixed place, add a width check to `tests/host/test_i18n.cpp` (it measures with the screen font
   and checks that every screen text only uses characters the font has).
3. **Registry.** Add a `WidgetId` **at the end** (`src/layout/LayoutEngine.h`; its bit position is
   stored in NVS `show_mask`, at most 16 widgets) with its size limits in cells (`limitsOf`) and
   its key in `WIDGET_KEYS` (used by the setup app, saved box layouts and the registry: never rename
   a key, saved layouts refer to it), and a line in `src/widgets/Registry.cpp`: name (a text key),
   layout ID, row height, column width, refresh interval in minutes, and the hint (a text key, or
   `NO_HINT`) shown in its box when its settings are missing ("Wetter: Ort in der App eintragen" /
   "Weather: set a place in the app"); in `configuredWidgets()` say when it
   counts as set up. The setup app gets it automatically (`/api/state` lists every widget with its
   size limits); for its sketch preview add a `case` to `widgetSvg()` in `web/app.js` and run
   `python3 tools/web/build.py`.
4. **Templates.** Place it in the templates that should show it (`src/layout/Templates.h`, grid
   cells `{col, row, cols, rows}` on the 4 × 3 grid). Classic stays as it is. The editor's empty
   structures (`GENERIC` in `web/layout.js`, tested by `tests/web/layout.test.cjs`) suggest a widget
   per box; add it there if it fits.
5. **Wake cycle.** In `src/core/App.cpp`: fetch it when due (`WakePlan`: add its refresh to
   `WakeInputs` / `WakePlan`, with a host test in `tests/host/test_wake_plan.cpp`), redraw it with
   `updatePartial(item, myWidget)` after new data, and add it to `FullScreenParts` /
   `drawFullScreen()` (`src/widgets/Dashboard.cpp`).
6. **Check it.** `tests/host/run.sh` (the layout tests check every template with every widget on and
   off), then render on the PC: add sample data in `tools/render/render.cpp`, run
   `tools/render/render.sh` (German, into `docs/images/layouts/`) and `tools/render/render.sh /tmp/en en`
   (English), look at the images. Classic must not change.
