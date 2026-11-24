#!/usr/bin/env python3
"""How much of the code still comes from the original project (VoIPshare/ESP32-eInk-Dashboard, up to
commit 8d0a66b): git blame with move and copy detection over the tracked source files.

Usage: python3 tools/dev/origin_report.py [--all]   (--all: every file, not only those with lines left)
Counts substantive lines only (see substantive()). Stage 7b (docs/plans/STAGE7B_OWN_CODE.md) brings
the code part to 0, apart from facts that are written the same way (pin numbers, partition offsets).
"""
import subprocess
import sys

ORIGIN = "8d0a66b"
EXT = (".cpp", ".h", ".ino", ".c", ".py", ".js", ".yml", ".yaml", ".csv")


def substantive(text):
    """Lines that carry code: not blank, not a lone brace, not an #include, at least 12 characters.
    (git's copy detection matches such trivial lines against any identical line of the original.)"""
    t = text.strip()
    if len(t) < 12 or t.startswith("#include") or t.startswith("#pragma"):
        return False
    return any(c.isalnum() for c in t)


def main():
    show_all = "--all" in sys.argv
    up = set(subprocess.run(["git", "rev-list", ORIGIN], capture_output=True, text=True, check=True).stdout.split())
    files = subprocess.run(["git", "ls-files"], capture_output=True, text=True, check=True).stdout.split()
    files = [f for f in files if f.endswith(EXT) and not f.startswith("src/web/generated")]
    rows, total, origin = [], {"code": 0, "fonts": 0}, {"code": 0, "fonts": 0}
    for f in files:
        out = subprocess.run(["git", "blame", "-M", "-C", "-C", "--line-porcelain", "--", f],
                             capture_output=True, text=True, errors="replace").stdout
        n = u = 0
        commit = None
        for line in out.split("\n"):
            head = line[:40]
            if len(line) > 40 and line[40] == " " and all(c in "0123456789abcdef" for c in head):
                commit = head
            elif line.startswith("\t") and commit:
                if substantive(line[1:]):
                    n += 1
                    u += commit in up
                commit = None
        kind = "fonts" if "/fonts/" in f and f.endswith(".h") else "code"
        total[kind] += n
        origin[kind] += u
        if u or show_all:
            rows.append((u, n, f))
    for u, n, f in sorted(rows, reverse=True):
        print("%5d / %5d  %3d %%  %s" % (u, n, 100 * u // max(n, 1), f))
    for kind in ("code", "fonts"):
        print("%s: %d of %d lines from the original (%d %%)" % (kind, origin[kind], total[kind],
                                                              100 * origin[kind] // max(total[kind], 1)))


if __name__ == "__main__":
    main()
