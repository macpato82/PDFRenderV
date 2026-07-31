/*
  PDFRender - BMP and GIF decoders against reference PPMs written by
  Python PIL.  Any difference is printed as a pixel count, because a
  decoder that is subtly wrong usually still produces a plausible
  picture.

  Copyright (c) RISC OS Technologies 2026.
  SPDX-License-Identifier: CDDL-1.0
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pdftypes.h"
#include "pdfbmp.h"
#include "pdfgif.h"

static u8 *slurp(const char *p, u32 *len)
{
    FILE *f = fopen(p, "rb"); long n; u8 *b;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    b = malloc(n); if (fread(b, 1, n, f) != (size_t) n) { fclose(f); return NULL; }
    fclose(f); *len = (u32) n; return b;
}

/* binary PPM (P6), maxval 255 */
static u8 *readppm(const char *p, int *w, int *h)
{
    FILE *f = fopen(p, "rb"); int mv; u8 *d;
    if (!f) return NULL;
    if (fscanf(f, "P6 %d %d %d", w, h, &mv) != 3) { fclose(f); return NULL; }
    fgetc(f);
    d = malloc((size_t) *w * *h * 3);
    if (fread(d, 1, (size_t) *w * *h * 3, f) != (size_t) *w * *h * 3) { fclose(f); return NULL; }
    fclose(f); return d;
}

int main(int argc, char **argv)
{
    int fails = 0, i;
    for (i = 1; i + 1 < argc; i += 2) {
        const char *img = argv[i], *ref = argv[i + 1];
        u32 len = 0; u8 *src = slurp(img, &len);
        int w = 0, h = 0, rw = 0, rh = 0, bad = 0, x, y;
        u32 *pix = NULL; u8 *rgb;
        int isgif = strstr(img, ".gif") != NULL;
        int err;
        if (!src) { printf("FAIL %s: unreadable\n", img); fails++; continue; }
        err = isgif ? pdfgif_decode(src, len, &w, &h, &pix)
                    : pdfbmp_decode(src, len, &w, &h, &pix);
        if (err != PDF_OK) { printf("FAIL %s: decode err %d\n", img, err); fails++; free(src); continue; }
        rgb = readppm(ref, &rw, &rh);
        if (!rgb) { printf("FAIL %s: no reference\n", img); fails++; free(src); free(pix); continue; }
        if (w != rw || h != rh) {
            printf("FAIL %s: %dx%d but reference is %dx%d\n", img, w, h, rw, rh);
            fails++;
        } else {
            for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
                u32 p = pix[y * w + x];
                u8 *r = rgb + (y * w + x) * 3;
                if ((p & 0xFF) != r[0] || ((p >> 8) & 0xFF) != r[1] ||
                    ((p >> 16) & 0xFF) != r[2]) bad++;
            }
            if (bad) { printf("FAIL %s: %d of %d pixels differ\n", img, bad, w * h); fails++; }
            else printf("ok   %s (%dx%d exact)\n", img, w, h);
        }
        free(src); free(pix); free(rgb);
    }
    printf(fails ? "%d FAILURES\n" : "all image decoders match PIL\n", fails);
    return fails != 0;
}
