#pragma once
// Masks secrets in a log line (security finding S6): every occurrence of a registered secret
// becomes "***". For a URL secret (a private iCal link) its path is masked on its own too, so a
// log line with only the path doesn't leak it; the host stays readable. Secrets shorter than 4
// characters are ignored (they would mask ordinary words). Longest first, so a secret that
// contains another is masked whole. Plain C++, tested on the PC (tests/host/test_redact.cpp).
#include <cstddef>
#include <cstring>

inline void redactSecrets(char* line, size_t cap, const char* const* secrets, size_t count) {
  if (!line || cap == 0 || !secrets) return;
  constexpr size_t MAX = 32;
  const char* cand[MAX];
  size_t len[MAX];
  size_t k = 0;
  for (size_t i = 0; i < count && k + 2 <= MAX; i++) {
    const char* s = secrets[i];
    if (!s) continue;
    size_t n = std::strlen(s);
    if (n < 4) continue;
    cand[k] = s;
    len[k++] = n;
    const char* scheme = std::strstr(s, "://");
    if (scheme) {
      const char* path = std::strchr(scheme + 3, '/');
      if (path && std::strlen(path) >= 8) {
        cand[k] = path;
        len[k++] = std::strlen(path);
      }
    }
  }
  // Longest first (insertion sort; a handful of entries).
  for (size_t i = 1; i < k; i++) {
    for (size_t j = i; j > 0 && len[j] > len[j - 1]; j--) {
      const char* c = cand[j]; cand[j] = cand[j - 1]; cand[j - 1] = c;
      size_t l = len[j]; len[j] = len[j - 1]; len[j - 1] = l;
    }
  }
  static const char MASK[] = "***";
  const size_t m = sizeof(MASK) - 1;
  for (size_t i = 0; i < k; i++) {
    char* p = line;
    while ((p = std::strstr(p, cand[i])) != nullptr) {
      // len >= 4 > m: the line only gets shorter, so the buffer never overflows.
      std::memcpy(p, MASK, m);
      std::memmove(p + m, p + len[i], std::strlen(p + len[i]) + 1);
      p += m;
    }
  }
}
