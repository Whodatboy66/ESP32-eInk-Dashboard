# Stage 7b: own code base (before the handover)

Asked for on 2026-11-23: "moving away from his project before handing out the devices is the smart
thing to do… Optimize his code. Keep logic the same, but rebuild the project. If you find any issues
with his code, improve as you go."

**Why:** the fork started from VoIPshare/ESP32-eInk-Dashboard (commit `8d0a66b`, 2026-10-12, no licence
file, inactive since). Technically nothing depends on it any more; what's left is his code in the
repository (about 4,000 lines in 45 files on 2026-11-23, `python3 tools/dev/origin_report.py`), which
is still his under copyright, and the visible fork link. The owner wants his own repository before
the family devices go out.

**Goal:** every part that still comes from the original is rebuilt from what it does (the services'
APIs, the file formats, the behaviour our tests and the device show), not translated line by line.
The behaviour stays the same; problems found on the way are fixed and listed. Then the repository
leaves the fork network (owner, DEVICE_TESTS block M) and gets a licence of the owner's choice
(done 2026-11-24: MIT, see below).

**Non-goals:** new features; changing the partition table, pin assignments or the update format
(those are facts the devices depend on: they stay, only their code is new).

## How (for every part)

1. **Pin the behaviour first:** host tests with recorded inputs (calendar files, API answers, MQTT
   messages) against today's code. They must pass before anything is rewritten.
2. **Rebuild:** a pure, host-tested part for the logic (parsing, formatting, decisions) and a thin
   part for network / hardware, like the rest of the project (`PowerPlan.h`, `MonthModel.h`).
3. **The same tests pass** on the new code; each improvement gets its own test that failed before.
4. **Renders** before / after for anything drawn; firmware sizes in the result.

## Parts, in order

| # | Part | Lines left | What it does |
|---|---|---|---|
| 1 | Old setup page `/basic` in `src/web/SetupServer.cpp` | ~610 | Removed: the app replaces it (checked on phones) |
| 2 | Fonts: converter `tools/ListFontConvertV4.py`, renderer `src/ui/SparseFont.cpp`, `SparseFontBase.h`, the generated icon and text fonts | ~2,200 incl. data | Own converter, own renderer, all fonts regenerated (icons are Material Design Icons, Pictogrammers Free License) |
| 3 | Calendar reader `src/services/Calendar.cpp` (iCal lines, dates, repeats, exceptions) | ~460 | A pure `IcsParser.h` with tests; Calendar.cpp only downloads |
| 4 | Data services: Weather, Stocks, Tracking, Flight, MakerWorld, HttpJson, ServiceUtil, Fetchers.h | ~600 | Pure answer parsers with recorded answers as tests; thin download code |
| 5 | Printer: `Bambu.cpp/.h` (MQTT), `Printer.cpp` (widget) | ~380 | Message parsing pure and tested |
| 6 | Widgets: `Widgets.cpp/.h` (stocks, parcels, weather, MakerWorld, flights), remnants in StatusBar, Clock, MonthView | ~600 | Rebuilt drawing, same places |
| 7 | Base: PinMapping, StatusLed, TouchController, EpaperDriver, BatteryMonitor / Curve, WifiService, TimeService, Problems, `configure.h`, `app.h`, ConfigManager, AppState, App.cpp, the `.ino` | ~800 | Rebuilt; pin tables and constants keep their values |
| 8 | Build files: `sketch.yaml`, `partitions.csv`, workflow remnants, `tools/epd_test` | ~160 | Rewritten; the partition table keeps its values |
| 9 | Repository: README credits ("started as a fork of …"), `LICENSE` (owner's choice), origin report at 0 for code | – | Done: README credits, MIT `LICENSE`, out of the fork network; the report shows only facts (see the result) |

Facts that have to stay as they are (partition offsets, pin numbers, API addresses and field names)
may still show up in the origin report when written the same way; they are listed in the result.

## Tests

Host tests for every new pure part (`tests/host/test_ics_parser.cpp`, `test_services.cpp`,
`test_bambu.cpp`, `test_sparse_font.cpp`, …) with recorded inputs in `tests/fixtures/`; PC renders of
every layout before and after (pixel-compared where nothing should change); all four profiles build.
On the device: [DEVICE_TESTS.md](../DEVICE_TESTS.md) block 9b (every service still loads, the screen
looks the same, setup without `/basic`).

## Decided by the owner (2026-11-24)

- The repository left the fork network and is under the **MIT** licence ([LICENSE](../../LICENSE));
  the third-party parts keep their own licences (README, "Credits and licenses").

## Result (build 80, 2026-11-23)

Done in code and checked on the PC: 29 host test files (new: `test_glyph_text`, `test_ics_parser`,
`test_service_parsers`, `test_printer_report`, `test_flight_text`, `test_pin_presets`,
`test_feed_schedule`, `test_agenda_days`), web tests, the font tool's and feed script's tests, all four
profiles build; PC renders of all 37 cases compared pixel by pixel before and after each part
(identical, except the text font, see below). Not yet on the device (DEVICE_TESTS block 9b).

**What came from the original and is now own code:** the old setup page `/basic` (removed), the font
converter and renderer and every font (rebuilt with `tools/fonts/make_font.py`), the calendar reader
(`IcsParser.h`), the data services and their answers (`ServiceParsers.h`, `HttpJson`), the printer
(`PrinterReport.h`, `Bambu.cpp`, `Printer.cpp`), the widgets (`Widgets.cpp/.h`, `FlightText.h`,
`ServiceData.h`), the pin presets (`PinPresets.h`, `PinMapping.cpp`), LED, button, display, battery,
Wi-Fi, time, the problem list, the settings (`app.h`, `ConfigManager.cpp`: one table of text settings)
and the wake cycle (`App.cpp`: phases, one helper per download result, one table for the redraws).
`python3 tools/dev/origin_report.py`: code from the original down from about 4,000 to 368 substantive
lines (2 %); what is left reads the same because it is a fact (API addresses and field
names, NVS keys, pin numbers, library versions, workflow syntax) or a single call that can't be
written another way.

**Problems found in the original and fixed:**
- A broken UTF-8 byte in a calendar title made the text drawing loop forever (the device hung until
  the watchdog reset it); a cut-off character at the end of a text was read past its end.
- Calendar entries with an Outlook time zone (`TZID="(UTC+01:00) …"`) were dropped (the colon inside
  the quotes was taken as the separator); `DURATION` instead of `DTEND` gave entries without length;
  an alarm inside an entry could replace its title.
- The stocks widget used its box before checking it exists.
- With two AMS units the second one's spools were drawn over the first.
- The alert colour followed a switch that was never set (always black, also on a colour display).
- The status bar icons' font was made with another threshold than documented (40, not 24).

**Also better:** about 4 KB less static RAM (unused layout fields; 123,416 → 119,560 bytes on the classic ESP32, 117,772 → 113,900 on the C6), the whole answer of a service is
no longer kept as one text in memory (parsed from the stream), error logs show only the host (no
paths or ids), one date helper instead of four copies, the font tool builds every font from one list
(`tools/fonts/fonts.txt`), the MakerWorld numbers are right-aligned, the parcel line uses fixed gaps.

**Changed on the screen (on purpose):** the text font was rebuilt from Montserrat at weight 600
(the original's base glyphs could not be reproduced); letters differ slightly, widths by about
0.3 px per character. Three German texts were adjusted so they still fit: "Druckercode" (was
"Zugangscode"), the MakerWorld columns, the parcel line.

**Tried and dropped:** YAML anchors to write the four build profiles' libraries once: arduino-cli
doesn't read them ("Profile not found").

**Review (a fresh reviewer, build 80) and fixes in the same build:** the wake cycle, the saved settings
(NVS keys and sizes), the sleep memory and the calendar repeats match the earlier build. Fixed:
- *Important:* a calendar file without a final line break lost its last entry (the new line reader
  could finish two lines at the end but kept one): test `test_no_final_newline`.
- *Important:* a download whose connection ended right after the headers (an empty answer, Wi-Fi gone)
  crashed the board, again at every wake; now it counts as a failed download.
- A TLS read error could make the download buffer be read past its end (also in the original).
- The Wi-Fi driver kept its own copy of the network and password in flash (the order of two calls);
  a factory reset now also clears that copy (the device may be given away).
- 3.8 KB of RAM for an empty list, large temporary copies on the stack: removed (reset() methods).
- A negative remaining print time is shown as 0; an empty AMS slot shows "Leer" (was "0%").
- A birthday entry that began before today is no longer collected; the user guide's log lines updated.
