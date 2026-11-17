// Host test for src/core/ConfigValidation.h. Run: tests/host/run.sh
#include "src/core/ConfigValidation.h"
#include "tests/host/check.h"

static const char* ics(const char* in) {
  static char out[260];
  return validateIcsUrl(in, out, sizeof(out)) ? out : "<invalid>";
}

static void test_ics_urls() {
  CHECK_STR(ics(""), "");
  CHECK_STR(ics("  "), "");
  CHECK_STR(ics("https://calendar.google.com/calendar/ical/x/basic.ics"),
            "https://calendar.google.com/calendar/ical/x/basic.ics");
  // http:// can't work (every request uses TLS): the same address over HTTPS.
  CHECK_STR(ics(" http://example.org/cal.ics "), "https://example.org/cal.ics");
  // Apple / Outlook "subscribe" links: same file over HTTPS.
  CHECK_STR(ics("webcal://p01-caldav.icloud.com/published/2/abc"), "https://p01-caldav.icloud.com/published/2/abc");
  CHECK_STR(ics("WEBCAL://example.org/a.ics"), "https://example.org/a.ics");
  CHECK_STR(ics("webcals://example.org/a.ics"), "https://example.org/a.ics");
  CHECK_STR(ics("ftp://example.org/a.ics"), "<invalid>");
  CHECK_STR(ics("calendar.google.com/basic.ics"), "<invalid>");  // no scheme
  CHECK_STR(ics("https://exa mple.org/a.ics"), "<invalid>");     // space inside
  CHECK_STR(ics("https://"), "<invalid>");                       // no host
}

static void test_ics_url_too_long_for_buffer() {
  char small[16];
  CHECK(!validateIcsUrl("https://example.org/a.ics", small, sizeof(small)));
}

static const char* stocks(const char* in) {
  static char out[96];
  return normalizeStockSymbols(in, out, sizeof(out)) ? out : "<invalid>";
}

static void test_stock_symbols() {
  CHECK_STR(stocks(""), "");
  CHECK_STR(stocks("AAPL"), "AAPL");
  CHECK_STR(stocks("AAPL,NVDA,^IXIC, BRK-B ,EURUSD=X"), "AAPL,NVDA,^IXIC,BRK-B,EURUSD=X");
  CHECK_STR(stocks("SAP.DE"), "SAP.DE");
  // Lists the fetcher always read (split on spaces and commas, first 5) stay usable.
  CHECK_STR(stocks("AAPL MSFT"), "AAPL,MSFT");
  CHECK_STR(stocks("AAPL,MSFT,"), "AAPL,MSFT");
  CHECK_STR(stocks("AAPL,,MSFT"), "AAPL,MSFT");
  CHECK_STR(stocks("A,B,C,D,E,F"), "A,B,C,D,E");
  // Characters a symbol can't have are refused.
  CHECK_STR(stocks("AAPL;NVDA"), "<invalid>");
  CHECK_STR(stocks("AAPL,<script>"), "<invalid>");
}

static void test_mqtt_port() {
  CHECK_EQ(parseMqttPort(""), 8883);
  CHECK_EQ(parseMqttPort("8883"), 8883);
  CHECK_EQ(parseMqttPort(" 1883 "), 1883);
  CHECK_EQ(parseMqttPort("65535"), 65535);
  CHECK_EQ(parseMqttPort("0"), -1);
  CHECK_EQ(parseMqttPort("65536"), -1);
  CHECK_EQ(parseMqttPort("88a3"), -1);
  CHECK_EQ(parseMqttPort("-5"), -1);
}

static void test_printer_address() {
  CHECK(validatePrinterAddress(""));                // no printer
  CHECK(validatePrinterAddress("192.168.1.50"));
  CHECK(validatePrinterAddress("x1c.local"));
  // Names from the router's DNS: checked when connecting (the resolved address must be local).
  CHECK(validatePrinterAddress("bambu-x1c"));
  CHECK(validatePrinterAddress("printer.fritz.box"));
  CHECK(!validatePrinterAddress("8.8.8.8"));        // a public address: never (no certificate check)
  CHECK(!validatePrinterAddress("printer name"));
  CHECK(!validatePrinterAddress("http://192.168.1.50"));
}

static void test_schema_migration_steps() {
  // Devices without the key are schema 0 (= build 66 layout); 0 -> 1 only stores the key.
  CHECK(configNeedsMigration(0, 1));
  CHECK(!configNeedsMigration(1, 1));
  CHECK(!configNeedsMigration(2, 1));  // newer settings than this firmware: left alone
}

int main() {
  test_ics_urls();
  test_ics_url_too_long_for_buffer();
  test_stock_symbols();
  test_mqtt_port();
  test_printer_address();
  test_schema_migration_steps();
  return finish("config validation");
}
