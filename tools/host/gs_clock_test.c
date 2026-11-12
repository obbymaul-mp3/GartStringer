/* SPDX-License-Identifier: GPL-3.0-only */
/* GartStringer's clock on the host: run the sequencer, the metronome, the gate and the arp for ten minutes at several tempos and
 * check that each keeps the tempo exactly (no step loses time, nothing rounds to whole samples), also with swing and at a
 * corrected sample rate.     cc -O2 -Ifirmware/src -Ibuild/gen tools/host/gs_clock_test.c -o /tmp/gsck -lm && /tmp/gsck */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define SK_POOL
#include "../../firmware/src/gs_synth.c"
#include "../../firmware/src/gs_fx.c"
#include "../../firmware/src/gs_patch.c"

static int run(int bpm, int swing, uint32_t rate, int arp)
{
    uint32_t total = 44100u * 600u, i;
    int32_t buf[2 * SK_CTL];
    uint64_t steps = 0, clicks = 0, gates = 0, arps = 0;
    uint8_t ls = 255, lg = 255, la = 0;
    uint32_t b0;
    double want_s, want_c, e1, e2, e3, e4 = 0;
    gs_init();
    gs_load(0);
    gs.bpm = (uint8_t)bpm;
    gs_rate = rate;
    gs_set(P_SSWG, swing);
    gs_set(P_SMET, 3);
    gs_set(P_AON, 0);
    sk_f.ready = 0;
    if (arp) {
        gs_set(P_AON, 1);
        gs_key(60, 1);
    } else
        gs_seq_start();
    b0 = gs.beat;
    for (i = 0; i < total; i += SK_CTL) {
        sk_render(buf, SK_CTL);
        if (gs.sstep != ls) { ls = gs.sstep; steps++; }
        if (gs.gstep != lg) { lg = gs.gstep; gates++; }
        if (gs.astep != la) { la = gs.astep; arps++; }
    }
    clicks = (gs.beat - b0) / 4u;                                             /* (the metronome ticks on every fourth sixteenth of this count) */
    /* what ten minutes should hold, at the rate the converter really runs at */
    want_s = 600.0 * 44100.0 / ((double)rate / 1000.0) * bpm / 60.0 * 4.0;      /* sixteenths (the sequence at 1/16, the gate at 1/16) */
    want_c = want_s / 4.0;
    e1 = (double)steps - want_s;
    e2 = (double)gates - want_s;
    e3 = (double)clicks - want_c;
    if (arp)
        e4 = (double)arps - want_s;
    printf("bpm %3d swing %3d rate %u %s: steps %+.2f gate %+.2f clicks %+.2f arp %+.2f (of %.1f sixteenths)\n", bpm, swing, rate, arp ? "arp" : "seq",
           arp ? 0.0 : e1, e2, arp ? 0.0 : e3, e4, want_s);
    return (!arp && (fabs(e1) > 1.5 || fabs(e3) > 1.5)) || fabs(e2) > 1.5 || fabs(e4) > 1.5;
}

/* recording: after the count-in (a bar), keys played at chosen moments must land on the steps nearest to them */
static int rec(int bpm)
{
    static const double AT[5] = {0.0, 2.02, 4.96, 7.45, 15.1};      /* in sixteenths after the count-in */
    static const int WANT[5] = {0, 2, 5, 7, 15};
    double s16 = 44100.0 * 15.0 / bpm, t0 = 16.0 * s16;
    int32_t buf[2 * SK_CTL];
    uint32_t i, k, bad = 0, done[5] = {0};
    gs_init();
    gs_load(0);
    gs.bpm = (uint8_t)bpm;
    gs_rate = 44100000u;
    sk_f.ready = 0;
    for (k = 0; k < GS_NS; k++)
        gs.sp[0][k] = 0;
    for (i = 0; i < 3000; i += SK_CTL)                              /* (some time passes before the start: the clock is anywhere) */
        sk_render(buf, SK_CTL);
    gs.srec = 1;
    gs_seq_start();
    for (i = 0; i < (uint32_t)(34.0 * s16); i += SK_CTL) {
        for (k = 0; k < 5; k++)
            if (!done[k] && (double)i >= t0 + AT[k] * s16) {
                done[k] = 1;
                gs_key(60 + k, 1);
            } else if (done[k] == 1 && (double)i >= t0 + AT[k] * s16 + 2000) {
                done[k] = 2;
                gs_key(60 + k, 0);
            }
        sk_render(buf, SK_CTL);
        if ((double)i < t0 - 2.0 * SK_CTL && !gs.scount)
            bad |= 1;                                              /* the count-in ended early */
        if ((double)i > t0 + 2.0 * SK_CTL && gs.scount)
            bad |= 2;                                              /* .. or late */
    }
    for (k = 0; k < 5; k++)
        if (!SP_ON(gs.sp[0][WANT[k]]) || SP_NOTE(gs.sp[0][WANT[k]]) != 12 + (int)k)
            bad |= 4;
    if (bad & 4) { for (k = 0; k < 20; k++) printf("%d ", SP_ON(gs.sp[0][k]) ? SP_NOTE(gs.sp[0][k]) : -1); printf(" len %d\n", gs.spl[0]); }
    printf("record at %d bpm: %s%s%s%s\n", bpm, bad ? "" : "ok", bad & 1 ? " count-in early" : "", bad & 2 ? " count-in late" : "", bad & 4 ? " notes on the wrong steps" : "");
    return bad != 0;
}

int main(void)
{
    static const int B[] = {60, 97, 120, 124, 128, 133, 138, 140, 174};
    int bad = 0;
    unsigned i;
    for (i = 0; i < sizeof B / sizeof B[0]; i++)
        bad |= run(B[i], 0, 44100000u, 0);
    bad |= run(128, 80, 44100000u, 0);
    bad |= run(128, 0, 44117300u, 0);
    bad |= run(128, 0, 44100000u, 1);
    bad |= rec(124);
    bad |= rec(138);
    printf(bad ? "CLOCK FAIL\n" : "clock ok\n");
    return bad;
}
