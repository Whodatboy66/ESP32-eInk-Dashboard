#!/usr/bin/env bash
# Compiles and runs every tests/host/test_*.cpp on the PC (no Arduino, no board).
# Usage: tests/host/run.sh            (uses g++; override with CXX=/path/to/g++)
set -euo pipefail

cd "$(dirname "$0")/../.."
CXX="${CXX:-g++}"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

status=0
for test in tests/host/test_*.cpp; do
  name="$(basename "$test" .cpp)"
  echo "== $name"
  if ! "$CXX" -std=c++17 -Wall -Wextra -Werror -I. "$test" -o "$OUT/$name"; then
    echo "!! $name: build failed"
    status=1
    continue
  fi
  "$OUT/$name" || status=1
done
exit "$status"
