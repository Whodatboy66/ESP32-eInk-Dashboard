# Implementation plan (staged upgrades)

Status: **draft.** The foundation stages (0-1) are defined. Feature stages 2+ will be derived from the
dev specification (Google Doc "implementation plan / dev specifications") once it's available in the repo.

Each stage is built, flashed and checked on my own device before the next one starts. Each stage
ends with a version bump (`FW_VERSION` / `FW_BUILD`) and a short changelog entry below.

## Stage 0: Stable base

Goal: the dashboard runs reliably on the target hardware without workarounds.

- [ ] Button GPIO2: switch to `INPUT_PULLDOWN` (active-HIGH button / TTP223), with matching pull-down
      during deep sleep for the EXT1 wake. An unconnected pin must read "not pressed" (today: endless setup mode).
- [ ] Check that the FireBeetle preset boots straight into the dashboard after setup, with no button connected.
- [ ] Optional: build option `KEEP_AWAKE_ON_USB` for debugging (no deep sleep while USB is connected).

Done when: 24 h run without a USB connection, clock updates every minute, full refresh works.

## Stage 1: Over-the-air updates

Goal: pushing a release to GitHub updates all devices (details: [OTA_UPDATES.md](OTA_UPDATES.md)).

- [ ] OTA partition table (2 x 1.875 MB app slots, NVS offset unchanged so settings survive).
- [ ] `FW_BUILD` integer and board-variant name baked into each build.
- [ ] Update client: daily check of `releases/latest/download/manifest.json`, HTTPS with certificate
      bundle, SHA-256 check, battery guard, "Updating..." message on the display.
- [ ] Rollback: mark the new firmware valid only after Wi-Fi + display refresh succeed.
- [ ] CI: app binaries + `manifest.json` per release; versions aligned with `sketch.yaml`; Actions enabled on the fork.
- [ ] Channels: stable (parents) / beta (pre-releases, my device). Setting on the setup page.
- [ ] Setup page / status bar: show the current version, plus a "Check for update now" action.

Done when: a test release (.64 to .65) installs itself on my device; a deliberately broken release rolls back.

**Before devices leave the house: flash every device once over USB with the stage 1 firmware.**

## Stage 2+: Features from the specification

Pending the specification document. Planned structure per stage:

- goal (one sentence, user-visible)
- tasks (checklist)
- affected files / widgets
- memory and flash budget check (the Zigbee build is at 89 % of an OTA slot)
- acceptance test

## Changelog

| Version | Stage | Change |
|---|---|---|
| .63 + fork fixes | - | `sketch.yaml` profiles fixed, display test sketch `tools/epd_test` |
