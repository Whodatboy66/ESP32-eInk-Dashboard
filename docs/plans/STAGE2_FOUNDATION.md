# Stage 2 plan: firmware foundation

Detailed plan for stage 2 of [IMPLEMENTATION_PLAN.md](../IMPLEMENTATION_PLAN.md) (spec §4, Phase 2).
Rule for the whole stage: **no visible change on the device.** Only structure, verified TLS and
logging change.

## Starting point (build 66)

| File | Lines | Contents |
|---|---|---|
| `ESP32-eInk-Dashboard.ino` | 2800 | settings globals + NVS load/save, pin presets, SuperMini LED, setup page HTML + handlers, AP mode, sparse-font renderer, e-paper init, clock + status bar drawing, battery, button / touch, BOOT button, Wi-Fi, NTP, OTA flow, the whole wake cycle in `setup()` (≈540 lines) |
| `directFetch.cpp` | 1005 | all HTTP fetchers (Open-Meteo, geocoding, Yahoo, iCal, MakerWorld, PKGE, AeroDataBox), each with `setInsecure()` |
| `fetchAllInfo.cpp` | 548 | layout table, widget drawing (events, stocks, tracking, weather, MakerWorld), data globals |
| `bambulab.cpp`, `monthView.cpp` | 337 / 135 | printer over MQTT/TLS (`setInsecure()`), month calendar |
| `app.h`, `display.h`, `configure.h` | | config buffers, display type + `display` macro, compile options + pin presets |

## Design decisions

1. **Move code, don't rewrite it.** Functions are moved verbatim into modules. Changes are limited
   to: declarations in headers, `static` → module-internal, `display.` → `canvas().` in drawing code
   (decision 4), and the TLS client (decision 6). A script compares every moved function with its
   original (whitespace-normalized) so "verbatim" is checked, not assumed.
2. **Keep the settings buffers and NVS keys.** `wifi_ssid`, `ics_urls` & co. stay global `char`
   arrays declared in `app.h` (the fetchers use them), but only `ConfigManager` defines, loads,
   validates and saves them. NVS keys stay byte-identical, so every device keeps its settings
   through the update. A settings struct would touch every fetcher for no visible gain now.
3. **Extract the wake decisions into pure code.** The ≈150 lines in `setup()` that decide "what to
   fetch / redraw this wake" (refresh intervals, flight live window, Wi-Fi backoff, NTP re-sync,
   MakerWorld, Bambu retry) become `WakePlan` (plain C++, no Arduino). The same expressions, now
   host-tested with scenarios; `App` just executes the plan. Biggest behavior risk of the stage,
   so it gets the most tests.
4. **A canvas seam for all drawing.** Drawing code calls `canvas()` (an `Adafruit_GFX&`) instead of
   the `display` macro; refresh control (`setPartialWindow`, `firstPage`, `nextPage`, `hibernate`)
   stays on the e-paper driver. On the device `canvas()` is the display, so nothing changes; on
   the PC it can be an `GFXcanvas1`, which stage 3 uses to render whole layouts into images and
   stage 5 for the phone preview (spec §7.3: one drawing code for e-ink and preview).
5. **One logger, with redaction.** `DBG`/`DBGF` keep their names (≈300 call sites), but `DBGF`
   formats into a buffer and masks every registered secret (Wi-Fi password, MQTT access code, API
   keys, the secret part of iCal URLs) before printing. Redaction is pure code, host-tested.
   `DEBUG 0` still compiles all logging away.
6. **Verified TLS everywhere (S1).** One `SecureClient` (the proven GitHub client from stage 1,
   without the host allow-list): certificate bundle re-attached on every connect, so redirects
   work. Used by every fetcher. Bambu LAN MQTT keeps an unverified TLS connection (the printer has
   a self-signed certificate), but only to a private IPv4 address (10/8, 172.16/12, 192.168/16,
   or a `.local` name); documented as the one exception. iCal feeds on self-hosted servers with
   self-signed certificates stop working: documented (Google, iCloud, Outlook, Nextcloud with a
   real certificate are fine).
7. **ConfigManager** = load / validate / save / migrate / factory reset:
   - `cfg_schema` key in NVS (`FW_CONFIG_SCHEMA` = 1). Devices without the key are schema 0 =
     today's layout; migration 0 → 1 only writes the key (the format is identical). Later stages add
     real migrations here.
   - Validation (pure, host-tested): timezone (stage 1 `normalizeTimezone`), MQTT port 1-65535,
     iCal URLs `https://` / `http://` / `webcal://`, stock symbols. The setup page refuses invalid
     input with a message instead of saving it. (Field lengths are still cut silently by the buffers.)
   - Factory reset: erases the `config` namespace (settings, OTA state) and restarts into setup.
     Triggers: setup page button (with confirmation), and the serial command `factory-reset`
     (typed within the first seconds after a reset, during the BOOT-button window). Spec A.6 recovery.
8. **Host tests stay on the existing plain C++ checks** (`tests/host/check.h`, CI job since stage 0)
   instead of switching to doctest: the plan item is "host test setup in CI", which exists; a
   framework switch adds a dependency without changing what is tested. (Ruling; easy to revisit.)
9. **Dead code goes:** the commented-out old Wi-Fi function, commented debug prints, unused
   variables (`previousMillis`, `lastPrint`, ...), the unused `spiffs`-era leftovers.

## Target layout

```
ESP32-eInk-Dashboard.ino      setup() { appSetup(); }  loop() { setupModeLoop(); }
configure.h, partitions.csv, sketch.yaml   (must stay in the sketch folder)
src/core/        App (wake cycle), WakePlan (pure), ConfigManager (+ ConfigValidation, pure),
                 Logger (+ Redact, pure), Timezone (stage 1), Problems (status-bar problem flags)
src/hardware/    PinMapping, StatusLed (SuperMini), BatteryMonitor (+ BatteryCurve, pure),
                 TouchController (button, BOOT button), EpaperDriver (display, init, partial update)
src/ui/          Canvas (the seam), SparseFont (renderer), fonts/ (moved from /fonts)
src/services/    SecureClient, WifiService (+ backoff, pure), TimeService (NTP),
                 Weather, Stocks, Calendar (iCal), Tracking, Flight, MakerWorld, Bambu
src/widgets/     Clock, StatusBar, Events, Stocks, Weather, Tracking, MakerWorld/Bambu, MonthView
                 (still the fixed layout; stage 3 makes them size-aware)
src/web/         SetupServer (AP mode, page, handlers), Html (escaping, pure)
src/update/      stage 1 modules + OtaFlow (the OTA section of the .ino)
src/input/       ButtonGesture (stage 1)
```

`display.h`, `fetchAllInfo.h`, `bambulab.h`, `monthView.h`, `directFetch.h` became one-line forwarding
headers to `src/hardware/EpaperDriver.h`, `src/widgets/Widgets.h`, `src/services/Bambu.h`,
`src/widgets/MonthView.h`, `src/services/Fetchers.h` (removed in stage 3). `app.h` (the settings
buffers) stays the real header for now.

## Steps (each ends with all four profiles compiling and host tests green)

1. **Tooling** (removed after stage 2, in git history): `tools/dev/check_moves.py` (compares moved functions with `git show <base>:file`),
   `tools/dev/build_all.sh` (the CI compile for all profiles + size table, locally).
2. **Fonts and renderer:** `fonts/*.h` → `src/ui/fonts/`, sparse-font functions → `src/ui/SparseFont`,
   `Canvas` seam; `display.` → `canvas().` in drawing code.
3. **Hardware:** PinMapping, StatusLed, BatteryMonitor, EpaperDriver, TouchController out of the `.ino`.
4. **Services:** SecureClient; split `directFetch.cpp` per service; WifiService, TimeService; Bambu
   with the LAN rule. TLS switches here (the only intended behavior change: certificates are checked).
5. **Widgets:** `fetchAllInfo.cpp` / `monthView.cpp` / clock + status bar → `src/widgets/`.
6. **Web:** setup page + handlers → `src/web/SetupServer`.
7. **Config:** ConfigManager (load/save moved from `setup()` / `handleSave()`), validation, schema key,
   factory reset (setup page + serial).
8. **Logger:** redaction; register secrets after loading the config.
9. **WakePlan + App:** decisions → `WakePlan` (pure), execution → `App`; `.ino` shrinks to two calls.
10. **OTA flow** → `src/update/OtaFlow`. Dead code removed. Docs.

## Tests (host)

- `WakePlan`: first boot, every refresh interval boundary, button wake (refresh all), daily full
  refresh, NTP due, Wi-Fi backoff (with and without a button wake), flight countdown / live window
  edges (6 h before, landed + 1 h, max requests), Bambu printing / idle / retry, MakerWorld hourly,
  tracking expiry redraw, night-time update check, pending-verify (Wi-Fi forced).
- `ConfigValidation`: each rule, good and bad values; migration 0 → 1.
- `Redact`: secrets masked anywhere in a line, short secrets (< 4 chars) not masked, iCal secret
  paths, overlapping secrets.
- `BatteryCurve`, Wi-Fi backoff sequence (2, 4, 8, 16, 30 min), `Html` escaping, private-IP check.
- Existing tests keep passing.

## How "same behavior" is checked (before the device)

1. All profiles compile; image size and static RAM within a few KB of build 66 (TLS adds a little).
2. `check_moves.py`: every moved function identical to its original, apart from the listed edits.
3. `WakePlan` tests encode today's decisions (derived from the current code, line by line).
4. Same boot log on the device (side-by-side check, the plan's "Done when") — that one is yours.
5. A fresh-context review of the whole diff.

## Risks

| Risk | Counter |
|---|---|
| A wake decision changes subtly in the extraction | WakePlan tests written from the current code first, then the extraction |
| Arduino prototype / include order issues when leaving the `.ino` | one module per step, compile after each |
| TLS needs more heap or time; a CA missing from the bundle | measured on the device; per-service error shows in the status bar as before; bundle covers the services used |
| Self-hosted iCal with self-signed certificate breaks | documented; an "allow unverified" option per feed only if someone needs it |
| RTC memory (`RTC_DATA_ATTR`) layout changes with the move | expected and harmless: an OTA restart re-initializes RTC memory |

## Out of scope (later stages)

Layout engine and size-aware widgets (3), per-device setup security, admin PIN and the new setup UI
(5), power profiles (6).

## Result (build 67)

Done as planned; checked on the PC, not yet on the device. Decisions taken during the work:

- **Widgets moved whole** (`fetchAllInfo.cpp` → `src/widgets/Widgets.cpp`, `monthView` → `MonthView`)
  instead of one file per widget: stage 3 rebuilds them around the widget registry, so splitting
  twice would be wasted. Clock and status bar have their own files already.
- **`setup()` became `appSetup()` in one piece** (`src/core/App.cpp`, ≈420 lines): splitting it into
  phases means passing about 20 local flags between them; stage 3's render planner replaces the
  drawing part, so the split happens once, there.
- **No separate HTML-escaping test:** `htmlAttr` works on Arduino `String`; the setup page is rebuilt
  in stage 5.
- **The tracking "expired parcel" redraw** keeps using the interval before a Wi-Fi backoff, exactly as
  before (a subtle ordering the extraction would otherwise have changed).
- **Fonts once:** the font headers define `const` arrays, so every file that included them had its
  own copy; `src/ui/Fonts.cpp` now holds the only one (−17 KB).
- **Found on the way:** a header declared `isPrinting` with `RTC_DATA_ATTR` (an attribute that belongs
  only on the definition; the compiler ignored the second one), and a deprecated
  `StaticJsonDocument`; both fixed. The setup page saved half-copied values when a custom pin map
  was invalid; validation now happens before anything is copied.

Review (fresh context) found no critical issue and confirmed the wake cycle is equivalent. Fixed
after it: printer **names** from the router's DNS are accepted again (the resolved address must be
local, and the connection goes to that address), stock lists are **normalized** instead of refused
(spaces, trailing commas, more than 5 worked before), `http://` iCal links become `https://` (plain
HTTP never worked: every request uses TLS), the serial factory reset confirms a pending update and
keeps the rolled-back build number, `cfg_schema` is read without opening NVS for writing, and the
move checker now also sees functions whose `{` is on the next line.

Verification: all profiles compile without warnings from our code; `tools/dev/check_moves.py` reports
138 moved functions unchanged and 12 changed, all intended: certificate check (`httpGetJson`,
`readIcsFeed`, MakerWorld, tracking, flight), `JsonDocument` (`callback`), local-network check
(`fetchBambu`), validation (`handleSave`), factory reset (`buildPage`, `startAP`), and `setup`/`loop`
(now `appSetup()` + `setupModeLoop()`); 10 host test files pass (wake plan, config validation,
redaction, LAN rule, battery curve, plus stage 1's).
