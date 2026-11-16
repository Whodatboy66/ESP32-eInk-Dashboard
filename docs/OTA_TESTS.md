# OTA tests on the device

How to check that broken updates are rejected or rolled back (stage 1 "Done when" in
[IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md)). The broken updates go out on the **beta channel**
only, through the manual workflow **"OTA test release"** ([ota-test.yml](../.github/workflows/ota-test.yml)).
Devices on the stable channel never see them.

## Before you start

1. The secret `OTA_SIGNING_KEY` is set ([OTA_UPDATES.md](OTA_UPDATES.md#signing)).
2. Build 66 (or later) is **released on stable** and installed on the test device (FireBeetle C6).
   Older builds can't read the beta channel or check signatures.
3. On the device's setup page: **Update channel = Beta**, **Install updates automatically = off**
   (so nothing installs at night by itself while a test is published). Save.
4. Optional: the serial monitor on USB shows the `[OTA]` log lines.

Each test: GitHub → Actions → **OTA test release** → Run workflow on `main` → pick the scenario. When the
run is green, **double tap** on the device (or press **Check for updates now** on the setup page, which
installs without the confirm tap). Test builds are numbered 90001-90005, so they always count as newer.

## Tests

| # | Scenario | What the device must do | Screen (and setup page "Last check") |
|---|---|---|---|
| 1 | `wrong-board` | Refuses before downloading: the release has no file for this board | "No update for this device", "Release .90001 has no file for esp32c6_firebeetle" |
| 2 | `bad-checksum` | Shows "New version .90002"; after the confirm tap it downloads the image, finds the SHA-256 wrong and keeps the old version (nothing is activated) | Downloading 25 % … 100 %, then "Update failed: previous version kept", "file damaged - checksum wrong" |
| 3 | `unsigned` | Refuses the manifest (no `manifest.sig`): nothing in it is used | "Update check failed: try again later", "Release signature not valid: not installed" |
| 4 | `crash` | Installs .90004 and restarts; the test build crashes right away (log: `[OTA test] crashing on purpose`); the bootloader goes back to the old version, which reports it | "Updated to" never appears; after the restart "Update failed: previous version kept", "The new version did not start correctly." The status bar still shows the old version |
| 5 | `hang` (extra) | Installs .90005; the test build hangs after "Updated to .90005"; after 3 minutes the board restarts and rolls back. **Don't touch the button meanwhile**: using it confirms the new version | "Updated to .90005" for 3 min, then "Update failed: previous version kept" and the old version |
| 6 | `restore` | Puts the beta channel back to the latest regular release | Double tap: "Up to date: .66" (the "Update available" mark disappears) |

After tests 4 and 5 the failed build is remembered: the night-time check won't auto-install it again
(it only shows "Update available"). That only applies to that exact build number.

**Always finish with `restore`**, then set the channel back to **Stable** and auto install back **on**
if that's how the device should run.

## Results

Add each test to the hardware test log in [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md#hardware-test-log)
(date, build, scenario, what the screen showed).
