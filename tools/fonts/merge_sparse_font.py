#!/usr/bin/env python3
"""Merge the glyphs of two sparse font headers (src/ui/fonts/, made by tools/ListFontConvertV4.py).

Glyphs of the base font are kept unchanged (so text already on the screen stays pixel-identical);
glyphs only in the extra font are added. The result is sorted by code point (findGlyph() searches
binary) and written with the base font's name.

  python3 tools/fonts/merge_sparse_font.py src/ui/fonts/epaperFont.h extra.h src/ui/fonts/epaperFont.h

See src/ui/fonts/README.md for how the extra glyphs of epaperFont were made.
"""
import re
import sys


def load(path):
    text = open(path, encoding="utf-8").read()
    name = re.search(r"const SparseGFXfont (\w+) PROGMEM", text).group(1)
    bitmaps = [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]{2}\b", text.split("_Bitmaps[]")[1].split("};")[0])]
    charmap = [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]+", text.split("_CharMap[]")[1].split("};")[0])]
    glyphs = [tuple(map(int, g)) for g in re.findall(
        r"\{ *(-?\d+), *(-?\d+), *(-?\d+), *(-?\d+), *(-?\d+), *(-?\d+) *\}", text.split("_Glyphs[]")[1].split("};")[0])]
    tail = text.split("const SparseGFXfont")[1]
    nums = re.findall(r"^\s*(\d+),?\s*$", tail, re.M)
    y_advance = int(nums[-1])
    if len(charmap) != len(glyphs):
        sys.exit(f"{path}: {len(charmap)} code points but {len(glyphs)} glyphs")
    out = {}
    for i, (code, g) in enumerate(zip(charmap, glyphs)):
        start = g[0]
        end = glyphs[i + 1][0] if i + 1 < len(glyphs) else len(bitmaps)
        out[code] = (g[1:], bitmaps[start:end])
    return name, out, y_advance


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    name, base, y_adv = load(sys.argv[1])
    _, extra, _ = load(sys.argv[2])
    merged = dict(extra)
    merged.update(base)  # base glyphs win
    added = sorted(set(merged) - set(base))

    bitmaps, glyphs = [], []
    codes = sorted(merged)
    for c in codes:
        metrics, bits = merged[c]
        glyphs.append((len(bitmaps),) + metrics)
        bitmaps.extend(bits)

    lines = ["#pragma once", "", "#include <Adafruit_GFX.h>", "",
             f"// Base glyphs from the original conversion; added by tools/fonts/merge_sparse_font.py: "
             + " ".join(f"U+{c:04X}" for c in added), "",
             f"const uint8_t {name}_Bitmaps[] PROGMEM = {{"]
    for i in range(0, len(bitmaps), 12):
        lines.append(",".join(f"0x{b:02X}" for b in bitmaps[i:i + 12]) + ",")
    lines += ["};", "", f"const uint32_t {name}_CharMap[] PROGMEM = {{",
              ",".join(f"0x{c:02X}" for c in codes) + ",", "};", "",
              f"const GFXglyph {name}_Glyphs[] PROGMEM = {{"]
    lines += [f"{{ {g[0]}, {g[1]}, {g[2]}, {g[3]}, {g[4]}, {g[5]} }}," for g in glyphs]
    lines += ["};", "", f"const SparseGFXfont {name} PROGMEM = {{",
              f"  {name}_Bitmaps,", f"  {name}_Glyphs,", f"  {name}_CharMap,",
              f"  {len(codes)},", f"  {y_adv}", "};", ""]
    open(sys.argv[3], "w", encoding="utf-8").write("\n".join(lines))
    print(f"{name}: {len(base)} kept, {len(added)} added, {len(bitmaps)} bitmap bytes")


if __name__ == "__main__":
    main()
