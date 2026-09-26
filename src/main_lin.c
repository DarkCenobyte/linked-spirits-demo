/* Linux preview harness (development only, not part of the release).
 * Renders chosen frames offscreen through Mesa/EGL with exactly the same
 * shaders and director as the Windows executable, and can dump the audio.
 *   preview frames <t0> <t1> <step> <outdir> [scale]
 *   preview audio <out.wav>
 */
#define _GNU_SOURCE
#include <EGL/egl.h>
#include <GL/glcorearb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define DEBUG 1
static void dbg_log(const char *s) { fprintf(stderr, "GL: %s\n", s); }
static void (APIENTRYP glViewport)(GLint, GLint, GLsizei, GLsizei);
static void (APIENTRYP glDrawArrays)(GLenum, GLint, GLsizei);
static void (APIENTRYP glEnable)(GLenum);
static void (APIENTRYP glDisable)(GLenum);
static void (APIENTRYP glBlendFunc)(GLenum, GLenum);
static void (APIENTRYP glClear)(GLbitfield);
static void (APIENTRYP glReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *);
static void (APIENTRYP glFinish)(void);
#include "glfuncs.h"
#include "gen_shaders.h"
#include "synth.c"
#include "demo.c"

static void *getp(const char *n) { return (void *)eglGetProcAddress(n); }
static void hook(float *u)
{   /* U<n>="x y z w" environment overrides for look development */
    for (int i = 0; i < NU; i++) {
        char k[16]; const char *v; sprintf(k, "U%d", i);
        if ((v = getenv(k))) sscanf(v, "%f %f %f %f", u + i * 4, u + i * 4 + 1, u + i * 4 + 2, u + i * 4 + 3);
    }
}

static void load_text(void)
{
    /* the Windows build draws the credits with GDI; here a pre-rendered
     * bitmap (tools/mktext.py) stands in for it */
    static unsigned char px[TEXT_W * TEXT_H];
    FILE *f = fopen("build/text.raw", "rb");
    if (f) { if (fread(px, 1, sizeof(px), f)) {} fclose(f); }
    glCreateTextures(GL_TEXTURE_2D, 1, &tex_text);
    glTextureStorage2D(tex_text, 1, GL_R8, TEXT_W, TEXT_H);
    glTextureSubImage2D(tex_text, 0, 0, 0, TEXT_W, TEXT_H, GL_RED, GL_UNSIGNED_BYTE, px);
    glTextureParameteri(tex_text, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

int main(int argc, char **argv)
{
    EGLDisplay d; EGLint ma, mi, n; EGLConfig c; EGLContext x;
    EGLint ca[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_NONE };
    EGLint xa[] = { EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 6,
                    EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE };
    if (argc < 2) { fprintf(stderr, "usage: preview frames t0 t1 step dir [scale] | audio out.wav\n"); return 1; }
    if (!strcmp(argv[1], "audio")) {
        struct timespec a, b; clock_gettime(CLOCK_MONOTONIC, &a);
        synth_render();
        if (argc > 3 && !strcmp(argv[3], "stems")) {
            for (int k = 0; k < NTRK; k++) {
                char fn[256]; FILE *o; sprintf(fn, "%s.stem%d.raw", argv[2], k);
                o = fopen(fn, "wb"); fwrite(trk[k], 4, SONG_LEN, o); fclose(o);
            }
        }
        if (argc > 3 && !strcmp(argv[3], "vox")) {   /* voices only (for intelligibility tests) */
            for (int i = 0; i < SONG_LEN; i++) { float v = (trk[T_VA][i] + trk[T_VB][i]) * 0.8f; if (v > 1) v = 1; if (v < -1) v = -1; song16[i * 2] = song16[i * 2 + 1] = (short)(v * 32000); }
        }
        clock_gettime(CLOCK_MONOTONIC, &b);
        fprintf(stderr, "synth %.2fs\n", (b.tv_sec - a.tv_sec) + (b.tv_nsec - a.tv_nsec) * 1e-9);
        {
            FILE *o = fopen(argv[2], "wb");
            int hdr[11] = { 0x46464952, 36 + SONG_LEN * 4, 0x45564157, 0x20746d66, 16, 0x20001, 44100, 44100 * 4, 0x100004, 0x61746164, SONG_LEN * 4 };
            fwrite(hdr, 4, 11, o); fwrite(song16, 4, SONG_LEN, o); fclose(o);
        }
        return 0;
    }
    setenv("MESA_GL_VERSION_OVERRIDE", "4.6", 1);
    setenv("MESA_GLSL_VERSION_OVERRIDE", "460", 1);
    setenv("EGL_PLATFORM", "surfaceless", 1);
    d = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(d, &ma, &mi);
    eglChooseConfig(d, ca, &c, 1, &n);
    eglBindAPI(EGL_OPENGL_API);
    x = eglCreateContext(d, c, EGL_NO_CONTEXT, xa);
    eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, x);
    glViewport = getp("glViewport"); glDrawArrays = getp("glDrawArrays"); glEnable = getp("glEnable");
    glDisable = getp("glDisable"); glBlendFunc = getp("glBlendFunc"); glClear = getp("glClear");
    glReadPixels = getp("glReadPixels"); glFinish = getp("glFinish");
    gl_load(getp);
    {
        float t0 = atof(argv[2]), t1 = atof(argv[3]), st = atof(argv[4]), scale = argc > 6 ? atof(argv[6]) : 0.5f;
        int W = (int)(RW * scale), H = (int)(RH * scale), fi = 0;
        GLuint fbo, ct; unsigned char *px = malloc(W * H * 3);
        synth_timeline_only();
        preview_hook = hook;
        demo_init(W, H);
        load_text();
        glCreateTextures(GL_TEXTURE_2D, 1, &ct);
        glTextureStorage2D(ct, 1, GL_RGB8, W, H);
        glCreateFramebuffers(1, &fbo);
        glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, ct, 0);
        for (float t = t0; t <= t1 + 1e-4f; t += st, fi++) {
            char fn[512]; FILE *o; struct timespec a, b;
            clock_gettime(CLOCK_MONOTONIC, &a);
            preview_fbo = fbo;
            demo_frame(t);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glReadPixels(0, 0, W, H, GL_RGB, GL_UNSIGNED_BYTE, px);
            glFinish();
            clock_gettime(CLOCK_MONOTONIC, &b);
            sprintf(fn, "%s/f%05d.ppm", argv[5], fi);
            o = fopen(fn, "wb"); fprintf(o, "P6\n%d %d\n255\n", W, H);
            for (int y = H - 1; y >= 0; y--) fwrite(px + y * W * 3, 1, W * 3, o);
            fclose(o);
            fprintf(stderr, "t=%.2f  %.2fs\n", t, (b.tv_sec - a.tv_sec) + (b.tv_nsec - a.tv_nsec) * 1e-9);
        }
    }
    return 0;
}
