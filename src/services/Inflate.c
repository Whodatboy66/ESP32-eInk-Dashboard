// miniz 3.0.2's inflate (tinfl) for HttpBody.h, compiled into the firmware (build 85): the same code
// the PC tests run (tests/host/test_http_body.cpp). The chip's ROM has an older tinfl (miniz 1.15);
// with it every packed calendar download failed on the device (builds 83-84), in ways the PC
// couldn't reproduce. Only decompression is built: no deflate, zlib, archive or file functions.
// MIT License: third_party/miniz/LICENSE.
#define MINIZ_NO_STDIO
#define MINIZ_NO_TIME
#define MINIZ_NO_ARCHIVE_APIS
#define MINIZ_NO_DEFLATE_APIS
#define MINIZ_NO_ZLIB_APIS
#include "MinizNames.h"  // hb_tinfl_decompress & co.: the C6 ROM claims the plain names
#include "../../third_party/miniz/miniz.c"
