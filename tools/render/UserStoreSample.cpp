// PC renderer stand-in for src/personal/UserStore.cpp (no LittleFS on the PC): sample tasks, note
// and birthdays. renderSample: 0 = normal, 1 = empty, 2 = long texts / many entries.
#include <stdlib.h>
#include <time.h>

#include "src/personal/UserStore.h"
#include "src/ui/SparseFont.h"

int renderSample = 0;

namespace {
personal::TaskList tasks;
personal::BirthdayList manual, feed;
char note[personal::NOTE_BYTES];
bool built = false;

void addTask(const char* title, int y, int m, int d, uint8_t prio, bool done = false) {
  personal::Task& t = tasks.items[tasks.count++];
  personal::copyChars(t.title, sizeof(t.title), title, personal::TASK_TITLE_CHARS);
  t.due = y ? personal::Date{y, m, d} : personal::Date{};
  t.prio = prio;
  t.done = done;
}
void addBday(personal::BirthdayList& l, const char* name, int d, int m, int y) {
  personal::Birthday& b = l.items[l.count++];
  personal::copyChars(b.name, sizeof(b.name), name, personal::BDAY_NAME_CHARS);
  b.day = d;
  b.month = m;
  b.year = y;
}

void build() {
  if (built) return;
  built = true;
  if (renderSample == 1) return;  // empty
  addTask("Altglas wegbringen", 2026, 11, 14, 2);
  addTask("Call the plumber (kitchen tap)", 2026, 11, 17, 1);
  addTask("Buy milk & bread", 2026, 11, 18, 2);
  addTask("Pay invoice #2231", 2026, 11, 20, 2);
  addTask("Return library books", 2026, 11, 28, 3);
  addTask("Plan Anna's party", 0, 0, 0, 2);
  addTask("Water the plants", 0, 0, 0, 3, true);
  if (renderSample == 2) {
    addTask("Eine sehr lange Aufgabe mit Umlauten: Größe prüfen, Übergänge säubern", 2026, 12, 3, 2);
    for (int i = 0; i < 8; i++) addTask("Extra task", 0, 0, 0, 3);
  }
  snprintf(note, sizeof(note), "%s",
           renderSample == 2
               ? "Wi-Fi für Gäste: Sunflower-42 (Passwort am Kühlschrank)\nMüll: Donnerstag!\n"
                 "Donaudampfschifffahrtsgesellschaftskapitän\nA long note goes on and on, with more "
                 "words than the box can show, so it ends with an ellipsis at the bottom."
               : "Wi-Fi for guests: Sunflower-42\nBins out on Thursday!");
  addBday(manual, "Anna", 17, 11, 1990);
  addBday(manual, "Grandpa Joe", 21, 11, 1941);
  addBday(manual, "Jürgen", 18, 11, 0);
  addBday(feed, "Mia", 29, 11, 0);
  addBday(feed, "Lukas", 24, 12, 0);
  addBday(manual, "Oma Erika", 17, 1, 1967);  // the January renders of the month view (stage 7)
  if (renderSample == 2)
    for (int i = 0; i < 10; i++) addBday(feed, "Maximilian-Alexander von Hohenzollern-Sigmaringen", 1 + i, 12, 0);
}
}  // namespace

const personal::TaskList& userTasks() { build(); return tasks; }
const char* userNote() { build(); return note; }
const personal::BirthdayList& userBirthdays() { build(); return manual; }
const personal::BirthdayList& userFeedBirthdays() { build(); return feed; }
// Birthdays from the calendar links (stage 7), as Calendar.cpp stores them: the word "Geburtstag"
// already dropped. Long names with an age: the line is shortened, the age stays (build 81).
const personal::BirthdayList& userCalBirthdays() {
  static personal::BirthdayList cal;
  static bool once = false;
  if (!once && renderSample != 1) {
    once = true;
    addBday(cal, "Konstantin Weißenberger", 2, 12, 1996);
    addBday(cal, "Elisabeth Hohenstein", 5, 1, 2016);  // the January renders: footer, side column, sheet
  }
  return cal;
}
bool saveUserTasks(const personal::TaskList&) { return true; }
bool saveUserNote(const char*) { return true; }
bool saveUserBirthdays(const personal::BirthdayList&) { return true; }
bool saveUserFeedBirthdays(const personal::BirthdayList&) { return true; }
bool saveUserCalBirthdays(const personal::BirthdayList&) { return true; }
bool clearUserCalBirthdays() { return true; }
size_t readUserFile(const char*, char*, size_t) { return 0; }
void eraseUserData() {}

personal::Date localToday() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  time_t now = time(nullptr);
  struct tm lt;
  localtime_r(&now, &lt);
  return personal::Date{lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday};
}
bool screenFontHas(uint32_t cp) { return findGlyph(&epaperFont, cp) != nullptr; }
