// Stock quotes (Yahoo Finance chart API), stage 7b own code. "SYMBOL" or "SYMBOL:Label" entries
// (ConfigValidation.h); the answers are read by ServiceParsers.h.
#include "Fetchers.h"

#include "HttpJson.h"
#include "ServiceParsers.h"
#include "../core/ConfigValidation.h"

bool fetchStocksDirect(int* failed) {
  StockItem fresh[MAX_STOCKS];  // the kept quotes stay unless at least one new one is read (stage 8)
  int freshCount = 0;
  int failures = 0;
  if (!stock_symbols[0]) return false;
  JsonDocument filter;  // only the quote, not the chart
  JsonObject meta = filter["chart"]["result"][0]["meta"].to<JsonObject>();
  for (const char* k : {"regularMarketPrice", "chartPreviousClose", "previousClose", "regularMarketDayHigh",
                        "regularMarketDayLow", "fiftyTwoWeekHigh", "fiftyTwoWeekLow"})
    meta[k] = true;
  HttpJsonOptions opt;
  opt.filter = &filter;
  StockEntry entries[STOCK_MAX_ENTRIES];
  int n = parseStockEntries(stock_symbols, entries, STOCK_MAX_ENTRIES);
  for (int i = 0; i < n && freshCount < MAX_STOCKS; i++) {
    char encoded[48], url[160];
    svc::urlEncode(entries[i].symbol, encoded, sizeof(encoded));
    snprintf(url, sizeof(url), "https://query1.finance.yahoo.com/v8/finance/chart/%s?range=1d&interval=1d", encoded);
    JsonDocument doc;
    StockItem s = {};
    if (!httpGetJson(url, doc, opt) || !svc::readQuote(doc, s)) {
      DBGF("[Stocks] No quote for %s", entries[i].symbol);
      failures++;
      continue;
    }
    snprintf(s.name, sizeof(s.name), "%s", entries[i].label);  // own label, or the symbol without ^
    fresh[freshCount++] = s;
    DBGF("[Stocks] %s %.2f (prev %.2f)", s.name, s.price, s.prevClose);
  }
  if (failed) *failed = failures;
  if (freshCount == 0) return false;
  for (int i = 0; i < freshCount; i++) stocks[i] = fresh[i];
  stockCount = freshCount;
  return true;
}
