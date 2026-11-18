In order to create the .h file for the fonts, here is the exact command. Run it from this `src/ui/fonts/` folder (the converter is [tools/ListFontConvertV4.py](../../../tools/ListFontConvertV4.py); it writes `<output>.h` into the current folder). You can modify the font (which must be in the folder). If you change the size, you will need to adjust the code for the spaces.

python3 ../../../tools/ListFontConvertV4.py --font Montserrat-VariableFont_wght.ttf --size 16 --threshold 20 --output epaperFont --glyphs "# %*+,-./0123456789:=?ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz~°³µÀÇÈÉàçèé"

Extra glyphs of `epaperFont` (build 70: umlauts, ß, €, quotes and `! " & ' ( ) ; @ [ ]` for tasks and notes). The original conversion can't be reproduced exactly with today's Montserrat file, so the extra glyphs are made separately (weight 600 matches the existing ones best) and merged in; the existing 85 glyphs stay byte for byte:

python3 ../../../tools/ListFontConvertV4.py --font Montserrat-VariableFont_wght.ttf --size 16 --threshold 20 --weight 600 --output extra --glyphs "!\"&'();@[]ÄÖÜßäöü€–‘’“”„…"
python3 ../../../tools/fonts/merge_sparse_font.py epaperFont.h extra.h epaperFont.h

(Montserrat: google/fonts, `ofl/montserrat/Montserrat[wght].ttf`, SIL Open Font License. The converter needs `pip install freetype-py`.)

python3 ../../../tools/ListFontConvertV4.py --font materialdesignicons-webfont.ttf --size 22 --threshold 24 --output MDI_22_Sparse --glyphs "0xF0026,0xF0079,0xF007A,0xF007B,0xF007C,0xF007D,0xF007E,0xF007F,0xF0080,0xF0081,0xF0082,0xF0085,0xF00F5,0xF018B,0xF01FC,0xF0210,0xF0328,0xF050F,0xF053E,0xF058E,0xF05A9,0xF05AA,0xF05B8,0xF0697,0xF06BD,0xF072E,0xF0737,0xF0788,0xF08BC,0xF08BD,0xF08BE,0xF08BF,0xF0D43,0xF0E1E,0xF0E5B,0xF1294,0xF1295,0xF1299,0xF1446,0xF1452,0xF1797,0xF1B52,0xF1B9A,"

python3 ../../../tools/ListFontConvertV4.py --font materialdesignicons-webfont.ttf --size 80 --threshold 22 --output MDI_80_Sparse --glyphs "0xF0590,0xF0591,0xF0592,0xF0593,0xF0594,0xF0595,0xF0596,0xF0597,0xF0598,0xF0599,0xF059A,0xF059B,0xF059C,0xF059D,0xF059E,0xF067E,0xF067F,0xF0898,0xF0E6E,0xF0F2F,0xF0F30,0xF0F31,0xF0F32,0xF0F33,0xF0F34,0xF0F35,0xF0F36,0xF0F37,0xF0F38,0xF14E4"

python3 ../../../tools/ListFontConvertV4.py --font high-speed.regular.ttf --size 120 --threshold 40 --output HighSpeed_120 --glyphs "0123456789:" 
