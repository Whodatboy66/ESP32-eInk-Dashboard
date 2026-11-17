#include "SecureClient.h"

// The ESP-IDF default certificate bundle, linked into the core's mbedTLS library.
extern const uint8_t x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_bundle_end[] asm("_binary_x509_crt_bundle_end");

int SecureClient::connect(const char* host, uint16_t port, int32_t timeout) {
  setCACertBundle(x509_crt_bundle_start, x509_crt_bundle_end - x509_crt_bundle_start);
  return NetworkClientSecure::connect(host, port, timeout);
}
