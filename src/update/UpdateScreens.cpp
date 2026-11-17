#include "UpdateScreens.h"

#include "../../display.h"

namespace ota {
namespace {

constexpr int16_t W = 800;
constexpr int16_t CX = W / 2;
constexpr int16_t LINE = 36;  // line spacing for the 16 px font

template <typename Draw>
void render(bool full, Draw draw) {
  if (full) display.setFullWindow();
  else display.setPartialWindow(0, 0, canvas().width(), canvas().height());
  display.firstPage();
  do {
    canvas().fillScreen(GxEPD_WHITE);
    draw();
  } while (display.nextPage());
}

void text(int16_t y, const char* s) {
  drawSparseStringCentered(&epaperFont, CX, y, s, GxEPD_BLACK);
}

// The font has a single weight: draw titles twice, 1 px apart.
void title(int16_t y, const char* s) {
  drawSparseStringCentered(&epaperFont, CX, y, s, GxEPD_BLACK);
  drawSparseStringCentered(&epaperFont, CX + 1, y, s, GxEPD_BLACK);
}

void header(const char* s) {
  title(90, s);
  canvas().drawFastHLine(160, 112, W - 320, GxEPD_BLACK);
}

// Check mark from thick lines (the font has no check glyph).
void checkMark(int16_t cx, int16_t cy) {
  for (int d = -3; d <= 3; d++) {
    canvas().drawLine(cx - 30, cy + d, cx - 10, cy + 20 + d, GxEPD_BLACK);
    canvas().drawLine(cx - 10, cy + 20 + d, cx + 32, cy - 22 + d, GxEPD_BLACK);
  }
}

void notesBlock(int16_t y, const char (*notes)[NOTE_LEN], int count) {
  if (count <= 0) return;
  text(y, "New in this version:");
  for (int i = 0; i < count; i++) {
    char line[NOTE_LEN + 4];
    snprintf(line, sizeof(line), "- %s", notes[i]);
    text(y + LINE * (i + 1), line);
  }
}

void progressBar(int16_t y, int quarter) {
  constexpr int16_t BW = 520, BH = 28, BX = CX - BW / 2;
  canvas().drawRect(BX, y, BW, BH, GxEPD_BLACK);
  canvas().drawRect(BX + 1, y + 1, BW - 2, BH - 2, GxEPD_BLACK);
  int16_t fill = (int16_t)((BW - 8) * quarter / 4);
  if (fill > 0) canvas().fillRect(BX + 4, y + 4, fill, BH - 8, GxEPD_BLACK);
}

}  // namespace

void showInfo(const DeviceInfo& info) {
  char l1[64], l2[48], l3[48], l4[48], l5[80], ssid[40];
  snprintf(l1, sizeof(l1), "Firmware %s   -   %s", info.version, info.variant);
  snprintf(l2, sizeof(l2), "Channel: %s   -   Auto install: %s", info.channel, info.autoInstall ? "on" : "off");
  snprintf(l3, sizeof(l3), "Last update: %s", info.lastUpdate);
  if (info.batteryPercent < 0) snprintf(l4, sizeof(l4), "Power: USB");
  else snprintf(l4, sizeof(l4), "Battery: %d %%%s", info.batteryPercent, info.usbPowered ? " - USB connected" : "");
  sanitizeNote(info.wifi, ssid, sizeof(ssid));
  snprintf(l5, sizeof(l5), "Wi-Fi: %s", ssid[0] ? ssid : "not set up");
  render(true, [&] {
    header("Device info");
    text(170, l1);
    text(170 + LINE, l2);
    text(170 + LINE * 2, l3);
    text(170 + LINE * 3, l4);
    text(170 + LINE * 4, l5);
    title(400, "Checking for updates...");
  });
}

void showAvailable(const Manifest& m, const char* installedVersion, bool waitForTap, uint32_t cancelMs) {
  char t[40], installed[40], prompt[48];
  snprintf(t, sizeof(t), "New version %s", m.version);
  snprintf(installed, sizeof(installed), "Installed: %s", installedVersion);
  if (waitForTap) snprintf(prompt, sizeof(prompt), "Tap to install - cancels in %u s", (unsigned)(cancelMs / 1000));
  else snprintf(prompt, sizeof(prompt), "Installing...");
  render(false, [&] {
    header(t);
    text(160, installed);
    notesBlock(160 + LINE * 2, m.notes, m.noteCount);
    title(420, prompt);
  });
}

void showProgress(InstallStage stage, int quarter, const char* newVersion) {
  char t[40], step[40];
  snprintf(t, sizeof(t), "Updating to %s", newVersion);
  switch (stage) {
    case InstallStage::Downloading: snprintf(step, sizeof(step), "1/4  Downloading  %d %%", quarter * 25); break;
    case InstallStage::Verifying:   snprintf(step, sizeof(step), "2/4  Verifying"); break;
    case InstallStage::Installing:  snprintf(step, sizeof(step), "3/4  Installing"); break;
  }
  render(false, [&] {
    header(t);
    title(200, step);
    progressBar(250, stage == InstallStage::Downloading ? quarter : 4);
    text(340, "Please keep the device powered.");
  });
}

void showRestarting(const char* newVersion) {
  char t[40];
  snprintf(t, sizeof(t), "Updating to %s", newVersion);
  render(false, [&] {
    header(t);
    title(200, "4/4  Restarting");
    progressBar(250, 4);
    text(340, "Please keep the device powered.");
  });
}

void showMessage(const char* t, const char* detail, bool fullRefresh) {
  render(fullRefresh, [&] {
    header("Firmware update");
    title(220, t);
    if (detail && *detail) text(220 + LINE + 10, detail);
    text(420, "Back to the dashboard in a few seconds.");
  });
}

void showUpdated(const char* version, const char (*notes)[NOTE_LEN], int noteCount) {
  char t[40];
  snprintf(t, sizeof(t), "Updated to %s", version);
  render(true, [&] {
    checkMark(CX, 90);
    title(170, t);
    notesBlock(170 + LINE + 20, notes, noteCount);
    text(440, "The dashboard follows in a moment.");
  });
}

}  // namespace ota
