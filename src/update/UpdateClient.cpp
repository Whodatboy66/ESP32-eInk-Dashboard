#include "UpdateClient.h"

#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Update.h>
#include <mbedtls/sha256.h>

#include <memory>
#include <new>

#include "../../configure.h"
#include "OtaPublicKey.h"
#include "Signature.h"
#include "../services/SecureClient.h"

// What this image is, as one string: CI reads it back out of the .bin to write the manifest entry
// (tools/ota/make_manifest.py), so the manifest always matches what was compiled. Sent as the
// User-Agent, which also keeps the linker from dropping it.
extern "C" const char OTA_FIRMWARE_ID[] =
    "OTA-ID:" FW_VARIANT ":" FW_DISPLAY ":" FW_STR(FW_CONFIG_SCHEMA) ":" FW_STR(FW_BUILD) ":END";


namespace ota {
namespace {

constexpr uint32_t HTTP_TIMEOUT_MS = 15000;
constexpr uint32_t STALL_TIMEOUT_MS = 20000;   // no data for this long: give up
constexpr size_t MANIFEST_MAX_BYTES = 8192;
constexpr size_t SIGNATURE_MAX_BYTES = 160;  // DER ECDSA P-256: at most 72 bytes
constexpr int TOO_LARGE = -100;              // download(): the body didn't fit the buffer

// Verified TLS (SecureClient re-attaches the certificate bundle on every connect, so GitHub's
// redirect to its file host works) and only to GitHub.
class GitHubClient : public SecureClient {
public:
  int connect(const char* host, uint16_t port, int32_t timeout) override {
    if (!isAllowedHost(host)) {
      logPrintf("[OTA] refused connection to %s\n", host ? host : "?");
      return 0;
    }
    return SecureClient::connect(host, port, timeout);
  }
  using SecureClient::connect;
};

void prepare(HTTPClient& http) {
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);  // GitHub redirects to its file host
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setUserAgent(String("ESP32-eInk-Dashboard ") + OTA_FIRMWARE_ID);
}

// Collects a response body into a fixed buffer (binary safe: the signature is DER).
class BufferStream : public Stream {
public:
  BufferStream(uint8_t* buf, size_t cap) : buf_(buf), cap_(cap) {}
  size_t write(uint8_t c) override {
    if (len_ >= cap_) {
      overflow_ = true;
      return 0;
    }
    buf_[len_++] = c;
    return 1;
  }
  size_t write(const uint8_t* data, size_t n) override {
    size_t i = 0;
    while (i < n && write(data[i])) i++;
    return i;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
  size_t length() const { return len_; }
  bool overflow() const { return overflow_; }

private:
  uint8_t* buf_;
  size_t cap_;
  size_t len_ = 0;
  bool overflow_ = false;
};

// GET url into buf; returns the HTTP code (200 = complete body in buf), a negative HTTPClient
// error, or TOO_LARGE.
int download(const char* url, uint8_t* buf, size_t cap, size_t* len) {
  GitHubClient client;
  HTTPClient http;
  prepare(http);
  *len = 0;
  if (!http.begin(client, url)) return HTTPC_ERROR_CONNECTION_REFUSED;
  int code = http.GET();
  if (code == HTTP_CODE_OK) {
    BufferStream body(buf, cap);
    int n = http.writeToStream(&body);
    if (body.overflow()) code = TOO_LARGE;
    else if (n < 0) code = n;
    *len = body.length();
  }
  http.end();
  return code;
}

}  // namespace

namespace {

FetchOutcome fetchManifestOnce(bool beta, Manifest& out) {
  FetchOutcome outcome;
  out = Manifest{};
  char url[192];
  std::unique_ptr<uint8_t[]> json(new (std::nothrow) uint8_t[MANIFEST_MAX_BYTES]);
  if (!json) {
    outcome.httpCode = HTTPC_ERROR_TOO_LESS_RAM;
    return outcome;
  }
  if (!manifestUrl(url, sizeof(url), OTA_REPO, beta, "manifest.json")) return outcome;

  size_t jsonLen = 0;
  outcome.httpCode = download(url, json.get(), MANIFEST_MAX_BYTES, &jsonLen);
  if (outcome.httpCode != HTTP_CODE_OK) {
    outcome.result = outcome.httpCode == HTTP_CODE_NOT_FOUND ? FetchResult::NoRelease
                   : outcome.httpCode == TOO_LARGE ? FetchResult::BadManifest
                   : FetchResult::HttpError;
    if (outcome.httpCode == TOO_LARGE) outcome.manifestError = ManifestError::BadJson;
    logPrintf("[OTA] %s manifest: HTTP %d\n", channelName(beta), outcome.httpCode);
    return outcome;
  }

  // Signature first: nothing in an unsigned or altered manifest is used.
  uint8_t sig[SIGNATURE_MAX_BYTES];
  size_t sigLen = 0;
  if (!manifestUrl(url, sizeof(url), OTA_REPO, beta, "manifest.sig")) return outcome;
  int code = download(url, sig, sizeof(sig), &sigLen);
  if (code != HTTP_CODE_OK && code != HTTP_CODE_NOT_FOUND && code != TOO_LARGE) {
    outcome.httpCode = code;
    outcome.result = FetchResult::HttpError;
    return outcome;
  }
  if (code != HTTP_CODE_OK || !verifySignature(OTA_PUBLIC_KEY_PEM, json.get(), jsonLen, sig, sigLen)) {
    logPrintf("[OTA] manifest signature %s: rejected\n", code == HTTP_CODE_OK ? "invalid" : "missing");
    outcome.result = FetchResult::BadSignature;
    return outcome;
  }

  outcome.manifestError = parseManifest(reinterpret_cast<const char*>(json.get()), jsonLen, FW_VARIANT, out);
  outcome.result = outcome.manifestError == ManifestError::None ? FetchResult::Ok : FetchResult::BadManifest;
  logPrintf("[OTA] %s manifest (signed): build %u (%s), file for %s: %s, error %d\n", channelName(beta),
                (unsigned)out.build, out.version, FW_VARIANT, out.hasFile ? out.fileName : "none",
                (int)outcome.manifestError);
  return outcome;
}

}  // namespace

FetchOutcome fetchManifest(bool beta, Manifest& out) {
  FetchOutcome outcome = fetchManifestOnce(beta, out);
  if (outcome.result == FetchResult::BadSignature) {
    // manifest.json and manifest.sig are two downloads: while a release is being published they
    // can briefly not belong together. Try the pair once more before calling it invalid.
    delay(5000);
    outcome = fetchManifestOnce(beta, out);
  }
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
    logPrintf("[OTA] download: HTTP %d\n", code);
    http.end();
    return InstallResult::DownloadFailed;
  }
  int size = http.getSize();
  if (size != (int)m.fileSize) {
    logPrintf("[OTA] download: size %d, manifest says %u\n", size, (unsigned)m.fileSize);
    http.end();
    return InstallResult::WrongSize;
  }
  if (!Update.begin(m.fileSize, U_FLASH)) {
    logPrintf("[OTA] no update slot: %s\n", Update.errorString());
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
      logPrintf("[OTA] write failed: %s\n", Update.errorString());
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
    logPrintf("[OTA] download stopped at %u of %u bytes\n", (unsigned)done, (unsigned)m.fileSize);
    Update.abort();
    return result;
  }

  if (progress) progress(InstallStage::Verifying, 4);
  char hex[65];
  toHex(digest, sizeof(digest), hex, sizeof(hex));
  if (strcmp(hex, m.sha256) != 0) {
    logPrintf("[OTA] SHA-256 mismatch: got %s\n", hex);
    Update.abort();
    return InstallResult::HashMismatch;
  }

  if (progress) progress(InstallStage::Installing, 4);
  // Checks the image and makes the new slot the boot slot. The bootloader keeps the old slot
  // until the new firmware confirms itself (rollback).
  if (!Update.end()) {
    logPrintf("[OTA] activate failed: %s\n", Update.errorString());
    return InstallResult::WriteFailed;
  }
  logPrintf("[OTA] installed %s (%u bytes), restart to run it\n", m.version, (unsigned)m.fileSize);
  return InstallResult::Ok;
}

const char* describe(InstallResult r, i18n::Lang lang) {
  using i18n::S;
  S key = S::InstUnknown;
  switch (r) {
    case InstallResult::Ok:             key = S::InstOk; break;
    case InstallResult::BadUrl:         key = S::InstBadUrl; break;
    case InstallResult::DownloadFailed: key = S::InstDownload; break;
    case InstallResult::WrongSize:      key = S::InstWrongSize; break;
    case InstallResult::NoSpace:        key = S::InstNoSpace; break;
    case InstallResult::HashMismatch:   key = S::InstHash; break;
    case InstallResult::WriteFailed:    key = S::InstWrite; break;
  }
  return i18n::text(key, lang);
}

}  // namespace ota
