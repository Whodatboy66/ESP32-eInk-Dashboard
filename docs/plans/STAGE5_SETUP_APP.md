# Stage 5 plan: setup app v2 + layout editor

Detailed plan for stage 5 of [IMPLEMENTATION_PLAN.md](../IMPLEMENTATION_PLAN.md) ("Smartphone
configuration v2", spec §7, Phase 4). Builds on stage 3 ([STAGE3_WIDGETS_LAYOUT.md](STAGE3_WIDGETS_LAYOUT.md)):
the layout engine, the widget registry and the shared `drawFullScreen()`.

The clickable prototype used for these decisions was removed once the real app existed (build 71);
the app itself can be opened without a device: `web/dev/index.html`.

**Done when:** a person without ESP32 knowledge sets up a device with only a phone, designs the
screen with the editor (structure, split / merge, a widget per box), and the device shows exactly
that; all settings of today's page are in the new app.

## Why

Today's setup page works (it opens by itself when the phone joins `Dashboard-Setup`) but looks old
and bare: one long form of text fields. It won't have room for what's coming (tasks, notes,
birthdays, power profiles, the layout editor). The layout choice is four fixed templates; the owner
wants TRMNL-like freedom: first the structure (full screen, halves, boxes ...), then what goes in each
box, including no clock at all.

## Decided by the owner

- **Editor B:** choose a structure, then split any box in two or merge two neighbors, then pick a
  widget per box. (Not A, structures only: too limited. Not C, free drawing: fiddly on a phone.)
- **Look "Paper":** black on white with strong outlines, like the e-paper itself.
- **Code on the screen** for the app on the home network: a fresh 6-digit code shown on the e-paper
  each time, like Bluetooth pairing (not a PIN to remember, not open).
- **"Always reachable on USB power" moves to stage 6** (it needs USB detection and a firmware that
  doesn't deep-sleep on USB: the power profiles).

## Approaches considered

| Question | Options | Chosen |
|---|---|---|
| Editor | A: structures only / **B: structures + split / merge** / C: draw boxes freely | B (owner) |
| Look | **Paper** / Phone settings / Night | Paper (owner) |
| How the app gets onto the device | **one gzipped file compiled into the firmware** / files on the LittleFS partition / a JS framework (Preact + Vite) | compiled in (my call): updates come with every OTA automatically and can't get out of step with the firmware; LittleFS would need its own update path; a framework adds an npm toolchain for an app of a few screens |
| Preview | **live sketch in the app + exact image from the device on request** / only the device image / only the sketch | both (my call): the sketch reacts instantly while editing; the device image is the truth (same drawing code as the screen) |

## Design decisions

1. **One self-contained web app.** Sources in `web/` (`index.html`, `app.css`, `app.js`,
   `layout.js`), plain JavaScript, no framework, no external files: in setup mode the phone has no
   internet, so no web fonts or downloads (system fonts only). `tools/web/build.py` (Python standard
   library only) joins the files, strips comments, gzips them into
   `src/web/generated/WebApp.h` (a byte array), which is committed (Arduino can't run build
   steps). CI rebuilds it and fails if the committed file differs, and fails above **60 KB
   gzipped**. Served at `/` with `Content-Encoding: gzip`.
2. **JSON API instead of form posts** (WebServer + ArduinoJson, both already used):

   | Route | What |
   |---|---|
   | `GET /api/state` | all settings, device info (version, battery, Wi-Fi, problems, last update), widget list with min sizes and "configured" flags, the layout; **secrets only as "set" / "empty"** |
   | `GET /api/networks` | Wi-Fi scan (name, signal, locked) |
   | `POST /api/settings` | changed fields only; validated with the existing `ConfigValidation.h` rules; answers per-field errors in plain words, or OK (then the device restarts, as today) |
   | `POST /api/layout` | the layout (below); validated by the engine, stored, and drawn on the screen right away: **no restart**, so you can keep editing |
   | `POST /api/preview.bmp` | exact picture of the layout sent in the request (the one being edited, not yet saved; decision 8) |
   | `POST /api/wifi-test` | joins a network without saving it, answers OK or the reason (first start, decision 9) |
   | `POST /api/update`, `POST /api/factory-reset` | as today's buttons |

   An empty secret field on save keeps the stored value (today's Wi-Fi behavior, now for all
   secrets). The captive-portal handling stays as it is (DNS to the device, OS connectivity checks
   redirected).
3. **The old page stays at `/basic`** until the new app is checked on a phone (iPhone and Android),
   as a fallback if the app fails in some captive-portal browser; removed in the release after.
4. **Paper look** (tokens from the prototype): background `#f4f5f3`, surfaces white, text and
   accents black `#000`, secondary text `#4d5257`, 2 px black outlines, radius 8 px (6 px small
   parts), icons as black squares with white symbols (one small inline SVG set), system font stack,
   16 px base text, touch targets at least 44 px. No dark mode in v1 (my call: Paper is meant to look
   like the display).
5. **Four tabs:** Home (preview, battery, Wi-Fi, last update, problems in plain words with the fix,
   "update available"), Screen (the editor, 12 / 24 h), Data (calendars, weather, stocks, parcels,
   flights, printer), Device (Wi-Fi, time zone, updates and channel, factory reset, diagnostics).
   **First start** (no Wi-Fi saved): a guided setup instead of the tabs: Wi-Fi, name and place,
   calendars, screen, done. The Wi-Fi password is tested in step 1 (`/api/wifi-test`); everything is
   saved **once at the end** (one restart).
   **Device name** (new setting, e.g. "Kitchen display"): shown in the app's header and the browser
   title, and used for the address on the home network (`kitchen-display.local`; letters, digits and
   hyphens; default `dashboard`), so two devices don't clash. Not drawn on the e-paper (the status bar
   stays as it is).
6. **Layout model: boxes on the stage 3 grid.** A custom layout = up to 12 boxes
   `{col, row, cols, rows, widget or empty}` on the 4 × 3 grid + status bar on / off.
   - Stored in NVS as one short string, key `lay_box`, with `lay_tpl = "custom"`, e.g.
     `v1;sb1;0,0,4,1,agenda;0,1,2,2,clock;2,1,2,2,month` (readable in the log; the version prefix
     allows a later format). Settings schema 3; devices keep their template until they edit.
   - The editor's structures are starting points: Full, Halves, Top + main, Top + 2, Four, Main +
     side, Six, Big + 2 small, and Planner / Minimal / Technical with their widgets. **Classic**
     stays selectable as it is (pixel layout, not editable).
   - **Everything the editor saves is a box layout.** A device on Planner / Minimal / Technical opens
     the editor with that template's current result (after gap filling) as boxes; saving stores
     boxes, so the sketch and the screen always agree. The old template keys keep working for devices
     that never edit.
   - **"Show" switches only for Classic:** with Classic selected, the Screen tab shows today's Show
     list instead of the boxes. In a box layout, a widget is shown because it's in a box.
     Schema 3 converts nothing (`lay_tpl`, `show_mask` stay; `lay_box` and the device name are new).
   - **Split** cuts across the side with more cells; with equal sides (2 × 2) it cuts horizontally
     (two 2 × 1, keeping the two columns most widgets need). Odd counts give the larger part first
     (3 rows → 2 + 1). The widget moves to the part it fits (the first if both), otherwise the box
     becomes empty. A 1 × 1 box can't be split. **Merge:** with a neighbor that shares a full edge;
     the selected box's widget wins (or the neighbor's if the selected one is empty).
   - Each widget at most once per screen; a widget too big for a box is greyed out with the reason
     ("needs 2 × 2"). The min sizes come from the device (`/api/state`), not a copy in the app.
7. **Engine changes** (`src/layout/`, host-tested):
   - custom layouts from boxes; **empty boxes stay empty** (no gap filling: the owner placed the
     boxes on purpose; gap filling stays for the built-in templates);
   - **clock and status bar optional** in custom layouts (today `enabledWidgets()` forces both on);
   - **without status bar** the grid starts at the top (rows 160 px); problems then show as a small
     mark in the bottom-right corner (like the "old" badge);
   - a widget whose settings are missing (weather without a place) shows a one-line hint in its box
     ("Weather: set a place in the app") instead of a blank box;
   - an invalid stored layout → Classic, and the reason is shown on Home.
8. **Preview.** While editing, the app draws a sketch (like the prototype). "Exact preview" sends
   the layout being edited to the device: it draws it with `drawFullScreen()` into a temporary 1-bit 800 × 480 canvas
   (48 KB, allocated only for this, freed after) with its cached data (sample data where none),
   and sends it as a 1-bit BMP. The PC renderer stays the reference for golden images.
9. **Checks that need internet** (calendar link, weather place search): in setup mode the phone has
   no internet. The device joins the home network in parallel (AP + station), the saved one or, in
   first start, the one just entered, and runs the checks through it; if that fails, the app says
   the check happens on the first wake. (My call; the
   AP then follows the home network's channel, see risks.)
10. **Security (stage 5 list):** a random per-device setup password, shown on the e-paper together
    with a Wi-Fi QR code (the AP is open today); secrets never sent back; on the home network a code
    shown on the screen (decision 11); POSTs only from the app's own origin.
11. **Edit mode on the home network** (plan A.8): a setup hold on a device with working Wi-Fi opens
    the app on the home network for 10 minutes. The screen shows the address (`<name>.local` + IP),
    a QR code and a **fresh 6-digit code** (owner's choice). Typing the code gives the browser a
    session until edit mode ends; nothing to remember, nothing to recover, and only someone at the
    device can open it. Without working Wi-Fi a setup hold opens the setup AP as today (protected by
    the setup password, no extra code). BOOT held after a reset always opens the AP (unchanged). The
    app has a "Change Wi-Fi" action that switches to the AP. Diagnostics are in the app in both modes.

## Steps

Part 5a: the new app and the editor (what's needed to replace today's page):

1. `web/` skeleton, `tools/web/build.py`, `WebApp.h`, CI check + size limit; served at `/`, old page
   at `/basic`.
2. API: `/api/state`, `/api/networks`, `/api/settings` (validation with per-field errors), secrets
   as "set" / "empty", factory reset and update routes. Host tests for the JSON ↔ settings part.
3. App shell in the Paper look: tabs, lists, fields, error display, save + restart screen.
4. Data and Device tabs: every field of today's page.
5. Layout model + engine changes (decisions 6, 7), `lay_box` storage, schema 3 migration, host tests.
6. Screen tab: structures, box list, widget picker, split / merge (`web/layout.js`, tested with
   `node --test`), live sketch, `/api/layout`.
7. Device: `FW_BUILD` bump, CHANGELOG, user guide (new screenshots), beta release; checked on an
   iPhone and an Android phone.

Part 5b: the rest of the stage list:

8. Exact preview (`/api/preview.bmp`).
9. First-start guide.
10. AP + station checks (calendar link, place search).
11. Setup password + QR code on the screen.
12. Edit mode on the home network with the code on the screen and `<name>.local`.
13. Remove `/basic`; release.

## Tests

- Host (C++): layout string parse / write (round trip, damaged or too long strings → Classic,
  unknown widget, overlaps, outside the grid); engine with empty boxes, without status bar, without
  clock; every structure with every widget that fits; settings JSON → validation errors; secrets
  never in `/api/state` output.
- Web (`node --test`, no packages): split / merge rules, widget fits / greyed out, layout string
  identical to the C++ one for the same boxes (shared test vectors file).
- PC renderer: images of the new structures, of a layout without clock / status bar, of the
  missing-setting hint.
- Device: captive portal opens the app on iPhone and Android; save and restart; every editor
  action shows on the screen as previewed; `/basic` still works; heap after an exact preview.

## Risks

| Risk | Counter |
|---|---|
| Captive-portal browsers (iOS "mini browser") behave differently from Safari / Chrome | plain JS without new features, tested on both phone types; `/basic` fallback during the first release |
| App and firmware disagree about sizes or the layout format | min sizes come from the device; one shared test-vectors file for both parsers |
| 48 KB for the exact preview on top of the setup-mode heap | allocated only on request, refused with a message if the heap is too small |
| AP + station: the AP channel follows the home network, phones may reconnect | checks are optional; without them the first wake reports problems as today |
| Firmware size | app budget 60 KB gzipped (CI); today 1.47 of 1.92 MB used |
| A broken stored layout leaves a blank screen | the engine validates on every load; Classic fallback; reason on Home |

## Out of scope

- **Several screens that take turns (TRMNL playlists):** not decided yet; the layout format has a
  version prefix so it can grow into a list later.
- Free drawing of boxes (editor C), dark mode, finer grids than 4 × 3.
- Tasks, notes and birthdays pages: they come with stage 4 (the editor lists those widgets once they
  exist).
- Longer sleeps when no clock is on the screen: today the device wakes every minute and counts
  wakes for all refresh intervals; changing that belongs to the power profiles (stage 6). Noted
  there as a battery win.
- Editing while the device sleeps (remote management, spec §11).
- The app always reachable on USB power: stage 6, with the USB power profile (owner's choice).

## My calls, for you to check

- The app is compiled into the firmware (not LittleFS, no framework).
- No dark mode in the Paper look.
- Empty boxes stay empty (no automatic growing) in your own layouts; the built-in templates still
  fill gaps.
- Without the status bar, problems show as a small corner mark.
- Setup hold with working Wi-Fi = edit mode on the home network (10 min, code on the screen); the
  AP only without Wi-Fi, via "Change Wi-Fi" in the app, or BOOT after a reset.
- Layout saves apply without a restart; other settings restart as today; first start saves once.
- The editor always saves boxes (Planner etc. become boxes when edited); "Show" switches only for
  Classic.
- Split rule: across the side with more cells, 2 × 2 → two 2 × 1, larger part first.
- New "device name": app header and `<name>.local`, not on the e-paper.
- Part 5a first (replaces today's page), part 5b after it, each with its own beta release.

## Result (build 71)

Parts 5a and 5b done in code, not yet checked on a device or a phone (list:
[DEVICE_TESTS.md](../DEVICE_TESTS.md)). The app was checked with a mock device (`web/dev/`,
open `web/dev/index.html`) and a DOM stand-in, not in a real browser. Decisions taken during the work:

- **The app** (`web/`, plain JavaScript, ES2017 for the iOS captive-portal browser) is 95 KB, 30 KB
  gzipped; compiled in as `src/web/generated/WebApp.h` by `tools/web/build.py`; CI checks that the
  header matches `web/` (hash of the unpacked bundle, so different zlib versions don't matter) and the
  60 KB limit, and runs the editor tests (`node --test tests/web/*.test.cjs`).
- **One set of layout rules, tested twice:** `tests/vectors/layouts.json` (20 cases) runs through the
  app's `validate()` and the firmware's `validateBoxes()`; every built-in template converted to boxes
  draws the same rectangles as the template (host test, every widget combination).
- **Custom layouts** are stored as `v1;sb1;c,r,w,h,key;…` (`src/layout/BoxLayout.h`); the widget keys
  moved into the engine (`WIDGET_KEYS`), so registry, storage and app use one list.
- **No gap filling in box layouts;** a widget without settings is left out of the layout table (nothing
  is fetched) and its box shows a hint from the registry. Without a status bar a change of the problems
  is drawn with a full refresh (the corner mark sits over a widget).
- **The exact preview** draws with the device's data; widgets without data show their empty text
  (no sample data, unlike the plan said).
- **Saving a layout or personal data redraws the dashboard in setup mode** (no restart); the setup
  screen with the password is gone then, the password stays the same.
- **The hotspot password** (8 characters without look-alikes, `ap_pass` in NVS) is new with this
  build: existing devices show it on the setup screen the first time.
- **Edit mode:** session cookie after the code (HttpOnly, SameSite=Strict), every API call checks the
  `Host` header (no DNS rebinding) and requires JSON (no form posts from other sites); the old page
  `/basic` exists only in the hotspot. mDNS name from the device name.
- **The calendar link check** counts the events of a link without touching the calendar on screen;
  **the place search** uses Open-Meteo's geocoding (the same service as the weather).
- Agreed between app and firmware beyond the first API draft (now in `docs/SETUP_API.md`): `{}` keeps
  a calendar link, the saved birthday link can be checked, "Change Wi-Fi" = `/api/finish {"ap": true}`.

Review (fresh context) found, all fixed: the request body was read after it was freed
(`WebServer::arg()` returns a copy; every save would have failed at random on the device); in the
hotspot, after a Wi-Fi test, the home network could reach the setup without the hotspot password
(now only requests over the hotspot for 192.168.4.1, also the old `/basic`); a problem change without
status bar blanked the widgets that weren't fetched in that wake; partial refreshes erased the
problem mark; unlimited code guessing and rejected requests kept edit mode open (now 1 s per wrong
code, edit mode ends after 3 new codes, only accepted requests count as activity); a Wi-Fi test in
edit mode dropped the connection (now answered from the saved password); a second Wi-Fi test could
report the first one's result; update / factory reset / finish didn't require JSON; after a save in
edit mode the code was gone from the screen (a login attempt brings it back); `/basic` replaced an
own layout with Classic when saved; an unknown board preset was silently replaced.

Battery (reported on the device, 2026-11-19: "battery too low" with USB attached): USB is seen only
when a computer is connected, so on a charger the battery reading decided; a reading below 3.0 V
(no battery, or not measurable) now counts as USB power, and the top bar shows `USB`.

Verification: host tests (new `test_box_layout.cpp` incl. the shared vectors), web tests (20), all
profiles compile, images of custom layouts reviewed (top row agenda without clock, without status bar
with a hint and the problem mark, full-screen month).

