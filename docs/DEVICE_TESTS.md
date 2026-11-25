# Device tests

Where a stage of [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md) is confirmed: the plan says what is
implemented, this file whether it works on the device. Each block starts with **Confirms**: the
stage's gate; the stage is confirmed when all boxes of its block are ticked. Blocks 0-7 are made to
be done in one session, in this order (each builds on the one before); blocks 8 and 13 are separate
sessions. Block M lists the manual tasks outside the device (GitHub Actions, releases, accounts). Blocks 9-12 (the gift edition, stages 7-10) grow as each part is built: a part marked
*(not built yet)* has nothing to test yet. Tick the boxes, note what the screen showed, and add a line to the
[results log](#results-log) at the end.

**You need:** the FireBeetle C6 with the 7.5" display, the TTP223 touch sensor (wired, but unplug it
for block 1), a USB cable and the PC with `arduino-cli`, a phone (iPhone and/or Android, ideally both),
a laptop for the GitHub workflows, about 2-3 hours (block 4 waits for 03:00 or is skipped, see there).

## M. Manual tasks (GitHub, accounts, releases)

Everything that isn't done on the device itself but by you, in one place. Tick per build where it
says so; the rest once.

**After every push to main (each new build):**
- [x] GitHub → Actions → **Build firmware**: the run of the push is green (host tests, web tests, feed
      script tests, all four profiles)
- [x] Releases: **firmware-.NN** exists as a pre-release with the "What's new" lines, and the fixed
      **beta** pre-release was refreshed (beta devices update at night or with a double tap)

**Data feeds (school holidays, build 79 on):**
- [x] Once after the first push of build 79: Actions → **Data feeds** → Run workflow (branch main)
- [x] Releases: the pre-release **feeds** has 16 files `ferien-BW.ics` … `ferien-TH.ics`, and it is
      **not** marked "Latest" (the stable firmware release stays "Latest")
- [ ] A week later: the Monday run happened by itself (Actions → Data feeds, scheduled), green
- [ ] If a run fails: the old files stay online (devices keep their holidays); look at the log of the
      failed state, run again by hand later

**Promote a tested beta to stable (the family devices get only these):**
- [x] Only a build whose device-test block is complete. Its `FW_BUILD` must still be the one on main:
      promote **before** pushing the next build
- [x] Actions → **Build firmware** → Run workflow: branch **main**, "Publish" **on**, channel
      **stable**, "What's new" empty (the beta's lines stay), oldest build **0**, "Allow a changed
      signing public key" **off**
- [x] Releases: **firmware-.NN** is now "Latest" (not a pre-release); a stable device updates at night
      (or double tap → update screen shows it)

**Accounts and calendars (once):**
- [x] Google Calendar → Settings → "Geburtstage" (contacts' birthdays) → "Kalender integrieren": is there
      a "Privatadresse im iCal-Format"? Yes: it can go into the birthday link. No: birthdays as yearly
      events in a normal calendar (found automatically, build 79) or typed into the app. Note the answer
      in the results log
- [ ] Waste collection: does each family member's town offer a calendar link (ICS) for the pickups?
      (needed for stage 10's waste box) Note per town
- [x] Signing key: a backup of `~/ota-signing-key/` exists in a safe place (not in the repository);
      copies on the Windows desktop are deleted

**Own repository (stage 7b, before the handover):**
- [x] Before detaching: Releases (stable "Latest", "beta", "feeds") and Settings → Secrets → Actions
      `OTA_SIGNING_KEY` are there (screenshot or note)
- [x] GitHub → this repository → Settings → General → Danger Zone → **Leave fork network** (permanent;
      the name and address stay, so the devices' updates keep working)
- [x] After: the repository no longer says "forked from VoIPshare/…"; the releases and the secret are
      still there; Actions → Build firmware runs green on the next push; a device's update check still
      works (double tap → update screen)
- [x] The repository stays **public**: devices download updates and the school holidays without
      logging in (a private repository would stop both)
- [x] Licence for your own code chosen (see stage 7b) and the `LICENSE` file is on main

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

**Confirms stage 0:** a push builds in CI with a size report (done, 2026-11-16); the device boots
into the dashboard with no sensor connected.

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

**Confirms stage 2:** behavior as before, every HTTPS request verified (CI green is checked by CI).

- [x] Side by side with what you remember from build 66: same screen, nothing missing
- [x] Each service once (status bar shows no problem after a few minutes): weather, stocks, calendar
      (all links), parcels, flight (if one is in a calendar), printer (if on), MakerWorld
- [x] Serial log: no secret (Wi-Fi password, keys, calendar links) printed in clear text (masked as `***`)
- [x] *(page removed in stage 7b)* Old setup page `http://192.168.4.1/basic` (in setup mode): saving a wrong value (e.g. stock symbol
      `AAPL!`) shows what to fix and saves nothing; a `webcal://` link is accepted and turned into `https://`
- [x] Factory reset from the serial console: type `factory-reset` during the BOOT window after a reset
      (only if you're willing to type all settings again; the app's factory reset is in block 7)

## 3. Stage 3: layouts and data age

**Confirms stage 3:** widgets and layout switched through configuration only; every template renders
correctly on the device.

- [x] In the app (block 6) or the old page: switch through **Classic, Planner, Minimal, Technical,
      Personal**; each is drawn with a full refresh, no leftovers of the previous one
- [ ] Planner without a weather place: the clock fills the top row (gap filling)
- [ ] 12-hour clock: "9:41" with AM/PM; at 12:xx without AM/PM where it doesn't fit
- [x] Partial refreshes over a few minutes: clock changes every minute without ghosting
- [ ] "old" badge: set a wrong weather place, wait 30+ minutes: weather keeps its last data and shows
      "old"; the status bar names "Weather". Retries: log shows retries after 2, 4, 8, 16 minutes, then hourly
- [ ] Fix the place again: the badge disappears at the next fetch

## 4. Stage 1: OTA

**Confirms stage 1:** a beta release installs itself at night (auto on) and via double tap + confirm
tap (auto off); notify-only shows the status-bar mark; below 30 % without USB it refuses with the
battery message; a corrupted, unsigned or wrong-board image is rejected; a release that crashes on
boot rolls back; settings survive.

Done earlier (results log, 2026-11-16/17):

- [x] Settings survive the USB migration and an update
- [x] Installs via double tap + confirm tap (auto off)
- [x] Corrupted image rejected; wrong-board image rejected; unsigned image rejected; a release that
      crashes on boot rolls back (workflow "OTA test release", [OTA_TESTS.md](OTA_TESTS.md))
- [x] The same with a beta release

Open (set channel **Beta**, auto install **off** first: setup app, Device tab):

- [x] **Release build 75 to beta**: GitHub → Actions → Build firmware → Run workflow: release ✔,
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

**Confirms stage 4:** tasks, note and birthdays are editable in the setup app and survive restarts
and updates.

- [x] In the app (Data tab): add 5 tasks (one overdue, one today, one high priority, one without date,
      one done), a note with umlauts and line breaks ("Müll: Donnerstag!"), 3 birthdays (one today with
      year, one in 3 days, one in February)
- [x] Layout **Personal** (Screen tab, "Ready-made layouts" from build 76; before: at the far right
      end of the structure strip): tasks in the right order (overdue first, date in a black box; high priority
      = filled box; the done one not shown), the note wrapped, birthdays with "Today" in a black box and
      the age
- [x] Umlauts, ß, €, `! ( ) & ' "` look right; an emoji typed into a task is removed when saving (the app says so)
- [x] Limits: the app allows at most 30 tasks (adding is greyed out at 30) and 400 characters for the note
- [x] **Birthday calendar link** (e.g. Google's birthday calendar as iCal): "Check" finds events; after
      the next wake its birthdays appear (marked as from the calendar in the app)
- [x] **Survives a restart** (hold 2 s): everything still there
- [x] **Survives an update:** release a build to beta and install it (block 4): everything still there
- [x] Next day (or change the device clock by waiting past midnight): "in 3 d" became "in 2 d", "today"
      labels moved (redrawn right after midnight)
- [x] A task ticked off shows "done 20.11." in the app; 30 days later it's gone (log: `[User] 1 done
      task(s) older than 30 days removed`)

## 6. Stage 5: the setup app

**Confirms stage 5:** a person without ESP32 knowledge sets up and personalizes a device using only a
phone (try it once with someone else at the end of this block).

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

- [x] With working Wi-Fi, **hold 5 s**: the screen shows **"Edit mode"**, the address
      (`http://<name>.local` and the IP), a QR code and a **6-digit code**
- [x] Phone in the home Wi-Fi: scan the code or open the address → the app asks for the code; the right
      code opens it; 5 wrong codes → a new code appears on the screen
- [x] `http://<name>.local` works (iPhone; on Android the IP may be needed)
- [x] A second phone without the code can't change anything (asks for the code)
- [ ] Device tab: **Change Wi-Fi** → the device restarts into the hotspot
- [ ] 10 minutes without use: back to the dashboard

### Diagnostics (build 72)

Seen on the device on 2026-11-19: the diagnostics view works, the firmware line is in the log.

- [x] After a few hours on the wall: Device → Diagnostics shows log lines of the Wi-Fi wakes (with
      times and a date line) and one battery value per hour
- [x] A broken calendar link (or Wi-Fi off for a while): it shows in the service table (failures,
      next retry) and in the log
- [x] **Download log** opens / saves a text file (iPhone captive browser and a normal browser)
- [x] **Clear log**: the list is empty; new lines appear after the next Wi-Fi wake
- [x] Battery chart: the case of 2026-11-19 (1.1-1.4 V with the battery plugged in) is visible as
      "no battery measured"; after the battery fix the curve rises while on USB
- [x] The USB serial monitor still shows every line live, secrets masked

### Fixes of build 73 (found in the device test, 2026-11-19)

- [x] Hotspot: save a setting → "Saved, restarting" → the device comes back in setup mode (hotspot
      again, same password), the phone reconnects, the app reloads by itself; **Done** shows the dashboard
- [x] Edit mode: save a setting → back in edit mode with the **same code**, the app goes on without
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

### Fixes of build 76 (device test, 2026-11-20)

- [x] Top bar: the battery in percent ("53 %") instead of volts, right of the battery icon (build 77);
      in full-width layouts the date on the left ("Fri 20 Nov")
- [x] Weather in a grid layout: the weekday above each icon ("Fri", "Sat", ...) (build 77)
- [x] Agenda: today's and tomorrow's events (in the evening too: tomorrow's appear under their own date)
      (build 77)
- [x] Exact preview after switching to 12 h (not saved yet): shows AM/PM where it fits

### Build 78: German / English, stock names, automatic beta

- [x] After the update the screen is German (dates "Di 17. Nov", "Dienstag, 17. November", weekdays,
      "Heute / Morgen", status notes, update screens); the app is German (tabs Start / Bildschirm /
      Daten / Gerät)
- [x] Gerät → Sprache: English → the device restarts, screen and app in English; back to Deutsch
- [x] Device messages in the app (problems, field errors, check results) in the chosen language
- [ ] Factory reset → first start begins in German with the language as step 1
- [x] Stocks `EUNL.DE:MSCI World,QYLE.DE:Nasdaq CC,PPFB.DE:Gold`: the screen shows the names, German
      prices with a decimal comma; a name over 12 characters is refused with a message
- [x] The push of build 78 published `firmware-.78` as a beta pre-release by itself (GitHub
      Releases); the update screen shows the German "What's new" lines with "(", ")" and "ü"
- [x] Later: Run workflow with release + channel stable promotes it (it becomes "Latest")

## 6b. Stage 6 part 1: power (build 75)

**Confirms stage 6 part 1:** clock interval, quiet hours and battery saver behave as set and as
described in the app.

- [x] Clock refresh "every 5 minutes": the display updates at :00, :05, :10 ...; the status bar says
      "Clock every 5 min"; the log shows wakes 5 minutes apart; calendar still once an hour
- [x] The app shows days per charge for each choice; the numbers change with the quiet hours on / off
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

## 8. Power measurements (separate session, needs a USB power meter)

**Confirms stage 6 and the estimates of stage 10:** real numbers instead of the one estimate so far
(about 3 % in 13 hours); `docs/POWER.md` and the app's battery estimate are written from them.

**No meter needed** (2026-11-25): with stage 10 the device measures itself from its hourly battery
readings (2-3 days on battery only, Diagnostics shows "gemessen: x % pro Tag"); this block is then
rewritten for that. The meter way below is only for anyone who has one anyway.

**Setup:** a USB power meter between the charger and the board that shows **0.1 mA or finer** and
counts **mAh** (e.g. the FNIRSI FNB48 / FNB58 kind; a simple "USB doctor" that shows 0.00 A can't
measure the sleep). **Battery disconnected** (otherwise the meter counts the charging current), the
display and touch sensor connected as always.

- [ ] Clock every **1 minute**: reset the meter's mAh counter, leave the device alone for exactly
      1 hour: ____ mAh (the hourly Wi-Fi wake and the full refresh are included)
- [ ] The same with the clock every **5 minutes**: ____ mAh in 1 hour
- [ ] The lowest steady reading between two wakes (deep sleep): ____ mA
- [ ] Send the numbers: battery life = capacity ÷ mAh per hour; from the two runs the cost of a clock
      wake and of a Wi-Fi wake follow (into `docs/POWER.md` and the estimate in the app)

## 9. Stage 7: new look (build 79)

**Confirms stage 7:** the screen looks like the app's sketch; holidays, school holidays and birthdays
show where planned; a device-tested beta is on stable.

PC renders of every case (German): [docs/images/layouts/](images/layouts/) (`month-*.png`, `clock-*.png`,
`holiday*.png`, January 2027 in Bavaria as the test date).

**Before the device test:** block M (after every push, Data feeds run once, the Google birthday question).

**On the device:**
- [x] Clock: filled Montserrat digits in every layout incl. Classic, date below, AM/PM with the 12-hour
      clock in a 2 × 1 box; readable with reflections
- [x] A clock in a 1 × 1 box (editor allows it now): small digits, short date
- [x] Headings with a line over the list boxes (not in Classic); weather weekdays and "9° / 2°"
- [x] Status bar: Wi-Fi bars left of the battery (after a wake with Wi-Fi), not touching the version
- [x] Edit mode code screen: the 6 digits in the new font, readable
- [x] Month view: today a small filled square, holiday a frame, school holidays a line, appointment
      dots only from links with "Im Monat markieren", birthday cake, bold weekend, KW, footer
- [xx] Month view in a full screen, a half (2 × 3), 2 × 2, 1 × 2, 2 × 1 (two weeks) and 1 × 1 (sheet)
      box, and in Classic: each looks tidy, nothing overlaps (the editor allows 1 × 1 now)
- [x] App → Data → "Feiertage und Ferien": Bundesland list, both switches turn on when a state is
      chosen; the school link field shows our link as placeholder
- [x] Bundesland set, Feiertage on: the holiday name after the date in the status bar on a holiday,
      "★" entry in "Heute und morgen" today and tomorrow (next real one: 25 Dec, or the 6 Jan in BW / BY / ST)
- [x] Schulferien on: within an hour the line appears in the month view (Christmas holidays), start /
      end in the footer; Diagnostics log: "school holidays: read, n ranges"
- [x] After the update from build 78 the month view has **no appointment dots** until "Im Monat
      markieren" is switched on for a link (off by default); on: dots, off: none
- [x] **Before** running the "Data feeds" workflow (feed missing) with Schulferien on: the log shows
      one failed attempt, then none for 6 hours (no Wi-Fi every minute; Diagnostics battery curve normal)
- [x] Bundesland → "Aus" (or another state): the old school holiday lines disappear at the next wake
- [x] "Heute und morgen" with an entry that began before today (e.g. a holiday trip) on a public
      holiday: every day once, in date order, the ★ line under today
- [x] Wi-Fi switched off at the router: "! WLAN" shows, the Wi-Fi bars disappear
- [x] A yearly all-day entry in a calendar link shows as a birthday (cake, birthday box), not as an
      appointment dot; "Name (1966)" shows the age
- [x] After many clock changes: no disturbing ghosting from the filled digits
- [x] A build with all of this promoted to stable (block M, "Promote a tested beta to stable")

## 9b. Stage 7b: own code base (build 80)

**Confirms stage 7b:** after the rebuild everything works and looks as before. The PC renders are
pixel-identical except the text font ([plans/STAGE7B_OWN_CODE.md](plans/STAGE7B_OWN_CODE.md), result).

- [x] Every service loads as before: weather (also a place given as "lat, lon"), stocks (incl. own
      names), calendar, parcels, flight with live status, printer (MQTT) and MakerWorld
- [x] Calendar: repeating entries (weekly on several days, "2nd Tuesday", yearly), an entry cancelled
      or excluded in a series, all-day entries, an entry with a time from Outlook / Exchange (were
      missing before), an entry with an alarm (its title stays)
- [x] The screen looks as before in every layout; the text font looks slightly different (rebuilt),
      nothing is cut off; the problem names show "Druckercode" for a wrong printer code
- [x] MakerWorld numbers right-aligned in their columns; the parcel line has even gaps
- [x] Printer with an AMS: spools 1-4 with their fill level; with two AMS units nothing overlaps
- [x] Setup only through the app; `/basic` is gone (the address shows the app); with JavaScript off the
      page says it needs JavaScript
- [x] Button: tap, double tap, 2 s and 4 s holds; the BOOT button backup after a reset; the setup hold
      while the dashboard is refreshing
- [x] Battery, Wi-Fi fast connect (log: "WiFi connected in … ms (… dBm)"), time zone, update check:
      as before
- [x] Settings kept after the update to build 80 (Wi-Fi, calendar links, printer, layout, pins)

### Build 81: birthday lines (device test, 2026-11-25)

Reported: "Geburtstag Lennart Kempf t…" — the word and the long name pushed the age out of the line.
The names are cleaned at the next calendar update (within the hour, or double tap).

- [ ] Birthdays from the calendar links show the name only: "Geburtstag Anna", "Geburtstag von Anna",
      "Anna hat Geburtstag", "Anna's birthday" all become "Anna" (the cake says it's a birthday)
- [ ] The age stays visible in every month size: "Anna wird 60" where it fits, otherwise
      "Lennart Kempf (30)", "Lennart K. (30)", or the name cut before the age ("Lenn… (30)")
- [ ] The same in the birthday widget: a long name is shortened, the "(30)" stays
- [ ] Full-screen month, side column: school holidays read "Fr 8. Jan · Ende" above the holiday's
      name (the name is no longer cut off)

### Build 82: quiet hours and battery (log of 2026-11-25)

- [ ] Quiet hours: after a tap at night the screen wakes **at** the end of the night (log: "quiet hours
      over" at e.g. 10:20:00, not a minute later; before, a tap's 25 s refresh pushed it to 10:21)
- [ ] Edit mode: the exact preview's battery percent matches the dashboard's (it showed 25 % while the
      dashboard showed 41 %: the reading with Wi-Fi on), and the app's battery stays the same after
      opening the preview

### Build 83: calendar downloads packed (log of 2026-11-25: 15 s for the work calendar)

- [ ] Log after a refresh: `[ICS] … events parsed in … ms (… KB, received packed: … KB)` for each
      calendar link; the work calendar's time well below the 15 s before (note the new number here: ____ ms)
- [ ] Agenda, month marks and calendar birthdays show the same entries as before
- [ ] The app's "Check" of a calendar link still counts its events
- [ ] School holidays and the birthday link still load (their lines in the log, no problem shown)
- [ ] Without Wi-Fi in the middle of a download (e.g. router off for a moment): the previous calendar
      stays on screen, the log says "answer incomplete or damaged"

### Build 84: fixes from the device test of build 83 (2026-11-25)

- [ ] Both calendar links load again: `[ICS] … events parsed in … ms (… KB, received packed: … KB,
      longest pause … ms)`; the work calendar's time and longest pause: ____ ms / ____ ms
- [ ] If a calendar still fails: the log line names the reason (send it), e.g. "no data for 30 s"
- [ ] Status bar with a problem: "⚠" and the sentence of the first problem (e.g. "Kalender 2 kann
      nicht gelesen werden"), "+1" when there are more; on a holiday the problem replaces its name
- [ ] Fonts: the text looks clean and even (no dot inside the "e", letters don't run into each other);
      everything still fits as before (agenda, widgets, status bar)
- [ ] USB power without a battery: the status bar shows "USB" (not "100 %"); with a battery that isn't
      full, the percentage as before
- [ ] No MakerWorld id: the printer box shows the printer (temperatures, AMS, spools), not old numbers

### Build 85: calendar downloads with the firmware's own decompressor (2026-11-25)

- [ ] Both calendar links load: `[ICS] 268 events parsed in … ms (… KB, received packed: … KB, …)` and
      the same for the work calendar (1,100 entries); its time: ____ ms (was ~16,000 ms unpacked)
- [ ] The app's "Check" of both links shows their number of entries (was "No connection")
- [ ] No "packed downloads off" line in the log (if there is one: the line before it names the reason;
      the calendars then still load, unpacked as before)

### Build 86: the packed download itself fixed (2026-11-25)

Build 85's log: "inflate error" after ~7 KB, then the safety net loaded both calendars unpacked
(15 s). The cause was in the download loop since build 84: chunked data reached the unpacker twice.

- [ ] After the update: `[ICS] 268 events parsed … (87 KB, received packed: 18 KB …)` and
      `[ICS] 1101 events parsed in ____ ms (2816 KB, received packed: ~440 KB …)`, no "inflate error",
      no "packed downloads off"
- [ ] The same after a restart (setup mode and back): still packed
- [ ] The app's "Check" of both links shows their number of entries

## 10. Stage 8: data store and several screens

**Confirms stage 8:** several screens switch by time and by tap, quickly and without extra battery
use; sleep screen and picture of the day work. Plan: [plans/STAGE8_DATA_AND_SCREENS.md](plans/STAGE8_DATA_AND_SCREENS.md).

### Part A, build 87: data store, intervals, calendar names

- [ ] After the update (a restart): every box loads once (log: one Wi-Fi session, all services)
- [ ] Data tab → **Load new data**: the four choices are there; e.g. weather 2 h: the log shows
      `[Weather]` only every 2 hours; with the clock every 30 min a 15-min choice comes every 30 min
- [ ] Downloads share Wi-Fi: no extra short Wi-Fi sessions a few minutes apart (log over an hour)
- [ ] The daily full refresh (and the redraw after an update screen) shows all boxes **without** a
      download (log: no Wi-Fi in that wake unless something was due); weather / stocks keep their values
- [ ] A tap still loads everything fresh (one screen)
- [ ] Calendar names: set "Familie" and "Arbeit" in the app; the log says `[ICS] Arbeit: … events`
- [ ] Work calendar on **Every 3 h**: between its reads the log says `[ICS] Arbeit: not due (every 3 h)`,
      its entries and marks stay on the screen; on **Mornings**: read once, at the first refresh from 06:00
- [ ] A broken link (e.g. one letter changed): the status bar says "Arbeit kann nicht gelesen werden";
      the other calendar's entries still update; fixing the link brings its entries back
- [ ] Wi-Fi off for a while: weather and stocks keep their values (the "old" badge after 2 intervals)

### Later parts (C, D)

- [ ] One screen (default): a tap refreshes like before
- [ ] Two screens: a tap shows the next one within a few seconds (no Wi-Fi in the log unless data is
      due), back to the scheduled one after the chosen 2 / 5 / 10 min; dots in the status bar
- [ ] Time windows: the screen changes at the window's start on the normal clock wake
- [ ] Hold 2 s: restart with fresh data
- [ ] Refresh interval per data type: e.g. the calendar every 2 h, visible in Diagnostics
- [ ] Sleep screen: drawn at the start of the quiet hours, "Schläft bis 06:00", tomorrow line; no wakes
      in the night (log); a tap at night shows the day screen, then the sleep screen again
- [ ] Picture of the day: a new picture each day with title and artist; with Wi-Fi off the last one stays
- [ ] Battery: a day with 2 screens and the picture uses about the same as a day before (Diagnostics)

## 11. Stage 9: profiles, gift mode, help *(not built yet)*

**Confirms stage 9:** profiles switch in one tap, a device can be prepared as a gift and set up at
the recipient's home with WLAN only.

- [ ] Switch between ready-made profiles: the screen redraws right away; calendars, tasks, location,
      clock interval and quiet hours stay
- [ ] Save an own profile with a name, change something, switch away and back; rename, delete
- [ ] "Neu einrichten" with a greeting: the gift screen, then deep sleep; with the battery
      disconnected the gift screen stays
- [ ] Battery connected again (or a tap): the welcome screen with QR; the app opens on the WLAN step;
      after WLAN the prepared dashboard appears
- [ ] Help from every tab, German and English; the link to the full guide opens it
- [ ] Quick-start card printed; its QR code joins the device's hotspot

## 12. Stage 10: export / import and extras *(not built yet)*

**Confirms stage 10:** a set-up copies to another device; waste and countdown boxes work.

- [ ] Export a file, import it on a second device (or after a factory reset): profiles and settings
      arrive, the target's WLAN and links stay; the file contains no password or link
- [ ] Waste box with the town's link: next pickups with icons; "Morgen: …" in the status bar from 18:00
- [ ] Countdown: "Noch n Tage bis …", "Heute: …" on the day, gone the day after
- [ ] Experiment (beta): on a USB charger the app is reachable at `<name>.local` without the hold;
      unplugged, the device is back to normal sleep within a few minutes (log); a nearly full battery
      without a charger isn't mistaken for one for long

## 13. Release validation (stage 11, separate session)

**Confirms stage 11:** a release candidate goes out to the first family device via the stable channel.

- [ ] First boot of a fresh device (factory reset or new board): guided setup only with a phone
- [ ] Wi-Fi loss for an hour: last data kept, "old" badges, Wi-Fi problem shown, recovers by itself
- [ ] Invalid input in every field of the app: a clear message, nothing saved
- [ ] Sleep / wake over a day: wakes on time, nothing missed (log)
- [ ] Touch: tap, double tap, holds still work after a day on the wall
- [ ] Persistence: settings, layout, tasks, note, birthdays after a power cut
- [ ] OTA and a failed OTA (block 4 and [OTA_TESTS.md](OTA_TESTS.md)) with the release candidate
- [ ] Power cut during a normal wake and during an update: the device comes back by itself
- [ ] 7 days on the wall without touching it: battery curve in Diagnostics, no hang
- [ ] **Release gate:** the release candidate is promoted to stable and installed on the first family
      device

## After the session

- [ ] Results into the results log below (the plan has nothing to tick: a stage is confirmed when its
      block here is complete)
- [ ] Problems found: one line each (what you did, what the screen showed, the serial log if you have it)

## Results log

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
