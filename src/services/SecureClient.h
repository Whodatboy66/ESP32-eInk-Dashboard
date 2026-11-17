#pragma once
// HTTPS client that verifies the server certificate with the ESP-IDF certificate bundle (common
// CAs). Used for every HTTPS request (security finding S1 in docs/IMPLEMENTATION_PLAN.md).
//
// The core forgets the bundle whenever a connection is closed (stop_ssl_socket() wipes the whole
// TLS context), and HTTPClient reconnects for a redirect to another host. So the bundle is attached
// again before every connect (found and hardware-proven with GitHub's release downloads, stage 1).
#include <NetworkClientSecure.h>

class SecureClient : public NetworkClientSecure {
public:
  int connect(const char* host, uint16_t port, int32_t timeout) override;
  using NetworkClientSecure::connect;
};
