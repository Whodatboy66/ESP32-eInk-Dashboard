// Host test for src/services/PrinterReport.h (stage 7b): a Bambu printer's MQTT report to the
// printer widget's data. Run: tests/host/run.sh
#include <ArduinoJson.h>

#include "src/services/PrinterReport.h"
#include "tests/host/check.h"

namespace {

JsonDocument json(const char* text) {
  JsonDocument d;
  deserializeJson(d, text);
  return d;
}

const char* FULL = R"({"print":{"gcode_state":"RUNNING","percent":42,"mc_remaining_time":95,
  "3D":{"layer_num":12,"total_layer_num":180},"bed_temper":60.4,"nozzle_temper":219.6,
  "cooling_fan_speed":"15","big_fan1_speed":"10","big_fan2_speed":"0",
  "ams":{"ams":[{"id":"0","humidity_raw":"38","temp":"24.5","tray":[{"id":"0","remain":80,"tray_type":"PLA"},
  {"id":"1","remain":-1,"tray_type":"PETG"},{"id":"2"}]}]}}})";

void test_full_report() {
  JsonDocument d = json(FULL);
  bambu::PrinterStatus p{};
  CHECK(bambu::readReport(d["print"], p));
  CHECK_STR(p.gcode_state, "RUNNING");
  CHECK(p.percent == 42 && p.remainTime == 95 && p.layer_num == 12 && p.total_layer_num == 180);
  CHECK(p.bed_temp > 60.3f && p.nozzle_temp > 219.5f);
  CHECK(p.cooling_fan_speed == 15 && p.big_fan1_speed == 10 && p.big_fan2_speed == 0);  // numbers in strings
  CHECK_EQ(p.amsCount, 1);
  CHECK(p.ams[0].humidity == 38 && p.ams[0].temp > 24.4f && p.ams[0].trayCount == 3);
  CHECK(p.ams[0].trays[0].remain == 80 && p.ams[0].trays[1].remain == -1);
  CHECK_STR(p.ams[0].trays[1].tray_type, "PETG");
  CHECK_STR(p.ams[0].trays[2].tray_type, "");  // an empty slot
  CHECK_EQ(p.ams[0].trays[2].remain, -1);  // shown as "Leer" (the old code showed "0%")
  JsonDocument unknown = json(R"({"print":{"gcode_state":"PREPARE","mc_remaining_time":-1}})");
  CHECK(bambu::readReport(unknown["print"], p) && p.remainTime == 0);  // -1: not known yet
  CHECK(bambu::isPrinting(p));
}

// Idle printers send small updates without gcode_state; only the full report (the answer to
// "pushall") counts, so the updates don't overwrite it with zeros.
void test_delta_ignored() {
  JsonDocument d = json(R"({"print":{"bed_temper":25.0,"percent":0}})");
  bambu::PrinterStatus p{};
  CHECK(!bambu::readReport(d["print"], p));
  JsonDocument other = json(R"({"info":{"command":"get_version"}})");
  CHECK(!bambu::readReport(other["print"], p));
}

void test_printing_states() {
  bambu::PrinterStatus p{};
  for (const char* s : {"RUNNING", "PREPARE", "PAUSE"}) {
    snprintf(p.gcode_state, sizeof(p.gcode_state), "%s", s);
    CHECK(bambu::isPrinting(p));
  }
  snprintf(p.gcode_state, sizeof(p.gcode_state), "FINISH");
  p.percent = 100;
  CHECK(!bambu::isPrinting(p));
  p.percent = 57;  // a state the firmware doesn't know, but the job is half done
  snprintf(p.gcode_state, sizeof(p.gcode_state), "SLICING");
  CHECK(bambu::isPrinting(p));
}

void test_fan_percent() {
  CHECK_EQ(bambu::fanPercent(15), 100);  // the printer reports 0-15
  CHECK_EQ(bambu::fanPercent(10), 70);   // rounded up to tens, as the printer's screen
  CHECK_EQ(bambu::fanPercent(0), 0);
}

}  // namespace

int main() {
  test_full_report();
  test_delta_ignored();
  test_printing_states();
  test_fan_percent();
  return finish("printer report");
}
