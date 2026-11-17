#pragma once
// The one TLS connection without a certificate check is the Bambu printer's local MQTT (the
// printer only has a self-signed certificate). It is allowed only to an address in the local
// network: a private IPv4 address (10/8, 172.16/12, 192.168/16), an mDNS ".local" name, or a name
// whose resolved address is private (checked when connecting, src/services/Bambu.cpp).
// Plain C++, tested on the PC (tests/host/test_lan_address.cpp).
#include <cctype>
#include <cstring>

inline bool isPrivateIPv4(int a, int b, int, int) {
  return a == 10 || (a == 172 && b >= 16 && b <= 31) || (a == 192 && b == 168);
}

// A DNS name (labels of letters, digits and '-', not starting or ending with '-'), not an address.
inline bool looksLikeHostname(const char* s) {
  if (!s || !*s) return false;
  size_t n = std::strlen(s);
  if (n > 253) return false;
  bool onlyDigitsAndDots = true;
  size_t label = 0;
  for (size_t i = 0; i <= n; i++) {
    char c = s[i];
    if (c == '.' || c == '\0') {
      if (label == 0 || s[i - 1] == '-') return false;
      label = 0;
      continue;
    }
    bool ok = std::isalnum(static_cast<unsigned char>(c)) || c == '-';
    if (!ok || (label == 0 && c == '-') || ++label > 63) return false;
    if (!std::isdigit(static_cast<unsigned char>(c))) onlyDigitsAndDots = false;
  }
  return !onlyDigitsAndDots;
}

inline bool isLocalNetworkHost(const char* host) {
  if (!host) return false;
  while (*host == ' ') host++;
  size_t n = std::strlen(host);
  while (n && host[n - 1] == ' ') n--;
  if (n == 0) return false;

  // mDNS name: "<name>.local"
  static const char suffix[] = ".local";
  const size_t m = sizeof(suffix) - 1;
  if (n > m) {
    bool match = true;
    for (size_t i = 0; i < m && match; i++)
      match = std::tolower(static_cast<unsigned char>(host[n - m + i])) == suffix[i];
    if (match) return true;
  }

  // Dotted IPv4 with exactly four parts 0-255.
  int part[4];
  int count = 0;
  size_t i = 0;
  while (i < n && count < 4) {
    if (!std::isdigit(static_cast<unsigned char>(host[i]))) return false;
    int v = 0, digits = 0;
    while (i < n && std::isdigit(static_cast<unsigned char>(host[i]))) {
      v = v * 10 + (host[i] - '0');
      if (++digits > 3 || v > 255) return false;
      i++;
    }
    part[count++] = v;
    if (i < n) {
      if (host[i] != '.' || count == 4) return false;
      i++;
      if (i == n) return false;
    }
  }
  if (count != 4 || i != n) return false;
  return isPrivateIPv4(part[0], part[1], part[2], part[3]);
}
