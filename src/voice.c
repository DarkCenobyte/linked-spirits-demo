/* LINKED//SPIRITS - runtime singing synthesiser.
 *
 * The word bank (gen_voice.h) holds, for every unique sung word, sparse
 * keyframes of a 16-pole vocal-tract envelope (PCA-coded line spectral
 * frequencies), a 20 ms energy contour and the phoneme segmentation.
 * Everything else is generated here: melody, glottal source, breath,
 * vibrato, formant scaling (voice identity), time-warping of vowels onto
 * notes, coarticulation across word joins and the lipsync curves.
 */
#include "gen_voice.h"

#define VSR 16000.0f
#ifndef MELP
#define MELP 0.5f
#endif
#ifndef DHOLD
#define DHOLD 0.35f
#endif
#ifndef VMORPH
#define VMORPH 0.8f
#endif
#ifndef VFT
#define VFT 1        /* high notes: first formant follows the pitch */
#endif
#ifndef VBW
#define VBW 0.025f   /* high notes: formant widening */
#endif
#ifndef VCMP
#define VCMP 0.6f    /* sung phrasing: energy compression exponent */
#endif
#ifndef VCB
#define VCB 0        /* 0: first-version diction with gentler sibilants, 1: gentler consonant boosts overall */
#endif
#ifndef VGL
#define VGL 1        /* 1: natural pitch (glides, scoops, irregular vibrato, drift, jitter), 0: the first version */
#endif
#ifndef VSUS
#define VSUS 0.35f   /* held vowels: how much spoken stress remains */
#endif
#define VMAXF 24000
#define VMAXP 1024
#define VMAXS 256

static float vf_l[VMAXF][16];          /* per 5 ms frame: LSF (radians) */
static float vf_g[VMAXF];              /* per 5 ms frame: excitation gain */
static int vw_p0[VB_WORDS + 1];         /* first phoneme of word */
static unsigned char vp_fl[VMAXP];      /* phoneme class | flags */
static float vp_r0[VMAXP], vp_r1[VMAXP];/* phoneme span in the frame pool */
static float vt_l[16][16];              /* canonical vowel targets (mean of all occurrences) */
#define VCLS(f) ((f) & 64 ? 0 : (f) & 7)

typedef struct { float pitch, start, dur; int syl; } VNote;
typedef struct {
    float alpha;   /* formant scale (runtime vocal tract length) */
    float tilt;    /* 1 = flat glottal spectrum, <1 = softer */
    float breath;  /* aspiration noise in voiced sounds */
    float vibr, vibd; /* vibrato rate (Hz) / depth (semitones) */
    float sub;     /* sub-octave glottal component (cyborg depth) */
    float dual;    /* second glottal source at +interval (merged voice) */
    float dualint; /* interval of the second source (semitones) */
    float gain;
} VStyle;

static unsigned vrs = 12345u;
static float vrnd(void) { vrs = vrs * 1664525u + 1013904223u; return (float)(int)vrs * (1.0f / 2147483648.0f); }

static void lsf_fix(float *l)
{
    int i;
    if (l[0] < 0.006f) l[0] = 0.006f;
    for (i = 1; i < 16; i++) if (l[i] < l[i - 1] + 0.012f) l[i] = l[i - 1] + 0.012f;
    if (l[15] > 3.13f) l[15] = 3.13f;
}

void voice_init(void)
{
    const unsigned char *p = vb_blob, *nph = p, *nk = p + VB_WORDS, *ph, *du, *gp, *rm, *cf;
    int w, i, j, np = 0, ngp = 0, nrm = 0, nkt = 0, fr = 0;
    for (w = 0; w < VB_WORDS; w++) { np += nph[w]; nkt += nk[w]; }
    ph = nk + VB_WORDS; du = ph + np; gp = du + np + VB_WORDS;
    for (w = 0; w < VB_WORDS; w++) {
        int n = 1; for (i = 0; i < nk[w] - 1; i++) n += gp[ngp + i];
        ngp += nk[w] - 1; nrm += (n + 3) >> 2;
    }
    rm = gp + ngp; cf = rm + nrm;
    {
        const unsigned char *g = gp, *r = rm; int pp = 0, kbase = 0;
        for (w = 0; w < VB_WORDS; w++) {
            int k, key[256], n, t = 0, rq = 0; float L[256][16];
            key[0] = 0; for (k = 1; k < nk[w]; k++) key[k] = key[k - 1] + *g++;
            n = key[nk[w] - 1] + 1;
            /* keyframes */
            for (k = 0; k < nk[w]; k++) {
                for (i = 0; i < 16; i++) L[k][i] = vb_mu[i] * (1.0f / 32767.0f);
            }
            for (j = 0; j < VB_M; j++) {
                const unsigned char *c = cf + j * nkt + kbase; int q = 0;
                for (k = 0; k < nk[w]; k++) {
                    q += (signed char)c[k];
                    for (i = 0; i < 16; i++) L[k][i] += q * vb_step[j] * vb_B[j * 16 + i];
                }
            }
            for (k = 0; k < nk[w]; k++) { for (i = 0; i < 16; i++) L[k][i] *= 3.14159265f; lsf_fix(L[k]); }
            kbase += nk[w];
            /* interpolate to frames */
            for (k = 0; k < nk[w] - 1; k++)
                for (t = key[k]; t <= key[k + 1]; t++) {
                    float u = (float)(t - key[k]) / (key[k + 1] - key[k]);
                    for (i = 0; i < 16; i++) vf_l[fr + t][i] = L[k][i] + (L[k + 1][i] - L[k][i]) * u;
                }
            if (nk[w] == 1) for (i = 0; i < 16; i++) vf_l[fr][i] = L[0][i];
            /* energy: log2 steps every 4 frames */
            {
                float lg[256]; int m = (n + 3) >> 2;
                for (k = 0; k < m; k++) { rq += (signed char)*r++; lg[k] = rq * (VB_RDB / 6.02f); }
                for (t = 0; t < n; t++) {
                    int a = t >> 2; float u = (t & 3) * 0.25f;
                    float v = a + 1 < m ? lg[a] + (lg[a + 1] - lg[a]) * u : lg[m - 1];
                    vf_g[fr + t] = exp2f(v);
                }
            }
            /* phonemes */
            vw_p0[w] = pp;
            {
                int b = *du++;
                for (i = 0; i < nph[w]; i++) {
                    int e = b + *du++;
                    vp_fl[pp] = *ph++; vp_r0[pp] = (float)(fr + b); vp_r1[pp] = (float)(fr + e);
                    b = e; pp++;
                }
            }
            fr += n;
        }
        vw_p0[VB_WORDS] = pp;
        {
            float cnt[16] = { 0 };
            for (i = 0; i < pp; i++) if (vp_fl[i] & 64) {
                int id = vp_fl[i] & 15, c = (int)(vp_r0[i] + (vp_r1[i] - vp_r0[i]) * (vp_fl[i] & 16 ? DHOLD : 0.5f));
                for (j = 0; j < 16; j++) vt_l[id][j] += vf_l[c][j];
                cnt[id] += 1;
            }
            for (i = 0; i < 16; i++) for (j = 0; j < 16; j++) vt_l[i][j] /= cnt[i] + 1e-9f;
        }
    }
}

/* one line of singing ------------------------------------------------------ */
typedef struct { float t0, t1, r0, r1, gm; int ph, kind; } VSeg; /* kind: 0 cons, 1 vowel, 2 diphthong; gm: mean gain */

static void lsf2poly(const float *l, double *a)
{
    /* A(z) = (P(z)+Q(z))/2 from line spectral frequencies */
    double P[18], Q[18];
    int i, j;
    for (i = 0; i < 18; i++) P[i] = Q[i] = 0;
    P[0] = Q[0] = 1; P[1] = 1; Q[1] = -1;
    for (i = 0; i < 16; i += 2) {
        double cp = -2 * cos(l[i]), cq = -2 * cos(l[i + 1]);
        int n = i + 2;
        for (j = n + 1; j >= 2; j--) { P[j] += cp * P[j - 1] + P[j - 2]; Q[j] += cq * Q[j - 1] + Q[j - 2]; }
        P[1] += cp * P[0]; Q[1] += cq * Q[0];
    }
    for (i = 1; i <= 16; i++) a[i - 1] = 0.5f * (P[i] + Q[i]);
}

/* renders one sung line, adding into out (44.1 kHz mono).  words: bank indices.
 * mouth (optional, 100 Hz): jaw opening in [0,1] (bits 0-7), lip round (8-15). */
int voice_line(float *out, int outlen, int t0samp, const short *words, int nw,
               const VNote *nt, int nn, const VStyle *st, unsigned short *mouth)
{
    static VSeg sg[VMAXS * 4];
    int lp[VMAXP], nlp = 0, sb[VMAXS], sv[VMAXS], se[VMAXS], ns = 0, nsg = 0;
    int w, i, j, s;
    float isr = VSR * st->alpha;
    /* gather phonemes, syllabify per word */
    for (w = 0; w < nw; w++) {
        int p0 = vw_p0[words[w]], p1 = vw_p0[words[w] + 1], first = nlp, prevv = -1;
        for (i = p0; i < p1; i++) {
            lp[nlp] = i;
            if (vp_fl[i] & 64) {
                if (prevv < 0) sb[ns] = first;
                else {
                    int nc = nlp - prevv - 1;
                    sb[ns] = prevv + (nc <= 1 ? 1 : 2);
                    se[ns - 1] = sb[ns];
                }
                sv[ns++] = nlp; prevv = nlp;
            }
            nlp++;
        }
        se[ns - 1] = nlp;
    }
    /* per syllable timing from notes */
    for (s = 0; s < ns; s++) {
        float start = -1, end = 0, on = 0, co = 0, vdur, sc = 1, t, nextOn;
        for (j = 0; j < nn; j++) if (nt[j].syl == s) { if (start < 0) start = nt[j].start; end = nt[j].start + nt[j].dur; }
        for (i = sb[s]; i < sv[s]; i++) on += (vp_r1[lp[i]] - vp_r0[lp[i]]) * 0.005f;
        for (i = sv[s] + 1; i < se[s]; i++) co += (vp_r1[lp[i]] - vp_r0[lp[i]]) * 0.005f;
        if (s + 1 < ns) {
            float nstart = 0, non = 0;
            for (j = 0; j < nn; j++) if (nt[j].syl == s + 1) { nstart = nt[j].start; break; }
            for (i = sb[s + 1]; i < sv[s + 1]; i++) non += (vp_r1[lp[i]] - vp_r0[lp[i]]) * 0.005f;
            nextOn = nstart - non;
            if (nextOn < end) end = nextOn;
        }
        vdur = end - co - start;
        if (vdur < 0.07f) { sc = (end - start - 0.07f) / (co + 1e-6f); if (sc < 0.35f) sc = 0.35f; vdur = end - co * sc - start; }
        t = start - on;
        for (i = sb[s]; i < se[s]; i++) {
            VSeg *g = &sg[nsg++]; int p = lp[i];
            float d = (vp_r1[p] - vp_r0[p]) * 0.005f;
            g->ph = p; g->r0 = vp_r0[p]; g->r1 = vp_r1[p];
            { int f; float sm = 0; for (f = (int)g->r0; f < (int)g->r1; f++) sm += vf_g[f]; g->gm = sm / (g->r1 - g->r0 + 1e-6f); }
            g->kind = i == sv[s] ? ((vp_fl[p] & 16) ? 2 : 1) : 0;
            if (i == sv[s]) d = vdur; else if (i > sv[s]) d *= sc;
            g->t0 = t; g->t1 = t + d; t += d;
        }
    }
    /* render */
    {
        double bs[17], a[16]; float lsf[16], pit = nt[0].pitch - 0.5f, ph1 = 0, ph2 = 0, ph3 = 0, de = 0, lpv = 0, lpn = 0;
        float vo = 0, no = 0, gs = 0, prevv = 0, pv = 0, drift = 0, vr = st->vibr, vph = 0, jf = 1, sh = 1, G = 0, cnt = 0;
        float tstart = sg[0].t0 - 0.02f, tend = sg[nsg - 1].t1 + 0.05f;
        int nlast = 0;
        /* the line's sung level: the mean of its vowels (speech stress is flattened around it) */
        for (i = 0; i < nsg; i++) if (sg[i].kind) { G += sg[i].gm; cnt += 1; }
        G = G / (cnt + 1e-6f) + 1e-6f;
        int n0 = (int)(tstart * isr), n1 = (int)(tend * isr), n, si = 0, ni = 0;
        float *buf; int blen = n1 - n0 + 8;
        static float vbuf[16000 * 60];
        buf = vbuf;
        for (i = 0; i < 17; i++) bs[i] = 0;
        for (n = 0; n < blen; n++) buf[n] = 0;
        for (n = n0; n < n1; n++) {
            float t = n / isr, x;
            if ((n & 15) == 0 || n == n0) {
                /* control rate: segment, envelope, voicing */
                float r, g, tv = 1, tn = 0; int fl, kind; VSeg *c;
                while (si < nsg - 1 && t >= sg[si].t1) si++;
                c = &sg[si]; fl = vp_fl[c->ph]; kind = c->kind;
                if (t < c->t0) r = c->r0; else if (t >= c->t1) r = c->r1 - 0.01f;
                else {
                    float D = c->t1 - c->t0, R = (c->r1 - c->r0) * 0.005f, x2 = t - c->t0, e = 0.04f;
                    if (e > R * 0.3f) e = R * 0.3f;
                    if (kind == 2) {
                        float h = R * DHOLD, gl = D * 0.22f; if (gl < 0.1f) gl = 0.1f; if (gl > 0.3f) gl = 0.3f; if (gl > D * 0.45f) gl = D * 0.45f;
                        r = x2 < e ? x2 : x2 < D - gl ? e + (h - e) * (x2 - e) / (D - gl - e) : h + (R - h) * (x2 - (D - gl)) / gl;
                    } else if (kind == 1 && D > 2 * e) {
                        r = x2 < e ? x2 : x2 > D - e ? R - (D - x2) : e + (R - 2 * e) * (x2 - e) / (D - 2 * e);
                    } else r = x2 / D * R;
                    r = c->r0 + r * 200.0f;
                }
                {
                    int i0 = (int)r; float u = r - i0;
                    for (i = 0; i < 16; i++) lsf[i] = vf_l[i0][i] + (vf_l[i0 + 1][i] - vf_l[i0][i]) * u;
                    g = vf_g[i0] + (vf_g[i0 + 1] - vf_g[i0]) * u;
                    /* coarticulation across word joins: blend towards the neighbour */
                    if (si + 1 < nsg && sg[si + 1].r0 != c->r1 && t > c->t1 - 0.02f) {
                        float wgt = 0.5f * (t - (c->t1 - 0.02f)) / 0.02f; int k2 = (int)sg[si + 1].r0;
                        for (i = 0; i < 16; i++) lsf[i] += (vf_l[k2][i] - lsf[i]) * wgt;
                    } else if (si > 0 && sg[si - 1].r1 != c->r0 && t < c->t0 + 0.02f && t >= c->t0) {
                        float wgt = 0.5f - 0.5f * (t - c->t0) / 0.02f; int k2 = (int)sg[si - 1].r1 - 1;
                        for (i = 0; i < 16; i++) lsf[i] += (vf_l[k2][i] - lsf[i]) * wgt;
                    }
                }
                if (kind && t > c->t0 && t < c->t1) {   /* a held note sustains at its vowel's level, with a slight decay */
                    float D = c->t1 - c->t0, x2 = t - c->t0, h = (x2 - 0.03f) / 0.05f, h2 = (D - x2 - 0.04f) / 0.06f;
                    h = h < 0 ? 0 : h > 1 ? 1 : h; h2 = h2 < 0 ? 0 : h2 > 1 ? 1 : h2;
                    g += (G * powf(c->gm / G + 1e-9f, VSUS) - g) * h * h2;   /* near the line's level: sung, not spoken, stress */
                    g *= 1 - 0.12f * (x2 > 0.3f ? (x2 - 0.3f < 1.5f ? (x2 - 0.3f) / 1.5f : 1) : 0);
                }
                g = G * powf(g / G + 1e-9f, VCMP);        /* sung phrasing: stress contrasts compressed */
                if (t < sg[0].t0 || t > sg[nsg - 1].t1) g = 0;
                /* sung diction: consonants are articulated harder than in speech (sibilants gently) */
                { static const float CB[2][8] = { { 1.0f, 1.25f, 1.7f, 1.9f, 1.6f, 1.5f, 1.8f, 1.6f }, { 1.0f, 1.2f, 1.55f, 1.65f, 1.35f, 1.3f, 1.5f, 1.4f } };
                  g *= CB[VCB][VCLS(fl)]; }
                switch (VCLS(fl)) {
                case 0: case 1: tv = 1; tn = st->breath; break;
                case 2: tv = 0.35f; tn = 0.8f; break;
                case 5: case 7: tv = 0.6f; tn = 0.7f; break;
                default: tv = 0; tn = 1;
                }
                if (kind && t > c->t0 && t < c->t1) {   /* held sung vowel -> clear canonical vowel */
                    float D = c->t1 - c->t0, x2 = t - c->t0, wv = x2 < 0.06f ? x2 / 0.06f : 1;
                    if (kind == 2) { float gg = D * 0.22f; if (gg < 0.1f) gg = 0.1f; if (gg > 0.3f) gg = 0.3f; if (gg > D * 0.45f) gg = D * 0.45f; gg = D - gg; wv *= x2 < gg ? 1 : 1 - (x2 - gg) / (D - gg); }
                    else if (x2 > D - 0.05f) wv *= (D - x2) / 0.05f;
                    if (wv < 0) wv = 0;
                    wv *= VMORPH * (D > 0.12f ? 1 : D / 0.12f);
                    for (i = 0; i < 16; i++) lsf[i] += (vt_l[fl & 15][i] - lsf[i]) * wv;
                }
                {   /* high notes: the first formant follows the pitch (as a soprano opens her jaw) */
                    float f1 = (lsf[0] + lsf[1]) * 0.5f * isr / 6.2831853f, need = prevv * 1.05f;
                    if (VFT && tv > 0.5f && prevv > 470.0f && f1 < need) { float dl = (need - f1) * 3.7f / isr; if (dl > 0.15f) dl = 0.15f; lsf[0] += dl; lsf[1] += dl * 0.8f; }
                }
                lsf_fix(lsf); lsf2poly(lsf, a);
                {   /* and the resonances widen, so sparse harmonics never ring on a narrow peak */
                    float hp = (prevv - 420.0f) / 250.0f, gm = 1, gg;
                    hp = hp < 0 ? 0 : hp > 1 ? 1 : hp;
                    gg = 1 - VBW * hp;
                    for (i = 0; i < 16; i++) { gm *= gg; a[i] *= gm; }
                }
                gs = g;
                vo += (tv - vo) * 0.35f; no += (tn - no) * 0.35f;
                /* pitch */
                while (ni + 1 < nn && t >= nt[ni + 1].start - (VGL ? 0.04f : 0.015f)) ni++;   /* the voice anticipates the next note */
                {
                    float tg = nt[ni].pitch, dt = t - nt[ni].start, vib, vd, cr = 16.0f / isr;
                    if (VGL && ni != nlast) {              /* after a rest the note is approached from a little below */
                        if (nt[ni].start - (nt[nlast].start + nt[nlast].dur) > 0.2f) { pit = tg - 0.4f; pv = 0; }
                        nlast = ni;
                    }
                    if (VGL) { pv += (1800.0f * (tg - pit) - 60.0f * pv) * cr; pit += pv * cr; }   /* a smooth glide with a hint of overshoot */
                    else pit += (tg - pit) * 0.16f;
                    drift += (vrnd() * 0.12f - drift) * 0.004f;             /* slow wander, a few cents */
                    vr += (st->vibr * (1 + 0.08f * vrnd()) - vr) * 0.003f;   /* the vibrato is never quite regular */
                    vph += 6.2831853f * vr * cr;
                    vd = dt < 0.22f ? 0 : dt < 0.72f ? (dt - 0.22f) * 2 : 1;
                    vib = st->vibd * vd * (0.85f + 0.15f * sinf(t * 1.7f + vph * 0.05f)) * sinf(vph);
                    prevv = 440.0f * exp2f((pit + vib + drift - 69.0f) / 12.0f);
                }
                if (mouth) {
                    int mi = (int)(t * 100);
                    float f1 = (lsf[0] + lsf[1]) * 0.5f * VSR / 6.2831853f;
                    float op = (f1 - 250.0f) / 650.0f, rd = (fl & 32) ? 1.0f : 0.0f;
                    if (op < 0) op = 0; if (op > 1) op = 1;
                    if (g < 0.002f) op = 0;
                    if (!(fl & 64) && (fl & 16)) op *= 0.1f;
                    if (mi >= 0 && mi < 100 * 480) mouth[mi] = (unsigned short)((int)(op * 255) | ((int)(rd * 255) << 8));
                }
            }
            /* excitation: band-limited pulse train(s) */
            {
                float f0 = prevv * jf, v = 0, nz, H, tl;
                ph1 += f0 / isr; if (ph1 >= 1) { ph1 -= 1; jf = 1 + 0.004f * vrnd(); sh = 1 + 0.035f * vrnd(); }   /* jitter, shimmer */
                H = floorf(isr * 0.5f / f0);
                {
                    float p2 = ph1 * 6.2831853f, d = sinf(p2 * 0.5f);
                    v = (fabsf(d) < 1e-5f ? H : sinf((H + 0.5f) * p2) / (2 * d) - 0.5f) * sqrtf(2.0f / H);
                }
                if (st->sub > 0) {
                    float p2, d, H2 = floorf(isr * 0.5f / (f0 * 0.5f));
                    ph2 += f0 * 0.5f / isr; if (ph2 >= 1) ph2 -= 1;
                    p2 = ph2 * 6.2831853f; d = sinf(p2 * 0.5f);
                    v += st->sub * (fabsf(d) < 1e-5f ? H2 : sinf((H2 + 0.5f) * p2) / (2 * d) - 0.5f) * sqrtf(2.0f / H2);
                }
                if (st->dual > 0) {
                    float f2 = f0 * exp2f(st->dualint / 12.0f), p2, d, H2 = floorf(isr * 0.5f / f2);
                    ph3 += f2 / isr; if (ph3 >= 1) ph3 -= 1;
                    p2 = ph3 * 6.2831853f; d = sinf(p2 * 0.5f);
                    v += st->dual * (fabsf(d) < 1e-5f ? H2 : sinf((H2 + 0.5f) * p2) / (2 * d) - 0.5f) * sqrtf(2.0f / H2);
                }
                v *= sh;
                tl = (prevv - 330.0f) / 300.0f; tl = st->tilt * (1 - 0.3f * (tl < 0 ? 0 : tl > 1 ? 1 : tl));   /* softer at the top */
                lpv += (v - lpv) * tl;
                nz = vrnd() * 1.732f;
                lpn += (nz - lpn) * 0.9f;
                if (MELP > 0) {   /* mixed excitation: periodic below ~4 kHz, pulse-gated noise above */
                    static float hl, hn;
                    hl += (lpv - hl) * 0.6f; hn += (nz - hn) * 0.6f;
                    lpv = hl + (lpv - hl) * (1 - MELP) + (nz - hn) * MELP * (ph1 < 0.35f ? 1.6f : 0.4f);
                }
                x = vo * lpv * (tl < 1 ? 1.0f / sqrtf(tl) : 1.0f) + no * lpn * (vo > 0.5f ? (0.4f + 0.6f * (ph1 < 0.5f)) : 1.0f);
            }
            x *= gs;
            {
                double y = x;
                for (i = 0; i < 16; i++) y -= a[i] * bs[i];
                for (i = 15; i > 0; i--) bs[i] = bs[i - 1];
                bs[0] = y;
                de = (float)y + 0.9f * de;
            }
            buf[n - n0] = de;
        }
        /* resample to 44.1 kHz, add */
        {
            float ratio = isr / 44100.0f;
            int o0 = (int)(tstart * 44100.0f), o1 = (int)(tend * 44100.0f), o;
            for (o = o0; o < o1; o++) {
                float p = o * ratio - n0; int k = (int)p; float u = p - k, A, B, C, D;
                if (k < 1 || k >= blen - 3 || o + t0samp < 0 || o + t0samp >= outlen) continue;
                A = buf[k - 1]; B = buf[k]; C = buf[k + 1]; D = buf[k + 2];
                out[o + t0samp] += st->gain * (B + 0.5f * u * (C - A + u * (2 * A - 5 * B + 4 * C - D + u * (3 * (B - C) + D - A))));
            }
        }
    }
    return nsg;
}
