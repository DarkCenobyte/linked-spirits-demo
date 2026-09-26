from codecsd import *
lines = vb.read_lyrics(); an = vb.analyse_lines(lines)
for kw in [dict(thr=3.0,M=16,qscale=4.5,pw=0.0), dict(thr=3.0,M=12,qscale=4.0,pw=0.0), dict(thr=3.0,M=16,qscale=4.5,pw=1.0), dict(thr=3.0,M=12,qscale=4.0,pw=1.0), dict(thr=3.0,M=12,qscale=3.5,pw=1.5)]:
    bk = vb.build_bank(lines, an, rdb=1.5, uniform=True, lsfit=True, **kw)
    print(kw, 'SD %.2f size %d' % (sd_eval(bk), size(bk)), flush=True)
