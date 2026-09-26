#!/usr/bin/env python3
"""voice intelligibility test: build bank -> header -> render all lines with a
test melody -> Whisper WER (small.en + base.en)"""
import os, sys, struct, subprocess, re, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import voicebank as vb
sys.path.insert(0, '/tmp/claude-0/work')
OUT = '/tmp/claude-0/work/vt'
os.makedirs(OUT, exist_ok=True)
STY = dict(A=(1.0, 1.0, 0.1, 5.2, 0.2, 0, 0, 0, 1), B=(0.88, 1.0, 0.05, 4.6, 0.3, 0.0, 0, 0, 1), C=(0.94, 1.0, 0.08, 5.0, 0.25, 0, 0, 0, 1))
def test_melody(nsyl, seed, lo=57):
    rng = np.random.RandomState(seed); scale = [0, 2, 3, 5, 7, 8, 10, 12, 14]
    pos = 3; t = 0.4; notes = []
    for s in range(nsyl):
        d = 0.3 if s < nsyl - 1 else 1.0
        if rng.rand() < 0.2: d = 0.6
        notes.append((lo + scale[pos], t, d * 0.95, s)); t += d
        pos = int(np.clip(pos + rng.randint(-2, 3), 0, 8))
    return notes
def speech_notes(l, bk):
    """notes at the reference syllable timing (spoken rhythm, flat pitch)"""
    notes = []; t = 0.4; s = 0
    for w, p in l['words']:
        b = bk['bank'][w.lower() + '=' + ' '.join(p)]; bnd = b['bnd']
        for (sb, sv, se) in vb.syllabify(p):
            on = bnd[sv] - bnd[sb]; dur = bnd[se] - bnd[sb]
            notes.append((57 + 5, t + on, dur - on + 0.02, s)); t += dur; s += 1
        t += 0.03
    return notes
def write_score(fn, lines, idx, style_of=lambda l: STY['A' if l['speaker'] in 'A' else 'B' if l['speaker'] == 'B' else 'C'], melody=test_melody, bk=None):
    d = bytearray(); d += struct.pack('<i', len(lines))
    for li, l in enumerate(lines):
        ws = [idx[w.lower() + '=' + ' '.join(p)] for w, p in l['words']]
        nsyl = sum(len(vb.syllabify(p)) for w, p in l['words'])
        sp = l['speaker']; lo = 57 if sp != 'B' else 52
        notes = speech_notes(l, bk) if melody == 'speech' else melody(nsyl, li, lo)
        d += struct.pack('<i', len(ws)) + struct.pack('<%dh' % len(ws), *ws)
        d += struct.pack('<i', len(notes))
        for p, s, du, sy in notes: d += struct.pack('<fffi', p, s, du, sy)
        d += struct.pack('<9f', *style_of(l))
    open(fn, 'wb').write(d)
def norm(s): return re.sub(r"[^a-z' ]", " ", s.lower().replace('—', ' ')).split()
def wer(ref, hyp):
    r = norm(ref); h = norm(hyp); D = np.zeros((len(r) + 1, len(h) + 1), int); D[:, 0] = range(len(r) + 1); D[0, :] = range(len(h) + 1)
    for i in range(1, len(r) + 1):
        for j in range(1, len(h) + 1): D[i, j] = min(D[i - 1, j] + 1, D[i, j - 1] + 1, D[i - 1, j - 1] + (r[i - 1] != h[j - 1]))
    return D[-1, -1], len(r)
def run(thr=5.0, M=8, qs=1.5, rdb=1.5, models=('small.en', 'base.en'), verbose=False, lines_sel=None, thr_mid=None, edge=0.025, melody=test_melody):
    lines = vb.read_lyrics(); an = vb.analyse_lines(lines)
    bk = vb.build_bank(lines, an, thr=thr, M=M, qscale=qs, rdb=rdb, thr_mid=thr_mid, edge=edge)
    S = vb.pack(bk)
    idx = vb.emit_header(bk, S, os.path.join(os.path.dirname(vb.__file__), '..', 'src', 'gen_voice.h'))
    import lzma
    tot = b''.join(bytes(S[k]) for k in ('nph', 'nk', 'ph', 'dur', 'gap', 'rms', 'coef'))
    size = len(lzma.compress(tot, preset=9 | lzma.PRESET_EXTREME))
    subprocess.run(['gcc', '-O2', '-w', '-o', OUT + '/vtest', os.path.join(os.path.dirname(vb.__file__), 'vtest.c'), '-lm'], check=True)
    write_score(OUT + '/score.bin', lines, idx, melody=melody, bk=bk)
    subprocess.run([OUT + '/vtest', OUT + '/score.bin', OUT], check=True)
    import asr
    res = {}
    for m in models:
        r = asr.rec(m); E = N = 0
        for li, l in enumerate(lines):
            if lines_sel and li not in lines_sel: continue
            a, sr = asr.load(OUT + '/line%02d.wav' % li)
            h = asr.transcribe(r, a, sr); e, n = wer(l['text'], h); E += e; N += n
            if verbose and e: print('  %-8s %2d %-50s | %s' % (m, li, l['text'], h))
        res[m] = 100 * E / N
    print('thr %.1f/%s M %d qs %.2f rdb %.1f keys %d lzma %d  WER %s' % (thr, thr_mid, M, qs, rdb, sum(len(bk['bank'][k]['keys']) for k in bk['order']), size, ' '.join('%s %.1f%%' % kv for kv in res.items())), flush=True)
    return res
if __name__ == '__main__':
    run(verbose=True)
