# Changelog

One line per change that reaches the devices or the build. Stages: [docs/IMPLEMENTATION_PLAN.md](docs/IMPLEMENTATION_PLAN.md).
Versions: `FW_VERSION` in [configure.h](configure.h) (an integer `FW_BUILD` follows in stage 1).

| Version | Stage | Change |
|---|---|---|
| .63 + fork | 0 | `sketch.yaml` profiles fixed; `tools/epd_test`; docs |
| .63 + fork | 0 | Zigbee removed; CI builds the `sketch.yaml` profiles on every push with a size check and host tests; button: pull-down, fault limit, setup on release, BOOT backup |
| .63 + fork | 0 | Repository layout: short README, user manual in `docs/USER_GUIDE.md`, screenshots in `docs/images/`, font converter in `tools/`, this changelog |
| .64 | 1 (part 1) | Over-the-air updates from GitHub releases: A/B partition table (one USB flash needed), `FW_BUILD`, update check at night with "Install updates automatically" (or "Update available" in the status bar), double tap = info and update screen with confirm tap, setup page "Check for updates now", battery guard (30 % or USB), SHA-256 check, rollback, update screens; CI release job with `manifest.json`. Not signed yet |
| .66 | 1 | Signed release manifests (ECDSA P-256, secret `OTA_SIGNING_KEY`; unsigned or altered releases are rejected), stable/beta update channel (setup page), display id and settings-schema version checked before an install; CI reads them out of the image. Must be released on stable (64/65 devices don't know channels). Timezone: names like `CET` / `Europe/Berlin` become a POSIX rule (a bare `CET` was UTC, 1 h off), invalid ones are refused. Manual workflow "OTA test release" for broken-update tests on the beta channel |
