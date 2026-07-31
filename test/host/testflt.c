/*
  PDFRender - host-side filter tests.

  Reads test vectors produced by mkvectors.py: each vector is a pair
  of files NNN.raw (expected plaintext) and NNN.z (zlib-compressed by
  Python's zlib at assorted levels/strategies).  Also self-tests the
  predictor, ASCII85, ASCIIHex, RLE and LZW paths with synthetic data.

  Copyright (c) RISC OS Technologies 2026.
  SPDX-License-Identifier: CDDL-1.0
  Licensed under the Common Development and Distribution License 1.0; see LICENSE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdftypes.h"
#include "pdfflt.h"

static int failures;

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

static void
check(int ok, const char *what)
{
	if (!ok) {
		printf("FAIL: %s\n", what);
		failures++;
	}
}

static void
test_inflate_vectors(const char *dir)
{
	int i;
	int seen = 0;

	for (i = 0; i < 1000; i++) {
		char pz[512], praw[512], label[560];
		u8 *z, *raw;
		u32 zlen, rawlen;
		PdfBuf out = { 0 };
		int err;

		snprintf(pz, sizeof pz, "%s/%03d.z", dir, i);
		snprintf(praw, sizeof praw, "%s/%03d.raw", dir, i);
		z = slurp(pz, &zlen);
		if (!z) {
			break;
		}
		raw = slurp(praw, &rawlen);
		seen++;
		err = pdf_inflate(z, zlen, &out);
		snprintf(label, sizeof label, "inflate %03d err=%d", i, err);
		check(err == PDF_OK, label);
		snprintf(label, sizeof label,
		         "inflate %03d len %u want %u", i, out.len, rawlen);
		check(out.len == rawlen, label);
		if (out.len == rawlen && rawlen) {
			snprintf(label, sizeof label, "inflate %03d content", i);
			check(memcmp(out.p, raw, rawlen) == 0, label);
		}
		pdfbuf_free(&out);
		free(z);
		free(raw);
	}
	printf("inflate: %d vectors\n", seen);
	check(seen >= 20, "expected at least 20 inflate vectors");
}

static void
test_truncated(const char *dir)
{
	/* Truncating a valid stream must fail or stop cleanly - never
	   crash, never loop.  (Run under ASan for the full benefit.)   */
	char pz[512];
	u8 *z;
	u32 zlen, cut;

	snprintf(pz, sizeof pz, "%s/000.z", dir);
	z = slurp(pz, &zlen);
	if (!z) {
		check(0, "missing vector 000.z");
		return;
	}
	for (cut = 0; cut < zlen && cut < 200; cut++) {
		PdfBuf out = { 0 };
		pdf_inflate(z, cut, &out);      /* any result, no crash */
		pdfbuf_free(&out);
	}
	free(z);
	printf("inflate: truncation sweep ok\n");
}

static void
test_predict(void)
{
	/* PNG Up filter: rows [1 2 3 4], deltas after row 1 are zero,
	   so every row must reconstruct identically.                   */
	u8 d[] = { 0, 1, 2, 3, 4,   2, 0, 0, 0, 0,   2, 0, 0, 0, 0 };
	u32 len = sizeof d;
	int err = pdf_predict(d, &len, 12, 1, 8, 4);

	check(err == PDF_OK, "predict err");
	check(len == 12, "predict len");
	check(d[0] == 1 && d[3] == 4, "predict row0");
	check(d[4] == 1 && d[7] == 4, "predict row1 (Up)");
	check(d[8] == 1 && d[11] == 4, "predict row2 (Up)");

	{
		/* Sub filter with 3-byte pixels */
		u8 s[] = { 1, 10, 20, 30, 5, 5, 5 };
		u32 sl = sizeof s;
		err = pdf_predict(s, &sl, 11, 3, 8, 2);
		check(err == PDF_OK && sl == 6, "predict sub err/len");
		check(s[0] == 10 && s[1] == 20 && s[2] == 30, "sub px0");
		check(s[3] == 15 && s[4] == 25 && s[5] == 35, "sub px1");
	}
}

static void
test_ascii(void)
{
	PdfBuf out = { 0 };
	/* "Man " in ASCII85 is 9jqo^ ; canonical example from the spec  */
	int err = pdf_ascii85((const u8 *) "9jqo^~>", 7, &out);

	check(err == PDF_OK && out.len == 4 &&
	      memcmp(out.p, "Man ", 4) == 0, "ascii85 Man ");
	out.len = 0;
	err = pdf_ascii85((const u8 *) "z~>", 3, &out);
	check(err == PDF_OK && out.len == 4 &&
	      out.p[0] == 0 && out.p[3] == 0, "ascii85 z");
	out.len = 0;
	err = pdf_asciihex((const u8 *) "48 65 6C 6c 6F >", 16, &out);
	check(err == PDF_OK && out.len == 5 &&
	      memcmp(out.p, "Hello", 5) == 0, "asciihex");
	out.len = 0;
	err = pdf_asciihex((const u8 *) "4>", 2, &out);
	check(err == PDF_OK && out.len == 1 && out.p[0] == 0x40,
	      "asciihex odd pad");
	out.len = 0;
	{
		const u8 rle[] = { 2, 'a', 'b', 'c', 255, 'x', 128 };
		err = pdf_rle(rle, sizeof rle, &out);
		check(err == PDF_OK && out.len == 5 &&
		      memcmp(out.p, "abcxx", 5) == 0, "rle");
	}
	pdfbuf_free(&out);
}

static void
test_lzw(void)
{
	/* The PDF spec's worked example (7.4.4.2): the byte sequence
	   45 45 45 45 45 65 45 45 45 66 encodes as the 9-bit codes
	   256 45 258 258 65 259 66 257, packed MSB-first:
	   100000000 001000101 100000010 100000010 001100101
	   100000011 001100110 100000001                                 */
	const u8 enc[] = { 0x80, 0x11, 0x60, 0x50, 0x23, 0x2c,
	                   0x0c, 0xcd, 0x01 };
	const u8 want[] = { 0x45, 0x45, 0x45, 0x45, 0x45, 0x65,
	                    0x45, 0x45, 0x45, 0x66 };
	PdfBuf out = { 0 };
	int err = pdf_lzw(enc, sizeof enc, 1, &out);

	check(err == PDF_OK, "lzw err");
	check(out.len == sizeof want, "lzw len");
	if (out.len == sizeof want) {
		check(memcmp(out.p, want, sizeof want) == 0, "lzw content");
	}
	pdfbuf_free(&out);
}

int
main(int argc, char **argv)
{
	const char *dir = argc > 1 ? argv[1] : "vectors";

	test_inflate_vectors(dir);
	test_truncated(dir);
	test_predict();
	test_ascii();
	test_lzw();
	if (failures) {
		printf("*** %d FAILURES\n", failures);
		return 1;
	}
	printf("all filter tests passed\n");
	return 0;
}
