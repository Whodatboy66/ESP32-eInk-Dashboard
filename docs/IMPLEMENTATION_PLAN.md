# Implementation plan: modular e-ink dashboard platform

Source of requirements: "Development Specification: Modular E-Ink Smart Dashboard Platform"
(Google Doc, 20 chapters; referred to as **spec §n** below).
Related: [PROJECT_INFO.md](PROJECT_INFO.md) (hardware, workflow), [OTA_UPDATES.md](OTA_UPDATES.md) (update design).

Ground rules (spec §14, §20): small stages, each stage builds and runs before the next starts,
existing working code is reused, nothing is reported as hardware-tested unless it was tested on the device.

---

## Part A: Repository assessment (spec Phase 1)

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

Today's firmware uses **69 %** of a slot, leaving about 600 KB of headroom. Planned additions (layout engine,
web UI assets gzipped in flash, new widgets, signature check, QR code) are estimated at 100-250 KB,
so this fits. The CI size check (stage 0) watches the limit. If the firmware ever outgrows 1.875 MB:
first drop optional modules from the family build (Bambu/MQTT, stocks); then move to an ESP32-C6
module with 8 MB flash (same chip, same code).

### A.4 Security findings (spec §12)

| # | Finding | Where | Fix in stage |
|---|---|---|---|
| S1 | `setInsecure()`: no TLS certificate check on any HTTPS request | `directFetch.cpp` (5x), `bambulab.cpp` | 2 (certificate bundle; Bambu LAN uses a self-signed cert, so pin it or accept on LAN only) |
| S2 | Setup Wi-Fi `Dashboard-Setup` is **open** (no password) | `startAP()` | 5 (per-device WPA2 password, shown on the e-ink screen + QR code) |
| S3 | Setup page **echoes saved calendar URLs** to anyone connected to the AP | `handleConfig` (`htmlAttr(ics_urls[i])`) | 5 (show "saved", never echo) |
| S4 | Setup page has no authentication | web server | 5 (admin PIN / pairing) |
| S5 | No firmware authenticity check (no OTA yet) | n/a | 1 (signed manifest) |
| S6 | Secrets partly redacted in logs already (recent upstream commit) | debug log | 2 (central logger with redaction) |

### A.5 Licensing (spec §1: "review licensing")

- **The upstream repo has no license file** (GitHub: `license: null`). By default that means all rights
  reserved: forking on GitHub is allowed, but modifying and distributing it is legally unclear.
  Private family use is low-risk. **Recommended: open an issue upstream asking the author to add a license
  (e.g. MIT)** before sharing builds more widely.
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

### A.8 Key design constraint: the device is asleep most of the time

The phone web interface (spec §7) can only be reached while the device is awake. Tasks and notes
editing therefore needs an explicit "edit mode": a long touch opens the web interface on the
home network (not just the setup AP) for a few minutes, the screen shows the address and a QR code, and
on USB power ("USB Powered" profile) it stays reachable. Editing while the device sleeps
(spec §11 remote management) is a later phase.

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
manifest checks) is plain C++ without Arduino headers, so it can be **unit-tested on the PC with g++ and
doctest** in CI (spec §15). Switching to PlatformIO isn't needed for this.

---

## Part B: Stages

Order differs slightly from spec §14: **OTA comes early (stage 1)**. Every later stage then reaches devices
without USB, and the partition change (the only step that needs USB) happens once, before devices are
handed out. Every stage ends with a `FW_BUILD` bump, a changelog line and, from stage 1 on, a beta release.

### Stage 0: Reproducible base (spec Phase 1)
- [x] Fix `sketch.yaml` profiles (partition, Zigbee flag): done `daa506c`
- [x] Display bring-up test `tools/epd_test`
- [x] Assessment (Part A)
- [x] Remove Zigbee (smart-plug automation, unrelated to the dashboard): code, profiles, CI, README
- [ ] Align CI with `sketch.yaml` (core 3.3.7, same library versions); enable Actions on the fork
- [ ] CI: compile on every push + firmware size check against the slot limit
- [ ] Button input fix (A.6): pull-down + fault limit; BOOT button as backup setup trigger
- [ ] License question to upstream (A.5)

**Done when:** a push builds in CI with a size report; the device boots into the dashboard with no sensor connected.

### Stage 1: Secure OTA (spec §9, Phase 6)
- [ ] OTA partition table (A.3); one-time USB migration, settings kept
- [ ] `FW_BUILD`, board id, display id, config-schema version compiled in
- [ ] Update client: daily check (+ "check now"), manifest download, **ECDSA signature check**, board/display/schema/min-version checks, no downgrades, SHA-256 check while writing, battery guard
- [ ] Rollback: new firmware is confirmed only after Wi-Fi + display refresh succeed
- [ ] Channels stable/beta (per device setting)
- [ ] CI: release job builds app binaries + manifest, signs with a GitHub secret, publishes only when triggered manually (pre-release = beta)
- [ ] Status bar shows the version; "Updating..." screen
- [ ] Host tests: manifest parsing, version and compatibility rules

**Done when:** a beta release installs itself on my device; a corrupted, unsigned or wrong-board image is rejected; a release that crashes on boot rolls back; settings survive.

### Stage 2: Firmware foundation (spec §4, Phase 2)
No visible changes, only structure:
- [ ] Split the `.ino` into `src/core` (App, Scheduler, ConfigManager, Logger), `src/hardware` (BoardConfig, PinMapping, BatteryMonitor, TouchController, EpaperDriver), `src/services` (Weather, Calendar, Time, Update), `src/web`
- [ ] ConfigManager: versioned schema, validation, migration, factory reset; secrets in NVS
- [ ] TLS: certificate bundle instead of `setInsecure()` (S1); Bambu LAN exception documented
- [ ] Logger with secret redaction (S6)
- [ ] Host test setup (g++ + doctest) in CI

**Done when:** behavior is the same as before (side-by-side check on the device), CI is green, every HTTPS request is verified.

### Stage 3: Widget system + layout engine (spec §5, §6, Phase 3)
- [ ] `WidgetDef` static registry; existing widgets moved over: clock (12/24 h, timezone, compact/large), weather (cached), calendar (agenda + month view, multiple feeds), status bar
- [ ] Optional technical widgets as modules: Bambu, stocks, tracking, flights, MakerWorld
- [ ] Grid layout engine + templates Classic / Planner / Minimal / Technical; validation, fallback to a known-good layout, disabled widgets leave no gaps
- [ ] Render planning: data update vs. layout change vs. partial vs. full refresh; periodic full refresh against ghosting
- [ ] Data cache per service with "stale" marker and backoff (spec §10.2)
- [ ] Host tests: layout fitting, minimum sizes, empty/max layouts, invalid configs

**Done when:** widgets and the layout are switched through configuration only; all four templates render correctly on the device.

### Stage 4: Personal widgets (spec §5.3)
- [ ] LittleFS user-data partition; storage for tasks / notes / birthdays with size limits
- [ ] Tasks (add, delete, done, due date, priority, show the top N)
- [ ] Notes (multi-line, length limit)
- [ ] Birthdays (from a chosen calendar feed or manual list; days remaining)

**Done when:** all three are editable (through a temporary JSON API at first) and survive restarts and OTA.

### Stage 5: Smartphone configuration v2 (spec §7, Phase 4)
- [ ] Secure provisioning: per-device AP password shown on the e-ink screen + Wi-Fi QR code (S2); admin PIN for settings (S4); secrets never echoed (S3)
- [ ] First-time setup flow: Wi-Fi scan, device name, profile, weather, calendars
- [ ] Pages: overview, widgets (enable, order, size, template), calendar (URL check with clear errors), weather, tasks & notes, device settings, firmware (version, channel, last update, last error)
- [ ] **Preview:** device-rendered 800x480 image of the configured layout
- [ ] Edit mode on the home network (long touch, or always on USB power), see A.8
- [ ] Error texts for non-technical users; diagnostics behind the PIN

**Done when:** a person without ESP32 knowledge sets up and personalizes a device using only a phone.

### Stage 6: Power management (spec §8, Phase 5)
- [ ] Power profiles Battery Saver / Balanced / USB Powered; USB detection
- [ ] Configurable refresh intervals per data type
- [ ] Low-battery threshold, reduced updates, critical-battery screen, then stay asleep
- [ ] Touch wake + gestures validated with the TTP223 on hardware
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

## Changelog

| Version | Stage | Change |
|---|---|---|
| .63 + fork | 0 | `sketch.yaml` profiles fixed; `tools/epd_test`; docs |
