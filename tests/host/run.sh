#!/usr/bin/env bash
# Compiles and runs every tests/host/test_*.cpp on the PC (no Arduino, no board).
# Usage: tests/host/run.sh            (uses g++; override with CXX=/path/to/g++)
# ArduinoJson (header-only) is needed for some tests: set ARDUINOJSON_INCLUDE to the folder that
# contains ArduinoJson.h, otherwise the copy installed by arduino-cli for sketch.yaml is used.
# Tests marked "// needs: mbedtls" link mbedTLS (2.28 or 3.x): the system one (Ubuntu:
# libmbedtls-dev), or the one under MBEDTLS_PREFIX (include/ and lib/).
set -euo pipefail

cd "$(dirname "$0")/../.."
CXX="${CXX:-g++}"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

AJ_VERSION="$(sed -n 's/.*- ArduinoJson (\(.*\))/\1/p' sketch.yaml | head -1)"
if [[ -z "${ARDUINOJSON_INCLUDE:-}" ]]; then
  for dir in "$HOME"/.arduino15/internal/ArduinoJson_"$AJ_VERSION"_*/ArduinoJson/src \
             "$HOME"/Arduino/libraries/ArduinoJson/src; do
    if [[ -f "$dir/ArduinoJson.h" ]]; then ARDUINOJSON_INCLUDE="$dir"; break; fi
  done
fi
INCLUDES=(-I. -Itests/host/shim)  # shim: the font headers' glyph type (test_i18n.cpp)
if [[ -n "${ARDUINOJSON_INCLUDE:-}" ]]; then
  INCLUDES+=(-I"$ARDUINOJSON_INCLUDE")
else
  echo "!! ArduinoJson $AJ_VERSION not found: set ARDUINOJSON_INCLUDE (tests that need it will fail)"
fi

status=0
for test in tests/host/test_*.cpp; do
  name="$(basename "$test" .cpp)"
  echo "== $name"
  EXTRA=()
  if grep -q '^// needs: mbedtls' "$test"; then
    if [[ -n "${MBEDTLS_PREFIX:-}" ]]; then
      EXTRA+=(-I"$MBEDTLS_PREFIX/include" -L"$MBEDTLS_PREFIX/lib" -Wl,-rpath,"$MBEDTLS_PREFIX/lib")
    fi
    EXTRA+=(-lmbedcrypto)
  fi
  # Tests marked "// needs: miniz" link the pinned miniz copy (third_party/miniz: the firmware uses
  # the same decompressor from the chip's ROM). Third-party code: built without our warning flags.
  if grep -q '^// needs: miniz' "$test"; then
    if [[ ! -f "$OUT/miniz.o" ]]; then
      "$CXX" -x c -w -O1 -DMINIZ_NO_STDIO -DMINIZ_NO_ARCHIVE_APIS -c tests/host/third_party/miniz/miniz.c -o "$OUT/miniz.o"
    fi
    EXTRA+=(-isystem tests/host/third_party/miniz -x none "$OUT/miniz.o")
  fi
  if ! "$CXX" -std=c++17 -Wall -Wextra -Werror "${INCLUDES[@]}" "$test" "${EXTRA[@]}" -o "$OUT/$name"; then
    echo "!! $name: build failed"
    status=1
    continue
  fi
  "$OUT/$name" || status=1
done
exit "$status"
