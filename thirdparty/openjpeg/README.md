# OpenJPEG 2.5.2 (vendored, decoder only)

Upstream: https://github.com/uclouvain/openjpeg — **2-clause BSD**
(see `LICENSE`), which permits use in closed-source products provided
the copyright notice is retained. Do not relicense these files; they
stay BSD and are the only non-RISC OS Technologies code in the tree.

Provides `JPXDecode` (JPEG 2000) for scanned PDFs. Without it those
images render as blank rectangles.

## What was taken and why

`src/lib/openjp2/` in RISC OS layout (`c/foo`, `h/foo`), decoder path
only — 21 sources:

    bio cio dwt event function_list ht_dec image invert j2k jp2
    mct mqc openjpeg opj_malloc pi sparse_array t1 t2 tcd tgt thread

Deliberately excluded:

- `opj_clock` — a benchmark timer needing `sys/time.h` and
  `sys/resource.h`; verified unreferenced by every other source, so
  dropped rather than shimmed.
- `cidx_manager`, `phix_manager`, `ppix_manager`, `thix_manager`,
  `tpix_manager` — JPIP (network streaming) index writers; they do not
  even compile without `USE_JPIP` and are irrelevant to decoding.
- `bench_dwt`, `t1_generate_luts`, `t1_ht_generate_luts`,
  `test_sparse_array` — build-time tools, not library code.

## Local changes (keep this list exact — it is the upgrade recipe)

1. `h/opj_config` and `h/opj_config_private` are hand-written. CMake
   normally generates them; ours declare version 2.5.2 and the
   presence of `<stdint.h>`/`<inttypes.h>` and nothing else.
2. `h/memory` is a two-line shim including `<string.h>`. OpenJPEG
   includes `<memory.h>`, an SVR4-ism that the SharedCLibrary does not
   have; the `mem*` functions live in `<string.h>`.

No OpenJPEG `.c` file is modified. To upgrade: drop in the new
sources, re-add the two config headers and the shim, and rebuild.

## Notes for RISC OS

Built by Norcroft at `-arch 4` like the rest of the module, so one
binary still runs StrongARM through ARMv8. The irreversible (9/7)
wavelet path uses floating point and will go through FPEmulator on
machines without VFP — lossless (5/3) JPEG 2000, which is what
scanned PDFs normally use, is integer throughout and unaffected.
