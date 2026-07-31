#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pdftypes.h"
#include "pdfobj.h"
#include "pdfflt.h"
#include "pdffile.h"
#include "pdfpage.h"
int main(int argc, char **argv)
{
    FILE *f; long n; u8 *buf; PdfDoc *d; u32 p;
    if (argc < 2) return 1;
    f = fopen(argv[1], "rb"); if (!f) { perror("open"); return 1; }
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    buf = malloc(n); fread(buf, 1, n, f); fclose(f);
    if (pdf_open(&d, buf, (u32) n) != PDF_OK) { printf("open failed\n"); return 1; }
    printf("%s: %u pages\n", argv[1], d->npages);
    for (p = 0; p < d->npages; p++) {
        PdfLink *lk = NULL; char *uris = NULL; u32 cnt = 0, ul = 0, i;
        if (pdf_extractlinks(d, p, FX_ONE, &lk, &cnt, &uris, &ul) != PDF_OK) {
            printf("  page %u: extract failed\n", p + 1); continue;
        }
        if (cnt) printf("  page %u: %u link(s)\n", p + 1, cnt);
        for (i = 0; i < cnt; i++)
            printf("    [%4d,%4d %4d,%4d] %s\n",
                   lk[i].x0 >> 16, lk[i].y0 >> 16, lk[i].x1 >> 16, lk[i].y1 >> 16,
                   uris + lk[i].uoff);
        free(lk); free(uris);
    }
    pdf_close(d); free(buf); return 0;
}
