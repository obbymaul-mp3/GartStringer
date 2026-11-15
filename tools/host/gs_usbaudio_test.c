/* SPDX-License-Identifier: GPL-3.0-only */
/* The USB recording's stream on a PC (usb_audio_stream.c alone, as the firmware configures it: stereo, 48 kHz only).
 * A sine goes in at the synth's rate in blocks of 32, packets come out once a millisecond for a minute, with the synth's clock
 * 400 ppm fast, then 1000 ppm slow: every packet must be 47..49 frames, the ring must never run dry or over once it has
 * primed, and what comes out must be the same sine at 48 kHz (the two channels kept apart).
 *   cc -O2 -Ifirmware/src tools/host/gs_usbaudio_test.c -o /tmp/gsua -lm && /tmp/gsua */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define UA_CAP_CHANNELS 2u
#define UA_ONLY48 1
#undef __APPLE__
#include "usb_audio_stream.c"
#undef UA_POOL

static int run(double fs_in, double hz_l, double hz_r, const char *name)
{
    static int16_t got[2 * 48000 * 62];
    uint8_t pk[UA_PACKET];
    double t_in = 0, ph_l = 0, ph_r = 0;
    uint32_t ms, n_out = 0, i, bad = 0, sizes[3] = {0, 0, 0}, first = 0;
    int fail = 0;
    ua_reset();
    ua.cap_underruns = ua.cap_overruns = ua.cap_nudges_up = ua.cap_nudges_down = 0;
    ua.cap_alt = 1;
    ua.cap_hold = 0;
    for (ms = 0; ms < 60000u; ms++) {
        uint32_t len, fr;
        while (t_in < (ms + 1) * 1e-3) {                       /* the blocks the audio interrupt makes in this millisecond */
            int16_t st[64];
            uint32_t n;
            for (i = 0; i < 32u; i++) {
                st[2 * i] = (int16_t)lrint(20000.0 * sin(ph_l));
                st[2 * i + 1] = (int16_t)lrint(12000.0 * sin(ph_r));
                ph_l += 2 * M_PI * hz_l / 44100.0;             /* (the synth's notes are tuned for a nominal 44.1 kHz) */
                ph_r += 2 * M_PI * hz_r / 44100.0;
            }
            n = ua_resample(st, 32);
            if (n < 34u || n > 35u)
                bad++;
            ua_feed(ua.rs_out, n, 1);
            t_in += 32.0 / fs_in;
        }
        len = ua_transmit(pk);
        fr = len / 4u;
        if (fr < 47u || fr > 49u)
            bad++;
        else
            sizes[fr - 47u]++;
        if (ua.cap_ready && !first)
            first = ms + 1u;
        if (ua.cap_ready)
            for (i = 0; i < 2u * fr; i++)
                got[n_out++] = (int16_t)(pk[2 * i] | pk[2 * i + 1] << 8);
    }
    {   /* each channel against a sine of its own frequency (least squares), over a second from the middle */
        uint32_t ch, N = 48000, off = 2u * 48000u * 20u;
        for (ch = 0; ch < 2u; ch++) {
            /* (the host counts the frames at a nominal 48 kHz, as the synth counts its own at a nominal 44.1: the pitch is kept) */
            double w = 2 * M_PI * (ch ? hz_r : hz_l) / 48000.0, sn = 0, c = 0, e = 0, a, b, best;
            for (i = 0; i < N; i++) {
                sn += got[off + 2 * i + ch] * sin(w * i);
                c += got[off + 2 * i + ch] * cos(w * i);
            }
            a = 2 * sn / N;
            b = 2 * c / N;
            for (i = 0; i < N; i++) {
                double d = got[off + 2 * i + ch] - a * sin(w * i) - b * cos(w * i);
                e += d * d;
            }
            best = 10 * log10((a * a + b * b) / 2 / (e / N + 1e-9));
            printf("  %s ch%u: %.1f dB signal to everything else\n", name, ch, best);
            if (best < 60.0)
                fail = 1;
        }
    }
    printf("%s: primed after %u ms, packets 47/48/49 frames: %u/%u/%u, nudges +%u -%u, underruns %u, overruns %u, bad %u\n", name, first,
           sizes[0], sizes[1], sizes[2], ua.cap_nudges_up, ua.cap_nudges_down, ua.cap_underruns, ua.cap_overruns, bad);
    if (bad || ua.cap_underruns || ua.cap_overruns || !first || first > 20u)
        fail = 1;
    return fail;
}

int main(void)
{
    int f = 0;
    f |= run(44100.0, 1000.0, 3000.0, "exact");
    f |= run(44117.6, 440.0, 7000.0, "+400 ppm");
    f |= run(44055.9, 100.0, 15000.0, "-1000 ppm");
    puts(f ? "FAILED" : "usb audio ok");
    return f;
}
