# Build 72 plan: diagnostics in the setup app

The last open item of stage 5 ("diagnostics in the app", [IMPLEMENTATION_PLAN.md](../IMPLEMENTATION_PLAN.md)).
Asked for after the battery problem of 2026-11-19: see what the device did without a PC on USB.

**Done when:** the setup app (hotspot or edit mode) shows the recent log, the battery voltage of the
last days and the state of every service, and the log can be downloaded as a text file.

## Decisions

1. **Not live, kept:** the device sleeps about 59 s of every minute and is reachable only in setup or
   edit mode, so the log is stored on the device and shown later.
2. **One log for USB and the app:** every output (the `DBGF` lines and the direct `Serial.print`s,
   about 70 places) goes through the logger: printed on USB as before, secrets masked as before,
   and collected in RAM (4 KB per wake), each line with the local time (`09:41:20`), or the seconds
   since start before the clock is set; a date line when the day changes.
3. **Written to flash only after wakes that did something** (Wi-Fi, a problem changed, a full refresh,
   setup mode, an update): the once-a-minute clock wakes write nothing (flash wear, awake time).
   Files on the `userdata` partition: `/log.txt` up to 16 KB, then it becomes `/log.1` (two files,
   about 400-600 lines). A factory reset erases them.
4. **Battery history:** once an hour (every wake checks the time, cheap) the measured voltage and
   whether a computer's USB is seen, appended to `/battery.csv`; the last 7 days (168 values) are kept.
   The raw voltage is stored, also when it isn't plausible: that's what the diagnosis needs.
5. **Service state:** for weather, stocks, calendar, parcels: last success, failures in a row, next
   retry, "old" or not; the current problems; Wi-Fi failures and next attempt; last time sync;
   free memory.
6. **API:** `GET /api/diagnostics` (JSON, the last 200 log lines), `GET /api/log.txt` (whole log, plain
   text, for downloading), `POST /api/log/clear`. Same access rules as the rest (hotspot only, or the
   session in edit mode).
7. **App:** a "Diagnostics" section on the Device tab: battery chart (voltage over the days, the
   3.0-4.2 V band of a working battery marked), service table, log list (newest first, monospace),
   "Download log", "Clear log".

## Tests

- Host: log line assembly (time prefix, date line, line splitting of partial prints, 4 KB limit with
  a "cut" mark, masking still applied), battery history ring (168 values, trimmed), CSV parsing.
- Device ([DEVICE_TESTS.md](../DEVICE_TESTS.md)): after a few hours the app shows log lines of the
  Wi-Fi wakes and an hourly battery curve; a failed calendar link shows up in the service table and
  the log; the downloaded file opens as text.

## Result (build 72)

Done in code, checked on the PC (host tests, the app against the mock device in `web/dev/`), not yet
on the device ([DEVICE_TESTS.md](../DEVICE_TESTS.md), "Diagnostics").

- **All 69 direct `Serial.print`s now go through the logger** (`logPrint` / `logPrintln` /
  `logPrintf`), so the OTA, button and setup messages are in the stored log too; `DBG()` as well.
- **One lock around the log output:** the button task logs in parallel with the main task; a mutex,
  not a critical section, because the time prefix (`localtime_r`) may block.
- **Not stored:** lines of the button task's restart (2 s hold) and of a crash; the 3-minute hang timer
  restart. Everything else is written before sleep or restart.
- **Flash use:** at most one append per wake with Wi-Fi, plus one battery line per hour (about 20
  bytes); the log rotates at 16 KB into a second file.
- **App:** battery chart (7 days, the 3.0-4.2 V band of a working LiPo marked, hours with USB as
  filled dots), service table, log newest first with a filter, download, copy, clear. Bundle 33 KB
  gzipped.
- Memory: the per-wake log buffer (4 KB) is on the heap, made at the first log line: as a static
  buffer it overflowed the classic ESP32's static RAM by 3.3 KB (now 123 KB of 328 KB).

Verification: `tests/host/test_log_buffer.cpp` (line assembly, the cut mark, keeping the last lines;
also run with the address sanitizer, which found an off-by-one at the cut mark), all profiles compile.
