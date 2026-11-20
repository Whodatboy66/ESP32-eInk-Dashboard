# Implementation plan: modular e-ink dashboard platform

Source of requirements: "Development Specification: Modular E-Ink Smart Dashboard Platform"
(Google Doc, 20 chapters; referred to as **spec §n** below).
Related: [PROJECT_INFO.md](PROJECT_INFO.md) (hardware, workflow), [OTA_UPDATES.md](OTA_UPDATES.md) (update design).

Ground rules (spec §14, §20): small stages, each stage builds and runs before the next starts,
existing working code is reused, nothing is reported as hardware-tested unless it was tested on the device.

---

## Part A: Repository assessment (spec Phase 1)

**Original assessment (upstream .63, 2026-11-15) — historical.** It describes the code before the
stages: file names, sizes and "today" refer to that state. Notes marked *Now* say what changed where a
statement would mislead today; the current code is described in the [README](../README.md#repository-layout).

Based on upstream commit `8d0a66b` (FW .63) plus our fork fixes.

### A.1 Source files and responsibilities

| File | Lines | Responsibility | Verdict |
|---|---|---|---|
| `ESP32-eInk-Dashboard.ino` | 2012 | Everything else: setup page HTML + handlers, pin presets, button logic, Wi-Fi, NTP, clock/status drawing, sparse-font renderer, wake/sleep scheduler, `setup()` | **Split up** (stage 2) |
| `directFetch.cpp/.h` | 1038 | HTTP + JSON fetchers: Open-Meteo, geocoding, stocks, MakerWorld, iCal parser (recurrence, exceptions), PKGE tracking, AeroDataBox flights | **Keep logic**, move behind a service interface, fix TLS |
| `fetchAllInfo.cpp/.h` | 714 | `LayoutItem` table with fixed positions (`loadDefaultLayout`), widget draw functions (calendar events, stocks, tracking, weather, MakerWorld), RTC caches | **Basis for widget registry**; replace fixed coordinates with the layout engine |
| `monthView.cpp` | 135 | Month calendar widget | Keep, wrap as widget |
| `bambulab.cpp/.h` | 405 | Bambu printer over MQTT/TLS | Keep as optional widget, developer device only (shown only when a printer is configured) |
| `configure.h` | 171 | Compile options, pin presets, board detection | Becomes `BoardConfig` / `PinMapping` |
| `fonts/` | 224 KB src | Sparse fonts + Material Design Icons | Keep |
| `tools/epd_test/` | ours | Display bring-up test | Keep |

Already good and reused unchanged: deep-sleep wake cycle with per-widget refresh intervals,
RTC-memory caches (calendar, tracking survive sleep), partial refresh per layout item
(`setPartialWindow`), battery curve + factory ADC calibration, captive-portal setup flow,
2 s / 4 s button gestures, iCal recurrence parser.

### A.2 Memory and flash (measured builds, core 3.3.7)

| Build | Flash (app) | Static RAM |
|---|---|---|
| esp32c6_firebeetle | 1,354,854 B | 106,884 B of 327,680 (32 %), about 220 KB left for heap |

Wi-Fi + TLS + HTTP make up most of the image, so new widgets cost relatively little. The full display
frame buffer (800x480 1 bpp = 48 KB) is static.

### A.3 Flash budget on the 4 MB FireBeetle (spec §9.2)

Proposed OTA layout (Zigbee partitions dropped; Zigbee support removed, see stage 0):

| Partition | Offset | Size | Purpose |
|---|---|---|---|
| nvs | 0x9000 | 20 KB | settings + secrets (offset unchanged, so current settings survive) |
| otadata | 0xE000 | 8 KB | active slot |
| app0 / app1 | 0x10000 / 0x1F0000 | 2 x 1.875 MB | A/B firmware |
| userdata (LittleFS) | 0x3D0000 | 128 KB | tasks, notes, birthdays, layout config, data caches |
| coredump | 0x3F0000 | 64 KB | crash diagnostics |

*Now:* this layout is in `partitions.csv` since build 64. The userdata partition holds tasks, the
note, birthdays, the diagnostics log and the battery history; the layout is stored in NVS. Sizes:
the CI run summary of each build.

Today's firmware uses **69 %** of a slot, leaving about 600 KB of headroom. Planned additions (layout engine,
web UI assets gzipped in flash, new widgets, signature check, QR code) are estimated at 100-250 KB,
so this fits. The CI size check (stage 0) watches the limit. If the firmware ever outgrows 1.875 MB:
first drop optional modules from the family build (Bambu/MQTT, stocks); then move to an ESP32-C6
module with 8 MB flash (same chip, same code).

### A.4 Security findings (spec §12)

| # | Finding | Where | Fix in stage |
|---|---|---|---|
| S1 | `setInsecure()`: no TLS certificate check on any HTTPS request | `directFetch.cpp` (5x), `bambulab.cpp` | 2: done (build 67), certificate bundle everywhere; Bambu LAN only to a local address |
| S2 | Setup Wi-Fi `Dashboard-Setup` was **open** (no password) | `startAP()` | 5: done (build 71), per-device WPA2 password, shown on the e-ink screen + Wi-Fi QR code |
| S3 | Setup page **echoed saved calendar URLs** to anyone connected to the AP | `handleConfig` (`htmlAttr(ics_urls[i])`) | 5: done (build 71), the app shows "Saved", secrets are never sent back |
| S4 | Setup page had no authentication | web server | 5: done (build 71), hotspot password; on the home network a 6-digit code shown on the screen and a session |
| S5 | No firmware authenticity check (no OTA yet) | n/a | 1: done, signed manifest (build 66) |
| S6 | Secrets partly redacted in logs already (recent upstream commit) | debug log | 2: done (build 67), `DBGF` masks registered secrets |

### A.5 Licensing (spec §1: "review licensing")

- **The upstream repo has no license file** (GitHub: `license: null`). By default that means all rights
  reserved: forking on GitHub is allowed, but modifying and distributing it is legally unclear.
  Private family use is low-risk. **Recommended: open an issue upstream asking the author to add a license
  (e.g. MIT)** before sharing builds more widely. Status: issue posted upstream (2026-11-19), no answer yet.
- Fonts: Montserrat (OFL 1.1, fine), Material Design Icons (Apache 2.0, fine),
  **HighSpeed font: personal use only**. OK for family devices, not for anything commercial.

### A.6 TTP223 touch input (spec §3.4)

- TTP223 module default: **active-HIGH push-pull output** (HIGH while touched; solder jumper A inverts
  the polarity, jumper B switches to toggle mode). Leave both jumpers open.
- The firmware already treats HIGH as "pressed" and wakes on EXT1 `ANY_HIGH`, which is correct for the TTP223.
  **Bug:** the input is configured with `INPUT_PULLUP`. With no sensor or a disconnected sensor, the pin
  reads "held" and the device enters setup mode by itself. Fix: `INPUT_PULLDOWN`, plus a pull-down kept
  during deep sleep (GPIO2 is an LP GPIO on the C6, so that's supported).
- The TTP223 auto-calibrates at power-up: don't touch it while powering on. It can also report a long
  "touch" after recalibration problems. Add a sanity limit (e.g. a "touch" longer than 15 s is ignored
  as a fault and not counted as a gesture).
- *Now:* pull-down, fault limit and BOOT backup are done (stage 0) and were tested on the device on 2026-11-19.
- Standby current: datasheet low-power mode is in the µA range, but module LEDs add current. **Measure it** (stage 6).
- **Recovery if the sensor fails:** the FireBeetle's **BOOT button (GPIO9)** works as a backup:
  tap RST, then hold BOOT within 3 s for setup mode. Also: factory reset from the setup page and
  over USB serial.

### A.7 Power (spec §8)

Today: wake every minute, redraw the clock with partial refresh (about 2 s awake without Wi-Fi), Wi-Fi only
when a widget is due, CPU at 80 MHz for clock-only wakes, display power gated by GPIO4 with `gpio_hold`
during sleep. That's a good base. Needed: power profiles (clock interval 1/5/15 min), low-battery mode,
retry backoff, **real current measurements** (USB power meter for averages, and ideally a PPK2 or
µCurrent for sleep current) instead of estimates.

*Now:* retry backoff (build 68: per service; build 73: printer), clock interval 1/2/5/10/15/30 min,
night quiet hours, automatic battery saver and the critical-battery screen (build 75) are done.
Open: power profiles and the current measurements (stage 6).

### A.8 Key design constraint: the device is asleep most of the time

The phone web interface (spec §7) can only be reached while the device is awake. Tasks and notes
editing therefore needs an explicit "edit mode": a long touch opens the web interface on the
home network (not just the setup AP) for a few minutes, the screen shows the address and a QR code, and
on USB power ("USB Powered" profile) it stays reachable. Editing while the device sleeps
(spec §11 remote management) is a later phase.

*Now:* edit mode is done (build 71): a 4-second hold with working Wi-Fi, 10 minutes, address, QR code
and a 6-digit code on the screen. Staying reachable on USB power is open (stage 6).

### A.9 Answers to the spec's "Immediate Assignment"

| Question | Answer |
|---|---|
| Keep unchanged? | iCal parser, Open-Meteo/geocoding, stocks/tracking/flight fetchers, Bambu MQTT, sparse-font renderer, battery curve, sleep/wake scheduler logic, GxEPD2 setup |
| Refactor? | The 2,000-line `.ino` (setup page, buttons, Wi-Fi, scheduler, drawing all mixed), fixed-coordinate layout, global config variables, TLS handling |
| Modular widgets? | Static registry of `WidgetDef` structs (id, name, sizes, min size, refresh interval, config keys, `fetch()`, `render(ctx, bounds)`), with no heap allocation. Grows out of the existing `LayoutItem` table |
| Layout engine? | Grid (e.g. 4 x 3 cells on 800x480) + named templates (Classic, Planner, Minimal, Technical); validation and fallback in pure C++ (unit-tested on the PC) |
| Mobile UI? | Responsive single page (HTML/JS gzipped in firmware, so it updates with OTA) + small JSON API. **Preview = the device renders the real screen into an 800x480 bitmap** and serves it as an image. The same code draws the e-ink and the preview, so nothing can drift (spec §7.3) |
| Secure OTA? | A/B partitions, signed manifest (ECDSA P-256, public key in firmware, private key only in a GitHub secret / on the developer PC), SHA-256 of the image, board/display/schema checks, rollback via the bootloader, stable/beta channels. See OTA_UPDATES.md |
| Enough flash? | Yes, with the layout in A.3 (about 600 KB headroom per slot). Limit and fallback in A.3 |
| TTP223? | Pull-down instead of pull-up, fault limit, BOOT button as backup. See A.6 |
| Power? | Profiles, low-battery mode, backoff, measurements. See A.7 / stage 6 |

### A.10 Toolchain decision

Stay with **arduino-cli + `sketch.yaml`** (it works on WSL, Windows and CI). New modules go in `src/`
(Arduino compiles it recursively). Hardware-independent logic (layout, config validation, scheduling,
manifest checks) is plain C++ without Arduino headers, so it can be **unit-tested on the PC with g++** in
CI (spec §15). Switching to PlatformIO isn't needed for this. (*Now:* plain C++ checks, no doctest; see stage 2.)

---

## Part B: Stages

Order differs slightly from spec §14: **OTA comes early (stage 1)**. Every later stage then reaches devices
without USB, and the partition change (the only step that needs USB) happens once, before devices are
handed out. Every stage ends with a `FW_BUILD` bump, a line in [CHANGELOG.md](../CHANGELOG.md) and, from stage 1 on, a beta release.

**Where we are (2026-11-20, build 76):** stages 0-5 and stage 6 part 1 (clock interval, night quiet
hours, battery saver, build 75) are implemented. Two device test rounds found fixes that went into
builds 73 and 76; the rest of the open checks are in [DEVICE_TESTS.md](DEVICE_TESTS.md). Releases:
build 70 (stable and beta), build 76 (beta). Next: [plans/LOOK_AND_LANGUAGE.md](plans/LOOK_AND_LANGUAGE.md),
part 1 (German / English), then part 2 (the new look); then the rest of stage 6 (profiles, USB
detection, current measurements). Not done on purpose so far (details in the plans' "Result"
sections): the old setup page `/basic` stays until the app is checked on phones; the exact preview
uses the device's own data (no sample data); agenda times stay 24 h with the 12-hour clock; tasks
can't be ticked off on the device; one note only; several alternating screens (TRMNL playlists) not
decided.

### Stage 0: Reproducible base (spec Phase 1)
- [x] Fix `sketch.yaml` profiles (partition, Zigbee flag): done `daa506c`
- [x] Display bring-up test `tools/epd_test`
- [x] Assessment (Part A)
- [x] Remove Zigbee (smart-plug automation, unrelated to the dashboard): code, profiles, CI, README
- [x] Align CI with `sketch.yaml`: CI builds the profiles (`--profile`), so core and library versions come from one place; library patches dropped (not needed for the pinned versions)
- [x] Enable Actions on the fork (Actions tab → "I understand my workflows, go ahead and enable them"); first green run on GitHub: 2026-11-16
- [x] CI: compile on every push and pull request + size check (fails above the planned 1.875 MB OTA slot, flash/RAM table in the run summary); host tests (`tests/host/run.sh`); releases only from a manual run with "release" ticked
- [x] Button input fix (A.6): pull-down (kept in deep sleep), 15 s fault limit (no gesture, no button wake while stuck), setup on release after 4 s; BOOT button backup (tap RST, then hold BOOT 1 s). Gesture logic in `src/input/ButtonGesture.h` with host tests. Tested on hardware (reported by user, 2026-11-19)
- [x] License question to upstream (A.5): issue posted (reported by user, 2026-11-19); waiting for the author's answer
- [x] Repository layout: short README, manual in `docs/USER_GUIDE.md`, `docs/images/`, font converter in `tools/`, `CHANGELOG.md` (code moves into `src/` with stage 2)

**Done when:** a push builds in CI with a size report; the device boots into the dashboard with no sensor connected.

### Stage 1: Secure OTA (spec §9, Phase 6)
All parts implemented; open are the remaining "Done when" tests on the device (see below).

- [x] OTA partition table (A.3); one-time USB migration, settings kept (nvs offset unchanged)
- [x] `FW_BUILD` (64) and board variant (`FW_VARIANT` = sketch.yaml profile) compiled in
- [x] Display id (`FW_DISPLAY`) and config-schema version (`FW_CONFIG_SCHEMA`) compiled in, listed per file in the manifest (read out of the image by CI) and checked
- [x] Update client (`src/update/`): daily check from 03:00 until noon (03:00-03:59 until build 74; with quiet hours over 03:00 at their end), manifest download over verified HTTPS (certificate bundle), board-variant and min-build checks, no downgrades, SHA-256 check while writing, size check
- [x] **ECDSA signature check** of the manifest (P-256, `manifest.sig`, public key in `src/update/OtaPublicKey.h`); unsigned or altered manifests are rejected
- [x] **Battery guard (decided):** below 30 % battery no update is installed unless USB is connected; the device retries at the next check
- [x] Rollback: new firmware is confirmed only after Wi-Fi + display refresh succeed; without Wi-Fi it rolls back right away (clean restart); a hang rolls back after 3 min (timer restart); using the button (hold, setup, update screen) confirms it, so a user restart doesn't roll back (not in test builds, 90001+, which also take any regular release again); a build that rolled back is not auto-installed again
- [x] Channels stable/beta ("Update channel", setup app Device tab; the old page until build 70); beta reads the fixed `beta` pre-release, which every release refreshes
- [x] CI: release job publishes app binaries + `manifest.json` only when triggered manually; the "What's new" input (max. 3 lines) goes into the manifest and the release notes
- [x] CI: signs the manifest with the `OTA_SIGNING_KEY` secret and checks it against the firmware's public key before publishing; channel input, beta = pre-release
- [x] Host tests: manifest parsing, version and compatibility rules (incl. display, settings schema), signature check, channel URLs, battery-guard and auto/notify decisions, double tap
- [x] Secret `OTA_SIGNING_KEY` set on GitHub, offline backup of the private key (me)

**Update triggers and settings (decided):**
- [x] **Switch "Install updates by themselves"** (setup app Device tab; on the old page "Install updates automatically"; per device, default on). On: the night-time check installs by itself. Off (notify only): the night-time check only looks; a new version shows a small **"Update available"** mark in the status bar until it's installed
- [x] **Double tap on the TTP223 = info and update screen** ("do it now", works in both modes). Add `ButtonEvent::DoubleTap` to `src/input/ButtonGesture.h` with host tests: second tap within about 400 ms; a single tap is then reported after that window (refresh starts about 0.4 s later). Must also work as the first wake from deep sleep (first tap wakes, second tap is caught after boot)
- [x] **Button "Check for updates now"** (setup app Device tab, before build 71 the setup page): backup trigger if the touch sensor fails. The setup network goes away when the device restarts to go online, so the result appears on the e-ink screen (and in the app next time); a newer version is installed without a confirm tap

**Update screens (e-ink, partial refresh about 1.6 s per step):**
The font has no ✓, so the check mark is drawn with lines. (Before build 70 it also lacked `( ) ! '`, hence wording like "Tap to install - cancels in 15 s".)
- [x] Info screen (double tap): firmware version, channel, auto install, last update date, battery %, Wi-Fi status, then "Checking for updates…"
- [x] Update available: "New version .65", the "What's new" lines, **"Tap to install - cancels in 15 s"**. No tap: back to the dashboard (protects against accidental touches)
- [x] Progress: Downloading (bar refreshed every 20-25 %), Verifying, Installing, Restarting
- [x] After the restart, shown once: "Updated to .65" with the drawn check mark + "What's new", then the dashboard
- [x] Plain-language results: "Up to date: .64", "No Wi-Fi: try again later", "Battery too low for an update: please charge", "Update failed: previous version kept"
- [x] Status bar shows the current version (and the "Update available" mark in notify-only mode)

**Done when** (status: ✔ = confirmed on the device, see hardware test log):
- [x] settings survive (USB migration and update)
- [x] installs via double tap + confirm tap (auto off)
- [ ] installs itself at night (auto on); ☐ notify-only shows the status-bar mark; [ ] battery refusal below 30 % without USB
- [x] corrupted image rejected; ✔ wrong-board image rejected; ✔ unsigned image rejected; ✔ release that crashes on boot rolls back (workflow "OTA test release", procedure in [OTA_TESTS.md](OTA_TESTS.md))
- [x] the same with a **beta** release

Original wording: a beta release installs itself on my device at night (auto on) and via double tap + confirm tap (auto off); notify-only shows the status-bar mark; below 30 % without USB it refuses with the battery message; a corrupted, unsigned or wrong-board image is rejected; a release that crashes on boot rolls back; settings survive.

### Stage 2: Firmware foundation (spec §4, Phase 2)
No visible changes, only structure:
Detailed plan and result: [plans/STAGE2_FOUNDATION.md](plans/STAGE2_FOUNDATION.md). Done in code (build 67); checked on the device in the first test round (DEVICE_TESTS block 2, all ticked).
- [x] Split the `.ino` (2800 lines → 14) into `src/core` (App, WakePlan, ConfigManager, Logger, Problems), `src/hardware` (PinMapping, StatusLed, BatteryMonitor, TouchController, EpaperDriver), `src/services` (Weather, Stocks, Calendar, Tracking, Flight, MakerWorld, Bambu, Wi-Fi, Time, SecureClient), `src/widgets`, `src/ui` (canvas seam, sparse fonts), `src/web` (setup page), `src/update` (OTA flow); moved functions checked verbatim (`tools/dev/check_moves.py` (removed after stage 2))
- [x] ConfigManager: schema version (`cfg_schema`), validation before saving (timezone, iCal links incl. `webcal://`, stock symbols, MQTT port, printer in the local network), migration hook, factory reset (setup page + serial `factory-reset`); secrets stay in NVS
- [x] TLS: certificate bundle for every HTTPS request (S1); Bambu LAN MQTT the one exception, only to a local network address
- [x] Logger: `DBGF` masks Wi-Fi password, access code, API keys and iCal links (S6)
- [x] Host tests in CI (plain C++ checks, no doctest: ruling in the stage plan): wake plan, config validation, redaction, LAN rule, battery curve, plus stage 1's
- [x] Side-by-side check on the device (same screen, same boot log, every service still loads)

**Done when:** behavior is the same as before (side-by-side check on the device), CI is green, every HTTPS request is verified.

### Stage 3: Widget system + layout engine (spec §5, §6, Phase 3)
Detailed plan and result: [plans/STAGE3_WIDGETS_LAYOUT.md](plans/STAGE3_WIDGETS_LAYOUT.md). Done in code (build 68); rendered on the PC: [docs/images/layouts/](images/layouts/). On the device: all templates switch with a full refresh and partial refreshes work; gap filling, the 12-hour clock and the "old" badge are still open (DEVICE_TESTS block 3).
- [x] Static widget registry (`src/widgets/Registry.cpp`): clock (12/24 h, timezone; large, centered in any size), weather (days that fit), agenda + month view (multiple feeds), status bar, and the technical ones (printer / MakerWorld, stocks, tracking); flights inside the agenda. Compact 1×1 clock dropped (no medium font; see the plan result)
- [x] Grid layout engine (4 × 3 cells, `src/layout/`) + templates Classic (today's exact pixels) / Planner / Minimal / Technical; validation, fallback to Classic, disabled widgets leave no gaps (neighbors grow)
- [x] Render planning: one full-screen drawing for device and PC (`drawFullScreen`), every widget clipped to its place; full refresh on a layout change (signature in RTC), button wake, setup, daily; partial refreshes otherwise
- [x] Per service retry after a failure (2, 4, 8, 16 min, capped at the interval, then the regular schedule) and an "old" badge on stale data (spec §10.2)
- [x] Host tests: layout (every template × every widget combination), fitting, min/max sizes, invalid templates → fallback, service state, retries in the wake plan; PC renderer for the images
- [x] Setup page: Layout, Clock (24 h / 12 h), Show (per widget); settings schema 2 (since build 71 in the setup app's Screen tab)
- [x] On the device: all templates and switching between them (DEVICE_TESTS block 3)

**Done when:** widgets and the layout are switched through configuration only; all four templates render correctly on the device.

### Stage 4: Personal widgets (spec §5.3)
Detailed plan: [plans/STAGE4_PERSONAL_WIDGETS.md](plans/STAGE4_PERSONAL_WIDGETS.md). Done in code (build 70); partly checked on the device (DEVICE_TESTS block 5).
- [x] LittleFS user-data partition; storage for tasks / notes / birthdays with size limits
- [x] Tasks (add, delete, done, due date, priority, show the top N)
- [x] Notes (multi-line, length limit)
- [x] Birthdays (from a chosen calendar feed or manual list; days remaining)

**Done when:** all three are editable (through a temporary JSON API at first, since build 71 in the setup app) and survive restarts and OTA.

### Stage 5: Smartphone configuration v2 (spec §7, Phase 4)
Detailed plan: [plans/STAGE5_SETUP_APP.md](plans/STAGE5_SETUP_APP.md) (editor: structure, then split / merge boxes, then a widget per box; look "Paper"). Done in code (build 71); partly checked on the device, fixes in builds 73 and 76; open checks in [DEVICE_TESTS.md](DEVICE_TESTS.md) block 6.
- [x] Secure provisioning: per-device AP password shown on the e-ink screen + Wi-Fi QR code (S2); on the home network a fresh code shown on the screen instead of an admin PIN (S4); secrets never echoed (S3)
- [x] First-time setup flow: Wi-Fi (scan, test), name and place, calendars, screen
- [x] Tabs Home (preview, problems, update, device state), Screen (layout editor, clock, power), Data (calendars with URL check and clear errors, weather place search, keys, printer, tasks, note, birthdays), Device (name, Wi-Fi, time zone, updates: channel, last result, check now; pins; factory reset)
- [x] **Layout editor:** choose a structure (full, halves, boxes …), split / merge boxes on the 4 × 3 grid, pick a widget per box; clock and status bar optional
- [x] **Preview:** device-rendered 800x480 image of the configured layout
- [x] Edit mode on the home network (setup hold, 10 minutes), see A.8; "always on USB power" moved to stage 6
- [x] Error texts for non-technical users; problems with what to do on the Home tab (behind the setup password / screen code); **diagnostics** (log, battery history, service state) in build 72, see [plans/STAGE5B_DIAGNOSTICS.md](plans/STAGE5B_DIAGNOSTICS.md)

**Done when:** a person without ESP32 knowledge sets up and personalizes a device using only a phone.

### Stage 6: Power management (spec §8, Phase 5)
Part 1 (build 75): [plans/STAGE6_POWER.md](plans/STAGE6_POWER.md): clock interval, night quiet hours,
battery saver, critical screen, estimates. Done in code, not yet checked on the device.
- [ ] Power profiles Battery Saver / Balanced / USB Powered; USB detection; on USB the setup app stays reachable on the home network (moved from stage 5)
- [ ] Configurable refresh intervals per data type
- [ ] **Battery use measured, not estimated.** First numbers (2026-11-20, build 73, from the
  diagnostics log): 3 % in 13 hours, about 2-4 weeks per charge; the clock wakes are the largest
  share, then the hourly Wi-Fi wake. Measure with a USB power meter and replace the estimates in
  `src/core/PowerPlan.h` (quiet hours, clock interval and battery saver are done, see below);
  calendar every 2 hours (or per calendar) still open
- [x] **Clock refresh interval** as a setting (1-30 min) with the estimated battery life per choice; **night quiet hours** (default 23:00-06:00, configurable) (decided 2026-11-20, build 75)
- [x] **Automatic battery saver (decided 2026-11-19), whatever profile and refresh settings are chosen** (build 75):
  - below **20 %** (about 3.73 V, where the LiPo curve starts to drop fast), on battery (a plausible
    reading, no computer USB): the screen refreshes **every 15 minutes** instead of every minute; data
    fetches keep their intervals, rounded up to a multiple of 15 minutes; one full refresh a day;
  - back to normal above **30 %** (no flapping around the threshold) or when a computer's USB is seen
    (a USB charger isn't detected);
  - below **5 %**: a "Please charge" screen; only a battery check once an hour (nothing drawn), back
    above 10 % or on a computer's USB;
  - the setup app's Home tab says why;
  - a reading that isn't plausible (no battery measured, below 3.0 V) never triggers it (that's USB);
  - expected effect: about 15 times fewer wakes, the largest part of the battery use (to be measured);
  - the clock shows the time of the last refresh, with the note "Battery saver: every 15 min" in the
    status bar (decided 2026-11-20)
- [x] Low-battery threshold, reduced updates, critical-battery screen, then stay asleep (see the
  battery saver above; build 75)
- [x] Touch wake + gestures validated with the TTP223 on hardware (2026-11-19, hardware test log; the edit-mode hold is in DEVICE_TESTS block 6)
- [ ] **Measurements:** sleep current, clock-wake energy, Wi-Fi-wake energy, TTP223 + HAT standby, and the battery life calculated from them; documented in `docs/POWER.md`

**Done when:** measured values and the resulting battery life are documented per profile.

### Stage 7: Product validation (spec §16, §17, Phase 7)
- [ ] Hardware test checklist executed (first boot, Wi-Fi loss, invalid input, sleep/wake, touch, persistence, OTA + failed OTA, power cut, 7-day run)
- [ ] Docs: README, wiring + TTP223, battery requirements, setup guide, user guide, update guide, recovery guide, architecture, "how to add a widget"
- [ ] Example profiles: personal planner, family planner, minimal desk clock, technical dashboard
- [ ] Validation report: implemented / deferred / tests run / hardware tests done / limits / memory / open security points

**Done when:** a release candidate goes out to the first family device via the stable channel.

### Later (not part of the initial release, spec §10.3, §11)
- Optional integration service (Python/FastAPI) for Proxmox, NAS, home lab, MQTT, Home Assistant; the ESP32 fetches a lightweight summary
- Remote device management (device list, versions, battery, channels, messages to a device) with per-device credentials, outbound connections only
- Further widgets: habits, shopping list sync, energy, network monitoring, robotics telemetry

---

## Hardware test log

Only tests actually performed on the device go here.

| Date | Build | Test | Result |
|---|---|---|---|
| 2026-11-15 | .63 + fixes | Display bring-up (`epd_test`), FireBeetle C6 + Waveshare HAT + 7.5" B/W | Refresh OK (about 3.4 s) with firm wiring; failures traced to loose contacts |
| 2026-11-16 | .63 + fixes | Dashboard after setup, Wi-Fi, weather, NTP | Running (reported by user) |
| 2026-11-16 | .64 → .65 | USB migration to the OTA partition table (build 64), then over-the-air update to build 65 | Settings kept after the USB flash and after the update (reported by user) |
| 2026-11-16 | .64 → .65 | Double tap on the TTP223 → info screen → "New version .65" → confirm tap → download from the GitHub release, install, restart | Installed and running .65 (reported by user). Confirms the certificate bundle fix for GitHub's redirect |
| 2026-11-17 | .66 | OTA tests on the beta channel: wrong board, bad checksum, unsigned, crash on boot, beta release | All as expected: rejected / rolled back (reported by user) |
| 2026-11-17 | .66 → .90005 | OTA test "hang", then `restore` | Test build kept instead of rolled back (most likely confirmed by a button use during the hang); afterwards every release showed "Up to date: .90005" because test builds count as newer. Fixed in build 69 (see CHANGELOG); the device needs one USB flash (reported by user) |
| 2026-11-19 | .70 / older | Update with USB attached; battery voltage in the top bar; charging | "Battery too low" refused the update with USB attached: a USB charger isn't seen as USB, and the battery read 1.1-1.4 V with the battery plugged in (about 3.7 V without it); the charge LED stays on. The inline adapter between the battery's plug and the board crosses red and black: reversed polarity or an over-discharged cell suspected, battery unplugged until measured (DEVICE_TESTS block 0b). Software: from build 71 a reading below 3.0 V counts as USB power (reported by user) |
| 2026-11-19 | before 71 | Button (stage 0 / A.6): boot without sensor, tap, double tap, 2 s and 5 s holds, sensor held HIGH 15 s, BOOT backup | All as expected (reported by user). Opening edit mode with the 5 s hold is new in build 71 and is checked in DEVICE_TESTS block 6 |
| 2026-11-19 | .72 | Diagnostics view and log download on the device; battery reading over time | Diagnostics work (firmware line, log, download). Battery: 0.06-0.11 V without a battery, a steady 1.16-1.17 V with it on the charger (charge LED on): not charging. Either the cell is over-discharged, or the inline adapter (red and black crossed) connects it reversed: with a charger attached a reversed cell is reverse-charged, so the positive reading proves nothing. To check: battery red must end on the board's "+" pin; cell voltage at the battery's own plug (reported by user). The log showed: each printer connection attempt 30 s (printer not answering, 1 minute per wake), times jumping between UTC and local time after every Wi-Fi connect, a calendar with 1090 events (10 s per read). Fixed in build 73 |
| 2026-11-19 | .72 | Battery after flipping the wires of the inline adapter (battery red now on the board's "+") | Charges: 4.22 V on the charger, 3.91 V (67 %) shortly after; the cell survived the reversed connection. Cause of the 1.17 V: the adapter connected the battery reversed (reported by user) |

## Changelog

Moved to [CHANGELOG.md](../CHANGELOG.md).
