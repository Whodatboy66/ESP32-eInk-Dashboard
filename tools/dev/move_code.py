#!/usr/bin/env python3
"""Moves a block of source verbatim from one file to another (refactoring aid).

  tools/dev/move_code.py SRC START END DEST

The block starts at the first line of SRC that starts with START and ends before the next line
(after it) that starts with END (END "EOF" = to the end). It is removed from SRC and appended to
DEST (created if missing). Prints the moved line count.
"""
import sys
from pathlib import Path

src, start, end, dest = sys.argv[1:5]
lines = Path(src).read_text().splitlines(keepends=True)
try:
    a = next(i for i, l in enumerate(lines) if l.startswith(start))
except StopIteration:
    sys.exit(f"start not found: {start!r}")
if end == "EOF":
    b = len(lines)
else:
    try:
        b = next(i for i in range(a + 1, len(lines)) if lines[i].startswith(end))
    except StopIteration:
        sys.exit(f"end not found after start: {end!r}")
block = lines[a:b]
Path(src).write_text("".join(lines[:a] + lines[b:]))
d = Path(dest)
d.parent.mkdir(parents=True, exist_ok=True)
prev = d.read_text() if d.exists() else ""
d.write_text(prev + ("" if prev.endswith("\n") or not prev else "\n") + "".join(block))
print(f"{len(block)} lines {src} -> {dest}")
