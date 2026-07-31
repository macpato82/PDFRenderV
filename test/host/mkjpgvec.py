#!/usr/bin/env python3
"""PDFRender - JPEG test vectors and their reference decodes.

Writes baseline and progressive encodings of the same pictures, with
PIL's own decode of each as a PPM.  4:4:4 for the ones compared
tightly: at 4:2:0 the difference against libjpeg is dominated by its
triangular chroma upsampling, which is a separate matter from whether
the entropy decoding is right.

Copyright (c) RISC OS Technologies 2026.
SPDX-License-Identifier: CDDL-1.0
"""
import os
from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "jpgvec")


def make(name, size, grey=False, **kw):
    im = Image.new("RGB", size, (250, 248, 240))
    dr = ImageDraw.Draw(im)
    for i in range(0, size[0], 11):
        dr.line([(i, 0), (size[0] - i, size[1])],
                fill=((i * 7) % 256, (i * 3) % 256, (255 - i) % 256), width=2)
    dr.ellipse([size[0] // 6, size[1] // 6, size[0] * 5 // 6, size[1] * 5 // 6],
               outline=(0, 0, 0), width=3)
    if grey:
        im = im.convert("L").convert("RGB")
    im.save(os.path.join(OUT, name + ".jpg"), "JPEG", **kw)
    Image.open(os.path.join(OUT, name + ".jpg")).convert("RGB").save(
        os.path.join(OUT, name + ".ppm"))


os.makedirs(OUT, exist_ok=True)
make("base444",    (320, 240), progressive=False, quality=90, subsampling=0)
make("prog444",    (320, 240), progressive=True,  quality=90, subsampling=0)
make("prog444o",   (317, 239), progressive=True,  quality=95, subsampling=0)
make("proglo444",  (160, 120), progressive=True,  quality=35, subsampling=0)
make("progbig444", (640, 480), progressive=True,  quality=92, subsampling=0)
make("basegrey",   (200, 150), grey=True, progressive=False, quality=80)
make("proggrey",   (200, 150), grey=True, progressive=True,  quality=80)
print("JPEG vectors written to", OUT)
