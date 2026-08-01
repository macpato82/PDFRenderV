/*
  PDFRender - does the decoded-mask cache actually get used?

  Runs one page twice and reports what each pass cost.  A JBIG2
  stencil is decoded pixel by pixel through an arithmetic coder and
  cannot be scaled down, so on a scanned page it dominates; the point
  of the cache is that only the first pass pays for it.  Measuring
  both passes is the only way to know the cache is being hit rather
  than quietly missing on a key that never matches.

  Copyright (c) RISC OS Technologies 2026.
  SPDX-License-Identifier: CDDL-1.0
  Licensed under the Common Development and Distribution License 1.0; see LICENSE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pdftypes.h"
#include "pdffile.h"
#include "pdfpage.h"
#include "pdfimg.h"

typedef struct {
    int nimage;
    int nalpha;
    unsigned long alphasum;
} Counts;

static void
d_image(void *ctx, const PdfImg *img, const fix m[6])
{
    Counts *c = ctx;

    c->nimage++;
    if (img->alpha != NULL) {
        long i, n = (long) img->w * (long) img->h;
        unsigned long s = 0;

        c->nalpha++;
        /* a checksum of the coverage, so the second pass can be shown
           to produce the same mask and not merely to be faster       */
        for (i = 0; i < n; i++) {
            s = s * 31u + img->alpha[i];
        }
        c->alphasum ^= s;
    }
}

static void s_gsave(void *ctx) { (void) ctx; }
static void s_grestore(void *ctx) { (void) ctx; }
static void s_clip(void *ctx, fix a, fix b, fix c, fix d)
{ (void) ctx; (void) a; (void) b; (void) c; (void) d; }
static void s_fill(void *ctx, const PdfPath *p, u32 rgb, int eo,
                   int blend, fix alpha)
{ (void) ctx; (void) p; (void) rgb; (void) eo; (void) blend; (void) alpha; }
static void s_stroke(void *ctx, const PdfPath *p, u32 rgb, fix w, int cap,
                     int join, const fix *dash, int nd, fix phase)
{ (void) ctx; (void) p; (void) rgb; (void) w; (void) cap; (void) join;
  (void) dash; (void) nd; (void) phase; }
static void s_text(void *ctx, const char *ro, const u8 *t, u32 n,
                   const fix m[6], u32 rgb)
{ (void) ctx; (void) ro; (void) t; (void) n; (void) m; (void) rgb; }
static s32 s_textwidth(void *ctx, const char *ro, const u8 *t, u32 n)
{ (void) ctx; (void) ro; (void) t; (void) n; return 0; }
static void s_glyph(void *ctx, u32 ch, fix x0, fix y0, fix x1, fix y1)
{ (void) ctx; (void) ch; (void) x0; (void) y0; (void) x1; (void) y1; }

static double
now(void)
{
    return (double) clock() / (double) CLOCKS_PER_SEC;
}

int
main(int argc, char **argv)
{
    FILE *f;
    u8 *buf;
    long len;
    PdfDoc *doc;
    u32 page;
    int pass;
    unsigned long sums[2];

    if (argc < 3) {
        fprintf(stderr, "usage: testmask <pdf> <page, 1-based>\n");
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
    buf = malloc((size_t) len);
    if (fread(buf, 1, (size_t) len, f) != (size_t) len) {
        return 1;
    }
    fclose(f);
    page = (u32) (atoi(argv[2]) - 1);

    if (pdf_open(&doc, buf, (u32) len) != PDF_OK) {
        fprintf(stderr, "cannot open %s\n", argv[1]);
        return 1;
    }

    for (pass = 0; pass < 2; pass++) {
        Counts c;
        PdfBackend be;
        double t0, t1;
        u32 mh = 0, mm = 0;

        memset(&c, 0, sizeof c);
        memset(&be, 0, sizeof be);
        be.ctx = &c;
        be.gsave = s_gsave;
        be.grestore = s_grestore;
        be.clip = s_clip;
        be.fill = s_fill;
        be.stroke = s_stroke;
        be.text = s_text;
        be.textwidth = s_textwidth;
        be.glyph = s_glyph;
        be.image = d_image;

        t0 = now();
        pdf_runpage(doc, page, &be, FX_ONE);
        t1 = now();
        pdfimg_mask_stats(&mh, &mm);
        sums[pass] = c.alphasum;
        printf("pass %d: %.2fs  %d images, %d masked   mask cache: "
               "%u hits, %u misses\n",
               pass + 1, t1 - t0, c.nimage, c.nalpha, mh, mm);
    }
    printf(sums[0] == sums[1]
           ? "same coverage both passes\n"
           : "COVERAGE DIFFERS BETWEEN PASSES\n");
    pdf_close(doc);
    free(buf);
    return sums[0] == sums[1] ? 0 : 1;
}
