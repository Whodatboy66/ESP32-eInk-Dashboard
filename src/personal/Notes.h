#pragma once
// The note (docs/plans/STAGE4_PERSONAL_WIDGETS.md): /notes.json and the API ({"note": "..."}), and
// wrapping it into lines for the widget. Plain C++, tested on the PC.
#include <ArduinoJson.h>

#include "../i18n/Strings.h"
#include "PersonalText.h"

namespace personal {

constexpr size_t NOTE_CHARS = 400;
constexpr size_t NOTE_BYTES = 2 * NOTE_CHARS + 1;  // umlauts take 2 bytes (cut by bytes too)

// Parses {"note": "..."}: line breaks kept, cut at NOTE_CHARS, filtered with hasGlyph. A message in
// `lang` (as parseTasks).
inline bool parseNote(const char* json, size_t len, char* note, size_t cap, char* err, size_t errCap,
                      bool (*hasGlyph)(uint32_t) = nullptr, char* removed = nullptr, size_t removedCap = 0,
                      i18n::Lang lang = i18n::Lang::En) {
  if (err && errCap) err[0] = '\0';
  if (cap) note[0] = '\0';
  JsonDocument doc;
  if (deserializeJson(doc, json, len) || !doc["note"].is<const char*>()) {
    if (err) std::snprintf(err, errCap, "%s", i18n::text(i18n::S::NoteUnreadable, lang));
    return false;
  }
  const char* text = doc["note"].as<const char*>();
  if (utf8Count(text) > NOTE_CHARS) {
    if (err) std::snprintf(err, errCap, i18n::text(i18n::S::NoteTooLong, lang), (unsigned)NOTE_CHARS);
    return false;
  }
  copyChars(note, cap, text, NOTE_CHARS);
  if (hasGlyph) filterText(note, hasGlyph, true, removed, removedCap);
  trimSpaces(note);
  return true;
}

inline size_t writeNote(const char* note, char* out, size_t cap) {
  JsonDocument doc;
  doc["note"] = note;
  if (measureJson(doc) + 1 > cap) return 0;
  return serializeJson(doc, out, cap);
}

struct TextLine {
  int start;  // byte offset into the text
  int len;    // bytes
};

// Wraps text into lines no wider than maxWidth (width(text + start, len) measures a piece in
// pixels): breaks at spaces, at line breaks, and inside words that are wider than a whole line.
// Returns the number of lines (at most maxLines); *cut = true when text was left over.
template <typename WidthFn>
int wrapText(const char* text, int maxWidth, WidthFn width, TextLine* lines, int maxLines, bool* cut) {
  int n = 0;
  int pos = 0;
  const int total = static_cast<int>(std::strlen(text));
  if (cut) *cut = false;
  while (pos < total) {
    if (n >= maxLines) {
      if (cut) *cut = true;
      return n;
    }
    // Longest piece from pos that fits: grow word by word.
    int lineEnd = pos, lastFit = -1, i = pos;
    bool hardBreak = false;
    while (i <= total) {
      bool atEnd = i == total || text[i] == ' ' || text[i] == '\n';
      if (atEnd) {
        if (width(text + pos, i - pos) <= maxWidth) {
          lastFit = i;
          if (i == total || text[i] == '\n') {
            hardBreak = i < total;
            break;
          }
        } else {
          break;
        }
      }
      if (i == total) break;
      int l;
      utf8Decode(text + i, &l);
      i += l;
    }
    if (lastFit >= 0) {
      lineEnd = lastFit;
    } else {
      // A single word wider than the line: as many characters as fit (at least one).
      int j = pos;
      lineEnd = pos;
      while (j < total && text[j] != ' ' && text[j] != '\n') {
        int l;
        utf8Decode(text + j, &l);
        if (width(text + pos, j + l - pos) > maxWidth && lineEnd > pos) break;
        j += l;
        lineEnd = j;
      }
    }
    lines[n++] = TextLine{pos, lineEnd - pos};
    pos = lineEnd;
    if (hardBreak || (pos < total && (text[pos] == ' ' || text[pos] == '\n'))) pos++;  // skip the break
    (void)hardBreak;
  }
  return n;
}

}  // namespace personal
