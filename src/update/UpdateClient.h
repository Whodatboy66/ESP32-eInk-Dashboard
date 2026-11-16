#pragma once
// Over-the-air update client: downloads the signed manifest of OTA_REPO's release channel
// (stable = latest release, beta = the "beta" pre-release) and installs the release file for this
// board into the inactive app slot (docs/OTA_UPDATES.md). Every request uses verified HTTPS (the
// ESP-IDF certificate bundle) and only goes to GitHub. Needs Wi-Fi.

#include <Arduino.h>

#include "Manifest.h"

namespace ota {

// Result of fetching and reading the manifest.
enum class FetchResult : uint8_t {
  Ok,
  NoRelease,    // HTTP 404: no release with a manifest yet
  HttpError,    // other HTTP / connection error (code in httpCode)
  BadSignature, // manifest.sig missing or not made with our signing key: nothing is used
  BadManifest   // signed, but not a valid manifest (error in manifestError)
};

struct FetchOutcome {
  FetchResult result = FetchResult::HttpError;
  int httpCode = 0;
  ManifestError manifestError = ManifestError::None;
};

FetchOutcome fetchManifest(bool beta, Manifest& out);

enum class InstallStage : uint8_t { Downloading, Verifying, Installing };

// Called when the stage changes and, while downloading, at each new quarter (0-4) of the file.
using ProgressFn = void (*)(InstallStage stage, int quarter);

enum class InstallResult : uint8_t {
  Ok,              // written and activated: restart to run it
  BadUrl,          // unsafe tag or file name in the manifest
  DownloadFailed,  // HTTP error, connection lost or timeout (nothing activated)
  WrongSize,       // the server's size doesn't match the manifest
  NoSpace,         // doesn't fit the update slot / slot not available (old partition table)
  HashMismatch,    // SHA-256 differs from the manifest (corrupt download, nothing activated)
  WriteFailed      // flash write or image check failed (nothing activated)
};

InstallResult install(const Manifest& m, ProgressFn progress);

const char* describe(InstallResult r);

}  // namespace ota
