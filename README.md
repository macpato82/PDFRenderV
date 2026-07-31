# PDFRenderV

A fast, native document previewer for RISC OS 5 - a softloadable
relocatable module (the engine + SWIs + *Commands) and a small
absolute application (`aif.Viewer`) providing the desktop window
with its toolbar.  It opens **PDF, JPEG, PNG, Sprite and Draw**
files in the same window with the same zoom, selection and printing,
so one viewer covers everything. One binary runs on everything from
ARMv5 (Iyonix/XScale) through ARMv7 (iMX6/ARMX6, Titanium, Pi) to
ARMv8 AArch32: the core is integer-only 16.16 fixed point (no FPA, no
VFP), and the Norcroft build emits no ARMv6+ encodings.

Copyright (c) RISC OS Technologies 2026.
SPDX-License-Identifier: CDDL-1.0

## Requirements

**SpriteExtend 1.88 recommended.**  Every JPEG - both standalone
`&C85` files and `DCTDecode` images inside a PDF - is plotted by
SpriteExtend rather than decoded here, so its version sets the JPEG
behaviour you get.  1.88 is what PDFRenderV is developed and tested
against: libjpeg-turbo 3.1.3 in place of IJG 8d, one binary from
ARMv5TE to ARMv8, and CMYK/YCCK JPEGs carried through the migration
with the RGB approximation upstream does not provide.

It is not a hard floor - 1.86 and earlier decode CMYK/YCCK as well,
and PDFRenderV asks `JPEG_Info` before plotting, falling back to its
own `c/pdfjpg` decoder if the OS refuses a file, so an older or
stricter SpriteExtend still draws the page.  That fallback is slower
and is not the tested path.

RISC OS 5 otherwise - developed and tested on 5.31, and nothing here
needs a call newer than RISC OS 3.5, though earlier RO5 releases have
not been tried.  No other modules are needed.

## Design

```
        c/pdfflt    filters: in-house inflate (RFC 1950/1951), LZW,
                    PNG/TIFF predictors, ASCII85/Hex, RunLength
        c/pdfobj    lexer, object model (16.16 fixed reals), arena pool
        c/pdffile   xref (classic + streams + hybrid), object streams,
 core   |           lazy object cache, page tree with inheritance,
 (host- |           filter-chain stream decoding
 test-  c/pdffont   widths, encodings (WinAnsi/MacRoman), ToUnicode,
 able)  |           standard-14 -> Homerton/Trinity/Corpus mapping
        c/pdfimg    image XObjects + inline images -> XRGB32 or raw
        |           JPEG passthrough
        c/pdfpage   content-stream interpreter -> backend vtable
        ------------------------------------------------------------
        c/pdfdraw   RISC OS backend: sprite redirection (SpriteOp 60),
 module |           Draw module paths, FontManager AA text,
 only   |           SpriteOp 52 images, SpriteExtend JPEG_PlotScaled
        c/module    SWI dispatch, *Commands, document handles
```

Rendering is OS-assisted: paths go through the Draw module, text
through FontManager (anti-aliased, using Acorn's metric clones of the
PDF standard fonts), JPEGs through SpriteExtend - which means VideoX
acceleration applies where installed. Everything is plotted into a
caller-supplied 32bpp 180dpi sprite via VDU redirection, so 1 pixel =
1 OS unit and the device space matches PDF's bottom-left-origin
convention with no y-flips.

## SWIs (chunk base is a PLACEHOLDER - needs ROOL allocation)

| SWI | Entry | Exit |
|---|---|---|
| `PDFRenderV_Open` | R1 filename | R0 handle, R1 pages |
| `PDFRenderV_Close` | R0 handle | - |
| `PDFRenderV_Info` | R0 handle | R0 pages, R1 version |
| `PDFRenderV_PageSize` | R0 handle, R1 page (1-based) | R0/R1 w/h millipoints, R2 rotation |
| `PDFRenderV_Render` | R0 flags (b0 white bg), R1 handle, R2 page, R3 sprite area, R4 sprite ptr | - |

`PDFRenderV_Render` flags: bit 0 = white background; bit 1 = direct
mode (no sprite: render to the current VDU output at R3 = zoom in
16.16 device-units-per-point - used inside PDriver print jobs).
Otherwise the page is scaled to fit the given sprite, which should be
32bpp (type 6, 180dpi mode word &302D0169).

## Installing

`app/!Viewer` is the shipping application directory - named for what it
does rather than for the module inside it, which is still PDFRenderV,
as are its system variables and `*Commands`. Put it
anywhere the boot sequence scans (`Boot:^.Apps`, or Configure > Boot >
Look at) and the module loads itself at boot: the app's `!Boot` sets
`PDFRenderV$Viewer`, `RMEnsure`s the module, and names the filetype.
Opening the parent directory once in a session has the same effect.
`build.sh` refreshes the copies inside it.

Nothing needs typing at the command line - that is the difference
between "the module is loaded" and "PDFs just open".

## The viewer

Double-clicking any supported file opens it: window + toolbar
`[<] [n/N] [>]  [-] [zoom%] [+]  [Print]`, scrollable page, zoom
25-400%, vector printing through PDriver.  On a PDF the Menu button
over the page also offers Copy / Select all / Save text, with
click-drag text selection.

The toolbar is embedded in the window's horizontal scroll bar, the
way Ovation and NetSurf do it: the pane is a nested-Wimp *furniture
window* (window flag bit 23), which is what lets a child encroach on
its parent's border instead of being clipped to the work area, and the
Wimp then shortens the scroll bar to fit beside it. Its height is
therefore whatever the scroll bar is in the current mode and theme, so
the icons are sized when the window opens and again on
Message_ModeChange rather than being fixed in the definition. This
needs the nested Wimp, so the task initialises as Wimp 380.

Running `!Viewer` with no document puts it on the **left** of the icon
bar, next to the ROM `Apps` icon (`Wimp_CreateIcon` window handle -5
with priority &4F000000; the PRM gives Apps &50000000 and the RAM disc
&40000000). Documents dropped there open in a window. Only the no-file
copy creates a bar icon - a document window is a separate copy of the
binary, so N open documents do not give N identical icons.

| Type | | Rendered by |
|---|---|---|
| PDF | &ADF | this engine |
| JPEG | &C85 | SpriteExtend (`JPEG_PlotScaled`) |
| PNG | &B60 | `c/pdfpng`, reusing our inflate and PNG predictors |
| Sprite | &FF9 | `OS_SpriteOp 52` |
| Draw | &AFF | `DrawFile_Render` |

`PDFRenderV$Claim` lists which filetypes the module takes over
(default: all five).  An application whose `!Boot` runs after ours can
take one back - ChangeFSI claims JPEG, for instance - so the module
re-claims on Service_StartWimp and `*PDFClaim` re-asserts them at any
time (the app's `!Boot` calls it).  Drop `FF9` to keep Paint on sprites, or `AFF`
to keep Draw on drawings; every claimed type is handed back when the
module is killed, and only if the alias still points at our viewer.

The module claims `Alias$@RunType_ADF` at init (only when
`PDFRenderV$Viewer` is set) and releases it at finalisation, so
killing the module cleanly hands the filetype back.

The viewer is deliberately NOT inside the module: CMunge's
`module-is-runnable:` entry (`_clib_entermodule`) dies inside
Wimp_Initialise on the Kinetic Box RO5.31 softload - a 20-line
clean-room module reproduces it - so the window half is a plain
application, which is solid everywhere. All document work still
happens in the module through the SWIs above.

## *Commands

```
*PDFInfo <pdffile>
*PDFToSprite <pdf> <page> <spritefile> [<width px>]
```

`*PDFToSprite` renders a page to a new 32bpp sprite file (default
width 1000px, filetype &FF9) - the quickest way to test.

## Building

```
./build.sh                 # Norcroft DDE via NorcroftUniversal (~2s)
cd test/host && make run   # host tests: clang + ASan, zlib oracle,
                           # real-PDF parse of /Volumes/Nvme/PDFs
```

## Supported / not yet

In: PDF 1.0-1.7 (classic xref, xref streams, object streams), Flate
(+predictors), LZW, ASCII85/Hex, RLE; paths, fills (both winding
rules), strokes with dashes/caps/joins, rect clip (bbox approximation
for complex clips); device gray/RGB/CMYK, ICCBased by N, Indexed;
text with widths, Tz/Tc/Tw/TL/rise, Type0 via ToUnicode, rotated text
via Font_Paint matrices; image XObjects (gray/RGB/CMYK/indexed,
1/2/4/8/16 bpc), DCT passthrough to SpriteExtend, inline images; form
XObjects; page /Rotate.

JPEG 2000 (`JPXDecode`) via the vendored OpenJPEG in
`thirdparty/openjpeg` - needed by scanned PDFs, which are otherwise
blank pages - decoded at a resolution matched to the display size,
since JPX is resolution-scalable and a full-size decode of a scan is
many times more work than the screen can show.

Decoded images are cached (LRU, 4MB by default, keyed by stream
offset and decode size), so revisiting a page costs nothing.

A note on JPEG: every JPEG is left to SpriteExtend even though
`c/pdfjpg` can decode them at 1/2, 1/4 or 1/8 scale.  Measured on an
emulated StrongARM with a 2550x3300 scan, SpriteExtend took ~19s and
our scaled decode 26-33s: skipping the IDCT does not help when the
Huffman pass dominates, and the OS decoder does that in hand-written
ARM.  The numbers, not the theory, decided it.  `c/pdfjpg` stays as
the fallback described under Requirements - the backend tries
`JPEG_Info` first and only decodes in-house if the OS refuses the
file, so an unsupported variant draws something rather than leaving a
hole in the page. Constant alpha and Multiply are composited for real
(the backend owns its sprite and reads it back) on axis-aligned
rectangles.

Encrypted PDFs are decrypted transparently when the user password is
empty - which is what almost every "encrypted" PDF in the wild is,
restricting permissions rather than demanding a password. All four
standard-handler flavours are covered: RC4 40-bit (/V 1 /R 2), RC4
128-bit (/V 2 /R 3), AES-128 (/V 4 /R 4, /CFM /AESV2) and AES-256 (/V 5
/R 6, /AESV3, including the Algorithm 2.B hardened hash). MD5, SHA-256/
384/512, RC4 and AES are written from their specifications in
`c/pdfcrypt` - no GPL or OpenSSL source. A PDF that genuinely needs a
password is refused with a clear error; supplying one is not
implemented.

Out (v1): user-supplied passwords, embedded font programs (mapped to
metric-equivalent RISC OS fonts instead), /Differences encodings,
Type3 glyph rendering, shadings/patterns (skipped or grey),
transparency groups and luminosity soft masks - so a highlight
annotation drawn as an opaque fill inside a form XObject still covers
what is under it - CCITT/JBIG2 images.

## Testing

- `test/host/testflt` - inflate vs Python zlib oracle (85 vectors,
  including streams lifted from real PDFs), truncation sweep, LZW
  spec vector, predictor/ASCII85/Hex/RLE unit tests. ASan/UBSan.
- `test/host/testpdf` - opens real PDFs, dumps/counts backend ops.
- `test/host/testcrypt` - MD5/SHA-256/384/512 against Python hashlib
  (18 lengths around every block boundary, plus streaming in awkward
  chunk sizes), RC4 and AES-128/192/256 against the FIPS-197 vectors.
  End to end, the four qpdf-produced encryption levels above extract
  text byte-identical to the unencrypted original.
- On target: `*PDFToSprite` then view the sprite (or pull it back to
  the Mac for SpriteViewer).
