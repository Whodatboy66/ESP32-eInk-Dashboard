#!/usr/bin/env bash
# Renders the dashboard on the PC (tools/render/render.cpp) into PNG images.
# Usage: tools/render/render.sh [out-dir] [de|en] [screens]
#   default docs/images/layouts in German; "screens" adds the setup, update and charge screens
#   (screen-*.png). Needs g++, python3.
set -euo pipefail
cd "$(dirname "$0")/../.."
CXX="${CXX:-g++}"
OUT="${1:-docs/images/layouts}"
LANG_ARG="${2:-de}"
SCREENS="${3:-}"
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
  src/ui/Canvas.cpp src/core/Problems.cpp src/core/DataStore.cpp src/update/UpdateScreens.cpp src/web/SetupScreen.cpp \
  -x c src/ui/qr/qrcodegen.c -x none \
  -Wl,--wrap=time -o "$BUILD/render"
# name template hidden-widgets
while read -r name tpl hide variant; do
  [[ -z "$name" ]] && continue
  [[ "$hide" == "-" ]] && hide=""
  "$BUILD/render" "$BUILD/$name.pbm" "$tpl" "$hide" "${variant:-}" "$LANG_ARG" 2>/dev/null
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
planner-problems planner - problems
minimal-12h minimal - 12h
classic-stale classic - stale
personal personal
personal-long personal - long
custom-top-agenda v1;sb1;0,0,4,1,agenda;0,1,2,2,tasks;2,1,2,2,month
custom-no-status-bar v1;sb0;0,0,2,1,clock;2,0,2,1,weather;0,1,2,2,agenda;2,1,2,2,notes - problems
planner-night planner - night
classic-saver classic - saver
clock-2x1-12h v1;sb1;0,0,2,1,clock;2,0,2,1,weather;0,1,2,2,agenda;2,1,2,2,month - 12h1258
clock-1x1 v1;sb1;0,0,1,1,clock;1,0,3,1,weather;0,1,2,2,agenda;2,1,2,2,month
month-full v1;sb1;0,0,4,3,month - jan27
month-half v1;sb1;0,0,2,3,month;2,0,2,3,agenda - jan27
month-2x2 planner - jan27
month-1x2 v1;sb1;0,0,1,2,month;1,0,3,2,agenda;0,2,4,1,weather - jan27
month-2x1 v1;sb1;0,0,2,1,month;2,0,2,1,clock;0,1,4,2,agenda - jan27
month-4x1 v1;sb1;0,0,4,1,month;0,1,4,2,agenda - jan27
month-1x1 v1;sb1;0,0,1,1,month;1,0,3,1,weather;0,1,4,2,agenda - jan27
month-classic classic - jan27
holiday planner - holiday
holiday-saver planner - holiday-saver
LIST
if [[ "$SCREENS" == "screens" ]]; then
  for screen in setup-ap setup-home update-info update-available update-progress update-failed update-battery \
                update-done charge; do
    "$BUILD/render" "$BUILD/screen-$screen.pbm" "screen:$screen" "" "" "$LANG_ARG" 2>/dev/null
    python3 -I tools/render/pbm_to_png.py "$BUILD/screen-$screen.pbm" "$OUT/screen-$screen.png"
    echo "$OUT/screen-$screen.png"
  done
fi
