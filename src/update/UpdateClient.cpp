#include "UpdateClient.h"

#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Update.h>
#include <mbedtls/sha256.h>

#include "../../configure.h"

// The ESP-IDF default certificate bundle (common CAs), linked into the core's mbedTLS library.
extern const uint8_t x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_bundle_end[] asm("_binary_x509_crt_bundle_end");

namespace ota {
namespace {

constexpr uint32_t HTTP_TIMEOUT_MS = 15000;
constexpr uint32_t STALL_TIMEOUT_MS = 20000;   // no data for this long: give up
constexpr size_t MANIFEST_MAX_BYTES = 8192;

// TLS client that verifies every connection with the certificate bundle and only talks to GitHub.
// The core clears the bundle setting whenever a connection is closed (stop_ssl_socket() wipes
// the whole context), and HTTPClient reconnects for each redirect to another host (github.com ->
// release-assets.githubusercontent.com). So the bundle is attached again before every connect.
class GitHubClient : public NetworkClientSecure {
public:
  int connect(const char* host, uint16_t port, int32_t timeout) override {
    if (!isAllowedHost(host)) {
      Serial.printf("[OTA] refused connection to %s\n", host ? host : "?");
      return 0;
    }
    setCACertBundle(x509_crt_bundle_start, x509_crt_bundle_end - x509_crt_bundle_start);
    return NetworkClientSecure::connect(host, port, timeout);
  }
  using NetworkClientSecure::connect;
};

void prepare(HTTPClient& http) {
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);  // GitHub redirects to its file host
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setUserAgent("ESP32-eInk-Dashboard/" FW_VERSION);
}

}  // namespace

FetchOutcome fetchManifest(Manifest& out) {
  FetchOutcome outcome;
  GitHubClient client;
  HTTPClient http;
  prepare(http);
  const char* url = "https://github.com/" OTA_REPO "/releases/latest/download/manifest.json";
  if (!http.begin(client, url)) return outcome;

  outcome.httpCode = http.GET();
  if (outcome.httpCode != HTTP_CODE_OK) {
    outcome.result = outcome.httpCode == HTTP_CODE_NOT_FOUND ? FetchResult::NoRelease : FetchResult::HttpError;
    Serial.printf("[OTA] manifest: HTTP %d\n", outcome.httpCode);
    http.end();
    return outcome;
  }
  int size = http.getSize();
  if (size > (int)MANIFEST_MAX_BYTES) {
    outcome.result = FetchResult::BadManifest;
    outcome.manifestError = ManifestError::BadJson;
    http.end();
    return outcome;
  }
  String body = http.getString();
  http.end();
  if (body.length() > MANIFEST_MAX_BYTES) {  // no Content-Length: checked after reading
    outcome.result = FetchResult::BadManifest;
    outcome.manifestError = ManifestError::BadJson;
    return outcome;
  }

  outcome.manifestError = parseManifest(body.c_str(), body.length(), FW_VARIANT, out);
  outcome.result = outcome.manifestError == ManifestError::None ? FetchResult::Ok : FetchResult::BadManifest;
  Serial.printf("[OTA] manifest: build %u (%s), file for %s: %s, error %d\n", (unsigned)out.build,
                out.version, FW_VARIANT, out.hasFile ? out.fileName : "none", (int)outcome.manifestError);
  return outcome;
}

InstallResult install(const Manifest& m, ProgressFn progress) {
  char url[256];
  if (!buildAssetUrl(url, sizeof(url), OTA_REPO, m.tag, m.fileName)) return InstallResult::BadUrl;

  GitHubClient client;
  HTTPClient http;
  prepare(http);
  if (progress) progress(InstallStage::Downloading, 0);
  if (!http.begin(client, url)) return InstallResult::DownloadFailed;
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[OTA] download: HTTP %d\n", code);
    http.end();
    return InstallResult::DownloadFailed;
  }
  int size = http.getSize();
  if (size != (int)m.fileSize) {
    Serial.printf("[OTA] download: size %d, manifest says %u\n", size, (unsigned)m.fileSize);
    http.end();
    return InstallResult::WrongSize;
  }
  if (!Update.begin(m.fileSize, U_FLASH)) {
    Serial.printf("[OTA] no update slot: %s\n", Update.errorString());
    http.end();
    return InstallResult::NoSpace;
  }

  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);

  NetworkClient* stream = http.getStreamPtr();
  static uint8_t buf[4096];
  uint32_t done = 0;
  int quarter = 0;
  unsigned long lastData = millis();
  InstallResult result = InstallResult::Ok;
  while (done < m.fileSize) {
    size_t avail = stream->available();
    if (!avail) {
      if (!http.connected() || millis() - lastData > STALL_TIMEOUT_MS) {
        result = InstallResult::DownloadFailed;
        break;
      }
      delay(5);
      continue;
    }
    size_t want = m.fileSize - done;
    if (want > sizeof(buf)) want = sizeof(buf);
    if (want > avail) want = avail;
    int got = stream->readBytes(buf, want);
    if (got <= 0) continue;
    lastData = millis();
    mbedtls_sha256_update(&sha, buf, got);
    if (Update.write(buf, got) != (size_t)got) {
      Serial.printf("[OTA] write failed: %s\n", Update.errorString());
      result = InstallResult::WriteFailed;
      break;
    }
    done += got;
    int q = progressStep(done, m.fileSize);
    if (q != quarter) {
      quarter = q;
      if (progress && q < 4) progress(InstallStage::Downloading, q);
    }
  }
  http.end();

  uint8_t digest[32];
  mbedtls_sha256_finish(&sha, digest);
  mbedtls_sha256_free(&sha);
  if (result != InstallResult::Ok) {
    Serial.printf("[OTA] download stopped at %u of %u bytes\n", (unsigned)done, (unsigned)m.fileSize);
    Update.abort();
    return result;
  }

  if (progress) progress(InstallStage::Verifying, 4);
  char hex[65];
  toHex(digest, sizeof(digest), hex, sizeof(hex));
  if (strcmp(hex, m.sha256) != 0) {
    Serial.printf("[OTA] SHA-256 mismatch: got %s\n", hex);
    Update.abort();
    return InstallResult::HashMismatch;
  }

  if (progress) progress(InstallStage::Installing, 4);
  // Checks the image and makes the new slot the boot slot. The bootloader keeps the old slot
  // until the new firmware confirms itself (rollback).
  if (!Update.end()) {
    Serial.printf("[OTA] activate failed: %s\n", Update.errorString());
    return InstallResult::WriteFailed;
  }
  Serial.printf("[OTA] installed %s (%u bytes), restart to run it\n", m.version, (unsigned)m.fileSize);
  return InstallResult::Ok;
}

const char* describe(InstallResult r) {
  switch (r) {
    case InstallResult::Ok:             return "installed";
    case InstallResult::BadUrl:         return "bad file name in the release";
    case InstallResult::DownloadFailed: return "download failed";
    case InstallResult::WrongSize:      return "file size does not match";
    case InstallResult::NoSpace:        return "does not fit - or no update slot: flash once over USB";
    case InstallResult::HashMismatch:   return "file damaged - checksum wrong";
    case InstallResult::WriteFailed:    return "writing failed";
  }
  return "unknown error";
}

}  // namespace ota
