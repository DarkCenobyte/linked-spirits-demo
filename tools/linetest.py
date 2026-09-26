#!/usr/bin/env python3
"""Whisper test of selected sung lines: tools/linetest.py song.wav FIRST [LAST] [model]"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mixtest import segments, asr, wer
fn, a0 = sys.argv[1], int(sys.argv[2]); a1 = int(sys.argv[3]) if len(sys.argv) > 3 else a0
model = sys.argv[4] if len(sys.argv) > 4 else 'small.en'
a, sr = asr.load(fn); r = asr.rec(model); s = segments()[a0:a1 + 1]
txt = ' '.join(x[0] for x in s); h = asr.transcribe(r, a[int(s[0][1] * sr):int(s[-1][2] * sr)], sr)
e, n = wer(txt, h); print('%s [%d-%d] WER %d/%d | %s' % (os.path.basename(fn), a0, a1, e, n, h))
