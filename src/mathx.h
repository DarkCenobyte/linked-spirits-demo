/* tiny libm replacement for the CRT-free Windows build (x87 transcendental ops) */
static inline float sinf(float x) { float r; __asm__("fsin" : "=t"(r) : "0"(x)); return r; }
static inline float cosf(float x) { float r; __asm__("fcos" : "=t"(r) : "0"(x)); return r; }
static inline double cos(double x) { double r; __asm__("fcos" : "=t"(r) : "0"(x)); return r; }
static inline float tanf(float x) { float r; __asm__("fptan\n\tfstp %%st(0)" : "=t"(r) : "0"(x)); return r; }
static inline float exp2f(float x)
{
    float r;
    __asm__("fld %%st(0)\n\tfrndint\n\tfsubr %%st, %%st(1)\n\tfxch\n\tf2xm1\n\tfld1\n\tfaddp\n\tfscale\n\tfstp %%st(1)" : "=t"(r) : "0"(x));
    return r;
}
static inline float expf(float x) { return exp2f(x * 1.44269504f); }
static inline float floorf(float x) { float i = (float)(int)x; return i > x ? i - 1 : i; }
#define fabsf __builtin_fabsf
#define sqrtf __builtin_sqrtf
static inline float atan2f(float y, float x) { float r; __asm__("fpatan" : "=t"(r) : "0"(x), "u"(y) : "st(1)"); return r; }
static inline float fmodf(float a, float b) { return a - b * floorf(a / b); }
static inline float log2f(float x) { float r; __asm__("fld1\n\tfxch\n\tfyl2x" : "=t"(r) : "0"(x)); return r; }
static inline float powf(float x, float y) { return exp2f(y * log2f(x)); }
