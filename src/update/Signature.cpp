#include "Signature.h"

#include <cstring>

#include <mbedtls/md.h>
#include <mbedtls/pk.h>

namespace ota {

bool verifySignature(const char* publicKeyPem, const uint8_t* data, size_t len, const uint8_t* sig,
                     size_t sigLen) {
  if (!publicKeyPem || !*publicKeyPem || !data || !sig || sigLen == 0) return false;

  uint8_t hash[32];
  const mbedtls_md_info_t* sha256 = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!sha256 || mbedtls_md(sha256, data, len, hash) != 0) return false;

  mbedtls_pk_context key;
  mbedtls_pk_init(&key);
  // PEM input: the length includes the terminating NUL.
  bool ok = mbedtls_pk_parse_public_key(&key, reinterpret_cast<const unsigned char*>(publicKeyPem),
                                        std::strlen(publicKeyPem) + 1) == 0 &&
            mbedtls_pk_can_do(&key, MBEDTLS_PK_ECKEY) &&
            mbedtls_pk_verify(&key, MBEDTLS_MD_SHA256, hash, sizeof(hash), sig, sigLen) == 0;
  mbedtls_pk_free(&key);
  return ok;
}

}  // namespace ota
