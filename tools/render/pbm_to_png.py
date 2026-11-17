#!/usr/bin/env python3
"""Converts a binary PBM (P4) image to PNG (1-bit grayscale), standard library only."""
import struct
import sys
import zlib


def main(src, dst):
    data = open(src, "rb").read()
    parts = data.split(b"\n", 2)
    assert parts[0] == b"P4", "not a binary PBM"
    w, h = map(int, parts[1].split())
    bits = parts[2]
    row = (w + 7) // 8
    raw = bytearray()
    for y in range(h):
        raw.append(0)  # filter: none
        raw.extend(b ^ 0xFF for b in bits[y * row:(y + 1) * row])  # PBM 1 = black, PNG 1 = white
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 1, 0, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
    open(dst, "wb").write(png)


if __name__ == "__main__":
    main(*sys.argv[1:3])
