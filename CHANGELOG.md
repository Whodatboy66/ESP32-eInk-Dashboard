# Changelog

One line per change that reaches the devices or the build. Stages: [docs/IMPLEMENTATION_PLAN.md](docs/IMPLEMENTATION_PLAN.md).
Versions: `FW_VERSION` in [configure.h](configure.h) (an integer `FW_BUILD` follows in stage 1).

| Version | Stage | Change |
|---|---|---|
| .63 + fork | 0 | `sketch.yaml` profiles fixed; `tools/epd_test`; docs |
| .63 + fork | 0 | Zigbee removed; CI builds the `sketch.yaml` profiles on every push with a size check and host tests; button: pull-down, fault limit, setup on release, BOOT backup |
| .63 + fork | 0 | Repository layout: short README, user manual in `docs/USER_GUIDE.md`, screenshots in `docs/images/`, font converter in `tools/`, this changelog |
