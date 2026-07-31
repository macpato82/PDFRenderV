#!/usr/bin/env python3
"""
Build the !Sprites file for the Viewer application from a PNG.

RISC OS sprite conventions used here were taken from the applications
already installed on the target, not from memory:

  * a large application icon is 68 pixels tall at 180x180 dpi (so one
    pixel is one OS unit) - !PipeDream, !PrivatEye and !SciCalc all do
    this; the small icon is 34 tall
  * for a 32bpp (type 6) sprite, bit 31 of the mode word selects an
    8bpp alpha mask instead of the usual 1bpp one.  !PackMan and
    !Store ship &B01680B5 with a 40-byte mask row for 37 pixels;
    !Git ships &301680B5 with an 8-byte row.  Same type, same dpi -
    bit 31 is the only difference.

Sprites already in the destination file that we do not generate (the
filetype icons) are carried over untouched.

Copyright (c) RISC OS Technologies 2026.
SPDX-License-Identifier: CDDL-1.0
Licensed under the Common Development and Distribution License 1.0; see LICENSE.
"""

import struct
import sys

from PIL import Image

# 32bpp, 180x180 dpi, alpha mask (bit 31)
MODE_32BPP_180_ALPHA = (1 << 31) | (6 << 27) | (180 << 14) | (180 << 1) | 1


def build_sprite(name, img, height):
    """One sprite block: 44-byte header, XRGB32 image, 8bpp alpha."""
    w0, h0 = img.size
    width = max(1, round(w0 * height / h0))
    im = img.convert("RGBA").resize((width, height), Image.LANCZOS)
    px = im.load()

    image = bytearray()
    for y in range(height):
        for x in range(width):
            r, g, b, _a = px[x, y]
            image += bytes((r, g, b, 0))       # little-endian XRGB

    maskrow = ((width + 3) // 4) * 4           # 1 byte per pixel, word aligned
    mask = bytearray()
    for y in range(height):
        row = bytearray(maskrow)
        for x in range(width):
            row[x] = px[x, y][3]
        mask += row

    hdr = bytearray(44)
    nm = name.encode("latin1")[:12]
    hdr[4:4 + len(nm)] = nm
    struct.pack_into(
        "<7I", hdr, 16,
        width - 1,              # width in words - 1 (32bpp: 1 word/pixel)
        height - 1,             # height in scanlines - 1
        0,                      # first bit used
        31,                     # last bit used
        44,                     # offset to image
        44 + len(image),        # offset to mask
        MODE_32BPP_180_ALPHA,
    )
    block = bytes(hdr) + bytes(image) + bytes(mask)
    return struct.pack("<I", len(block)) + block[4:]


def read_existing(path):
    """Return {name: raw block} for the sprites already in a file."""
    try:
        d = open(path, "rb").read()
    except IOError:
        return {}
    if len(d) < 12:
        return {}
    count, first, _free = struct.unpack("<3I", d[0:12])
    out, off = {}, first - 4
    for _ in range(count):
        if off + 44 > len(d):
            break
        nxt, = struct.unpack("<I", d[off:off + 4])
        if nxt <= 0 or off + nxt > len(d):
            break
        name = d[off + 4:off + 16].split(b"\0")[0].decode("latin1")
        out[name] = d[off:off + nxt]
        off += nxt
    return out


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: mksprites.py <icon.png> <!Sprites file>")
    png, dest = sys.argv[1], sys.argv[2]

    img = Image.open(png)
    keep = read_existing(dest)
    blocks = [
        build_sprite("!viewer", img, 68),
        build_sprite("sm!viewer", img, 34),
    ]
    generated = {"!viewer", "sm!viewer"}
    for name, raw in keep.items():
        if name not in generated:
            blocks.append(raw)               # filetype icons, untouched

    body = b"".join(blocks)
    header = struct.pack("<3I", len(blocks), 16, len(body) + 16)
    open(dest, "wb").write(header + body)

    print("%s: %d sprites, %d bytes" % (dest, len(blocks), len(body) + 12))
    for b in blocks:
        name = b[4:16].split(b"\0")[0].decode("latin1")
        ww, hl = struct.unpack("<2I", b[16:24])
        mode, = struct.unpack("<I", b[40:44])
        print("   %-12s %dx%d  mode &%08X" % (name, ww + 1, hl + 1, mode))


if __name__ == "__main__":
    main()
