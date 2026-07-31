#!/usr/bin/env python3
# PDFRender - convert a 32bpp new-format RISC OS sprite file to PNG
# for host-side inspection of rendered output.
#
# Sprite file = sprite area without its first word: [nsprites, first,
# free] then sprites.  Sprite: next, name[12], wwords-1, hlines-1,
# firstbit, lastbit, imgoff, maskoff, mode.  32bpp: red in bits 0-7.
#
# Copyright (c) RISC OS Technologies 2026.
# SPDX-License-Identifier: CDDL-1.0
# Licensed under the Common Development and Distribution License 1.0; see LICENSE.

import struct
import sys
import zlib


def png_chunk(tag, data):
    c = tag + data
    return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c))


def write_png(path, w, h, rows):
    raw = b"".join(b"\x00" + r for r in rows)
    png = (b"\x89PNG\r\n\x1a\n"
           + png_chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
           + png_chunk(b"IDAT", zlib.compress(raw, 6))
           + png_chunk(b"IEND", b""))
    open(path, "wb").write(png)


def main():
    if len(sys.argv) != 3:
        print("usage: spr2png.py <spritefile> <out.png>")
        return 1
    d = open(sys.argv[1], "rb").read()
    nspr, first, free = struct.unpack_from("<III", d, 0)
    off = first - 4                      # file omits the size word
    nxt, = struct.unpack_from("<I", d, off)
    wwords, hlines, firstbit, lastbit, imgoff, maskoff, mode = \
        struct.unpack_from("<7I", d, off + 16)
    w = wwords + 1                       # 32bpp: 1 word = 1 pixel
    h = hlines + 1
    spritetype = (mode >> 27) & 31
    if spritetype != 6:
        print("not a 32bpp (type 6) sprite: type", spritetype)
        return 1
    img = off + imgoff
    rows = []
    for y in range(h):
        row = bytearray()
        base = img + y * w * 4
        for x in range(w):
            px, = struct.unpack_from("<I", d, base + x * 4)
            row += bytes((px & 0xFF, (px >> 8) & 0xFF, (px >> 16) & 0xFF))
        rows.append(bytes(row))
    write_png(sys.argv[2], w, h, rows)
    print("wrote %s (%dx%d)" % (sys.argv[2], w, h))
    return 0


if __name__ == "__main__":
    sys.exit(main())
