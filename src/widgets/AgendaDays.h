#pragma once
// "Heute und morgen" (stage 7): the days that get a header, in date order, and which of them start
// with the public holiday ("★ Neujahr"): the days of the listed entries plus today / tomorrow when
// they are holidays. Day numbers, so a DST change can't make "tomorrow" the same day. Plain C++,
// tested on the PC (tests/host/test_agenda_days.cpp).

namespace agenda {

struct Day {
  long key = 0;  // day number (personal::dayNumber)
  bool holiday = false;
};

// eventDays: the day of each listed entry (any order, repeats allowed). today = 0: the clock isn't
// set, no holidays. Returns how many days were written (at most cap).
inline int plan(const long* eventDays, int n, long today, bool holidayToday, bool holidayTomorrow, Day* out, int cap) {
  long keys[16];
  int k = 0;
  auto addKey = [&](long d) {
    for (int i = 0; i < k; i++)
      if (keys[i] == d) return;
    if (k < 16) keys[k++] = d;
  };
  for (int i = 0; i < n; i++) addKey(eventDays[i]);
  if (today && holidayToday) addKey(today);
  if (today && holidayTomorrow) addKey(today + 1);
  for (int a = 1; a < k; a++) {  // date order
    long v = keys[a];
    int b = a - 1;
    while (b >= 0 && keys[b] > v) {
      keys[b + 1] = keys[b];
      b--;
    }
    keys[b + 1] = v;
  }
  int m = k < cap ? k : cap;
  for (int i = 0; i < m; i++) {
    out[i].key = keys[i];
    out[i].holiday = today && ((keys[i] == today && holidayToday) || (keys[i] == today + 1 && holidayTomorrow));
  }
  return m;
}

}  // namespace agenda
