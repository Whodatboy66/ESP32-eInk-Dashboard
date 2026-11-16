// Minimal check macros for the host tests (no test framework needed).
#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>

static int failures = 0;

#define CHECK(cond)                                                              \
  do {                                                                           \
    if (!(cond)) {                                                               \
      std::printf("%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond);      \
      ++failures;                                                                \
    }                                                                            \
  } while (0)

#define CHECK_EQ(actual, expected)                                                        \
  do {                                                                                    \
    auto a_ = (actual);                                                                   \
    auto e_ = (expected);                                                                 \
    if (!(a_ == e_)) {                                                                    \
      std::printf("%s:%d: CHECK_EQ(%s, %s) failed: %ld != %ld\n", __FILE__, __LINE__,    \
                  #actual, #expected, static_cast<long>(a_), static_cast<long>(e_));     \
      ++failures;                                                                         \
    }                                                                                     \
  } while (0)

#define CHECK_STR(actual, expected)                                                      \
  do {                                                                                   \
    const char* a_ = (actual);                                                           \
    const char* e_ = (expected);                                                         \
    if (std::strcmp(a_, e_) != 0) {                                                      \
      std::printf("%s:%d: CHECK_STR(%s) failed: \"%s\" != \"%s\"\n", __FILE__, __LINE__, \
                  #actual, a_, e_);                                                      \
      ++failures;                                                                        \
    }                                                                                    \
  } while (0)

static int finish(const char* name) {
  if (failures) {
    std::printf("FAILED: %d check(s)\n", failures);
    return EXIT_FAILURE;
  }
  std::printf("All %s tests passed\n", name);
  return EXIT_SUCCESS;
}
