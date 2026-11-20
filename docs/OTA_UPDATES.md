# Self-updating devices (OTA from GitHub)

Devices update themselves over Wi-Fi from this repository's GitHub releases. Each device needs
**one USB flash** to switch to the OTA partition table (build 64 or later); after that no cable is
needed. User guide: [USER_GUIDE.md](USER_GUIDE.md#firmware-updates).

## Status (stage 1)

Implemented: A/B partition table, update client with verified HTTPS (GitHub hosts only),
**signed manifest (ECDSA P-256)**, board-variant / display / settings-schema / build / min-build
checks, SHA-256 check, rollback, battery guard, daily check (03:00 until noon) with "Install updates
by themselves" (auto or notify only), **stable/beta channels**, double tap + confirm tap, "Check for
updates now" in the setup app, e-ink screens, CI release job (signs and publishes).

Tested on the device: USB migration with settings kept, update via double tap + confirm tap (build 64
→ 65), and the broken-update tests on the beta channel (wrong board, bad checksum, unsigned, crash on
boot; 2026-11-17). Still open: the night-time install, the notify-only mark and the battery refusal
(stage 1 in [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md), block 4 of [DEVICE_TESTS.md](DEVICE_TESTS.md)).

Code: `src/update/UpdatePolicy.h` (decisions), `src/update/Manifest.h` (format), `src/update/Signature.cpp`
(signature check), all host-tested; `src/update/UpdateClient.cpp` (download + install),
`src/update/UpdateScreens.cpp` (screens), `src/update/OtaPublicKey.h` (public key), the flow in
`src/update/OtaFlow.cpp` (night check, double tap session, install, confirm or roll back).

### One-time USB migration (per device)

Firmware before build 64 has a single app slot. Flash the new firmware once over USB with
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
channel is set per device in the setup app (Device tab, **Update channel**).

### Publishing a release

Needs the `OTA_SIGNING_KEY` secret (see Signing).

1. Raise `FW_BUILD` in `configure.h` (e.g. 76 -> 77; `FW_VERSION` becomes ".77"), commit, push; CI must be green.
2. GitHub -> Actions -> "Build firmware" -> Run workflow on `main`: tick **release**, pick the
   **channel** (beta first for your own device, stable for everyone), fill **whats_new** (up to 3
   lines separated by `|`, max. 70 characters each, letters/digits/spaces and `#%*+,-./:=?_~` only),
   leave **min_build** at 0 unless older builds must not update directly.
3. The release `firmware-.77` gets every board's images, `manifest.json` and `manifest.sig`.
   Auto-install devices on that channel install it with the next daily check, the others show
   "Update available"; a double tap installs it at once.

Devices with build 64/65 don't know channels or signatures and only read the latest regular
release; from 66 on, beta works. (64/65 also don't check the display: a 4-color device on 64/65
pointed at this repository would install the black-and-white build. All devices of this fork are
black and white.)

Testing broken releases (wrong board, bad checksum, unsigned, crash, hang) on a beta device:
[OTA_TESTS.md](OTA_TESTS.md).

Test a release on your own device first (beta channel, double tap) before parents' devices pick it
up at night. A new version must confirm itself within 3 minutes (Wi-Fi + display worked); a crash,
hang, restart or missing Wi-Fi before that rolls back to the previous version, and that build is then
not auto-installed again. A release that starts fine but misbehaves later is not caught: publish
the next build to replace it.

### Manifest format

```json
{"schema": 1, "build": 76, "version": ".76", "min_build": 0, "tag": "firmware-.76",
 "notes": ["Up to 3 What's new lines"],
 "files": {"esp32c6_firebeetle": {"name": "ESP32C6-FireBeetle-ESP32-eInk-Dashboard.ino.bin",
                                  "size": 1461168, "sha256": "<64 hex digits>",
                                  "display": "bw750", "config_schema": 3}}}
```

Written by `tools/ota/make_manifest.py` in the release job; `display`, `config_schema` and the build
are read out of each compiled image (the `OTA-ID:<variant>:<display>:<schema>:<build>:END` string),
so they always match what was built. `manifest.sig` = DER ECDSA signature of the exact
`manifest.json` bytes. The device reads only the entry for its own variant (`FW_VARIANT`) and
installs it only if it is newer, built for its display (`FW_DISPLAY`), and able to read its settings
(`config_schema` >= the device's `FW_CONFIG_SCHEMA`). It refuses unsafe names and an unknown `schema`.

## Partition table (4 MB flash)

`nvs` kept its offset, so the one-time USB flash **keeps existing settings**
([partitions.csv](../partitions.csv)):

```
# Name,     Type, SubType,  Offset,   Size
nvs,        data, nvs,      0x9000,   0x5000      # settings and secrets (offset unchanged)
otadata,    data, ota,      0xE000,   0x2000      # which slot boots
app0,       app,  ota_0,    0x10000,  0x1E0000    # 1.875 MB
app1,       app,  ota_1,    0x1F0000, 0x1E0000    # 1.875 MB
userdata,   data, spiffs,   0x3D0000, 0x20000     # 128 KB LittleFS: tasks, note, birthdays, log, battery history
coredump,   data, coredump, 0x3F0000, 0x10000     # 64 KB crash diagnostics
```

OTA only writes the inactive app slot, so NVS and the user data survive updates. CI fails a build
whose app image is larger than a slot; the run summary shows the size of each build.

## How it works

```
GitHub (fork)                                  Device
-------------                                  ------
manual run of "Build firmware" with "release"
  -> Actions builds every board variant
  -> Release "firmware-.76" with:
       ESP32C6-FireBeetle-ESP32-eInk-Dashboard.ino.bin
       ...
       manifest.json  {build, min_build, notes,
                       files: {variant: {name, sha256, size, display, config_schema}}}
       manifest.sig   ECDSA P-256 signature of manifest.json
  -> the "beta" pre-release gets the same manifest

                                 once a day, at the first wake after 03:00 (until noon),
                                 or on a double tap / "Check for updates now":
                                   GET the manifest of the device's channel
                                   verify manifest.sig with the public key built into the firmware
                                   build > FW_BUILD, variant + display + schema compatible?
                                   -> download .bin over HTTPS (certificate bundle)
                                   -> write it to the inactive slot, check size + SHA-256
                                   -> reboot into the new slot (marked "pending verify")
                                 new firmware boots:
                                   Wi-Fi OK + display refresh OK -> mark valid
                                   crash / reset before that     -> bootloader rolls back to the old slot
```

Details:

- **Stable URL, no API parsing:** `https://github.com/<owner>/<repo>/releases/latest/download/manifest.json`
  always points to the newest *non-prerelease*, has no API rate limit and is a tiny file.
- **Version number:** the integer `FW_BUILD`; `FW_VERSION` (".76") follows from it.
- **Board variant:** each build bakes in its variant name (e.g. `esp32c6_firebeetle`) and only accepts
  the matching file, so a classic-ESP32 image can never land on a C6.
- **Redirects:** GitHub redirects downloads to `*.githubusercontent.com`. The core forgets the
  certificate bundle when it reconnects for a redirect to another host, so `UpdateClient.cpp` attaches
  it again on every connect (and only allows github.com / *.githubusercontent.com).
- **Auto-install vs. notify only:** setup app switch **"Install updates by themselves"** (per device,
  default on). With it off, the daily check only shows an **"Update available"** mark in the status
  bar; installing then takes a double tap + confirm tap (or **Check for updates now** in the app).
- **Battery guard:** below 30 % battery no update is installed unless a computer's USB is connected.
  The screen says "Battery too low for an update: please charge". Without a measured battery (below
  3.0 V: USB power) updates are allowed.
- **Visible on the display:** info screen (version, channel, auto install, last update, battery,
  Wi-Fi) → "New version .77" with up to 3 "What's new" lines from the release notes → "Tap to install
  - cancels in 15 s" → progress steps (Downloading %, Verifying, Installing, Restarting) → after the
  restart "Updated to .77" with a check mark once. Failures in plain language ("Update failed:
  previous version kept").

## What keeps it safe

| Failure | What happens |
|---|---|
| Power loss / Wi-Fi drop during download | The old slot is untouched and stays active. Retry with the next daily check |
| Corrupt download | SHA-256 / size mismatch -> not activated |
| New firmware crashes on boot | Rollback to the previous slot (bootloader) |
| New firmware runs but its update code is broken | **Can't fix itself remotely.** Mitigation: keep the update code small and stable, run it early in the wake cycle, and always test on my own device (beta channel) first |
| Someone else publishes a release / GitHub account compromised | Device accepts only manifests signed with the private key (a GitHub Actions secret plus an offline backup, **never in the repo or build artifacts**). The manifest pins the image SHA-256, so the image is covered too. Limit: anyone with write access to the repository can run the release job, which uses the secret: keep 2FA on (optionally a GitHub environment with a required reviewer for the release job) |
| Downgrade to an old, vulnerable build | Rejected: build number must increase; `min_build` in the manifest |
| Private repo | Not possible without putting a GitHub token on every device. **The repo must stay public** (firmware contains no secrets; user settings stay on the device) |
