#!/usr/bin/env python3
"""
LINKED//SPIRITS - voice bank builder (build-time only).

Pipeline (see README "Voices"):
  1. lyrics_ph.txt gives every sung line with a hand-checked phoneme transcription.
  2. Each line is spoken by a neural TTS (Kokoro, via sherpa-onnx) -> natural,
     clearly articulated female speech used purely as a *spectral* reference.
  3. The same phonemes are rendered with MBROLA (known phoneme boundaries) and
     DTW-aligned against the TTS audio -> phoneme boundaries in the TTS audio.
  4. 16th-order LPC analysis (16 kHz), converted to line spectral frequencies.
  5. Each unique word is cut out once; only variable-rate keyframes are kept
     (where linear LSF interpolation would exceed a spectral-distortion bound).
  6. LSF keyframes are PCA-decorrelated and quantised; energy is kept at 20 ms.
The demo then *sings* these words: pitch, glottal source, timing, vibrato,
formant scaling and all voice colour are generated at runtime.
"""
import os, sys, re, struct, subprocess, hashlib, pickle, wave
import numpy as np
from scipy.signal import resample_poly
from scipy.fft import rfft, dct

SR = 16000
ORDER = 16
HOP = 80          # 5 ms analysis hop
WIN = 400
CACHE = os.environ.get('VOICE_CACHE', '/tmp/claude-0/work/vcache')
KOKORO = os.environ.get('KOKORO_DIR', '/tmp/claude-0/asr/kokoro-en-v0_19')
HERE = os.path.dirname(os.path.abspath(__file__))
os.makedirs(CACHE, exist_ok=True)

VOW = set('@ @U A AI E EI I O OI U V aU i u { r='.split())
DIPH = set('AI EI OI aU @U'.split())
VOWID = '@ A E I O U V i u { r= AI EI OI aU @U'.split()
# phoneme classes used by the runtime excitation model
#  0 vowel  1 voiced sonorant  2 voiced plosive  3 unvoiced plosive
#  4 unvoiced fricative  5 voiced fricative  6 affricate(unv)  7 affricate(v)
CLS = {}
for p in VOW: CLS[p] = 0
for p in 'm n N l r w j'.split(): CLS[p] = 1
for p in 'b d g 4'.split(): CLS[p] = 2
for p in 'p t k p_h t_h k_h'.split(): CLS[p] = 3
for p in 's S f T h'.split(): CLS[p] = 4
for p in 'v D z Z'.split(): CLS[p] = 5
CLS['tS'] = 6; CLS['dZ'] = 7
REFDUR = {0: 150, 1: 70, 2: 65, 3: 90, 4: 110, 5: 80, 6: 120, 7: 100}


def read_lyrics(fn=os.path.join(HERE, 'lyrics_ph.txt')):
    lines = []
    for l in open(fn):
        if l.startswith('#') or '|' not in l: continue
        sp, text, ws = l.rstrip('\n').split('|')
        words = []
        for w in ws.split(' / '):
            word, ph = w.split('=', 1)
            words.append((word, ph.split()))
        lines.append(dict(speaker=sp, text=text, words=words))
    return lines


def syllabify(ph):
    """same rule as the runtime: vowels are nuclei; between two nuclei a single
    consonant goes to the onset, a cluster gives its first consonant to the coda."""
    vi = [i for i, p in enumerate(ph) if CLS[p] == 0]
    b = [0]
    for a, c in zip(vi, vi[1:]):
        nc = c - a - 1
        b.append(a + 1 if nc <= 1 else a + 2)
    res = []
    for k in range(len(vi)):
        e = b[k + 1] if k + 1 < len(vi) else len(ph)
        res.append((b[k], vi[k], e))
    return res


# ---------------------------------------------------------------- references
def _wav(fn):
    w = wave.open(fn)
    return np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float64) / 32768


def mbrola_ref(line, f0=180):
    """render the line's phonemes with MBROLA (female us1) -> audio + boundaries (s)"""
    ph = []
    toks = re.findall(r"[A-Za-z']+|[,.?!]", line['text'])
    wi = 0
    for t in toks:
        if t in ',.?!':
            ph.append(('_', 120)); continue
        for p in line['words'][wi][1]:
            d = REFDUR[CLS[p]] + (40 if p in DIPH else 0)
            if p == 'h': d = 70
            ph.append((p, d))
        wi += 1
    s = '_ 60\n' + ''.join('%s %d 0 %d 100 %d\n' % (p, d, f0, f0) if p != '_' else '_ %d\n' % d for p, d in ph) + '_ 60\n'
    open(CACHE + '/in.pho', 'w').write(s)
    subprocess.run(['mbrola', '/usr/share/mbrola/us1/us1', CACHE + '/in.pho', CACHE + '/mb.wav'], check=True, capture_output=True)
    a = _wav(CACHE + '/mb.wav')
    t = 0.06; seg = []
    for p, d in ph:
        seg.append((p, t, t + d / 1000)); t += d / 1000
    return a, seg


_TTS = None
def kokoro_ref(text, sid=1, speed=0.9):
    key = hashlib.md5(('%s|%d|%.2f' % (text, sid, speed)).encode()).hexdigest()
    fn = CACHE + '/k_' + key + '.npy'
    if os.path.exists(fn): return np.load(fn)
    global _TTS
    if _TTS is None:
        import sherpa_onnx
        c = sherpa_onnx.OfflineTtsConfig(model=sherpa_onnx.OfflineTtsModelConfig(
            kokoro=sherpa_onnx.OfflineTtsKokoroModelConfig(model=KOKORO + '/model.onnx', voices=KOKORO + '/voices.bin',
                                                          tokens=KOKORO + '/tokens.txt', data_dir=KOKORO + '/espeak-ng-data'),
            num_threads=4))
        _TTS = sherpa_onnx.OfflineTts(c)
    a = _TTS.generate(text, sid=sid, speed=speed)
    x = resample_poly(np.array(a.samples, dtype=np.float64), 2, 3)   # 24k -> 16k
    x = np.concatenate([np.zeros(800), x, np.zeros(800)])
    np.save(fn, x)
    return x


# ---------------------------------------------------------------- alignment
def melspec(a, hop=160, win=400, nm=40):
    x = np.append(a[0], a[1:] - 0.95 * a[:-1])
    w = np.hanning(win); n = (len(x) - win) // hop + 1
    fr = np.stack([x[i * hop:i * hop + win] * w for i in range(n)])
    S = np.abs(rfft(fr, 512, axis=1)) ** 2
    f = np.linspace(0, SR / 2, 257); mel = lambda f: 2595 * np.log10(1 + f / 700)
    m = np.linspace(mel(80), mel(7600), nm + 2); fm = 700 * (10 ** (m / 2595) - 1)
    fb = np.zeros((nm, 257))
    for i in range(nm):
        l, c, r = fm[i], fm[i + 1], fm[i + 2]
        fb[i] = np.clip(np.minimum((f - l) / (c - l), (r - f) / (r - c)), 0, None)
    return np.log(S @ fb.T + 1e-8)


def feats(a):
    M = melspec(a)
    C = dct(M, type=2, norm='ortho', axis=1)[:, 1:16]
    C = (C - C.mean(0)) / (C.std(0) + 1e-6)
    d = np.gradient(C, axis=0)
    e = M.max(1, keepdims=True); e = (e - e.mean()) / e.std()
    return np.hstack([C, 0.5 * d, e])


def dtw(A, B):
    n, m = len(A), len(B)
    D = ((A[:, None, :] - B[None, :, :]) ** 2).sum(-1)
    C = np.full((n + 1, m + 1), np.inf); C[0, 0] = 0
    for i in range(1, n + 1):
        ci = C[i]; cp = C[i - 1]; di = D[i - 1]
        row = di + np.minimum(cp[1:], cp[:-1])
        for j in range(m):
            h = ci[j] + di[j]
            ci[j + 1] = row[j] if row[j] < h else h
    i, j = n, m; path = [(i - 1, j - 1)]
    while i > 1 or j > 1:
        opts = []
        if i > 1 and j > 1: opts.append((C[i - 1, j - 1], i - 1, j - 1))
        if i > 1: opts.append((C[i - 1, j], i - 1, j))
        if j > 1: opts.append((C[i, j - 1], i, j - 1))
        _, i, j = min(opts); path.append((i - 1, j - 1))
    return np.array(path[::-1])


def map_time(P, t, hop=0.01):
    fa = t / hop
    idx = np.where(P[:, 0] == int(round(fa)))[0]
    if len(idx) == 0: idx = [np.argmin(abs(P[:, 0] - fa))]
    return P[idx, 1].mean() * hop


# ---------------------------------------------------------------- LPC / LSF
def lpc(frame, order):
    r = np.correlate(frame, frame, 'full')[len(frame) - 1:len(frame) + order]
    if r[0] < 1e-10: return np.zeros(order)
    lag = np.exp(-0.5 * (2 * np.pi * 60 / SR * np.arange(order + 1)) ** 2); r = r * lag; r[0] *= 1.0001
    a = np.zeros(order + 1); a[0] = 1; e = r[0]; k = np.zeros(order)
    for i in range(1, order + 1):
        acc = r[i] + np.dot(a[1:i], r[i - 1:0:-1])
        ki = -acc / e; k[i - 1] = ki
        a[1:i + 1] = a[1:i + 1] + ki * np.concatenate([a[i - 1:0:-1], [1]])
        e *= (1 - ki * ki)
    return k


def k2a(k):
    a = np.array([1.0])
    for ki in k:
        a = np.concatenate([a, [0]]); a = a + ki * a[::-1]
    return a


def a2lsf(a):
    p = len(a) - 1
    ae = np.concatenate([a, [0]])
    P = ae + ae[::-1]; Q = ae - ae[::-1]
    P1 = np.polydiv(P, [1, 1])[0]; Q1 = np.polydiv(Q, [1, -1])[0]
    w = np.concatenate([np.angle(np.roots(P1)), np.angle(np.roots(Q1))])
    w = np.sort(w[w > 0])
    assert len(w) == p
    return w


def lsf2a(w):
    p = len(w)
    P = np.array([1.0, 1.0]); Q = np.array([1.0, -1.0])
    for i in range(0, p, 2): P = np.convolve(P, [1, -2 * np.cos(w[i]), 1])
    for i in range(1, p, 2): Q = np.convolve(Q, [1, -2 * np.cos(w[i]), 1])
    return ((P + Q) / 2)[:p + 1]


ENVMODE = os.environ.get('ENVMODE', 'mel')
_m = np.linspace(2595 * np.log10(1 + 100 / 700), 2595 * np.log10(1 + 7800 / 700), 48)
W64 = (700 * (10 ** (_m / 2595) - 1)) / SR * 2 * np.pi if ENVMODE == 'mel' else np.linspace(0.01, np.pi - 0.01, 64)
Z64 = None
def env(a):
    global Z64
    if Z64 is None or Z64.shape[1] != len(a): Z64 = np.exp(-1j * np.outer(W64, np.arange(len(a))))
    e = -20 * np.log10(np.abs(Z64 @ a) + 1e-9)
    if ENVMODE == 'mel': e = np.maximum(e, e.max() - 35)   # valleys below -35 dB are irrelevant
    return e


def analyze(a):
    """returns per 5ms frame: LSF (rad), rms of pre-emphasised signal * sqrt(pred. error)"""
    x = np.append(a[0], a[1:] - 0.9 * a[:-1])
    w = np.hanning(WIN)
    L = []; G = []
    for c in range(0, len(x) - WIN, HOP):
        fr = x[c:c + WIN] * w
        k = lpc(fr, ORDER)
        E = np.prod(1 - k * k)
        seg = x[c + WIN // 2 - HOP:c + WIN // 2 + HOP]
        rms = np.sqrt(np.mean(seg ** 2) + 1e-12)
        L.append(a2lsf(k2a(k))); G.append(rms * np.sqrt(E))
    return np.array(L), np.array(G)


def frame_of(t):
    return (t * SR - WIN / 2) / HOP


# ---------------------------------------------------------------- analysis of all lines
def analyse_lines(lines, sid=1):
    out = []
    for li, line in enumerate(lines):
        key = hashlib.md5(('v3|' + line['text'] + '|' + repr(line['words']) + '|%d' % sid).encode()).hexdigest()
        fn = CACHE + '/a_' + key + '.pkl'
        if os.path.exists(fn):
            out.append(pickle.load(open(fn, 'rb'))); continue
        mb, seg = mbrola_ref(line)
        k = kokoro_ref(line['text'], sid)
        P = dtw(feats(mb), feats(k))
        bt = [s for p, s, e in seg] + [seg[-1][2]]
        kt = [map_time(P, t) for t in bt]
        ksegs = [(seg[i][0], kt[i], kt[i + 1]) for i in range(len(seg))]
        L, G = analyze(k)
        # word spans
        words = []; si = 0
        for word, ph in line['words']:
            while ksegs[si][0] == '_': si += 1
            s0 = si; si += len(ph)
            bnd = [ksegs[j][1] for j in range(s0, si)] + [ksegs[si - 1][2]]
            words.append(dict(word=word.lower(), ph=ph, bnd=bnd))
        r = dict(L=L, G=G, words=words, audio=k)
        pickle.dump(r, open(fn, 'wb'))
        out.append(r)
        print('analysed', li, line['text'], file=sys.stderr)
    return out


def vfr_keys(L, E, thr, maxgap=48, force=()):
    """greedy variable-frame-rate keyframes; thr may be a per-frame array.
    frames in `force` are always keyframes."""
    n = len(L); keys = [0]; i = 0
    thr = np.broadcast_to(np.asarray(thr, dtype=float), (n,))
    force = sorted(set(f for f in force if 0 < f < n - 1))
    while i < n - 1:
        j = i + 2; best = i + 1
        nf = [f for f in force if f > i]
        lim = nf[0] if nf else n - 1
        if best > lim: best = lim
        while j <= lim and j - i <= maxgap:
            ok = True
            for t in range(i + 1, j):
                u = (t - i) / (j - i); v = L[i] * (1 - u) + L[j] * u
                if np.sqrt(np.mean((env(lsf2a(v)) - E[t]) ** 2)) > thr[t]: ok = False; break
            if not ok: break
            best = j; j += 1
        keys.append(best); i = best
    return np.array(sorted(set(keys)))


def ls_fit(L, keys, W=None):
    """least-squares keyframe values for a piecewise-linear curve through fixed key positions"""
    n = len(L); m = len(keys)
    A = np.zeros((n, m))
    for j in range(m - 1):
        a, b = keys[j], keys[j + 1]
        for t in range(a, b + 1):
            u = (t - a) / (b - a); A[t, j] = 1 - u; A[t, j + 1] = u
    if W is not None: A2 = A * W[:, None]; L2 = L * W[:, None]
    else: A2, L2 = A, L
    V, *_ = np.linalg.lstsq(A2, L2, rcond=None)
    return V


def build_bank(lines, an, thr=3.5, M=10, qscale=1.0, rdb=1.5, margin=2, thr_mid=None, edge=0.025, uniform=False, lsfit=False, pw=0.0, tags=None, eh=4):
    """collect unique words -> keyframe data.  Returns dict with decoded (reconstructed)
    frames for testing and the packed streams.  tags[i] prefixes the word keys of line i
    (one bank can hold the words of several speakers)."""
    bank = {}; order = []
    for li, (line, a) in enumerate(zip(lines, an)):
        for w in a['words']:
            key = (tags[li] if tags else '') + w['word'] + '=' + ' '.join(w['ph'])
            if key in bank: continue
            f0 = int(np.floor(frame_of(w['bnd'][0]))) - margin
            f1 = int(np.ceil(frame_of(w['bnd'][-1]))) + margin
            f0 = max(f0, 0); f1 = min(f1, len(a['L']) - 1)
            L = a['L'][f0:f1 + 1]; G = a['G'][f0:f1 + 1]
            E = np.array([env(lsf2a(v)) for v in L])
            t0 = (f0 * HOP + WIN / 2) / SR  # time of frame f0
            bnd = [b - t0 for b in w['bnd']]
            force = []
            if thr_mid is None: th = thr
            else:
                th = np.full(len(L), float(thr_mid))
                ft = np.arange(len(L)) * 0.005
                for b in bnd: th[np.abs(ft - b) < edge] = thr
                for pi, p in enumerate(w['ph']):
                    if CLS[p] == 0:
                        c = (bnd[pi] + bnd[pi + 1]) / 2
                        force.append(int(round(c / 0.005)))
                        if p in DIPH: force.append(int(round((bnd[pi] + 0.3 * (bnd[pi + 1] - bnd[pi])) / 0.005)))
            keys = vfr_keys(L, E, th, force=force)
            V = ls_fit(L, keys, np.sqrt(G / G.max()) + 0.05) if lsfit else L[keys]
            bank[key] = dict(ph=w['ph'], L=L, G=G, keys=keys, bnd=bnd, n=len(L), V=V)
            order.append(key)
    # PCA on keyframe LSFs
    X = np.concatenate([bank[k]['V'] for k in order])
    mu = X.mean(0)
    Wp = np.exp(-pw * np.arange(16) / 15)          # perceptual weight: low LSFs matter more
    U, S, Vt = np.linalg.svd((X - mu) * Wp, full_matrices=False)
    B = Vt[:M] / Wp; Bi = np.linalg.pinv(B); sd = S[:M] / np.sqrt(len(X))
    step = qscale * 0.25 * sd * (np.arange(M) * 0.08 + 1) if not uniform else np.full(M, qscale * 0.01)
    for k in order:
        b = bank[k]
        c = (b['V'] - mu) @ Bi
        q = np.round(c / step).astype(int)
        b['q'] = q
        rec = mu + (q * step) @ B
        rec = np.maximum.accumulate(np.clip(rec, 0.005, np.pi - 0.005), axis=1)
        for j in range(1, ORDER): rec[:, j] = np.maximum(rec[:, j], rec[:, j - 1] + 0.01)
        b['Lq'] = rec
        idx = np.arange(0, b['n'], eh)
        lg = np.log2(b['G'][idx] + 1e-6)
        rq = np.clip(np.round(lg / (rdb / 6.02)), -70, 0).astype(int)
        b['rq'] = rq
    return dict(bank=bank, order=order, mu=mu, B=B, step=step, M=M, rdb=rdb, eh=eh)


MD = 3


def pack(bk):
    """planar streams (LZMA-friendly); returns dict name->bytes"""
    bank, order = bk['bank'], bk['order']
    S = {k: bytearray() for k in ('nph', 'ph', 'dur', 'nk', 'gap', 'rms', 'coef')}
    LAB = set('m b p p_h f v w'.split()); RND = set('u U O @U OI w r='.split())
    for k in order:
        b = bank[k]
        S['nph'].append(len(b['ph']))
        for p in b['ph']:
            if CLS[p] == 0: S['ph'].append(64 | VOWID.index(p) | (32 if p in RND else 0) | (16 if p in DIPH else 0))
            else: S['ph'].append(CLS[p] | (16 if p in LAB else 0) | (32 if p in RND else 0))
        prev = 0
        for t in b['bnd']:
            v = int(round((t) / 0.005)); S['dur'].append(max(0, min(255, v - prev))); prev = v
        S['nk'].append(len(b['keys']))
        for g in np.diff(b['keys']): S['gap'].append(int(g))
        pr = 0
        for v in b['rq']: S['rms'].append((int(v) - pr) & 255); pr = int(v)
    mx = 0
    for j in range(bk['M']):                      # the first components move smoothly: deltas; the others are noise-like: values
        for k in order:
            q = bank[k]['q'][:, j]; pr = 0
            for v in q:
                d = int(v) - (pr if j < MD else 0); mx = max(mx, abs(d))
                S['coef'].append(d & 255); pr = int(v)
    bk['maxdelta'] = mx
    if mx > 127: print('WARNING coef delta overflow', mx, file=sys.stderr)
    return S


def emit_header(bk, S, fn):
    """writes the C header with the packed bank + PCA tables"""
    M = bk['M']
    blob = bytearray()
    for k in ('nph', 'nk', 'ph', 'dur', 'gap', 'rms', 'coef'): blob += S[k]
    mu = np.round(bk['mu'] / np.pi * 32767).astype(int)
    Bs = np.abs(bk['B']).max()
    B = np.round(bk['B'] / Bs * 127).astype(int)
    st = bk['step'] * Bs / np.pi  # coefficient step folded with basis scale (in units of pi)
    o = ['/* generated by tools/voicebank.py - do not edit */',
         '#define VB_WORDS %d' % len(bk['order']), '#define VB_M %d' % M, '#define VB_RDB %.4ff' % bk['rdb'], '#define VB_EH %d' % bk.get('eh', 4), '#define VB_MD %d' % MD,
         'static const short vb_mu[16]={%s};' % ','.join(map(str, mu)),
         'static const signed char vb_B[%d]={%s};' % (M * 16, ','.join(map(str, B.flatten()))),
         'static const float vb_step[%d]={%s};' % (M, ','.join('%.6gf' % v for v in st / 127)),
         'static const unsigned char vb_blob[%d]={%s};' % (len(blob), ','.join(map(str, blob)))]
    open(fn, 'w').write('\n'.join(o) + '\n')
    # word index for the score compiler
    return {k: i for i, k in enumerate(bk['order'])}


if __name__ == '__main__':
    import lzma
    lines = read_lyrics()
    an = analyse_lines(lines)
    for thr in [3.5, 5.0]:
        for M, qs in [(10, 1.0), (8, 1.5)]:
            bk = build_bank(lines, an, thr=thr, M=M, qscale=qs)
            S = pack(bk)
            tot = b''.join(bytes(v) for v in S.values())
            print('thr', thr, 'M', M, 'qs', qs, 'words', len(bk['order']), 'keys', sum(len(bk['bank'][k]['keys']) for k in bk['order']),
                  {k: len(v) for k, v in S.items()}, 'lzma', len(lzma.compress(tot, preset=9 | lzma.PRESET_EXTREME)))
