# OTA tests on the device

How to check that broken updates are rejected or rolled back (stage 1 "Done when" in
[IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md)). The broken updates go out on the **beta channel**
only, through the manual workflow **"OTA test release"** ([ota-test.yml](../.github/workflows/ota-test.yml)).
Devices on the stable channel never see them.

## Before you start

1. The secret `OTA_SIGNING_KEY` is set ([OTA_UPDATES.md](OTA_UPDATES.md#signing)).
2. Build 66 (or later) is **released on stable** and installed on the test device (FireBeetle C6).
   Older builds can't read the beta channel or check signatures.
3. In the setup app, Device tab → Updates: **Update channel = Beta**, **Install updates by themselves = off**
   (so nothing installs at night by itself while a test is published). Save.
4. Optional: the serial monitor on USB shows the `[OTA]` log lines.

Each test: GitHub → Actions → **OTA test release** → Run workflow on `main` → pick the scenario. When the
run is green, **double tap** on the device (or press **Check for updates now** in the setup app, Device
tab, which installs without the confirm tap). Test builds are numbered 90001-90005, so they always count as newer.
The other way round, a device running a test build takes **any** regular release (from build 69 on;
see [Stuck on a test build](#stuck-on-a-test-build)).

## Tests

| # | Scenario | What the device must do | Screen (and the last result in the setup app: Device tab "Installed", Home tab "Last update") |
|---|---|---|---|
| 1 | `wrong-board` | Refuses before downloading: the release has no file for this board | "No update for this device", "Release .90001 has no file for esp32c6_firebeetle" |
| 2 | `bad-checksum` | Shows "New version .90002"; after the confirm tap it downloads the image, finds the SHA-256 wrong and keeps the old version (nothing is activated) | Downloading 25 % … 100 %, then "Update failed: previous version kept", "file damaged - checksum wrong" |
| 3 | `unsigned` | Refuses the manifest (no `manifest.sig`): nothing in it is used | "Update check failed: try again later", "Release signature not valid: not installed" |
| 4 | `crash` | Installs .90004 and restarts; the test build crashes right away (log: `[OTA test] crashing on purpose`); the bootloader goes back to the old version, which reports it | "Updated to" never appears; after the restart "Update failed: previous version kept", "The new version did not start correctly." The status bar still shows the old version |
| 5 | `hang` (extra) | Installs .90005; the test build hangs after "Updated to .90005"; after 3 minutes the board restarts and rolls back. Test builds from build 69 on can't be confirmed by the button, setup mode or edit mode, so a hold restarts and rolls back right away; older test builds (before 69) were kept by a hold | "Updated to .90005" for 3 min, then "Update failed: previous version kept" and the old version |
| 6 | `restore` | Puts the beta channel back to the latest regular release | Double tap: "Up to date: " with the version the device runs (the "Update available" mark disappears) |

After tests 4 and 5 the failed build is remembered: the night-time check won't auto-install it again
(it only shows "Update available"). That only applies to that exact build number.

**Always finish with `restore`**, then set the channel back to **Stable** and auto install back **on**
if that's how the device should run.

`restore` only changes what the beta channel offers; it never sends anything to the device by itself.
A device that rolled back runs its old version anyway. A device that still runs a test build gets the
restored release with the next double tap (or night check), if the test build is from build 69 or later.

## Stuck on a test build

If the status bar shows `.9000x` after the tests, the test build was kept (before build 69 a button
hold during the hang test confirmed it). Test builds made from build 69 on treat any regular release as
newer, so a double tap after `restore` brings the device back. A test build made **before** 69 (the
`.90005` of 2026-11-16) only knows the "never a downgrade" rule and calls every release "Up to date":
flash a current build once over USB (settings are kept):

```
arduino-cli compile --profile esp32c6_firebeetle -u -p <port> .
```

## Results

Add each test to the results log in [DEVICE_TESTS.md](DEVICE_TESTS.md#results-log)
(date, build, scenario, what the screen showed).
