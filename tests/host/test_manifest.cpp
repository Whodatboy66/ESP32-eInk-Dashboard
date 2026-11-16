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
                        const char* size = "1359040", const char* hash = kHash) {
  return std::string("\"") + variant + "\":{\"name\":\"" + name + "\",\"size\":" + size +
         ",\"sha256\":\"" + hash + "\"}";
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
  test_at_most_three_notes();
  test_rejects_bad_input();
  return finish("manifest");
}
