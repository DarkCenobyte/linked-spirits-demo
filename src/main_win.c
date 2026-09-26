/* LINKED//SPIRITS - Windows x64 entry point (no CRT). */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <GL/glcorearb.h>
#ifndef GLMINOR
#define GLMINOR 6
#endif

WINGDIAPI void APIENTRY glViewport(GLint, GLint, GLsizei, GLsizei);
WINGDIAPI void APIENTRY glDrawArrays(GLenum, GLint, GLsizei);
WINGDIAPI void APIENTRY glEnable(GLenum);
WINGDIAPI void APIENTRY glDisable(GLenum);
WINGDIAPI void APIENTRY glBlendFunc(GLenum, GLenum);
WINGDIAPI void APIENTRY glClear(GLbitfield);
WINGDIAPI void APIENTRY glClearColor(GLfloat, GLfloat, GLfloat, GLfloat);
WINGDIAPI void APIENTRY glScissor(GLint, GLint, GLsizei, GLsizei);
WINGDIAPI GLenum APIENTRY glGetError(void);
WINGDIAPI void APIENTRY glReadPixels(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *);
WINGDIAPI const GLubyte *APIENTRY glGetString(GLenum);

/* the compiler may emit these */
void *memset(void *d, int c, size_t n) { unsigned char *p = d; while (n--) *p++ = (unsigned char)c; return d; }
void *memcpy(void *d, const void *s, size_t n) { unsigned char *p = d; const unsigned char *q = s; while (n--) *p++ = *q++; return d; }
void *memmove(void *d, const void *s, size_t n) { unsigned char *p = d; const unsigned char *q = s; if (p < q) while (n--) *p++ = *q++; else while (n--) p[n] = q[n]; return d; }
int _fltused;

#ifdef DEBUG
size_t strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
static void dbg_log(const char *s)
{
    DWORD w; int n = 0;
    HANDLE f = CreateFileA("debug.log", GENERIC_WRITE, 0, 0, OPEN_ALWAYS, 0, 0);
    SetFilePointer(f, 0, 0, FILE_END);
    if (!s) s = "(null)";
    while (s[n]) n++;
    WriteFile(f, s, n, &w, 0); WriteFile(f, "\n", 1, &w, 0); CloseHandle(f);
}
#endif
#ifdef DEBUG
static void dbg_hex(const char *tag, unsigned v) { char b[40]; int k = 0; while (tag[k]) { b[k] = tag[k]; k++; } b[k++] = ' '; for (int j = 0; j < 8; j++) b[k++] = "0123456789abcdef"[(v >> (28 - 4 * j)) & 15]; b[k] = 0; dbg_log(b); }
#endif
#include "glfuncs.h"
#include "gen_shaders.h"
#include "synth.c"
#include "demo.c"

static void *getp(const char *n) { return (void *)wglGetProcAddress(n); }

static const PIXELFORMATDESCRIPTOR pfd = {
    sizeof(pfd), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32
};
static const int ctx_attr[] = {
    0x2091, 4,          /* WGL_CONTEXT_MAJOR_VERSION_ARB */
    0x2092, GLMINOR,    /* WGL_CONTEXT_MINOR_VERSION_ARB */
    0x9126, 1,          /* WGL_CONTEXT_PROFILE_MASK_ARB = CORE */
    0
};
static WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 2, 44100, 44100 * 4, 4, 16, 0 };
static WAVEHDR whdr;
static MMTIME mmt = { TIME_SAMPLES };

static void make_text(void)
{
    /* credits drawn once with GDI into an 8-bit texture */
    static unsigned char px[TEXT_W * TEXT_H];
    static struct { BITMAPINFOHEADER h; RGBQUAD pal[2]; } bmi = { { sizeof(BITMAPINFOHEADER), TEXT_W, TEXT_H, 1, 32 } };
    unsigned *bits; int i;
    HDC dc = CreateCompatibleDC(0);
    HBITMAP bm = CreateDIBSection(dc, (BITMAPINFO *)&bmi, 0, (void **)&bits, 0, 0);
    SelectObject(dc, bm);
    SetTextColor(dc, 0xffffff);
    SetBkMode(dc, TRANSPARENT);
    SetTextAlign(dc, TA_CENTER);
    SelectObject(dc, CreateFontA(150, 0, 0, 0, 200, 0, 0, 0, 0, 0, 0, ANTIALIASED_QUALITY, 0, "Segoe UI Light"));
    TextOutA(dc, TEXT_W / 2, 10, "LINKED//SPIRITS", 15);
    SelectObject(dc, CreateFontA(40, 0, 0, 0, 300, 0, 0, 0, 0, 0, 0, ANTIALIASED_QUALITY, 0, "Segoe UI Light"));
    TextOutA(dc, TEXT_W / 2, 190, "By: Claude Opus 5.5  //  Prompts by: DarkCenobyte", 49);
    GdiFlush();
    for (i = 0; i < TEXT_W * TEXT_H; i++) px[i] = (unsigned char)bits[i];
    glCreateTextures(GL_TEXTURE_2D, 1, &tex_text);
    glTextureStorage2D(tex_text, 1, GL_R8, TEXT_W, TEXT_H);
    glTextureSubImage2D(tex_text, 0, 0, 0, TEXT_W, TEXT_H, GL_RED, GL_UNSIGNED_BYTE, px);
    glTextureParameteri(tex_text, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

void entry(void)
{
    HWAVEOUT wo;
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    HDC dc = GetDC(CreateWindowExA(0, "static", 0, WS_POPUP | WS_VISIBLE | WS_MAXIMIZE, 0, 0, sw, sh, 0, 0, 0, 0));
    SetPixelFormat(dc, ChoosePixelFormat(dc, &pfd), &pfd);
    wglMakeCurrent(dc, wglCreateContext(dc));
    {   /* OpenGL 4.6 core; keep the default context if the driver refuses */
        HGLRC rc = ((HGLRC (WINAPI *)(HDC, HGLRC, const int *))wglGetProcAddress("wglCreateContextAttribsARB"))(dc, 0, ctx_attr);
        if (rc) wglMakeCurrent(dc, rc);
    }
    ShowCursor(0);
    gl_load(getp);
#ifdef DEBUG
    dbg_log((const char *)glGetString(GL_VERSION));
    dbg_log((const char *)glGetString(GL_RENDERER));
#endif
    synth_start();                         /* worker threads synthesise the soundtrack */
    demo_init(sw, sh);                     /* meanwhile: shaders, render targets */
    make_text();
    while (!synth_done()) {                /* precalc: a thin green thread of light */
        MSG m;
        PeekMessageA(&m, 0, 0, 0, PM_REMOVE);
        if (GetAsyncKeyState(VK_ESCAPE)) ExitProcess(0);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_SCISSOR_TEST);
        glScissor(sw / 2 - (int)(synth_progress * sw * 0.2f), sh / 2, (int)(synth_progress * sw * 0.4f) + 1, 1);
        glClearColor(0.1f, 0.9f, 0.4f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);
        SwapBuffers(dc);
        Sleep(15);
    }
    glClearColor(0, 0, 0, 1);
#ifdef DEBUG
    dbg_log("precalc done");
#endif
    whdr.lpData = (LPSTR)song16;
    whdr.dwBufferLength = SONG_LEN * 4;
    {
        int audio = !waveOutOpen(&wo, WAVE_MAPPER, &wfx, 0, 0, 0);   /* no device: run on the clock */
        DWORD t0 = timeGetTime();
#ifdef DEBUG
        dbg_log(audio ? "audio ok" : "no audio");
#endif
        if (audio) { waveOutPrepareHeader(wo, &whdr, sizeof(whdr)); waveOutWrite(wo, &whdr, sizeof(whdr)); }
        do {
            MSG m;
            PeekMessageA(&m, 0, 0, 0, PM_REMOVE);
            if (audio) waveOutGetPosition(wo, &mmt, sizeof(mmt));
            else mmt.u.sample = (timeGetTime() - t0) * 441 / 10;
            demo_frame(mmt.u.sample * (1.0f / 44100));
#ifdef DEBUG
            { static int once; GLenum e = glGetError(); if (e && once++ < 5) { char b[32] = "glerr 0x"; int k; for (k = 0; k < 4; k++) b[8 + k] = "0123456789abcdef"[(e >> (12 - 4 * k)) & 15]; b[12] = 0; dbg_log(b); } }
#endif
            SwapBuffers(dc);
        } while (!GetAsyncKeyState(VK_ESCAPE) && mmt.u.sample < SONG_LEN - 4410);
    }
    ExitProcess(0);
}
