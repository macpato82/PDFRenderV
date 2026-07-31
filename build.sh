#!/bin/bash
##
# Build PDFRender with the host-side Norcroft DDE (NorcroftUniversal).
#
#   ./build.sh
#
# One conservative ARM binary: Norcroft emits no ARMv6+/v7-only
# encodings and the core is integer-only (16.16 fixed point), so the
# same rm/PDFRenderV runs on everything from ARMv5 (Iyonix/XScale)
# through ARMv7 (iMX6, Titanium) to ARMv8 AArch32.
#
# Copyright (c) RISC OS Technologies 2026.
# SPDX-License-Identifier: CDDL-1.0
# Licensed under the Common Development and Distribution License 1.0; see LICENSE.
# Closed source - not for redistribution.

set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
D=/Volumes/Nvme/NorcroftUniversal
DDE_HOME="$D"
. "$D/dde-python.sh"
dde_find_python
dde_find_timeout

cd "$HERE"
mkdir -p o rm scrap

EXP="$D/Export"
[ -d "$D/Export-DDE" ] && EXP="$D/Export-DDE"
CPATH="$EXP/APCS-32/C,$EXP/APCS-32/Lib/CLib,$EXP/APCS-32/Lib"
HPATH="$EXP/APCS-32/Hdr/Global,$EXP/APCS-32/Hdr/Interface"

run() {
    ${DDE_TIMEOUT:+$DDE_TIMEOUT 300} "$DDE_PY" "$D/src/romachine.py" \
        -m "$D/modules/FPEmulator,ffa" -m "$D/modules/CLib623,ffa" \
        -d . -p "C=$CPATH" -p "Hdr=$HPATH" \
        -V 'C$LibRoot=C:' -V 'Wimp$ScrapDir=$.scrap' "$@"
}

# NorcroftUniversal tmpnam flake: CMunge occasionally prints its banner,
# emits no object, and still exits 0.  Never trust exit status - check
# the artefact exists (and retry once).
build_modhead() {
    rm -f o/modhead h/modhead
    run "$D/bin/CMunge,ff8" -p -tnorcroft -32bit \
        -d h.modhead -o o.modhead cmhg.modhead || true
    [ -s o/modhead ] && return 0
    echo "   (CMunge produced nothing - retrying)"
    run "$D/bin/CMunge,ff8" -p -tnorcroft -32bit \
        -d h.modhead -o o.modhead cmhg.modhead || true
    [ -s o/modhead ]
}

SRCS="module pdfobj pdfflt pdffile pdffont pdfimg pdfjpx pdfpage pdfdraw"

echo "== module header (CMunge) =="
build_modhead || { echo "CMunge FAILED"; exit 1; }

# --- vendored OpenJPEG (JPEG 2000 / JPXDecode) ---------------------
# Built at the same ARMv4 floor as our own code so one binary still
# runs StrongARM through ARMv8.  -W disables its (very noisy) warnings;
# these sources are upstream's and are not ours to clean up.
OJ_DIR="$HERE/thirdparty/openjpeg"
OJ_SRCS="bio cio dwt event function_list ht_dec image invert j2k jp2
         mct mqc openjpeg opj_malloc pi sparse_array t1 t2 tcd tgt thread"

echo "== OpenJPEG (vendored, JPEG 2000) =="
mkdir -p "$OJ_DIR/o"
for n in $OJ_SRCS; do
    if [ -s "$OJ_DIR/o/$n" ]; then continue; fi     # cached: rarely changes
    printf '   %s\n' "$n"
    run "$D/bin/cc,ff8" -c -zps1 -arch 4 -W -Ithirdparty.openjpeg \
        -o "thirdparty.openjpeg.o.$n" "thirdparty.openjpeg.c.$n"
    [ -s "$OJ_DIR/o/$n" ] || { echo "cc produced no object for $n"; exit 1; }
done

echo "== C sources (Norcroft cc) =="
for n in $SRCS; do
    printf '   %s\n' "$n"
    rm -f "o/$n"
    # -zM: module static relocation - REQUIRED to match the CMunge
    # veneers' _Lib$Reloc$Off protocol.  Without it the SWI/command
    # paths happen to work but entering the module as an application
    # (the viewer) corrupts the SCL client workspace and aborts.
    run "$D/bin/cc,ff8" -zM -c -arch 4 -Ithirdparty.openjpeg -o "o.$n" "c.$n"
    [ -s "o/$n" ] || { echo "cc produced no object for $n"; exit 1; }
done

echo "== link (relocatable module) =="
LIST="o.modhead"
for n in $SRCS; do LIST="$LIST o.$n"; done
for n in $OJ_SRCS; do LIST="$LIST thirdparty.openjpeg.o.$n"; done
rm -f rm/PDFRenderV
run "$D/bin/link,ff8" -rmf -o rm.PDFRenderV $LIST C:CLib.o.stubs
[ -s rm/PDFRenderV ] || { echo "link produced no module"; exit 1; }

echo "== viewer application (AIF) =="
mkdir -p aif
rm -f o/viewapp aif/Viewer
run "$D/bin/cc,ff8" -c -o o.viewapp c.viewapp
[ -s o/viewapp ] || { echo "cc produced no object for viewapp"; exit 1; }
run "$D/bin/link,ff8" -aif -o aif.Viewer o.viewapp C:CLib.o.stubs
[ -s aif/Viewer ] || { echo "viewer link produced nothing"; exit 1; }

# Keep the shipping application directory in step with the build.
if [ -d "$HERE/app/!PDFRenderV" ]; then
    cp "$HERE/rm/PDFRenderV" "$HERE/app/!PDFRenderV/PDFRenderV,ffa"
    cp "$HERE/aif/Viewer"    "$HERE/app/!PDFRenderV/Viewer,ff8"
    echo "   refreshed app/!PDFRenderV"
fi

echo
ls -l rm/PDFRenderV aif/Viewer
"$DDE_PY" "$D/src/romodule.py" rm/PDFRenderV 2>/dev/null | head -8 || true
echo "OK: rm/PDFRenderV"
