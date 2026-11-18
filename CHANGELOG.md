# Changelog

One line per change that reaches the devices or the build. Stages: [docs/IMPLEMENTATION_PLAN.md](docs/IMPLEMENTATION_PLAN.md).
Versions: `FW_VERSION` in [configure.h](configure.h) (an integer `FW_BUILD` follows in stage 1).

| Version | Stage | Change |
|---|---|---|
| .63 + fork | 0 | `sketch.yaml` profiles fixed; `tools/epd_test`; docs |
| .63 + fork | 0 | Zigbee removed; CI builds the `sketch.yaml` profiles on every push with a size check and host tests; button: pull-down, fault limit, setup on release, BOOT backup |
| .63 + fork | 0 | Repository layout: short README, user manual in `docs/USER_GUIDE.md`, screenshots in `docs/images/`, font converter in `tools/`, this changelog |
| .64 | 1 (part 1) | Over-the-air updates from GitHub releases: A/B partition table (one USB flash needed), `FW_BUILD`, update check at night with "Install updates automatically" (or "Update available" in the status bar), double tap = info and update screen with confirm tap, setup page "Check for updates now", battery guard (30 % or USB), SHA-256 check, rollback, update screens; CI release job with `manifest.json`. Not signed yet |
| .66 | 1 | Signed release manifests (ECDSA P-256, secret `OTA_SIGNING_KEY`; unsigned or altered releases are rejected), stable/beta update channel (setup page), display id and settings-schema version checked before an install; CI reads them out of the image. Must be released on stable (64/65 devices don't know channels). Timezone: names like `CET` / `Europe/Berlin` become a POSIX rule (a bare `CET` was UTC, 1 h off), invalid ones are refused. Manual workflow "OTA test release" for broken-update tests on the beta channel. Setup page: the saved Wi-Fi network is pre-selected (saving the page used to switch to the first scanned network and lose Wi-Fi) |
| .67 | 2 | Firmware split into modules (`src/`); every HTTPS request checks the server certificate (Bambu LAN excepted, local addresses only); secrets masked in the log; setup page checks input before saving (says what to fix), turns `webcal://` and `http://` calendar links into `https://`, accepts printer names that resolve to a local address; factory reset (setup page, or `factory-reset` on the serial console); fonts stored once (17 KB smaller). No visible change on the screen |
| .68 | 3 | Layouts: Classic (unchanged), Planner, Minimal, Technical, chosen on the setup page together with 12/24 h and which widgets are shown; switched-off widgets leave no gap; widgets adapt to their size and are clipped to their place; a failed download is retried after 2, 4, 8 and 16 minutes instead of waiting a full interval, and data that is too old gets an "old" badge. Settings schema 2 |
| .69 | 1 (fix) | OTA test builds: a device left on a test build (90001+) takes any regular release again (it called every release "Up to date"), and a test build is never confirmed by the button, setup mode or the update screen, so the hang / crash tests always roll back |
