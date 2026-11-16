# Self-updating devices (OTA from GitHub): feasibility analysis

**Question:** can I push to GitHub so that devices (e.g. at my parents') update themselves,
with no USB flashing?

**Answer: yes, it's very feasible.** Everything needed is in the ESP32 Arduino core we already use
(3.3.7). The one hard requirement is a **one-time USB flash per device** to switch to an OTA-capable
partition table. After that, every update can arrive over Wi-Fi.

## Status (stage 1)

Implemented: A/B partition table, update client with verified HTTPS (GitHub hosts only),
**signed manifest (ECDSA P-256)**, board-variant / display / settings-schema / build / min-build
checks, SHA-256 check, rollback, battery guard, night-time check with "Install updates
automatically" (auto or notify only), **stable/beta channels**, double tap + confirm tap, setup page
"Check for updates now", e-ink screens, CI release job (signs and publishes). Tested on the device so
far: USB migration with settings kept, update via double tap + confirm tap (build 64 → 65, unsigned).
The remaining "Done when" tests are listed under stage 1 in [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md).

Code: `src/update/UpdatePolicy.h` (decisions), `src/update/Manifest.h` (format), `src/update/Signature.cpp`
(signature check), all host-tested; `src/update/UpdateClient.cpp` (download + install),
`src/update/UpdateScreens.cpp` (screens), `src/update/OtaPublicKey.h` (public key), the flow in
`ESP32-eInk-Dashboard.ino` ("Over-the-air updates").

### One-time USB migration (per device)

The old firmware has a single app slot. Flash the new firmware once over USB with
`arduino-cli compile --profile esp32c6_firebeetle -u -p COM3 .` (writes bootloader, partition table,
`boot_app0` and app separately). The `nvs` partition keeps its offset, so Wi-Fi and all settings stay.
**Not** a release's `*.merged.bin`: it is padded with empty flash and erases the settings.
From then on updates come over the air.

### Signing

Devices from build 66 on only use a manifest whose `manifest.sig` verifies with the public key
compiled into them (`src/update/OtaPublicKey.h`). The private key exists only as the GitHub secret
`OTA_SIGNING_KEY` and in an offline backup.

- **Key pair:** `tools/ota/make_signing_key.sh` writes the private key outside the repository
  (default `~/ota-signing-key/ota_signing_key.pem`, mode 600) and the public key into
  `OtaPublicKey.h`. The key in the repository was created on 2026-11-16. The script refuses to
  replace an existing key (`--rotate` only for a lost or leaked one), and the release job refuses a
  public key that changed since the last release unless **rotate_key** is ticked.
- **Secret:** GitHub → Settings → Secrets and variables → Actions → New repository secret,
  name `OTA_SIGNING_KEY`, value = the whole PEM file (`-----BEGIN EC PRIVATE KEY-----` ...).
- **Backup:** keep a copy offline (password manager or USB stick), then delete the local file.
  **If the private key is lost, no device can be updated over the air any more** (each needs a USB
  flash with a new public key). If it leaks, anyone can sign firmware for the devices: make a new key
  pair, and flash every device over USB.
- The release job signs (`openssl dgst -sha256 -sign`) and verifies the signature against the public
  key in the repository before publishing: a missing or wrong secret stops the release.
- An unsigned or altered manifest shows "Update check failed: try again later" with
  "Release signature not valid: not installed". The image itself is covered through the SHA-256 in
  the signed manifest.

### Channels

| Channel | Devices read | Gets |
|---|---|---|
| stable (default) | `releases/latest/download/manifest.json` | regular releases only |
| beta | `releases/download/beta/manifest.json` | beta releases and regular releases |

`beta` is a fixed pre-release that only holds `manifest.json` + `manifest.sig`; every release job
(beta or stable) replaces them, so beta devices always see the newest build of either kind. A beta
release itself is a pre-release `firmware-.NN`, so stable devices never see it. To make a tested beta
available to everyone, publish the next build on stable (build numbers can't be reused). The
channel is set per device on the setup page ("Update channel").

### Publishing a release

Needs the `OTA_SIGNING_KEY` secret (see Signing).

1. Raise `FW_BUILD` in `configure.h` (e.g. 66 -> 67; `FW_VERSION` becomes ".67"), commit, push; CI must be green.
2. GitHub -> Actions -> "Build firmware" -> Run workflow on `main`: tick **release**, pick the
   **channel** (beta first for your own device, stable for everyone), fill **whats_new** (up to 3
   lines separated by `|`, max. 70 characters each, letters/digits/spaces and `#%*+,-./:=?_~` only),
   leave **min_build** at 0 unless older builds must not update directly.
3. The release `firmware-.67` gets every board's images, `manifest.json` and `manifest.sig`.
   Auto-install devices on that channel install it the next night, the others show
   "Update available"; a double tap installs it at once.

**Build 66 must go out on stable:** devices with build 64/65 don't know channels or signatures yet
and only read the latest regular release. From 66 on, beta works. (64/65 also don't check the
display: a 4-color device on 64/65 pointed at this repository would install the black-and-white
build. All devices of this fork are black and white.)

Testing broken releases (wrong board, bad checksum, unsigned, crash, hang) on a beta device:
[OTA_TESTS.md](OTA_TESTS.md).

Test a release on your own device first (beta channel, double tap) before parents' devices pick it
up at night. A new version must confirm itself within 3 minutes (Wi-Fi + display worked); a crash,
hang, restart or missing Wi-Fi before that rolls back to the previous version, and that build is then
not auto-installed again. A release that starts fine but misbehaves later is not caught: publish
the next build to replace it.

### Manifest format

```json
{"schema": 1, "build": 66, "version": ".66", "min_build": 0, "tag": "firmware-.66",
 "notes": ["Up to 3 What's new lines"],
 "files": {"esp32c6_firebeetle": {"name": "ESP32C6-FireBeetle-ESP32-eInk-Dashboard.ino.bin",
                                  "size": 1461168, "sha256": "<64 hex digits>",
                                  "display": "bw750", "config_schema": 1}}}
```

Written by `tools/ota/make_manifest.py` in the release job; `display`, `config_schema` and the build
are read out of each compiled image (the `OTA-ID:<variant>:<display>:<schema>:<build>:END` string),
so they always match what was built. `manifest.sig` = DER ECDSA signature of the exact
`manifest.json` bytes. The device reads only the entry for its own variant (`FW_VARIANT`) and
installs it only if it is newer, built for its display (`FW_DISPLAY`), and able to read its settings
(`config_schema` >= the device's `FW_CONFIG_SCHEMA`). It refuses unsafe names and an unknown `schema`.

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
| Someone else publishes a release / GitHub account compromised | Device accepts only manifests signed with the private key (kept as a GitHub Actions secret or on the developer PC, **never in the repo or build artifacts**). The manifest pins the image SHA-256, so the image is covered too. Limit: anyone with write access to the repository can run the release job, which uses the secret: keep 2FA on (optionally a GitHub environment with a required reviewer for the release job) |
| Downgrade to an old, vulnerable build | Rejected: build number must increase; `min_build` in the manifest |
| Private repo | Not possible without putting a GitHub token on every device. **The repo must stay public** (firmware contains no secrets; user settings stay on the device) |

## GitHub side

`.github/workflows/compile.yml` builds every variant from the `sketch.yaml` profiles (same versions as
local builds) with the OTA partition table, and a manual run with "release" publishes
`firmware-<FW_VERSION>` with all images and `manifest.json` (sizes + SHA-256 of the app images).

- [x] App-only `.bin` per board + `manifest.json`, published only on a manual trigger
- [x] Core and library versions from `sketch.yaml`
- [x] Actions enabled on the fork
- [x] Sign the manifest (`manifest.sig`) with the `OTA_SIGNING_KEY` secret; pre-release = beta channel

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
