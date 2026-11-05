/* SPDX-License-Identifier: GPL-3.0-only */
/* GartStringer's sound on the host: render a patch playing a figure to a WAV, report level, spectrum and the time it took.
 *   cc -O2 -Ifirmware/src -Ibuild/gen tools/host/gs_audio_test.c -o /tmp/gsat -lm
 *   /tmp/gsat PATCH SECONDS OUT.wav [chord|single|arp]       env GS_SET="MODE=2,CUT=60"  GS_REC=a:b (REC held from second a to b) */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#define SK_POOL
#include "../../firmware/src/gs_synth.c"
#include "../../firmware/src/gs_fx.c"
#include "../../firmware/src/gs_patch.c"

static const char *PN[P_N] = {"MODE", "DET", "MIX", "SUB", "COL", "OCT", "CUT", "RES", "FENV", "KEY", "FTYPE", "DRV", "AA", "AD", "AS", "AR", "FA", "FD", "FS", "FR", "LRATE", "LAMT", "LDST", "LSHP", "GLIDE", "VMODE", "CHOR", "DLY", "DTIM", "DFB", "REV", "RSZ", "GPAT", "GRATE", "GDEP", "GSMO", "APAT", "ARATE", "ARNG", "AGATE", "AON", "GLP", "GLL", "GLT", "CRU"};

static void wav(const char *path, int16_t *pcm, uint32_t frames)
{
    FILE *f = fopen(path, "wb");
    uint32_t n = frames * 4, v;
    fwrite("RIFF", 1, 4, f); v = 36 + n; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    { uint16_t a = 1, ch = 2; uint32_t rr = 44100, br = 44100 * 4; uint16_t ba = 4, bits = 16;
      fwrite(&a, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&rr, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f); }
    fwrite("data", 1, 4, f); fwrite(&n, 4, 1, f); fwrite(pcm, 1, n, f); fclose(f);
}

int main(int argc, char **argv)
{
    int patch = argc > 1 ? atoi(argv[1]) : 0;
    double secs = argc > 2 ? atof(argv[2]) : 8;
    const char *path = argc > 3 ? argv[3] : "/tmp/gs.wav", *how = argc > 4 ? argv[4] : "chord";
    uint32_t total = (uint32_t)(secs * 44100) / SK_CTL * SK_CTL, i, k;
    int16_t *pcm = malloc(total * 4);
    int32_t buf[2 * SK_CTL];
    double ss = 0, pk = 0;
    clock_t t0;
    float *fl = malloc(sizeof(float) * total);
    double ra = -1, rb = -1;
    gs_init();
    gs_load((uint32_t)patch);
    if (getenv("GS_SET")) {
        char *s = strdup(getenv("GS_SET")), *tok = strtok(s, ",");
        while (tok) {
            char *eq = strchr(tok, '=');
            if (eq) { *eq = 0; for (k = 0; k < P_N; k++) if (!strcmp(PN[k], tok)) gs_set(k, atoi(eq + 1)); }
            tok = strtok(0, ",");
        }
    }
    if (getenv("GS_REC")) sscanf(getenv("GS_REC"), "%lf:%lf", &ra, &rb);
    sk_f.ready = 0;
    t0 = clock();
    for (i = 0; i < total; i += SK_CTL) {
        double t = (double)i / 44100.0;
        if (!strcmp(how, "chord") || !strcmp(how, "arp")) {          /* a minor chord held for 3 s, then a lifted one */
            static int st;
            if (t >= 0.2 && st == 0) { gs_key(57, 1); gs_key(60, 1); gs_key(64, 1); st = 1; }
            if (t >= 3.4 && st == 1) { gs_key(57, 0); gs_key(60, 0); gs_key(64, 0); st = 2; }
            if (t >= 3.8 && st == 2) { gs_key(55, 1); gs_key(59, 1); gs_key(62, 1); st = 3; }
            if (t >= 7.2 && st == 3) { gs_key(55, 0); gs_key(59, 0); gs_key(62, 0); st = 4; }
        } else {
            static int st; static const int N[8] = {45, 48, 52, 45, 50, 53, 57, 50};
            int beat = (int)(t / 0.8), pos = (int)(t / 0.8 * 1000) % 1000;
            if (beat < 8 && st == beat * 2 && pos < 5) { gs_key(N[beat], 1); st++; }
            if (beat < 8 && st == beat * 2 + 1 && pos > 600) { gs_key(N[beat], 0); st++; }
        }
        gl.manual = (ra >= 0 && t >= ra && t < rb);
        sk_render(buf, SK_CTL);
        for (k = 0; k < SK_CTL; k++) {
            int32_t a = sk_clampi(buf[2 * k], -32767, 32767), b = sk_clampi(buf[2 * k + 1], -32767, 32767);
            pcm[2 * (i + k)] = (int16_t)a; pcm[2 * (i + k) + 1] = (int16_t)b;
            fl[i + k] = (float)(a + b) * 0.5f;
        }
    }
    {
        double el = (double)(clock() - t0) / CLOCKS_PER_SEC;
        for (i = 0; i < total * 2; i++) { double a = fabs((double)pcm[i]); if (a > pk) pk = a; ss += (double)pcm[i] * pcm[i]; }
        printf("patch %d %s: peak %.1f dBFS rms %.1f dBFS; %.2f s of audio in %.3f s (%.1f%% of one core on this PC)\n", patch, GS_PRE[patch].name,
               20 * log10(pk / 32767.0 + 1e-9), 20 * log10(sqrt(ss / (total * 2.0)) / 32767.0 + 1e-9), secs, el, 100.0 * el / secs);
    }
    wav(path, pcm, total);
    return 0;
}
