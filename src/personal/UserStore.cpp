#include "UserStore.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <time.h>

#include <new>

#include "../../app.h"
#include "../ui/SparseFont.h"

namespace {

constexpr const char* PARTITION = "userdata";
constexpr size_t MAX_FILE = 16 * 1024;

bool mounted = false;
bool mountTried = false;

// The lists live on the heap, allocated on first use (the classic ESP32 has little static RAM
// left). If an allocation fails, readers get these constant empty lists (in flash) and saving
// reports an error.
const personal::TaskList EMPTY_TASKS{};
const personal::BirthdayList EMPTY_BIRTHDAYS{};
template <typename T>
T* lazy(T*& p) {
  if (!p) p = new (std::nothrow) T();
  return p;
}
personal::TaskList* tasksP = nullptr;
personal::BirthdayList* birthdaysP = nullptr;
personal::BirthdayList* feedP = nullptr;
personal::BirthdayList* calP = nullptr;  // stage 7: birthdays found in the calendar links
char note[personal::NOTE_BYTES];
bool tasksLoaded = false, noteLoaded = false, bdayLoaded = false, feedLoaded = false, calLoaded = false;

bool mount() {
  if (mountTried) return mounted;
  mountTried = true;
  // A damaged file system is formatted: the data is lost, but the device keeps working.
  // A partition that was never formatted (the first start after the USB migration) makes the core
  // print "Corrupted dir pair ... mount failed"; it then formats it and mounts again.
  mounted = LittleFS.begin(true, "/user", 4, PARTITION);
  if (mounted)
    DBGF("[User] storage mounted: %u of %u KB used", (unsigned)(LittleFS.usedBytes() / 1024),
         (unsigned)(LittleFS.totalBytes() / 1024));
  else
    DBGF("[User] LittleFS on \"%s\" not available (even after formatting)", PARTITION);
  return mounted;
}

// Reads a whole file into a heap buffer (caller frees). nullptr if missing or too large.
char* readAll(const char* path, size_t* len) {
  *len = 0;
  if (!mount() || !LittleFS.exists(path)) return nullptr;
  File f = LittleFS.open(path, "r");
  if (!f) return nullptr;
  size_t size = f.size();
  if (size == 0 || size > MAX_FILE) {
    f.close();
    return nullptr;
  }
  char* buf = static_cast<char*>(malloc(size + 1));
  if (!buf) {
    f.close();
    return nullptr;
  }
  *len = f.readBytes(buf, size);
  buf[*len] = '\0';
  f.close();
  return buf;
}

// Written to a temporary file first, then renamed: a power cut leaves the old or the new file.
bool writeAtomic(const char* path, const char* data, size_t len) {
  if (!mount()) return false;
  char tmp[40];
  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  File f = LittleFS.open(tmp, "w");
  if (!f) return false;
  size_t written = f.write(reinterpret_cast<const uint8_t*>(data), len);
  f.close();
  if (written != len) {
    LittleFS.remove(tmp);
    return false;
  }
  LittleFS.remove(path);
  return LittleFS.rename(tmp, path);
}

template <typename Write>
bool saveJson(const char* path, Write write) {
  char* buf = static_cast<char*>(malloc(MAX_FILE));
  if (!buf) return false;
  size_t n = write(buf, MAX_FILE);
  bool ok = n > 0 && writeAtomic(path, buf, n);
  free(buf);
  if (!ok) DBGF("[User] %s not saved", path);
  return ok;
}

void loadBirthdayFile(const char* path, const char* key, personal::BirthdayList& out) {
  out.count = 0;
  size_t len;
  char* buf = readAll(path, &len);
  if (!buf) return;
  char err[80];
  if (!personal::parseBirthdays(buf, len, key, out, err, sizeof(err))) DBGF("[User] %s: %s", path, err);
  free(buf);
}

// Which widgets have content, kept in NVS (user_content, app.h).
void setContent(uint8_t bit, bool on) {
  uint8_t v = on ? (user_content | bit) : (user_content & ~bit);
  if (v == user_content) return;
  user_content = v;
  Preferences p;  // own object: the global one may be open elsewhere
  p.begin("config", false);
  p.putUChar("user_flags", v);
  p.end();
}

bool hasOpenTasks(const personal::TaskList& l) {
  for (int i = 0; i < l.count; i++)
    if (!l.items[i].done) return true;
  return false;
}

void updateBirthdayFlag() {
  setContent(USER_HAS_BIRTHDAYS, (birthdaysP && birthdaysP->count > 0) || (feedP && feedP->count > 0) ||
                                     (calP && calP->count > 0));
}

}  // namespace

const personal::TaskList& userTasks() {
  personal::TaskList* t = lazy(tasksP);
  if (!t) return EMPTY_TASKS;
  if (!tasksLoaded) {
    tasksLoaded = true;
    t->count = 0;
    size_t len;
    char* buf = readAll(USER_TASKS_FILE, &len);
    if (buf) {
      char err[80];
      if (!personal::parseTasks(buf, len, *t, err, sizeof(err))) DBGF("[User] tasks: %s", err);
      free(buf);
    }
  }
  return *t;
}

const char* userNote() {
  if (!noteLoaded) {
    noteLoaded = true;
    note[0] = '\0';
    size_t len;
    char* buf = readAll(USER_NOTE_FILE, &len);
    if (buf) {
      char err[80];
      if (!personal::parseNote(buf, len, note, sizeof(note), err, sizeof(err))) DBGF("[User] note: %s", err);
      free(buf);
    }
  }
  return note;
}

const personal::BirthdayList& userBirthdays() {
  personal::BirthdayList* b = lazy(birthdaysP);
  if (!b) return EMPTY_BIRTHDAYS;
  if (!bdayLoaded) {
    bdayLoaded = true;
    loadBirthdayFile(USER_BDAY_FILE, "manual", *b);
  }
  return *b;
}

const personal::BirthdayList& userFeedBirthdays() {
  personal::BirthdayList* b = lazy(feedP);
  if (!b) return EMPTY_BIRTHDAYS;
  if (!feedLoaded) {
    feedLoaded = true;
    loadBirthdayFile(USER_BDAY_FEED_FILE, "feed", *b);
  }
  return *b;
}

const personal::BirthdayList& userCalBirthdays() {
  personal::BirthdayList* b = lazy(calP);
  if (!b) return EMPTY_BIRTHDAYS;
  if (!calLoaded) {
    calLoaded = true;
    loadBirthdayFile(USER_BDAY_CAL_FILE, "cal", *b);
  }
  return *b;
}

bool saveUserTasks(const personal::TaskList& list) {
  personal::TaskList* t = lazy(tasksP);
  if (!t) return false;
  if (&list != t) *t = list;
  tasksLoaded = true;
  setContent(USER_HAS_TASKS, hasOpenTasks(*t));
  return saveJson(USER_TASKS_FILE, [&](char* b, size_t cap) { return personal::writeTasks(*t, b, cap); });
}

bool saveUserNote(const char* text) {
  if (text != note) personal::copyChars(note, sizeof(note), text, personal::NOTE_CHARS);
  noteLoaded = true;
  setContent(USER_HAS_NOTE, note[0] != '\0');
  return saveJson(USER_NOTE_FILE, [&](char* b, size_t cap) { return personal::writeNote(note, b, cap); });
}

bool saveUserBirthdays(const personal::BirthdayList& list) {
  personal::BirthdayList* m = lazy(birthdaysP);
  if (!m) return false;
  if (&list != m) *m = list;
  bdayLoaded = true;
  userFeedBirthdays();
  updateBirthdayFlag();
  return saveJson(USER_BDAY_FILE, [&](char* b, size_t cap) { return personal::writeBirthdays(*m, "manual", b, cap); });
}

bool saveUserFeedBirthdays(const personal::BirthdayList& list) {
  personal::BirthdayList* f = lazy(feedP);
  if (!f) return false;
  if (&list != f) *f = list;
  feedLoaded = true;
  userBirthdays();
  updateBirthdayFlag();
  return saveJson(USER_BDAY_FEED_FILE, [&](char* b, size_t cap) { return personal::writeBirthdays(*f, "feed", b, cap); });
}

bool saveUserCalBirthdays(const personal::BirthdayList& list) {
  const personal::BirthdayList& stored = userCalBirthdays();
  if (personal::sameBirthdays(stored, list)) return true;  // unchanged: no flash write
  personal::BirthdayList* c = lazy(calP);
  if (!c) return false;
  if (&list != c) *c = list;
  userBirthdays();
  userFeedBirthdays();
  updateBirthdayFlag();
  return saveJson(USER_BDAY_CAL_FILE, [&](char* b, size_t cap) { return personal::writeBirthdays(*c, "cal", b, cap); });
}

bool clearUserCalBirthdays() {
  personal::BirthdayList* c = lazy(calP);
  if (!c) return false;
  userCalBirthdays();  // loaded, so an empty file isn't written again
  if (c->count == 0) return true;
  c->count = 0;
  updateBirthdayFlag();
  return saveJson(USER_BDAY_CAL_FILE, [&](char* b, size_t cap) { return personal::writeBirthdays(*c, "cal", b, cap); });
}

void purgeOldDoneTasks() {
  personal::TaskList* t = lazy(tasksP);
  if (!t) return;
  userTasks();  // loaded
  int removed = personal::purgeDoneTasks(*t, localToday());
  if (removed) {
    DBGF("[User] %d done task(s) older than %d days removed", removed, personal::DONE_KEEP_DAYS);
    saveUserTasks(*t);
  }
}

size_t readUserFile(const char* path, char* out, size_t cap) {
  size_t len;
  char* buf = readAll(path, &len);
  if (!buf || len + 1 > cap) {
    free(buf);
    return 0;
  }
  memcpy(out, buf, len + 1);
  free(buf);
  return len;
}

bool userFileExists(const char* path) { return mount() && LittleFS.exists(path); }

size_t userFileSize(const char* path) {
  if (!userFileExists(path)) return 0;
  File f = LittleFS.open(path, "r");
  if (!f) return 0;
  size_t n = f.size();
  f.close();
  return n;
}

bool appendUserFile(const char* path, const char* data, size_t len) {
  if (!mount()) return false;
  File f = LittleFS.open(path, "a");
  if (!f) return false;
  size_t w = f.write(reinterpret_cast<const uint8_t*>(data), len);
  f.close();
  return w == len;
}

bool writeUserFile(const char* path, const char* data, size_t len) { return writeAtomic(path, data, len); }

void removeUserFile(const char* path) {
  if (userFileExists(path)) LittleFS.remove(path);
}

void userFileRotate(const char* path, const char* oldPath) {
  if (!mount()) return;
  LittleFS.remove(oldPath);
  LittleFS.rename(path, oldPath);
}

void eraseUserData() {
  if (!mount()) return;
  LittleFS.remove(USER_TASKS_FILE);
  LittleFS.remove(USER_NOTE_FILE);
  LittleFS.remove(USER_BDAY_FILE);
  LittleFS.remove(USER_BDAY_FEED_FILE);
  LittleFS.remove(USER_BDAY_CAL_FILE);
  LittleFS.remove("/log.txt");  // diagnostics (src/core/Logger.cpp)
  LittleFS.remove("/log.1");
  LittleFS.remove("/battery.csv");
  if (tasksP) tasksP->count = 0;
  note[0] = '\0';
  if (birthdaysP) birthdaysP->count = 0;
  if (feedP) feedP->count = 0;
  if (calP) calP->count = 0;
  user_content = 0;  // the NVS key goes with the factory reset of "config"
  DBG("[User] personal data erased");
}

personal::Date localToday() {
  time_t now = time(nullptr);
  if (now < 1700000000) return personal::Date{};
  struct tm lt;
  localtime_r(&now, &lt);
  return personal::Date{lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday};
}

bool screenFontHas(uint32_t codepoint) {
  return findGlyph(&epaperFont, codepoint) != nullptr;
}
