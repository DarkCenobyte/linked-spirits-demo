/* LINKED//SPIRITS - render core (platform independent).
 * Passes: scene (raymarched, MRT colour+depth) -> particles (additive points,
 * depth-aware) -> bloom pyramid -> final grade / letterbox / title. */

#define RW 1920
#define RH 816                    /* 2.35:1 cinema frame */
#define NBLOOM 6
#define NU 70                     /* vec4 uniforms shared by every program */

enum { PR_LAB, PR_CATH, PR_SPACE, PR_PART, PR_DOWN, PR_UP, PR_FINAL, NPROG };

static GLuint prog[NPROG], tex_col, tex_dep, tex_text, bl[NBLOOM], fb_scene, fb_part, fb_bl[NBLOOM], fb_out, vao;
static GLuint tex_snc, tex_snd, fb_snap;   /* a frozen frame of the world, for its dissolution */
static float Us[NU * 4], snap_t = -1;
static float U[NU * 4];
static GLuint preview_fbo;
static void (*preview_hook)(float *);
#include "director.c"
static int scr_w, scr_h, rw = RW, rh = RH;   /* internal resolution (lowered on slow GPUs) */

static GLuint mk_tex(GLenum fmt, int w, int h)
{
    GLuint t;
    glCreateTextures(GL_TEXTURE_2D, 1, &t);
    glTextureStorage2D(t, 1, fmt, w, h);
    glTextureParameteri(t, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(t, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(t, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

static GLuint mk_fb(GLuint t0, GLuint t1)
{
    GLuint f; static const GLenum db[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glCreateFramebuffers(1, &f);
    glNamedFramebufferTexture(f, GL_COLOR_ATTACHMENT0, t0, 0);
    if (t1) glNamedFramebufferTexture(f, GL_COLOR_ATTACHMENT1, t1, 0);
    glNamedFramebufferDrawBuffers(f, t1 ? 2 : 1, db);
    return f;
}

static GLuint mk_shader(GLenum type, const char *const *src, int n)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, n, src, 0);
    glCompileShader(s);
#ifdef DEBUG
    {
        GLint ok; char log[8192];
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) { glGetShaderInfoLog(s, sizeof(log), 0, log); dbg_log(log); }
    }
#endif
    return s;
}

static GLuint mk_prog(GLuint vs, GLuint fs)
{
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
#ifdef DEBUG
    {
        GLint ok; char log[8192];
        glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) { glGetProgramInfoLog(p, sizeof(log), 0, log); dbg_log(log); }
    }
#endif
    return p;
}

/* render targets at the internal resolution w x h */
static void targets(int w, int h)
{
    int i;
    rw = w; rh = h;
    tex_col = mk_tex(GL_RGBA16F, w, h);
    tex_dep = mk_tex(GL_R32F, w, h);
    fb_scene = mk_fb(tex_col, tex_dep);
    fb_part = mk_fb(tex_col, 0);
    tex_snc = mk_tex(GL_RGBA16F, w, h);
    tex_snd = mk_tex(GL_R32F, w, h);
    fb_snap = mk_fb(tex_snc, tex_snd);
    snap_t = -1;
    for (i = 0; i < NBLOOM; i++) {
        w = (w + 1) / 2; h = (h + 1) / 2;
        bl[i] = mk_tex(GL_RGBA16F, w, h);
        fb_bl[i] = mk_fb(bl[i], 0);
    }
}

static void demo_init(int sw, int sh)
{
    int i;
    const char *src[5];
    GLuint vs_full, vs_part;
    scr_w = sw; scr_h = sh;
    glCreateVertexArrays(1, &vao);
    glBindVertexArray(vao);
    targets(RW, RH);
    {   /* final image at screen resolution, blitted to the window */
        GLuint t; glCreateTextures(GL_TEXTURE_2D, 1, &t); glTextureStorage2D(t, 1, GL_RGBA8, sw, sh);
        fb_out = mk_fb(t, 0);
    }
    /* shaders: common header + stage/scene specific part */
    src[0] = sh_head;
    src[1] = sh_vfull;
    vs_full = mk_shader(GL_VERTEX_SHADER, src, 2);
    src[1] = sh_vpart;
    vs_part = mk_shader(GL_VERTEX_SHADER, src, 2);
    src[2] = sh_lib;
    src[3] = sh_char;
    src[4] = sh_scene;
    for (i = 0; i < 3; i++) {
        static const char *const defs[3] = { "#define S_LAB\n", "#define S_CATH\n", "#define S_SPACE\n" };
        src[1] = defs[i];
        prog[i] = mk_prog(vs_full, mk_shader(GL_FRAGMENT_SHADER, src, 5));
    }
    src[1] = "";
    src[3] = sh_fpart;
    prog[PR_PART] = mk_prog(vs_part, mk_shader(GL_FRAGMENT_SHADER, src, 4));
    src[2] = sh_post; src[1] = "#define DOWN\n";
    prog[PR_DOWN] = mk_prog(vs_full, mk_shader(GL_FRAGMENT_SHADER, src, 3));
    src[1] = "#define UP\n";
    prog[PR_UP] = mk_prog(vs_full, mk_shader(GL_FRAGMENT_SHADER, src, 3));
    src[1] = "#define FINAL\n";
    prog[PR_FINAL] = mk_prog(vs_full, mk_shader(GL_FRAGMENT_SHADER, src, 3));
}

static void use(int p)
{
    glUseProgram(prog[p]);
    glUniform4fv(0, NU, U);
}

/* U[11].y > 0: the world will dissolve from a snapshot of the frame at that time.
 * It is rendered once (the director is a pure function of time) and its camera
 * is handed to the particles in U[18] (position, fov) and U[19] (target, roll). */
static void snapshot(void)
{
    float st = U[11 * 4 + 1];
    int i;
    if (st <= 0) return;
    if (st != snap_t) {
        int sc = direct(st, Us);
        glBindFramebuffer(GL_FRAMEBUFFER, fb_snap);
        glViewport(0, 0, rw, rh);
        glUseProgram(prog[sc]);
        glUniform4fv(0, NU, Us);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        snap_t = st;
    }
    for (i = 0; i < 8; i++) U[18 * 4 + i] = Us[4 + i];
}

static void demo_frame(float t)
{
    int i, w, h, np;
    int sc = direct(t, U);                    /* director fills U, returns scene program */
    U[2] = (float)rh;
    if (preview_hook) sc = (preview_hook(U), sc);
    if (U[11 * 4 + 1] > 0 && U[11 * 4 + 1] != snap_t) { snapshot(); sc = direct(t, U); U[2] = (float)rh; if (preview_hook) preview_hook(U); }
    snapshot();
    /* 1: scene */
    glBindFramebuffer(GL_FRAMEBUFFER, fb_scene);
    glViewport(0, 0, rw, rh);
    glDisable(GL_BLEND);
    use(sc);
    glBindTextureUnit(2, tex_text);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    /* 2: particles */
    np = (int)U[63 * 4 + 3];
    if (np > 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, fb_part);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glEnable(GL_PROGRAM_POINT_SIZE);
        use(PR_PART);
        glBindTextureUnit(1, tex_dep);
        glBindTextureUnit(4, tex_snc);
        glBindTextureUnit(5, tex_snd);
        glDrawArrays(GL_POINTS, (int)U[63 * 4 + 1], np - (int)U[63 * 4 + 1]);
        if (U[62 * 4 + 2] > 0) glDrawArrays(GL_LINES, 10000000, (int)U[62 * 4 + 2]);
        glDisable(GL_BLEND);
    }
    /* 3: bloom pyramid */
    use(PR_DOWN);
    w = rw; h = rh;
    for (i = 0; i < NBLOOM; i++) {
        w = (w + 1) / 2; h = (h + 1) / 2;
        glBindFramebuffer(GL_FRAMEBUFFER, fb_bl[i]);
        glViewport(0, 0, w, h);
        glBindTextureUnit(0, i ? bl[i - 1] : tex_col);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    use(PR_UP);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    for (i = NBLOOM - 1; i > 0; i--) {
        w = rw; h = rh;
        { int k; for (k = 0; k < i; k++) { w = (w + 1) / 2; h = (h + 1) / 2; } }
        glBindFramebuffer(GL_FRAMEBUFFER, fb_bl[i - 1]);
        glViewport(0, 0, w, h);
        glBindTextureUnit(0, bl[i]);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    glDisable(GL_BLEND);
    /* 4: final, letterboxed */
    glBindFramebuffer(GL_FRAMEBUFFER, fb_out);
    glViewport(0, 0, scr_w, scr_h);
    glClear(GL_COLOR_BUFFER_BIT);
    {
        int vw = scr_w, vh = scr_w * RH / RW;
        if (vh > scr_h) { vh = scr_h; vw = scr_h * RW / RH; }
        glViewport((scr_w - vw) / 2, (scr_h - vh) / 2, vw, vh);
    }
    use(PR_FINAL);
    glBindTextureUnit(0, tex_col);
    glBindTextureUnit(1, bl[0]);
    glBindTextureUnit(2, tex_text);
    glBindTextureUnit(3, tex_dep);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBlitNamedFramebuffer(fb_out, preview_fbo, 0, 0, scr_w, scr_h, 0, 0, scr_w, scr_h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
}
