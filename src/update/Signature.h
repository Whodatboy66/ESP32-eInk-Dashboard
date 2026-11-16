#pragma once
// ECDSA P-256 signature check of the release manifest (docs/OTA_UPDATES.md, "Signing").
// The CI release job signs manifest.json with the private key (a GitHub secret):
//   openssl dgst -sha256 -sign key.pem -out manifest.sig manifest.json
// and the device checks manifest.sig with the public key built into the firmware
// (src/update/OtaPublicKey.h). No Arduino headers: tested on the PC (tests/host/test_signature.cpp).

#include <cstddef>
#include <cstdint>

namespace ota {

// True if `sig` (DER-encoded ECDSA signature over SHA-256 of `data`) was made with the private
// key belonging to `publicKeyPem` (an EC P-256 public key in PEM format).
bool verifySignature(const char* publicKeyPem, const uint8_t* data, size_t len, const uint8_t* sig,
                     size_t sigLen);

}  // namespace ota
