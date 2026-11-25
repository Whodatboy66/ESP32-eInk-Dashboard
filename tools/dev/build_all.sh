#!/usr/bin/env bash
# Compiles the firmware like CI does (sketch.yaml profiles + pin preset + OTA slot limit) and prints
# a size table. Usage: tools/dev/build_all.sh [profile ...]   (default: all four)
# Build output: /tmp/eink-build/<profile>/ (separate build paths, so runs never share a cache dir).
set -uo pipefail
cd "$(dirname "$0")/../.."
SLOT=1966080
RTC_MAX=6000  # classic ESP32: 7,656 B of RTC slow memory (the store of stage 8 lives there)
declare -A PRESET=(
  [esp32]=DEFAULT_PIN_PRESET_ESP32
  [esp32_waveshare]=DEFAULT_PIN_PRESET_ESP32_WAVESHARE
  [esp32c6_firebeetle]=DEFAULT_PIN_PRESET_ESP32C6_FIREBEETLE
  [esp32c6_supermini]=DEFAULT_PIN_PRESET_ESP32C6_SUPERMINI
)
PROFILES=("$@")
[[ ${#PROFILES[@]} -eq 0 ]] && PROFILES=(esp32 esp32_waveshare esp32c6_firebeetle esp32c6_supermini)
status=0
printf "%-20s %4s %10s %7s %8s %6s %s\n" profile rc app slot ram rtc own_warnings
for p in "${PROFILES[@]}"; do
  out="/tmp/eink-build/$p"
  mkdir -p "$out"
  arduino-cli compile --profile "$p" --build-path "$out/bp" \
    --build-property "compiler.cpp.extra_flags=-D${PRESET[$p]} ${EXTRA_FLAGS:-}" \
    --build-property "upload.maximum_size=$SLOT" --warnings default \
    --export-binaries --output-dir "$out/bin" . > "$out/compile.log" 2>&1
  rc=$?
  app=$(stat -c %s "$out/bin/ESP32-eInk-Dashboard.ino.bin" 2>/dev/null || echo 0)
  ram=$(sed -n 's/^Global variables use \([0-9]*\) bytes.*/\1/p' "$out/compile.log")
  # Sleep memory (RTC): kept data, states. The classic ESP32 has the least (RTC_MAX).
  sizetool=$(ls ~/.arduino15/internal/*/bin/*-elf-size 2>/dev/null | grep -E "$([[ $p == esp32c6* ]] && echo riscv32 || echo xtensa-esp32-)" | head -1)
  rtc=$("$sizetool" -A "$out/bp/ESP32-eInk-Dashboard.ino.elf" 2>/dev/null |
        awk '$1 ~ /^\.rtc(\.data|_noinit|\.force_slow)$/ {s += $2} END {print s + 0}')
  warn=$(grep -cE "$(pwd)/(ESP32-eInk-Dashboard.ino|src/|configure.h)[^:]*:[0-9]+:[0-9]+: (warning|error)" "$out/compile.log")
  printf "%-20s %4s %10s %6.1f%% %8s %6s %s\n" "$p" "$rc" "$app" "$(awk -v a="$app" -v b="$SLOT" 'BEGIN{print 100*a/b}')" "${ram:-?}" "${rtc:-?}" "$warn"
  if [[ $p != esp32c6* && ${rtc:-0} -gt $RTC_MAX ]]; then
    echo "!! $p: RTC memory $rtc B > $RTC_MAX B"
    rc=1
  fi
  if [[ $rc -ne 0 ]]; then
    status=1
    grep -E "error" "$out/compile.log" | grep -v "^Error during build" | head -15
  fi
done
exit $status
