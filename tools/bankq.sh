#!/bin/sh
# tools/bankq.sh thr M qs rdb  -> builds that voice bank, measures composite, restores the standard bank
cd "$(dirname "$0")/.."
cp src/gen_voice.h /tmp/claude-0/gv_save.h; cp src/gen_score.h /tmp/claude-0/gs_save.h
python3 - "$@" <<'PY' 2>&1 | grep 'voice bank'
import sys; sys.path.insert(0,'tools')
import build_data
a=sys.argv[1:]
build_data.VB_PARAMS=dict(thr=float(a[0]),M=int(a[1]),qscale=float(a[2]),rdb=float(a[3]),uniform=True,lsfit=True)
build_data.main()
PY
tools/vq.sh "" 2>&1 | grep "=>\|FAIL" | sed "s/^ */bank $* /"
cp /tmp/claude-0/gv_save.h src/gen_voice.h; cp /tmp/claude-0/gs_save.h src/gen_score.h
