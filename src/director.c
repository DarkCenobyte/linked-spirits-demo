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

static int direct(float t, float *u)
{
    Pose ps = { { 0, 0.93f, 0 } };
    float hy, hp;
    u[0] = t;
    walk_pose(&ps, t * 0.9f, 1);
    fk(u, 20, &ps, &hy, &hp);
    u[4 * 4] = u[23 * 4]; u[4 * 4 + 1] = u[23 * 4 + 1]; u[4 * 4 + 2] = u[23 * 4 + 2];
    u[5 * 4] = hy; u[5 * 4 + 1] = hp;
    u[6 * 4 + 2] = 0.35f;
    u[NU * 4 - 1] = 0;
    return PR_LAB;
}
