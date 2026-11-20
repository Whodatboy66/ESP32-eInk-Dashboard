#include "Fetchers.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ctype.h>
#include <time.h>

#include "HttpJson.h"
#include "SecureClient.h"
#include "../widgets/MonthView.h"
#include "../core/ConfigValidation.h"

// ----------------------- Stocks (Yahoo Finance) -----------------------

bool fetchStocksDirect(int* failed) {
  stockCount = 0;
  int failures = 0;
  if (stock_symbols[0] == '\0') return false;

  JsonDocument filter;
  JsonObject meta = filter["chart"]["result"][0]["meta"].to<JsonObject>();
  meta["regularMarketPrice"] = true;
  meta["chartPreviousClose"] = true;
  meta["previousClose"] = true;
  meta["regularMarketDayHigh"] = true;
  meta["regularMarketDayLow"] = true;
  meta["fiftyTwoWeekHigh"] = true;
  meta["fiftyTwoWeekLow"] = true;

  // "SYMBOL" or "SYMBOL:Label" (ConfigValidation.h): the symbol for the request, the label on screen.
  StockEntry entries[STOCK_MAX_ENTRIES];
  int entryCount = parseStockEntries(stock_symbols, entries, STOCK_MAX_ENTRIES);

  for (int k = 0; k < entryCount && stockCount < MAX_STOCKS; k++) {
    const char* sym = entries[k].symbol;
    char encoded[48];
    urlEncode(sym, encoded, sizeof(encoded));

    char url[160];
    snprintf(url, sizeof(url),
             "https://query1.finance.yahoo.com/v8/finance/chart/%s?range=1d&interval=1d", encoded);

    JsonDocument doc;
    if (!httpGetJson(url, doc, &filter)) { failures++; continue; }

    JsonObject m = doc["chart"]["result"][0]["meta"];
    if (m.isNull() || m["regularMarketPrice"].isNull()) {
      DBGF("[Stocks] No quote for %s", sym);
      failures++;
      continue;
    }

    StockItem& s = stocks[stockCount++];
    snprintf(s.name, sizeof(s.name), "%s", entries[k].label);  // own label, or the symbol without ^
    s.price = m["regularMarketPrice"] | 0.0f;
    s.prevClose = m["chartPreviousClose"] | (m["previousClose"] | 0.0f);
    s.dayHigh = m["regularMarketDayHigh"] | 0.0f;
    s.dayLow = m["regularMarketDayLow"] | 0.0f;
    s.fiftyTwoWeekHigh = m["fiftyTwoWeekHigh"] | 0.0f;
    s.fiftyTwoWeekLow = m["fiftyTwoWeekLow"] | 0.0f;
    DBGF("[Stocks] %s %.2f (prev %.2f)", s.name, s.price, s.prevClose);
  }
  if (failed) *failed = failures;
  return stockCount > 0;
}

