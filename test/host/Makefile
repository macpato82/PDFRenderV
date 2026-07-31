# PDFRender - host-side test build (macOS clang, ASan).
#
# The project sources use RISC OS naming: extensionless files under
# c/ and h/, included as "foo.h" (Norcroft rewrites that to h.foo).
# On the host we bridge with hostinc/, a directory of foo.h symlinks
# pointing at ../../h/foo, and compile the c/ files with -x c.
#
# Copyright (c) RISC OS Technologies 2026.
# SPDX-License-Identifier: CDDL-1.0
# Licensed under the Common Development and Distribution License 1.0; see LICENSE.

TOP     := ../..
CC      := cc
OJDIR   := $(TOP)/thirdparty/openjpeg
OJSRCS  := bio cio dwt event function_list ht_dec image invert j2k jp2 \
           mct mqc openjpeg opj_malloc pi sparse_array t1 t2 tcd tgt thread
OJOBJ   := $(addprefix obj/oj_,$(addsuffix .o,$(OJSRCS)))
CFLAGS  := -std=c99 -O1 -g -Wall -Wextra -Wno-unused-parameter \
           -fsanitize=address,undefined -Ihostinc -Iojinc
CORE    := pdfflt pdfobj pdffile pdffont pdfimg pdfjpx pdfjpg pdfpng pdfpage
COREOBJ := $(addprefix obj/,$(addsuffix .o,$(CORE)))

all: headers testflt testpdf testtext

headers:
	@mkdir -p hostinc obj ojinc
	@for f in $(TOP)/h/*; do \
	    ln -sf ../$$f hostinc/$$(basename $$f).h; \
	done
	@for f in $(OJDIR)/h/*; do \
	    ln -sf ../$$f ojinc/$$(basename $$f).h; \
	done

obj/oj_%.o: $(OJDIR)/c/% headers
	$(CC) -std=c99 -O1 -g -w -Iojinc -fsanitize=address,undefined -x c -c $< -o $@

obj/%.o: $(TOP)/c/% headers
	$(CC) $(CFLAGS) -x c -c $< -o $@

testflt: testflt.c obj/pdfflt.o
	$(CC) $(CFLAGS) testflt.c obj/pdfflt.o -o $@

testpdf: testpdf.c $(COREOBJ) $(OJOBJ)
	$(CC) $(CFLAGS) testpdf.c $(COREOBJ) $(OJOBJ) -o $@

testtext: testtext.c $(COREOBJ) $(OJOBJ)
	$(CC) $(CFLAGS) testtext.c $(COREOBJ) $(OJOBJ) -o $@

vectors: mkvectors.py
	python3 mkvectors.py

run: all vectors
	./testflt
	./testpdf

clean:
	rm -rf obj hostinc ojinc testflt testpdf testtext vectors

.PHONY: all headers run clean vectors
