// The setup app's JSON API (docs/SETUP_API.md). Stage 4: personal data (tasks, note, birthdays).
#include "Api.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>

#include <memory>
#include <new>

#include "../../app.h"
#include "../personal/UserStore.h"

namespace {

constexpr size_t MAX_BODY = 8 * 1024;
WebServer* srv = nullptr;

void sendJson(int code, const JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  srv->sendHeader("Cache-Control", "no-store");
  srv->send(code, "application/json", out);
}

void sendError(int code, const char* message) {
  JsonDocument doc;
  doc["ok"] = false;
  doc["error"] = message;
  sendJson(code, doc);
}

void sendOk(const char* removed = nullptr) {
  JsonDocument doc;
  doc["ok"] = true;
  if (removed && removed[0]) doc["removed"] = removed;
  sendJson(200, doc);
}

// The body of a POST / PUT: JSON only (a page from another site can't send that without a
// preflight, which the device never answers), at most MAX_BODY.
bool readBody(const char*& body, size_t& len) {
  String type = srv->header("Content-Type");
  if (!type.startsWith("application/json")) {
    sendError(415, "JSON expected");
    return false;
  }
  if (!srv->hasArg("plain")) {
    sendError(400, "Empty request");
    return false;
  }
  const String& plain = srv->arg("plain");
  if (plain.length() > MAX_BODY) {
    sendError(413, "Too much data");
    return false;
  }
  body = plain.c_str();
  len = plain.length();
  return true;
}

void sendFile(const char* path, const char* emptyJson) {
  constexpr size_t CAP = 16 * 1024;
  std::unique_ptr<char[]> buf(new (std::nothrow) char[CAP]);  // heap: little static RAM
  size_t n = buf ? readUserFile(path, buf.get(), CAP) : 0;
  srv->sendHeader("Cache-Control", "no-store");
  srv->send(200, "application/json", n ? buf.get() : emptyJson);
}

void handleGetTasks() { sendFile(USER_TASKS_FILE, "{\"tasks\":[]}"); }
void handleGetNote() { sendFile(USER_NOTE_FILE, "{\"note\":\"\"}"); }

void handlePutTasks() {
  const char* body;
  size_t len;
  if (!readBody(body, len)) return;
  std::unique_ptr<personal::TaskList> listP(new (std::nothrow) personal::TaskList());
  if (!listP) {
    sendError(503, "Not enough memory right now: try again.");
    return;
  }
  personal::TaskList& list = *listP;
  char err[96], removed[32];
  if (!personal::parseTasks(body, len, list, err, sizeof(err), screenFontHas, removed, sizeof(removed))) {
    sendError(400, err);
    return;
  }
  if (!saveUserTasks(list)) {
    sendError(500, "The tasks could not be saved.");
    return;
  }
  sendOk(removed);
}

void handlePutNote() {
  const char* body;
  size_t len;
  if (!readBody(body, len)) return;
  std::unique_ptr<char[]> noteP(new (std::nothrow) char[personal::NOTE_BYTES]);
  if (!noteP) {
    sendError(503, "Not enough memory right now: try again.");
    return;
  }
  char* note = noteP.get();
  char err[96], removed[32];
  if (!personal::parseNote(body, len, note, personal::NOTE_BYTES, err, sizeof(err), screenFontHas, removed, sizeof(removed))) {
    sendError(400, err);
    return;
  }
  if (!saveUserNote(note)) {
    sendError(500, "The note could not be saved.");
    return;
  }
  sendOk(removed);
}

void handleGetBirthdays() {
  JsonDocument doc;
  const personal::BirthdayList& manual = userBirthdays();
  JsonArray m = doc["manual"].to<JsonArray>();
  for (int i = 0; i < manual.count; i++) {
    JsonObject o = m.add<JsonObject>();
    o["name"] = manual.items[i].name;
    o["day"] = manual.items[i].day;
    o["month"] = manual.items[i].month;
    o["year"] = manual.items[i].year;
  }
  std::unique_ptr<personal::Upcoming[]> up(new (std::nothrow) personal::Upcoming[personal::MAX_BIRTHDAYS]);
  int n = up ? personal::upcomingBirthdays(manual, userFeedBirthdays(), localToday(), up.get(), personal::MAX_BIRTHDAYS) : 0;
  JsonArray u = doc["upcoming"].to<JsonArray>();
  for (int i = 0; i < n; i++) {
    JsonObject o = u.add<JsonObject>();
    char date[11];
    personal::formatIsoDate(up[i].date, date, sizeof(date));
    o["name"] = up[i].who->name;
    o["date"] = date;
    o["days"] = up[i].days;
    o["age"] = up[i].age;
    o["fromFeed"] = up[i].fromFeed;
  }
  sendJson(200, doc);
}

void handlePutBirthdays() {
  const char* body;
  size_t len;
  if (!readBody(body, len)) return;
  std::unique_ptr<personal::BirthdayList> listP(new (std::nothrow) personal::BirthdayList());
  if (!listP) {
    sendError(503, "Not enough memory right now: try again.");
    return;
  }
  personal::BirthdayList& list = *listP;
  char err[96], removed[32];
  if (!personal::parseBirthdays(body, len, "manual", list, err, sizeof(err), screenFontHas, removed, sizeof(removed))) {
    sendError(400, err);
    return;
  }
  if (!saveUserBirthdays(list)) {
    sendError(500, "The birthdays could not be saved.");
    return;
  }
  sendOk(removed);
}

}  // namespace

void registerApiRoutes(WebServer& server) {
  srv = &server;
  static const char* headers[] = {"Content-Type", "Cookie", "Host", "Origin"};
  server.collectHeaders(headers, sizeof(headers) / sizeof(headers[0]));
  server.on("/api/tasks", HTTP_GET, handleGetTasks);
  server.on("/api/tasks", HTTP_PUT, handlePutTasks);
  server.on("/api/notes", HTTP_GET, handleGetNote);
  server.on("/api/notes", HTTP_PUT, handlePutNote);
  server.on("/api/birthdays", HTTP_GET, handleGetBirthdays);
  server.on("/api/birthdays", HTTP_PUT, handlePutBirthdays);
}
