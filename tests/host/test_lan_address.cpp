// Host test for src/services/LanAddress.h. Run: tests/host/run.sh
#include "src/services/LanAddress.h"
#include "tests/host/check.h"

static void test_private_ipv4() {
  CHECK(isLocalNetworkHost("192.168.1.100"));
  CHECK(isLocalNetworkHost("10.0.0.5"));
  CHECK(isLocalNetworkHost("172.16.0.1"));
  CHECK(isLocalNetworkHost("172.31.255.254"));
  CHECK(isLocalNetworkHost(" 192.168.178.20 "));
}

static void test_public_or_odd_addresses() {
  CHECK(!isLocalNetworkHost("172.32.0.1"));
  CHECK(!isLocalNetworkHost("172.15.0.1"));
  CHECK(!isLocalNetworkHost("8.8.8.8"));
  CHECK(!isLocalNetworkHost("192.169.1.1"));
  CHECK(!isLocalNetworkHost("192.168.1"));       // incomplete
  CHECK(!isLocalNetworkHost("192.168.1.256"));   // out of range
  CHECK(!isLocalNetworkHost("192.168.1.1.1"));
  CHECK(!isLocalNetworkHost("10.0.0.5x"));
  CHECK(!isLocalNetworkHost(""));
  CHECK(!isLocalNetworkHost(nullptr));
}

static void test_mdns_names() {
  CHECK(isLocalNetworkHost("printer.local"));
  CHECK(isLocalNetworkHost("Bambu-X1C.LOCAL"));
  CHECK(!isLocalNetworkHost("printer.example.com"));
  CHECK(!isLocalNetworkHost(".local"));
  CHECK(!isLocalNetworkHost("evil.local.example.com"));
}

static void test_private_ipv4_octets() {
  CHECK(isPrivateIPv4(192, 168, 0, 1));
  CHECK(isPrivateIPv4(10, 1, 2, 3));
  CHECK(isPrivateIPv4(172, 20, 0, 1));
  CHECK(!isPrivateIPv4(172, 32, 0, 1));
  CHECK(!isPrivateIPv4(8, 8, 8, 8));
  CHECK(!isPrivateIPv4(0, 0, 0, 0));
}

static void test_hostnames() {
  CHECK(looksLikeHostname("bambu-x1c"));
  CHECK(looksLikeHostname("printer.fritz.box"));
  CHECK(looksLikeHostname("X1C.lan"));
  CHECK(!looksLikeHostname(""));
  CHECK(!looksLikeHostname("-printer"));
  CHECK(!looksLikeHostname("printer name"));
  CHECK(!looksLikeHostname("a..b"));
  CHECK(!looksLikeHostname("http://x"));
  CHECK(!looksLikeHostname("1.2.3.4"));  // that's an address, checked as one
}

int main() {
  test_private_ipv4_octets();
  test_hostnames();
  test_private_ipv4();
  test_public_or_odd_addresses();
  test_mdns_names();
  return finish("LAN address");
}
