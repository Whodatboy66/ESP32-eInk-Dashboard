# Self-updating devices (OTA from GitHub): feasibility analysis

**Question:** can I push to GitHub so that devices (e.g. at my parents') update themselves,
with no USB flashing?

**Answer: yes, it's very feasible.** Everything needed is in the ESP32 Arduino core we already use
(3.3.7). The one hard requirement is a **one-time USB flash per device** to switch to an OTA-capable
partition table. After that, every update can arrive over Wi-Fi.

## What the device needs

| Building block | Available? | Notes |
|---|---|---|
| Write new firmware while running | Yes | `Update` / `HTTPUpdate` libraries in the core |
| Two app slots (A/B) | **No, not yet** | `partitions.csv` has one `factory` app slot and no `otadata`. Needs a new table, flashed once over USB |
| Automatic rollback if new firmware fails | Yes | `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=1` in the C6 core; the core has `verifyRollbackLater()` / `esp_ota_mark_app_valid_cancel_rollback()` hooks |
| Verified HTTPS to GitHub | Yes | `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=1`, `NetworkClientSecure::setCACertBundle()`. The project currently uses `setInsecure()` everywhere and should change that for updates at least |
| Follow GitHub's download redirect | Yes | `HTTPClient::setFollowRedirects()` (already used in `directFetch.cpp`) |
| Keep settings across updates | Yes | Settings live in NVS (`0x9000`); OTA only writes the inactive app slot |

## Partition table (4 MB flash)

Keep `nvs` at the same offset, so the one-time USB flash **keeps existing settings**. Zigbee partitions
are dropped (Zigbee is a non-goal in the spec); a small LittleFS area holds tasks/notes/layout (spec §7.4):

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
| esp32c6_firebeetle_zigbee (not planned) | 1,757,432 B | 89 % |

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
- **When to check:** once a day plus on demand (button hold or setup page "Check for update"). An
  update costs about 1.4 MB of download (10-20 s of Wi-Fi), which is negligible for the battery when it
  happens only on new releases.
- **Battery guard:** skip updates below about 30 % battery (the firmware already measures the voltage).
- **Visible on the display:** briefly show "Updating to .64..." and the current version in the status bar,
  so parents can see what happened.

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

The existing `.github/workflows/compile.yml` already builds every variant and creates a release
named `firmware-<FW_VERSION>`. Changes needed:

1. Build the **app-only `.bin`** (not just `merged.bin`) with the new OTA partition table.
2. Compute SHA-256 and size, generate `manifest.json`, sign it (`manifest.sig`) with the secret key; attach both.
   Publish only on manual trigger; pre-release = beta channel.
3. Align core and library versions with `sketch.yaml`, so OTA builds match local builds.
4. Enable Actions on the fork (off by default for forks), and trigger it on a tag or manually.

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
