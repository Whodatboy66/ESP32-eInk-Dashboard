# Stage 8: data store, per-calendar settings, printer box, several screens — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Every widget draws from data kept in sleep memory and downloads only when its own interval is due; calendar links get a name and their own interval; the printer box uses its space (every AMS, filament details); then up to four screens (tap, time of day), a night sleep screen and a picture of the day.

**Architecture:** Four parts, each a beta build of its own (87-90), in the owner's order. Part A turns the downloaded data into an RTC "store" and the wake plan from a minute counter into "older than its interval" per service, so a screen change or a tap never forces downloads. Part B gives the printer box size-dependent designs from a pure geometry header (like `ClockLayout.h` / `MonthModel.h`), with a mockup gate before the build. Parts C and D add screens, the sleep screen and the picture; every decision (which screen, which data is due, colour names, layout geometry) sits in a pure, host-tested header; the `.cpp` files only draw and download.

**Tech Stack:** Arduino ESP32 core 3.3.7 (`sketch.yaml`, 4 profiles), GxEPD2 + own sparse fonts, ArduinoJson 7, NVS (`Preferences`), RTC memory (`RTC_DATA_ATTR`), host tests (`tests/host/run.sh`), web tests (`node --test tests/web/`), PC renders (`tools/render/render.sh`), GitHub Actions (Python + Pillow for the picture).

**Spec:** [GIFT_EDITION.md](GIFT_EDITION.md), section "Stage 8" (with "Look of the widgets", "Printer box when idle"), "Constraints", "Storage and program space". Facts about today's code: this plan's "Starting point".

## Global Constraints

- RTC memory: 8 KB on the classic ESP32 (7,656 B usable segment, ~3 KB used before this stage), 16 KB on the C6. New store data must keep the classic ESP32's RTC use **≤ 6,000 B** (checked by the build script, Task A1).
- Firmware: stay under 95 % of the 1.875 MB slot (89.3 % / 89.8 % at build 86).
- Static RAM on the classic ESP32: big tables `const` (flash), lists on the heap.
- `userdata` (LittleFS, 128 KB): nothing new stored there in this stage; pictures are drawn, not kept.
- App bundle: under the 60 KB gzipped CI limit (50 KB at build 86); compact UI.
- Battery: "New features must not add Wi-Fi wakes beyond what the user turns on."
- Everything German and English (`src/i18n/Strings.h`, `web/i18n.js`); German first.
- One screen (default, all existing devices) behaves exactly like today; the sleep screen is off for existing devices.
- "The clock interval is the upper bound for everything": nothing refreshes more often than the clock interval; intervals are rounded up to a multiple of it.
- Each part: `FW_BUILD` raised, a `.github/whats_new.txt` line (≤ 71 bytes UTF-8 per line), a CHANGELOG row, a DEVICE_TESTS block, renders and all four profiles built. No commits by the implementer (the owner commits).
- Settings keys never change once released (NVS); new keys only.

## Review Focus

1. **After a reset** (power loss, update, restart) the store is empty: every widget on the screen is due at once in **one** Wi-Fi session; no box shows "no data" after that first wake — test in Task A2 (`test_due_after_reset`).
2. **Services drift apart**: time-based intervals must not split one hourly Wi-Fi session into several; a service due within the next quarter of its interval rides along when Wi-Fi is on anyway — test in Task A2 (`test_ride_along`).
3. **A calendar link changed or removed** while its entries are kept: the old link's entries, marks and flight must disappear at the next read, not linger — test in Task A4 (`test_link_changed_drops_kept`).
4. **Screen windows past midnight, overlapping, or fewer screens than the index shown** (the owner deletes screen 3 while it is on): always a valid screen, the first outside all windows — test in Task C1 (`test_screen_choice_edges`).
5. **The clock not set yet** (no NTP after a reset, `now < 1700000000`): due checks must not divide or subtract into nonsense; everything counts as due — test in Task A2 (`test_due_without_clock`).

---

## Starting point (facts, build 86)

- Kept in RTC already: `cal` (`CalEvents`, 1,808 B), `tracking[5]`, `liveFlight`, `schoolHolidays`, `serviceStates[4]` (`lastOk`, `retryAt`, `failures` per Weather / Stocks / Calendar / Tracking; `src/core/ServiceState.h`). **Not kept:** `weather` (116 B), `stocks[5]` + `stockCount` (380 B, `src/widgets/Widgets.cpp:22-24`), `printer` (`bambu::PrinterStatus`, ~364 B, `src/services/Bambu.cpp:10`); MakerWorld lives in NVS.
- Due rule today: `WakePlan.h` `dueByWakes` = `bootCount % roundedRefresh(refresh, stepMin) == 0`; per-widget minutes in `src/widgets/Registry.cpp:15-27` (weather 360, agenda 60, month 60, stocks 60, tracking 120, printer 4). A button wake, a layout change and the end of the quiet hours set `bootCount = 0`, so **every widget downloads** then.
- Known side bug: a full redraw caused by a problem change (`App.cpp:624-628`) redraws weather and stocks from data that is no longer in RAM ("not available"). The store fixes it.
- Printer box: `bambuWidget` (`src/widgets/Printer.cpp:39-99`) has fixed x offsets; `firstAms()` shows one AMS; `PrinterReport.h` reads `remain` and `tray_type` only.
- Button: a single tap at wake = full refresh with downloads (`TouchController.cpp:90`, `App.cpp:201-205`); hold 2 s = restart, 4 s = setup (`configure.h:114-118`).
- Layout: one screen from `layout_template` / `layout_boxes` (`Registry.cpp:91-122`), 4 × 3 grid of 200 × 150 px under a 28 px status bar.

## File structure

| File | Part | Responsibility |
|---|---|---|
| `src/core/DataStore.h` (new) | A | Pure: the due rule per service (age, interval, clock step, ride-along), the interval choices |
| `src/core/DataStore.cpp` (new) | A | The RTC copies of weather, stocks, printer (moved from RAM) and their validity |
| `src/services/CalendarLinks.h` (new) | A | Pure: per-link due rule, merging kept and new agenda entries / marks / flight |
| `src/widgets/PrinterLayout.h` (new) | B | Pure: printer box geometry per size; filament colour names |
| `src/services/PrinterReport.h` | B | + brand, colour, every AMS, HMS codes |
| `src/core/Screens.h` (new) | C | Pure: which screen now (windows, tap, quiet hours), status dots |
| `src/widgets/SleepScreen.cpp/.h` (new) | C | The sleep screen (graphic, "Schläft bis 06:00", tomorrow line) |
| `src/widgets/SleepArt.h` (new, generated) | C | The 1-bit sleeping graphic (by `tools/art/make_sleep_art.py`) |
| `.github/workflows/picture.yml`, `tools/picture/` (new) | D | The daily picture: choose, dither, publish |
| `src/services/Picture.cpp/.h` (new) | D | Download the picture into the frame buffer, draw title and artist |

---

# Part A: data store and intervals (build 87)

### Task A1: the store in RTC memory, and a size check

**Files:**
- Create: `src/core/DataStore.cpp`, `src/core/DataStore.h` (the RTC part; the pure part comes in A2)
- Modify: `src/widgets/Widgets.cpp:22-24` (remove the RAM `weather`, `stocks`, `stockCount`), `src/services/Bambu.cpp:10` (`printer`), `tools/render/render.cpp` (sample data into the store), `tools/dev/build_all.sh` (RTC column)
- Test: `tests/host/test_data_store.cpp` (new)

**Interfaces:**
- Produces: `extern WeatherData weather; extern StockItem stocks[5]; extern int stockCount; extern bambu::PrinterStatus printer;` now `RTC_DATA_ATTR` in `DataStore.cpp` (same names: no widget changes); `bool storeHas(StoreItem)` / `void storeMark(StoreItem, bool valid)` with `enum class StoreItem : uint8_t { Weather, Stocks, Printer }` (validity bits, RTC).

- [ ] **Step 1: Write the failing test** `test_store_sizes`: `static_assert`-style checks in the host test that `sizeof(WeatherData) + sizeof(StockItem) * 5 + sizeof(bambu::PrinterStatus) <= 1400` (the budget left for Part B's filament fields is checked there).
- [ ] **Step 2: Run** `tests/host/run.sh` → FAIL (no `DataStore.h`).
- [ ] **Step 3: Implement** the RTC definitions and validity bits; a redraw of a widget whose `storeHas()` is false draws its "no data yet" text as today.
- [ ] **Step 4: `build_all.sh`**: a column `rtc` = `.rtc.data` + `.rtc.bss` (+ `.lp_ram` on the C6) from `<toolchain>-size -A` of the ELF; fail the profile when the classic ESP32's value exceeds 6,000 B.
- [ ] **Step 5: Run** host tests, renders (pixel-identical to build 86), `tools/dev/build_all.sh` → all four build, `rtc` column shown and under the limit.

### Task A2: due by age, per data type, with ride-along

**Files:**
- Modify: `src/core/DataStore.h` (pure part), `src/core/WakePlan.h:83-164`, `src/core/App.cpp:410-459` (inputs), `src/widgets/Registry.cpp:15-27`
- Test: `tests/host/test_data_store.cpp`, `tests/host/test_wake_plan.cpp` (existing tests that assume `bootCount` alignment for weather / stocks / calendar / tracking change to age)

**Interfaces:**
- Produces:
  - `constexpr uint16_t INTERVAL_CHOICES[] = {15, 30, 60, 120, 180, 360};` (minutes, all divide 1440)
  - `int effectiveInterval(int chosenMin, int stepMin)` → the choice rounded up to a multiple of `stepMin` (never below `stepMin`)
  - `bool dueByAge(time_t lastOk, time_t now, int intervalMin, int stepMin, bool wifiAnyway)` → true when `lastOk == 0`, or `now < 1700000000`, or the age ≥ the effective interval minus 30 s, or (`wifiAnyway` and the age ≥ ¾ of it)
  - `WakeInputs` gets `time_t lastOk[SERVICE_COUNT]` and `int intervalMin[SERVICE_COUNT]`; `forceFetch` (true on a button wake with one screen, a layout change from the app, the morning after the quiet hours — today's `bootCount = 0` cases); the per-widget `*Refresh` fields stay for the redraws only.
- Defaults (spec): calendar 30, weather 60, stocks 30, parcels 120 minutes.
- The old-data marks (`isStale`, `drawStaleMark`) keep using `serviceStates[].lastOk` with the chosen
  interval (today `item->Refresh`): "older than 2 × its interval, at least 30 min".
- MakerWorld stays in NVS (it already survives a reset, better than RTC); its hourly rule is unchanged.

- [ ] **Step 1: Write the failing tests** in `test_data_store.cpp`:
  - `test_effective_interval`: `effectiveInterval(30, 1) == 30`, `(15, 30) == 30`, `(60, 15) == 60`, `(60, 45) == 90`.
  - `test_due_by_age`: due at age 29 min 31 s for 30 min; not due at 29 min; due with `lastOk == 0`.
  - `test_ride_along`: 30-min interval at age 23 min: not due alone, due with `wifiAnyway`.
  - `test_due_after_reset` (Review Focus 1): all four `lastOk == 0` → the plan fetches all four and `needWiFi` once.
  - `test_due_without_clock` (Review Focus 5): `now = 1000` → due, no overflow.
  - In `test_wake_plan.cpp`: a tap with `forceFetch` fetches all; a daily full refresh (`bootCount % 1440 == 0`) **without** `forceFetch` redraws only.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement**: `WakePlan` decides weather / stocks / calendar / tracking with `dueByAge` (first pass without ride-along to find whether Wi-Fi is needed, second pass with `wifiAnyway`); printer, MakerWorld, flight and NTP keep their rules; `recordResult` (`ServiceState.h`) unchanged.
- [ ] **Step 4: Run** host tests → PASS; renders unchanged.

### Task A3: interval settings in NVS, API and app

**Files:**
- Modify: `src/core/ConfigManager.cpp` (keys `int_cal`, `int_wx`, `int_stk`, `int_trk`, `uint16_t`, validated against `INTERVAL_CHOICES`), `app.h`, `src/web/Api.cpp` (`/api/state` and `/api/settings`: `interval_calendar`, `interval_weather`, `interval_stocks`, `interval_tracking`), `web/app.js` (Data tab: one choice per service, under its own section; the note "höchstens so oft wie die Uhr" when the clock interval is larger), `web/i18n.js`, `web/dev/mock.js`, `docs/SETUP_API.md`, `tools/render/render.cpp`
- Test: `tests/web/*.test.cjs` (a settings round trip), `tests/host/test_config_validation.cpp`

- [ ] **Step 1: Failing tests**: validation rejects 45, accepts 15 … 360; the web test renders the four choices and sends `interval_weather: 60`.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement**; `python3 tools/web/build.py` regenerates `WebApp.h`.
- [ ] **Step 4: Run** host + web tests, `python3 tools/web/build.py --check` (bundle size noted) → PASS.

### Task A4: calendar links: own name and interval, kept entries per link

**Files:**
- Create: `src/services/CalendarLinks.h`
- Modify: `src/services/ServiceData.h:47-60` (`CalEventItem` + `uint8_t link`), `src/services/Calendar.cpp:fetchCalendarIcs`, `src/core/ConfigManager.cpp` (TEXT_SETTINGS `ics_name0..2`, 21 bytes each; `ics_every` `uint8_t[3]`: 0 = with the calendar interval, 1 = every 3 hours, 2 = mornings only), `src/core/Problems.h` (`problemLine` uses the name: "Arbeit kann nicht gelesen werden"), `src/web/Api.cpp`, `web/app.js` (a name field above each link, placeholder "Kalender 1"; the interval choice below "Im Monat markieren"), `web/i18n.js`, `src/i18n/Strings.h` (`PtCalendarNamed` "%s kann nicht gelesen werden" / "%s can't be read")
- Test: `tests/host/test_calendar_links.cpp` (new), `tests/host/test_i18n.cpp`

**Interfaces:**
- Produces (RTC, `Calendar.cpp`): `time_t icsLastRead[ICS_MAX_FEEDS]`, `uint32_t icsUrlHash[ICS_MAX_FEEDS]`, `uint32_t icsMonthDays[ICS_MAX_FEEDS]` (bit d = day d of `icsMonth`), `uint8_t icsMonth`.
- Produces (pure, `CalendarLinks.h`):
  - `bool linkDue(uint8_t every, time_t lastRead, time_t now, int minuteOfDay, bool calendarDue, int stepMin)` — 0: `calendarDue`; 1: age ≥ 3 h (rounded up to the step); 2: `lastRead` on an earlier local day and `minuteOfDay >= 6 * 60` (after the quiet hours: their end), or never read.
  - `int mergeAgenda(const CalEventItem* kept, int keptCount, uint8_t readMask, const CalEventItem* fresh, int freshCount, time_t now, CalEventItem* out, int cap)` — kept entries of links **not** in `readMask` that haven't ended, plus all fresh ones, sorted by start, at most `cap`.
  - `uint32_t mergedMonthDays(const uint32_t* perLink, uint8_t monthMask)`.
- Changes: `problemLine(uint16_t flags, uint8_t calFailedMask, const char* const* linkNames, i18n::Lang, char* out, size_t cap)`
  (`linkNames[i]` "" = unnamed: "Kalender 2 …" as today).
- Rules: a link whose URL hash changed or that was removed drops its kept entries, days and flight (Review Focus 3); calendar birthdays are rebuilt only in a read that included every link (the stored list stays otherwise); a tap with one screen reads only links with `every == 0` plus those whose own interval is due ("mornings only" stays once a day).

- [ ] **Step 1: Failing tests** in `test_calendar_links.cpp`:
  - `test_link_due`: every-3-h link not due at 2 h 59 min; mornings-only due at 06:00 on a new day, not at 05:59, not twice the same day.
  - `test_merge_keeps_unread_links`: link 2 not read → its two entries stay, link 1's old ones are replaced by the fresh ones; an ended kept entry is dropped; order by start; cap 10.
  - `test_link_changed_drops_kept` (Review Focus 3): changed hash → kept entries and days of that link gone.
  - In `test_i18n.cpp`: `problemLine` with a name → "Arbeit kann nicht gelesen werden"; every name of 20 characters still fits the status bar (fallback to the generic sentence when not).
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement** the pure header, then `fetchCalendarIcs` reads only the due links (`readMask`), merges with the kept data, stores the per-link days; the log names the link ("[ICS] Arbeit: 1101 events …", "[ICS] Arbeit: not due (every 3 h)").
- [ ] **Step 4: Run** host + web tests, renders → PASS.

### Task A5: part A done

- [ ] `FW_BUILD` 87, whats_new line, CHANGELOG row, DEVICE_TESTS block 10 part A (store: no downloads on a daily full redraw; tap still downloads with one screen; interval settings; link names in app / status bar / log; "every 3 hours" visible in the log; after a restart everything once), `docs/SETUP_API.md`, `docs/USER_GUIDE.md` (the new settings).
- [ ] All host tests, web tests, bundle check, renders, `build_all.sh` (rtc column) → report sizes.

---

# Part B: printer box and the look of the widgets (build 88)

### Task B1: every AMS and the filament details

**Files:**
- Modify: `src/services/PrinterReport.h` (`Tray` + `char brand[24]` from `tray_sub_brands`, `uint32_t color` from `tray_color` "RRGGBBAA"; all AMS units, not only the first; `uint32_t hms[4]` + `hmsCount` from `print.hms[]` (`attr << 32 | code` reduced to the 16-hex-digit form `AAAA_BBBB_CCCC_DDDD` as two `uint32_t`)), `src/widgets/Printer.cpp` (`firstAms()` removed)
- Test: `tests/host/test_printer_report.cpp` (a recorded report with **two** AMS units, brands, colours and one HMS entry; a delta message keeps the last full report)

- [ ] **Step 1: Failing tests**: `amsCount == 2`; `ams[1].trays[0].brand == "PLA Basic"`; `color == 0xFF6A13FF`; `hmsCount == 1`; an empty slot: brand "", color 0, remain -1.
- [ ] **Step 2: Run** → FAIL. **Step 3: Implement.** **Step 4: Run** → PASS; `test_store_sizes` (A1) still within 1,400 B (raise its bound only with the measured RTC column under 6,000 B).

### Task B2: geometry per box size and colour names (pure), then mockups — owner gate

**Files:**
- Create: `src/widgets/PrinterLayout.h`
- Modify: `tools/render/render.cpp` + `render.sh` (cases `printer-1x2`, `printer-2x1`, `printer-2x2`, `printer-4x1`, `printer-2x3`, each printing and idle, sample with two AMS units)
- Test: `tests/host/test_printer_layout.cpp` (new)

**Interfaces:**
- Produces: `enum class PrinterDesign : uint8_t { Compact, Row, Panel, Wide, Tall }` and `struct PrinterGeometry` (areas for the state line, progress bar, temperatures, AMS rows, spool cells) from `PrinterGeometry printerLayout(int w, int h, int amsCount, bool printing)`; `const char* colorName(uint32_t rgba, i18n::Lang)` (nearest of: Schwarz, Weiß, Grau, Rot, Orange, Gelb, Grün, Blau, Violett, Pink, Braun, Beige / English) and `SwatchFill swatchFor(uint32_t rgba)` (`Filled`, `Hatched`, `Empty` by luminance: < 0.35, < 0.7, else).
- Design rules (spec): use the whole box; no fixed pixel columns; spools as filled bars (height = fill level) with brand/type and colour name below; both AMS as rows when there is room (2 × 4 spools), otherwise the AMS with the most spools in use plus "+1 AMS"; idle view = temperatures + AMS (no progress); the hinted fonts.

- [ ] **Step 1: Failing tests**: `printerLayout(400, 300, 2, false)` places 8 spool cells inside the box without overlap; `printerLayout(200, 150, 2, true)` is `Compact` (state, progress, time left only); `colorName(0x000000FF, De) == "Schwarz"`, `colorName(0xFF6A13FF, De) == "Orange"`; `swatchFor(0xFFFFFFFF) == Empty`.
- [ ] **Step 2: Run** → FAIL. **Step 3: Implement** the header and draw `bambuWidget` from it. **Step 4: Run** → PASS.
- [ ] **Step 5 (gate):** renders of all printer cases go to the owner (in the chat, as images) **before** Task B3; changes go into this task; record the chosen designs in this plan's result.

### Task B3: the idle view setting, printer messages, and the look pass

**Files:**
- Modify: `src/widgets/Widgets.cpp` (`makerWorldWidget` chooses by the new setting `prn_idle`: 0 = MakerWorld when an id is set, 1 = printer), `src/core/ConfigManager.cpp`, `src/web/Api.cpp`, `web/app.js` ("Wenn der Drucker ruht: MakerWorld / Drucker"), `src/widgets/Printer.cpp` (an HMS line: a short `const` table of common codes in plain words, else "Meldung am Drucker (HMS 0700_…)"), `src/i18n/Strings.h`; the look pass over stocks, parcels, weather and MakerWorld (columns from the box width, the hinted fonts)
- Test: `tests/host/test_printer_layout.cpp` (HMS text lookup: known code → text, unknown → the code form), renders (owner review, like B2 step 5)

- [ ] **Step 1: Failing tests**: `hmsText(known code, De)` → its plain sentence; an unknown code → "Meldung am Drucker (HMS 0700_2000_0002_0001)"; `prn_idle = 1` with a MakerWorld id → the printer view is chosen (`showIdlePrinter(bool hasMwId, uint8_t prnIdle)` in `PrinterLayout.h`).
- [ ] **Step 2: Run** → FAIL. **Step 3: Implement**, then the look pass. **Step 4: Run** tests and renders → owner review of the renders (like B2 step 5) → adjust.

### Task B5: a switched-off printer gives its box to a neighbour (owner, 2026-11-25)

The owner switches the printer fully off most of the time: the box shouldn't show "not reachable" with
a warning 90 % of the day, but give its space to a neighbouring box, and come back when the printer does.

**Files:**
- Modify: `src/layout/LayoutEngine.h` / `BoxLayout.h` (pure merge), `src/widgets/Registry.cpp` (`loadLayoutFrom` with the printer off), `src/core/App.cpp` (printer state; a merge change = full redraw from the store, **no** `forceFetch`), `src/core/WakePlan.h` (a button wake ignores the printer backoff), `src/core/ConfigManager.cpp` (`prn_merge`: the neighbour's widget key, "" = automatic), `src/web/Api.cpp`, `web/app.js` (printer box: "Wenn der Drucker aus ist: Platz geben an …", only neighbours that fit), `web/i18n.js`, `src/i18n/Strings.h` ("Drucker aus" / "Printer off")
- Test: `tests/host/test_layout.cpp`, `tests/host/test_wake_plan.cpp`

**Interfaces:**
- Produces: `int mergeTarget(const Placement* boxes, int count, int printerIdx, int preferredIdx)` → the index of a box that, with the printer's box, forms a rectangle (the preferred one if it does, else one sharing a full edge, above / below first, then left / right; -1 = none); `bool mergeBoxes(Placement* boxes, int& count, int printerIdx, int targetIdx)` (the target grows to the union, the printer box is removed).
- State (RTC, `Bambu.cpp`): `bool printerOff` — set after **2** failed connections in a row without an auth error, cleared by the next answer. While off: no `PROBLEM_PRINTER` (a refused access code stays `PROBLEM_PRINTER_CODE`); the box merges when the idle view would be the printer's own (no MakerWorld id, or `prn_idle` = printer); with a MakerWorld idle view the box stays and shows MakerWorld.
- Polling while off: the existing backoff (10, 20, 40, then 60 min); **a button wake polls at once** (backoff ignored), so a tap after switching the printer on brings the box back.

- [ ] **Step 1: Failing tests**:
  - `test_merge_target`: printer 2×1 above weather 2×1 (same columns) → weather; printer 2×1 next to a 2×2 → -1; a preferred box that doesn't fit → the automatic one; `mergeBoxes` makes the weather box 2×2 and removes the printer box; the classic template's pixel boxes too (same rule on rectangles).
  - `test_bambu_tap_ignores_backoff`: `bambuRetryAfter` in the future, `buttonWake` → `p.bambu`.
  - No fitting neighbour: the box stays and draws "Drucker aus" without the alert icon (render case `printer-off-alone`).
- [ ] **Step 2: Run** → FAIL. **Step 3: Implement.** **Step 4: Run** host tests; renders `printer-off-merged` (planner, printer + weather), `printer-off-alone` → owner review with B2's renders.

### Task B4: part B done

- [ ] (after B5) `FW_BUILD` 88, whats_new, CHANGELOG, DEVICE_TESTS block 10 part B (two AMS shown, filament details, idle view setting, a printer message, the look of stocks / parcels / weather / MakerWorld; printer switched off: no warning, the box merges into its neighbour and comes back after a tap), docs images, sizes.

---

# Part C: several screens and the night sleep screen (build 89)

### Task C1: which screen now (pure)

**Files:**
- Create: `src/core/Screens.h`
- Test: `tests/host/test_screens.cpp` (new)

**Interfaces:**
- Produces: `struct ScreenWindow { int16_t from, to; }` (minutes, `from == to` = no window); `int chooseScreen(int count, const ScreenWindow* w, int minuteOfDay, int tapIndex, time_t tapUntil, time_t now)` → 0 … count-1; `int nextScreen(int shown, int count)`; `bool windowsOverlap(const ScreenWindow*, int count)` (for the app); `void screenDots(int shown, int count, char* out, size_t cap)` ("● ○ ○" as glyphs of the text font).

- [ ] **Step 1: Failing tests**: `test_screen_choice_edges` (Review Focus 4): a window 22:00-06:00 at 23:30 and at 05:59; outside all windows → 0; `count` reduced to 2 with `tapIndex == 3` → a valid index; a tap shown until `tapUntil`, after it the scheduled one; `windowsOverlap` true for 06:00-09:00 and 08:00-10:00, false for touching windows.
- [ ] **Step 2: Run** → FAIL. **Step 3: Implement.** **Step 4: Run** → PASS.

### Task C2: screens in settings, the wake, the gestures

**Files:**
- Modify: `src/core/ConfigManager.cpp` (`scr_n` 1-4; screen 0 = today's `layout_template` / `layout_boxes`; screens 1-3 in keys `scr1` … `scr3` as `"<template or custom boxes>|<from>-<to>"`; `scr_tap` 2 / 5 / 10 minutes, default 5), `src/widgets/Registry.cpp` (`loadLayoutFrom` for the chosen screen), `src/core/App.cpp` (RTC `screenShown`, `tapScreen`, `tapUntil`; a screen change = full redraw **from the store**, no `forceFetch`; a tap with ≥ 2 screens = next screen and one extra wake at `tapUntil`; with one screen = refresh as today), `src/hardware/TouchController.cpp` (unchanged gestures; hold 2 s stays restart = "Neu laden"), `src/widgets/StatusBar.cpp` (the dots right of the middle, before the battery, only with ≥ 2 screens), `src/web/Api.cpp`, `web/app.js` (Screens section: up to 4, each a layout / own boxes / picture / sleep, a window, presets "Morgens Kalender + Wetter, tagsüber alles, nachts Schlafbildschirm"; the overlap check), `web/i18n.js`, `docs/SETUP_API.md`, `docs/USER_GUIDE.md` (gestures)
- Test: `tests/host/test_wake_plan.cpp` (a screen change doesn't fetch fresh data; data of the new screen that is older than its interval does), `tests/web/` (screens round trip, overlap message), renders (status bar with dots, a holiday name still fits)

- [ ] Steps: failing tests → implement → host + web tests, renders → PASS.

### Task C3: the night sleep screen

**Files:**
- Create: `tools/art/make_sleep_art.py` (draws the original 1-bit graphic, about 200 × 150 px: moon, "z z Z", a sleeping face, from primitives; writes `src/widgets/SleepArt.h`), `src/widgets/SleepScreen.cpp/.h`
- Modify: `src/core/App.cpp` (night branch: with `sleep_on` the first night wake draws the sleep screen in place of the status-bar note; a tap at night shows the day screen for `scr_tap` minutes, then the sleep screen again), `src/core/ConfigManager.cpp` (`sleep_on`, default off; on in the new ready-made profiles of stage 9), `src/web/Api.cpp`, `web/app.js` ("Nachts: Schlafbildschirm" under the quiet hours), `src/i18n/Strings.h` ("Schläft bis %s" / "Asleep until %s"; tomorrow line: first appointment, weather, holiday — whichever the store has)
- Test: `tests/host/test_power_plan.cpp` (night with `sleep_on`: one wake, one draw, the sleep until the end unchanged), renders `sleep-de`, `sleep-en`, `sleep-tomorrow` (owner review)

- [ ] Steps: failing test → implement → renders → owner review.

### Task C4: part C done

- [ ] `FW_BUILD` 89, whats_new, CHANGELOG, DEVICE_TESTS block 10 part C (one screen exactly as before; two screens by tap and by window; the dots; tap time; sleep screen on/off; tap at night), sizes (bundle!).

---

# Part D: picture of the day (build 90)

### Task D1: the Action

**Files:**
- Create: `.github/workflows/picture.yml` (daily 03:30 UTC and manual; Python 3.12, Pillow), `tools/picture/make_picture.py`, `tools/picture/test_make_picture.py`, `pictures/own/.gitkeep`, `pictures/config.json` (`{"source": "mixed" | "own" | "aic", "own_share": 0.3}`)

**Interfaces:**
- Produces on the pre-release with the fixed tag `picture` (never "latest"): `picture.bin` (800 × 452, 1 bit, rows MSB first, 45,200 B), `picture.json` (`{"title", "artist", "date", "source", "build": <unix>}`).
- Selection (AIC open access, CC0): `is_public_domain`, classification print / drawing / woodcut / etching, an image id; a day seeds the choice so a rerun gives the same picture; on any failure the previous assets stay.
- Conversion: grayscale, fit inside 800 × 452 (letterbox white), autocontrast, Floyd–Steinberg to 1 bit.

- [ ] Steps: failing unit tests (`test_dither_size`: output exactly 45,200 bytes; `test_letterbox`; `test_same_day_same_choice` with a fixed seed and a stubbed list) → implement → `python3 -m unittest discover -s tools/picture` → PASS; `actionlint` clean; CI (`compile.yml`) runs these tests too. DEVICE_TESTS block M: run the workflow once by hand, check the release.

### Task D2: the device side

**Files:**
- Create: `src/services/Picture.cpp/.h`: download `picture.json`, then `picture.bin` straight into the canvas row by row, at most once a day when the picture screen is first due. On a failed download the e-paper is **not redrawn** (it keeps showing the last picture, as the spec asks) and the next try is in 60 minutes; only when the picture screen is newly switched to and nothing can be loaded, it shows "Bild nicht geladen" with the status bar. Title and artist drawn by the device (hinted fonts, umlauts).
- Modify: `src/core/App.cpp` (the picture screen: Wi-Fi only on its first draw of the day and on retries), `tools/render/render.cpp` (a sample picture case)
- Test: `tests/host/test_screens.cpp` (picture due once per local day; retry after a failure in 60 min)

- [ ] Steps: failing test → implement → renders → device check.

### Task D3: part D done

- [ ] `FW_BUILD` 90, whats_new, CHANGELOG, DEVICE_TESTS block 10 part D, README credits (AIC open access, CC0), sizes; then the stage-8 result section in this plan and the final whole-stage review (one fresh reviewer, as in stages 7 / 7b).
