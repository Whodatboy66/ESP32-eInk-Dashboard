// Host test for src/update/Manifest.h (needs ArduinoJson, found by tests/host/run.sh).
#include "src/update/Manifest.h"
#include "tests/host/check.h"

#include <string>

using namespace ota;

static const char* kHash = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

static std::string manifest(const std::string& files, const std::string& extra = "") {
  return std::string("{\"schema\":1,\"build\":65,\"version\":\".65\",\"min_build\":60,"
                     "\"tag\":\"firmware-.65\",\"notes\":[\"Faster calendar\",\"Fix (weather)!\"],"
                     "\"files\":{") + files + "}" + extra + "}";
}

static std::string file(const char* variant, const char* name = "ESP32C6-FireBeetle.bin",
                        const char* size = "1359040", const char* hash = kHash,
                        const char* ids = ",\"display\":\"bw750\",\"config_schema\":1") {
  return std::string("\"") + variant + "\":{\"name\":\"" + name + "\",\"size\":" + size +
         ",\"sha256\":\"" + hash + "\"" + ids + "}";
}

static ManifestError parse(const std::string& json, const char* variant, Manifest& m) {
  return parseManifest(json.c_str(), json.size(), variant, m);
}

static void test_valid_manifest() {
  Manifest m;
  std::string json = manifest(file("esp32") + "," + file("esp32c6_firebeetle"));
  CHECK(parse(json, "esp32c6_firebeetle", m) == ManifestError::None);
  CHECK_EQ(m.build, 65u);
  CHECK_EQ(m.minBuild, 60u);
  CHECK_STR(m.version, ".65");
  CHECK_STR(m.tag, "firmware-.65");
  CHECK_EQ(m.noteCount, 2);
  CHECK_STR(m.notes[0], "Faster calendar");
  CHECK_STR(m.notes[1], "Fix weather.");  // made displayable
  CHECK(m.hasFile);
  CHECK_STR(m.fileName, "ESP32C6-FireBeetle.bin");
  CHECK_EQ(m.fileSize, 1359040u);
  CHECK_STR(m.sha256, kHash);
  CHECK_STR(m.fileDisplay, "bw750");
  CHECK_EQ(m.fileConfigSchema, 1u);
}

static void test_display_and_schema_are_required() {
  Manifest m;
  CHECK(parse(manifest(file("esp32", "a.bin", "100", kHash, "")), "esp32", m) == ManifestError::BadFile);
  CHECK(parse(manifest(file("esp32", "a.bin", "100", kHash, ",\"config_schema\":1")), "esp32", m) ==
        ManifestError::BadFile);  // no display
  CHECK(parse(manifest(file("esp32", "a.bin", "100", kHash, ",\"display\":\"bw750\"")), "esp32", m) ==
        ManifestError::BadFile);  // no config_schema
  CHECK(parse(manifest(file("esp32", "a.bin", "100", kHash, ",\"display\":\"../x\",\"config_schema\":1")),
              "esp32", m) == ManifestError::BadFile);  // unsafe display id
  CHECK(parse(manifest(file("esp32", "a.bin", "100", kHash, ",\"display\":\"display_id_16_ch\",\"config_schema\":1")),
              "esp32", m) == ManifestError::BadFile);  // 16 characters: too long
  CHECK(parse(manifest(file("esp32", "a.bin", "100", kHash, ",\"display\":\"display_id_15_c\",\"config_schema\":1")),
              "esp32", m) == ManifestError::None);     // 15 is the limit
}

static void test_availability_for_this_device() {
  Manifest m;
  CHECK(parse(manifest(file("esp32c6_firebeetle")), "esp32c6_firebeetle", m) == ManifestError::None);
  // build 65, min_build 60, display bw750, config_schema 1
  CHECK(availabilityFor(m, DeviceIds{64, "bw750", 1}) == Availability::Newer);
  CHECK(availabilityFor(m, DeviceIds{65, "bw750", 1}) == Availability::UpToDate);
  CHECK(availabilityFor(m, DeviceIds{59, "bw750", 1}) == Availability::NeedsNewerFirmware);
  CHECK(availabilityFor(m, DeviceIds{64, "color750", 1}) == Availability::WrongDisplay);
  // The new firmware knows settings schema 1, this device already has schema 2: it can't read them.
  CHECK(availabilityFor(m, DeviceIds{64, "bw750", 2}) == Availability::IncompatibleSettings);
  CHECK(availabilityFor(m, DeviceIds{64, "bw750", 0}) == Availability::Newer);  // it migrates older ones

  Manifest none;
  CHECK(parse(manifest(file("esp32")), "esp32c6_firebeetle", none) == ManifestError::None);
  CHECK(availabilityFor(none, DeviceIds{64, "bw750", 1}) == Availability::NotForThisBoard);
}

static void test_no_file_for_this_variant() {
  Manifest m;
  CHECK(parse(manifest(file("esp32")), "esp32c6_supermini", m) == ManifestError::None);
  CHECK(!m.hasFile);
}

static void test_at_most_three_notes() {
  Manifest m;
  std::string json = "{\"schema\":1,\"build\":65,\"version\":\".65\",\"tag\":\"t\","
                     "\"notes\":[\"a\",\"b\",\"c\",\"d\"],\"files\":{}}";
  CHECK(parse(json, "esp32", m) == ManifestError::None);
  CHECK_EQ(m.noteCount, 3);
  CHECK_EQ(m.minBuild, 0u);  // optional
}

static void test_rejects_bad_input() {
  Manifest m;
  CHECK(parse("not json", "esp32", m) == ManifestError::BadJson);
  CHECK(parse("{\"schema\":2,\"build\":65,\"version\":\".65\",\"tag\":\"t\",\"files\":{}}", "esp32", m) ==
        ManifestError::BadSchema);
  CHECK(parse("{\"schema\":1,\"version\":\".65\",\"tag\":\"t\",\"files\":{}}", "esp32", m) ==
        ManifestError::MissingField);  // no build
  CHECK(parse("{\"schema\":1,\"build\":65,\"version\":\".65\",\"files\":{}}", "esp32", m) ==
        ManifestError::MissingField);  // no tag
  CHECK(parse("{\"schema\":1,\"build\":65,\"version\":\".65\",\"tag\":\"../x\",\"files\":{}}", "esp32", m) ==
        ManifestError::MissingField);  // unsafe tag
  CHECK(parse(manifest(file("esp32", "../x.bin")), "esp32", m) == ManifestError::BadFile);
  CHECK(parse(manifest(file("esp32", "a.bin", "0")), "esp32", m) == ManifestError::BadFile);
  CHECK(parse(manifest(file("esp32", "a.bin", "100", "abc")), "esp32", m) == ManifestError::BadFile);
  // An invalid entry for another board doesn't matter.
  CHECK(parse(manifest(file("esp32", "../x.bin") + "," + file("esp32c6_firebeetle")), "esp32c6_firebeetle", m) ==
        ManifestError::None);
}

int main() {
  test_valid_manifest();
  test_no_file_for_this_variant();
  test_display_and_schema_are_required();
  test_availability_for_this_device();
  test_at_most_three_notes();
  test_rejects_bad_input();
  return finish("manifest");
}
