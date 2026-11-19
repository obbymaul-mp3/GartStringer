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

/* an outside clock: ticks at a tempo that is no whole BPM, handed over at the start of each 128-sample half (as the hardware does);
 * the sequence must take exactly one step every six ticks, close to where the tick fell */
static int ext(double bpm)
{
    double per = 44100.0 * 60.0 / bpm / 24.0, next = 0.0, worst = 0.0;
    int32_t buf[2 * SK_CTL];
    uint32_t i, total = 44100u * 300u, ticks = 0, steps = 0, bad = 0;
    uint8_t ls = 255;
    gs_init();
    gs_load(0);
    gs.bpm = 120;
    gs_rate = 44100000u;
    sk_f.ready = 0;
    memset(&gs_ext, 0, sizeof gs_ext);
    gs_ext_start();
    for (i = 0; i < total; i += SK_CTL) {
        if ((i & 127u) == 0u)
            while (next <= (double)i) {
                gs_ext_tick();
                ticks++;
                next += per;
            }
        sk_render(buf, SK_CTL);
        if (gs.srun && gs.sstep != ls) {
            double ideal = (double)steps * 6.0 * per, e = (double)i - ideal;
            ls = gs.sstep;
            if (steps > 64u && fabs(e) > worst)
                worst = fabs(e);
            if (getenv("CKDBG") && (steps < 12u || (steps % 400u) == 0u)) printf("  step %u e %.0f samples bpm %u pend %d per %u\n", steps, e, gs.bpm, gs_ext.pend, gs_ext.per >> 8);
            steps++;
        }
    }
    if (labs((long)steps - (long)((ticks + 5u) / 6u)) > 1)
        bad |= 1;
    if (worst > 44100.0 * 0.006)
        bad |= 2;
    printf("outside clock at %.1f bpm: %u ticks, %u steps (want %u), worst step %.1f ms from its tick, shows %u bpm: %s\n", bpm, ticks, steps, (ticks + 5u) / 6u,
           worst * 1000.0 / 44100.0, gs.bpm, bad ? "FAIL" : "ok");
    gs_ext.on = 0;
    return bad != 0;
}

/* the sync and the outside transport: Start, Stop and Continue; the clock going away in AUTO and in EXTERNAL; INTERNAL deaf to it */
static double sm_next, sm_pos;
static uint32_t sm_steps;
static uint8_t sm_ls, sm_first;
static int sm_zero;                                                /* which step change (from 0) was the first to be step 1 of the pattern: -1 none */
static void sm_feed(double ms, double bpm)
{
    int32_t buf[2 * SK_CTL];
    double per = bpm > 0 ? 44100.0 * 60.0 / bpm / 24.0 : 0, end = sm_pos + ms * 44.1;
    if (sm_next < sm_pos)
        sm_next = sm_pos;
    sm_steps = 0;
    sm_first = 255;
    sm_zero = -1;
    for (; sm_pos < end; sm_pos += SK_CTL) {
        if (((uint32_t)sm_pos & 127u) == 0u)
            while (per > 0 && sm_next <= sm_pos) {
                gs_ext_tick();
                sm_next += per;
            }
        sk_render(buf, SK_CTL);
        if (gs.srun && gs.sstep != sm_ls) {
            sm_ls = gs.sstep;
            if (sm_first == 255)
                sm_first = gs.sstep;
            if (gs.sstep == 0 && sm_zero < 0)
                sm_zero = (int)sm_steps;
            sm_steps++;
        }
    }
}
static int sm_sounding(void)
{
    int i, n = 0;
    for (i = 0; i < GS_NV; i++)
        n += gs.v[i].on != 0;
    return n;
}
#define SM_CHECK(c, what) do { if (!(c)) { printf("  sync: %s: FAIL (bpm %u first %u steps %u nh %u sounding %d acur %d on %u)\n", what, gs.bpm, sm_first, sm_steps, gs.nh, sm_sounding(), gs.acur, gs_ext.on); bad = 1; } } while (0)
static int sync_modes(void)
{
    int bad = 0, i;
    uint8_t s0;
    gs_init();
    gs_load(0);
    gs_rate = 44100000u;
    sk_f.ready = 0;
    gs_set_hold(0);
    gs_release_all();
    memset(&gs_ext, 0, sizeof gs_ext);
    for (i = 0; i < 16; i++)
        gs.sp[0][i] = SP_MAKE(12 + (i % 5), 0, 0);
    gs.spl[0] = 16;
    sm_pos = sm_next = 0;
    sm_ls = 255;
    /* AUTO */
    gs_sync = GS_SYNC_AUTO;
    gs.bpm = 100;
    gs_ext_start();
    sm_feed(2000, 120);
    SM_CHECK(gs.srun && sm_first == 0 && sm_steps >= 15 && sm_steps <= 17, "Start plays from step 1, in the clock's time");
    SM_CHECK(gs.bpm == 120 && gs_ext.on, "the tempo shown is the clock's");
    gs_ext_stop();
    s0 = gs.sstep;
    sm_feed(700, 120);
    SM_CHECK(!gs.srun && sm_steps == 0 && sm_sounding() == 0 && gs.snote < 0, "Stop stops it and nothing is left sounding");
    gs_ext_cont();
    sm_feed(1000, 120);
    SM_CHECK(gs.srun && sm_first == (uint8_t)((s0 + 1) % 16) && sm_steps >= 7 && sm_steps <= 9, "Continue goes on from where it stopped");
    gs_ext_start();
    sm_feed(500, 120);
    SM_CHECK(gs.srun && sm_zero >= 0 && sm_zero <= 1, "Start again is step 1 again");
    sm_feed(1500, 0);                                              /* the clock goes away */
    SM_CHECK(!gs_ext.on && gs.srun && sm_steps >= 8, "AUTO: without the clock it carries on at its own tempo");
    /* EXTERNAL */
    gs_sync = GS_SYNC_EXT;
    sm_feed(1000, 120);
    SM_CHECK(gs_ext.on && sm_steps >= 7 && sm_steps <= 9, "EXTERNAL follows the clock");
    sm_feed(700, 0);
    s0 = gs.sstep;
    sm_feed(1500, 0);
    SM_CHECK(!gs_ext.on && gs.sstep == s0 && sm_steps == 0, "EXTERNAL: without the clock the sequence waits");
    SM_CHECK(sm_sounding() == 0 && gs.snote < 0 && gs.gate_t == 32767, "EXTERNAL: nothing hangs while it waits, and the gate is open");
    sm_feed(1000, 120);
    SM_CHECK(gs_ext.on && sm_steps >= 6 && sm_steps <= 9, "EXTERNAL: the clock back, it goes on");
    gs_ext_stop();
    /* a chord held by HOLD under the arp: an outside Stop lets it go */
    gs_sync = GS_SYNC_AUTO;
    gs_set(P_AON, 1);
    gs_set_hold(1);
    gs_key(60, 1); gs_key(64, 1); gs_key(60, 0); gs_key(64, 0);
    sm_feed(600, 120);
    SM_CHECK(gs.nh == 2, "(the arp holds its chord)");
    gs_ext_stop();
    sm_feed(1500, 120);                                            /* (long enough for the release to end) */
    SM_CHECK(gs.nh == 0 && sm_sounding() == 0 && gs.acur < 0, "Stop lets a held arp chord go");
    gs_set_hold(0);
    gs_set(P_AON, 0);
    /* INTERNAL */
    sm_feed(700, 0);
    gs_sync = GS_SYNC_INT;
    gs.bpm = 100;
    gs_ext_start();
    sm_feed(1500, 133);
    SM_CHECK(!gs_ext.on && !gs.srun && gs.bpm == 100, "INTERNAL: the clock and Start are let go, the tempo is its own");
    gs_seq_start();
    sm_ls = 255;
    sm_feed(3000, 133);
    SM_CHECK(sm_steps >= 19 && sm_steps <= 21, "INTERNAL: the sequence runs at its own tempo with a clock coming in");   /* 100 bpm: 20 sixteenths in 3 s */
    gs_seq_stop();
    gs_sync = GS_SYNC_AUTO;
    memset(&gs_ext, 0, sizeof gs_ext);
    printf("sync and transport: %s\n", bad ? "FAIL" : "ok");
    return bad;
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
    bad |= ext(126.3);
    bad |= ext(140.0);
    bad |= ext(87.5);
    bad |= sync_modes();
    bad |= rec(124);
    bad |= rec(138);
    printf(bad ? "CLOCK FAIL\n" : "clock ok\n");
    return bad;
}
