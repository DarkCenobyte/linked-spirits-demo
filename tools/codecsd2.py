from codecsd import *
lines = vb.read_lyrics(); an = vb.analyse_lines(lines)
import sys
for kw in [dict(thr=3.0,M=16,qscale=1.0,rdb=1.5,uniform=True), dict(thr=3.0,M=16,qscale=2.0,rdb=1.5,uniform=True),
           dict(thr=3.0,M=16,qscale=1.0,rdb=1.5,uniform=True,lsfit=True), dict(thr=4.0,M=16,qscale=1.0,rdb=1.5,uniform=True,lsfit=True),
           dict(thr=4.0,M=16,qscale=2.0,rdb=1.5,uniform=True,lsfit=True)]:
    bk = vb.build_bank(lines, an, **kw)
    print(kw, 'SD %.2f size %d keys %d' % (sd_eval(bk), size(bk), sum(len(bk['bank'][k]['keys']) for k in bk['order'])), flush=True)
