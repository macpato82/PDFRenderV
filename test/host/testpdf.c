/*
  PDFRender - host-side end-to-end parser test.

  Opens each PDF given on the command line (or a default set from
  /Volumes/Nvme/PDFs), reports document structure, then runs page 1
  (and up to the first 3 pages) through the interpreter with a
  backend that counts and optionally prints operations.  Under ASan
  this shakes out memory errors across the whole core.

  Usage: testpdf [-v] file.pdf ...
     -v prints every backend call (op dump)

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

static int verbose;

typedef struct {
    int nfill, nstroke, ntext, nimage, nclip, nsave;
    long textbytes;
} Counts;

static double
fxd(fix v)
{
    return (double) v / 65536.0;
}

static void
d_gsave(void *ctx)
{
    ((Counts *) ctx)->nsave++;
    if (verbose) {
        printf("  gsave\n");
    }
}

static void
d_grestore(void *ctx)
{
    if (verbose) {
        printf("  grestore\n");
    }
    (void) ctx;
}

static void
d_clip(void *ctx, fix x0, fix y0, fix x1, fix y1)
{
    ((Counts *) ctx)->nclip++;
    if (verbose) {
        printf("  clip [%.1f %.1f %.1f %.1f]\n",
               fxd(x0), fxd(y0), fxd(x1), fxd(y1));
    }
}

static void
d_fill(void *ctx, const PdfPath *p, u32 rgb, int evenodd, int blend,
       fix alpha)
{
    ((Counts *) ctx)->nfill++;
    if (verbose) {
        printf("  fill %u els rgb=%06x eo=%d a=%.2f%s\n", p->n, rgb, evenodd,
               fxd(alpha), blend == PDF_BLEND_MULTIPLY ? " MULTIPLY" : "");
    }
}

static void
d_stroke(void *ctx, const PdfPath *p, u32 rgb, fix w,
         int cap, int join, const fix *dash, int ndash, fix phase)
{
    ((Counts *) ctx)->nstroke++;
    if (verbose) {
        printf("  stroke %u els rgb=%06x w=%.2f cap=%d join=%d nd=%d\n",
               p->n, rgb, fxd(w), cap, join, ndash);
    }
    (void) dash;
    (void) phase;
}

static void
d_text(void *ctx, const char *rofont, const u8 *s, u32 len,
       const fix m[6], u32 rgb)
{
    Counts *c = ctx;

    c->ntext++;
    c->textbytes += len;
    if (verbose) {
        u32 i;
        printf("  text %-24s @(%.1f,%.1f) sz=%.1f rgb=%06x \"",
               rofont, fxd(m[4]), fxd(m[5]), fxd(m[3]), rgb);
        for (i = 0; i < len && i < 60; i++) {
            putchar(s[i] >= 32 && s[i] < 127 ? s[i] : '.');
        }
        printf("\"\n");
    }
}

static s32
d_textwidth(void *ctx, const char *rofont, const u8 *s, u32 len)
{
    (void) ctx;
    (void) rofont;
    (void) s;
    return (s32) len * 500;             /* crude but deterministic */
}

static void
d_image(void *ctx, const PdfImg *img, const fix m[6])
{
    ((Counts *) ctx)->nimage++;
    if (verbose) {
        printf("  image %dx%d kind=%s @(%.1f,%.1f) scale(%.1f,%.1f)\n",
               img->w, img->h,
               img->kind == PDFIMG_JPEG ? "jpeg" : "xrgb32",
               fxd(m[4]), fxd(m[5]), fxd(m[0]), fxd(m[3]));
    }
}

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

static const char *const default_files[] = {
    "/Volumes/Nvme/PDFs/viewfinder.pdf",
    "/Volumes/Nvme/PDFs/StrongArm Overclock.pdf",
    "/Volumes/Nvme/PDFs/RiscOSDirectLogo.pdf",
    "/Volumes/Nvme/PDFs/Acorn Risc PC Technical Reference Manual-opt.pdf",
    NULL
};

static int
run_one(const char *path)
{
    u8 *buf;
    u32 len;
    PdfDoc *doc = NULL;
    int err;
    u32 p, np;

    printf("=== %s\n", path);
    buf = slurp(path, &len);
    if (buf == NULL) {
        printf("  cannot read file\n");
        return 1;
    }
    err = pdf_open(&doc, buf, len);
    if (err != PDF_OK) {
        printf("  pdf_open err %d%s\n", err,
               err == PDF_E_ENCRYPTED ? " (encrypted)" : "");
        free(buf);
        return err == PDF_E_ENCRYPTED ? 0 : 1;
    }
    printf("  PDF %d.%d, %u pages, %u xref slots\n",
           doc->vermajor, doc->verminor, doc->npages, doc->nobjs);

    np = doc->npages < 3 ? doc->npages : 3;
    for (p = 0; p < np; p++) {
        fix w, h;
        int rot;
        Counts c;
        PdfBackend be;

        memset(&c, 0, sizeof c);
        memset(&be, 0, sizeof be);
        be.ctx = &c;
        be.gsave = d_gsave;
        be.grestore = d_grestore;
        be.clip = d_clip;
        be.fill = d_fill;
        be.stroke = d_stroke;
        be.text = d_text;
        be.textwidth = d_textwidth;
        be.image = d_image;

        pdf_pagegeom(doc, p, &w, &h, &rot);
        printf("  page %u: %.0f x %.0f pt rot %d\n",
               p + 1, fxd(w), fxd(h), rot);
        err = pdf_runpage(doc, p, &be, FX_ONE);
        printf("    run err=%d: %d fill, %d stroke, %d text (%ld bytes),"
               " %d image, %d clip, %d save\n",
               err, c.nfill, c.nstroke, c.ntext, c.textbytes,
               c.nimage, c.nclip, c.nsave);
        if (err != PDF_OK) {
            pdf_close(doc);
            free(buf);
            return 1;
        }
    }
    pdf_close(doc);
    free(buf);
    return 0;
}

int
main(int argc, char **argv)
{
    int i, bad = 0, first = 1;

    if (argc > 1 && strcmp(argv[1], "-v") == 0) {
        verbose = 1;
        first = 2;
    }
    if (argc > first) {
        for (i = first; i < argc; i++) {
            bad += run_one(argv[i]);
        }
    } else {
        for (i = 0; default_files[i]; i++) {
            bad += run_one(default_files[i]);
        }
    }
    printf(bad ? "*** %d file(s) FAILED\n" : "all documents parsed\n", bad);
    return bad != 0;
}
