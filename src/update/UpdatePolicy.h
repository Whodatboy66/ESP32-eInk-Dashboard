#pragma once
// Over-the-air update decisions (docs/OTA_UPDATES.md): is a release newer and meant for this
// board, may it be installed now (battery guard), is the night-time check due, what does the
// night-time check do (auto install or notify only). Plus small helpers for release asset names,
// URLs, SHA-256 hex strings and display-safe "What's new" lines.
//
// Plain C++ without Arduino headers, tested on the PC (tests/host/test_update_policy.cpp).

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace ota {

enum class Availability : uint8_t {
  UpToDate,            // the release is not newer than the running firmware
  Newer,               // a newer release for this board, installable
  NeedsNewerFirmware,  // newer, but needs at least min_build (install an intermediate release by USB)
  NotForThisBoard,     // newer, but the release has no file for this board variant
  WrongDisplay,        // newer, but its file is built for another display
  IncompatibleSettings // newer, but it can't read this device's settings (config schema too new)
};

// Test builds of the "OTA test release" workflow (docs/OTA_TESTS.md) are numbered from 90001 on, so
// they count as newer than any release. A device left on one (a test build that got confirmed)
// must still get back to the regular releases: for it, every regular release counts as newer.
constexpr uint32_t FIRST_TEST_BUILD = 90000;
inline bool isTestBuild(uint32_t build) { return build >= FIRST_TEST_BUILD; }

inline Availability availability(uint32_t remoteBuild, uint32_t minBuild, bool hasFileForVariant,
                                 uint32_t currentBuild) {
  bool leaveTestBuild = isTestBuild(currentBuild) && !isTestBuild(remoteBuild) && remoteBuild > 0;
  if (remoteBuild <= currentBuild && !leaveTestBuild) return Availability::UpToDate;  // never a downgrade
  if (!hasFileForVariant) return Availability::NotForThisBoard;
  if (currentBuild < minBuild) return Availability::NeedsNewerFirmware;
  return Availability::Newer;
}

// Below minPercent no install, unless USB is connected. Without a battery sensor the board is
// assumed to run on USB.
inline bool batteryAllowsInstall(bool hasBatterySensor, int percent, bool usbPowered, int minPercent) {
  return !hasBatterySensor || usbPowered || percent >= minPercent;
}

// One number per calendar day (year * 1000 + day of the year, 0-365).
inline int dayKey(int year, int yday) { return year * 1000 + yday; }

// The night-time check runs once per day, during nightHour (local time).
// Once per day, from nightHour until noon: normally at the first wake after 03:00; with night quiet
// hours over 03:00 (stage 6) at the first wake after them, and after a failed Wi-Fi at 03:00 at the
// next Wi-Fi wake instead of a day later.
inline bool nightCheckDue(int hour, int todayKey, int lastCheckKey, int nightHour) {
  return hour >= nightHour && hour < 12 && todayKey != lastCheckKey;
}

enum class NightAction : uint8_t {
  Nothing,        // no installable update
  Install,        // auto install on: install now
  NotifyOnly,     // auto install off: show "Update available" in the status bar
  WaitForCharge   // auto install on, battery too low: show the mark, try again next night
};

// The build that was installed but rolled back (failed to start or to reach Wi-Fi) is not
// auto-installed again: that would repeat every night, and every few minutes within the night
// hour. Only that exact build: the next one is the fix (and test builds far above the real ones,
// docs/OTA_TESTS.md, must not block them). 0 = no failed build.
inline bool failedBefore(uint32_t remoteBuild, uint32_t failedBuild) {
  return failedBuild != 0 && remoteBuild == failedBuild;
}

inline NightAction nightAction(Availability a, bool autoInstall, bool batteryOk, bool failedBefore = false) {
  if (a != Availability::Newer) return NightAction::Nothing;
  if (!autoInstall || failedBefore) return NightAction::NotifyOnly;
  return batteryOk ? NightAction::Install : NightAction::WaitForCharge;
}

// Release tag / asset name: letters, digits, '.', '_', '-' only (no path, no query), 1-63 chars,
// not starting with '.'.
inline bool isSafeName(const char* s) {
  if (!s || !*s || *s == '.') return false;
  size_t n = 0;
  for (; s[n]; n++) {
    char c = s[n];
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '.' || c == '_' || c == '-';
    if (!ok || n >= 63) return false;
  }
  return true;
}

// Release channels: "stable" reads the newest regular release (GitHub's "latest" skips
// pre-releases); "beta" reads the fixed pre-release "beta", whose manifest CI replaces with every
// release (beta and stable), so it always names the newest build of either kind.
inline const char* channelName(bool beta) { return beta ? "beta" : "stable"; }

inline bool manifestUrl(char* out, size_t cap, const char* repo, bool beta, const char* file) {
  int n = beta ? std::snprintf(out, cap, "https://github.com/%s/releases/download/beta/%s", repo, file)
               : std::snprintf(out, cap, "https://github.com/%s/releases/latest/download/%s", repo, file);
  return n > 0 && static_cast<size_t>(n) < cap;
}

// https://github.com/<repo>/releases/download/<tag>/<name>; false if a part is unsafe or too long.
inline bool buildAssetUrl(char* out, size_t cap, const char* repo, const char* tag, const char* name) {
  if (!isSafeName(tag) || !isSafeName(name)) return false;
  int n = std::snprintf(out, cap, "https://github.com/%s/releases/download/%s/%s", repo, tag, name);
  return n > 0 && static_cast<size_t>(n) < cap;
}

// Update requests may only connect to GitHub and its file host (releases redirect there).
inline bool isAllowedHost(const char* host) {
  if (!host || !*host) return false;
  if (std::strcmp(host, "github.com") == 0) return true;
  static const char suffix[] = ".githubusercontent.com";
  size_t n = std::strlen(host), m = sizeof(suffix) - 1;
  return n > m && std::strcmp(host + n - m, suffix) == 0;
}

inline bool isSha256Hex(const char* s) {
  if (!s || std::strlen(s) != 64) return false;
  for (const char* p = s; *p; p++) {
    if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f'))) return false;
  }
  return true;
}

inline void toHex(const uint8_t* data, size_t len, char* out, size_t cap) {
  static const char digits[] = "0123456789abcdef";
  size_t o = 0;
  for (size_t i = 0; i < len && o + 2 < cap; i++) {
    out[o++] = digits[data[i] >> 4];
    out[o++] = digits[data[i] & 0x0F];
  }
  if (cap) out[o] = '\0';
}

// Makes a release-notes line displayable with the e-paper font (which has no ( ) ! ' " ; & etc.):
// drops or replaces those characters, turns line breaks and tabs into spaces, collapses spaces and
// trims, and cuts to cap-1 bytes without splitting a UTF-8 character.
inline void sanitizeNote(const char* in, char* out, size_t cap) {
  if (!cap) return;
  size_t o = 0;
  bool space = true;  // drops leading spaces
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(in); *p; p++) {
    unsigned char c = *p;
    char rep = 0;
    if (c >= 0x80) {
      rep = static_cast<char>(c);  // UTF-8 byte: the font has a few accented letters
    } else if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
      rep = ' ';
    } else if (c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' ||
               c == '\'' || c == '"' || c == '`') {
      continue;
    } else if (c == '!') {
      rep = '.';
    } else if (c == '&') {
      rep = '+';
    } else if (c == ';') {
      rep = ',';
    } else if (c < 0x20 || c == 0x7F) {
      continue;
    } else if (c == '$' || c == '<' || c == '>' || c == '@' || c == '\\' || c == '^' || c == '|') {
      rep = ' ';
    } else {
      rep = static_cast<char>(c);
    }
    if (rep == ' ') {
      if (space) continue;
      space = true;
    } else {
      space = false;
    }
    if (o + 1 >= cap) break;
    out[o++] = rep;
  }
  // Don't end in the middle of a UTF-8 character: drop a trailing incomplete sequence.
  size_t end = o;
  size_t lead = end;
  while (lead > 0 && (static_cast<unsigned char>(out[lead - 1]) & 0xC0) == 0x80) lead--;
  if (lead > 0 && static_cast<unsigned char>(out[lead - 1]) >= 0xC0) {
    unsigned char b = static_cast<unsigned char>(out[lead - 1]);
    size_t need = b >= 0xF0 ? 4 : b >= 0xE0 ? 3 : 2;
    if (end - (lead - 1) < need) end = lead - 1;
  }
  while (end > 0 && out[end - 1] == ' ') end--;
  out[end] = '\0';
}

// Download progress in quarters (0-4): the e-paper bar is only redrawn when this changes.
inline int progressStep(uint32_t done, uint32_t total) {
  if (total == 0) return 0;
  if (done >= total) return 4;
  return static_cast<int>(static_cast<uint64_t>(done) * 4 / total);
}

}  // namespace ota
