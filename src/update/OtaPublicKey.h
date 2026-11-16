#pragma once
// Public key for OTA manifest signatures (ECDSA P-256), made by tools/ota/make_signing_key.sh.
// The matching private key is the GitHub secret OTA_SIGNING_KEY (never in this repository).
// Changing this key locks out devices with the old key until they are flashed over USB.
static const char OTA_PUBLIC_KEY_PEM[] =
    "-----BEGIN PUBLIC KEY-----\n"
    "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE7zFczH9omaKB5Rvb23eVm2D+Y1pt\n"
    "spPb8Eo0uggvhnOcWFC97r48l1rU0a5UAosD/EtReFK7RrqF1Gv2TlS2mg==\n"
    "-----END PUBLIC KEY-----\n"
    ;
