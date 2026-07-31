#!/bin/bash
# Prove no C source in PDFRenderV compiles above ARMv4 (cc -S listing
# scan, per the PB209 lesson: never byte-scan a linked image).
set -e
cd /Volumes/Nvme/PDFRender
D=/Volumes/Nvme/NorcroftUniversal
. "$D/dde-python.sh"; dde_find_python >/dev/null
EXP=$D/Export/APCS-32
mkdir -p s scrap
BAD='MOVW|MOVT|UBFX|SBFX|BFI|BFC|MLS|UDIV|SDIV|CLZ|BLX|LDRD|STRD|PLD|REV|REV16|RBIT|SEL|PKHBT|PKHTB|SSAT|USAT|UXTB|UXTH|SXTB|SXTH|UXTB16|SMULBB|SMLABB|QADD|QSUB'
fail=0; n=0
for f in $(ls c) $(ls thirdparty/openjpeg/c | sed 's|^|OJ:|'); do
  rm -f s/audit
  case "$f" in
    OJ:*) src="thirdparty.openjpeg.c.${f#OJ:}";;
    *)    src="c.$f";;
  esac
  "$DDE_PY" "$D/src/romachine.py" -m "$D/modules/FPEmulator,ffa" \
     -m "$D/modules/CLib623,ffa" -d . \
     -p "C=$EXP/C,$EXP/Lib/CLib,$EXP/Lib" -V 'C$LibRoot=C:' \
     -V 'Wimp$ScrapDir=$.scrap' \
     "$D/bin/cc,ff8" -S -arch 4 -W -Ithirdparty.openjpeg -o s.audit "$src" \
     > scrap/audit.log 2>&1 || true
  [ -s s/audit ] || { echo "   $f: NO LISTING"; fail=1; continue; }
  n=$((n+1))
  hits=$(grep -aoE "^[[:space:]]+($BAD)[A-Z]{0,2}[[:space:]]" s/audit | sort -u | tr -d ' \t' | tr '\n' ' ')
  [ -n "$hits" ] && { echo "   FAIL $f: $hits"; fail=1; }
done
rm -f s/audit
[ $fail = 0 ] && echo "== $n sources: nothing above ARMv4 ==" || { echo "== ARMv4 violations =="; exit 1; }
