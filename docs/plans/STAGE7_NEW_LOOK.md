# Stage 7 (new look) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The dashboard looks like the app's sketch (filled Montserrat clock with the date, headings, a
clean month view with a design per box size), and shows public holidays, school holidays and
birthdays from the calendar.

**Architecture:** Pure, host-tested headers decide (holidays, month geometry and marks, clock layout,
school-holiday ranges); the widgets only draw what they return, like `PowerPlan.h` and
`ButtonGesture.h` today. School holidays come from our own ICS per Bundesland, published by a
GitHub Action, and are read by the existing ICS reader.

**Tech Stack:** C++17 (Arduino ESP32 core 3.3.7, GxEPD2, Adafruit GFX, ArduinoJson 7, LittleFS, NVS),
sparse bitmap fonts made with `tools/ListFontConvertV4.py`, plain ES2017 JS app (`web/`), Python 3
for tools and the Action, GitHub Actions.

**Spec:** [GIFT_EDITION.md](GIFT_EDITION.md) (stage 7) and [LOOK_AND_LANGUAGE.md](LOOK_AND_LANGUAGE.md) part 2.

## Global Constraints

- **No commits or pushes by the implementer.** Each task ends with a checkpoint (tests green); the owner
  commits. At the end: a summary and the commit command, without a Co-Authored-By line.
- Every text on the screen and in the app in German and English: `src/i18n/Strings.h`
  (`X(Key, "Deutsch", "English")`), `web/i18n.js`; German is the default.
- Storage: nothing new in the 128 KB `userdata` unless needed; files rewritten only when their content
  changed. New data that survives sleep goes in RTC memory, small (classic ESP32: 8 KB RTC in total).
- Big constant tables in flash (`const`), lists on the heap (the classic ESP32 profile's static RAM
  overflowed before).
- Firmware below 95 % of the app slot on every profile (`tools/dev/build_all.sh`); new fonts, net
  of the removed HighSpeed font, at most 40 KB; report the numbers.
- App bundle at most 60 KB gzipped (`python3 tools/web/build.py --check`).
- Classic keeps its places (pixel layout); it gets the new clock but no headings.
- Fonts: Montserrat only (SIL OFL), original artwork only.
- Checks after every task: `CXX=~/micromamba/envs/hosttools/bin/x86_64-conda-linux-gnu-g++ MBEDTLS_PREFIX=~/micromamba/envs/hosttools tests/host/run.sh`
  (all pass), `node --test tests/web/*.test.cjs`, `python3 tools/web/build.py --check`; after drawing
  changes, PC renders: `CXX=... tools/render/render.sh /tmp/r de` (and `en`).
- `docs/DEVICE_TESTS.md` block 9 grows with what each task built (the owner tests everything at once).

## Review Focus

1. **Year end:** December / January: KW 53 (2026 has 53 weeks), school holidays from 23 Dec to 9 Jan
   spanning the year, the footer in late December naming holidays of the next year. Tests: task 5
   (`test_iso_week_year_end`), task 6 (`test_school_across_year`, `test_footer_next_year`).
2. **Long names in the status bar** together with the power note and the battery ("Tag der Deutschen
   Einheit" + "Akku-Sparen: alle 15 Min."): shortened, nothing overlaps. Test: task 11
   (`test_status_holiday_fit` in `test_i18n.cpp`) and the render variant `holiday-saver`.
3. **Flash wear from the calendar:** links are read every 30-60 min; the calendar-birthday file must not
   be rewritten each time. Test: task 7 (`test_same_birthdays`).
4. **School feed missing or broken** (Action not run yet, 404, empty or garbage file, no Wi-Fi): the last
   ranges stay, a retry after 6 h, no problem marker, nothing drawn wrong. Test: task 8
   (`test_school_apply_keeps_old`).
5. **Clock not set yet** (after power-on, before NTP): the month view draws only what it can (no marks,
   no today), the status bar no holiday. Test: task 6 (`test_marks_without_date`).

---

### Task 1: New fonts, HighSpeed removed

**Files:**
- Create: `src/ui/fonts/ClockDigits_64.h`, `ClockDigits_110.h`, `ClockDigits_180.h`, `Heading_22.h`, `DigitsBold_16.h`
- Delete: `src/ui/fonts/HighSpeed_130.h`, `src/ui/fonts/GothamRnd_Bold_16.h` (not compiled anywhere)
- Modify: `src/ui/Fonts.h`, `src/ui/Fonts.cpp`, `src/ui/fonts/README.md`, `tests/host/test_i18n.cpp`

**Interfaces:**
- Produces: `extern const SparseGFXfont ClockDigits_64, ClockDigits_110, ClockDigits_180, Heading_22, DigitsBold_16;`
  `HighSpeed_130` no longer exists.

- [ ] **Step 1: Write the failing test** in `tests/host/test_i18n.cpp`: `test_heading_font_covers()` —
  every character of every heading text (keys `HeadAgenda`, `HeadTasks`, `HeadBirthdays`, `HeadNote`,
  `HeadStocks`, `HeadTracking`, both languages), every `i18n::monthLong`, `weekdayLong`, and "AM", "PM",
  "0123456789,. " exists in `Heading_22`; "0123456789:" exists in all three clock fonts; "0123456789" in
  `DigitsBold_16`. Add the six `Head*` keys to `Strings.h` ("Heute und morgen"/"Today and tomorrow",
  "Aufgaben"/"Tasks", "Geburtstage"/"Birthdays", "Notiz"/"Note", "Aktien"/"Stocks",
  "Sendungen"/"Parcels").
- [ ] **Step 2: Run it**, expect a compile failure (fonts missing).
- [ ] **Step 3: Generate the fonts** from `Montserrat[wght].ttf` (google/fonts `ofl/montserrat`) with
  `tools/ListFontConvertV4.py`: clock digits `--weight 700 --glyphs "0123456789:"` at sizes 64, 110,
  180; `Heading_22` `--weight 600 --size 22` with A-Z a-z ÄÖÜäöüß 0-9 and `,.:-–/ ` (space);
  `DigitsBold_16` `--weight 700 --size 16 --glyphs "0123456789"`. Threshold as for `epaperFont` (20);
  adjust if strokes break. Write the exact commands into `src/ui/fonts/README.md`. Declare and include
  them in `Fonts.h` / `Fonts.cpp`; remove HighSpeed (Clock.cpp still uses it until task 2: leave a
  temporary `#define HighSpeed_130 ClockDigits_110` there, removed in task 2).
- [ ] **Step 4: Run the host tests:** PASS. Run `tools/dev/build_all.sh`: note app size per profile
  (new fonts net ≤ 40 KB; if over, drop `ClockDigits_180` and note it).
- [ ] **Step 5: Checkpoint** (no commit).

### Task 2: Clock with the date

**Files:**
- Create: `src/widgets/ClockLayout.h`, `tests/host/test_clock_layout.cpp`
- Modify: `src/widgets/Clock.cpp`, `tools/render/render.sh` (LIST)

**Interfaces:**
- Consumes: the fonts of task 1 (widths measured through a callback, so the header stays pure).
- Produces:
  ```cpp
  namespace clockui {
  enum class Digits : uint8_t { D64, D110, D180 };
  struct Layout { Digits digits; int digitsX, digitsBaseline; bool showAmPm; int amPmX; bool showDate; int dateBaseline; };
  using WidthFn = int (*)(Digits, const char* text);     // digits width in px
  Layout layoutClock(int boxW, int boxH, const char* time, const char* amPm /*nullptr = 24 h*/,
                     WidthFn digitsWidth, int amPmWidth, int dateWidth);
  }
  ```

- [ ] **Step 1: Write the failing tests** with a fake `WidthFn` (64: 38 px/char, 110: 66, 180: 108,
  ":" a third): `test_clock_2x1_12h` (400 × 150, "10:28", "AM": D110, AM/PM shown, date shown,
  everything inside the box), `test_clock_classic` (430 × 155, 24 h: D110, date shown),
  `test_clock_full` (800 × 300: D180), `test_clock_1x1` (200 × 150: D64, date shown if it fits, else
  not), `test_clock_too_narrow_ampm` (AM/PM dropped before the digits shrink, as today).
- [ ] **Step 2: Run**, expect FAIL (header missing).
- [ ] **Step 3: Implement `layoutClock`:** pick the largest digits whose time (plus AM/PM and a 6 px gap)
  fits `boxW - 8` and whose cap height plus a 22 px date line fits `boxH - 8`; digits and date centred
  as a block; date dropped only if no size leaves room for it. `Clock.cpp` draws digits, AM/PM
  (`Heading_22`, on the digits' baseline) and the date (`i18n::formatDayHeader`, `Heading_22`).
  Remove the temporary define of task 1.
- [ ] **Step 4: Run** host tests: PASS; render `classic`, `planner`, `minimal`, a 2 × 1 12-hour box
  (`v1;...` variant `12h1258`) and a 1 × 1 clock: check by eye, nothing clipped.
- [ ] **Step 5: Checkpoint.**

### Task 3: Headings and weather labels

**Files:**
- Modify: `src/widgets/Widgets.cpp` (agenda, stocks, tracking, weather), `src/widgets/Personal.cpp`
  (tasks, note, birthdays), `src/widgets/Widgets.h`, `src/widgets/Registry.cpp`

**Interfaces:**
- Produces: `int drawHeading(LayoutItem* item, const char* text);` in `Widgets.h` — draws the text in
  `Heading_22` and a 1 px line under it across the box, returns the height used (`HEADING_H = 30`),
  or 0 when `layouts::wantsHeading` says no (Classic, which the layout result knows as the pixel
  template, or a box lower than 100 px).

- [ ] **Step 1: Write the failing test** `test_heading_rule` in `tests/host/test_layout.cpp` for a pure
  helper `bool layouts::wantsHeading(bool pixelLayout, int boxH)` (Classic: false; 2 × 1 box 150 px:
  true; 90 px: false).
- [ ] **Step 2: Run**, FAIL. **Step 3:** implement the helper in `src/layout/LayoutEngine.h` and
  `drawHeading`; each list widget starts its first line `drawHeading(...)` px lower. Weather: the
  temperatures as `"%.0f° / %.0f°"` (max / min). Month: its own title in task 10.
- [ ] **Step 4: Run** tests: PASS; render planner, minimal, technical, personal in de and en: headings
  match the app's sketch texts.
- [ ] **Step 5: Checkpoint.**

### Task 4: Settings for holidays, month marks and calendar birthdays

**Files:**
- Modify: `src/core/ConfigManager.{h,cpp}`, `src/core/ConfigValidation.h`, `src/web/Api.cpp`,
  `web/app.js`, `web/i18n.js`, `docs/SETUP_API.md`, `tests/host/test_config_validation.cpp`,
  `tests/web/layout.test.cjs` (or a new `tests/web/settings.test.cjs`)

**Interfaces:**
- Produces (globals, NVS key in brackets):
  `char holiday_state[4]` ("hol_state", "" = off, else `BW BY BE BB HB HH HE MV NI NW RP SL SN ST SH TH`),
  `bool holidays_on` ("hol_on", false), `bool school_on` ("school_on", false),
  `char school_url[CFG_ICS_URL_MAX]` ("school_url", "" = our feed for the state),
  `uint8_t ics_month_mask` ("ics_mon", 0; bit i = link i marks the month),
  `bool cal_birthdays` ("cal_bday", true).
  API settings fields: `holiday_state`, `holidays_on`, `school_on`, `school_url`, `ics_month` (array of
  3 bools), `cal_birthdays`. `bool validStateCode(const char*)` in `ConfigValidation.h`.

- [ ] **Step 1: Write the failing tests:** `test_state_codes` (all 16 codes and "" valid; "XX", "by",
  "BYY" invalid); a web test that the Data tab renders the Bundesland select, the two switches, the
  school link field with the default link as placeholder, and an "Im Monat markieren" switch under
  each calendar link; choosing a state switches both on.
- [ ] **Step 2: Run**, FAIL. **Step 3:** implement load / save / API (an invalid state is a field error
  `ErrState`; `school_url` validated like a calendar link, not a secret: it is sent back), the app
  fields (Data tab, "Feiertage und Ferien" card; the calendar card gets the switches; the cal-birthdays
  switch in the birthdays card), texts in both languages, `SETUP_API.md`.
- [ ] **Step 4: Run** all checks: PASS.
- [ ] **Step 5: Checkpoint.**

### Task 5: Public holidays

**Files:**
- Create: `src/core/Holidays.h`, `tests/host/test_holidays.cpp`
- Modify: `src/i18n/Strings.h` (holiday names)

**Interfaces:**
- Produces:
  ```cpp
  namespace holidays {
  enum class State : uint8_t { None, BW, BY, BE, BB, HB, HH, HE, MV, NI, NW, RP, SL, SN, ST, SH, TH };
  State parseState(const char* code);                 // "" / unknown -> None
  struct Holiday { int16_t year; uint8_t month, day; i18n::S name; };
  void easter(int year, int& month, int& day);        // Gregorian
  int holidaysOfYear(int year, State s, Holiday* out, int cap);   // sorted by date
  bool holidayOn(int year, int month, int day, State s, Holiday* found);
  int isoWeek(int year, int month, int day);          // ISO 8601, 1..53
  }
  ```

- [ ] **Step 1: Write the failing tests:** `test_easter` (2024-03-31, 2025-04-20, 2026-04-05,
  2027-03-28, 2030-04-21); `test_national_2026` (BY and NI both have 1.1, 3.4 Karfreitag, 6.4, 1.5,
  14.5 Himmelfahrt, 25.5 Pfingstmontag, 3.10, 25.12, 26.12); `test_state_rules`: 6.1 only BW BY ST;
  8.3 only BE MV; Fronleichnam 2026-06-04 only BW BY HE NW RP SL; 15.8 only BY SL; 20.9 only TH;
  31.10 only BB HB HH MV NI SN ST SH TH; 1.11 only BW BY NW RP SL; Buß- und Bettag only SN
  (2026-11-18, 2027-11-17); Ostersonntag and Pfingstsonntag only BB; `State::None` has none;
  `test_iso_week` (2026-11-23 → 48, 2024-12-30 → 1) and `test_iso_week_year_end` (2027-01-01 → 53,
  2026-12-31 → 53, 2027-01-04 → 1).
- [ ] **Step 2: Run**, FAIL. **Step 3:** implement (Easter: the anonymous Gregorian algorithm; Buß- und
  Bettag: the Wednesday between 16 and 22 November). Names in `Strings.h` (`HolNewYear` "Neujahr" /
  "New Year's Day", `HolEpiphany` "Heilige Drei Könige" / "Epiphany", … `HolRepentance` "Buß- und
  Bettag" / "Day of Repentance"); the screen font must have every character (the i18n test checks).
- [ ] **Step 4: Run**: PASS. **Step 5: Checkpoint.**

### Task 6: Month model (geometry, marks, footer)

**Files:**
- Create: `src/widgets/MonthModel.h`, `tests/host/test_month_model.cpp`, `src/services/SchoolHolidays.h`
  (only the two types here; task 8 adds the functions)

**Interfaces:**
- Consumes: `holidays::` (task 5).
- Creates in `SchoolHolidays.h`: `namespace school { struct Range { long fromDay, toDay; char name[24]; };
  struct SchoolList { uint8_t count; Range r[8]; }; }` (days as `personal::dayNumber`, `toDay` inclusive).
- Produces:
  ```cpp
  namespace month {
  enum class Design : uint8_t { Sheet, TwoWeeks, Compact, Full, Wide };
  struct Geometry { Design design; bool kw; int cols, cellW, cellH, gridX, gridW, sideW, footerLines; bool markRow, bigDigits; };
  constexpr int TITLE_H = 26, DOW_H = 20, LINE_H = 20, MIN_MARK_CELL_H = 34;
  Geometry layoutFor(int w, int h);
  struct Marks { uint32_t holiday, school, appointment, birthday; int today; };  // bit d = day d; today 0 = unknown
  Marks marksFor(int year, int month, int today, holidays::State, const school::SchoolList*,
                 uint32_t appointmentDays, const personal::BirthdayList* const* lists, int listCount);
  enum class Kind : uint8_t { Holiday, SchoolStart, SchoolEnd, Birthday };
  struct Highlight { personal::Date date; Kind kind; char text[48]; int age; };
  int highlights(const personal::Date& today, holidays::State, const school::SchoolList*,
                 const personal::BirthdayList* const* lists, int listCount, Highlight* out, int cap);
  }
  ```

- [ ] **Step 1: Write the failing tests** (exact values; the builder may tune the thresholds, then these
  numbers change with them):
  - `test_layout_sizes`: 800 × 452 → Wide, kw, sideW 240, cellW 70, cellH 67, bigDigits;
    400 × 452 → Full, kw, footerLines 3, cellW 50, cellH 57; 400 × 300 → Full, footerLines 2, cellH 35,
    markRow; 360 × 230 (Classic) → Full, footerLines 0, cellH 30, no markRow; 200 × 300 → Compact,
    no kw, cols 7, cellW 28, cellH 42, markRow; 400 × 150 → TwoWeeks, kw, footerLines 1, cellH 42;
    800 × 150 → TwoWeeks, sideW 240; 200 × 150 → Sheet.
  - `test_marks`: January 2027, BY, today 13: holiday bits 1 and 6; birthday bit 17 from a list entry
    (17 Jan); appointment bits passed through; today 13.
  - `test_school_across_year`: a range 2026-12-23 … 2027-01-09 marks Jan 1-9 in January 2027 and
    Dec 23-31 in December 2026.
  - `test_marks_without_date`: today 0 → no today, no marks (Review Focus 5).
  - `test_footer_order`: today 2027-01-04, BY: first "Heilige Drei Könige" (6 Jan), school end 9 Jan,
    then birthday 17 Jan with age 60 if the year is 1967; cap respected.
  - `test_footer_next_year`: today 2026-12-28, BY, cap 3: Neujahr 2027 is in it.
- [ ] **Step 2: Run**, FAIL. **Step 3:** implement. Rules: `w < 300 && h < 200` Sheet; `h < 200`
  TwoWeeks (side column 240 px if `w >= 600`, else 1 footer line); `w < 300` Compact; `w >= 560` Wide
  (side column 240 px, or 200 if `w < 760`); else Full with the most footer lines (max 3) that keep
  `cellH >= MIN_MARK_CELL_H`, else 0. `markRow = cellH >= MIN_MARK_CELL_H`; `bigDigits = cellH >= 56`.
  Highlights look ahead 60 days.
- [ ] **Step 4: Run**: PASS. **Step 5: Checkpoint.**

### Task 7: Month marks per link, birthdays from the calendar

**Files:**
- Modify: `src/services/Calendar.cpp`, `src/widgets/Widgets.h` (`CalEvents`), `src/personal/Birthdays.h`,
  `src/personal/UserStore.{h,cpp}`, `src/widgets/Personal.cpp`, `tests/host/test_personal.cpp`
- Create: `src/services/IcsRules.h`

**Interfaces:**
- Consumes: `ics_month_mask`, `cal_birthdays` (task 4).
- Produces: `bool ics::countsAsBirthday(char freq, bool allDay, bool settingOn)` (`IcsRules.h`);
  `upcomingBirthdays(manual, feed, today, out, max, horizon, const BirthdayList* cal = nullptr)` (a
  third source, the same person on the same day only once); `bool personal::sameBirthdays(const BirthdayList&, const BirthdayList&)`;
  `bool saveUserCalBirthdays(const BirthdayList&)` (writes `/bday_cal.json` only if different) and
  `const BirthdayList& userCalBirthdays()`.

- [ ] **Step 1: Write the failing tests:** `test_counts_as_birthday` (Y + all-day + on → true; W, timed,
  or off → false); `test_upcoming_three_lists` (an entry in the feed and the calendar list shows once;
  a calendar-only entry shows); `test_same_birthdays` (same content in another order → true; a changed
  day → false) (Review Focus 3).
- [ ] **Step 2: Run**, FAIL. **Step 3:** implement: in `fetchCalendarIcs` each link reads with
  `w.markMonth = ics_month_mask & (1 << i)`; `markDays` only when set; birthday-like events are still
  listed in "Heute und morgen" but never marked as appointments and are collected (60-day horizon,
  their own window end) into a heap list saved with `saveUserCalBirthdays` after all links were read
  successfully. Age only from a "(1966)"-style year in the title (strip it from the name). Check whether
  Google's contacts birthday calendar offers an iCal link; write the answer (and the setup step if not)
  into `docs/SETUP_API.md`'s birthdays part and the result section.
- [ ] **Step 4: Run**: PASS. **Step 5: Checkpoint.**

### Task 8: School holidays on the device

**Files:**
- Create: `tests/host/test_school_holidays.cpp`
- Modify: `src/services/SchoolHolidays.h` (functions)
- Modify: `src/services/Calendar.cpp`, `src/services/Fetchers.h`, `src/core/App.cpp`, `src/core/AppState.{h,cpp}`

**Interfaces:**
- Consumes: `holiday_state`, `school_on`, `school_url` (task 4); `OTA_REPO`.
- Produces (in `SchoolHolidays.h`, next to the types of task 6):
  `bool school::feedUrl(char* out, size_t cap, const char* repo, const char* state, const char* custom)`
  (custom wins; else `https://github.com/<repo>/releases/download/feeds/ferien-<STATE>.ics`; false
  without state), `void school::addRange(SchoolList&, long fromDay, long toDayInclusive, const char* name)`
  (sorted, merges duplicates, keeps the 8 nearest from today − 31 days), `bool school::apply(SchoolList& kept, const SchoolList& fresh, bool ok)`
  (fresh replaces kept only if `ok`; returns whether it changed), `const Range* school::currentOrNext(const SchoolList&, long today)`.
  `bool fetchSchoolHolidays(school::SchoolList& out)` in `Fetchers.h`. RTC: `school::SchoolList schoolHolidays`.

- [ ] **Step 1: Write the failing tests:** `test_feed_url` (BY default URL exact; custom wins; no state →
  false); `test_add_range` (DTEND exclusive handled by the caller: pass inclusive; duplicates merged;
  more than 8 keeps the nearest); `test_school_apply_keeps_old` (a failed or empty fetch keeps the old
  list) (Review Focus 4); `test_current_or_next`.
- [ ] **Step 2: Run**, FAIL. **Step 3:** implement: the ICS reader gets a school mode (all-day events →
  `addRange`, title as name); in `App.cpp` the same pattern as the birthday feed (`bdayFeedDay`,
  URL hash, retry), but due once every 7 days, retry after 6 hours, only when `school_on` and a state
  is set and the month view or agenda is on the layout; no problem marker.
- [ ] **Step 4: Run**: PASS. **Step 5: Checkpoint.**

### Task 9: The school-holiday feed (GitHub Action)

**Files:**
- Create: `tools/feeds/school_holidays.py`, `tools/feeds/test_school_holidays.py`,
  `tools/feeds/fixtures/BY-2026.json`, `.github/workflows/feeds.yml`
- Modify: `.github/workflows/compile.yml` (run the Python tests), `docs/PROJECT_INFO.md` (where the feeds live)

**Interfaces:**
- Produces: release assets `ferien-<STATE>.ics` (16 files) on the pre-release with the fixed tag
  `feeds` — never GitHub's "latest" (stable devices read `releases/latest`).

- [ ] **Step 1: Write the failing tests:** `test_ics_from_fixture` (the fixture, taken from a real
  `https://ferien-api.de/api/v1/holidays/BY/2026` answer, becomes VEVENTs with `DTSTART;VALUE=DATE`,
  `DTEND;VALUE=DATE` = last day + 1, a stable UID `ferien-BY-<start>@eink-dashboard`, SUMMARY with a
  capitalised German name such as "Herbstferien", CRLF line ends); `test_failed_state_not_written`
  (a failing fetch for one state writes no file for it).
- [ ] **Step 2: Run** `python3 -m unittest discover -s tools/feeds`, FAIL.
- [ ] **Step 3: Implement:** fetch this year and next per state, check how the API marks the end date
  (inclusive or not; UTC midnight) against the fixture and document it in the script; write only states
  that worked. `feeds.yml`: weekly (Monday 03:17 UTC) and `workflow_dispatch`; creates the `feeds`
  pre-release if missing (`--prerelease --latest=false`), uploads with `--clobber`. Data licence: note
  ferien-api.de (MIT) in the release notes. `compile.yml`: the unit test step. Check with
  `/tmp/fontenv/bin/actionlint`.
- [ ] **Step 4: Run** the tests and actionlint: PASS.
- [ ] **Step 5: Checkpoint.** After the owner pushes: the owner runs the workflow once by hand (noted
  in DEVICE_TESTS block 9).

### Task 10: Month view drawing

**Files:**
- Modify: `src/widgets/MonthView.cpp`, `tools/render/render.cpp` (sample marks, holidays, school, birthdays), `tools/render/render.sh` (LIST)

**Interfaces:**
- Consumes: `month::layoutFor`, `month::marksFor`, `month::highlights` (task 6), `holidays::isoWeek`,
  fonts (task 1), `cal.daysWithEvents` (task 7: only marked links), `schoolHolidays` (task 8),
  the birthday lists (task 7).

- [ ] **Step 1: Add render cases** for a test date in January 2027 (BY): `month-full` (4 × 3),
  `month-half` (2 × 3), `month-2x2`, `month-1x2`, `month-2x1`, `month-1x1`, and Classic.
- [ ] **Step 2: Render**: they look like today's month (the baseline to compare).
- [ ] **Step 3: Implement** per the design: title in `Heading_22`, KW column, weekday row; per cell the
  number (weekend in `DigitsBold_16`, `bigDigits` uses `Heading_22` digits), today a filled rounded
  square (radius 4, tight around the number, white digits), holiday the same as a 1 px frame (today
  wins), mark row (dot radius 2; a 9 × 9 cake bitmap drawn in code; both centred side by side), school
  line 2 px at the cell's bottom edge joining neighbours; footer / side column from `highlights`
  (`★` as the MDI star icon from `MDI_22_Sparse`, cake, line symbol); TwoWeeks: this and next week;
  Sheet: weekday, big date in `ClockDigits_64`, month + KW, one highlight.
- [ ] **Step 4: Render** all cases in de and en: nothing overlaps, matches the design's mockups. Keep the
  PNGs for the owner's review (`docs/images/layouts/`).
- [ ] **Step 5: Checkpoint.**

### Task 11: Status bar holiday name, Wi-Fi bars, holidays in "Heute und morgen"

**Files:**
- Modify: `src/widgets/StatusBar.cpp`, `src/widgets/Widgets.cpp` (agenda), `src/services/WifiService.cpp`
  (last RSSI into RTC), `tests/host/test_i18n.cpp`, `tools/render/render.sh`

**Interfaces:**
- Consumes: `holidays::holidayOn` (task 5), `holidays_on`, `holiday_state` (task 4).
- Produces: `RTC int8_t lastRssi` (0 = unknown) in `WifiService`.

- [ ] **Step 1: Write the failing test** `test_status_holiday_fit`: the longest German holiday name after
  the longest status date, plus the longest power note, plus the battery block, measured with
  `epaperFont`, fits 800 px once the name is shortened by the same rule the status bar uses
  (`drawFittedString` to the space left) (Review Focus 2).
- [ ] **Step 2: Run**, FAIL. **Step 3:** implement: `Mi, 6. Jan · Heilige Drei Könige` (name fitted
  before the power note); Wi-Fi bars from `lastRssi` (MDI wifi-strength 1-4; check the code points
  against the MDI metadata; add them to `MDI_22_Sparse` with the README command), left of the battery.
  Agenda: a holiday today or tomorrow is the first entry under its day (`★ Heilige Drei Könige`), the
  day header shown even without other entries.
- [ ] **Step 4: Run** tests: PASS; render variants `holiday` and `holiday-saver` (test date 6 Jan, BY).
- [ ] **Step 5: Checkpoint.**

### Task 12: The app's sketch follows the device

**Files:**
- Modify: `web/app.js` (`sketch()`, the month, clock, weather and list boxes), `web/i18n.js`,
  `tests/web/layout.test.cjs`

- [ ] **Step 1: Write the failing web test:** the sketch of the planner layout contains the headings of
  the device (`Heute und morgen`, month title), the clock box a date line, the month box a filled
  square for today and KW numbers; weather "9° / 2°".
- [ ] **Step 2: Run**, FAIL. **Step 3:** implement with the same sizes per box as `month::layoutFor`
  (the same thresholds, written once in JS).
- [ ] **Step 4: Run** web tests and `build.py --check` (≤ 60 KB): PASS.
- [ ] **Step 5: Checkpoint.**

### Task 13: Build 79, documents, review renders

**Files:**
- Modify: `configure.h` (`FW_BUILD 79`), `CHANGELOG.md`, `.github/whats_new.txt`, `docs/DEVICE_TESTS.md`
  (block 9: "(not built yet)" removed, items as built, the one-time feeds workflow run), `docs/IMPLEMENTATION_PLAN.md`
  (stage 7 items `[x]`, "Where we are"), `docs/plans/LOOK_AND_LANGUAGE.md` and `GIFT_EDITION.md`
  (result sections), `README.md` if screenshots are referenced

- [ ] **Step 1:** whats_new line 79 in German, three short points (e.g. `79: Neue Uhr mit Datum |
  Monatskalender neu, mit Feiertagen und Ferien | Geburtstage aus dem Kalender`).
- [ ] **Step 2:** all checks, `tools/dev/build_all.sh` (all four profiles, no warnings), sizes into the
  result section (app slot %, fonts net, bundle KB, RTC bytes added).
- [ ] **Step 3:** renders of every ready-made layout and the month sizes in de and en for the owner.
- [ ] **Step 4:** summary for the owner and the commit command (no Co-Authored-By).

## Result (build 79, 2026-11-23)

Done in code and checked on the PC (host tests incl. the new `test_clock_layout`, `test_holidays`,
`test_month_model`, `test_school_holidays`; web tests; `tools/feeds` unit tests; renders of every
case in German and English in [docs/images/layouts/](../images/layouts/)); not yet on the device
(DEVICE_TESTS block 9).

Decisions taken while building (all in the ledger of the run; the ones that change what you see):
- **AM/PM goes before digit size** (a 12-hour clock without it is ambiguous); narrow boxes show the
  short date ("Di 17. Nov").
- **Clock and month view are allowed from 1 × 1** (the editor's minimum sizes; the month has a design
  for every size).
- **School holidays come from the OpenHolidays API** (ODbL 1.0), not ferien-api.de: its real answer
  lacked Bavaria's Christmas holidays 2026/27 and all of 2027. The feed is read only when the month
  view is on the screen; a changed state or link replaces the old list.
- **Symbols are drawn in code:** star, cake and the school bar (9 × 9) and the Wi-Fi bars, so they have
  the same size everywhere and need no icon font.
- **Big month cells (full screen, half) use 22 px digits** (two small extra digit fonts, about 1 KB).
- **Side column entries** are two lines (date, then the text); school holidays read "Weihnachtsferien
  enden / beginnen" there; the footer says "Weihnachtsferien bis Fr 8. Jan".
- **The footer follows the last week row** (a month with 5 weeks has no gap above it).
- **The app's sketch** draws the month like the device (week numbers, small today square, bold weekend,
  two weeks, sheet) but without holidays or birthdays (the device's preview shows the real data).
- **Google's contacts birthday calendar:** no answer from Google's help pages; checked in the device test.

Sizes: firmware 90.5 % of the app slot on the classic ESP32 profiles, 90.2 % on the C6 (build 78: about
88 %), no warnings on any of the four profiles; new fonts about 30 KB of bitmaps (the HighSpeed font,
13 KB, is gone); app bundle 50.0 KB of the 60 KB limit; RTC memory about 460 bytes more (school
holidays, Wi-Fi signal, feed state); the 128 KB storage gets one small file (`/bday_cal.json`, written
only when the calendar's birthdays change).

**Review (a fresh reviewer over the whole stage) and fixes in the same build:**
- *Critical, fixed:* a failing feed link (school holidays before the "Data feeds" workflow has run,
  or a broken birthday link, a bug since stage 4) cleared its retry wait on every wake, so Wi-Fi went
  on every minute. Now `src/core/FeedSchedule.h` keeps the last tried link apart from the last applied
  one: a failing link waits 6 hours (school) or 1 hour (birthdays); only a changed setting tries at once.
- *Fixed:* school holidays now disappear when switched off or when the state changes; the agenda shows
  every day once and in order (entries that began earlier, DST), and no holiday before the clock is
  set; only all-day entries of a school link count, a one-day school holiday is one footer line; no
  Wi-Fi bars next to the Wi-Fi problem mark.
- *Deferred (small):* a captive-portal page counts as a read school feed (next try in a week); an
  off-by-one between the holiday space and the power note; a 3 × 1 month gets the footer instead of
  the side column; no appointment dots for next-month days in the two-week view; the school link is
  stored in plain text (it is meant to be a public link); the app's school-link placeholder updates only
  after saving.

**Device test fix (build 81, 2026-11-25):** birthday lines were cut before the age ("Geburtstag
Lennart Kempf t…"). Calendar birthday titles lose the word "Geburtstag" / "birthday" (`ics::birthdayName`),
and `src/widgets/BirthdayText.h` shortens the line step by step so the age stays ("Anna wird 60" →
"Lennart Kempf (30)" → "Lennart K. (30)" → "Lenn… (30)"); tests `test_birthday_text`, `test_personal`.
The full-screen side column puts "Beginn" / "Ende" of school holidays on the date line.
