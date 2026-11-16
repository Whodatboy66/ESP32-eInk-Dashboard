# Self-updating devices (OTA from GitHub): feasibility analysis

**Question:** can I push to GitHub so that devices (e.g. at my parents') update themselves,
with no USB flashing?

**Answer: yes, it's very feasible.** Everything needed is in the ESP32 Arduino core we already use
(3.3.7). The one hard requirement is a **one-time USB flash per device** to switch to an OTA-capable
partition table. After that, every update can arrive over Wi-Fi.

## Status (stage 1, part 1)

Implemented, **not yet tested on hardware**: A/B partition table, update client with verified HTTPS,
board-variant / build / min-build checks, SHA-256 check, rollback, battery guard, night-time check with
"Install updates automatically" (auto or notify only), double tap + confirm tap, setup page
"Check for updates now", e-ink screens, CI release job with `manifest.json`.
**Still open:** ECDSA signature of the manifest, stable/beta channels, display id / config schema.
Until the signature exists, the devices trust whatever a release of this repository contains: only
publish from a trusted account (2FA on).

Code: `src/update/UpdatePolicy.h` (decisions, host-tested), `src/update/Manifest.h` (format, host-tested),
`src/update/UpdateClient.cpp` (download + install), `src/update/UpdateScreens.cpp` (screens), the
flow in `ESP32-eInk-Dashboard.ino` ("Over-the-air updates").

### One-time USB migration (per device)

The old firmware has a single app slot. Flash the new firmware once over USB with
`arduino-cli compile --profile esp32c6_firebeetle -u -p COM3 .` (writes bootloader, partition table,
`boot_app0` and app separately). The `nvs` partition keeps its offset, so Wi-Fi and all settings stay.
**Not** a release's `*.merged.bin`: it is padded with empty flash and erases the settings.
From then on updates come over the air.

### Publishing a release

1. Raise `FW_BUILD` in `configure.h` (e.g. 64 -> 65; `FW_VERSION` becomes ".65"), commit, push; CI must be green.
2. GitHub -> Actions -> "Build firmware" -> Run workflow on `main`: tick **release**, fill **whats_new**
   (up to 3 lines separated by `|`, max. 70 characters each, plain text), leave **min_build** at 0
   unless older builds must not update directly.
3. The release `firmware-.65` gets every board's images plus `manifest.json`. Devices look at
   `releases/latest/download/manifest.json`: auto-install devices install it the next night, the
   others show "Update available"; a double tap installs it at once.

Test a release on your own device first (double tap) before parents' devices pick it up at night.
A new version must confirm itself within 3 minutes (Wi-Fi + display worked); a crash, hang, restart
or missing Wi-Fi before that rolls back to the previous version, and that build is then not
auto-installed again. A release that starts fine but misbehaves later is not caught: publish the
next build to replace it.

### Manifest format

```json
{"schema": 1, "build": 65, "version": ".65", "min_build": 0, "tag": "firmware-.65",
 "notes": ["Up to 3 What's new lines"],
 "files": {"esp32c6_firebeetle": {"name": "ESP32C6-FireBeetle-ESP32-eInk-Dashboard.ino.bin",
                                  "size": 1458848, "sha256": "<64 hex digits>"}}}
```

Written by `tools/ota/make_manifest.py` in the release job. The device reads only the entry for its
own variant (`FW_VARIANT`), refuses unsafe names, unknown `schema`, and builds that aren't newer.

## What the device needs

| Building block | Available? | Notes |
|---|---|---|
| Write new firmware while running | Yes | `Update` / `HTTPUpdate` libraries in the core |
| Two app slots (A/B) | Yes (build 64) | `partitions.csv` has `otadata` + `app0`/`app1`. Devices with older firmware need one USB flash |
| Automatic rollback if new firmware fails | Yes | `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=1` in the C6 core; the core has `verifyRollbackLater()` / `esp_ota_mark_app_valid_cancel_rollback()` hooks |
| Verified HTTPS to GitHub | Yes | `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=1`, `NetworkClientSecure::setCACertBundle()`. The project currently uses `setInsecure()` everywhere and should change that for updates at least |
| Follow GitHub's download redirect | Yes, with care | `HTTPClient::setFollowRedirects()`. The core forgets the certificate bundle when it reconnects for a redirect to another host, so `UpdateClient.cpp` attaches it again on every connect (and only allows github.com / *.githubusercontent.com) |
| Keep settings across updates | Yes | Settings live in NVS (`0x9000`); OTA only writes the inactive app slot |

## Partition table (4 MB flash)

Keep `nvs` at the same offset, so the one-time USB flash **keeps existing settings**. Zigbee partitions
are dropped (Zigbee support was removed from the firmware); a small LittleFS area holds tasks/notes/layout (spec §7.4):

```
# Name,     Type, SubType,  Offset,   Size
nvs,        data, nvs,      0x9000,   0x5000      # unchanged -> settings survive
otadata,    data, ota,      0xE000,   0x2000      # new: which slot boots
app0,       app,  ota_0,    0x10000,  0x1E0000    # 1.875 MB
app1,       app,  ota_1,    0x1F0000, 0x1E0000    # 1.875 MB
userdata,   data, spiffs,   0x3D0000, 0x20000     # 128 KB LittleFS: tasks, notes, layout, caches
coredump,   data, coredump, 0x3F0000, 0x10000     # 64 KB crash diagnostics
```

The current `spiffs` partition (384 KB) isn't used by the code (no SPIFFS/LittleFS calls).
User data in LittleFS isn't touched by app OTA, so it survives updates like NVS does.

| Build | Size today | Share of a 1.875 MB slot |
|---|---|---|
| esp32c6_firebeetle (target) | 1,354,854 B | 69 %, about 600 KB headroom |

**Risk:** future features grow the firmware. CI checks the size against the slot limit on every build.

## Update flow (recommended design)

```
GitHub (fork)                                  Device
-------------                                  ------
git push / tag v.64
  -> Actions builds every board variant
  -> Release "firmware-.64" with:
       ESP32C6-FireBeetle.bin
       ...
       manifest.json  {build: 64, min_build, channel,
                       files: {board+display: {name, sha256, size, schema}}}
       manifest.sig   ECDSA P-256 signature of manifest.json

                                 once a day (e.g. 03:00, already awake for the clock):
                                   GET github.com/<owner>/<repo>/releases/latest/download/manifest.json
                                   verify manifest.sig with the public key built into the firmware
                                   build > FW_BUILD, board + display + schema compatible?
                                   -> download .bin over HTTPS (certificate bundle)
                                   -> write it to the inactive slot, check size + SHA-256
                                   -> reboot into the new slot (marked "pending verify")
                                 new firmware boots:
                                   Wi-Fi OK + display refresh OK -> mark valid
                                   crash / reset before that     -> bootloader rolls back to the old slot
```

Design details:

- **Stable URL, no API parsing:** `https://github.com/<owner>/<repo>/releases/latest/download/manifest.json`
  always points to the newest *non-prerelease*, has no API rate limit and is a tiny file. The device
  only parses a small JSON (ArduinoJson is already a dependency).
- **Version number:** add an integer `FW_BUILD` next to the text `FW_VERSION` (".63"), and compare integers.
- **Board variant:** each build bakes in its variant name (e.g. `esp32c6_firebeetle`) and only accepts
  the matching file, so a classic-ESP32 image can never land on a C6.
- **Release channels:** mark a release as *pre-release* to test it on my own device first (a "beta"
  setting reads the newest pre-release manifest). Parents' devices stay on "stable" and only get
  releases I've promoted.
- **When to check:** once a night automatically, plus on demand: **double tap** on the touch sensor or the
  setup page button **"Check for updates now"** (backup if the sensor fails). An update costs about 1.4 MB
  of download (10-20 s of Wi-Fi), which is negligible for the battery when it happens only on new releases.
- **Auto-install vs. notify only:** setup page checkbox **"Install updates automatically"** (per device,
  default on). With it off, the night-time check only shows an **"Update available"** mark in the status
  bar; installing then takes a double tap + confirm tap (or the setup page button).
- **Battery guard (decided):** below 30 % battery no update is installed unless USB is connected
  (the firmware already measures the voltage). The screen says "Battery too low for an update: please charge".
- **Visible on the display:** info screen (version, channel, last update, battery, Wi-Fi) → "New version
  .65" with up to 3 "What's new" lines from the release notes → "Tap to install (cancels in 15 s)" →
  progress steps (Downloading %, Verifying, Installing, Restarting; partial refresh about 1.6 s per step) →
  after the restart "Updated to .65 ✓" once. Failures in plain language ("Update failed: previous version kept").
  Details and checklist: stage 1 in [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md).

## What keeps it safe

| Failure | What happens |
|---|---|
| Power loss / Wi-Fi drop during download | The old slot is untouched and stays active. Retry next day |
| Corrupt download | SHA-256 / size mismatch -> not activated |
| New firmware crashes on boot | Rollback to the previous slot (bootloader) |
| New firmware runs but its update code is broken | **Can't fix itself remotely.** Mitigation: keep the update code small and stable, run it early in the wake cycle, and always test on my own device (beta channel) first |
| Someone else publishes a release / GitHub account compromised | Device accepts only manifests signed with the private key (kept as a GitHub Actions secret or on the developer PC, **never in the repo or build artifacts**). The manifest pins the image SHA-256, so the image is covered too |
| Downgrade to an old, vulnerable build | Rejected: build number must increase; `min_build` in the manifest |
| Private repo | Not possible without putting a GitHub token on every device. **The repo must stay public** (firmware contains no secrets; user settings stay on the device) |

## GitHub side

`.github/workflows/compile.yml` builds every variant from the `sketch.yaml` profiles (same versions as
local builds) with the OTA partition table, and a manual run with "release" publishes
`firmware-<FW_VERSION>` with all images and `manifest.json` (sizes + SHA-256 of the app images).

- [x] App-only `.bin` per board + `manifest.json`, published only on a manual trigger
- [x] Core and library versions from `sketch.yaml`
- [x] Actions enabled on the fork
- [ ] Sign the manifest (`manifest.sig`) with a secret key; pre-release = beta channel

## Effort estimate

| Part | Effort |
|---|---|
| OTA partition table + USB migration flash | small |
| Update client (manifest, signature check with mbedTLS, download, verify, rollback marking) | medium (about 300-400 lines) |
| CI: manifest + app binaries + release channels | small-medium |
| Display/setup-page integration (version, "check now", channel) | small |

**Recommendation:** make OTA one of the **first** stages. Every later feature then reaches the
parents' devices without a USB cable. The one-time USB flash should happen before the devices
leave the house.
