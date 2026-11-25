# Design: the gift edition (stages 7-11, 2026-11-23 to about 2026-12-21)

Worked out with the owner on 2026-11-23. Stages and checkboxes: [IMPLEMENTATION_PLAN.md](../IMPLEMENTATION_PLAN.md);
device tests: [DEVICE_TESTS.md](../DEVICE_TESTS.md) blocks 9-13.

## Why

The dashboards are a **Christmas gift**: one device each for the owner's brother, sister and parents,
plus the owner's own. The owner builds them, flashes the stable build, prepares a starting dashboard
for each person, hands them over at Christmas (and helps set them up there). After that the family
changes whatever they like. Family devices run **stable** and only get versions the owner approved;
the owner's device runs **beta** and is the test device. Development continues after Christmas through
updates.

**Success:** stable, already useful, looks good, easy for non-technical people, gets better over time.
**Failure (anti-goals):** unstable, buggy, or limited. A feature that works but makes the device
less reliable is a failure.

## Goals of this month

1. The screen looks as good as the app's sketch (new fonts, cleaner month view).
2. **Several screens** per device, switched by time of day (opt-in) and by a tap.
3. **Night sleep screen** and a TRMNL-style **picture of the day**.
4. **Profiles:** ready-made and own (named), switchable in one tap.
5. **Export / import** of settings (backup, copying a set-up to another device).
6. **Gift mode** ("Neu einrichten"), a **help page** always reachable in the app, a printable quick-start card.
7. Extras: **public holidays and school holidays**, **birthdays from the calendar**, **waste collection**, **countdown**.

## Non-goals

- Remote viewing or managing devices from afar (decided: no, security).
- New hardware (stays: FireBeetle 2 ESP32-C6, 7.5" Waveshare, 3000 mAh LiPo, TTP223).
- Train / bus departures (decided: no).
- Storing pictures on the device (decided: keep the 128 KB storage free for features).
- Family photos online (the repo is public); own pictures only if they're fine to be public.

## Constraints

- **Storage:** the `userdata` partition is **128 KB** for everything (log 2 × 16 KB, battery history,
  tasks, note, birthdays, birthday feed). The partition table can only change over USB; it stays.
  New data on it must be small (profiles about 1-2 KB each).
- **Firmware:** 88 % of the 1.875 MB app slot (about 230 KB left) on the largest profile. New fonts,
  graphics and tables must fit, with room for later updates (aim: stay under 95 %).
- **Static RAM on the classic ESP32 profile** has overflowed three times: new big tables go to flash
  (`const`), lists to the heap.
- **RTC memory** (survives deep sleep, lost on reset): 8 KB on the classic ESP32, 16 KB on the C6.
  Already used for calendar, tracking, flight and states.
- **App bundle:** 48 KB gzipped of the 60 KB CI limit (`tools/web/build.py`). Screens, profiles, export
  and help will press on it; see "Storage and program space" below.
- **Battery:** about 20 days per charge at a 1-minute clock (measured roughly 2026-11-20). New features
  must not add Wi-Fi wakes beyond what the user turns on.
- Everything in **German and English** (`src/i18n/Strings.h`, `web/i18n.js`); German first.
- Plan first, then implementation; each stage ends with a build bump, CHANGELOG line, beta release.

## How the month runs (decided 2026-11-23)

- **Priorities:** the current build is already deliverable; everything here is nice-to-have and
  quality of life. Nothing must be finished by Christmas; what isn't ready comes later by update. If
  time runs short, the extras of stage 10 come last. (Builder's call: gift mode and help come in week 3
  so they're never at the end of the queue.)
- **No waiting for device tests.** The owner mostly has no device at hand while the code is written.
  Each stage starts when the previous one is implemented; findings from a device test are fixed in a
  later build. Several changes may go into one beta.
- **What the builder does each stage:** host tests, web tests, PC renders and all four profile builds
  as so far, plus new tests for the new code; and **[DEVICE_TESTS.md](../DEVICE_TESTS.md) grows with
  every change** (a living list), so the owner can test everything open in one session.
- **Device tests are what moves beta to stable.** Only device-tested builds go to stable; the family
  devices get only those. Feature freeze for the handover: about 2026-12-18.

## Approaches (decided in the conversation)

| Question | Considered | Picked, and why |
|---|---|---|
| Data for a screen change | (a) download everything on each change; (b) keep the last download in storage (LittleFS); (c) keep it in RTC memory | **(c)**: each download is kept until the next one of that data replaces it (owner: "drop it when the new interval / update occurred"). RTC costs no storage and no flash wear; after a reset everything is downloaded once anyway. (a) means 15-30 s and Wi-Fi per tap. Fallback for data too big for RTC on the classic ESP32: a small LittleFS file (builder measures) |
| Changing screens | time of day only; tap only; both | **Both**: time windows opt-in, tap always. The tap's refresh moves to the 2 s hold (a restart already redraws everything with fresh data; the owner rarely needs a manual refresh) |
| Pictures | (a) upload photos in the app and store them; (b) the device downloads any JPEG and converts it; (c) a GitHub Action prepares e-ink pictures, the device downloads one a day and draws it without storing | **(c)**: no storage used, pictures chosen to look good on e-ink (like TRMNL's). (a) uses 48 KB per picture of the 128 KB; (b) is heavy on the device and many photos look muddy |
| Picture source | public-domain art; own folder; both | **Both**: art by default, own folder possible (generic pictures only, public repo) |
| Night screen | tomorrow preview; photo; both | **Built-in sleep screen** (cute sleeping graphic, "Schläft bis 06:00", optional line for tomorrow) |
| Profiles | power presets; one-time starting points; switchable setups | **Switchable**, ready-made + own with a name; a profile holds look and behaviour, data stays device-wide |
| Holiday data | download from a service; calculate on the device; built-in table; a calendar link | Public holidays **calculated** (Easter-based rules, always right, nothing to download). School holidays **a calendar link** (owner, 2026-11-23): our own GitHub Action publishes one ICS per Bundesland, the link is filled in when the state is chosen and can be replaced (e.g. by a school's own calendar). Read by the existing calendar code, current without firmware updates, no table in flash. Not a direct outside service: the Action takes the dates from a public source (OpenHolidays API, ODbL 1.0; ferien-api.de was tried first and found incomplete: no Christmas holidays 2026/27, nothing for 2027) and keeps the last good file if that source fails |
| Gift mode | separate feature; part of the reset | **Part of the reset** ("Neu einrichten": forget the WLAN, keep everything else). Harmless if the family uses it; useful for a new router or a move |
| Export with secrets | optional tick box; never | **Never** (confirmed by the owner, 2026-11-23: no sensitive information in exports): since build 71 secrets never leave the device (finding S3 in the plan). WLAN and calendar links differ per person anyway; an import keeps the target's own secrets |

## Stage 7: new look (build 79-80, week 1, until 2026-11-30)

Everything of [LOOK_AND_LANGUAGE.md](LOOK_AND_LANGUAGE.md) part 2 (Montserrat Bold clock in all
layouts incl. Classic, date under the clock, headings with a line, weather labels, the HighSpeed font
removed, the app's sketch follows the device), plus:

### Month view rework

Cleaner and more minimal than today's filled circles. Each mark has its own fixed place in a day cell,
so nothing overlaps:

```
 ┌──────── cell ─────────┐
 │       ╭──────╮        │  ① number: today = small filled rounded square, white digits;
 │       │  13  │        │     public holiday = the same shape as a 1-px frame; today wins if both
 │       ╰──────╯        │
 │        •   ◊          │  ② mark row: • appointment (4-px dot), ◊ birthday (9-px cake); both: side by side
 │━━━━━━━━━━━━━━━━━━━━━━━│  ③ school holidays: 2-px line on the bottom edge, joins neighbouring days
 └───────────────────────┘
```

- Weekend (Sa, So) numbers in **bold** (a bold 16 px digit set, 0-9 only).
- **KW** (week numbers, ISO 8601) column, a **footer** with the next highlights (`★` holiday, `━` start
  or end of school holidays, `◊` birthday with age if known).
- **Appointments** only from calendar links with the new switch **"Im Monat markieren"** (off by default,
  per link). Birthdays always (see below).
- If a cell is too low for the mark row (under about 34 px), the mark row is dropped; square and line stay.

**Sizes** (grid cell 200 × 150 px below the 28 px status bar; estimates with a 26 px title, 20 px
weekday row, 20 px footer lines):

| Box | Size (px) | Design | Estimated cell |
|---|---|---|---|
| 4 × 3 (full screen) | 800 × 452 | grid left (KW + 7 days), **side column "Demnächst"** right (about 240 px: up to 8 highlights incl. marked appointments of the next 7 days), 22 px digits | 70 × 66 |
| 4 × 2, 3 × 2, 3 × 3 | 800/600 × 300/452 | grid + side column (5-8 lines) | 55-65 × 42-66 |
| 2 × 3 (half screen, upright) | 400 × 452 | full cell design, KW, footer 3 lines below | 50 × 56 |
| 2 × 2 | 400 × 300 | full cell design, KW, footer 1 line | 50 × 37 |
| 1 × 2, 1 × 3 (narrow) | 200 × 300/452 | no KW, no footer, weekdays as one letter, marks kept | 28 × 42-66 |
| 2 × 1, 3 × 1, 4 × 1 (low) | 400-800 × 150 | **two weeks** (this and next) with KW, marks and line; footer 1 line (2 × 1) or side column (3 × 1, 4 × 1) | 50 × 40 |
| 1 × 1 | 200 × 150 | **calendar sheet**: weekday, big date (clock digits), month + KW, one highlight line | - |

The builder may adjust these after the PC renders; the owner reviews the renders before the device test.

### Public holidays and school holidays

- Settings: **Bundesland** (dropdown, default "Aus") with two switches **Feiertage** and **Schulferien**
  (opt-in each).
- Public holidays computed on the device for the chosen state (national ones plus state ones such as
  Heilige Drei Könige, Fronleichnam, Reformationstag, Allerheiligen, Buß- und Bettag, Frauentag,
  Weltkindertag). Local ones (Augsburger Friedensfest) are left out.
- School holidays: a **calendar link "Schulferien"**, filled in with our own ICS for the chosen state
  (published by a GitHub Action in the repo, like the picture of the day, from a public source; the
  last good file stays if the source fails; never the GitHub "latest" release). Replaceable by any
  other link. Downloaded once a week; multi-day entries of this link become the line in the month view.
- Shown: holiday name after the date in the **status bar** (`Mi, 6. Jan · Heilige Drei Könige`);
  **"Heute und morgen"** lists the holiday first under its day (`★ Heilige Drei Könige`), also for
  tomorrow; frame and line in the **month view**; the footer / side column.

### Birthdays from the calendar

- Yearly all-day entries in the normal calendar links count as birthdays (how Google stores birthdays
  created as events). Setting **"Jährliche ganztägige Termine als Geburtstage"**, on by default.
- They show as `◊` in the month view and in the footer and birthday box; not also as an appointment dot.
- Age only if known: from the device's own birthday list, or a year in the title ("Anna (1966)").
- The existing birthday link (stage 4) stays. Google's automatic contacts birthday calendar has no
  link as far as known: the builder checks; if not, that's a setup step (owner: acceptable).

**Also in week 1:** promote a tested beta to stable (the family devices are flashed from stable).

## Stage 8: data store and several screens (build 81-82, week 2, until 2026-12-07)

### Data store

- Every service keeps its last good result in RTC memory (weather, stocks, MakerWorld, printer state;
  calendar, tracking and flight already do). A screen draws from the store; data is downloaded only
  when its interval is due (or the store is empty after a reset).
- **Refresh interval per data type** (open since stage 6): weather, calendar, stocks, parcels each get
  an interval setting with sensible defaults (calendar 30 min, weather 60 min, stocks 30 min,
  parcels 2 h; builder may adjust), school holidays weekly. **The clock interval is the upper bound
  for everything** (as today): with a 30-minute clock interval nothing refreshes more often than every
  30 minutes, whatever a data type's own interval says; intervals are rounded up to a multiple of it.
- Old-data badges keep working from the store's timestamps.
- **Per calendar link** (owner, 2026-11-25): a **name** ("Familie", "Arbeit") shown in the app instead
  of "Calendar link 1", in the Check result and the log, and in an export (the link itself never);
  and its **own interval** ("every hour" / "every 3 hours" / "mornings only"), so a large work calendar
  isn't read at every refresh. Needs the store: a link not read this time keeps its last entries,
  marks and birthdays (today all links are combined into one result at each refresh).

**Order in stage 8** (owner, 2026-11-25): data store → per calendar link (name, interval) → printer
box and look (brought forward from stage 10; mockups first) → several screens → night sleep screen →
picture of the day. A written plan (`plans/STAGE8_*.md`) with mockups comes before the build.

### Look of the widgets (owner, 2026-11-25: "love the functionality, hate the looks")

- The **printer box** gets designs per box size like the clock and the month (stage 7): today it
  draws one fixed small layout (3 rows at fixed x positions) even in a half-screen box. Bigger boxes:
  larger state and progress, the AMS spools as a row of filled bars with type and fill level,
  temperatures with icons; the idle view (no MakerWorld id) uses the same space.
- **Every AMS** (owner: two are connected, only the first is shown today) and **more per spool**:
  brand and type ("PLA Basic"), fill level, colour (as a name or pattern: black-and-white screen),
  from the printer's report (`tray_sub_brands`, `tray_type`, `tray_color`, `remain`).
- A look pass over the other data widgets (stocks, parcels, weather, MakerWorld) with the same rule:
  use the box, no fixed pixel columns, the hinted fonts (build 84). Mockups (PC renders) first, for
  the owner to pick, then the build.

### Printer box when idle (owner, 2026-11-25)

- Without a MakerWorld id the idle printer already shows its own view (temperatures, AMS humidity and
  temperature, spools; build 84). Wanted on top: the printer's **messages** (HMS warnings such as
  "AMS humidity high", a filament runout, a failed print) in plain words, and the idle view also when a
  MakerWorld id is set (a setting: "Wenn der Drucker ruht: MakerWorld / Drucker"). The builder checks
  which HMS codes the report carries and keeps a short table of the common ones (code + "see the
  printer" for the rest), within the space limits below.

### Several screens

- **Opt-in.** One screen (default, and all existing devices) behaves exactly like today.
- **Up to 4 screens.** Each is a normal layout (ready-made or own boxes from the editor) or a special
  screen: **picture of the day** or **sleep screen**.
- **Time of day (opt-in):** each screen can get a window ("06:00-09:00"); windows may not overlap
  (the app checks). Outside all windows the first screen is shown. The switch happens on the regular
  clock wake (no extra wake), with a full refresh of the new screen.
- **Tap with 2 or more screens:** next screen, shown for **2 / 5 / 10 minutes** (setting, default 5),
  then back to the scheduled screen (one extra wake). Each further tap moves on.
- **Gestures:** tap = next screen (with one screen: refresh, as today); double tap = update screen;
  hold 2 s = restart, which redraws everything with fresh data ("Neu laden"); hold 4 s = setup.
  The app and the help say so.
- **Status bar:** small dots for the screens (`● ○ ○`), right of the middle next to the battery, so a
  holiday name after the date fits.
- Presets in the app (each a set of screens with windows), e.g. "Morgens Kalender + Wetter, tagsüber
  alles, nachts Schlafbildschirm".

### Night sleep screen

- Opt-in setting for the quiet hours (independent of several screens): **"Nachts: Schlafbildschirm"**.
  Off for existing devices; on in the new ready-made profiles.
- Drawn once at the start of the quiet hours, then sleep until the end (no extra battery).
- A sleeping graphic in the firmware (moon, "z z Z", a sleeping face; about 200 × 150 px, 1-bit,
  original artwork), **"Schläft bis 06:00"**, an optional line for tomorrow (weather, first
  appointment, holiday).
- A tap at night shows the day screen for the tap time, then the sleep screen again.

### Picture of the day

- **GitHub Action** in the repo (daily, early morning UTC): picks a public-domain work (Art Institute
  of Chicago open access, CC0; works that convert well: prints, drawings, woodcuts, etchings), or a
  picture from the own folder (`pictures/own/`, mixed in or instead, setting in a config file),
  scales and dithers it to 800 × 452 1-bit, publishes it with title and artist (a small JSON) at a
  fixed URL. Hosting: the same way OTA files are hosted (release assets; already proven on the device)
  unless the builder finds something simpler. The picture release must **never** become GitHub's
  "latest" release (stable devices read `releases/latest` for updates): a pre-release with a fixed tag.
- The device downloads it once a day (when the picture screen is first due that day), straight into
  the frame buffer, draws title and artist itself (its fonts, umlauts) and the status bar; nothing
  stored. A failed download keeps the last picture on the screen and retries later.
- 48 KB once a day: a few seconds of Wi-Fi.

## Stage 9: profiles, gift mode, help (build 83-84, week 3, until 2026-12-14)

### Profiles

- **In a profile (the look):** screens with their layouts and windows, which boxes are on, picture of
  the day on / off, sleep screen on / off, tap time.
- **Device-wide (not in a profile):** WLAN, name, language, time zone, calendar links, location,
  stocks, birthdays, tasks, note, holidays settings, countdowns, waste link, and **power**: clock
  interval, quiet hours, refresh intervals per data type (corrected by the owner, 2026-11-23: power
  sits on top of any profile; the clock interval caps every refresh).
- The stage 6 "power profiles" (Akku sparen / Ausgewogen / USB) aren't built: the clock interval with
  its battery estimate and the quiet hours already are that setting, and the automatic battery saver
  covers low battery.
- **Ready-made profiles** in the firmware, grown from today's layouts: Familienplaner, Persönlicher
  Planer, Schreibtischuhr, Technik, Klassisch (the stage 7 "example profiles").
- **Own profiles:** up to 5, with a name (up to 24 characters), stored in `/profiles.json`.
- App, Start tab: list of profiles, one tap switches (the screen redraws right away). The active
  profile is shown; after changes "(geändert)". Buttons: save (own profiles), "Als neues Profil
  speichern", rename, delete.

### Gift mode ("Neu einrichten")

- App, Gerät tab, reset section: **"Neu einrichten …"** (forget the WLAN, keep everything else) next to
  the existing factory reset. Asks for confirmation and a greeting (optional; empty = "Willkommen!").
- The device draws the **gift screen** (greeting, a decorative frame / bow graphic, small "Antippen zum
  Starten"; no QR code) and sleeps without a timer (only the button wakes it). The e-paper keeps the
  picture without power; the owner disconnects the battery for transport.
- **At their home:** a tap, or connecting the battery / power, starts the device: the **welcome screen**
  with the hotspot QR code and three steps. The app opens on the WLAN step only, then done. After the
  hotspot timeout the welcome screen stays, a tap starts it again.

### Help

- A **"?" in the app header on every tab** opens the built-in help: gestures, what the status bar
  marks mean, "Was mache ich, wenn …" (no WLAN, screen doesn't change, battery low, wrong time, no
  updates, the screen shows an error). German and English, short (it's part of the app on the device).
- The **full guide** (every box, settings, updates, recovery) is a page in the repo, German and
  English, linked from the help and as a QR code on the quick-start card. Always reachable, costs no
  space on the device.

### Quick-start card

- App → Gerät → **"Kurzanleitung drucken"**: a printable page (A6 / postcard size) with the device's
  name, its own hotspot QR code (the device makes it as SVG with its QR code generator, so no QR
  library in the app), the gestures and a line on where the help is.

## Stage 10: export / import, extras, polish (build 85-86, week 4, until 2026-12-18)

### Export / import

- **Export:** a JSON file from the app (`dashboard-<name>-<date>.json`, format with a version number):
  all profiles, the active one, device-wide settings, tasks, note, birthdays, countdowns. **No secrets**
  (WLAN password, calendar and birthday links, API keys, MQTT password).
- **Import:** pick the file in the app, see what it contains, choose parts (profiles / settings /
  tasks, note, birthdays), apply. The target keeps its own secrets and WLAN.
- Use: backup, and set up one device, then copy to the others and change names and links.

### Waste collection box

- A calendar link for the town's waste calendar (field "Abfuhrkalender"). The box shows the next
  pickups with an icon per kind, recognised from the title (Restmüll, Bio, Papier, Gelbe Tonne /
  Wertstoff, Glas; other titles with a plain bin).
- From 18:00 the day before: a note in the status bar ("Morgen: Gelbe Tonne").
- The owner checks whether the family's towns offer such a link.

### Countdown box

- Up to 3 entries (name + date) set in the app: "Noch 12 Tage bis Urlaub", on the day "Heute: Urlaub!",
  gone the day after.

### Experiment: setup app reachable on a charger (owner's device first)

- The board can't see a USB charger directly (only a computer's USB). It infers it from the battery
  voltage: on a charger it rises, or stays at about 4.2 V with Wi-Fi on; on battery it drops within
  minutes under Wi-Fi load.
- While on a charger the device stays in edit mode on the home network (no 4 s hold needed) and goes
  back to normal sleep as soon as the voltage drops. A wrong guess costs a few minutes of battery
  (owner: acceptable).
- On beta first; to stable when it works reliably on the owner's device, otherwise it moves to Later.

### Polish before the handover

- The open robustness points (stage 6 and earlier), the stage-6 cost model from the block 8
  measurements, the full guide in the repo (German / English, incl. recovery), the old `/basic` page removed.

## Stage 11: release and handover (until about 2026-12-21)

- Feature freeze; release validation (DEVICE_TESTS block 13), a release candidate to stable, flash and
  prepare the four devices, "Neu einrichten" with greetings, print the cards.

## Storage and program space (owner, 2026-11-23: don't waste space on things not used regularly)

The device has two kinds of space: the **program** (firmware + app, 1.875 MB slot, about 230 KB free)
and the **data storage** (128 KB). Rules for every stage:

- **Nothing stored that the dashboard doesn't need.** Pictures are drawn and not kept; downloaded data
  lives in sleep memory, not in the 128 KB; profiles and settings in compact form.
- **Rarely used things live off the device where they work just as well:** the full help and user guide
  are pages in the repo (German / English), linked from the app and as a QR code on the quick-start
  card; the app keeps a short built-in help (gestures, the most common problems) for the hotspot, where
  the phone may have no internet. School holidays are a calendar link instead of a table.
- **Graphics small:** sleeping graphic, gift frame, cake and bin icons 1-bit and compressed; fonts only
  with the characters used.
- **Remove what's obsolete:** the old setup page `/basic` goes once the app is confirmed on phones
  (stage 10); the HighSpeed clock font goes in stage 7.
- **The app bundle:** the 60 KB limit stays the target. Screens, profiles and export are written
  compactly; if it still doesn't fit, rarely used parts (export / import, card) load on demand as a
  second file instead of raising the limit.
- Each stage's result notes program size, bundle size and storage use, so growth is visible.

## Left to the builder

- Exact pixel sizes, icons and artwork (original, e-ink friendly), after PC renders the owner reviews.
- RTC layout of the data store and the LittleFS fallback.
- The picture Action's selection rules, dithering method and hosting details.
- Default per-data intervals, file formats (profiles, export), API endpoints (documented in
  `docs/SETUP_API.md`).
- Which earlier test items each stage's device test repeats.
