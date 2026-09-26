/* LINKED//SPIRITS - director: time -> shot, camera, poses, animation parameters.
 *
 * Uniform layout (vec4 U[64]):
 *  0 t, shot time, -, -          1 cam pos, fov            2 cam target, roll
 *  3 focus dist, aperture, exposure, grade
 *  4 head pos                     5 head yaw, pitch, roll    6 gaze x, gaze y, pupil, blink
 *  7 mouth open, mouth round, iris mode (0 green .. 1 blue/red), eye glow
 *  8 cyborg head pos, reveal      9 cyborg yaw, pitch, blink L, blink R
 * 10 cyborg gaze x, y, pupil, mouth open        11 cyborg mouth round, -, -, -
 * 12 lights, systems, scan line, door open      13 wave origin, radius
 * 14 dissolve, translucency, -, -               15..19 scene specific
 * 20..39 android joints          40..59 second body joints (astral)
 * 60 left palm normal, curl      61 right palm normal, curl   62 -   63 -, -, -, particle count
 */
#define TEXT_W 2048
#define TEXT_H 256

typedef struct { float x, y, z; } V3;
static V3 v3(float x, float y, float z) { V3 r = { x, y, z }; return r; }
static V3 va(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static V3 vs(V3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static V3 vm(V3 a, V3 b, float s) { return va(a, vs(b, s)); }   /* a + b*s */
static float vd(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static V3 vn(V3 a) { return vs(a, 1.0f / sqrtf(vd(a, a) + 1e-12f)); }
static V3 vc(V3 a, V3 b) { return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static float mixf(float a, float b, float t) { return a + (b - a) * t; }
static float clampf(float x, float a, float b) { return x < a ? a : x > b ? b : x; }
static float sstep(float a, float b, float x) { float t = clampf((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); }

typedef struct {
    V3 root;            /* pelvis position */
    float yaw, pitch;   /* body orientation; pitch rotates the whole body about its side axis (lying down) */
    float lean;         /* spine forward lean */
    float hf[2], ha[2], kf[2], af[2];   /* legs: hip flex, hip abduction, knee flex, ankle (0 = left) */
    float sf[2], sa[2], ef[2];          /* arms: shoulder flex, abduction, elbow flex */
    float hy, hp;       /* head yaw / pitch relative to the body */
} Pose;

static void set_joint(float *u, int base, int i, V3 p) { u[(base + i) * 4] = p.x; u[(base + i) * 4 + 1] = p.y; u[(base + i) * 4 + 2] = p.z; }

/* forward kinematics -> 20 joints at U[base..base+19]; returns head yaw/pitch in world */
static void fk(float *u, int base, const Pose *ps, float *hyaw, float *hpitch)
{
    V3 J[20];
    float cy = cosf(ps->yaw), sy = sinf(ps->yaw);
    V3 fwd = v3(sy, 0, cy), side = v3(cy, 0, -sy), up = v3(0, 1, 0);   /* side = body's left -> +x when yaw 0 */
    V3 ups = vn(va(vs(up, cosf(ps->lean)), vs(fwd, sinf(ps->lean))));
    int s, i;
    J[0] = ps->root;
    J[1] = vm(J[0], ups, 0.33f);
    J[2] = vm(J[0], ups, 0.47f);
    J[3] = vm(vm(J[0], ups, 0.605f), fwd, 0.025f);
    for (s = 0; s < 2; s++) {
        float sg = s ? -1.0f : 1.0f;
        /* arm */
        V3 S = vm(vm(J[0], ups, 0.435f), side, 0.175f * sg);
        V3 d = vn(va(va(vs(ups, -cosf(ps->sf[s]) * cosf(ps->sa[s])), vs(fwd, sinf(ps->sf[s]) * cosf(ps->sa[s]))), vs(side, sg * sinf(ps->sa[s]))));
        V3 E = vm(S, d, 0.28f);
        V3 pr = vn(vm(fwd, d, -vd(fwd, d)));
        V3 f = vn(va(vs(d, cosf(ps->ef[s])), vs(pr, sinf(ps->ef[s]))));
        V3 W = vm(E, f, 0.245f);
        J[4 + s * 4] = S; J[5 + s * 4] = E; J[6 + s * 4] = W; J[7 + s * 4] = vm(W, f, 0.17f);
        /* leg */
        {
            V3 H = vm(vm(J[0], side, 0.088f * sg), up, -0.03f);
            float a = ps->hf[s], k = a - ps->kf[s];
            V3 td = vn(va(va(vs(up, -cosf(a)), vs(fwd, sinf(a))), vs(side, sg * ps->ha[s])));
            V3 K = vm(H, td, 0.41f);
            V3 sd = vn(va(vs(up, -cosf(k)), vs(fwd, sinf(k))));
            V3 A = vm(K, sd, 0.39f);
            V3 T = vm(vm(A, fwd, 0.16f * cosf(ps->af[s])), up, -0.055f + 0.16f * sinf(ps->af[s]));
            J[12 + s * 4] = H; J[13 + s * 4] = K; J[14 + s * 4] = A; J[15 + s * 4] = T;
        }
    }
    /* whole-body pitch about the pelvis side axis (lying on the pod) */
    if (ps->pitch != 0) {
        float c = cosf(ps->pitch), sn = sinf(ps->pitch);
        for (i = 0; i < 20; i++) {
            V3 r = va(J[i], vs(J[0], -1));
            float a = vd(r, up), b = vd(r, fwd), sd = vd(r, side);
            J[i] = va(J[0], va(va(vs(side, sd), vs(up, a * c - b * sn)), vs(fwd, a * sn + b * c)));
        }
    }
    for (i = 0; i < 20; i++) set_joint(u, base, i, J[i]);
    *hyaw = ps->yaw + ps->hy;
    *hpitch = ps->hp - ps->lean - ps->pitch;
}

/* standing pose with a walk cycle (phase in cycles, amt 0..1) */
static void walk_pose(Pose *ps, float ph, float amt)
{
    int s;
    for (s = 0; s < 2; s++) {
        float p = (ph + s * 0.5f) * 6.2831853f;
        ps->hf[s] = amt * 0.42f * sinf(p);
        ps->kf[s] = amt * (0.55f * sstep(-0.3f, 1.0f, -cosf(p) * 0.8f + sinf(p) * 0.4f)) + 0.04f;
        ps->af[s] = amt * 0.25f * sinf(p - 0.9f);
        ps->ha[s] = 0.02f;
        ps->sf[s] = -amt * 0.3f * sinf(p) + 0.05f;
        ps->sa[s] = 0.08f;
        ps->ef[s] = 0.2f + amt * 0.15f * (1 + sinf(p));
    }
    ps->root.y = 0.93f + amt * 0.02f * cosf(ph * 12.566f);
    ps->lean = 0.03f * amt;
}

/* ---------------------------------------------------------------- helpers */
static float *U_;
static void setv(int i, float x, float y, float z, float w) { U_[i * 4] = x; U_[i * 4 + 1] = y; U_[i * 4 + 2] = z; U_[i * 4 + 3] = w; }
static void cam(V3 p, V3 t, float fov) { setv(1, p.x, p.y, p.z, fov); setv(2, t.x, t.y, t.z, 0); }
static V3 vlerp(V3 a, V3 b, float t) { return va(a, vs(va(b, vs(a, -1)), t)); }
static float ease(float x) { x = clampf(x, 0, 1); return x * x * (3 - 2 * x); }
static float hsh(float n) { float x = sinf(n * 12.9898f) * 43758.5453f; return x - floorf(x); }
/* smooth random walk used for micro saccades and drifts */
static float wobble(float t, float seed) { float i = floorf(t), f = t - i; f = f * f * (3 - 2 * f); return mixf(hsh(i + seed), hsh(i + 1 + seed), f) * 2 - 1; }
/* head frame of the android: world position of a point given in head space */
static V3 hdP, hdF, hdR, hdU;
static void head_frame(float yaw, float pitch)
{
    hdF = v3(sinf(yaw) * cosf(pitch), sinf(pitch), cosf(yaw) * cosf(pitch));
    hdR = vn(vc(v3(0, 1, 0), hdF));           /* head +x (her left) */
    hdU = vc(hdF, hdR);
}
static V3 hpt(float x, float y, float z) { return va(va(va(hdP, vs(hdR, x)), vs(hdU, y)), vs(hdF, z)); }


static void mouth(float t, unsigned short *m, int slot)
{
    float f = t * 100, op = 0, rd = 0; int i = (int)f, k;
    if (!m) return;
    for (k = -2; k <= 2; k++) {                /* small smoothing kernel */
        int j = i + k; float w = 3 - (k < 0 ? -k : k);
        if (j >= 0 && j < 48000) { op += (m[j] & 255) * w; rd += (m[j] >> 8) * w; }
    }
    U_[slot] = op / (255 * 9.0f); U_[slot + 1] = rd / (255 * 9.0f);
}

/* ---------------------------------------------------------------- walking */
static const float PATH[6][2] = { { 0, -1.0f }, { 0, 1.2f }, { -2.3f, 2.4f }, { -2.5f, 4.4f }, { 0, 6.4f }, { 0, 56.6f } };
static V3 path_pt(float s)
{
    int i;
    for (i = 0; i < 5; i++) {
        float dx = PATH[i + 1][0] - PATH[i][0], dz = PATH[i + 1][1] - PATH[i][1], L = sqrtf(dx * dx + dz * dz);
        if (s <= L || i == 4) { float k = clampf(s / L, 0, 1); return v3(PATH[i][0] + dx * k, 0, PATH[i][1] + dz * k); }
        s -= L;
    }
    return v3(0, 0, 0);
}
static float walk_s(float t)
{
    if (t < 39.0f) return 0;
    if (t < 86.4f) return (t - 39.0f) * 0.5f;
    if (t < 129.6f) return 23.7f;
    if (t < 166.0f) return 23.7f + (t - 129.6f) * 1.0f;
    return 60.1f;
}
/* places the android on the path at time t; returns walking speed */
static float walk_at(Pose *ps, float t, float *yaw)
{
    float s = walk_s(t), s2 = walk_s(t + 0.25f), sp = (s2 - s) * 4;
    V3 p = path_pt(s), a = path_pt(s - 0.5f), b = path_pt(s + 0.5f);
    float amt = clampf(sp / 0.5f, 0, 1);
    walk_pose(ps, s / (sp > 0.8f ? 1.35f : 0.95f), amt * (sp > 0.8f ? 1 : 0.75f));
    ps->root.x = p.x; ps->root.z = p.z;
    *yaw = atan2f(b.x - a.x, b.z - a.z + 1e-4f);
    ps->yaw = *yaw;
    return sp;
}
/* two-bone arm IK: side s (0 left, 1 right) reaches the wrist to T; pole = elbow direction */
static void arm_ik(float *u, int base, int s, V3 T, V3 pole, V3 handDir)
{
    V3 S = v3(u[(base + 4 + s * 4) * 4], u[(base + 4 + s * 4) * 4 + 1], u[(base + 4 + s * 4) * 4 + 2]);
    V3 d = va(T, vs(S, -1)), dn = vn(d), pp, E, W;
    float D = clampf(sqrtf(vd(d, d)), 0.05f, 0.52f), a = (0.28f * 0.28f + D * D - 0.245f * 0.245f) / (2 * D), h = sqrtf(clampf(0.28f * 0.28f - a * a, 0, 1));
    pp = vn(va(pole, vs(dn, -vd(pole, dn))));
    E = va(va(S, vs(dn, a)), vs(pp, h));
    W = va(S, vs(dn, D));
    set_joint(u, base, 5 + s * 4, E); set_joint(u, base, 6 + s * 4, W); set_joint(u, base, 7 + s * 4, va(W, vs(vn(handDir), 0.17f)));
}
static V3 fwdv(float yaw) { return v3(sinf(yaw), 0, cosf(yaw)); }
static V3 sidev(float yaw) { return v3(cosf(yaw), 0, -sinf(yaw)); }

/* ---------------------------------------------------------------- the film */
#define B(x) ((x) * 2.4f)
static int direct(float t, float *u)
{
    Pose ps;
    float hy, hp, b = t / 2.4f;
    int prog = PR_LAB, i;
    U_ = u;
    for (i = 0; i < NU * 4; i++) u[i] = 0;
    u[0] = t;
    memset(&ps, 0, sizeof(ps));
    ps.root = v3(0, 0.93f, -1.2f);
    walk_pose(&ps, 0, 0);
    setv(60, -1, 0, 0, 0.3f); setv(61, 1, 0, 0, 0.3f);   /* palms face the thighs, fingers relaxed */
    setv(3, 1, 0, 0, 0);                        /* focus, aperture, exposure, grade */
    setv(19, 0, 0.93f, -1.2f, 0);               /* cradle hinge */
    setv(12, 1, 4, -10, 0);                     /* lights, systems, scan, door */
    setv(6, 0, 0, 0.3f, 0);                     /* gaze, pupil, blink */
    /* natural blinks */
    { float bp = fmodf(t, 4.3f); u[6 * 4 + 3] = bp < 0.16f ? sinf(bp / 0.16f * 3.14159f) : 0; }
    /* micro saccades */
    u[6 * 4] = 0.02f * wobble(t * 1.7f, 3) + 0.01f * wobble(t * 7, 9);
    u[6 * 4 + 1] = 0.015f * wobble(t * 1.3f, 5);
    if (b < 16) {
        /* ---- opening: the dark, the eye, the room, she rises */
        float podA = 1.1f;                      /* reclined 63 degrees */
        if (b > 14) podA = mixf(1.1f, 0.12f, ease((b - 14) / 1.6f));
        ps.pitch = -podA;
        u[15 * 4] = podA;
        ps.sf[0] = ps.sf[1] = 0.05f; ps.ef[0] = ps.ef[1] = 0.1f;
        fk(u, 20, &ps, &hy, &hp);
        hdP = v3(u[23 * 4], u[23 * 4 + 1], u[23 * 4 + 2]);
        head_frame(hy, hp);
        u[12 * 4] = b < 2 ? 0 : b < 8 ? 0.06f + 0.05f * (b > 3.5f) * (0.6f + 0.4f * sinf(t * 37) * sinf(t * 11)) : mixf(0.12f, 1, ease((b - 8) / 3));
        u[12 * 4 + 1] = b < 2 ? (b > 0.25f) + (b > 0.62f) + (b > 1.12f) + (b > 1.56f) : 4;
        u[6 * 4 + 3] = b < 6 ? 1 : b < 6.6f ? 1 - ease((b - 6) / 0.6f) : u[6 * 4 + 3];
        u[6 * 4 + 2] = b < 6.3f ? 0.55f : mixf(0.55f, 0.27f, ease((b - 6.3f) / 0.8f)) + 0.05f * (1 - ease((b - 8) / 3));
        if (b < 2) {                            /* green lights in the dark */
            cam(v3(0.3f, 1.3f, 3.5f), v3(1.6f, 1.35f, -1.7f), 30);
            setv(3, 0.8f, 0.03f, 0, 0);
            setv(63, 2, 0, 1, 4);
        } else if (b < 8) {                     /* extreme close-up: the closed eye, then it opens */
            float k = (b - 2) / 6;
            V3 e = hpt(-0.033f, 0.002f, 0.064f);
            V3 c = va(e, va(vs(hdF, mixf(0.085f, 0.055f, k)), va(vs(hdU, 0.006f), vs(hdR, -0.008f))));
            cam(c, e, 24);
            setv(3, mixf(0.085f, 0.055f, k), 0.05f, 0, 0);
            u[12 * 4 + 2] = b > 4 && b < 6 ? (b - 4) / 2 : -10;   /* the scan line sweeps across the face */
        } else {                                /* the long pull-back */
            float k = ease((b - 8) / 6);
            V3 e = hpt(-0.02f, 0.0f, 0.06f);
            V3 c0 = va(e, vs(hdF, 0.07f)), c1 = v3(2.6f, 2.2f, 4.2f);
            V3 c = vlerp(c0, c1, k * k), tg = vlerp(e, v3(0, 1.1f, -1.2f), k);
            cam(c, tg, mixf(24, 40, k));
            setv(16, 0, 1.6f, -1, 3); setv(63, 1, 0, 0.6f * k, 6000);
            setv(3, sqrtf(vd(va(c, vs(tg, -1)), va(c, vs(tg, -1)))), 0.05f * (1 - k) + 0.004f, 0, 0);
        }
    } else if (b < 70) {
        /* ---- she walks: the room, the glass, the corridor; the distant voice; the search */
        float yaw, sp = walk_at(&ps, t, &yaw), strange = ease((t - 132) / 28);
        V3 F = fwdv(yaw), S = sidev(yaw), H;
        u[15 * 4] = 0.12f;                       /* the pod stays upright */
        u[15 * 4 + 1] = strange;
        ps.hy = 0.15f * wobble(t * 0.4f, 7);
        if (t > 86.4f && t < 129.6f) {           /* she stops, turns to the wall, listens */
            float k = ease((t - 87) / 2);
            ps.yaw = yaw - 1.25f * k;
            ps.hy = k * (t < 109.2f ? 0.1f * wobble(t * 0.3f, 2) : 1.1f * ease((t - 109.2f) / 0.35f));
            ps.hp = -0.05f;
        }
        fk(u, 20, &ps, &hy, &hp);
        hdP = v3(u[23 * 4], u[23 * 4 + 1], u[23 * 4 + 2]);
        head_frame(hy, hp);
        H = hdP;
        setv(16, H.x, 1.6f, H.z + 1.5f, 3.5f);
        setv(17, clampf((t - 60) / 40, 0, 1), 0, 0, 0);
        setv(63, 1, 0, 0.5f + 0.5f * clampf((t - 100) / 60, 0, 1), 4000 + 20000 * clampf((t - 60) / 90, 0, 1));
        u[6 * 4 + 2] = 0.28f + 0.04f * wobble(t * 0.5f, 11);
        if (t > 109.2f && t < 110.5f) u[6 * 4 + 2] = 0.2f;          /* the pupil contracts */
        if (b < 18.3f) {                         /* low angle, she leaves the cradle; the music starts */
            cam(v3(0.9f, 0.3f, 3.2f), va(H, v3(0, -0.35f, 0)), 42);
            setv(3, 3.5f, 0.006f, 0, 0);
        } else if (b < 20) {                     /* tracking beside her */
            cam(va(va(H, vs(S, 1.8f)), v3(0, -0.2f, 0.3f)), va(H, v3(0, -0.2f, 0)), 34);
            setv(3, 1.8f, 0.01f, 0, 0);
        } else if (b < 22) {                     /* White room, no dawn: her face, the camera retreats */
            cam(va(va(H, vs(F, 0.62f)), vs(S, 0.12f)), va(H, v3(0, -0.02f, 0)), 26);
            setv(3, 0.63f, 0.025f, 0, 0);
        } else if (b < 24) {                     /* I wake beneath a borrowed sun: high angle under the halo */
            cam(v3(2.8f, 3.6f, 3.6f), va(H, v3(0, -0.6f, 0)), 48);
            setv(3, 4.5f, 0.004f, 0, 0);
        } else if (b < 28) {                     /* Green light through glass: from among the racks */
            cam(v3(-3.55f, 1.5f, H.z - 2.6f), va(H, v3(0, -0.1f, 0)), 30);
            setv(3, 1.8f, 0.03f, 0, 0);
        } else if (b < 30) {                     /* Footsteps return: low, her steps towards us */
            cam(va(va(v3(H.x, 0.25f, H.z), vs(F, 1.8f)), vs(S, 0.3f)), va(H, v3(0, -1.0f, 0)), 36);
            setv(3, 1.9f, 0.02f, 0, 0);
        } else if (b < 32) {                     /* but every echo sounds like mine: symmetric corridor */
            cam(v3(0, 1.45f, 16), va(H, v3(0, -0.15f, 0)), 40);
            setv(3, 16 - H.z, 0.008f, 0, 0);
        } else if (b < 34) {                     /* Who sings between: profile close-up */
            cam(va(va(H, vs(S, -0.55f)), vs(F, 0.25f)), H, 30);
            setv(3, 0.6f, 0.03f, 0, 0);
        } else if (b < 36) {                     /* the hidden currents in the line */
            cam(va(va(H, vs(F, 0.9f)), v3(0.3f, 0.1f, 0)), H, 32);
            setv(3, 0.95f, 0.02f, 0, 0);
        } else if (b < 38) {                     /* she stops: tiny in the long white corridor */
            cam(v3(0.2f, 1.2f, H.z + 9), va(H, v3(0, -0.3f, 0)), 36);
            setv(3, 9, 0.004f, 0, 0);
            u[12 * 4] = mixf(1, 0.8f, ease((t - 86.4f) / 4));
        } else if (b < 46) {                     /* the distant voice: a slow push towards her face */
            float k = ease((t - 91.2f) / 19);
            V3 fv = fwdv(ps.yaw), c = va(H, vs(va(vs(fv, 0.9f), va(vs(sidev(ps.yaw), -0.35f), v3(0, 0.02f, 0))), mixf(1.6f, 0.45f, k)));
            cam(c, H, mixf(34, 26, k));
            setv(3, mixf(1.6f, 0.45f, k), 0.025f, 0, 0);
            u[12 * 4] = 0.8f;
        } else if (b < 50) {                     /* I know that tone */
            cam(va(H, v3(-0.25f, 0.02f, 0.45f)), H, 27);
            setv(3, 0.52f, 0.03f, 0, 0);
            u[12 * 4] = 0.8f;
        } else if (b < 54) {                     /* You know the wave: towards the far end of the corridor */
            float k = (t - 120) / 9.6f;
            cam(v3(0.3f * sinf(k * 3), 1.6f, H.z + 1 + k * 14), v3(0, 1.4f, 60), 40);
            setv(3, 8, 0.004f, 0, 0);
            u[12 * 4] = 0.8f;
        } else if (b < 58) {                     /* the search: behind her, fast */
            cam(va(va(H, vs(F, -1.6f)), v3(0.25f, 0.1f, 0)), va(H, vs(F, 3)), 38);
            setv(3, 1.7f, 0.006f, 0, 0);
        } else if (b < 62) {                     /* overhead: the ribs flowing by */
            cam(va(H, v3(0.01f, 1.1f, -0.5f)), va(H, v3(0, -1.5f, 0.6f)), 62);
            setv(3, 2.6f, 0.004f, 0, 0);
        } else if (b < 66) {                     /* the corridor becomes vast and strange */
            cam(va(H, v3(1.2f, -1.2f, 3.5f)), va(H, v3(0, 1.5f, 0)), 55);
            setv(3, 3.8f, 0.004f, 0, 0);
        } else if (b < 68) {                     /* Why do I hear you in my thoughts? */
            cam(va(va(H, vs(F, 0.5f)), vs(S, -0.15f)), H, 27);
            setv(3, 0.52f, 0.03f, 0, 0);
        } else {                                  /* the great doors */
            cam(v3(0, 1.3f, 44), v3(0, 2.2f, 58), 42);
            setv(3, 12, 0.004f, 0, 0);
        }
    } else if (b < 72) {
        /* ---- she pushes the doors; light floods in */
        float k = ease((t - 169) / 2.5f);
        ps.root = v3(0, 0.93f, 56.8f + 0.4f * k);
        ps.sf[0] = ps.sf[1] = 1.35f; ps.ef[0] = ps.ef[1] = 0.25f; ps.sa[0] = ps.sa[1] = 0.25f;
        ps.lean = 0.12f;
        u[15 * 4 + 1] = 1;
        setv(60, 0, 0, 1, 0.1f); setv(61, 0, 0, 1, 0.1f);
        fk(u, 20, &ps, &hy, &hp);
        hdP = v3(u[23 * 4], u[23 * 4 + 1], u[23 * 4 + 2]);
        head_frame(hy, hp);
        u[12 * 4 + 3] = k;
        cam(v3(0.6f, 1.25f, 54.2f), v3(0, 2.4f, 58), 46);
        setv(3, 3.5f, 0.004f, 1.5f * ease((t - 170.5f) / 2.3f), 0);
        setv(16, 0, 2, 56, 3); setv(63, 1, 0, 1, 20000);
    } else if (b < 104) {
        /* ---- the cathedral: revelation, dialogue, contact */
        float z = clampf(2 + (t - 174) * 1.0f, 2, 38.6f), sp = t > 174 && t < 210.6f ? 1 : 0, cy;
        V3 C = v3(0, 2.1f, 40.02f), H;
        prog = PR_CATH;
        setv(3, 5, 0.004f, 0, 1);
        u[12 * 4] = 0.5f + 0.6f * ease((t - 180) / 8);
        u[12 * 4 + 1] = 1;
        walk_pose(&ps, z / 1.3f, sp);
        if (t > 228) z = mixf(38.6f, 39.35f, ease((t - 228) / 3));           /* one step closer */
        ps.root = v3(0, ps.root.y, z);
        ps.hp = t > 205 ? 0.2f + 0.05f * wobble(t * 0.5f, 4) : 0.1f;           /* she looks up at her */
        ps.hy = 0.1f * wobble(t * 0.3f, 8);
        fk(u, 20, &ps, &hy, &hp);
        if (t > 230.4f) {                        /* the hand rises and touches a cable below her face */
            float k = ease((t - 230.4f) / 4.2f);
            V3 rest = v3(u[30 * 4], u[30 * 4 + 1], u[30 * 4 + 2]), T = v3(-0.24f, 1.47f, 39.83f);
            arm_ik(u, 20, 1, vlerp(rest, T, k), v3(-1, -0.6f, -0.2f), v3(0.05f, mixf(-1, 0.55f, k), mixf(0, 1, k)));
            setv(61, 0, mixf(0, -0.3f, k), mixf(0, 1, k), 0.15f);
        }
        hdP = v3(u[23 * 4], u[23 * 4 + 1], u[23 * 4 + 2]);
        head_frame(hy, hp);
        H = hdP;
        /* the cyborg */
        cy = 3.14159f + 0.08f * wobble(t * 0.25f, 21);
        setv(8, C.x, C.y, C.z, ease((t - 176) / 10));
        setv(9, cy, -0.12f + 0.04f * wobble(t * 0.3f, 23), 1 - ease((t - 184.8f) / 0.25f), 1 - ease((t - 183.6f) / 0.35f));
        setv(10, 0.03f * wobble(t * 1.5f, 31), 0.02f * wobble(t * 1.1f, 33) - 0.12f, t < 186 ? mixf(0.6f, 0.3f, ease((t - 184) / 1.5f)) : 0.3f + 0.03f * wobble(t, 35), 0);
        mouth(t, mouth_b, 10 * 4 + 3);
        { float mr = u[10 * 4 + 3 + 1]; setv(11, mr, 0, 0, 0); }
        setv(16, 0, 3, z + 3, 6); setv(63, 1, 0, 0.6f, 30000);
        if (b < 74) {                            /* inside the nave, looking back: she enters */
            cam(v3(2.5f, 1.1f, 16), v3(0, 3.5f, 0), 44);
            setv(3, 14, 0.003f, 0.8f * (1 - ease((t - 172.8f) / 3)), 1);
        } else if (b < 76) {                     /* rising along ribs and cables towards the far wall */
            float k = ease((t - 177.6f) / 4.8f);
            cam(v3(-5 + 2 * k, 2 + 9 * k, 18 + 6 * k), vlerp(v3(-8, 14, 26), v3(0, 2.5f, 40), k), 55);
            setv(3, 16, 0.003f, 0, 1);
        } else if (b < 78) {                     /* extreme close-up: blue, then red */
            V3 e = v3(0.0f, 2.1f, 40.02f - 0.065f);
            cam(va(e, v3(0.02f, 0.012f, -0.2f + (t - 182.4f) * 0.012f)), e, 30);
            setv(3, 0.2f - (t - 182.4f) * 0.012f, 0.03f, 0, 1);
            u[8 * 4 + 3] = 1;
        } else if (b < 80) {                     /* They sealed my voice inside the wire */
            cam(v3(0.12f, 1.95f, 39.35f), v3(0, 2.1f, 40), 30);
            setv(3, 0.68f, 0.02f, 0, 1);
        } else if (b < 82) {                     /* scale: she walks towards the halo */
            cam(va(H, v3(0.6f, -0.2f, -2.2f)), v3(0, 2.3f, 40), 40);
            setv(3, 40 - H.z, 0.004f, 0, 1);
        } else if (b < 84) {                     /* They buried what I came to know */
            cam(v3(-0.3f, 2.0f, 39.4f), v3(0, 2.08f, 40), 28);
            setv(3, 0.66f, 0.02f, 0, 1);
        } else if (b < 86) {                     /* two-shot from the side */
            cam(v3(3.2f, 1.7f, 37.2f), v3(0, 1.9f, 39.3f), 36);
            setv(3, 3.8f, 0.006f, 0, 1);
        } else if (b < 88) {                     /* What did you find beyond the noise? */
            cam(va(H, v3(0.22f, 0.05f, 0.42f)), H, 27);
            setv(3, 0.48f, 0.025f, 0, 1);
        } else if (b < 90) {                     /* A truth they could not let me name */
            cam(v3(0.15f, 1.9f, 39.45f), v3(0, 2.1f, 40), 26);
            setv(3, 0.62f, 0.025f, 0, 1);
        } else if (b < 91) {                     /* the pause: two faces in profile */
            cam(v3(1.25f, 1.8f, 39.05f), v3(0, 1.83f, 39.35f), 38);
            setv(3, 1.4f, 0.01f, 0, 1);
        } else if (b < 93) {                     /* And if I reach across to you? */
            cam(va(H, v3(-0.12f, 0.04f, 0.4f)), H, 26);
            setv(3, 0.42f, 0.03f, 0, 1);
            u[6 * 4 + 2] = 0.42f;                /* fear, fascination: the pupils open */
        } else if (b < 96) {                     /* We may not leave this place the same */
            float k = ease((t - 223.2f) / 7);
            cam(vlerp(v3(0.1f, 2.0f, 39.4f), v3(1.3f, 2.0f, 38.1f), k), v3(0, 2.0f, 40), 30);
            setv(3, mixf(0.6f, 2.5f, k), 0.015f, 0, 1);
        } else if (b < 98) {                     /* the hand */
            V3 w = v3(u[30 * 4], u[30 * 4 + 1], u[30 * 4 + 2]);
            cam(va(w, v3(-0.42f, 0.1f, -0.25f)), va(w, v3(0.05f, 0, 0.12f)), 32);
            setv(3, 0.42f, 0.03f, 0, 1);
        } else {                                  /* the wave, then the world comes apart */
            float k = (t - 235.2f) / 14.4f;
            cam(v3(-1.6f + 1.2f * k, 1.6f + 0.8f * k, 37.4f - 2 * k), v3(-0.1f, 1.8f, 39.6f), 40);
            setv(3, 2.5f, 0.008f, 0, 1);
        }
        /* the contact wave and the dissolution */
        setv(13, -0.24f, 1.47f, 39.83f, t < 235.2f ? -1 : (t - 235.2f) * (t - 235.2f) * 0.6f);
        setv(14, ease((t - 243) / 6), 0, 0, 0);
    } else {
        cam(v3(0, 1.5f, 57.4f), v3(0, 1.5f, 60), 40);
        fk(u, 20, &ps, &hy, &hp);
        hdP = v3(u[23 * 4], u[23 * 4 + 1], u[23 * 4 + 2]);
        head_frame(hy, hp);
    }
    /* head uniforms from the skeleton */
    setv(4, hdP.x, hdP.y, hdP.z, 0);
    setv(5, hy, hp, 0, 0);
    mouth(t, mouth_a, 7 * 4);
    return prog;
}
