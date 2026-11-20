#!/usr/bin/env bash
# Renders the dashboard on the PC (tools/render/render.cpp) into PNG images.
# Usage: tools/render/render.sh [out-dir]   (default docs/images/layouts; needs g++, python3)
set -euo pipefail
cd "$(dirname "$0")/../.."
CXX="${CXX:-g++}"
OUT="${1:-docs/images/layouts}"
GFX="$(ls -d "$HOME"/.arduino15/internal/Adafruit_GFX_Library_*/"Adafruit GFX Library" | head -1)"
AJ="$(ls -d "$HOME"/.arduino15/internal/ArduinoJson_*/ArduinoJson/src | head -1)"
BUILD="$(mktemp -d)"
trap 'rm -rf "$BUILD"' EXIT
mkdir -p "$OUT"
"$CXX" -std=gnu++17 -O1 -w -DRENDER_ON_PC -DARDUINO=10800 \
  -DARDUINOJSON_ENABLE_ARDUINO_STRING=0 -DARDUINOJSON_ENABLE_ARDUINO_STREAM=0 \
  -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0 -DARDUINOJSON_ENABLE_PROGMEM=0 -Itools/render/shim -I. -I"$GFX" -I"$AJ" \
  tools/render/render.cpp "$GFX/Adafruit_GFX.cpp" \
  src/ui/SparseFont.cpp src/ui/Fonts.cpp src/widgets/Widgets.cpp src/widgets/MonthView.cpp \
  src/widgets/Clock.cpp src/widgets/StatusBar.cpp src/widgets/Printer.cpp src/widgets/Registry.cpp \
  src/widgets/Dashboard.cpp src/widgets/Personal.cpp tools/render/UserStoreSample.cpp \
  src/ui/Canvas.cpp src/core/Problems.cpp \
  -Wl,--wrap=time -o "$BUILD/render"
# name template hidden-widgets
while read -r name tpl hide variant; do
  [[ -z "$name" ]] && continue
  [[ "$hide" == "-" ]] && hide=""
  "$BUILD/render" "$BUILD/$name.pbm" "$tpl" "$hide" "${variant:-}" 2>/dev/null
  python3 -I tools/render/pbm_to_png.py "$BUILD/$name.pbm" "$OUT/$name.png"
  echo "$OUT/$name.png"
done <<'LIST'
classic classic
planner planner
minimal minimal
technical technical
planner-no-weather planner weather
minimal-clock-only minimal weather,agenda
technical-no-stocks-tracking technical stocks,tracking
planner-stale planner - stale
minimal-12h minimal - 12h
planner-12h-1258 planner - 12h1258
technical-12h-1258 technical - 12h1258
classic-stale classic - stale
personal personal
personal-empty personal - empty
personal-long personal - long
custom-top-agenda v1;sb1;0,0,4,1,agenda;0,1,2,2,tasks;2,1,2,2,month
custom-no-status-bar v1;sb0;0,0,2,1,clock;2,0,2,1,weather;0,1,2,2,agenda;2,1,2,2,notes - problems
custom-full-month v1;sb1;0,0,4,3,month
planner-night planner - night
classic-saver classic - saver
LIST
