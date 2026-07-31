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
    FILE *f; long n; u8 *buf; PdfDoc *d;
    PdfOutlineEnt *ol = NULL; char *t = NULL; u32 cnt = 0, tl = 0, i;
    if (argc < 2) return 1;
    f = fopen(argv[1], "rb"); if (!f) return 1;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    buf = malloc(n); if (fread(buf, 1, n, f) != (size_t) n) return 1;
    fclose(f);
    if (pdf_open(&d, buf, (u32) n) != PDF_OK) { printf("open failed\n"); return 1; }
    if (pdf_extractoutline(d, &ol, &cnt, &t, &tl) != PDF_OK) { printf("extract failed\n"); return 1; }
    printf("%s: %u pages, %u outline entries\n", argv[1], d->npages, cnt);
    for (i = 0; i < cnt && i < 40; i++)
        printf("  %*s[p%-3u] %s\n", (int) ol[i].level * 2, "", ol[i].page, t + ol[i].toff);
    free(ol); free(t); pdf_close(d); free(buf); return 0;
}
