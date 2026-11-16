#pragma once
// Release manifest (manifest.json, written by the CI release job, see docs/OTA_UPDATES.md):
//
//   {"schema": 1, "build": 65, "version": ".65", "min_build": 0, "tag": "firmware-.65",
//    "notes": ["What's new, up to 3 lines"],
//    "files": {"esp32c6_firebeetle": {"name": "ESP32C6-FireBeetle.bin", "size": 1359040,
//                                     "sha256": "<64 hex digits>"}, ...}}
//
// Only the entry for this board variant is read. Not signed yet (stage 1 follow-up): the
// manifest is trusted because it comes over verified HTTPS from this repository's releases.

#include <ArduinoJson.h>
#include <cstdint>
#include <cstring>

#include "UpdatePolicy.h"

namespace ota {

constexpr int MANIFEST_SCHEMA = 1;
constexpr int MAX_NOTES = 3;
constexpr size_t NOTE_LEN = 72;  // bytes per "What's new" line (about one screen width)

struct Manifest {
  uint32_t build = 0;
  uint32_t minBuild = 0;
  char version[16] = "";
  char tag[64] = "";
  char notes[MAX_NOTES][NOTE_LEN] = {};
  uint8_t noteCount = 0;
  bool hasFile = false;  // the release has a file for this variant
  char fileName[64] = "";
  uint32_t fileSize = 0;
  char sha256[65] = "";
};

enum class ManifestError : uint8_t {
  None,
  BadJson,       // not JSON / too large
  BadSchema,     // a manifest format this firmware doesn't know
  MissingField,  // build, version or a safe tag missing
  BadFile        // the entry for this variant is incomplete or unsafe
};

inline void copyField(char* dst, size_t cap, const char* src) {
  if (!src) src = "";
  std::strncpy(dst, src, cap - 1);
  dst[cap - 1] = '\0';
}

inline ManifestError parseManifest(const char* json, size_t len, const char* variant, Manifest& out) {
  out = Manifest{};
  JsonDocument doc;
  if (deserializeJson(doc, json, len)) return ManifestError::BadJson;
  if (!doc.is<JsonObject>()) return ManifestError::BadJson;
  if ((doc["schema"] | 0) != MANIFEST_SCHEMA) return ManifestError::BadSchema;

  if (!doc["build"].is<uint32_t>() || !doc["version"].is<const char*>() ||
      !doc["tag"].is<const char*>() || !isSafeName(doc["tag"].as<const char*>())) {
    return ManifestError::MissingField;
  }
  out.build = doc["build"].as<uint32_t>();
  out.minBuild = doc["min_build"] | 0u;
  copyField(out.version, sizeof(out.version), doc["version"].as<const char*>());
  copyField(out.tag, sizeof(out.tag), doc["tag"].as<const char*>());

  for (JsonVariantConst note : doc["notes"].as<JsonArrayConst>()) {
    if (out.noteCount >= MAX_NOTES) break;
    if (!note.is<const char*>()) continue;
    sanitizeNote(note.as<const char*>(), out.notes[out.noteCount], NOTE_LEN);
    if (out.notes[out.noteCount][0]) out.noteCount++;
  }

  JsonObjectConst file = doc["files"][variant];
  if (file.isNull()) return ManifestError::None;  // nothing for this board
  const char* name = file["name"] | "";
  const char* sha = file["sha256"] | "";
  uint32_t size = file["size"] | 0u;
  if (!isSafeName(name) || !isSha256Hex(sha) || size == 0) return ManifestError::BadFile;
  out.hasFile = true;
  copyField(out.fileName, sizeof(out.fileName), name);
  copyField(out.sha256, sizeof(out.sha256), sha);
  out.fileSize = size;
  return ManifestError::None;
}

}  // namespace ota
