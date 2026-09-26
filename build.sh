#!/bin/sh
# LINKED//SPIRITS build script.
#   ./build.sh          -> build/linked_spirits.exe (+ UPX-packed build/linked_spirits_upx.exe)
#   ./build.sh preview  -> build/preview (Linux/Mesa development harness)
#   ./build.sh data     -> regenerate src/gen_*.h from tools/ (needs the voice models, see README)
set -e
cd "$(dirname "$0")"
mkdir -p build
if [ "$1" = "data" ]; then python3 tools/build_data.py; shift; fi
if [ "$1" = "preview" ]; then
  python3 tools/embed_shaders.py
  gcc -O2 -g -march=native -w -o build/preview src/main_lin.c -lEGL -lm -lpthread
  exit 0
fi
python3 tools/embed_shaders.py -m
CFLAGS="-Oz -s -march=x86-64-v2 -mfpmath=sse -ffast-math -fno-math-errno -fno-asynchronous-unwind-tables \
 -fno-stack-protector -fno-ident -ffunction-sections -fno-builtin-memset -fno-builtin-memcpy -fno-tree-loop-distribute-patterns \
 -fomit-frame-pointer -fno-unwind-tables -fmerge-all-constants"
x86_64-w64-mingw32-gcc $CFLAGS -include src/mathx.h -nostdlib -nostartfiles -Wl,-e,entry -Wl,--gc-sections \
  -Wl,--subsystem,windows -Wl,--strip-all -Wl,--file-alignment,512 -Wl,--section-alignment,4096 \
  -o build/linked_spirits.exe src/main_win.c -lkernel32 -luser32 -lgdi32 -lopengl32 -lwinmm -lgcc
ls -l build/linked_spirits.exe
if [ "$1" = "debug" ]; then
  x86_64-w64-mingw32-gcc $CFLAGS -DDEBUG -include src/mathx.h -nostdlib -nostartfiles -Wl,-e,entry -Wl,--gc-sections \
    -Wl,--subsystem,windows -o build/linked_spirits_debug.exe src/main_win.c -lkernel32 -luser32 -lgdi32 -lopengl32 -lwinmm -lgcc
  exit 0
fi
rm -f build/linked_spirits_upx.exe
upx --best --ultra-brute --lzma -q -o build/linked_spirits_upx.exe build/linked_spirits.exe >/dev/null
ls -l build/linked_spirits_upx.exe
