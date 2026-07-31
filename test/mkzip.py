#!/usr/bin/env python3
"""
Package a directory as a Zip archive RISC OS can actually use.

Filetypes go in the Zip metadata, not in the filename.  A ",ffa" on
the end of a name is a host convention (HostFS, and how this repository
stores its files); unpacked on RISC OS it is just part of the name, so
the module is called "PDFRenderV,ffa" and is untyped - useless.

The real mechanism is the Acorn extra field, checked here against the
archives RISC OS itself ships (HardDisc4.zip) rather than assumed:

    offset  size  contents
    0       2     header id 0x4341 ('AC')
    2       2     size of the data that follows: 20
    4       4     'ARC0'
    8       4     load address
    12      4     execution address
    16      4     attributes
    20      4     zero

For a date-stamped file the filetype lives in the load address:

    load = 0xFFF00000 + (filetype << 8) + (top byte of the timestamp)
    exec = the low 32 bits of the timestamp

where the timestamp is centiseconds since 1900.  Names in the archive
carry no suffix.

Copyright (c) RISC OS Technologies 2026.
SPDX-License-Identifier: CDDL-1.0
Licensed under the Common Development and Distribution License 1.0; see LICENSE.
"""

import os
import struct
import sys
import time
import zipfile

ACORN_ID = 0x4341
RISCOS_EPOCH = 2208988800        # seconds between 1900 and 1970
ATTR_WR = 0x03                   # owner read + write
TYPE_DIR = 0xFFF                 # directories are stamped as text
TYPE_DEFAULT = 0xFFD             # Data, when a name carries no type


def acorn_extra(filetype, mtime):
    cs = int((mtime + RISCOS_EPOCH) * 100)
    load = 0xFFF00000 | ((filetype & 0xFFF) << 8) | ((cs >> 32) & 0xFF)
    exec_ = cs & 0xFFFFFFFF
    body = b"ARC0" + struct.pack("<3I", load, exec_, ATTR_WR) + b"\0\0\0\0"
    return struct.pack("<2H", ACORN_ID, len(body)) + body


def split_type(name):
    """"Foo,ffa" -> ("Foo", 0xFFA); "Foo" -> ("Foo", None)."""
    if len(name) > 4 and name[-4] == "," :
        try:
            return name[:-4], int(name[-3:], 16)
        except ValueError:
            pass
    return name, None


def add(zf, arcname, path, isdir, filetype):
    st = os.stat(path)
    zi = zipfile.ZipInfo(arcname, time.localtime(st.st_mtime)[:6])
    zi.create_system = 13                       # RISC OS
    if isdir:
        zi.external_attr = 0x41C00010           # directory
        zi.compress_type = zipfile.ZIP_STORED
    else:
        zi.external_attr = 0x81800000
        zi.compress_type = zipfile.ZIP_DEFLATED
    zi.extra = acorn_extra(filetype, st.st_mtime)
    if isdir:
        zf.writestr(zi, b"")
    else:
        with open(path, "rb") as f:
            zf.writestr(zi, f.read())
    return filetype


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: mkzip.py <directory> <archive.zip>")
    src, dest = sys.argv[1].rstrip("/"), sys.argv[2]
    root = os.path.dirname(src) or "."
    untyped = []

    with zipfile.ZipFile(dest, "w") as zf:
        for dirpath, dirnames, filenames in os.walk(src):
            dirnames.sort()
            filenames.sort()
            rel = os.path.relpath(dirpath, root).replace(os.sep, "/")
            add(zf, rel + "/", dirpath, True, TYPE_DIR)
            for fn in filenames:
                if fn == ".DS_Store":
                    continue
                clean, ft = split_type(fn)
                if ft is None:
                    ft = TYPE_DEFAULT
                    untyped.append(rel + "/" + fn)
                add(zf, rel + "/" + clean, os.path.join(dirpath, fn),
                    False, ft)

    print("%s written" % dest)
    for name in untyped:
        print("   no filetype in the name, stamped &%03X: %s"
              % (TYPE_DEFAULT, name))


if __name__ == "__main__":
    main()
