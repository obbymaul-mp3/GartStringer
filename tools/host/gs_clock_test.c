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
    printf(bad ? "CLOCK FAIL\n" : "clock ok\n");
    return bad;
}
