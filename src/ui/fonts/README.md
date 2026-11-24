# Screen fonts

Generated, don't edit by hand. Every font is one line of [tools/fonts/fonts.txt](../../../tools/fonts/fonts.txt)
(source file, size, weight, threshold, glyphs); [tools/fonts/make_font.py](../../../tools/fonts/make_font.py)
renders them with FreeType into these headers (format: [SparseFontBase.h](SparseFontBase.h), drawn by
[src/ui/GlyphText.h](../GlyphText.h)).

```
pip install freetype-py
python3 tools/fonts/make_font.py --all FONT_DIR            # all fonts
python3 tools/fonts/make_font.py --only epaperFont FONT_DIR
```

`FONT_DIR` holds the source files:

- `Montserrat-VariableFont_wght.ttf`: google/fonts, `ofl/montserrat/Montserrat[wght].ttf` (SIL Open Font
  License 1.1): the text font, headings, clock and month digits.
- `materialdesignicons-webfont.ttf`: `@mdi/font` 7.4.47, `fonts/` (Pictogrammers Free License): the icons.

To add a character or an icon: add it to its line in `fonts.txt` (characters as they are, icons as
`U+F0XXX` from materialdesignicons.com) and build again. The host test `test_i18n` checks that every
screen text has its glyphs and still fits its place.

Since stage 7b (build 80) all fonts come from this tool; the text font was rebuilt at weight 600 (the
original base glyphs of build 63 could not be reproduced exactly; widths differ by about 0.3 px per
character).
