#pragma once
// Tasks (docs/plans/STAGE4_PERSONAL_WIDGETS.md): the list as stored in /tasks.json and sent by the
// API ({"tasks": [{"title", "due", "prio", "done"}]}), its limits, and what the widget shows
// (open tasks in display order, due labels). Plain C++, tested on the PC.
#include <ArduinoJson.h>

#include "../i18n/Dates.h"
#include "PersonalText.h"

namespace personal {

constexpr int MAX_TASKS = 30;
constexpr size_t TASK_TITLE_CHARS = 60;
constexpr size_t TASK_TITLE_BYTES = 2 * TASK_TITLE_CHARS + 8;  // umlauts take 2 bytes, € 3 (cut by bytes too)

enum Priority : uint8_t { PRIO_HIGH = 1, PRIO_NORMAL = 2, PRIO_LOW = 3 };

struct Task {
  char title[TASK_TITLE_BYTES] = "";
  Date due;
  uint8_t prio = PRIO_NORMAL;
  bool done = false;
  Date doneAt;  // when it was ticked off (done tasks are removed after DONE_KEEP_DAYS)
};

constexpr int DONE_KEEP_DAYS = 30;

struct TaskList {
  Task items[MAX_TASKS];
  int count = 0;
};

// Parses and checks a task list. Titles are trimmed and, with hasGlyph, filtered (characters the
// screen font can't show are removed and listed in `removed`); empty titles are dropped. Returns
// false (and a message for people in err, in `lang`: the setup app's language, English for the log)
// for broken JSON, more than MAX_TASKS tasks or a bad date.
inline bool parseTasks(const char* json, size_t len, TaskList& out, char* err, size_t errCap,
                       bool (*hasGlyph)(uint32_t) = nullptr, char* removed = nullptr, size_t removedCap = 0,
                       i18n::Lang lang = i18n::Lang::En) {
  out.count = 0;
  if (err && errCap) err[0] = '\0';
  if (removed && removedCap) removed[0] = '\0';
  JsonDocument doc;
  if (deserializeJson(doc, json, len) || !doc["tasks"].is<JsonArrayConst>()) {
    if (err) std::snprintf(err, errCap, "%s", i18n::text(i18n::S::TasksUnreadable, lang));
    return false;
  }
  JsonArrayConst arr = doc["tasks"].as<JsonArrayConst>();
  if (arr.size() > static_cast<size_t>(MAX_TASKS)) {
    if (err) std::snprintf(err, errCap, i18n::text(i18n::S::TasksTooMany, lang), MAX_TASKS);
    return false;
  }
  for (JsonObjectConst t : arr) {
    Task& task = out.items[out.count];
    task = Task{};
    char raw[TASK_TITLE_BYTES * 2];
    copyChars(raw, sizeof(raw), t["title"] | "", TASK_TITLE_CHARS * 2);
    if (hasGlyph) {
      char rem[32];
      filterText(raw, hasGlyph, false, rem, sizeof(rem));
      if (removed && rem[0] && std::strlen(removed) + std::strlen(rem) < removedCap) std::strcat(removed, rem);
    }
    trimSpaces(raw);
    if (!raw[0]) continue;
    copyChars(task.title, sizeof(task.title), raw, TASK_TITLE_CHARS);
    if (!parseIsoDate(t["due"] | "", task.due)) {
      if (err) std::snprintf(err, errCap, i18n::text(i18n::S::TaskBadDate, lang), task.title);
      return false;
    }
    int prio = t["prio"] | 2;
    task.prio = static_cast<uint8_t>(prio < 1 ? 1 : prio > 3 ? 3 : prio);
    task.done = t["done"] | false;
    if (task.done) parseIsoDate(t["doneAt"] | "", task.doneAt);  // a bad date: unknown (set on save)
    out.count++;
  }
  return true;
}

// Writes the list as JSON; returns the length (0 if it doesn't fit).
inline size_t writeTasks(const TaskList& list, char* out, size_t cap) {
  JsonDocument doc;
  JsonArray arr = doc["tasks"].to<JsonArray>();
  for (int i = 0; i < list.count; i++) {
    const Task& t = list.items[i];
    JsonObject o = arr.add<JsonObject>();
    o["title"] = t.title;
    char due[11];
    formatIsoDate(t.due, due, sizeof(due));
    o["due"] = due;
    o["prio"] = t.prio;
    o["done"] = t.done;
    if (t.done && t.doneAt.valid()) {
      char d[11];
      formatIsoDate(t.doneAt, d, sizeof(d));
      o["doneAt"] = d;
    }
  }
  size_t need = measureJson(doc);
  if (need + 1 > cap) return 0;
  return serializeJson(doc, out, cap);
}

// Display order of the open tasks: overdue first, then by due date (without date last), then by
// priority, then in the order they were added. Writes indexes into `order`; returns how many.
inline int openTaskOrder(const TaskList& list, const Date& today, int* order, int max) {
  int n = 0;
  for (int i = 0; i < list.count && n < max; i++)
    if (!list.items[i].done) order[n++] = i;
  long todayN = today.valid() ? dayNumber(today) : 0;
  auto key = [&](int i, long& dueKey, int& prio) {
    const Task& t = list.items[i];
    dueKey = t.due.valid() ? dayNumber(t.due) : 0x7FFFFFFFL;
    if (t.due.valid() && today.valid() && dueKey < todayN) dueKey = -1000000 + dueKey;  // overdue
    prio = t.prio;
  };
  // insertion sort: stable, at most 30 items
  for (int a = 1; a < n; a++) {
    int v = order[a];
    long kv;
    int pv;
    key(v, kv, pv);
    int b = a - 1;
    while (b >= 0) {
      long kb;
      int pb;
      key(order[b], kb, pb);
      if (kb < kv || (kb == kv && pb <= pv)) break;
      order[b + 1] = order[b];
      b--;
    }
    order[b + 1] = v;
  }
  return n;
}

// Short due label: "heute" / "today", "morgen" / "tomorrow", a weekday within the next 6 days ("Fr" /
// "Fri"), else "24.11."; overdue tasks get their date and overdue = true. Empty without a due date.
inline void dueLabel(const Task& t, const Date& today, char* out, size_t cap, bool* overdue, i18n::Lang lang) {
  if (overdue) *overdue = false;
  if (cap) out[0] = '\0';
  if (!t.due.valid()) return;
  if (!today.valid()) {
    std::snprintf(out, cap, "%d.%d.", t.due.d, t.due.m);
    return;
  }
  long diff = dayNumber(t.due) - dayNumber(today);
  if (diff < 0) {
    if (overdue) *overdue = true;
    std::snprintf(out, cap, "%d.%d.", t.due.d, t.due.m);
  } else if (diff == 0) {
    std::snprintf(out, cap, "%s", i18n::text(i18n::S::DueToday, lang));
  } else if (diff == 1) {
    std::snprintf(out, cap, "%s", i18n::text(i18n::S::DueTomorrow, lang));
  } else if (diff <= 6) {
    std::snprintf(out, cap, "%s", i18n::weekdayShort((weekday(t.due) + 1) % 7, lang));  // Monday = 1 there
  } else {
    std::snprintf(out, cap, "%d.%d.", t.due.d, t.due.m);
  }
}

// Saving an edited list: a done task keeps the date it was ticked off (the app may not send it
// back: taken from the stored list by title), a newly ticked one gets today.
inline void stampDoneDates(TaskList& list, const TaskList& stored, const Date& today) {
  for (int i = 0; i < list.count; i++) {
    Task& t = list.items[i];
    if (!t.done) {
      t.doneAt = Date{};
      continue;
    }
    if (t.doneAt.valid()) continue;
    for (int k = 0; k < stored.count; k++)
      if (stored.items[k].done && stored.items[k].doneAt.valid() && std::strcmp(stored.items[k].title, t.title) == 0) {
        t.doneAt = stored.items[k].doneAt;
        break;
      }
    if (!t.doneAt.valid()) t.doneAt = today;
  }
}

// Removes done tasks ticked off DONE_KEEP_DAYS or more days ago (order kept). Returns how many.
inline int purgeDoneTasks(TaskList& list, const Date& today, int keepDays = DONE_KEEP_DAYS) {
  if (!today.valid()) return 0;
  int w = 0, removed = 0;
  long todayN = dayNumber(today);
  for (int r = 0; r < list.count; r++) {
    const Task& t = list.items[r];
    if (t.done && t.doneAt.valid() && todayN - dayNumber(t.doneAt) >= keepDays) {
      removed++;
      continue;
    }
    if (w != r) list.items[w] = list.items[r];
    w++;
  }
  list.count = w;
  return removed;
}

}  // namespace personal
