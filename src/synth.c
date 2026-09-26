/* LINKED//SPIRITS - soundtrack synthesiser.
 * Everything is rendered at startup into 16-bit stereo, in worker threads:
 * drums, chip bass, chip arps/lead, pads + granular "particle" textures + FX,
 * and the three singing voices.  The mixdown adds reverb, ping-pong delay,
 * kick sidechain and vocal ducking (the music always yields to the words). */
#include "gen_score.h"
#include "voice.c"

#define SR 44100
#define SPT 6615                  /* samples per 16th note at 100 BPM */
#define SPB (SPT * 16)
#define SONG_LEN (NBARS * SPB)
#define PI 3.14159265f
#ifndef TRANSP
#define TRANSP 0
#endif

enum { T_DRUM, T_BASS, T_CHIPL, T_CHIPR, T_BEDL, T_BEDR, T_VA, T_VB, NTRK };
static float *trk[NTRK];
static short *song16;
static volatile int synth_ndone;
static volatile float synth_progress;
static unsigned short mouth_a[48000], mouth_b[48000];

#ifdef _WIN32
#define ALLOC(n) VirtualAlloc(0, (n), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
#else
#include <pthread.h>
#define ALLOC(n) calloc(1, (n))
#endif

/* ------------------------------------------------------------------ helpers */
static unsigned rng_s = 0x9e3779b9u;
static float rnd(unsigned *s) { *s = *s * 1664525u + 1013904223u; return (float)(int)*s * (1.0f / 2147483648.0f); }
static float mtof(float m) { return 440.0f * exp2f((m - 69.0f) * (1.0f / 12.0f)); }
static float sat(float x) { return x / (1.0f + fabsf(x)); }
static float blep(float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1; }
    if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
    return 0;
}
static float saw(float ph, float dt) { return 2 * ph - 1 - blep(ph, dt); }
static float pulse(float ph, float dt, float w)
{
    float p2 = ph + w; if (p2 >= 1) p2 -= 1;
    return saw(ph, dt) - saw(p2, dt);
}

typedef struct { float a, b; } SVF;   /* TPT state variable filter */
static float svf(SVF *s, float x, float g, float k, int mode)
{
    float a1 = 1 / (1 + g * (g + k)), v3 = x - s->b, v1 = a1 * s->a + g * a1 * v3, v2 = s->b + g * v1;
    s->a = 2 * v1 - s->a; s->b = 2 * v2 - s->b;
    return mode == 0 ? v2 : mode == 1 ? v1 : x - k * v1 - v2;
}
static float fcut(float hz) { if (hz > 20000) hz = 20000; return tanf(PI * hz / SR); }

static int chord_notes(int bar, int *n)   /* chord tones (semitones above C4 region) */
{
    static const signed char Q[8][5] = { {0,3,7,12,15}, {0,4,7,12,16}, {0,4,7,10,12}, {0,5,7,10,12},
                                         {0,2,7,12,14}, {0,3,7,10,14}, {0,4,7,11,14}, {0,3,7,9,12} };
    int c = sc_chord[bar < NBARS ? bar : NBARS - 1], i, r = (c & 15) + TRANSP;
    for (i = 0; i < 5; i++) n[i] = r + Q[c >> 4][i];
    return r;
}
static int ARf(int bar, int sh, int mask) { return bar < 0 || bar >= NBARS ? 0 : (sc_arr[bar] >> sh) & mask; }
#define A_DRUM(b) ARf(b, 0, 7)
#define A_BASS(b) ARf(b, 3, 3)
#define A_ARP(b)  ARf(b, 5, 3)
#define A_PAD(b)  ARf(b, 7, 3)
#define A_LEAD(b) ARf(b, 9, 1)
#define A_GRAN(b) ARf(b, 10, 3)
#define A_DRONE(b) ARf(b, 12, 1)
#define A_DARK(b) ARf(b, 13, 1)
#define A_BRTH(b) ARf(b, 14, 1)

static float env_adsr(int i, int len, float a, float d, float s, float r)
{
    float t = i * (1.0f / SR), e, rel = len * (1.0f / SR);
    e = t < a ? t / a : s + (1 - s) * expf(-(t - a) / d);
    if (t > rel) e *= expf(-(t - rel) / r);
    return e;
}

/* ------------------------------------------------------------------ drums */
static const unsigned short DK[8] = { 0, 0x0101, 0x0481, 0x2491, 0x1111, 0x1111, 0x0801, 0x4949 };  /* kick bits (1<<step) */
static const unsigned short DS[8] = { 0, 0, 0x1010, 0x1010, 0x1010, 0x1010, 0x0100, 0x1010 };
static const unsigned short DH[8] = { 0, 0x4444, 0xffff, 0xffff, 0xffff, 0xffff, 0x5555, 0xffff };
static const unsigned short DO[8] = { 0, 0, 0, 0x4000, 0x4444, 0x4444, 0, 0x4444 };
static int kick_pos[3000], nkick;

static void drum_kick(float *o, int p, float v)
{
    int i; float ph = 0; unsigned s = 7;
    for (i = 0; i < 26000 && p + i < SONG_LEN; i++) {
        float t = i * (1.0f / SR), f = 43 + 120 * expf(-t * 28) + 500 * expf(-t * 350);
        ph += f / SR;
        o[p + i] += v * sat(1.8f * (sinf(2 * PI * ph) * expf(-t * 5.0f) * (t < 0.03f ? 1 : 1) + rnd(&s) * 0.3f * expf(-t * 500)));
    }
}
static void drum_snare(float *o, int p, float v, int heavy)
{
    int i; float ph = 0, lp = 0; SVF f = { 0 }; unsigned s = 11;
    for (i = 0; i < 20000 && p + i < SONG_LEN; i++) {
        float t = i * (1.0f / SR), n = rnd(&s), cl = 0, x;
        ph += (180 + 60 * expf(-t * 40)) / SR;
        cl = expf(-t * 120) + expf(-fabsf(t - 0.011f) * 150) * 0.8f + expf(-fabsf(t - 0.022f) * 150) * 0.6f;
        x = svf(&f, n, 0.22f, 1.2f, 1) * (cl * 0.8f + expf(-t * (heavy ? 7.0f : 13.0f)) * 0.6f);
        lp += (n - lp) * 0.5f;
        x += sinf(2 * PI * ph) * expf(-t * 22) * 0.7f + (n - lp) * expf(-t * 16) * 0.3f;
        o[p + i] += v * x;
    }
}
static void drum_hat(float *o, int p, float v, float dec)
{
    int i; float h = 0, prev = 0; unsigned s = 23;
    static const float F[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
    float ph[6] = { 0 };
    for (i = 0; i < 16000 && p + i < SONG_LEN; i++) {
        float t = i * (1.0f / SR), m = 0, x; int k;
        for (k = 0; k < 6; k++) { ph[k] += F[k] * 4.3f / SR; if (ph[k] > 1) ph[k] -= 1; m += ph[k] < 0.5f ? 1 : -1; }
        x = m * 0.15f + rnd(&s) * 0.5f;
        h = x - prev; prev = x;
        o[p + i] += v * h * expf(-t * dec);
    }
}
static void th_drums(void *u)
{
    float *o = trk[T_DRUM]; int bar, st;
    unsigned s = 99;
    (void)u;
    for (bar = 0; bar < NBARS; bar++) {
        int d = A_DRUM(bar), base = bar * SPB;
        if (!d) continue;
        for (st = 0; st < 16; st++) {
            int p = base + st * SPT + (st & 1 ? SPT / 9 : 0);   /* a little swing */
            float acc = (st & 3) == 0 ? 1.0f : (st & 1) ? 0.55f : 0.75f;
            if (DK[d] >> st & 1) { drum_kick(o, base + st * SPT, 0.9f); kick_pos[nkick++] = base + st * SPT; }
            if (DS[d] >> st & 1) drum_snare(o, base + st * SPT, d == 6 ? 0.55f : 0.45f, d == 6);
            if (DH[d] >> st & 1) drum_hat(o, p, 0.10f * acc * (0.8f + 0.2f * rnd(&s)), 70);
            if (DO[d] >> st & 1) drum_hat(o, p, 0.08f, 11);
            if (d == 5 && bar % 2 == 1 && st >= 8) drum_snare(o, base + st * SPT, 0.12f + st * 0.012f, 0);  /* build roll */
            if (d == 7 && (bar & 3) == 3 && st >= 12) drum_snare(o, base + st * SPT, 0.3f, 0);             /* fills */
        }
        if (d == 7 && (bar & 3) == 0) drum_hat(o, base, 0.22f, 2.5f);   /* crash */
    }
    __sync_fetch_and_add(&synth_ndone, 1);
}

/* ------------------------------------------------------------------ bass */
static void th_bass(void *u)
{
    float *o = trk[T_BASS]; int bar, i; (void)u;
    for (bar = 0; bar < NBARS; bar++) {
        int b = A_BASS(bar), n[5], root = chord_notes(bar, n), st;
        float f;
        if (!b) continue;
        root = 38 + ((root + 10) % 12) - 10 + 12;     /* around D2..C3 */
        for (st = 0; st < 16; st++) {
            static const unsigned short BP[4] = { 0x0001, 0x0001, 0x5555, 0xb6b5 };
            int len, p = bar * SPB + st * SPT, oct;
            if (!(BP[b] >> st & 1)) continue;
            len = b == 1 ? SPB - 2000 : b == 2 ? SPT * 2 - 800 : SPT - 600;
            oct = (b == 2 && (st & 6) == 6) || (b == 3 && st % 5 == 3) ? 12 : 0;
            f = mtof((float)(root + oct));
            {
                float ph = 0, ph2 = 0; SVF fl = { 0 };
                for (i = 0; i < len + 4000 && p + i < SONG_LEN; i++) {
                    float e = env_adsr(i, len, 0.004f, 0.25f, 0.7f, 0.03f), tri, x, cut;
                    ph += f / SR; if (ph >= 1) ph -= 1;
                    ph2 += f * 0.5f / SR; if (ph2 >= 1) ph2 -= 1;
                    tri = floorf((4 * fabsf(ph - 0.5f) - 1) * 8) * 0.125f;          /* 4-bit chip triangle */
                    cut = 200 + 1400 * expf(-i * (b == 3 ? 12.0f : 6.0f) / SR) * (b >= 2);
                    x = tri * 0.5f + sinf(2 * PI * ph2) * 0.55f + svf(&fl, saw(ph, f / SR), fcut(cut), 1.0f, 0) * 0.45f * (b >= 2);
                    o[p + i] += 0.5f * e * x;
                }
            }
        }
    }
    __sync_fetch_and_add(&synth_ndone, 1);
}

/* ------------------------------------------------------------------ chip arps + lead (stereo) */
static void chip_note(int p, int len, float f, float vol, float pan, float duty, int shape)
{
    float ph = 0, *L = trk[T_CHIPL], *R = trk[T_CHIPR], dt = f / SR; int i;
    for (i = 0; i < len + 3000 && p + i < SONG_LEN; i++) {
        float e = env_adsr(i, len, 0.002f, shape ? 0.4f : 0.07f, shape ? 0.8f : 0.25f, 0.02f), x, w;
        float vib = shape && i > 9000 ? 1 + 0.005f * sinf(i * (2 * PI * 5.5f / SR)) : 1;
        ph += dt * vib; if (ph >= 1) ph -= 1;
        w = duty + 0.12f * sinf(i * 0.00007f);
        x = pulse(ph, dt, w) * e * vol;
        L[p + i] += x * (1 - pan); R[p + i] += x * (1 + pan);
    }
}
static void th_chip(void *u)
{
    int bar, st, k; unsigned s = 5; (void)u;
    for (bar = 0; bar < NBARS; bar++) {
        int a = A_ARP(bar), n[5], dark = A_DARK(bar);
        chord_notes(bar, n);
        if (!a) continue;
        {
            int steps = a == 3 ? 32 : 16, step = SPB / steps;
            for (st = 0; st < steps; st++) {
                int idx, oct, note;
                if (a == 1 && (st & 1)) continue;
                idx = st % 8; idx = idx < 5 ? idx : 8 - idx;          /* up-down over 5 chord tones */
                oct = a == 3 ? ((st >> 3) & 1) * 12 : 0;
                note = 60 + n[idx] + oct - (n[0] > 5 ? 12 : 0);
                chip_note(bar * SPB + st * step, step - 300, mtof((float)note), (dark ? 0.035f : 0.055f) * (a == 3 ? 0.8f : 1),
                          0.4f * sinf(st * 0.7f + bar), a == 3 ? 0.125f : 0.25f, 0);
            }
        }
    }
    for (k = 0; k < NLEAD; k++) {
        int p = (LEAD_BAR * 16 + lead_notes[k * 3 + 1]) * SPT;
        chip_note(p, lead_notes[k * 3 + 2] * SPT - 500, mtof(lead_notes[k * 3] + TRANSP), 0.09f, 0, 0.5f, 1);
    }
    /* climax lead: the android motif, doubled at the octave */
    for (bar = 0; bar < NBARS; bar++) if (A_LEAD(bar) && bar < LEAD_BAR) {
        static const signed char M[8] = { 9, 5, 4, 2, 9, 12, 11, 7 };
        for (k = 0; k < 4; k++) {
            int n = 64 + 12 + M[(bar & 1) * 4 + k] + 2;
            chip_note(bar * SPB + k * 4 * SPT, 4 * SPT - 800, mtof((float)n), 0.06f, -0.3f, 0.5f, 1);
        }
    }
    (void)s;
    __sync_fetch_and_add(&synth_ndone, 1);
}

/* ------------------------------------------------------------------ bed: pads, granular particles, drones, FX */
static void bed_add(int p, float x, float pan)
{
    if (p < 0 || p >= SONG_LEN) return;
    trk[T_BEDL][p] += x * (1 - pan);
    trk[T_BEDR][p] += x * (1 + pan);
}
static void grain(int p, float f, float dur, float vol, float pan, int kind)
{
    int n = (int)(dur * SR), i; float ph = 0, dt = f / SR;
    for (i = 0; i < n; i++) {
        float w = 0.5f - 0.5f * cosf(2 * PI * i / n), x;
        ph += dt; if (ph >= 1) ph -= 1;
        x = kind ? pulse(ph, dt, 0.3f) * 0.5f : sinf(2 * PI * ph);
        bed_add(p + i, x * w * vol, pan);
    }
}
static void fx_event(int bar, int pos, int kind, int par)
{
    int p = bar * SPB + pos * SPT, i, n;
    unsigned s = bar * 131 + pos;
    SVF f = { 0 }, f2 = { 0 };
    float ph = 0, ph2 = 0;
    switch (kind) {
    case FX_BLIP: grain(p, mtof((float)par), 0.09f, 0.10f, 0.3f * rnd(&s), 0); grain(p + 9000, mtof((float)par), 0.09f, 0.03f, -0.4f, 0); break;
    case FX_CUT:
        for (i = 0; i < 30000; i++) { float t = i / (float)SR; ph += (50 + 40 * expf(-t * 30)) / SR; bed_add(p + i, 0.3f * sinf(2 * PI * ph) * expf(-t * 7), 0); }
        break;
    case FX_HUM: case FX_SWEEP:
        n = kind == FX_HUM ? SPB * 3 : SPB * 2;
        for (i = 0; i < n; i++) {
            float t = (float)i / n, e = sinf(PI * t) * (kind == FX_HUM ? 0.10f : 0.06f), x;
            ph += (kind == FX_HUM ? 55 + 55 * t : 1800 + 2500 * t) / SR; if (ph >= 1) ph -= 1;
            x = kind == FX_HUM ? svf(&f, saw(ph, 110.0f / SR), fcut(300 + 2000 * t), 3.0f, 1) * 0.5f
                               : svf(&f, rnd(&s), fcut(1500 + 7000 * t), 6.0f, 1) * 0.6f + sinf(2 * PI * ph) * 0.15f;
            bed_add(p + i, x * e, kind == FX_SWEEP ? 2 * t - 1 : 0);
        }
        break;
    case FX_OPEN: case FX_SHIMMER: case FX_TITLE: case FX_EYEB:
        /* a soft chord bloom + a glassless high glint */
        for (i = 0; i < 40; i++) {
            float fr = mtof((float)(74 + ((i * 7) % 17) - (kind == FX_EYEB ? 5 : 0)));
            grain(p + (int)(i * (kind == FX_TITLE ? 2400 : 900) * (1 + rnd(&s) * 0.3f)), fr * (i & 1 ? 2 : 1), 0.25f + 0.2f * rnd(&s), 0.025f, rnd(&s) * 0.8f, 0);
        }
        if (kind != FX_EYEB) break;
        /* fallthrough into a low cold swell for the blue eye */
    case FX_EYER: case FX_IMPACT: case FX_DOOR:
        n = kind == FX_DOOR ? SR * 4 : SR * 3;
        for (i = 0; i < n; i++) {
            float t = (float)i / SR, x;
            ph += (kind == FX_EYEB ? 58.27f : 38 + 45 * expf(-t * 4)) / SR;
            x = sinf(2 * PI * ph) * expf(-t * (kind == FX_EYEB ? 1.2f : 2.0f)) * 0.8f;
            if (kind != FX_EYEB) x += svf(&f, rnd(&s), fcut(4000 * expf(-t * 3) + 100), 0.8f, 0) * expf(-t * 4) * 0.5f;
            if (kind == FX_EYER) x = sat(x * 3) * 0.6f + sinf(2 * PI * ph * 3.0f) * 0.1f * expf(-t);
            if (kind == FX_DOOR) x = x * 0.6f + svf(&f2, rnd(&s), fcut(300 + 500 * t), 1.5f, 1) * expf(-t * 0.8f) * (t < 0.3f ? t / 0.3f : 1) * 0.4f;
            bed_add(p + i, x * (kind == FX_IMPACT ? 0.25f + 0.1f * par : 0.4f), 0);
        }
        break;
    case FX_RISER:
        n = par * SPB;
        for (i = 0; i < n; i++) {
            float t = (float)i / n, x;
            ph += (200 + 1800 * t * t) / SR; if (ph >= 1) ph -= 1;
            x = svf(&f, rnd(&s), fcut(300 + 9000 * t * t), 2.0f, 1) * 0.6f + saw(ph, 0.01f) * 0.08f * t;
            bed_add(p - n + i, x * t * t * 0.3f, 0.5f * sinf(t * 20));
        }
        break;
    case FX_TURN: case FX_TOUCH: case FX_BLINK:
        n = kind == FX_TURN ? SR / 2 : SR / 5;
        for (i = 0; i < n; i++) {
            float t = (float)i / n;
            bed_add(p + i, svf(&f, rnd(&s), fcut(kind == FX_TURN ? 3000 - 2500 * t : 5000), 3, 1) * sinf(PI * t) * (kind == FX_TURN ? 0.12f : 0.05f), kind == FX_TURN ? 0.6f - t : 0);
        }
        if (kind == FX_BLINK && par >= 1) {
            grain(p + 3000, mtof(81 + (par == 2 ? 3 : 0)), 0.4f, 0.02f, -0.7f, 0);
            grain(p + 3000, mtof(74), 0.4f, 0.02f, 0.7f, 0);
        }
        break;
    case FX_WAVE:
        n = SPB * 4;
        for (i = 0; i < n; i++) {
            float t = (float)i / n, x;
            x = svf(&f, rnd(&s), fcut(200 + 6000 * t), 12.0f, 1) * 0.3f;
            bed_add(p + i, x * t, sinf(t * 30) * 0.7f);
        }
        for (i = 0; i < 180; i++) {
            int n5[5]; chord_notes(bar + i / 45, n5);
            grain(p + i * (SPB * 4 / 180), mtof((float)(62 + n5[i % 5] + 12 * ((i / 5) % 3))), 0.15f, 0.03f + 0.0003f * i, rnd(&s), 1);
        }
        break;
    case FX_SHATTER:
        for (i = 0; i < 2500; i++) {
            float t = rnd(&s) * 0.5f + 0.5f;
            int n5[5]; chord_notes(bar, n5);
            grain(p + (int)(t * t * SR * 5), mtof((float)(62 + n5[i % 5] + 12 * (i % 4))), 0.03f + 0.05f * (rnd(&s) + 1),
                  0.012f, rnd(&s), i & 1);
        }
        for (i = 0; i < SR * 2; i++) {
            float t = (float)i / (SR * 2);
            bed_add(p - SR * 2 + i, svf(&f, rnd(&s), fcut(500 + 8000 * t), 1, 1) * t * t * t * 0.3f, 0);
        }
        break;
    case FX_THEME:
        grain(p, mtof(57), 1.2f, 0.03f, -0.3f, 0); grain(p + SR, mtof(58), 0.8f, 0.03f, 0, 0); grain(p + (int)(SR * 1.7f), mtof(57), 1.8f, 0.03f, 0.3f, 0);
        break;
    }
}
static void th_bed(void *u)
{
    int bar, i, k; unsigned s = 77;
    SVF pf[2] = { { 0 } };
    float ph[20] = { 0 }, dph = 0, dph2 = 0, bph = 0;
    (void)u;
    /* pads: continuous per-sample rendering with voice-leading between bars */
    for (bar = 0; bar < NBARS; bar++) {
        int n[5], pl = A_PAD(bar), plp = A_PAD(bar - 1), dr = A_DRONE(bar), br = A_BRTH(bar), gr = A_GRAN(bar);
        chord_notes(bar, n);
        for (i = 0; i < SPB; i++) {
            int p = bar * SPB + i;
            float t = (float)i / SPB, lvl = plp + (pl - plp) * (t < 0.25f ? t * 4 : 1), L = 0, R = 0;
            if (lvl > 0.01f) {
                float cut = 500 + 900 * lvl + 600 * sinf(p * 0.0000071f) + (A_DARK(bar) ? -300 : 0);
                for (k = 0; k < 10; k++) {
                    int ti = k % 5; float f = mtof((float)(48 + n[ti] + (ti >= 3 ? -12 : 0) + (k >= 5 && pl == 3 ? 12 : 0))) * (1 + (k - 4.5f) * 0.0023f);
                    float x;
                    ph[k] += f / SR; if (ph[k] >= 1) ph[k] -= 1;
                    x = saw(ph[k], f / SR);
                    if (k & 1) R += x; else L += x;
                }
                L = svf(&pf[0], L, fcut(cut), 0.9f, 0) * 0.030f * lvl;
                R = svf(&pf[1], R, fcut(cut), 0.9f, 0) * 0.030f * lvl;
            }
            if (dr) {
                float x; dph += 36.71f / SR; dph2 += 55.0f / SR;
                x = sinf(2 * PI * dph) * 0.10f + sinf(2 * PI * dph2) * 0.05f * (0.5f + 0.5f * sinf(p * 0.00003f));
                L += x; R += x;
            }
            if (br) {  /* machines breathing: slow inhale/exhale of filtered air */
                float cyc = 0.5f - 0.5f * cosf(2 * PI * p / (SR * 4.8f)), x;
                x = svf(&pf[0], rnd(&s), fcut(250 + 700 * cyc), 2.5f, 1) * 0.09f * cyc;
                bph += (30 + 8 * cyc) / SR;
                L += x + sinf(2 * PI * bph) * 0.04f * cyc; R += x;
            }
            trk[T_BEDL][p] += L; trk[T_BEDR][p] += R;
        }
        /* granular particles: tuned grains, density follows the arrangement */
        if (gr) for (i = 0; i < gr * gr * 6 + 2; i++) {
            float r = (rnd(&s) + 1) * 0.5f;
            grain(bar * SPB + (int)(r * SPB), mtof((float)(74 + n[i % 5] + 12 * (i % 3 == 0))), 0.02f + 0.05f * (rnd(&s) + 1),
                  0.02f / (1 + gr * 0.4f), rnd(&s), gr == 3 && (i & 1));
        }
        synth_progress = 0.2f + 0.6f * bar / NBARS;
    }
    for (k = 0; k < NFX; k++) fx_event(fx_ev[k * 4], fx_ev[k * 4 + 1], fx_ev[k * 4 + 2], fx_ev[k * 4 + 3]);
    __sync_fetch_and_add(&synth_ndone, 1);
}

/* ------------------------------------------------------------------ voices */
static const VStyle vstyle[5] = {
    /* alpha tilt breath vibr vibd sub dual dualint gain */
    { 1.00f, 0.80f, 0.14f, 5.3f, 0.16f, 0.00f, 0.0f, 0, 0.55f },  /* A android: clear, airy */
    { 0.86f, 1.00f, 0.05f, 4.7f, 0.28f, 0.30f, 0.0f, 0, 0.60f },  /* B cyborg: lower tract, sub-harmonic depth */
    { 0.93f, 0.90f, 0.09f, 5.0f, 0.20f, 0.00f, 0.6f, -12, 0.58f }, /* C merged: one tract, two glottal sources */
    { 0.97f, 0.70f, 0.35f, 4.2f, 0.10f, 0.00f, 0.0f, 0, 0.14f },  /* ghost */
};
static void sing(int track, const short *w, int nw, const unsigned char *nts, int nn, int bar, const VStyle *st, unsigned short *mouth)
{
    VNote vn[64]; int i;
    for (i = 0; i < nn; i++) {
        vn[i].pitch = nts[i * 3] + TRANSP;
        vn[i].start = (bar * 16 + nts[i * 3 + 1]) * 0.15f;
        vn[i].dur = nts[i * 3 + 2] * 0.15f - 0.03f;
        vn[i].syl = i;
    }
    voice_line(trk[track], SONG_LEN, 0, w, nw, vn, nn, st, mouth);
}
static void th_voice(void *u)
{
    int l, wi = 0, ni = 0; (void)u;
    voice_init();
    for (l = 0; l < NVL; l++) {
        short w[32]; int i, v = vl_voice[l], nn = vl_nn[l];
        for (i = 0; i < vl_nw[l]; i++) w[i] = vl_words[wi + i];
        wi += vl_nw[l];
        if (v == 0) sing(T_VA, w, vl_nw[l], vl_notes + ni * 3, nn, vl_bar[l], &vstyle[0], mouth_a);
        if (v == 1) sing(T_VB, w, vl_nw[l], vl_notes + ni * 3, nn, vl_bar[l], &vstyle[1], mouth_b);
        if (v == 3) { sing(T_VA, w, vl_nw[l], vl_notes + ni * 3, nn, vl_bar[l], &vstyle[2], mouth_a); }
        if (v == 2) {
            sing(T_VA, w, vl_nw[l], vl_notes + ni * 3, nn, vl_bar[l], &vstyle[0], mouth_a);
            sing(T_VB, w, vl_nw[l], vl_notes + (ni + nn) * 3, nn, vl_bar[l], &vstyle[1], mouth_b);
            if (vl_bar[l] == 130 || vl_bar[l] == 147 || vl_bar[l] == 149) {   /* the phantom third voice */
                unsigned char g[96];
                for (i = 0; i < nn * 3; i++) g[i] = vl_notes[ni * 3 + i] + (i % 3 == 0 ? 7 : 0);
                sing(T_VB, w, vl_nw[l], g, nn, vl_bar[l], &vstyle[3], 0);
            }
            ni += nn;
        }
        ni += nn;
        synth_progress = 0.8f * l / NVL;
    }
    __sync_fetch_and_add(&synth_ndone, 1);
}

/* ------------------------------------------------------------------ mixdown */
#define RVN 8
#define VG 6.5f
static float *rvbuf;
static void th_mix(void)
{
    static const int RL[RVN] = { 1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116 };
    int i, k, ki = 0, rp = 0;
    float rs[RVN] = { 0 }, *rv[RVN], dl[2] = { 0 }, dlf[2] = { 0 }, lim = 1, env_v = 0, env_k = 0, bfl[4] = { 0 };
    SVF vf[7] = { { 0 } };
    float *dly = rvbuf + RVN * 8192; int dlen = SPT * 3;
    for (k = 0; k < RVN; k++) rv[k] = rvbuf + k * 8192;
    for (i = 0; i < SONG_LEN; i++) {
        int bar = i / SPB;
        float t = (float)i / SR, va = trk[T_VA][i], vb = trk[T_VB][i], L, R, sendR, sendD, sc, duck, dark = (float)A_DARK(bar), x;
        /* kick sidechain envelope */
        while (ki < nkick && kick_pos[ki] <= i) { env_k = 1; ki++; }
        env_k *= 0.99985f;
        sc = 1 - 0.45f * env_k * (A_DRUM(bar) ? 1 : 0);
        /* vocal presence -> the music makes room */
        x = (fabsf(va) + fabsf(vb)) * VG;
        env_v += (x > env_v ? 0.002f : 0.00004f) * (x - env_v);
        duck = 1 / (1 + 12 * env_v);
        /* the cyborg: far away (break), behind doors (search), then the room itself sings with her */
        if (bar >= 36 && bar < 54 && !(bar >= 46 && bar < 50)) vb = svf(&vf[0], svf(&vf[1], vb, fcut(2600), 1.2f, 0), fcut(350), 1.0f, 2) * 0.6f;
        if (bar >= 56 && bar < 70) vb = svf(&vf[0], vb, fcut(3200 + (bar == 69 ? 4000 * (t - 69 * 2.4f) : 0)), 0.9f, 0) * 0.85f;
        L = trk[T_DRUM][i] * 0.4f * (0.45f + 0.55f * duck) + trk[T_BASS][i] * 0.5f * sc * (0.6f + 0.4f * duck);
        R = L;
        {
            float cl = trk[T_CHIPL][i] * sc * duck, cr = trk[T_CHIPR][i] * sc * duck;
            float bl = trk[T_BEDL][i] * 0.6f * (0.55f + 0.45f * sc) * (0.45f + 0.55f * duck), br = trk[T_BEDR][i] * 0.6f * (0.55f + 0.45f * sc) * (0.45f + 0.55f * duck);
            float ml, mr, pres = 1 - duck;
            if (dark > 0) { bl = svf(&vf[2], bl, fcut(900), 0.9f, 0); br = svf(&vf[3], br, fcut(900), 0.9f, 0); }
            ml = cl + bl; mr = cr + br;
            /* dynamic EQ: carve the consonant band out of the music while someone sings */
            ml -= svf(&vf[4], ml, fcut(2600), 1.0f, 1) * 0.85f * pres;
            mr -= svf(&vf[5], mr, fcut(2600), 1.0f, 1) * 0.85f * pres;
            L += ml; R += mr;
            sendD = (cl + cr) * 0.35f + va * (bar == 30 ? 1.2f : 0.03f) + vb * 0.04f;
            sendR = (cl + cr) * 0.25f + (bl + br) * 0.3f + va * 0.12f * VG + vb * (bar >= 70 && bar < 104 ? 0.3f : 0.2f) * VG + trk[T_DRUM][i] * 0.05f;
        }
        {   /* vocal presence: +6 dB around 3 kHz for the consonants */
            float vv = va * 0.95f + vb * 0.9f;
            vv += svf(&vf[6], vv, fcut(3000), 1.4f, 1) * 1.0f;
            L += vv * VG; R += vv * VG;
        }
        /* ping-pong delay, dotted eighth */
        {
            int j = i % dlen; float a = dly[j * 2], b = dly[j * 2 + 1];
            dlf[0] += (a - dlf[0]) * 0.4f; dlf[1] += (b - dlf[1]) * 0.4f;
            dly[j * 2] = sendD + dlf[1] * 0.42f; dly[j * 2 + 1] = dlf[0] * 0.42f;
            L += dlf[0] * 0.5f; R += dlf[1] * 0.5f;
        }
        /* 8-line feedback delay network reverb */
        {
            float o[RVN], sum = 0, g = bar >= 70 ? 0.935f : 0.9f;
            for (k = 0; k < RVN; k++) { o[k] = rv[k][(rp + 8192 - RL[k] * (bar >= 70 ? 2 : 1)) & 8191]; sum += o[k]; }
            sum *= 2.0f / RVN;
            for (k = 0; k < RVN; k++) {
                rs[k] += (o[k] - sum - rs[k]) * 0.45f;          /* damping */
                rv[k][rp] = sendR * 0.35f + rs[k] * g;
            }
            rp = (rp + 1) & 8191;
            L += (o[0] + o[2] + o[4] + o[6]) * 0.22f; R += (o[1] + o[3] + o[5] + o[7]) * 0.22f;
        }
        (void)bfl; (void)dl;
        /* master: gentle limiter */
        {
            float pk = fabsf(L) > fabsf(R) ? fabsf(L) : fabsf(R);
            pk *= 1.1f;
            if (pk * lim > 0.95f) lim = 0.95f / pk; else lim += (1 - lim) * 0.00002f;
            L = sat(L * lim * 1.2f) * 1.1f; R = sat(R * lim * 1.2f) * 1.1f;
        }
        song16[i * 2] = (short)(L * 32000); song16[i * 2 + 1] = (short)(R * 32000);
    }
}

typedef void (*thfn)(void *);
static const thfn workers[5] = { th_voice, th_bed, th_chip, th_drums, th_bass };
#ifdef _WIN32
static DWORD WINAPI synth_main(LPVOID u)
{
    int i; (void)u;
    for (i = 0; i < 5; i++) CreateThread(0, 0, (LPTHREAD_START_ROUTINE)workers[i], 0, 0, 0);
    while (synth_ndone < 5) Sleep(10);
    th_mix();
    __sync_fetch_and_add(&synth_ndone, 1);
    return 0;
}
#endif
static void synth_alloc(void)
{
    int i;
    for (i = 0; i < NTRK; i++) trk[i] = ALLOC(SONG_LEN * 4);
    song16 = ALLOC(SONG_LEN * 4);
    rvbuf = ALLOC((RVN * 8192 + SPT * 3 * 2) * 4);
}
#ifdef _WIN32
static void synth_start(void) { synth_alloc(); CreateThread(0, 0, synth_main, 0, 0, 0); }
static int synth_done(void) { return synth_ndone > 5; }
#else
static void *pth(void *f) { ((thfn)f)(0); return 0; }
static void synth_render(void)
{
    pthread_t th[5]; int i;
    synth_alloc();
    for (i = 0; i < 5; i++) pthread_create(&th[i], 0, pth, (void *)workers[i]);
    for (i = 0; i < 5; i++) pthread_join(th[i], 0);
    th_mix();
}
/* preview: only the lipsync / timing side of the synth is needed */
static void synth_timeline_only(void)
{
    static float dummy_line[1];
    (void)dummy_line;
}
#endif
