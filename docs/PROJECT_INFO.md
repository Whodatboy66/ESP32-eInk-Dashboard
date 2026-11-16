# Project info (fork: Whodatboy66/ESP32-eInk-Dashboard)

Fork of [VoIPshare/ESP32-eInk-Dashboard](https://github.com/VoIPshare/ESP32-eInk-Dashboard).
The upstream README describes the features and setup page. This file covers our hardware,
our workflow and what we found while bringing it up.

Requirements: "Development Specification: Modular E-Ink Smart Dashboard Platform" (Google Doc, Jonas).
Related: [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md) (assessment + staged upgrades),
[OTA_UPDATES.md](OTA_UPDATES.md) (self-updating devices).

## Hardware

| Part | Model |
|---|---|
| Board | DFRobot FireBeetle 2 ESP32-C6 (4 MB flash, LiPo charger) |
| Display driver | Waveshare e-Paper Driver HAT (has the PWR pin) |
| Panel | 7.5" black/white 800x480 (GxEPD2 driver `GxEPD2_750_T7`) |

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
which is normal.

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

- The dashboard deep-sleeps between minutes, so its USB port is often unavailable. Before each upload:
  hold **BOOT**, tap **RST**, release **BOOT**. After the upload, tap **RST**.
- `monitor` needs `--profile` (otherwise: "no monitor available for the port protocol serial").
- The monitor disconnects when the board sleeps. Press RST to see a full boot log.
- Settings (Wi-Fi, keys, location) live in NVS and survive firmware uploads.
- Zigbee: use profile `esp32c6_firebeetle_zigbee`.

## Display test

`tools/epd_test` checks the display wiring on its own, with no Wi-Fi and no sleep:
```powershell
arduino-cli compile --profile esp32c6_firebeetle -u -p COM3 tools/epd_test
```
LED blinks 4x/s, the BUSY line is probed, and a test pattern is redrawn every 20 s. A good refresh
logs `took ~3400 ms -> DISPLAY RESPONDED`.

## Findings and fixes so far

| Issue | Status |
|---|---|
| `sketch.yaml` profiles used the default 1.25 MB partition; Zigbee profiles never set `USE_ZIGBEE` (arduino-cli ignores `build.properties` in profiles) | Fixed (`daa506c`) |
| Button GPIO2 uses `INPUT_PULLUP` but is active-HIGH (TTP223): with nothing connected the pin can read "held" and open setup mode by itself | Open (stage 0). Device currently boots normally |
| README's `upload.maximum_size=3604480` doesn't match `partitions.csv` (app0 = 0x360000 = 3538944) | Note only |
| CI workflow uses core 3.3.12 / GxEPD2 1.6.9 / GFX 1.12.6, profiles use 3.3.7 / 1.6.8 / 1.12.5 | To align before OTA releases |
| All HTTPS calls use `setInsecure()` (no certificate check) | Stage 2 (full security list: plan A.4) |
| Upstream repo has no license; HighSpeed font is personal-use only | See plan A.5 |
