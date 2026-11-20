# Plan: new screen look and German / English

Asked for after the device test of 2026-11-20 (build 73): "the theory looks a lot better than the
actual dashboard" (the app's sketch vs. the device's exact preview), no date with the clock, AM/PM
missing in a 2 × 1 clock, and "add German: German default, English selectable, for everything,
including the setup website".

**Done when:** the dashboard on the device looks like the app's sketch (fonts, date under the clock,
headings), and the screen, the setup app and the device's messages are in German by default and in
English when chosen.

## Part 1: languages (build 77)

1. **Setting `lang`:** `de` (default, also for existing devices after the update) or `en`. Set on the
   Device tab, and as the first question of the first-start guide (which always starts in German,
   decided 2026-11-20). Changing it redraws the screen (no restart needed).
2. **Firmware texts through one table** (`src/i18n/Strings.h`): every text on the e-paper (widgets,
   status bar notes, update screens, setup screens, the charge screen, problem names, hints) and every
   text the API sends to the app (problems with their fix, field errors, hints, check results) is a key
   with a German and an English text. No text in the drawing code any more.
3. **Dates and times per language:** weekday and month names, formats ("Fr, 20. November" /
   "Fri, 20 November"), "Heute / Morgen / in 3 T." / "Today / Tomorrow / in 3 d", 24 h default for
   German (12 h stays a setting).
4. **The app:** every text in a dictionary per language (`web/i18n.js`), the language from
   `state.settings.lang`; switching it in the app changes the app at once.
5. **Not translated:** the diagnostics log (technical, stays English), user content (task titles,
   calendar entries, names).
6. **Test:** a host test that every key has both texts and fits where it's drawn (widths measured with
   the screen font for the longest German text, e.g. "Akku leer: bitte laden"); PC renders in both
   languages.

## Part 2: new screen look (build 78)

1. **New fonts from Montserrat** (SIL Open Font License, already used for the text): clock digits
   Montserrat Bold about 110 px (clean digits like the sketch, instead of the dotted HighSpeed font,
   which is also licensed for personal use only); a 22 px semibold font for headings, the date under
   the clock and AM/PM; the 16 px text font stays. Only the needed characters (digits for the clock),
   about 20 KB of flash in total.
2. **Clock:** digits, below them the date ("Freitag, 20. November"), AM/PM next to the digits in the
   22 px font; sizes chosen by the box (2 × 1 up to full screen). In a 2 × 1 box "10:28 AM" and the date
   then fit.
3. **Headings** for the list widgets like in the sketch ("Heute und morgen", "Aufgaben", "Geburtstage",
   "November 2026"), with a line under them.
4. **Month view like the sketch:** today as a black square, days with events underlined (now: black
   circles, which look heavy when a calendar has events on most days).
5. **Weather:** weekday above each icon, "9° / 2°" below.
6. **Status bar:** date left, battery percent with icon and Wi-Fi right (build 76 started this).
7. **Classic gets the new clock too** (decided 2026-11-20: the dotted digits are hard to read with
   reflections and scale badly); its places stay where they are. The HighSpeed font is then gone
   completely (it was licensed for personal use only). Filled digits cost the same energy per refresh
   (an e-paper refresh drives the whole window, black or white) and the same flash; large black areas
   can ghost a little more after many partial refreshes: watched in the device test, an extra full
   refresh after many clock changes if needed.
8. **The app's sketch follows the device:** same headings and order, so the two match.
9. **Check before the device:** PC renders of every ready-made layout and some own ones, next to the
   app's sketch of the same layout, for your review.

## Order

Part 1 first: Part 2 redraws the widgets anyway and then uses the text keys right away. Each part
gets its own build, and you check it before the next one starts.

## Decided (2026-11-20)

- Classic moves to the new filled clock too.
- The first-start guide always starts in German.
