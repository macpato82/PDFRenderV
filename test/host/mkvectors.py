#!/usr/bin/env python3
# PDFRender - generate inflate test vectors using Python's zlib as the
# oracle.  Covers: empty, tiny, text, binary, high-entropy (stored
# blocks), highly repetitive (deep LZ77), every compression level,
# raw-deflate (no zlib wrapper), and real FlateDecode streams pulled
# out of the PDFs in /Volumes/Nvme/PDFs.
#
# Copyright (c) RISC OS Technologies 2026.
# SPDX-License-Identifier: CDDL-1.0
# Licensed under the Common Development and Distribution License 1.0; see LICENSE.

import os
import random
import re
import sys
import zlib

OUT = os.path.join(os.path.dirname(__file__), "vectors")
random.seed(42)

vectors = []


def add(raw, z):
    vectors.append((raw, z))


def zl(raw, level=6):
    return zlib.compress(raw, level)


# --- synthetic ---------------------------------------------------------
add(b"", zl(b""))
add(b"a", zl(b"a"))
add(b"Hello, RISC OS PDF world!\n" * 4, zl(b"Hello, RISC OS PDF world!\n" * 4))

text = ("Lorem ipsum dolor sit amet, consectetur adipiscing elit. " * 200).encode()
for lvl in (0, 1, 6, 9):
    add(text, zl(text, lvl))

rep = b"AB" * 40000                       # deep match chains
add(rep, zl(rep, 9))

rnd = bytes(random.getrandbits(8) for _ in range(65536))   # incompressible
for lvl in (0, 1, 9):
    add(rnd, zl(rnd, lvl))

mixed = b"".join(
    (b"run" * random.randint(1, 60)) + bytes(random.getrandbits(8)
                                             for _ in range(random.randint(1, 50)))
    for _ in range(300))
for lvl in (1, 6, 9):
    add(mixed, zl(mixed, lvl))

# raw deflate, no zlib wrapper (broken generators do this)
co = zlib.compressobj(6, zlib.DEFLATED, -15)
raw_deflate = co.compress(text) + co.flush()
add(text, raw_deflate)

# many small random buffers across levels
for i in range(30):
    n = random.randint(0, 4096)
    data = bytes(random.getrandbits(8) for _ in range(n)) if i % 2 else \
        bytes(random.choice(b"abcde\n ") for _ in range(n))
    add(data, zl(data, random.choice((1, 6, 9))))

# --- real PDF streams --------------------------------------------------
# Pull FlateDecode streams out of real PDFs: find stream...endstream
# spans, try to inflate them with zlib; keep the ones zlib accepts.
pdfdir = "/Volumes/Nvme/PDFs"
picked = 0
for name in ("RiscOSDirectLogo.pdf", "viewfinder.pdf",
             "Acorn Risc PC Technical Reference Manual-opt.pdf"):
    path = os.path.join(pdfdir, name)
    if not os.path.exists(path):
        continue
    blob = open(path, "rb").read()
    for m in re.finditer(rb"stream\r?\n", blob):
        start = m.end()
        end = blob.find(b"endstream", start)
        if end < 0:
            continue
        cand = blob[start:end].rstrip(b"\r\n")
        try:
            raw = zlib.decompress(cand)
        except zlib.error:
            continue
        add(raw, cand)
        picked += 1
        if picked >= 40:
            break
    if picked >= 40:
        break

# --- write -------------------------------------------------------------
os.makedirs(OUT, exist_ok=True)
for f in os.listdir(OUT):
    os.unlink(os.path.join(OUT, f))
for i, (raw, z) in enumerate(vectors):
    open(os.path.join(OUT, "%03d.raw" % i), "wb").write(raw)
    open(os.path.join(OUT, "%03d.z" % i), "wb").write(z)
print("wrote %d vectors (%d from real PDFs)" % (len(vectors), picked))
