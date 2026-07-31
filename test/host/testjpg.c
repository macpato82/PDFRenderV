/*
  PDFRender - JPEG decoding against reference PPMs written by Python
  PIL, baseline and progressive.  A progressive decoder that gets a
  refinement scan slightly wrong still produces a recognisable
  picture, so the difference is measured rather than eyeballed: the
  spectral-selection and successive-approximation passes have to
  reconstruct the same coefficients libjpeg does, and any error in
  them shows up as a bounded but non-zero pixel difference.

  Copyright (c) RISC OS Technologies 2026.
  SPDX-License-Identifier: CDDL-1.0
  Licensed under the Common Development and Distribution License 1.0; see LICENSE.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pdftypes.h"
#include "pdfjpg.h"

static u8 *
slurp(const char *p, u32 *len)
{
    FILE *f = fopen(p, "rb");
    long n;
    u8 *b;

    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = malloc((size_t) n);
    if (fread(b, 1, (size_t) n, f) != (size_t) n) {
        fclose(f);
        return NULL;
    }
    fclose(f);
    *len = (u32) n;
    return b;
}

/* Binary PPM, as PIL writes it. */
static u8 *
read_ppm(const char *path, int *w, int *h)
{
    FILE *f = fopen(path, "rb");
    char magic[3];
    int maxv, c;
    u8 *px;

    if (!f) {
        return NULL;
    }
    if (fscanf(f, "%2s", magic) != 1 || strcmp(magic, "P6") != 0) {
        fclose(f);
        return NULL;
    }
    if (fscanf(f, "%d %d %d", w, h, &maxv) != 3) {
        fclose(f);
        return NULL;
    }
    c = fgetc(f);
    (void) c;
    px = malloc((size_t) *w * (size_t) *h * 3);
    if (fread(px, 1, (size_t) *w * (size_t) *h * 3, f) !=
        (size_t) *w * (size_t) *h * 3) {
        free(px);
        fclose(f);
        return NULL;
    }
    fclose(f);
    return px;
}

static int fails;

static void
check(const char *jpg, const char *ppm, int tol)
{
    u32 len = 0;
    u8 *data = slurp(jpg, &len);
    int rw = 0, rh = 0, w = 0, h = 0, err, i, n;
    u8 *ref = read_ppm(ppm, &rw, &rh);
    u32 *pix = NULL;
    long worst = 0, sum = 0, over = 0;

    if (data == NULL || ref == NULL) {
        printf("  %-24s MISSING INPUT\n", jpg);
        fails++;
        return;
    }
    err = pdfjpg_decode(data, len, 0, 0, &w, &h, &pix);
    if (err != PDF_OK) {
        printf("  %-24s decode failed: %d\n", jpg, err);
        fails++;
        free(data);
        free(ref);
        return;
    }
    if (w != rw || h != rh) {
        printf("  %-24s size %dx%d, expected %dx%d\n", jpg, w, h, rw, rh);
        fails++;
    } else {
        n = w * h;
        for (i = 0; i < n; i++) {
            int k;

            for (k = 0; k < 3; k++) {
                long d = (long) ((pix[i] >> (8 * k)) & 0xFF) -
                         (long) ref[i * 3 + k];

                if (d < 0) {
                    d = -d;
                }
                sum += d;
                if (d > worst) {
                    worst = d;
                }
                if (d > tol) {
                    over++;
                }
            }
        }
        printf("  %-24s %dx%d  mean %.2f  worst %ld  over tol(%d): %ld/%d\n",
               jpg, w, h, (double) sum / (n * 3), worst, tol, over, n * 3);
        if (over > 0) {
            fails++;
        }
    }
    free(pix);
    free(data);
    free(ref);
}

int
main(int argc, char **argv)
{
    int i;

    printf("JPEG decode vs PIL:\n");
    for (i = 1; i + 1 < argc; i += 2) {
        check(argv[i], argv[i + 1], 24);
    }
    printf(fails ? "FAILED (%d)\n" : "all within tolerance\n", fails);
    return fails ? 1 : 0;
}
