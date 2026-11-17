#pragma once
// Debug log (DBGF in configure.h) with secret masking (security finding S6, Redact.h).
#include <stddef.h>

// Secrets to mask: pointers to buffers (the settings), so later changes are covered too.
void logRegisterSecrets(const char* const* secrets, size_t count);
// printf to the serial log, secrets masked. Lines longer than 511 characters are cut.
void logPrintf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
