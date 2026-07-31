/*
  PDFRender - host test for the JBIG2 generic region decoder.

  Takes an embedded JBIG2 stream (as extracted from a PDF image
  XObject), its width and height, and writes a PBM of the result.  A
  correct decode of a scanned page's stencil is legible text; a wrong
  context, a wrong probability state or a wrong bit order gives noise,
  so this is worth looking at as well as measuring.

  Copyright (c) RISC OS Technologies 2026.
  SPDX-License-Identifier: CDDL-1.0
  Licensed under the Common Development and Distribution License 1.0; see LICENSE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdftypes.h"
#include "pdfjbig2.h"

int
main(int argc, char **argv)
{
    FILE *f;
    u8 *data;
    long len;
    int w, h, y;
    u8 *bits = NULL;
    u32 blen = 0, stride;
    long black = 0, i;
    int err;

    if (argc < 5) {
        fprintf(stderr, "usage: testjbig2 <stream> <w> <h> <out.pbm> "
                        "[globals]\n");
        return 2;
    }
    f = fopen(argv[1], "rb");
    if (f == NULL) {
        perror(argv[1]);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    data = malloc((size_t) len);
    if (fread(data, 1, (size_t) len, f) != (size_t) len) {
        return 1;
    }
    fclose(f);

    w = atoi(argv[2]);
    h = atoi(argv[3]);
    err = pdfjbig2_decode(NULL, 0, data, (u32) len, w, h, &bits, &blen);
    if (err != PDF_OK) {
        fprintf(stderr, "decode failed: %d\n", err);
        return 1;
    }
    stride = (u32) ((w + 7) / 8);
    for (i = 0; i < (long) blen; i++) {
        int b;

        for (b = 0; b < 8; b++) {
            if (((bits[i] >> b) & 1) == 0) {
                black++;                /* our output is PDF sense    */
            }
        }
    }
    printf("%d x %d, %lu bytes in, %u out, %ld black (%.1f%%)\n",
           w, h, (unsigned long) len, blen, black,
           100.0 * (double) black / ((double) stride * 8.0 * h));

    /* PBM is 1 = black, which is JBIG2's sense, so invert back      */
    f = fopen(argv[4], "wb");
    fprintf(f, "P4\n%d %d\n", w, h);
    for (y = 0; y < h; y++) {
        u32 x;

        for (x = 0; x < stride; x++) {
            u8 v = (u8) ~bits[(u32) y * stride + x];

            fwrite(&v, 1, 1, f);
        }
    }
    fclose(f);
    free(bits);
    free(data);
    return 0;
}
