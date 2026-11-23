# Project info (fork: Whodatboy66/ESP32-eInk-Dashboard)

Fork of [VoIPshare/ESP32-eInk-Dashboard](https://github.com/VoIPshare/ESP32-eInk-Dashboard).
The user manual ([USER_GUIDE.md](USER_GUIDE.md)) describes the features and the setup app. This file
covers our hardware, our workflow and what is still open from bringing it up.

Requirements: "Development Specification: Modular E-Ink Smart Dashboard Platform" (Google Doc, Jonas).
Related: [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md) (assessment + staged upgrades),
[OTA_UPDATES.md](OTA_UPDATES.md) (self-updating devices).

## Hardware

Parts: see the [README](../README.md#hardware). Details: the FireBeetle 2 ESP32-C6 has 4 MB flash and a
LiPo charger; the Waveshare e-Paper Driver HAT has the PWR pin; the 7.5" black/white panel uses the
GxEPD2 driver `GxEPD2_750_T7`.

### Wiring (firmware preset "ESP32 C6")

| HAT | FireBeetle | Other label |
|---|---|---|
| VCC | 3V3 (not VIN/5V) | |
| GND | GND | |
| DIN | GPIO22 | MOSI |
| CLK | GPIO23 | SCK |
| CS | GPIO1 | D6 |
| DC | GPIO8 | D2 |
| RST | GPIO14 | D3 |
| BUSY | GPIO7 | D11 |
| PWR | GPIO4 | A3 |
| (button, optional) | GPIO2 | A1 |

HAT switches: **Interface Config = 0** (4-line SPI), **Display Config = B** (0.47 R, 7.5" panels).
Connections must be firm: solder headers or use a breadboard. Loose Dupont contacts gave
random "no response" / "busy timeout" results during bring-up.

Red LED next to the battery connector = charge indicator. It flickers when no battery is connected,
which is normal. The battery's red wire must end on the board's "+" pin: an inline adapter with
crossed wires connected our battery reversed ([results log](DEVICE_TESTS.md#results-log), 2026-11-19).

## Workflow

```
Claude (WSL, ~/ESP32-eInk-Dashboard)  --git push-->  github.com/Whodatboy66/ESP32-eInk-Dashboard
                                                          |
Personal PC  <--git pull--  ------------------------------+
   arduino-cli compile + upload over USB
```

Remotes in the WSL clone: `origin` = fork (push), `upstream` = VoIPshare (pull upstream changes).

## Build and flash (personal PC, Windows)

One-time setup:
```powershell
winget install --id Git.Git -e
winget install --id ArduinoSA.CLI -e
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
git clone https://github.com/Whodatboy66/ESP32-eInk-Dashboard.git
```

Every update:
```powershell
git pull
arduino-cli compile --profile esp32c6_firebeetle -u -p COM3 .
arduino-cli monitor -p COM3 --profile esp32c6_firebeetle -c baudrate=115200
```

- The dashboard deep-sleeps between clock wakes (1-30 minutes, and through the whole night quiet
  hours), so its USB port is often unavailable. Before each upload: hold **BOOT**, tap **RST**,
  release **BOOT**. After the upload, tap **RST**.
- `monitor` needs `--profile` (otherwise: "no monitor available for the port protocol serial").
- The monitor disconnects when the board sleeps. Press RST to see a full boot log, or read the stored
  log in the setup app (Device tab, Diagnostics).
- Settings (Wi-Fi, keys, location) live in NVS, tasks / note / birthdays and the log on the user-data
  partition; both survive firmware uploads.
- Setup mode without the touch sensor: tap **RST**, then within 3 s hold **BOOT** for 1 s.
- **One-time step for build 64 (OTA partition table):** the first upload of build 64 or later changes the
  flash layout to two app slots. Settings survive (NVS keeps its offset). After that, updates come over the
  air (double tap, setup app, or at night). Publishing a release: [OTA_UPDATES.md](OTA_UPDATES.md#publishing-a-release).
- **Releases are signed (build 66 on):** the GitHub secret `OTA_SIGNING_KEY` must hold the private key
  ([OTA_UPDATES.md](OTA_UPDATES.md#signing)).
- `tools/dev/build_all.sh [profile ...]` (WSL) builds the profiles like CI (pin preset flag, OTA slot
  limit) and prints a size table.

## Setup app (web/)

The setup app is plain HTML / CSS / JavaScript in `web/`, compiled into the firmware as
`src/web/generated/WebApp.h`:

```bash
python3 tools/web/build.py            # after every change in web/; CI fails if the header is out of date
python3 tools/web/build.py --check    # what CI runs (also checks the size limit)
```

To work on it without a device, open `web/dev/index.html` in a browser: the real app with a fake
device (`web/dev/mock.js` answers every API call of [SETUP_API.md](SETUP_API.md)). Options in the
address: `?first=1` first start, `?home=1` home network (code 123456), `?offline=1`, `?slow=1`,
`?down=1`, `?classic=1`, `?mem=1`, `?lowbat=1`, `?saver=1`.

## Tests

Hardware-independent logic is tested on the PC, also in CI: wake plan, power plan, layout and box
layouts, button gestures, config validation, time zones, manifest and signature, update policy,
personal data, log buffer, redaction, LAN rule, battery curve, service state (`tests/host/test_*.cpp`).

```bash
tests/host/run.sh                    # needs g++ (WSL: sudo apt install g++), or CXX=/path/to/g++
node --test tests/web/*.test.cjs     # the setup app's editor logic (needs Node.js)
```

`run.sh` also needs mbedTLS (`sudo apt install libmbedtls-dev`, or `MBEDTLS_PREFIX=...`) and the
ArduinoJson header (found in the Arduino libraries folder, or set `ARDUINOJSON_INCLUDE`).

## VS Code IntelliSense (red squiggles on `#include`)

The C/C++ extension doesn't know Arduino's include paths. Generate them from a real build
(needs Python; re-run after changing versions in `sketch.yaml`):
```bash
python3 tools/vscode_intellisense.py            # Windows: py tools\vscode_intellisense.py
```
This writes `.vscode/c_cpp_properties.json` and `.vscode/settings.json` (`.ino` = C++). Both are
local (gitignored) because the paths differ per machine. Then run "Developer: Reload Window" in VS Code.

## Display test

`tools/epd_test` checks the display wiring on its own, with no Wi-Fi and no sleep:
```powershell
arduino-cli compile --profile esp32c6_firebeetle -u -p COM3 tools/epd_test
```
LED blinks 4x/s, the BUSY line is probed, and a test pattern is redrawn every 20 s. A good refresh
logs `took ~3400 ms -> DISPLAY RESPONDED`.

## Open points

Fixed findings are in the [CHANGELOG](../CHANGELOG.md) and the [results log](DEVICE_TESTS.md#results-log).

| Issue | Status |
|---|---|
| Upstream repo has no license; HighSpeed font is personal-use only | Issue posted upstream (2026-11-19), no answer yet. See plan A.5 |
