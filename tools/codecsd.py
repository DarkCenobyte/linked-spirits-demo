#!/usr/bin/env python3
"""objective voice-codec evaluation: energy-weighted log-spectral distortion of the
decoded (runtime-equivalent) envelopes vs the original analysis frames"""
import sys, numpy as np, lzma
import voicebank as vb
def decode_word(b):
    n = b['n']; x = np.arange(n); keys = b['keys']; Lq = b['Lq']
    L = np.stack([np.interp(x, keys, Lq[:, j]) for j in range(16)], 1)
    return L
def sd_eval(bk, maxw=None):
    tot = 0; wsum = 0
    for k in bk['order'][:maxw]:
        b = bk['bank'][k]; L = decode_word(b); G = b['G']
        act = G > G.max() * 0.03
        for t in np.where(act)[0]:
            e0 = vb.env(vb.lsf2a(b['L'][t])); e1 = vb.env(vb.lsf2a(L[t]))
            w = G[t]; tot += w * np.sqrt(np.mean((e0 - e1) ** 2)); wsum += w
    return tot / wsum
def size(bk):
    S = vb.pack(bk); return len(lzma.compress(b''.join(bytes(v) for v in S.values()), preset=9 | lzma.PRESET_EXTREME))
if __name__ == '__main__':
    lines = vb.read_lyrics(); an = vb.analyse_lines(lines)
    for thr, M, qs, rdb in [(1.5, 16, 0.3, 1.5), (3.0, 16, 0.3, 1.5), (1.5, 12, 2.0, 3.0), (3.0, 12, 2.0, 3.0)]:
        bk = vb.build_bank(lines, an, thr=thr, M=M, qscale=qs, rdb=rdb)
        print('thr %.1f M %2d qs %.1f: SD %.2f dB  size %d' % (thr, M, qs, sd_eval(bk), size(bk)), flush=True)
