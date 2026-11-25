#pragma once
// Own names for the miniz functions compiled into the firmware (src/services/Inflate.c): the ESP32-C6
// ROM linker script assigns tinfl_decompress & co. to the ROM's older copy (miniz 1.15) and that
// assignment wins over a compiled function of the same name (build 84 still ran the ROM's). Included
// before miniz.h everywhere (HttpBody.h, Inflate.c, the PC tests' build of miniz.c).
#define tinfl_decompress hb_tinfl_decompress
#define tinfl_decompress_mem_to_heap hb_tinfl_decompress_mem_to_heap
#define tinfl_decompress_mem_to_mem hb_tinfl_decompress_mem_to_mem
#define tinfl_decompress_mem_to_callback hb_tinfl_decompress_mem_to_callback
#define tinfl_decompressor_alloc hb_tinfl_decompressor_alloc
#define tinfl_decompressor_free hb_tinfl_decompressor_free
#define mz_adler32 hb_mz_adler32
#define mz_crc32 hb_mz_crc32
#define mz_free hb_mz_free
#define mz_version hb_mz_version
