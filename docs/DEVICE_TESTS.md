# Device test session: everything still open

Everything implemented so far that has only been checked on the PC (host tests, PC renderer, CI)
and still needs the real device: stages 0-6, builds 63-75. Made to be done in **one session**, in
this order (each block builds on the one before). Tick the boxes, note what the screen showed, and
copy the results into the hardware test log in [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md#hardware-test-log).

**You need:** the FireBeetle C6 with the 7.5" display, the TTP223 touch sensor (wired, but unplug it
for block 1), a USB cable and the PC with `arduino-cli`, a phone (iPhone and/or Android, ideally both),
a laptop for the GitHub workflows, about 2-3 hours (block 4 waits for 03:00 or is skipped, see there).

## 0. Bring the device to the current build (USB, once)

Whatever it runs now, flash it once over USB: on test build .90005 no release reaches it (see
[OTA_TESTS.md](OTA_TESTS.md#stuck-on-a-test-build)), and builds before 71 refuse updates as "battery
too low" while the battery reading is wrong (block 0b). Battery **unplugged** until block 0b says it's
safe.

- [x] `arduino-cli compile --profile esp32c6_firebeetle -u -p <port> .` with the current `main` (build 73);
      settings and Wi-Fi are kept
- [x] Serial monitor (115200): boot log shows `Firmware .73 (build 73, esp32c6_firebeetle, bw750, settings schema 3)`
- [x] Settings schema: log shows `Settings schema 2 -> 3` once (not again after the next restart);
      coming from build 66 or older it starts at 1 or 2, also fine
- [x] The dashboard appears as before (same layout as you had)

## 0b. Battery and power (reported 2026-11-19: "battery low" with USB attached, not charging)

Result (2026-11-19): with the battery the reading was a steady 1.16-1.17 V (0.06-0.11 V without it):
the inline adapter connected the battery **reversed**. After flipping its wires: 4.22 V on the
charger, 3.91 V (67 %) shortly after: the battery works.

- [x] The battery's **red** wire ends on the pin marked **+** next to the board's battery connector
      (the adapter's wires had to be swapped)
- [x] The battery is alive: 4.22 V on the charger, 3.91 V (67 %)
- [x] Charging and battery reading work (2026-11-19/20: 4.22 V on the charger; after unplugging 4.00 V
      dropping to 3.92 V within an hour (surface charge), then 71 % → 68 % in 13 hours)
- [ ] First full charge: the cell gets at most slightly warm

Note what the top bar shows (voltage or `USB`) in each case:

- [x] USB from a **computer**, battery connected: voltage ____ ; double tap shows "Battery: … - USB connected"
- [x] USB from a **charger / power bank**, battery connected: voltage ____ ; it should rise over an hour
      while charging (the board's charge LED is on while charging)
- [x] **Without battery**, on USB: the top bar shows `USB` (build 71), updates are allowed
- [x] Battery only (no USB): voltage ____ ; compare with a multimeter at the battery connector:
      ____ V (if they differ, `BAT_CALIBRATION`, user guide "battery level")
- [x] Charging: with USB connected for an hour the voltage goes up; if it doesn't, check the battery's
      plug polarity (+/- at the board's battery connector; JST plugs of different vendors are wired
      reversed) and that it's a single-cell 3.7 V LiPo
- [x] "Check for updates now" on a charger with a low battery: still "battery too low" (expected: the
      device can't see a charger); with a computer's USB it installs

## 1. Button and boot (stage 0): done

Confirmed on the device on 2026-11-19 (reported by user, with a build before 71). The 5 s hold
opening **edit mode** (new in build 71) is checked in block 6.

- [x] Boot with **no touch sensor connected**: dashboard appears, no reboots, no setup mode
- [x] Connect the TTP223. **Tap**: the dashboard refreshes (full refresh)
- [x] **Double tap**: info and update screen ("Up to date: .73" or a newer version)
- [x] **Hold 2 s**: restart
- [x] **Hold 5 s** (release after 4 s or more): setup / edit mode (see block 6)
- [x] **Sensor held HIGH 15 s or more** (keep a finger on it): no gesture, log `[Button] held past BUTTON_FAULT_MS`;
      after letting go the button works again; the board keeps sleeping (doesn't wake every second)
- [x] **BOOT backup:** tap RST, then hold BOOT for 1 s: setup hotspot opens even without the sensor

## 2. Stage 2: no visible change, every service still loads

- [x] Side by side with what you remember from build 66: same screen, nothing missing
- [x] Each service once (status bar shows no problem after a few minutes): weather, stocks, calendar
      (all links), parcels, flight (if one is in a calendar), printer (if on), MakerWorld
- [x] Serial log: no secret (Wi-Fi password, keys, calendar links) printed in clear text (masked as `***`)
- [x] Old setup page `http://192.168.4.1/basic` (in setup mode): saving a wrong value (e.g. stock symbol
      `AAPL!`) shows what to fix and saves nothing; a `webcal://` link is accepted and turned into `https://`
- [x] Factory reset from the serial console: type `factory-reset` during the BOOT window after a reset
      (only if you're willing to type all settings again; the app's factory reset is in block 7)

## 3. Stage 3: layouts and data age

- [x] In the app (block 6) or the old page: switch through **Classic, Planner, Minimal, Technical,
      Personal**; each is drawn with a full refresh, no leftovers of the previous one
- [ ] Planner without a weather place: the clock fills the top row (gap filling)
- [ ] 12-hour clock: "9:41" with AM/PM; at 12:xx without AM/PM where it doesn't fit
- [x] Partial refreshes over a few minutes: clock changes every minute without ghosting
- [ ] "old" badge: set a wrong weather place, wait 30+ minutes: weather keeps its last data and shows
      "old"; the status bar names "Weather". Retries: log shows retries after 2, 4, 8, 16 minutes, then hourly
- [ ] Fix the place again: the badge disappears at the next fetch

## 4. Stage 1 leftovers: OTA

Set channel **Beta**, auto install **off** first (setup app, Device tab).

- [ ] **Release build 75 to beta**: GitHub → Actions → Build firmware → Run workflow: release ✔,
      channel beta, "What's new" text. Then double tap: "Up to date: .75" (the device already has it)
- [ ] **Hang test with the fix (build 69+):** OTA test release → `hang`; double tap, confirm tap:
      "Updated to .90005", then hangs. **Hold the button 2 s** during the hang: it must restart and come
      back as .75 with "Update failed: previous version kept" (before build 69 a hold kept the test build)
- [ ] OTA test release → `restore`; double tap: "Up to date: .75"
- [ ] **Notify only:** auto install off, a newer beta release (bump `FW_BUILD`, release to beta, or use
      the next real release): after the night check (03:00-03:59) the status bar shows the "Update
      available" mark (with night quiet hours over 03:00: after their end, block 6b)
- [ ] **Night install:** auto install on, a newer beta release: installs by itself between 03:00 and
      03:59; next morning "Updated to …" was shown and the version is new
- [ ] **Battery guard:** on battery below 30 % (no USB), "Check for updates now": "Battery too low for
      an update: please charge" and nothing is installed (skip if no battery is connected yet)
- [ ] Set the channel back to Stable and auto install as you want it

The night tests need the device on overnight; if you want everything in one sitting, do them the night
before or after and note them separately.

## 5. Stage 4: tasks, note, birthdays

- [x] In the app (Data tab): add 5 tasks (one overdue, one today, one high priority, one without date,
      one done), a note with umlauts and line breaks ("Müll: Donnerstag!"), 3 birthdays (one today with
      year, one in 3 days, one in February)
- [ ] Layout **Personal**: tasks in the right order (overdue first, date in a black box; high priority
      = filled box; the done one not shown), the note wrapped, birthdays with "Today" in a black box and
      the age
- [x] Umlauts, ß, €, `! ( ) & ' "` look right; an emoji typed into a task is removed when saving (the app says so)
- [x] 31st task refused with a message; a note over 400 characters refused
- [ ] **Birthday calendar link** (e.g. Google's birthday calendar as iCal): "Check" finds events; after
      the next wake its birthdays appear (marked as from the calendar in the app)
- [ ] **Survives a restart** (hold 2 s): everything still there
- [ ] **Survives an update:** release a build to beta and install it (block 4): everything still there
- [ ] Next day (or change the device clock by waiting past midnight): "in 3 d" became "in 2 d", "today"
      labels moved (redrawn right after midnight)

## 6. Stage 5: the setup app

### Hotspot (setup mode)

- [x] Start setup mode with BOOT after a reset (or the 5 s hold without working Wi-Fi). The screen shows
      **"Setup mode"**, the network name, a **password** and a **QR code**
- [x] **Scan the QR code** with the phone camera: joins the hotspot without typing (iPhone and Android)
- [x] The **app opens by itself** (captive portal). If not: `http://192.168.4.1`
- [x] The Paper look, four tabs (Home, Screen, Data, Device); everything readable in the captive-portal
      window (iPhone: the small "mini browser" without address bar)
- [x] Home: battery, Wi-Fi, version, problems in plain words with the fix
- [x] Device: Wi-Fi list (scan), password "Test" (wrong password → "Wrong password", right one → OK)
- [x] Data: calendar link "Check" (right link → number of events; wrong link → reason); weather place
      search shows suggestions (needs the tested Wi-Fi)
- [x] Save a setting: "Saved, restarting"; the device restarts and uses it
- [x] The old page `http://192.168.4.1/basic` still works (fallback)
- [x] Without use it leaves setup mode and shows the dashboard (hotspot: after 5 minutes, edit mode: after 10)

### Layout editor

- [x] Screen tab: pick a structure, tap a box, choose a widget; widgets that don't fit are greyed out
      with the reason ("needs 2 × 2")
- [x] **Split** a 2 × 2 box: two 2 × 1 boxes; **merge** two neighbors back
- [x] **Your example:** "Top + main", agenda in the top row, no clock anywhere → "Show on the display":
      the screen shows it **without a restart**; the sketch in the app matched the screen
- [x] **Status bar off**: the boxes use the full height; with a problem (e.g. wrong weather place) a
      small mark appears in the bottom-right corner
- [ ] A widget without settings in a box (e.g. weather without a place): the box shows a hint
      ("Weather: set a place in the app") instead of being empty
- [x] **Exact preview** button: the device draws the edited layout; same as the screen afterwards
- [x] After a restart the custom layout is still there; no clock on screen → the time still right in the
      agenda (time zone applied without the clock)

### First start (on a reset device, see block 7)

- [ ] After a factory reset: setup screen with password + QR; the app shows the 5-step guide instead of
      the tabs: Wi-Fi (scan, test), name + time zone (pre-filled from the phone) + place, calendars,
      screen, done
- [ ] "Done": one restart, the dashboard appears with what was chosen

### Edit mode on the home network

- [ ] With working Wi-Fi, **hold 5 s**: the screen shows **"Edit mode"**, the address
      (`http://<name>.local` and the IP), a QR code and a **6-digit code**
- [ ] Phone in the home Wi-Fi: scan the code or open the address → the app asks for the code; the right
      code opens it; 5 wrong codes → a new code appears on the screen
- [ ] `http://<name>.local` works (iPhone; on Android the IP may be needed)
- [ ] A second phone without the code can't change anything (asks for the code)
- [ ] Device tab: **Change Wi-Fi** → the device restarts into the hotspot
- [ ] 10 minutes without use: back to the dashboard

### Diagnostics (build 72)

Seen on the device on 2026-11-19: the diagnostics view works, the firmware line is in the log.

- [ ] After a few hours on the wall: Device → Diagnostics shows log lines of the Wi-Fi wakes (with
      times and a date line) and one battery value per hour
- [ ] A broken calendar link (or Wi-Fi off for a while): it shows in the service table (failures,
      next retry) and in the log
- [ ] **Download log** opens / saves a text file (iPhone captive browser and a normal browser)
- [ ] **Clear log**: the list is empty; new lines appear after the next Wi-Fi wake
- [ ] Battery chart: the case of 2026-11-19 (1.1-1.4 V with the battery plugged in) is visible as
      "no battery measured"; after the battery fix the curve rises while on USB
- [ ] The USB serial monitor still shows every line live, secrets masked

### Fixes of build 73 (found in the device test, 2026-11-19)

- [ ] Hotspot: save a setting → "Saved, restarting" → the device comes back in setup mode (hotspot
      again, same password), the phone reconnects, the app reloads by itself; **Done** shows the dashboard
- [ ] Edit mode: save a setting → back in edit mode with the **same code**, the app goes on without
      asking for the code again
- [ ] First start: the guide's last step ends with the dashboard (no waiting)
- [ ] Printer switched off (or MakerWorld ID only, no printer address): the log shows `[Bambu] next try
      in 10 min` (then 20, 40, 60), not a printer connection every 10 minutes; a wake takes a few seconds,
      not tens of seconds
- [ ] MakerWorld ID without printer address: the printer widget shows MakerWorld stats, no MQTT lines in the log
- [x] Log after a reset: `[User] storage mounted: … KB used` (2026-11-20: 20-32 KB used, no errors). The "Corrupted dir pair … mount failed"
      lines may appear **once** (first start of storage); if they come on **every** boot, report it
- [x] USB monitor after a timer wake: the last line `Finished wake …: firmware .73 (build 73, …)` is visible (in the stored log, 2026-11-20)
- [x] Diagnostics log: times in local time (2026-11-20; the first lines of each wake were still UTC:
      fixed in build 74)
- [ ] A printer that doesn't answer costs about 5 s per wake, not 30-60 s (printer settings were removed
      for now: redo when the printer is set up again)

## 6b. Stage 6 part 1: power (build 75)

- [ ] Clock refresh "every 5 minutes": the display updates at :00, :05, :10 ...; the status bar says
      "Clock every 5 min"; the log shows wakes 5 minutes apart; calendar still once an hour
- [ ] The app shows days per charge for each choice; the numbers change with the quiet hours on / off
- [ ] Quiet hours set to start in 2 minutes and end 10 minutes later: at the start the status bar says
      "Night until …", no wakes in the log during the window, a full refresh with fresh data at its end
- [ ] A tap during the quiet hours shows the current dashboard (with the note), then it sleeps again
- [ ] Quiet hours off: normal updates through the night
- [ ] Night update check with quiet hours over 03:00: the log shows `[OTA] beta manifest` at the end of
      the quiet hours (06:00), not at 03:00
- [ ] Battery saver: below 20 % on battery (no computer USB) the status bar says "Battery saver: every
      15 min" and the log shows `[Power] battery saver on`; above 30 % (or with a computer's USB) off again
- [ ] Below 5 %: "Battery empty: please charge"; the log shows one short wake per hour; a tap shows the
      same screen; after charging above 10 % the dashboard comes back by itself (full refresh)

## 7. Factory reset (last, it erases everything)

- [ ] App → Device → Factory reset (confirm): restarts into setup mode with a **new** hotspot password;
      tasks, note and birthdays are gone; first-start guide appears (block 6)

## After the session

- [ ] Results into the hardware test log; tick stage 1-5 items in IMPLEMENTATION_PLAN.md
- [ ] Problems found: one line each (what you did, what the screen showed, the serial log if you have it)
