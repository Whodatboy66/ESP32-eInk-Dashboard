#pragma once
// Personal data on the "userdata" partition (LittleFS, docs/plans/STAGE4_PERSONAL_WIDGETS.md):
// tasks, the note, birthdays (hand-entered and from the birthday calendar link). OTA writes only the
// app slots, so the data survives updates. Loaded once per wake, on first use.
#include "Birthdays.h"
#include "Notes.h"
#include "Tasks.h"

// The data, loaded from flash on first use (empty if there is none or it can't be read).
const personal::TaskList& userTasks();
const char* userNote();
const personal::BirthdayList& userBirthdays();      // hand-entered
const personal::BirthdayList& userFeedBirthdays();  // from the birthday calendar link

// Saving (the API, the birthday feed): validated data, written atomically. False if the flash
// can't be written.
bool saveUserTasks(const personal::TaskList& list);
bool saveUserNote(const char* note);
bool saveUserBirthdays(const personal::BirthdayList& list);
bool saveUserFeedBirthdays(const personal::BirthdayList& list);

// The stored files as JSON, for the API (empty lists when there is none). Returns the length.
size_t readUserFile(const char* path, char* out, size_t cap);

// Plain files on the userdata partition (diagnostics log, battery history).
bool userFileExists(const char* path);
size_t userFileSize(const char* path);  // 0 if missing
bool appendUserFile(const char* path, const char* data, size_t len);
bool writeUserFile(const char* path, const char* data, size_t len);  // replaced atomically
void removeUserFile(const char* path);
void userFileRotate(const char* path, const char* oldPath);  // path becomes oldPath

// Factory reset: erases all personal data.
void eraseUserData();

// Today's date in local time (no date before the clock is set).
personal::Date localToday();
// Whether the screen font can draw this character (for the text filter).
bool screenFontHas(uint32_t codepoint);

#define USER_TASKS_FILE "/tasks.json"
#define USER_NOTE_FILE "/notes.json"
#define USER_BDAY_FILE "/birthdays.json"
#define USER_BDAY_FEED_FILE "/bday_feed.json"
