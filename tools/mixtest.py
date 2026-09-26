#!/usr/bin/env python3
"""Whisper test of every sung line inside the rendered soundtrack."""
import sys, os, re, numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE); sys.path.insert(0, '/tmp/claude-0/work')
import score as S, voicebank as vb, asr
from vtest import wer
def segments():
    lines = vb.read_lyrics(); out = []
    for l, v in zip(lines, S.VOX):
        n = v[2] + (v[3] or [])
        t0 = (v[1] * 16 + min(x[1] for x in n)) * 0.15 - 0.35
        t1 = (v[1] * 16 + max(x[1] + x[2] for x in n)) * 0.15 + 0.4
        out.append((l['text'], t0, t1, v[0]))
    return out
def run(fn, model='small.en', group=1, verbose=True):
    a, sr = asr.load(fn); r = asr.rec(model); segs = segments(); E = N = 0
    for i in range(0, len(segs), group):
        g = segs[i:i + group]; t0 = g[0][1]; t1 = g[-1][2]
        txt = ' '.join(x[0] for x in g)
        h = asr.transcribe(r, a[int(t0 * sr):int(t1 * sr)], sr)
        e, n = wer(txt, h); E += e; N += n
        if verbose and e: print('  %2d %-3s %-55s | %s' % (i, g[0][3], txt[:55], h))
    print('%s %s group %d: WER %.1f%%' % (os.path.basename(fn), model, group, 100 * E / N))
if __name__ == '__main__':
    run(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else 'small.en', int(sys.argv[3]) if len(sys.argv) > 3 else 1)
