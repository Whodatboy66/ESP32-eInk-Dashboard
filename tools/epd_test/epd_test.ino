// Display bring-up test: Waveshare e-Paper Driver HAT + 7.5" B/W 800x480 (GxEPD2_750_T7)
// on a FireBeetle 2 ESP32-C6, same pins as the dashboard's "ESP32 C6" preset.
//
// - Board LED (GPIO15) blinks fast (4x/s) from power-on: the test firmware is running.
// - Serial log (115200) probes the BUSY line and times every refresh.
// - Every 20 s the panel is redrawn (pattern alternates), so a working display shows a life sign.
// - Never sleeps, so the USB serial monitor stays connected and wires can be fixed between cycles.
//
// Build/flash:  arduino-cli compile --profile esp32c6_firebeetle -u -p COM3 tools/epd_test
// Monitor:      arduino-cli monitor -p COM3 --profile esp32c6_firebeetle -c baudrate=115200

#include <SPI.h>
#include <GxEPD2_BW.h>
#include <epd/GxEPD2_750_T7.h>
#include <Fonts/FreeMonoBold24pt7b.h>

constexpr int PIN_CS   = 1;
constexpr int PIN_DC   = 8;
constexpr int PIN_RST  = 14;
constexpr int PIN_BUSY = 7;
constexpr int PIN_SCK  = 23;
constexpr int PIN_MOSI = 22;
constexpr int PIN_PWR  = 4;
constexpr int PIN_LED  = 15;

constexpr unsigned long REDRAW_MS = 20000;

GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT> display(
  GxEPD2_750_T7(PIN_CS, PIN_DC, PIN_RST, PIN_BUSY));

unsigned long lastDraw = 0;
int cycle = 0;
String lastBusy;

// Read BUSY once with the internal pull-up and once with the pull-down.
// A line that follows the pull is not driven by anything (wire loose / HAT without power).
String probeBusy() {
  pinMode(PIN_BUSY, INPUT_PULLUP);
  delay(3);
  int up = digitalRead(PIN_BUSY);
  pinMode(PIN_BUSY, INPUT_PULLDOWN);
  delay(3);
  int down = digitalRead(PIN_BUSY);
  pinMode(PIN_BUSY, INPUT);
  if (up == HIGH && down == LOW)  return "NOT DRIVEN (BUSY wire loose, or HAT has no power)";
  if (up == HIGH && down == HIGH) return "HIGH, driven (display controller idle)";
  if (up == LOW && down == LOW)   return "LOW, driven (controller busy or held in reset)";
  return "unstable (loose contact?)";
}

void logBusy(const char* when) {
  String s = probeBusy();
  Serial.printf("[BUSY] %-22s %s\n", when, s.c_str());
  lastBusy = s;
}

void drawPattern(bool inverted) {
  uint16_t fg = inverted ? GxEPD_WHITE : GxEPD_BLACK;
  uint16_t bg = inverted ? GxEPD_BLACK : GxEPD_WHITE;
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(bg);
    for (int i = 0; i < 6; i++)  // thick frame
      display.drawRect(i, i, display.width() - 2 * i, display.height() - 2 * i, fg);
    for (int y = 0; y < 4; y++)  // checkerboard, top left
      for (int x = 0; x < 4; x++)
        if ((x + y) % 2 == 0) display.fillRect(30 + x * 30, 30 + y * 30, 30, 30, fg);
    display.setFont(&FreeMonoBold24pt7b);
    display.setTextColor(fg);
    display.setTextSize(2);
    display.setCursor(250, 230);
    display.print("HELLO");
    display.setTextSize(1);
    display.setCursor(250, 330);
    display.printf("Display OK  #%d", cycle);
  } while (display.nextPage());
}

void refresh() {
  cycle++;
  logBusy("before refresh");
  SPI.begin(PIN_SCK, -1, PIN_MOSI, PIN_CS);
  display.init(115200, cycle == 1, 2, false);  // cycle 1: full init incl. reset pulse
  unsigned long t0 = millis();
  drawPattern(cycle % 2 == 0);
  unsigned long ms = millis() - t0;
  display.powerOff();
  // A real refresh takes ~2.5-4.5 s. GxEPD2 gives up waiting for BUSY after 10 s per step,
  // so anything far above that is a timeout, not a refresh.
  const char* verdict = ms < 1500  ? "NO RESPONSE (commands not arriving: DIN/CLK/CS/DC)"
                      : ms < 9000  ? "DISPLAY RESPONDED (it should show the new pattern now)"
                                   : "BUSY TIMEOUT (BUSY wire not connected / not driven)";
  Serial.printf("[EPD]  refresh #%d took %lu ms -> %s\n", cycle, ms, verdict);
  logBusy("after refresh");
  lastDraw = millis();
}

// Heartbeat in its own task, so the LED blinks from the first moment even while
// setup() or a refresh is blocked waiting for the display.
void heartbeat(void*) {
  pinMode(PIN_LED, OUTPUT);
  for (;;) {
    digitalWrite(PIN_LED, !digitalRead(PIN_LED));
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

void setup() {
  xTaskCreate(heartbeat, "heartbeat", 2048, nullptr, 1, nullptr);
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && millis() - start < 4000) delay(10);
  delay(500);
  Serial.println("\n===== e-Paper display test =====");
  Serial.println("(LED should blink 4x per second now)");
  Serial.printf("Pins CS=%d DC=%d RST=%d BUSY=%d SCK=%d MOSI=%d PWR=%d\n",
                PIN_CS, PIN_DC, PIN_RST, PIN_BUSY, PIN_SCK, PIN_MOSI, PIN_PWR);

  pinMode(PIN_PWR, OUTPUT);
  digitalWrite(PIN_PWR, LOW);
  delay(200);
  logBusy("PWR off");

  digitalWrite(PIN_PWR, HIGH);
  delay(200);
  logBusy("PWR on");

  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, LOW);
  delay(20);
  logBusy("RST held low");
  digitalWrite(PIN_RST, HIGH);
  delay(200);
  logBusy("after RST pulse");

  Serial.println("Expected with good wiring: 'PWR on' -> HIGH driven, refresh ~4000 ms.");
  refresh();
}

void loop() {
  static unsigned long lastProbe = 0;
  if (millis() - lastProbe > 1000) {  // report BUSY changes live (e.g. while fixing wires)
    String s = probeBusy();
    if (s != lastBusy) {
      Serial.printf("[BUSY] %-22s %s\n", "changed", s.c_str());
      lastBusy = s;
    }
    lastProbe = millis();
  }
  if (millis() - lastDraw > REDRAW_MS) refresh();
}
