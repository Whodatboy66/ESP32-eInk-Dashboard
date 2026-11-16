#!/usr/bin/env python3
"""Writes manifest.json for an over-the-air release (format: src/update/Manifest.h).

Used by the CI release job. Each build artifact folder holds a variant.json next to the app image:
  {"variant": "<sketch.yaml profile>", "file": "<app .bin name>", "display": "bw750",
   "config_schema": 1, "build": 66}
The build job reads variant, display, config_schema and build from the OTA-ID string compiled
into the image (src/update/UpdateClient.cpp), so the manifest matches what was built.
The manifest is signed afterwards (manifest.sig, see docs/OTA_UPDATES.md).

Example:
  tools/ota/make_manifest.py --build 65 --version .65 --tag firmware-.65 \
      --notes "Faster calendar|Fixes the weather icons" --dir firmware --out manifest.json
"""
import argparse
import hashlib
import json
import pathlib
import re
import sys

SAFE_NAME = re.compile(r"^[A-Za-z0-9_-][A-Za-z0-9._-]{0,62}$")  # same rule as ota::isSafeName
MAX_NOTES = 3
NOTE_MAX_CHARS = 70
# Characters of the e-paper font (fonts/epaperFont.h), plus space: anything else shows as a gap.
FONT_CHARS = set(" #%*+,-./0123456789:=?ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz~°³µÀÇÈÉàçèé")


def fail(msg):
    print(f"make_manifest: {msg}", file=sys.stderr)
    sys.exit(1)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--build", type=int, required=True, help="FW_BUILD of the release")
    p.add_argument("--version", required=True, help='FW_VERSION, e.g. ".65"')
    p.add_argument("--tag", required=True, help="release tag")
    p.add_argument("--min-build", type=int, default=0, help="oldest build allowed to install it over the air")
    p.add_argument("--notes", default="", help="What's new: up to 3 lines separated by |")
    p.add_argument("--dir", required=True, help="folder with the build artifacts")
    p.add_argument("--out", required=True, help="manifest.json to write")
    p.add_argument("--notes-out", help="also write the GitHub release notes (Markdown) here")
    p.add_argument("--channel", choices=["stable", "beta"], default="stable", help="for the release notes")
    args = p.parse_args()

    if not SAFE_NAME.match(args.tag):
        fail(f"unsafe tag {args.tag!r}")

    notes = [n.strip() for n in args.notes.split("|") if n.strip()]
    if len(notes) > MAX_NOTES:
        fail(f"at most {MAX_NOTES} What's new lines, got {len(notes)}")
    for n in notes:
        if len(n) > NOTE_MAX_CHARS:
            fail(f"What's new line longer than {NOTE_MAX_CHARS} characters: {n!r}")
        bad = sorted(set(n) - FONT_CHARS)
        if bad:
            fail(f"What's new line {n!r} uses characters the e-paper font doesn't have: "
                 f"{' '.join(bad)} (allowed: letters, digits, space and #%*+,-./:=?_~)")

    files = {}
    for vj in sorted(pathlib.Path(args.dir).rglob("variant.json")):
        info = json.loads(vj.read_text())
        variant, name = info["variant"], info["file"]
        display, schema, build = info.get("display", ""), info.get("config_schema"), info.get("build")
        if not SAFE_NAME.match(name):
            fail(f"unsafe file name {name!r}")
        if not SAFE_NAME.match(display) or len(display) > 15:
            fail(f"{variant}: bad display id {display!r}")
        if not isinstance(schema, int) or schema < 0:
            fail(f"{variant}: bad config_schema {schema!r}")
        if build != args.build:
            fail(f"{variant}: image is build {build!r}, release is build {args.build}")
        if variant in files:
            fail(f"two builds for variant {variant!r}")
        data = (vj.parent / name).read_bytes()
        files[variant] = {"name": name, "size": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                          "display": display, "config_schema": schema}
    if not files:
        fail(f"no variant.json found under {args.dir}")

    manifest = {
        "schema": 1,
        "build": args.build,
        "version": args.version,
        "min_build": args.min_build,
        "tag": args.tag,
        "notes": notes,
        "files": files,
    }
    pathlib.Path(args.out).write_text(json.dumps(manifest, indent=1, sort_keys=True) + "\n")
    for variant, f in sorted(files.items()):
        print(f"{variant}: {f['name']} {f['size']} bytes sha256 {f['sha256']}")

    if args.notes_out:
        lines = [f"Firmware {args.version} (build {args.build}), {args.channel} channel"
                 + (": only devices set to beta install it." if args.channel == "beta" else "."), ""]
        if notes:
            lines += ["What's new:"] + [f"- {n}" for n in notes] + [""]
        lines += [
            'Devices with "Install updates automatically" on install it at night; the others show',
            '"Update available" and install it after a double tap (or "Check for updates now").',
            "",
            "First install over USB (devices before build 64): `arduino-cli upload`, or esptool with the",
            "separate bootloader, partitions, boot_app0 and app files, keeps the settings. The",
            "`*.merged.bin` image also works but erases all settings (the device starts in setup mode).",
        ]
        pathlib.Path(args.notes_out).write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
