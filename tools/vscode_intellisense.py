#!/usr/bin/env python3
"""Generate .vscode/c_cpp_properties.json for the VS Code C/C++ extension (IntelliSense).

Runs arduino-cli to produce a compilation database for one sketch.yaml profile, expands the
ESP32 core's @response files / -iprefix includes, and writes include paths, defines and the
compiler path, so headers like HTTPClient.h resolve in the editor.

Usage (from the repo root, after the profile has been built once):
    python3 tools/vscode_intellisense.py [profile]        # default: esp32c6_firebeetle
Re-run after changing the core/library versions in sketch.yaml.
"""
import json
import os
import shlex
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROFILE = sys.argv[1] if len(sys.argv) > 1 else "esp32c6_firebeetle"
BUILD = os.path.join(ROOT, "build", "intellisense")


def read_response_file(path):
    with open(path, encoding="utf-8") as f:
        return shlex.split(f.read(), posix=(os.name != "nt"))


def expand(args):
    out = []
    for a in args:
        if a.startswith("@") and os.path.isfile(a[1:]):
            out.extend(expand(read_response_file(a[1:])))
        else:
            out.append(a)
    return out


subprocess.run(
    ["arduino-cli", "compile", "--profile", PROFILE, "--only-compilation-database",
     "--build-path", BUILD, ROOT],
    check=True,
)
with open(os.path.join(BUILD, "compile_commands.json"), encoding="utf-8") as f:
    db = json.load(f)

# A C++ file from the sketch carries all library include paths used by the sketch.
entry = next(e for e in db if os.path.basename(e["file"]) == "ESP32-eInk-Dashboard.ino.cpp")
args = expand(entry["arguments"])

includes, defines, std, prefix = [], [], "gnu++17", ""
i = 0
while i < len(args):
    a = args[i]
    if a == "-iprefix":
        prefix = args[i + 1]; i += 2; continue
    if a == "-iwithprefixbefore":
        includes.append(prefix + args[i + 1]); i += 2; continue
    if a.startswith("-iwithprefixbefore"):
        includes.append(prefix + a[len("-iwithprefixbefore"):])
    elif a.startswith("-I"):
        p = a[2:]
        if os.path.join("build", "intellisense") not in p:
            includes.append(p)
    elif a.startswith("-D"):
        defines.append(a[2:])
    elif a.startswith("-std="):
        std = a[5:]
    i += 1

config = {
    "configurations": [{
        "name": f"Arduino {PROFILE}",
        "compilerPath": args[0],
        "cppStandard": std,
        "intelliSenseMode": "gcc-x86",
        "includePath": ["${workspaceFolder}/**"] + list(dict.fromkeys(includes)),
        "defines": list(dict.fromkeys(defines)),
        "browse": {"limitSymbolsToIncludedHeaders": True},
    }],
    "version": 4,
}
os.makedirs(os.path.join(ROOT, ".vscode"), exist_ok=True)
with open(os.path.join(ROOT, ".vscode", "c_cpp_properties.json"), "w", encoding="utf-8") as f:
    json.dump(config, f, indent=2)

settings_path = os.path.join(ROOT, ".vscode", "settings.json")
settings = {}
if os.path.isfile(settings_path):
    with open(settings_path, encoding="utf-8") as f:
        settings = json.load(f)
settings.setdefault("files.associations", {})["*.ino"] = "cpp"
with open(settings_path, "w", encoding="utf-8") as f:
    json.dump(settings, f, indent=2)

print(f"Wrote .vscode/c_cpp_properties.json ({len(config['configurations'][0]['includePath'])} include paths, "
      f"{len(config['configurations'][0]['defines'])} defines) for profile {PROFILE}")
