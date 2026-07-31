/*
  PDFRender - host-side text-extraction test.

  Dumps the glyphs of a page in document order, grouped into visual
  lines by baseline, so the extracted text can be eyeballed against
  the real document and the box geometry sanity-checked.

  Usage: testtext <file.pdf> [<page>]

  Copyright (c) RISC OS Technologies 2026.
  SPDX-License-Identifier: CDDL-1.0
  Licensed under the Common Development and Distribution License 1.0; see LICENSE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdftypes.h"
#include "pdfobj.h"
#include "pdfflt.h"
#include "pdffile.h"
#include "pdffont.h"
#include "pdfimg.h"
#include "pdfpage.h"

static u8 *
slurp(const char *path, u32 *lenp)
{
    FILE *f = fopen(path, "rb");
    u8 *p;
    long n;

    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    p = malloc(n ? (size_t) n : 1);
    if (fread(p, 1, (size_t) n, f) != (size_t) n) {
        free(p);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *lenp = (u32) n;
    return p;
}

int
main(int argc, char **argv)
{
    u8 *buf;
    u32 len, n, i;
    PdfDoc *doc = NULL;
    PdfGlyph *gl = NULL;
    int err, page = argc > 2 ? atoi(argv[2]) : 1;
    s32 lastbase = 0x7fffffff, lastx = 0;
    int bad = 0;

    if (argc < 2) {
        printf("usage: testtext <file.pdf> [page]\n");
        return 1;
    }
    buf = slurp(argv[1], &len);
    if (buf == NULL) {
        printf("cannot read %s\n", argv[1]);
        return 1;
    }
    err = pdf_open(&doc, buf, len);
    if (err != PDF_OK) {
        printf("pdf_open err %d\n", err);
        return 1;
    }
    err = pdf_extracttext(doc, (u32) page - 1, FX_ONE, &gl, &n);
    if (err != PDF_OK) {
        printf("extract err %d\n", err);
        return 1;
    }
    printf("=== %s page %d: %u glyphs\n", argv[1], page, n);
    for (i = 0; i < n; i++) {
        s32 base = gl[i].y0;
        /* new visual line when the baseline moves, or the pen jumps
           backwards on the same line                                */
        if (lastbase == 0x7fffffff ||
            base < lastbase - 2 * 65536 || base > lastbase + 2 * 65536 ||
            gl[i].x0 < lastx - 40 * 65536) {
            printf("\n[%6.1f,%6.1f] ", (double) gl[i].x0 / 65536.0,
                   (double) base / 65536.0);
            lastbase = base;
        }
        putchar(gl[i].ch >= 32 && gl[i].ch < 127 ? (char) gl[i].ch :
                gl[i].ch == 0 ? '?' : '.');
        lastx = gl[i].x0;
        if (gl[i].x1 < gl[i].x0 || gl[i].y1 < gl[i].y0) {
            bad++;
        }
    }
    printf("\n\n%u glyphs, %d with inverted boxes\n", n, bad);
    free(gl);
    pdf_close(doc);
    free(buf);
    return bad != 0;
}
