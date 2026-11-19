# How to add a widget

The screen is built from widgets placed by the layout engine (`src/layout/`). A widget is a
drawing function plus a few facts the engine and the wake cycle need.

1. **Data.** A fetch function in `src/services/` (verified HTTPS through `SecureClient`, see
   `HttpJson.h`) that fills a data struct (`src/widgets/Widgets.h`). If it can fail, give it a
   `Service` in `src/core/ServiceState.h` for retries and the "old data" mark.
2. **Drawing.** `void myWidget(LayoutItem* item)` in `src/widgets/`: draw with `canvas()` and the
   sparse fonts (`src/ui/SparseFont.h`) inside `item->PosX/PosY/Width/Height`. Drawing is clipped
   to that rectangle; use the size to decide how much to show (rows = `Height / RowHeight`, …).
   Call `drawStaleMark(item, Service::...)` at the end if the data can get old.
3. **Registry.** Add a `WidgetId` **at the end** (`src/layout/LayoutEngine.h`; its bit position is
   stored in NVS `show_mask`, at most 16 widgets) with its size limits in cells (`limitsOf`) and
   its key in `WIDGET_KEYS` (used by the setup app, saved box layouts and the registry: never rename
   a key, saved layouts refer to it), and a line in `src/widgets/Registry.cpp`: name, layout ID, row
   height, column width, refresh interval in minutes, and the hint shown in its box when its
   settings are missing ("Weather: set a place in the app"); in `configuredWidgets()` say when it
   counts as set up. The setup app gets it automatically (`/api/state` lists every widget with its
   size limits); for its sketch preview add a `case` to `widgetSvg()` in `web/app.js` and run
   `python3 tools/web/build.py`.
4. **Templates.** Place it in the templates that should show it (`src/layout/Templates.h`, grid
   cells `{col, row, cols, rows}` on the 4 × 3 grid). Classic stays as it is.
5. **Wake cycle.** In `src/core/App.cpp`: fetch it when due (`WakePlan`: add its refresh to
   `WakeInputs` / `WakePlan`, with a host test in `tests/host/test_wake_plan.cpp`), redraw it with
   `updatePartial(item, myWidget)` after new data, and add it to `FullScreenParts` /
   `drawFullScreen()` (`src/widgets/Dashboard.cpp`).
6. **Check it.** `tests/host/run.sh` (the layout tests check every template with every widget on and
   off), then render on the PC: add sample data in `tools/render/render.cpp`, run
   `tools/render/render.sh`, look at `docs/images/layouts/*.png`. Classic must not change.
