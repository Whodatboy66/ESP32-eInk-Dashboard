#!/usr/bin/env python3
"""Checks that functions moved during a refactor are unchanged (stage 2: "move code, don't rewrite").

  tools/dev/check_moves.py <base-commit>

Extracts every top-level function from the C/C++ sources of <base-commit> (sketch, *.cpp at the
root) and from the working tree (sketch, root, src/), then compares functions by name after
normalizing whitespace and the allowed mechanical edits (see ALLOWED). Prints:
  same     - identical (or identical after the allowed edits)
  CHANGED  - body differs: review the printed diff
  gone     - not found any more (deleted or renamed): review
Exit code 0 always; this is a review aid, not a gate.
"""
import difflib
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# Mechanical edits that don't change behavior; applied to both sides before comparing.
ALLOWED = [
    (re.compile(r"\bdisplay\s*\.\s*(?=(draw|fill|print|setCursor|setText|width|height|getTextBounds|setFont))"), "canvas()."),
    (re.compile(r"\bstatic\s+"), ""),
    (re.compile(r"\binline\s+"), ""),
]

HEADER = re.compile(r"^(?!\s)(?!#)(?!//)(?!typedef)(?!struct\b)(?!class\b)(?!enum\b)(?!namespace\b)"
                    r"[A-Za-z_][\w:<>,\*&\s]*?\b([A-Za-z_]\w*)\s*\([^;{]*\)\s*(const\s*)?(override\s*)?(\{)?\s*$")


def strip_comment(line):
    """Removes a // comment, but not // inside a string literal (URLs)."""
    out, in_str, esc = [], False, False
    for i, ch in enumerate(line):
        if in_str:
            out.append(ch)
            if esc:
                esc = False
            elif ch == "\\":
                esc = True
            elif ch == '"':
                in_str = False
        elif ch == '"':
            in_str = True
            out.append(ch)
        elif line.startswith("//", i):
            break
        else:
            out.append(ch)
    return "".join(out)


def functions(text):
    """Yields (name, body) for functions whose header starts at column 0 and ends with '{'."""
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        m = HEADER.match(lines[i])
        if not m or m.group(1) in ("if", "for", "while", "switch"):
            i += 1
            continue
        # Header with the brace on the next line ("bool fetchBambu(...)\n{").
        if not m.group(4) and not (i + 1 < len(lines) and lines[i + 1].strip() == "{"):
            i += 1
            continue
        depth, j, body, opened = 0, i, [], False
        while j < len(lines):
            line = re.sub(r'"(\\.|[^"\\])*"', '""', lines[j])       # ignore braces in strings
            line = re.sub(r"'(\\.|[^'\\])'", "''", line)
            line = strip_comment(line)
            depth += line.count("{") - line.count("}")
            opened = opened or "{" in line
            body.append(lines[j])
            j += 1
            if opened and depth <= 0:
                break
        yield m.group(1), "\n".join(body)
        i = j


def normalize(body):
    for rx, rep in ALLOWED:
        body = rx.sub(rep, body)
    body = "\n".join(strip_comment(l) for l in body.splitlines())
    return re.sub(r"\s+", " ", body).strip()


def sources_at(commit):
    names = subprocess.run(["git", "ls-tree", "-r", "--name-only", commit], cwd=ROOT,
                           capture_output=True, text=True, check=True).stdout.split()
    for n in names:
        if n.endswith((".ino", ".cpp")) and ("/" not in n or n.startswith("src/")):
            yield n, subprocess.run(["git", "show", f"{commit}:{n}"], cwd=ROOT,
                                    capture_output=True, text=True, check=True).stdout


def sources_now():
    for p in list(ROOT.glob("*.ino")) + list(ROOT.glob("*.cpp")) + list((ROOT / "src").rglob("*.cpp")) \
            + list((ROOT / "src").rglob("*.h")):
        yield str(p.relative_to(ROOT)), p.read_text(errors="replace")


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(2)
    base = {}
    for f, text in sources_at(sys.argv[1]):
        for name, body in functions(text):
            base.setdefault(name, []).append((f, body))
    now = {}
    for f, text in sources_now():
        for name, body in functions(text):
            now.setdefault(name, []).append((f, body))

    same = changed = gone = 0
    for name in sorted(base):
        for f, body in base[name]:
            if name not in now:
                print(f"gone     {name}  ({f})")
                gone += 1
                continue
            cands = now[name]
            if any(normalize(b) == normalize(body) for _, b in cands):
                same += 1
                continue
            changed += 1
            nf, nb = cands[0]
            print(f"CHANGED  {name}  ({f} -> {nf})")
            for d in difflib.unified_diff(body.splitlines(), nb.splitlines(), lineterm="", n=1):
                print("    " + d)
    print(f"\n{same} same, {changed} changed, {gone} gone")


if __name__ == "__main__":
    main()
