// Host test for src/update/Signature.cpp (needs mbedTLS, found by tests/host/run.sh).
// needs: mbedtls
//
// Fixtures: a throwaway test key (private half not kept), and a signature made exactly like the
// CI release job does it: openssl dgst -sha256 -sign key.pem -out msg.sig msg.json
#include "src/update/Signature.cpp"
#include "src/update/OtaPublicKey.h"
#include "tests/host/check.h"

#include <string>
#include <vector>

static const char kTestKey[] =
    "-----BEGIN PUBLIC KEY-----\n"
    "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEUEO/SUc924fmImaYxY8iFuSh4qNj\n"
    "+Y3w9AHDPmsg9zVVzpOwdgPDPVQ8dt4v462MFT0FTEBBisgL43/2r9+KMQ==\n"
    "-----END PUBLIC KEY-----\n";

static const char kOtherKey[] =
    "-----BEGIN PUBLIC KEY-----\n"
    "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE5KfNM0QiSXFhvZog+ppA4d8jAMIU\n"
    "hOXasSv7vLdLIR8iCU/s9uGZJC28oC5vZmlo6KoRxGIQMafkUFiOKNHHpw==\n"
    "-----END PUBLIC KEY-----\n";

static const char kMessage[] = "{\"schema\":1,\"build\":66}";

static const char kSigHex[] =
    "3046022100de35c13f4e0996aa6bdc9793481f2788814e09a5ae907150a89360befc446dd7022100897693ccda79"
    "ffdc7d3d3452095dca310d69d296a1003de13269f146a996f162";

static std::vector<uint8_t> fromHex(const char* hex) {
  std::vector<uint8_t> out;
  for (size_t i = 0; hex[i] && hex[i + 1]; i += 2) {
    out.push_back(static_cast<uint8_t>(std::stoi(std::string(hex + i, 2), nullptr, 16)));
  }
  return out;
}

static bool verify(const char* key, const std::string& msg, const std::vector<uint8_t>& sig) {
  return ota::verifySignature(key, reinterpret_cast<const uint8_t*>(msg.data()), msg.size(), sig.data(),
                              sig.size());
}

static void test_valid_signature() {
  CHECK(verify(kTestKey, kMessage, fromHex(kSigHex)));
}

static void test_changed_manifest_is_rejected() {
  std::string msg = kMessage;
  msg[msg.size() - 2] = '7';  // build 66 -> 67
  CHECK(!verify(kTestKey, msg, fromHex(kSigHex)));
  CHECK(!verify(kTestKey, std::string(kMessage) + " ", fromHex(kSigHex)));
}

static void test_changed_signature_is_rejected() {
  std::vector<uint8_t> sig = fromHex(kSigHex);
  sig[10] ^= 0x01;
  CHECK(!verify(kTestKey, kMessage, sig));
  sig = fromHex(kSigHex);
  sig.resize(sig.size() - 1);
  CHECK(!verify(kTestKey, kMessage, sig));
}

static void test_other_key_is_rejected() {
  CHECK(!verify(kOtherKey, kMessage, fromHex(kSigHex)));
}

static void test_garbage_is_rejected() {
  CHECK(!verify(kTestKey, kMessage, {}));
  CHECK(!verify("not a key", kMessage, fromHex(kSigHex)));
  CHECK(!verify("", kMessage, fromHex(kSigHex)));
  CHECK(!ota::verifySignature(kTestKey, nullptr, 0, nullptr, 0));
}

static void test_firmware_public_key_is_a_p256_key() {
  // A broken key in the firmware would make every device reject every release.
  mbedtls_pk_context key;
  mbedtls_pk_init(&key);
  int rc = mbedtls_pk_parse_public_key(&key, reinterpret_cast<const unsigned char*>(OTA_PUBLIC_KEY_PEM),
                                       sizeof(OTA_PUBLIC_KEY_PEM));
  CHECK_EQ(rc, 0);
  CHECK(mbedtls_pk_can_do(&key, MBEDTLS_PK_ECKEY));
  CHECK_EQ(mbedtls_pk_get_bitlen(&key), 256u);
  mbedtls_pk_free(&key);
  // ... and it is not the test key: the fixture signature must not pass with it.
  CHECK(!verify(OTA_PUBLIC_KEY_PEM, kMessage, fromHex(kSigHex)));
}

int main() {
  test_valid_signature();
  test_changed_manifest_is_rejected();
  test_changed_signature_is_rejected();
  test_other_key_is_rejected();
  test_garbage_is_rejected();
  test_firmware_public_key_is_a_p256_key();
  return finish("signature");
}
