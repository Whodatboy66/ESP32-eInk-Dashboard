// Host test for src/i18n/ (texts in German and English, dates per language): every text exists in
// both languages with the same printf conversions, uses only characters of the screen font, and
// the texts drawn in fixed places fit there (measured with the screen font). Run: tests/host/run.sh
#include <cstdio>
#include <string>

#include "src/i18n/Dates.h"
#include "src/i18n/Strings.h"
#include "tests/host/check.h"

// The screen font (src/ui/fonts/epaperFont.h) with the glyph type of tests/host/shim/Adafruit_GFX.h.
#include <Adafruit_GFX.h>
struct SparseGFXfont {
  const uint8_t* bitmap;
  const GFXglyph* glyph;
  const uint32_t* charMap;
  uint16_t glyphCount;
  uint8_t yAdvance;
};
#include "src/ui/fonts/epaperFont.h"

using i18n::Lang;
using i18n::S;

namespace {

uint32_t nextCodepoint(const char*& p) {
  const unsigned char* s = reinterpret_cast<const unsigned char*>(p);
  uint32_t cp;
  int n;
  if (s[0] < 0x80) cp = s[0], n = 1;
  else if ((s[0] & 0xE0) == 0xC0) cp = s[0] & 0x1F, n = 2;
  else if ((s[0] & 0xF0) == 0xE0) cp = s[0] & 0x0F, n = 3;
  else cp = s[0] & 0x07, n = 4;
  for (int i = 1; i < n; i++) cp = (cp << 6) | (s[i] & 0x3F);
  p += n;
  return cp;
}

const GFXglyph* glyphOf(uint32_t cp) {
  for (int i = 0; i < epaperFont.glyphCount; i++)
    if (epaperFont.charMap[i] == cp) return &epaperFont.glyph[i];
  return nullptr;
}

// As getSparseStringWidth() (src/ui/SparseFont.cpp).
int width(const char* s) {
  int w = 0;
  for (const char* p = s; *p;) {
    const GFXglyph* g = glyphOf(nextCodepoint(p));
    w += g ? g->xAdvance : epaperFont.yAdvance / 2;
  }
  return w;
}

// The first character the font can't draw, 0 if none.
uint32_t missingGlyph(const char* s) {
  for (const char* p = s; *p;) {
    uint32_t cp = nextCodepoint(p);
    if (cp != ' ' && !glyphOf(cp)) return cp;
  }
  return 0;
}

// "%s", "%02ld", "%%" ...: the conversions of a printf format, in order.
std::string conversions(const char* f) {
  std::string out;
  for (const char* p = f; *p; p++) {
    if (*p != '%') continue;
    const char* q = p + 1;
    while (*q && !std::strchr("sdiuxXfgcp%", *q)) q++;
    out.append(p, q - p + 1);
    out += ' ';
    p = q;
  }
  return out;
}

const Lang LANGS[] = {Lang::De, Lang::En};

void test_every_text() {
  for (int i = 0; i < i18n::TEXT_COUNT; i++) {
    S key = static_cast<S>(i);
    const char* de = i18n::text(key, Lang::De);
    const char* en = i18n::text(key, Lang::En);
    if (!de[0] || !en[0]) {
      std::printf("text %d: empty\n", i);
      CHECK(de[0] && en[0]);
    }
    if (conversions(de) != conversions(en)) {
      std::printf("text %d: \"%s\" / \"%s\": different conversions\n", i, de, en);
      CHECK(conversions(de) == conversions(en));
    }
    for (Lang l : LANGS) {
      if (i >= static_cast<int>(i18n::FIRST_APP_TEXT)) break;  // only shown in the setup app
      uint32_t cp = missingGlyph(i18n::text(key, l));
      if (cp) std::printf("text %d \"%s\": U+%04X not in the screen font\n", i, i18n::text(key, l), (unsigned)cp);
      CHECK(cp == 0);
    }
  }
  CHECK_STR(i18n::text(S::Count, Lang::De), "");  // "no text" (Registry.h NO_HINT)
  CHECK(i18n::parseLang("en") == Lang::En);
  CHECK(i18n::parseLang("de") == Lang::De);
  CHECK(i18n::parseLang("") == Lang::De);  // devices from before build 78
  CHECK(i18n::parseLang("fr") == Lang::De);
  CHECK(i18n::parseLang(nullptr) == Lang::De);
  CHECK_STR(i18n::langCode(Lang::En), "en");
  char n[16] = "98.12";
  i18n::localizeNumber(n, Lang::En);
  CHECK_STR(n, "98.12");
  i18n::localizeNumber(n, Lang::De);
  CHECK_STR(n, "98,12");
}

std::tm day(int y, int m, int d) {
  std::tm t{};
  t.tm_year = y - 1900;
  t.tm_mon = m - 1;
  t.tm_mday = d;
  t.tm_hour = 12;
  std::mktime(&t);  // fills tm_wday
  return t;
}

void test_dates() {
  char b[48];
  std::tm fri = day(2026, 11, 20), mar = day(2027, 3, 4), sun = day(2026, 11, 22);
  i18n::formatStatusDate(fri, Lang::De, b, sizeof(b));
  CHECK_STR(b, "Fr 20. Nov");
  i18n::formatStatusDate(fri, Lang::En, b, sizeof(b));
  CHECK_STR(b, "Fri 20 Nov");
  i18n::formatStatusDate(mar, Lang::De, b, sizeof(b));
  CHECK_STR(b, "Do 4. Mär");
  i18n::formatDayHeader(fri, Lang::De, b, sizeof(b));
  CHECK_STR(b, "Freitag, 20. November");
  i18n::formatDayHeader(fri, Lang::En, b, sizeof(b));
  CHECK_STR(b, "Friday, 20 November");
  i18n::formatDayHeader(mar, Lang::De, b, sizeof(b));
  CHECK_STR(b, "Donnerstag, 4. März");
  i18n::formatDayHeader(sun, Lang::En, b, sizeof(b));
  CHECK_STR(b, "Sunday, 22 November");
  i18n::formatMonthTitle(2026, 2, Lang::De, b, sizeof(b));
  CHECK_STR(b, "März 2026");
  i18n::formatMonthTitle(2026, 11, Lang::En, b, sizeof(b));
  CHECK_STR(b, "December 2026");
  i18n::formatShortDate(fri, Lang::De, b, sizeof(b));
  CHECK_STR(b, "20.11.");
  i18n::formatShortDate(mar, Lang::En, b, sizeof(b));
  CHECK_STR(b, "Mar 4");
  CHECK_STR(i18n::weekdayShort(fri.tm_wday, Lang::De), "Fr");
  CHECK_STR(i18n::weekdayShort(sun.tm_wday, Lang::En), "Sun");
  CHECK_STR(i18n::weekdayLong(0, Lang::De), "Sonntag");
  CHECK_STR(i18n::monthLong(9, Lang::De), "Oktober");
  const char* deRow[] = {"Mo", "Di", "Mi", "Do", "Fr", "Sa", "So"};
  const char* enRow[] = {"M", "T", "W", "T", "F", "S", "S"};
  for (int i = 0; i < 7; i++) {
    CHECK_STR(i18n::weekdayInitial(i, Lang::De), deRow[i]);
    CHECK_STR(i18n::weekdayInitial(i, Lang::En), enRow[i]);
  }
  i18n::formatCountdown(45 * 60, Lang::De, b, sizeof(b));
  CHECK_STR(b, "in 45m");
  i18n::formatCountdown(3 * 3600 + 5 * 60, Lang::En, b, sizeof(b));
  CHECK_STR(b, "in 3h05m");
  i18n::formatCountdown(86400 + 2 * 3600, Lang::De, b, sizeof(b));
  CHECK_STR(b, "in 1T 2h");
  i18n::formatCountdown(86400 + 2 * 3600, Lang::En, b, sizeof(b));
  CHECK_STR(b, "in 1d 2h");
}

// Fits within maxWidth pixels in both languages (formatted with the sample arguments).
template <typename... Args>
void fits(S key, int maxWidth, Args... args) {
  for (Lang l : LANGS) {
    char b[256];
    if constexpr (sizeof...(Args) == 0) std::snprintf(b, sizeof(b), "%s", i18n::text(key, l));
    else std::snprintf(b, sizeof(b), i18n::text(key, l), args...);
    int w = width(b);
    if (w > maxWidth) std::printf("\"%s\": %d px, room for %d\n", b, w, maxWidth);
    CHECK(w <= maxWidth);
  }
}

void fitsText(const char* s, int maxWidth) {
  int w = width(s);
  if (w > maxWidth) std::printf("\"%s\": %d px, room for %d\n", s, w, maxWidth);
  CHECK(w <= maxWidth);
}

// The places (pixels) as drawn in src/widgets, src/web/SetupScreen.cpp, src/update/UpdateScreens.cpp.
void test_fits_on_screen() {
  // Status bar in Classic (360 px): a note is drawn only if it ends 8 px before the version text
  // (a two-digit build); right of that the battery icon (22) and "100 %" (StatusBar.cpp).
  const int barRoom = 360 - (4 + width("100 %") + 4 + 22) - width("v.99") - 8;
  fits(S::NoteNight, barRoom, "06:00");
  fits(S::NoteSaver, barRoom, 15);
  fits(S::NoteClock, barRoom, 30);
  // ...and "Update available" in its place (then no note).
  fits(S::UpdateAvailable, 360 - (4 + width("100 %") + 4 + 22), ".100");
  // Problem names next to each other after the alert icon.
  for (S k : {S::ProbWifi, S::ProbTime, S::ProbWeather, S::ProbStocks, S::ProbCalendar, S::ProbPrinter,
              S::ProbPrinterCode, S::ProbMakerWorld, S::ProbTracking, S::ProbFlight, S::ProbBirthdays})
    fits(k, 110);
  // Status bar date.
  for (Lang l : LANGS)
    for (int wd = 0; wd < 7; wd++)
      for (int m = 0; m < 12; m++) {
        char b[24];
        std::tm t{};
        t.tm_wday = wd, t.tm_mon = m, t.tm_mday = 28;
        i18n::formatStatusDate(t, l, b, sizeof(b));
        fitsText(b, 120);
        i18n::formatDayHeader(t, l, b, sizeof(b));
        fitsText(b, 356);  // Classic's agenda, 360 px
        i18n::formatMonthTitle(2026, m, l, b, sizeof(b));
        fitsText(b, 200);
        fitsText(i18n::weekdayShort(wd, l), 40);    // above a weather icon (80 px)
        fitsText(i18n::weekdayInitial(wd, l), 28);  // month view column (Classic: 51 px)
      }

  // Widgets.
  fits(S::Unavailable1, 190);  // centered in a one-cell box (196 px)
  fits(S::Unavailable2, 190);
  fits(S::Stale, 34);
  for (S k : {S::MwLikes, S::MwDownloads, S::MwPrints, S::MwCollections, S::MwBoosts})
    fits(k, 100 + width("  ") - 4);  // a number right-aligned in 7 places 100 px further (up to 5 digits)
  fits(S::PrinterDone, 72, 100);  // the bed temperature follows 75 px further
  fits(S::PrinterEmpty, 60);
  fits(S::BdayToday, 92 - 10);  // birthday names start at 92 px; "Heute" in a black box (+6)
  fits(S::BdayTomorrow, 92 - 6);
  fits(S::BdayInDays, 92 - 6, 14);
  fits(S::BdayNone, 380);
  fits(S::TrackNone, 350);
  fits(S::TasksNone, 190);
  fits(S::TasksAllDone, 190);
  fits(S::NoteNone, 190);
  for (S k : {S::TrackChecking, S::TrackNoInfo, S::TrackInTransit, S::TrackPickup, S::TrackNotDelivered,
              S::TrackError, S::TrackPreparing, S::TrackEnded, S::TrackUnknown})
    for (Lang l : LANGS) {  // Classic's parcel line: icon (28 px) + 332 px
      char b[96];
      std::snprintf(b, sizeof(b), "12345678  %s  %s 30.12.", i18n::text(k, l), i18n::text(S::TrackEta, l));
      fitsText(b, 332);
    }

  // Charge and update screens: centered on the 800 px screen.
  for (S k : {S::ChargeTitle, S::ChargeBack, S::UpdDeviceInfo, S::UpdChecking, S::UpdInstalling, S::UpdStepVerify,
              S::UpdStepInstall, S::UpdStepRestart, S::UpdKeepPowered, S::UpdTitle, S::UpdBackSoon,
              S::UpdDashboardFollows, S::UpdNewInVersion, S::OtaPostponed, S::OtaStillStarting, S::OtaFailedKept,
              S::OtaBadSignature, S::OtaNoMemory, S::OtaNoGithub, S::OtaNoWifi, S::OtaSetUpWifi, S::OtaCheckFailed,
              S::OtaNoUpdate, S::OtaNeedsUsb, S::OtaBatteryLow, S::OtaDidNotStart, S::InstNoSpace, S::InstHash})
    fits(k, 760);
  fits(S::UpdChannel, 760, "stable", "off");
  fits(S::UpdBattery, 760, 100, " - USB connected");
  fits(S::UpdTapToInstall, 760, 30u);
  fits(S::OtaWrongDisplay, 760, ".100", "bw750", "color750");
  fits(S::OtaBadSettings, 760, ".100");
  fits(S::OtaNeedsNewer, 760, ".100", 100u);
  fits(S::OtaBatteryDetail, 760, 100, 30);
  fits(S::OtaNoFile, 760, ".100", "esp32c6_supermini");
  fits(S::OtaNoRelease, 760, "stable");

  // Setup screens: the hotspot screen's text left of its QR code (x 40 to 460), the edit screen's
  // left of x 520; the edit screen's timeout under the QR code, from x 520 to the edge.
  for (S k : {S::SetupTitle, S::SetupStep1, S::SetupStep1b, S::SetupStep2, S::SetupStep2b}) fits(k, 410);
  fits(S::SetupWifi, 410, "Dashboard-Setup");
  fits(S::SetupPassword, 410, "wwwwwwww");
  fits(S::SetupTimeout, 720);
  for (S k : {S::EditTitle, S::EditLine1, S::EditLine2, S::EditCode}) fits(k, 470);
  fits(S::EditOr, 470, "192.168.178.123");
  fits(S::EditTimeout, 800 - 520 - 4);
}

}  // namespace

int main() {
  test_every_text();
  test_dates();
  test_fits_on_screen();
  return finish("i18n");
}
