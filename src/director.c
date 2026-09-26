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
    J[3] = vm(vm(J[0], ups, 0.59f), fwd, 0.02f);
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
    if (t < 72.0f) return (t - 39.0f) * 0.33f;
    if (t < 86.4f) return 10.9f + (t - 72.0f) * 0.85f;
    if (t < 129.6f) return 23.14f;
    if (t < 166.0f) return 23.14f + (t - 129.6f) * 1.02f;
    return 60.3f;
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
static V3 facedir(float yaw, float pitch) { return v3(sinf(yaw) * cosf(pitch), sinf(pitch), cosf(yaw) * cosf(pitch)); }
/* a camera in front of a face: dist along its gaze, side offset along its right, looking at it */
static void facecam(V3 h, float yaw, float pitch, float dist, float side, float fov)
{
    V3 f = facedir(yaw, pitch), r = vn(vc(v3(0, 1, 0), f));
    cam(va(va(va(h, vs(f, dist)), vs(r, side)), v3(0, 0.015f, 0)), h, fov);
    setv(3, dist, 0.025f, U_[3 * 4 + 2], U_[3 * 4 + 3]);
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
            ps.yaw = yaw - 0.55f * k;
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
        } else if (b < 26) {                     /* Green light through glass: from among the racks */
            cam(v3(-3.55f, 1.5f, H.z - 2.6f), va(H, v3(0, -0.1f, 0)), 30);
            setv(3, 1.8f, 0.03f, 0, 0);
        } else if (b < 28) {                     /* telling me I've just begun: towards the door */
            cam(va(va(H, vs(F, -2.2f)), v3(0.4f, 0.1f, 0)), va(H, vs(F, 2)), 36);
            setv(3, 2.3f, 0.008f, 0, 0);
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
            cam(v3(-0.95f, 1.55f, 38.9f), vlerp(w, v3(-0.24f, 1.5f, 39.8f), 0.5f), 34);
            setv(3, 0.42f, 0.03f, 0, 1);
        } else {                                  /* the wave, then the world comes apart */
            float k = (t - 235.2f) / 14.4f;
            cam(v3(-1.6f + 1.2f * k, 1.6f + 0.8f * k, 37.4f - 2 * k), v3(-0.1f, 1.8f, 39.6f), 40);
            setv(3, 2.5f, 0.008f, 0, 1);
        }
        /* the contact wave and the dissolution */
        setv(13, -0.24f, 1.47f, 39.83f, t < 235.2f ? -1 : (t - 235.2f) * (t - 235.2f) * 0.6f);
        setv(14, ease((t - 243) / 6), 0, 0, 0);
    } else if (b < 167) {
        /* ---- cyberspace: bodies of light, the journey, the duo, the dream */
        Pose pc;
        float cyaw, chy, chp, spd = 0, k;
        V3 A, Cc, O = v3(0, 0, 0);
        prog = PR_SPACE;
        memset(&pc, 0, sizeof(pc));
        walk_pose(&ps, 0, 0); walk_pose(&pc, 0, 0);
        /* floating: loose limbs, a slow bob */
        ps.root = v3(-0.25f, 0.93f + 0.05f * sinf(t * 0.7f), 0); ps.sa[0] = ps.sa[1] = 0.2f; ps.ef[0] = ps.ef[1] = 0.35f;
        ps.hf[0] = 0.15f; ps.kf[0] = 0.3f; ps.hf[1] = -0.05f; ps.kf[1] = 0.15f;
        pc.root = v3(0.3f, 0.86f + 0.05f * sinf(t * 0.6f + 1), -0.55f); pc.sa[0] = pc.sa[1] = 0.25f; pc.ef[0] = pc.ef[1] = 0.3f;
        pc.hf[1] = 0.12f; pc.kf[1] = 0.25f;
        cyaw = 0;
        if (b < 110) {                           /* the meeting */
            float turn = ease((t - 257.5f) / 2.2f);
            ps.yaw = 3.14159f * turn;
            ps.hy = t < 257.5f ? 1.1f * ease((t - 255.2f) / 0.8f) : 0;
            pc.hy = t > 259.5f ? -0.9f * ease((t - 259.5f) / 1.5f) : 0;
            if (t > 260.5f) ps.hy = 0.9f * ease((t - 260.5f) / 1.5f);
        } else {                                 /* the journey: side by side, facing the flight */
            ps.root = v3(-0.35f, 0.93f + 0.05f * sinf(t * 0.7f), 0); pc.root = v3(0.4f, 0.86f + 0.05f * sinf(t * 0.6f + 1), -0.35f);
            ps.lean = pc.lean = 0.25f;
            if (b >= 132 && b < 134) {           /* two separate signals intertwined: they circle each other */
                float a = (t - 316.8f) * 1.3f;
                ps.root = v3(0.5f * cosf(a), 0.93f + 0.3f * sinf(a * 0.5f), 0.5f * sinf(a)); pc.root = v3(-0.5f * cosf(a), 0.86f - 0.3f * sinf(a * 0.5f), -0.5f * sinf(a));
                ps.yaw = -a; cyaw = 3.14159f - a;
            }
            if (b >= 155) { ps.yaw = 1.2f; cyaw = -1.2f; ps.root = v3(-0.45f, 0.93f, 0); pc.root = v3(0.5f, 0.86f, 0.05f); ps.lean = pc.lean = 0; }
        }
        pc.yaw = cyaw;
        fk(u, 20, &ps, &hy, &hp);
        fk(u, 40, &pc, &chy, &chp);
        for (i = 0; i < 20; i++) {               /* the cyborg's body of light is taller; her face keeps a human size */
            float *j = u + (40 + i) * 4, *r = u + 40 * 4;
            j[0] = r[0] + (j[0] - r[0]) * 1.12f; j[1] = r[1] + (j[1] - r[1]) * 1.12f; j[2] = r[2] + (j[2] - r[2]) * 1.12f;
        }
        if (b < 110 && t > 253.5f && t < 257.5f) {   /* her hand on the android's shoulder */
            V3 sh = v3(u[24 * 4], u[24 * 4 + 1] + 0.03f, u[24 * 4 + 2]);
            arm_ik(u, 40, 0, vlerp(v3(u[46 * 4], u[46 * 4 + 1], u[46 * 4 + 2]), va(sh, v3(0.02f, 0.02f, -0.05f)), ease((t - 253.5f) / 1.5f)), v3(1, -0.3f, -0.5f), v3(0, -0.4f, 1));
            setv(60, 0, -1, 0, 0.4f);
        }
        u[59 * 4 + 3] = b < 110 ? ease((t - 251.5f) / 2.5f) : 1;
        hdP = v3(u[23 * 4], u[23 * 4 + 1], u[23 * 4 + 2]);
        head_frame(hy, hp);
        A = hdP; Cc = v3(u[43 * 4], u[43 * 4 + 1], u[43 * 4 + 2]);
        if (b < 110 && t < 252.5f) Cc = v3(0, -100, 0);   /* she has not appeared yet */
        setv(8, Cc.x, Cc.y, Cc.z, 1);
        setv(9, chy, chp, 0, 0);
        setv(10, 0.02f * wobble(t, 3), 0.02f * wobble(t * 0.8f, 4), 0.3f, 0);
        mouth(t, mouth_b, 10 * 4 + 3);
        setv(12, 1, 1, -10, 0);
        setv(18, 0.55f, 0.45f, 0.7f, 0);
        /* the worlds */
        if (b >= 114 && b < 118) { k = (t - 273.6f) / 9.6f; setv(16, -60 + 20 * k, -45, -120 + 170 * k, 40); setv(17, 0, 0, 0, 0); }
        if (b >= 118 && b < 122) { k = (t - 283.2f) / 9.6f; setv(16, 70 - 10 * k, -30, -160 + 150 * k, 45); setv(17, 1, 0, 0, 0); }
        if (b >= 122 && b < 128) { k = (t - 292.8f) / 14.4f; setv(16, 0, -60 + 58 * k, -300 + 60 * k, 70); setv(17, 3, 90, 190, 0.15f); }
        if (b >= 128 && b < 134) { k = (t - 307.2f) / 14.4f; setv(16, -40, -35, -200 + 180 * k, 60); setv(17, 2, 0, 0, 0); }
        if (b >= 143 && b < 155) { k = (t - 343.2f) / 28.8f; setv(16, 80, -20, -380 + 200 * k, 90); setv(17, 3, 120, 260, -0.35f); setv(15, -120, 40, -600, 60); u[14 * 4] = 1; }
        if (b >= 155) { setv(15, 0, -1000, 0, 900); u[14 * 4] = 2; setv(16, 500, 380, -1500, 150); setv(17, 3, 200, 520, 0.5f); setv(18, 0.35f, 0.25f, -1, 0.26f); }
        spd = b < 110 ? 0 : b < 114 ? ease((t - 264) / 6) : b < 142 ? 1 : b < 143 ? 0 : b < 155 ? 1.3f : 0;
        setv(19, spd, 0.8f, 0, b >= 150 && b < 155 ? sinf(clampf((t - 360) / 12, 0, 1) * 3.14159f) : 0);
        setv(63, b >= 122 && b < 126 && t > 297.6f ? 4 : 3, 0, 1.2f, b >= 122 && b < 126 && t > 297.6f ? 60000 : 90000);
        u[62 * 4 + 2] = spd > 0.3f ? 4000 * spd : 0;
        /* cameras */
        if (b < 106) {                           /* she appears, made of light */
            float a = (t - 249.6f) * 0.15f;
            cam(va(A, v3(1.2f * sinf(a), -0.1f, 1.2f * cosf(a))), va(A, v3(0, -0.3f, 0)), 36);
            setv(3, 1.2f, 0.01f, 1.5f * (1 - ease((t - 249.6f) / 2)), 2);
        } else if (b < 107.2f) {                 /* the hand on the shoulder */
            V3 sh = v3(u[24 * 4], u[24 * 4 + 1], u[24 * 4 + 2]);
            cam(va(sh, v3(0.75f, 0.25f, 0.35f)), va(sh, v3(-0.1f, 0.0f, -0.1f)), 34);
            setv(3, 0.7f, 0.02f, 0, 2);
        } else if (b < 108.4f) {                 /* she turns; they face each other */
            cam(v3(1.3f, 1.55f, -0.2f), v3(0, 1.5f, -0.25f), 34);
            setv(3, 1.3f, 0.015f, 0, 2);
        } else if (b < 110) {                    /* both look into the void */
            cam(v3(0.2f, 1.7f, 2.2f), v3(0, 1.3f, -10), 45);
            setv(3, 2.2f, 0.006f, 0, 2);
        } else if (b < 114) {                    /* ACCELERATION: in front of them, the universe streams past */
            k = ease((t - 264) / 9.6f);
            cam(va(v3(0, 1.45f, 0), v3(0.2f, 0.1f, -1.5f - 0.6f * k)), v3(0, 1.4f, 0), 40 + 8 * k);
            setv(3, 1.7f, 0.012f, 0, 2);
        } else if (b < 118) {                    /* the first world passes beneath them */
            cam(v3(2.5f, 3.0f, 4.5f), v3(-8, -8, -30), 55);
            setv(3, 5, 0.004f, 0, 2);
        } else if (b < 122) {                    /* faces, singing together */
            setv(3, 0, 0, 0, 2);
            if (b < 120) facecam(A, hy, hp, 0.62f, 0.2f, 28); else facecam(Cc, chy, chp, 0.62f, -0.2f, 28);
        } else if (b < 124) {                    /* the rings approach */
            cam(v3(-3, 2.2f, 3), v3(0, -4, -60), 60);
            setv(3, 4, 0.004f, 0, 2);
        } else if (b < 126) {                    /* inside the ring: an ocean of fragments */
            cam(v3(0.6f * sinf(t * 0.4f), 1.4f, 2.4f), v3(0, 1.4f, 0), 50);
            setv(3, 2.4f, 0.01f, 0, 2);
        } else if (b < 128) {                    /* the gas giant, from above the ring */
            cam(v3(1.5f, 2.6f, 3.2f), v3(0, -10, -80), 58);
            setv(3, 4, 0.004f, 0, 2);
        } else if (b < 132) {                    /* Your echo moves / inside my own */
            setv(3, 0, 0, 0, 2);
            if (b < 130) facecam(A, hy, hp, 0.58f, -0.18f, 28); else facecam(Cc, chy, chp, 0.58f, 0.18f, 28);
        } else if (b < 134) {                    /* intertwined */
            cam(v3(0, 3.2f, 0.3f), v3(0, 1.2f, 0), 50);
            setv(3, 2.3f, 0.006f, 0, 2);
        } else if (b < 136) {                    /* Am I becoming you? the iris becomes a planetary system */
            k = (t - 321.6f) / 4.8f;
            if (k < 0.35f) {                     /* push into her eye until the iris fills the frame */
                V3 e = hpt(-0.033f, 0.002f, 0.064f);
                float dz = mixf(0.09f, 0.03f, ease(k / 0.35f));
                cam(va(e, vs(hdF, dz)), e, 24);
                setv(3, dz - 0.01f, 0.03f, 0, 2);
                u[63 * 4 + 3] = 0;
            } else {
                float kk = ease((k - 0.35f) / 0.65f);
                float D = 1600 + 1500 * kk * kk, el = 0.75f * kk;   /* pull back and rise above the orbital plane */
                cam(v3(0, D * sinf(el), -2000 + D * cosf(el)), v3(0, 0, -2000), 40);
                setv(16, 0, 0, -2000, 800); setv(17, 0, 0, 0, 0);
                u[19 * 4 + 2] = 1 + kk;
                setv(3, 1000, 0, 0, 2);
                u[63 * 4 + 3] = 0;
            }
        } else if (b < 138) {                    /* No, but I can feel your rhythm changing */
            setv(3, 0, 0, 0, 2); facecam(Cc, chy, chp, 0.55f, 0.15f, 27);
        } else if (b < 140) {                    /* Are you becoming me? */
            setv(3, 0, 0, 0, 2); facecam(A, hy, hp, 0.55f, -0.15f, 27);
        } else if (b < 142) {                    /* every boundary is fading */
            cam(v3(0.1f, 1.5f, -2.2f), v3(0, 1.3f, 0), 38);
            setv(3, 2.2f, 0.01f, 0, 2);
        } else if (b < 143) {                    /* suspension */
            cam(v3(0.05f, 1.45f, -1.6f), v3(0, 1.45f, 0), 32);
            setv(3, 1.6f, 0.012f, 0, 2);
        } else if (b < 150) {                    /* the final quatrain: circling them, the worlds behind */
            float a = (t - 343.2f) * 0.18f;
            cam(v3(2.2f * sinf(a), 1.6f, -2.2f * cosf(a)), v3(0, 1.3f, 0), 42);
            setv(3, 2.3f, 0.008f, 0, 2);
        } else if (b < 155) {                    /* climax: the third silhouette */
            float a = (t - 360) * 0.1f;
            cam(v3(1.8f * sinf(a), 1.4f, -1.8f * cosf(a)), v3(0.05f, 1.2f, -0.15f), 40);
            setv(3, 1.9f, 0.01f, 0, 2);
        } else if (b < 163) {                    /* the dream: above the ocean, under the great star */
            setv(3, 5, 0.004f, 0, 2);
            if (b < 159) cam(v3(-2.5f, 2.2f, 3.5f), v3(0, 0.4f, -8), 58);
            else if (b < 160) facecam(Cc, chy, chp, 0.6f, 0.25f, 26);
            else facecam(A, hy, hp, 0.6f, -0.25f, 26);
            if (t > 383 && t < 391) u[6 * 4 + 3] = ease((t - 383) / 0.6f);   /* she closes her eyes */
            setv(63, 3, 0, 1, 70000);
        } else {                                  /* the music finishes the phrase; the view opens and fades */
            k = ease((t - 391.2f) / 9.6f);
            cam(v3(-2.5f - 10 * k, 2.2f + 6 * k, 3.5f + 12 * k), v3(0, 0.2f, -8), 58);
            setv(3, 8, 0.004f, 0, 2);
            u[62 * 4] = ease((t - 397) / 3.5f);
        }
    } else {
        /* ---- the return: the same room, the same framing; not quite the same person */
        float podA = 1.1f, bl = 1, gl;
        V3 e, eL, eR;
        ps.pitch = -podA;
        u[15 * 4] = podA;
        ps.sf[0] = ps.sf[1] = 0.05f; ps.ef[0] = ps.ef[1] = 0.1f;
        ps.hy = t > 405 && t < 413 ? 0.25f * sinf((t - 405) * 0.9f) : 0;
        fk(u, 20, &ps, &hy, &hp);
        hdP = v3(u[23 * 4], u[23 * 4 + 1], u[23 * 4 + 2]);
        head_frame(hy, hp);
        /* three awakenings */
        if (t > 404.4f) bl = 1 - ease((t - 404.4f) / 0.35f);
        if (t > 408.6f && t < 409.6f) bl = 1 - fabsf(t - 409.1f) / 0.5f, bl = bl < 0 ? 0 : bl * 1.4f > 1 ? 1 : bl * 1.4f;
        if (t > 413.4f && t < 414.9f) bl = t < 414.3f ? ease((t - 413.4f) / 0.2f) : 1 - ease((t - 414.3f) / 0.5f);
        u[6 * 4 + 3] = bl;
        u[7 * 4 + 2] = t > 414 ? 1 : 0;                                              /* blue and red */
        u[62 * 4 + 3] = (t > 409.3f && t < 409.75f) || t > 415.5f ? 1 : 0;          /* the tiny reflections */
        u[6 * 4] = t > 405 && t < 413 ? 0.25f * sinf((t - 405) * 1.3f) : u[6 * 4];
        u[6 * 4 + 2] = t < 414.8f ? 0.3f : mixf(0.45f, 0.24f, ease((t - 414.8f) / 0.6f));
        gl = ease((t - 418) / 5);
        u[7 * 4 + 3] = gl;
        u[12 * 4] = t < 402 ? 0.4f : mixf(1, 0.035f, ease((t - 417.6f) / 6));
        u[12 * 4 + 1] = 4 * (1 - ease((t - 417.6f) / 3));
        eL = hpt(0.033f, 0, 0.075f); eR = hpt(-0.033f, 0, 0.075f);
        setv(13, eL.x, eL.y, eL.z, 0); setv(14, eR.x, eR.y, eR.z, 0);
        setv(16, hdP.x, hdP.y - 0.3f, hdP.z, 1.1f); setv(17, 0.4f, 1, 0, 0); setv(63, 1, 0, gl * 1.2f, 2500);
        e = hpt(-0.033f, 0.002f, 0.064f);
        if (t < 415.2f) {                        /* the same close-up as the beginning */
            cam(va(e, va(vs(hdF, 0.07f), va(vs(hdU, 0.006f), vs(hdR, -0.008f)))), e, 24);
            setv(3, 0.07f, 0.05f, 0, 0);
        } else if (t < 417.6f) {                 /* both eyes: blue, red */
            V3 m = hpt(0, 0.0f, 0.07f);
            cam(va(m, vs(hdF, 0.2f)), m, 24);
            setv(3, 0.2f, 0.03f, 0, 0);
        } else if (t < 427.2f) {                 /* the room goes dark; her eyes light it */
            float k = ease((t - 417.6f) / 9.6f);
            V3 m = hpt(0, 0, 0.06f), c = vlerp(va(m, vs(hdF, 0.3f)), v3(1.6f, 1.9f, 1.4f), k * k);
            cam(c, vlerp(m, va(hdP, v3(0, -0.2f, 0)), k), mixf(26, 38, k));
            setv(3, mixf(0.3f, 2.6f, k), 0.02f, 0.6f * k, 0);
        } else if (t < 436.8f) {                 /* the title */
            cam(v3(1.3f, 1.6f, 1.6f), va(hdP, v3(0, -0.15f, 0)), 36);
            setv(3, 2.4f, 0.03f, 0.8f, 0);
            u[62 * 4 + 1] = ease((t - 432) / 2.5f) * (1 - ease((t - 438) / 2));
        } else {                                  /* I wake with memories not my own */
            V3 m = hpt(0, 0.0f, 0.07f);
            float k = ease((t - 436.8f) / 12);
            cam(va(m, vs(hdF, mixf(0.45f, 0.28f, k))), va(m, vs(hdU, -0.02f)), 26);
            setv(3, mixf(0.45f, 0.28f, k), 0.02f, 0.8f, 0);
            u[62 * 4] = ease((t - 448) / 4.5f);
        }
        if (t < 402.5f) u[62 * 4] = 1 - ease((t - 401) / 1.5f);
    }
    /* head uniforms from the skeleton */
    setv(4, hdP.x, hdP.y, hdP.z, 0);
    setv(5, hy, hp, 0, 0);
    mouth(t, mouth_a, 7 * 4);
    return prog;
}
