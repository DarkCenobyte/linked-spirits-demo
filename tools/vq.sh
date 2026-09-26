#!/bin/sh
# voice quality composite: tools/vq.sh "-DFOO=1 ..."   (voices-only render, three Whisper conditions)
cd "$(dirname "$0")/.."
gcc -O2 -march=native -w $1 -o build/preview_q src/main_lin.c -lEGL -lm -lpthread || exit 1
rm -f build/vox_q.wav; ./build/preview_q audio build/vox_q.wav ${2:-vox} >/dev/null 2>&1 || { echo "RENDER FAILED"; exit 1; }
python3 - "$1" <<'PY'
import sys, io, contextlib
sys.path.insert(0,'tools')
import mixtest
r=[]
for m,g in (('small.en',1),('small.en',2),('base.en',2)):
    buf=io.StringIO()
    with contextlib.redirect_stdout(buf): mixtest.run('build/vox_q.wav',m,g,verbose=False)
    r.append(float(buf.getvalue().split('WER')[1].strip().rstrip('%')))
print('%-40s s1 %5.1f  s2 %5.1f  b2 %5.1f  => %5.1f'%(sys.argv[1],r[0],r[1],r[2],sum(r)/3))
PY
