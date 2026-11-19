#include "SetupScreen.h"

#include <Arduino.h>
#include <SPI.h>
#include <stdio.h>
#include <string.h>

#include "../hardware/EpaperDriver.h"
#include "../hardware/PinMapping.h"
#include "../ui/Canvas.h"
#include "../ui/Fonts.h"
#include "../ui/SparseFont.h"
#include "../ui/qr/qrcodegen.h"

namespace {

constexpr int QR_MAX_VERSION = 8;  // up to ~150 characters: Wi-Fi strings and addresses

// Wi-Fi QR strings escape \ ; , : " with a backslash.
void appendEscaped(char* out, size_t cap, const char* s) {
  size_t n = strlen(out);
  for (; *s && n + 2 < cap; s++) {
    if (strchr("\\;,:\"", *s)) out[n++] = '\\';
    out[n++] = *s;
  }
  out[n] = '\0';
}

// Full refresh of the whole screen with `draw`, then the panel sleeps.
void showScreen(void (*draw)(const void*), const void* ctx) {
  if (hasDisplayPowerPin()) {
    pinMode(activePins.displayPower, OUTPUT);
    digitalWrite(activePins.displayPower, HIGH);
    delay(50);
  }
  SPI.begin(activePins.epdSck, -1, activePins.epdMosi, activePins.epdCs);
  display.init(115200, true, 2, false);
  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();
  do {
    canvas().fillScreen(GxEPD_WHITE);
    draw(ctx);
  } while (display.nextPage());
  display.hibernate();
}

struct ApInfo {
  const char* ssid;
  const char* password;
};
struct HomeInfo {
  const char* host;
  const char* ip;
  const char* code;
};

void drawLines(int x, int y, const char* const* lines, int n) {
  for (int i = 0; i < n; i++) drawSparseString(&epaperFont, x, y + i * 30, lines[i], GxEPD_BLACK);
}

void drawAp(const void* p) {
  const ApInfo& a = *static_cast<const ApInfo*>(p);
  char qr[160] = "WIFI:T:WPA;S:";
  appendEscaped(qr, sizeof(qr), a.ssid);
  strncat(qr, ";P:", sizeof(qr) - strlen(qr) - 1);
  appendEscaped(qr, sizeof(qr), a.password);
  strncat(qr, ";;", sizeof(qr) - strlen(qr) - 1);
  drawQrCode(qr, 460, 70, 320);

  drawSparseString(&epaperFont, 40, 70, "Setup mode", GxEPD_BLACK);
  canvas().fillRect(40, 80, 380, 3, GxEPD_BLACK);
  char net[64], pass[64];
  snprintf(net, sizeof(net), "Wi-Fi:  %s", a.ssid);
  snprintf(pass, sizeof(pass), "Password:  %s", a.password);
  const char* lines[] = {"1. Scan the code with your phone's camera,",
                         "   or join this Wi-Fi by hand:",
                         net,
                         pass,
                         "2. The setup page opens by itself.",
                         "   If not, open http://192.168.4.1"};
  drawLines(40, 130, lines, 6);
  drawSparseString(&epaperFont, 40, 440, "Leaves setup mode after 5 minutes without use.", GxEPD_BLACK);
}

void drawHome(const void* p) {
  const HomeInfo& h = *static_cast<const HomeInfo*>(p);
  char url[80];
  snprintf(url, sizeof(url), "http://%s/", h.ip);
  drawQrCode(url, 520, 40, 250);

  drawSparseString(&epaperFont, 40, 60, "Edit mode", GxEPD_BLACK);
  canvas().fillRect(40, 70, 440, 3, GxEPD_BLACK);
  char a1[80], a2[80];
  snprintf(a1, sizeof(a1), "http://%s", h.host);
  snprintf(a2, sizeof(a2), "or http://%s", h.ip);
  const char* lines[] = {"On a phone or computer in this Wi-Fi,", "scan the code or open:", a1, a2};
  drawLines(40, 115, lines, 4);
  drawSparseString(&epaperFont, 40, 300, "Code to type in:", GxEPD_BLACK);
  // The code in the clock digits (they have 0-9).
  drawSparseString(&HighSpeed_130, 40, 450, h.code, GxEPD_BLACK);
  // Below the QR code, above the code digits (they span x 40-550, y 346-477).
  drawSparseString(&epaperFont, 520, 318, "Closes after 10 min unused.", GxEPD_BLACK);
}

}  // namespace

int drawQrCode(const char* text, int x, int y, int maxSize) {
  static uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(QR_MAX_VERSION)];
  static uint8_t tmp[qrcodegen_BUFFER_LEN_FOR_VERSION(QR_MAX_VERSION)];
  if (!qrcodegen_encodeText(text, tmp, qr, qrcodegen_Ecc_MEDIUM, qrcodegen_VERSION_MIN, QR_MAX_VERSION,
                            qrcodegen_Mask_AUTO, true))
    return 0;
  int n = qrcodegen_getSize(qr);
  int scale = maxSize / (n + 8);  // 4 modules of quiet zone on each side
  if (scale < 1) return 0;
  int x0 = x + 4 * scale, y0 = y + 4 * scale;
  for (int r = 0; r < n; r++)
    for (int c = 0; c < n; c++)
      if (qrcodegen_getModule(qr, c, r)) canvas().fillRect(x0 + c * scale, y0 + r * scale, scale, scale, GxEPD_BLACK);
  return (n + 8) * scale;
}

void drawSetupScreenAp(const char* ssid, const char* password) {
  ApInfo a{ssid, password};
  showScreen(drawAp, &a);
}

void drawSetupScreenHome(const char* hostName, const char* ip, const char* code) {
  HomeInfo h{hostName, ip, code};
  showScreen(drawHome, &h);
}
