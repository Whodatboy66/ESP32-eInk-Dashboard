# Stage 6 plan, part 1: clock interval, night quiet hours, battery saver

Detailed plan for the first part of stage 6 of [IMPLEMENTATION_PLAN.md](../IMPLEMENTATION_PLAN.md)
(power management). Motivation: the diagnostics log of 2026-11-20 (build 73): the once-a-minute
clock refresh is the largest share of the battery use (estimated 50-60 %), the hourly Wi-Fi wake the
second (25-30 %).

**Done when:** the clock interval and the quiet hours are set in the setup app (with an estimate of
the battery life for each choice), the device follows them, and below 20 % battery it slows down by
itself.

## Decided by the owner (2026-11-20)

1. **Night quiet hours,** configurable, **default 23:00-06:00**, can be switched off: no clock
   updates and no fetches; the screen keeps its last picture with a small note.
2. **Battery saver:** the screen refreshes every **15 minutes**, with a small note.
3. **Configurable clock refresh**, with the power saving and the estimated battery life shown for
   each choice.

Decided earlier (stage 6 checklist): the saver starts below **20 %**, ends above **30 %** or when a
computer's USB is seen; below **5 %** a "Please charge" screen and only the button wakes it; a
reading that isn't plausible (no battery measured) never triggers it. (Changed during the work: the
critical state checks the battery once an hour, so the dashboard comes back by itself after charging.)

## Design decisions

1. **Clock interval choices: 1, 2, 5, 10, 15, 30 minutes** (default 1 = today). Wakes are aligned to
   the wall clock (every 5 min: :00, :05, :10 ...), so the clock always shows a "round" time that is
   correct at the moment it's drawn.
2. **Refresh intervals stay in minutes.** Wakes keep counting minutes (the counter advances by the
   interval at each wake); a refresh that isn't a multiple of the interval is rounded up to the next
   multiple (the printer's 4-minute poll while printing becomes 5 with a 5-minute clock). Hourly and
   daily things stay on their hour, so they still share one Wi-Fi session. Tested on the PC.
3. **Night quiet hours:** at the first wake inside the window the status bar gets the note "Night
   until 06:00" (a partial refresh of the bar only), then one deep sleep until the end of the window.
   The first wake after it is a full refresh with fresh data. A button tap at night shows the current
   dashboard and goes back to the night sleep. The window may wrap past midnight; start = end means off.
4. **Night update check:** due from 03:00 until noon (was: only between 03:00 and 03:59), once per
   day. With quiet hours over 03:00 it runs at the first wake after them (06:00 by default); if Wi-Fi
   failed at 03:00 it's retried at the next Wi-Fi wake instead of being skipped for the day.
5. **Battery saver:** the interval becomes 15 minutes (or the chosen one if that's longer). Data
   intervals stay as they are (rounded to 15 minutes). The note: "Battery saver: every 15 min".
6. **Critical (below 5 %):** a full screen "Battery empty: please charge" (with the last voltage);
   once an hour a short wake only reads the battery (nothing drawn); above 10 % (or on a computer's
   USB) the dashboard comes back with a full refresh. A tap shows the charge screen again.
7. **Notes in the status bar** (left, after the voltage): "Night until 06:00", "Battery saver: every
   15 min", "Clock every 5 min" (only when the chosen interval isn't 1). Without a status bar (own
   layout) the note isn't shown on the screen; the app shows the mode.
8. **Battery life estimate** (pure function, shown in the app for each choice): energy per day =
   active hours × (clock wakes per hour × cost of a clock wake + cost of the hourly Wi-Fi wake) +
   24 h × sleep current; days = battery capacity × 85 % usable / energy per day. Costs from the
   log of 2026-11-20 and typical values (clock wake about 2.5 s at 32 mA, Wi-Fi wake about 27 s at
   100 mA, sleep 0.05 mA), to be replaced by measured values (the measurement step of stage 6). The
   battery capacity is a setting (default 3000 mAh). The app shows the days and the gain against
   "every minute", clearly marked as an estimate.
9. **Settings** (NVS, defaults keep today's behavior except the quiet hours): `clk_int` (minutes),
   `quiet_on`, `quiet_from`, `quiet_to` (minutes of the day), `bat_mah`. No schema change (new keys
   with defaults).
10. **API:** the settings above in `/api/settings` and `/api/state` (`clock_interval`, `quiet_enabled`,
   `quiet_from` / `quiet_to` as "HH:MM", `battery_mah`), and `power` in `/api/state`: current mode,
   note, and the estimate for each interval choice. The app: a "Power" section (Screen tab: clock
   interval with the estimates; Device tab: quiet hours, battery capacity).

## Steps

1. `src/core/PowerPlan.h` (pure): quiet window, saver with hysteresis, critical, step, aligned
   sleep, notes, estimate; host tests.
2. `WakePlan`: interval input, rounding; tests (existing ones unchanged with interval 1).
3. Night check window (`UpdatePolicy`), test.
4. App: mode decision per wake, night branch, critical screen, aligned sleep, counter, notes.
5. Settings, API, app (Power section), docs, `FW_BUILD` 75.

## Tests

- Host: quiet window incl. wrap past midnight and start = end; saver enter/leave (20 / 30 %, USB,
  implausible reading); critical; aligned sleep for each interval incl. the hour boundary; rounding
  of refresh intervals; due tests with interval 5 and 15 (hourly fetch still once an hour); the
  estimate (monotonic, longer interval = more days, quiet hours = more days); night check window.
- Device ([DEVICE_TESTS.md](../DEVICE_TESTS.md)): a 5-minute clock (round times, the note); night
  window set to the next few minutes (note, no wakes in the log, full refresh after); battery saver
  (set the thresholds with a nearly empty battery, or watch it below 20 %); the estimate shown.

## Out of scope (later in stage 6)

Power profiles as a whole (Battery Saver / Balanced / USB Powered), per-data refresh intervals in the
app, the app always reachable on USB power, the current measurements.

## Result (build 75)

Done in code and checked on the PC (host tests, renders of the status bar notes, the app against the
mock device), not yet on the device ([DEVICE_TESTS.md](../DEVICE_TESTS.md), block 6b).

- **`src/core/PowerPlan.h`** decides per wake: normal / night / saver / critical, the wake interval,
  the note, the next wake (aligned to the interval on the wall clock, or the end of the night).
  `tests/host/test_power_plan.cpp` covers the window (incl. past midnight), the hysteresis of the
  saver (20 / 30 %) and of the critical state (5 / 10 %), USB and implausible readings, aligned sleeps
  and the estimate.
- **The wake counter stays in minutes:** it advances by the interval and is moved onto a multiple of
  it when the interval changes, so hourly fetches stay hourly (host test: 24 calendar fetches a day
  with a 5- and a 15-minute clock).
- **The estimate is calibrated** to the only real number so far (about 3 % in 13 hours, every minute,
  no quiet hours, 3000 mAh: about 20 days): every minute 20 / 28 days (without / with 23-06 quiet
  hours), every 5 minutes 56 / 77, every 15 minutes 79 / 108, every 30 minutes 88 / 120. The split
  between clock wakes, Wi-Fi and sleep is a guess until the current measurements.
- **Critical:** changed from the plan: a battery check once an hour instead of no timer wake, so the
  dashboard comes back by itself after charging (it leaves the state above 10 %).
- **The sleep code is one function** (`sleepNow`): display off, button wake armed, log written; used by
  the normal end of a wake, the night and the critical branch.
- **Setup / edit mode** don't run the wake's power decision; the API makes the same decision for
  "now" so the app can show the note.
- Not done in this part (see "Out of scope"): profiles, per-data intervals in the app, always-on on
  USB, measurements.

**Device test fixes (build 82, 2026-11-25):** after a tap at night the sleep until the end of the
window is counted from the minute the wake ends in (`power::sleepAfterNightWake`; a 25 s refresh used
to cross a minute and wake one minute late), and past the end it follows the normal clock rhythm. The
status bar shows the last battery reading with the radio off while Wi-Fi or the hotspot is on
(`batteryForScreen`): the edit mode preview showed 25 % instead of 41 % and stored that reading.
