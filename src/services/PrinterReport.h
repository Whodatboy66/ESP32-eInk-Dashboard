#pragma once
// A Bambu printer's status report (MQTT, the answer to "pushall"), stage 7b own code: what the
// printer widget shows. Plain C++ with ArduinoJson, tested on the PC (tests/host/test_printer_report.cpp);
// the MQTT connection is in Bambu.cpp.
#include <ArduinoJson.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace bambu {

constexpr int MAX_AMS = 4;
constexpr int MAX_TRAYS = 4;

struct Tray {
  int remain;        // percent, -1 = unknown (a spool without RFID)
  char tray_type[10];  // "PLA", "" = an empty slot
  char brand[24];    // stage 8: "PLA Basic" (tray_sub_brands), "" = unknown
  uint32_t color;    // stage 8: RRGGBBAA (tray_color), 0 = none
};

struct Ams {
  int humidity;
  float temp;
  int trayCount;
  Tray trays[MAX_TRAYS];
};

// One of the printer's messages (HMS: health management): two numbers, shown as "0C00_0100_0002_0001".
struct Hms {
  uint32_t attr, code;
};
constexpr int MAX_HMS = 4;

struct PrinterStatus {
  char gcode_state[20];  // RUNNING, PREPARE, PAUSE, FINISH, FAILED, IDLE, ...
  int percent;
  unsigned remainTime;   // minutes
  int layer_num, total_layer_num;
  float bed_temp, nozzle_temp;
  int cooling_fan_speed, big_fan1_speed, big_fan2_speed;  // 0-15 (the printer's steps)
  int amsCount;
  Ams ams[MAX_AMS];
  int hmsCount;  // stage 8
  Hms hms[MAX_HMS];
};

// The printer sends some numbers as strings ("15", "24.5"): both are read.
inline float number(JsonVariantConst v) {
  if (v.is<const char*>()) return static_cast<float>(std::atof(v.as<const char*>()));
  return v.is<float>() ? v.as<float>() : 0.0f;
}

// A full report: only one with gcode_state counts (idle printers also send small updates that
// would overwrite the status with zeros). False otherwise, p unchanged.
inline bool readReport(JsonVariantConst print, PrinterStatus& p) {
  if (print.isNull() || print["gcode_state"].isNull()) return false;
  p = PrinterStatus();
  std::snprintf(p.gcode_state, sizeof(p.gcode_state), "%s", print["gcode_state"] | "");
  p.percent = static_cast<int>(number(print["percent"]));
  const float left = number(print["mc_remaining_time"]);
  p.remainTime = left > 0 ? static_cast<unsigned>(left) : 0;  // the printer sends -1 when it doesn't know
  p.layer_num = static_cast<int>(number(print["3D"]["layer_num"]));
  p.total_layer_num = static_cast<int>(number(print["3D"]["total_layer_num"]));
  p.bed_temp = number(print["bed_temper"]);
  p.nozzle_temp = number(print["nozzle_temper"]);
  p.cooling_fan_speed = static_cast<int>(number(print["cooling_fan_speed"]));
  p.big_fan1_speed = static_cast<int>(number(print["big_fan1_speed"]));
  p.big_fan2_speed = static_cast<int>(number(print["big_fan2_speed"]));
  for (JsonVariantConst a : print["ams"]["ams"].as<JsonArrayConst>()) {
    if (p.amsCount >= MAX_AMS) break;
    Ams& ams = p.ams[p.amsCount++];
    ams.humidity = static_cast<int>(number(a["humidity_raw"]));
    ams.temp = number(a["temp"]);
    for (JsonVariantConst t : a["tray"].as<JsonArrayConst>()) {
      if (ams.trayCount >= MAX_TRAYS) break;
      Tray& tray = ams.trays[ams.trayCount++];
      tray.remain = t["remain"].isNull() ? -1 : static_cast<int>(number(t["remain"]));
      std::snprintf(tray.tray_type, sizeof(tray.tray_type), "%s", t["tray_type"] | "");
      std::snprintf(tray.brand, sizeof(tray.brand), "%s", t["tray_sub_brands"] | "");
      tray.color = static_cast<uint32_t>(std::strtoul(t["tray_color"] | "0", nullptr, 16));
    }
  }
  for (JsonVariantConst h : print["hms"].as<JsonArrayConst>()) {
    if (p.hmsCount >= MAX_HMS) break;
    p.hms[p.hmsCount].attr = h["attr"] | 0u;
    p.hms[p.hmsCount].code = h["code"] | 0u;
    p.hmsCount++;
  }
  return true;
}

// A message's code as the printer and Bambu's wiki write it: "0C00_0100_0002_0001".
inline void hmsCode(const Hms& h, char* out, size_t cap) {
  std::snprintf(out, cap, "%04X_%04X_%04X_%04X", static_cast<unsigned>(h.attr >> 16), static_cast<unsigned>(h.attr & 0xFFFF),
                static_cast<unsigned>(h.code >> 16), static_cast<unsigned>(h.code & 0xFFFF));
}

// Printing (also preparing and paused), or a job between 0 and 100 %.
inline bool isPrinting(const PrinterStatus& p) {
  if (!std::strcmp(p.gcode_state, "RUNNING") || !std::strcmp(p.gcode_state, "PREPARE") ||
      !std::strcmp(p.gcode_state, "PAUSE"))
    return true;
  return p.percent > 0 && p.percent < 100;
}

// The printer's fan step (0-15) as percent, rounded up to tens like the printer's own screen.
inline int fanPercent(int raw) { return static_cast<int>(std::ceil(raw * 10.0f / 15.0f) * 10); }

}  // namespace bambu
