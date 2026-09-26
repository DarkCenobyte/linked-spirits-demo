#!/bin/sh
# LINKED//SPIRITS - the smallest packed variants (the Windows packers run under Wine).
#   tools/pack.sh   -> build/linked_spirits_squishy.exe    64-bit, packed by squishy (logicoma)
#                      build/linked_spirits32_kkrunchy.exe 32-bit, packed by kkrunchy_k7 (farbrausch)
# Needs: ./build.sh first (the 64-bit exe), wine with 32-bit support, i686-w64-mingw32-gcc,
#        SQUISHY=path/to/squishy-x64.exe, KKRUNCHY=path/to/kkrunchy_k7.exe (see tools/kkrunchy_gcc.py)
set -e
cd "$(dirname "$0")/.."
export WINEDEBUG=-all
python3 tools/embed_shaders.py -m
# 32-bit: x87 floating point and plain i386 code are the densest input for kkrunchy
i686-w64-mingw32-gcc -Oz -s -march=i386 -mfpmath=387 -ffast-math -fno-math-errno -fno-asynchronous-unwind-tables \
  -fno-stack-protector -fno-ident -fno-builtin-memset -fno-builtin-memcpy -fno-tree-loop-distribute-patterns \
  -fomit-frame-pointer -fno-unwind-tables -fmerge-all-constants -include src/mathx.h -nostdlib -nostartfiles \
  -Wl,-e,_entry -Wl,--gc-sections -Wl,--subsystem,windows -Wl,--strip-all -Wl,--file-alignment,512 \
  -Wl,--section-alignment,4096 -Wl,--disable-reloc-section -Wl,--disable-dynamicbase -Wl,--large-address-aware \
  -o build/linked_spirits32.exe src/main_win.c -lkernel32 -luser32 -lgdi32 -lopengl32 -lwinmm -lgcc
cd build
rm -f linked_spirits_squishy.exe linked_spirits32_kkrunchy.exe
wine "${SQUISHY:?set SQUISHY to squishy-x64.exe}" -i linked_spirits.exe -o linked_spirits_squishy.exe -p silent
wine "${KKRUNCHY:?set KKRUNCHY to kkrunchy_k7.exe}" --best --out linked_spirits32_kkrunchy.exe linked_spirits32.exe >/dev/null
ls -l linked_spirits.exe linked_spirits_squishy.exe linked_spirits32.exe linked_spirits32_kkrunchy.exe
