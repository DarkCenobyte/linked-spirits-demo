from codecsd import *
lines = vb.read_lyrics(); an = vb.analyse_lines(lines)
import sys, itertools
for thr, qs in itertools.product([3.0, 3.5], [3.0, 4.5, 6.0]):
    kw = dict(thr=thr, M=16, qscale=qs, rdb=1.5, uniform=True, lsfit=True)
    bk = vb.build_bank(lines, an, **kw)
    print('thr %.1f qs %.1f  SD %.2f size %d' % (thr, qs, sd_eval(bk), size(bk)), flush=True)
