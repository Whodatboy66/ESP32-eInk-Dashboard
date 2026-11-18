# Stage 4 plan: personal widgets (tasks, notes, birthdays)

Detailed plan for stage 4 of [IMPLEMENTATION_PLAN.md](../IMPLEMENTATION_PLAN.md) (spec §5.3). Builds on
stage 3 (widget registry, layout engine). Stage 5 ([STAGE5_SETUP_APP.md](STAGE5_SETUP_APP.md)) adds the
pages to edit them; this stage provides the storage, the widgets and the JSON API the app uses.

**Done when:** tasks, notes and birthdays are editable through the JSON API (setup mode), shown by
their widgets, and survive restarts and over-the-air updates.

## Design decisions

1. **Storage: LittleFS on the `userdata` partition** (128 KB, 0x3D0000, already in the partition
   table since stage 1, so no USB flash). OTA only writes the app slots, so the data survives
   updates. One JSON file per kind: `/tasks.json`, `/notes.json`, `/birthdays.json`, plus
   `/bday_feed.json` (birthdays read from a calendar link). Written to a temporary file first, then
   renamed, so a power cut can't leave half a file. Mounted only when needed (drawing these widgets,
   the API). A damaged file system is formatted (the data is gone, the device keeps working); a
   damaged file is ignored and logged.
2. **Limits** (the API refuses more, with a message):
   - tasks: 30, title up to 60 characters, optional due date, priority high / normal / low, done;
   - note: one note, up to 400 characters, line breaks kept;
   - birthdays: 50 entered by hand (name up to 32 characters, day, month, optional year for the age),
     plus the ones read from a calendar link.
3. **What the widgets show:**
   - **Tasks** (min 2 × 1): open tasks, overdue first, then by due date, then priority, then the order
     they were added; a box for each, the due date on the right ("today", "Fri", "24.11."), overdue
     marked; as many as fit, "+3 more" when they don't. Done tasks aren't shown (they stay in the list
     until deleted).
   - **Note** (min 1 × 1): the text, wrapped at word boundaries, as many lines as fit, "…" if cut.
   - **Birthdays** (min 2 × 1): the next ones (60 days ahead): "Today", "Tomorrow", "in 12 d" or the
     date, the name, the age when the year is known. A birthday on 29 February counts on 28 February
     in other years.
4. **Birthdays from a calendar:** a separate setting "Birthday calendar link" (e.g. Google's
   birthday calendar as an iCal link), not one of the three agenda feeds, so birthdays don't fill the
   agenda. Read once a day (and after setup) when the Birthdays widget is on the screen, with the
   existing iCal parser (all events in the next 60 days, yearly repeats expanded); kept in
   `/bday_feed.json`, merged with the hand-entered ones (same name and day = once). A failed read keeps
   the last list and sets the problem "Birthdays".
5. **Configured = has content** (the registry rule of stage 3): a template shows these widgets only
   when there is something to show; the gap filling closes the space otherwise. (Box layouts of
   stage 5 show a hint instead.)
6. **Redraw:** on every full refresh, and once when the date changes (due labels, "in 3 d"), as a
   partial refresh. Data changes happen in setup mode, which ends with a restart (full refresh).
7. **New built-in template "Personal":** Clock + Note on top, Tasks (2 × 2) on the left, Birthdays and
   Weather on the right. Classic stays unchanged (its pixel layout has no room).
8. **Text:** the screen font gets umlauts, ß, € and `! " & ' ( ) ; @ [ ]` (merged into the existing
   font, existing letters unchanged: `tools/fonts/merge_sparse_font.py`). Characters the font still
   lacks are dropped when saving, so nothing shows up as a gap.
9. **JSON API** (setup mode; stage 5 also opens it on the home network): `GET` / `PUT`
   `/api/tasks`, `/api/notes`, `/api/birthdays` (whole list replaced, validated, at most 8 KB).
   Described in [docs/SETUP_API.md](../SETUP_API.md).
10. **Factory reset** also erases this data (the device may be given away).

## Steps

1. Font glyphs (decision 8).
2. Pure logic, host-tested: `src/personal/Tasks.h`, `Notes.h`, `Birthdays.h` (parse / write JSON,
   limits, display order, days until and age, text wrapping), `TextFilter.h`.
3. `src/personal/UserStore.{h,cpp}`: LittleFS mount, atomic writes, factory reset.
4. Widgets (`src/widgets/Personal.cpp`), registry entries, template "Personal", PC renderer data.
5. Birthday feed: `fetchBirthdayFeed()` in `src/services/Calendar.cpp`, settings key `bday_url`.
6. Wake cycle: date-change redraw, feed fetch, problem flag.
7. API routes in the setup server.
8. `FW_BUILD` 70, CHANGELOG, user guide, images.

## Tests

- Host: JSON round trips, limits (31st task refused, long titles cut), broken JSON → empty with an
  error, display order (overdue, due, priority, added), labels ("today", weekday, date), days until
  and age across a year end and for 29 February, merge of hand-entered and feed birthdays, wrapping
  (long words, line breaks, "…"), character filter.
- PC renderer: the Personal template with sample data, empty lists, long texts.
- Device: data kept after a restart and after an OTA update; a factory reset erases it.

## Risks

| Risk | Counter |
|---|---|
| LittleFS mount time on every wake | mounted only when one of the widgets is drawn (full refresh, date change) |
| Flash wear | writes only when the user saves or once a day for the feed |
| The birthday feed is large (all contacts) | streamed like the agenda feeds; only the next 60 days are kept (at most 50 entries) |

## Out of scope

Editing pages in the app (stage 5), ticking tasks off on the device (one button only), several
notes, reminders.

## Result (build 70)

Done in code and rendered on the PC (`docs/images/layouts/personal*.png`), not yet checked on the
device. Decisions taken during the work:

- **The extra font glyphs are merged, not regenerated:** today's Montserrat file doesn't reproduce
  the original conversion (no weight gives the same bitmaps), so only the 25 new characters were made
  (weight 600, closest to the existing ones) and added; the 85 existing glyphs are byte for byte the
  same, so every existing screen stays pixel-identical. `tools/ListFontConvertV4.py` got a `--weight`
  option for this.
- **"Has content" flags in NVS** (`user_flags`): the registry decides on every wake whether a widget
  is configured; reading the flags avoids mounting LittleFS each minute. They are updated when the
  data is saved.
- **The time zone is applied at the start of every wake.** Until now only the clock widget set it
  (while drawing); with layouts without a clock (stage 5) the agenda would have shown UTC.
- **The birthday link is read once a day,** and right away when the link changed (a hash in RTC
  memory); after a failure once an hour (problem "Birthdays").
- **The lists live on the heap,** allocated on first use: as fixed buffers they overflowed the
  classic ESP32's static RAM by 56 KB (the C6 has more). Text limits are counted in characters and
  bytes (2 bytes per character: umlauts fit, a title full of € is cut a little earlier).
- **High priority** is a filled box; low and normal look the same on the screen (the order differs).
- Due labels and the birthday lines are in English like the rest of the screen.

Verification: host tests (`tests/host/test_personal.cpp`: dates, UTF-8, filter, limits, order,
labels, wrapping, birthdays across the year end and on 29 February), all profiles compile (79-80 % of
the app slot, LittleFS adds about 90 KB; ESP32: 118 KB static RAM), images reviewed (normal, empty, long texts).
