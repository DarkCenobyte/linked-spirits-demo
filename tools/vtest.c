/* test harness: renders sung lines from a test score into separate WAV files */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "../src/voice.c"
static void wav(const char *fn, float *x, int n)
{
    FILE *o = fopen(fn, "wb"); float mx = 1e-6f; int i;
    int hdr[11] = {0x46464952, 36 + n * 2, 0x45564157, 0x20746d66, 16, 0x10001, 44100, 88200, 0x100002, 0x61746164, n * 2};
    for (i = 0; i < n; i++) if (fabsf(x[i]) > mx) mx = fabsf(x[i]);
    fwrite(hdr, 4, 11, o);
    for (i = 0; i < n; i++) { short v = (short)(x[i] / mx * 30000); fwrite(&v, 2, 1, o); }
    fclose(o);
}
int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "rb"); int nl, l;
    static float out[44100 * 30];
    if (fread(&nl, 4, 1, f) != 1) return 1;
    voice_init();
    for (l = 0; l < nl; l++) {
        int nw, nn; short words[64]; VNote nt[128]; VStyle st; char fn[256];
        if (fread(&nw, 4, 1, f) != 1) return 1;
        if (fread(words, 2, nw, f) != (size_t)nw) return 1;
        if (fread(&nn, 4, 1, f) != 1) return 1;
        if (fread(nt, sizeof(VNote), nn, f) != (size_t)nn) return 1;
        if (fread(&st, sizeof(VStyle), 1, f) != 1) return 1;
        memset(out, 0, sizeof(out));
        voice_line(out, 44100 * 30, 0, words, nw, nt, nn, &st, 0);
        {
            int n = (int)((nt[nn - 1].start + nt[nn - 1].dur + 0.6f) * 44100);
            sprintf(fn, "%s/line%02d.wav", argv[2], l); wav(fn, out, n);
        }
    }
    return 0;
}
