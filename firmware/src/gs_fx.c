/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GartStringer contributors */
/* GartStringer's effects, in the order the sound meets them: the drive and the crusher, the ensemble (a three-tap
 * chorus that gives the mono voices their width), the glitch (a stutter that loops the last sixteenth, in reverse, a tape
 * stop, a drop-out), the tempo-synced ping-pong echo, a reverb (an eight-line feedback network with a Hadamard mix), and
 * the master (a DC cut, a soft limiter, the MASTER pot). Integer only; every buffer is int16 in the pool.
 * Included by gartstringer.c after gs_synth.c. */

#define SK_RVN 8                         /* reverb lines */
static const uint16_t SK_RVL[SK_RVN] = {1049, 1223, 1399, 1621, 1823, 2063, 2273, 2551};
#define SK_RVMAX 2560
#define SK_AP1 142
#define SK_AP2 107
#define SK_DLN 16384                     /* the echo's line, samples (a power of two): 0.37 s */
#define GS_GLN 16384                     /* the glitch buffer: 0.37 s */
#define GS_CHN 1024                      /* the ensemble's line */

static struct {
    int16_t line[SK_RVN][SK_RVMAX];
    int16_t ap1[SK_AP1], ap2[SK_AP2];
    int16_t dl[2][SK_DLN];
    int16_t gl[2][GS_GLN];
    int16_t ch[GS_CHN];
} sk_fxm SK_POOL;

static struct {
    uint32_t rp[SK_RVN];                 /* the reverb lines' write positions */
    int32_t lpz[SK_RVN];                 /* the damping filters */
    uint32_t ap1p, ap2p, mod;
    int32_t g[SK_RVN];                   /* the lines' gains */
    int32_t damp;                        /* the damping coefficient, Q15 */
    int32_t wet, in;                     /* the return and the send, Q15 */
    uint32_t dwp;                        /* the echo */
    int32_t dcur, dtgt;                  /* its length now and wanted, Q8 samples */
    int32_t dfb, dwet, dlz[2];
    int32_t dc[2], mlow[2], mlow2[2];    /* the master's DC cut and its two-pole top (about 9 kHz: the digital edge off a bright saw) */
    uint32_t master;                     /* MASTER, Q12 (the pot) */
    uint32_t chp, chph[3];               /* the ensemble */
    uint32_t gwp;                        /* the glitch's write position */
    uint8_t ready;
} sk_f;

static inline int32_t sk_q15(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b + 16384) >> 15); }

static void sk_reverb(const int32_t *in, int32_t *ol, int32_t *or_, uint32_t n)
{
    uint32_t i, k;
    for (i = 0; i < n; i++) {
        int32_t x = sk_q15(in[i], sk_f.in), t, d[SK_RVN], y;
        int32_t lfo = SK_SIN[(sk_f.mod >> 10) & 1023u];
        sk_f.mod += 11u;                                               /* ~0.4 Hz */
        /* two allpasses spread the input */
        t = sk_fxm.ap1[sk_f.ap1p];
        y = x - ((t * 19660) >> 15);
        sk_fxm.ap1[sk_f.ap1p] = (int16_t)sk_clampi(y, -32767, 32767);
        x = t + ((y * 19660) >> 15);
        if (++sk_f.ap1p >= SK_AP1)
            sk_f.ap1p = 0;
        t = sk_fxm.ap2[sk_f.ap2p];
        y = x - ((t * 19660) >> 15);
        sk_fxm.ap2[sk_f.ap2p] = (int16_t)sk_clampi(y, -32767, 32767);
        x = t + ((y * 19660) >> 15);
        if (++sk_f.ap2p >= SK_AP2)
            sk_f.ap2p = 0;
        for (k = 0; k < SK_RVN; k++) {
            uint32_t rd = sk_f.rp[k] + 1u;                              /* the oldest sample: a line's length ago */
            int32_t v;
            if (k == 1u || k == 4u) {                                   /* these two read at a slowly moving place */
                int32_t m = (lfo * 14) >> 10;                           /* +-14 sixteenth samples... */
                uint32_t a = sk_f.rp[k] + 1u + (uint32_t)((m >> 4) + 16), fr = (uint32_t)m & 15u;
                int32_t s0 = sk_fxm.line[k][a % SK_RVL[k]], s1 = sk_fxm.line[k][(a + 1u) % SK_RVL[k]];
                v = s0 + (((s1 - s0) * (int32_t)fr) >> 4);
            } else {
                v = sk_fxm.line[k][rd % SK_RVL[k]];
            }
            sk_f.lpz[k] += sk_q15(v - sk_f.lpz[k], 32767 - sk_f.damp);  /* the damping */
            d[k] = sk_q15(sk_f.lpz[k], sk_f.g[k]);
        }
        /* a Hadamard mix of the eight */
        {
            int32_t a0 = d[0] + d[1], a1 = d[0] - d[1], a2 = d[2] + d[3], a3 = d[2] - d[3];
            int32_t a4 = d[4] + d[5], a5 = d[4] - d[5], a6 = d[6] + d[7], a7 = d[6] - d[7];
            int32_t b0 = a0 + a2, b1 = a1 + a3, b2 = a0 - a2, b3 = a1 - a3;
            int32_t b4 = a4 + a6, b5 = a5 + a7, b6 = a4 - a6, b7 = a5 - a7;
            d[0] = b0 + b4;
            d[1] = b1 + b5;
            d[2] = b2 + b6;
            d[3] = b3 + b7;
            d[4] = b0 - b4;
            d[5] = b1 - b5;
            d[6] = b2 - b6;
            d[7] = b3 - b7;
        }
        {
            int32_t outl = 0, outr = 0;
            for (k = 0; k < SK_RVN; k++) {
                int32_t w = ((d[k] * 11585) >> 15) + ((k & 1u) ? x : -x) / 2;     /* 1 / sqrt(8), plus the input */
                uint32_t wp = sk_f.rp[k];
                sk_fxm.line[k][wp] = (int16_t)sk_clampi(w, -32767, 32767);
                if (++wp >= SK_RVL[k])
                    wp = 0;
                sk_f.rp[k] = wp;
                if (k & 1u)
                    outr += (k & 2u) ? -sk_fxm.line[k][wp] : sk_fxm.line[k][wp];
                else
                    outl += (k & 2u) ? -sk_fxm.line[k][wp] : sk_fxm.line[k][wp];
            }
            ol[i] += sk_q15(outl, sk_f.wet);
            or_[i] += sk_q15(outr, sk_f.wet);
        }
    }
}

/* the delay: the busses' mono in, ping-pong out */
static void sk_delay(const int32_t *in, int32_t *ol, int32_t *or_, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        uint32_t d, ip, fr;
        int32_t a, b, la, lb;
        sk_f.dcur += (sk_f.dtgt - sk_f.dcur) >> 13;                    /* glides to a new time like a tape */
        d = (uint32_t)sk_f.dcur;
        ip = sk_f.dwp - (d >> 8);
        fr = d & 255u;
        a = sk_fxm.dl[0][(ip - 1u) & (SK_DLN - 1u)] * (int32_t)(256u - fr) + sk_fxm.dl[0][(ip - 2u) & (SK_DLN - 1u)] * (int32_t)fr;
        b = sk_fxm.dl[1][(ip - 1u) & (SK_DLN - 1u)] * (int32_t)(256u - fr) + sk_fxm.dl[1][(ip - 2u) & (SK_DLN - 1u)] * (int32_t)fr;
        a >>= 8;
        b >>= 8;
        sk_f.dlz[0] += sk_q15(a - sk_f.dlz[0], 14000);                 /* each echo a little darker */
        sk_f.dlz[1] += sk_q15(b - sk_f.dlz[1], 14000);
        la = in[i] + sk_q15(sk_f.dlz[1], sk_f.dfb);
        lb = sk_q15(sk_f.dlz[0], sk_f.dfb);
        sk_fxm.dl[0][sk_f.dwp & (SK_DLN - 1u)] = (int16_t)sk_clampi(la, -32767, 32767);
        sk_fxm.dl[1][sk_f.dwp & (SK_DLN - 1u)] = (int16_t)sk_clampi(lb, -32767, 32767);
        sk_f.dwp++;
        ol[i] += sk_q15(a, sk_f.dwet);
        or_[i] += sk_q15(b, sk_f.dwet);
    }
}

/* ---------------------------------------------------------------- the ensemble --- */
/* three taps of a short line, each read at a place that slowly sways: the left gets the first two, the right the last two */
static void gs_chorus(const int32_t *in, int32_t *ol, int32_t *or_, uint32_t n)
{
    int32_t mix = (int32_t)gs.p[P_CHOR], dry = 32767 - mix * 120;
    static const uint32_t RATE[3] = {38956u, 53600u, 70000u};       /* 0.4, 0.55 and 0.72 Hz: 2^32 a cycle */
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t x = in[i], t[3];
        uint32_t k;
        sk_fxm.ch[sk_f.chp & (GS_CHN - 1u)] = (int16_t)sk_clampi(x, -32767, 32767);
        for (k = 0; k < 3u; k++) {
            int32_t m = SK_SIN[(sk_f.chph[k] >> 22) & 1023u];
            int32_t dq = (int32_t)(560u + k * 90u) * 16 + ((m * (40 + mix)) >> 11);    /* 12 ms, swaying a few ms (1/16 samples) */
            uint32_t ip = sk_f.chp - (uint32_t)(dq >> 4);
            int32_t a = sk_fxm.ch[ip & (GS_CHN - 1u)], b = sk_fxm.ch[(ip - 1u) & (GS_CHN - 1u)];
            t[k] = a + (((b - a) * (dq & 15)) >> 4);
            sk_f.chph[k] += RATE[k];
        }
        sk_f.chp++;
        {
            int32_t wl = ((t[0] + t[1]) * mix) >> 8, wr = ((t[2] - t[1]) * mix) >> 8, d = sk_q15(x, dry);
            ol[i] = d + wl;
            or_[i] = d + wr;
        }
    }
}

/* ------------------------------------------------------------------- the glitch --- */
static struct {
    uint8_t on;                          /* a glitch is playing */
    uint8_t manual;                      /* REC is held */
    uint32_t len, rp, start;             /* the loop's length, the place in it, where it begins */
    uint32_t until;                      /* the auto glitch ends on this sixteenth */
    int32_t rate, pos;                   /* tape stop: the speed (Q16) and the read place (Q16) */
    int32_t mute;                        /* drop-out: the dry level, Q15 */
    uint32_t last_beat;
    uint8_t kind;
} gl;

static uint32_t gs_gl_len(void)
{
    static const uint16_t M[5] = {256, 128, 512, 64, 171};          /* of a sixteenth, x 1/256 */
    uint32_t t = (uint32_t)(((uint64_t)gs_s16() * M[gs.p[P_GLL] % 5u]) >> 8);
    while (t > GS_GLN - 512u)
        t >>= 1;
    return t < 200u ? 200u : t;
}

static void gs_gl_start(uint32_t kind)
{
    gl.on = 1;
    gl.kind = (uint8_t)kind;
    gl.len = gs_gl_len();
    gl.start = sk_f.gwp - gl.len;
    gl.rp = 0;
    gl.rate = 65536;
    gl.pos = (int32_t)(sk_f.gwp << 8);                            /* (tape stop reads from now) */
    gs.glitch = 1;
}
static void gs_gl_stop(void)
{
    gl.on = 0;
    gs.glitch = 0;
}

/* once a block: REC held, or the chance of one on a beat */
static void gs_gl_control(void)
{
    uint32_t kind = gs.p[P_GLT] & 3u;
    if (gl.manual) {
        if (!gl.on)
            gs_gl_start(kind);
        gl.until = 0;
    } else if (gl.on && gl.until == 0u)
        gs_gl_stop();
    if (gs.beat != gl.last_beat) {
        gl.last_beat = gs.beat;
        if (gl.on && gl.until && gs.beat >= gl.until && !gl.manual) {
            gl.until = 0;
            gs_gl_stop();
        }
        if (!gl.on && gs.p[P_GLP] && (gs.beat & 3u) == 0u && (sk_rand() & 127u) < (uint32_t)gs.p[P_GLP] / 2u) {
            gs_gl_start(kind);
            gl.until = gs.beat + 1u + (sk_rand() & 3u);
        }
    }
}

static void gs_glitch(int32_t *l, int32_t *r, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t a = l[i], b = r[i], ol = a, or_ = b;
        sk_fxm.gl[0][sk_f.gwp & (GS_GLN - 1u)] = (int16_t)sk_clampi(a, -32767, 32767);
        sk_fxm.gl[1][sk_f.gwp & (GS_GLN - 1u)] = (int16_t)sk_clampi(b, -32767, 32767);
        sk_f.gwp++;
        if (gl.on) {
            if (gl.kind == 2u) {                                   /* tape stop: the read place slows to nothing */
                uint32_t ip = (uint32_t)gl.pos >> 8, fr = (uint32_t)gl.pos & 255u;
                int32_t a0 = sk_fxm.gl[0][ip & (GS_GLN - 1u)], a1 = sk_fxm.gl[0][(ip + 1u) & (GS_GLN - 1u)];
                int32_t b0 = sk_fxm.gl[1][ip & (GS_GLN - 1u)], b1 = sk_fxm.gl[1][(ip + 1u) & (GS_GLN - 1u)];
                ol = a0 + (((a1 - a0) * (int32_t)fr) >> 8);
                or_ = b0 + (((b1 - b0) * (int32_t)fr) >> 8);
                gl.pos += (gl.rate >> 8);
                gl.rate -= gl.rate >> 11;
                if (gl.rate < 600)
                    gl.rate = 600;
                if (sk_f.gwp - ((uint32_t)gl.pos >> 8) > GS_GLN - 600u)
                    gl.pos = (int32_t)((sk_f.gwp - (GS_GLN - 600u)) << 8);
            } else if (gl.kind == 3u) {                            /* drop-out: the dry goes, the tails stay */
                gl.mute -= gl.mute >> 7;
                ol = sk_q15(a, gl.mute);
                or_ = sk_q15(b, gl.mute);
            } else {                                               /* the loop, forward or back, with a short fade at its ends */
                uint32_t pos = gl.kind == 1u ? gl.len - 1u - gl.rp : gl.rp, ip = (gl.start + pos) & (GS_GLN - 1u);
                int32_t f = 32767, e = (int32_t)(gl.rp < gl.len - gl.rp ? gl.rp : gl.len - gl.rp);
                if (e < 48)
                    f = e * 682;
                ol = sk_q15(sk_fxm.gl[0][ip], f) + sk_q15(a, 32767 - f);
                or_ = sk_q15(sk_fxm.gl[1][ip], f) + sk_q15(b, 32767 - f);
                if (++gl.rp >= gl.len)
                    gl.rp = 0;
            }
        } else if (gl.mute < 32767) {
            gl.mute += 40 + ((32767 - gl.mute) >> 9);               /* back in, over about 20 ms */
            if (gl.mute > 32767)
                gl.mute = 32767;
            ol = sk_q15(a, gl.mute);
            or_ = sk_q15(b, gl.mute);
        }
        l[i] = ol;
        r[i] = or_;
    }
}

/* ---------------------------------------------------------------- drive, crush --- */
static inline int32_t sk_soft(int32_t x)
{
    int32_t t, x3;
    x = sk_clampi(x, -49152, 49152);
    t = (x * x) >> 15;
    x3 = (t * x) >> 15;
    return x - ((x3 * 4855) >> 15);
}

/* a tanh-like fold for the drive: x in Q15, gain in Q8 */
static inline int32_t gs_sat(int32_t x, int32_t g)
{
    int32_t y = (int32_t)(((int64_t)x * g) >> 8);
    y = sk_clampi(y, -65535, 65535);
    {
        int32_t a = y < 0 ? -y : y, o;
        if (a < 21845)
            o = a;
        else if (a < 43690)
            o = 21845 + ((a - 21845) * 2 / 3);                       /* the knee */
        else
            o = 34000 + ((a - 43690) >> 3);
        return y < 0 ? -o : o;
    }
}


/* the SPACE of it: the echo and the reverb from the patch; each block */
static void gs_space(void)
{
    uint32_t i, rt = (uint32_t)gs.p[P_RSZ] * 31u / 127u;
    int32_t g0 = SK_RVG[rt < 31u ? rt : 31u];
    static const uint16_t DT[6] = {256, 512, 768, 1024, 341, 768};  /* the echo's time, of a sixteenth x 1/256 */
    uint32_t t;
    for (i = 0; i < SK_RVN; i++)
        sk_f.g[i] = 32767 - ((32767 - g0) * (int32_t)SK_RVL[i]) / 2400;
    sk_f.damp = 22000 - (int32_t)gs.p[P_RSZ] * 40;
    sk_f.wet = 1200 + (int32_t)gs.p[P_REV] * 190;
    sk_f.in = 7000;
    sk_f.dfb = 3000 + (int32_t)gs.p[P_DFB] * 150;                  /* 0.09 .. 0.65 */
    sk_f.dwet = (int32_t)gs.p[P_DLY] * 210;
    t = (uint32_t)(((uint64_t)gs_s16() * DT[gs.p[P_DTIM] % 6u]) >> 8);
    while (t >= SK_DLN - 64u)
        t >>= 1;
    sk_f.dtgt = (int32_t)(t << 8);
}

/* the fade of the whole output (the opening brings the sound in): the level now and where it is going, Q12 */
static uint32_t sk_fade = 4096, sk_fade_to = 4096;

/* one block: the voices, the gate, the effects, the master. out: interleaved L R, Q15 */
static void sk_render(int32_t *out, uint32_t n)
{
    int32_t m[SK_CTL], l[SK_CTL], r[SK_CTL], wl[SK_CTL], wr[SK_CTL], sd[SK_CTL], rv[SK_CTL];
    int32_t gprev, gnew, dg, drive = 256 + (int32_t)gs.p[P_DRV] * 12;
    uint32_t i, crush = gs.p[P_CRU];
    static int32_t chold, ccnt;
    if (!sk_f.ready) {
        sk_f.ready = 1;
        sk_f.dcur = sk_f.dtgt = (int32_t)(gs_s16() << 8);
        sk_f.master = 2800;
        gl.mute = 32767;
        gs.gate_g = 32767;
        gs.rmask = 0x5555u;
    }
    gprev = gs.gate_g;
    gs_tick(n);
    gs_gl_control();
    gs_space();
    if (sk_fade != sk_fade_to)
        sk_fade = sk_fade < sk_fade_to ? (sk_fade + 3u > sk_fade_to ? sk_fade_to : sk_fade + 3u) : (sk_fade < sk_fade_to + 3u ? sk_fade_to : sk_fade - 3u);
    gs_voices(m, n);
    gnew = gs.gate_g;
    dg = (gnew - gprev) / (int32_t)n;
    {
        uint32_t peak = 0;
        for (i = 0; i < n; i++) {
            int32_t x;
            gprev += dg;
            x = (int32_t)(((int64_t)m[i] * gprev) >> 15);
            x = (x * 9) >> 5;                                       /* the voices' sum, brought down */
            x = gs_sat(x, drive);
            if (crush) {                                            /* sample rate and bits */
                uint32_t hold = 1u + crush / 18u, sh = crush / 12u;
                if (ccnt <= 0) {
                    chold = x;
                    ccnt = (int32_t)hold;
                }
                ccnt--;
                x = (chold >> sh) << sh;
            }
            m[i] = x;
            {
                uint32_t a = (uint32_t)(x < 0 ? -x : x);
                if (a > peak)
                    peak = a;
            }
        }
        {
            int32_t lv = (int32_t)(peak >> 7);
            gs.level += ((lv > 255 ? 255 : lv) - gs.level) >> 2;
        }
    }
    gs_chorus(m, l, r, n);
    gs_glitch(l, r, n);
    for (i = 0; i < n; i++) {
        sd[i] = (l[i] + r[i]) >> 1;
        rv[i] = sd[i];
        wl[i] = wr[i] = 0;
    }
    sk_reverb(rv, wl, wr, n);
    sk_delay(sd, wl, wr, n);
    for (i = 0; i < n; i++) {
        int32_t a = l[i] + wl[i], b = r[i] + wr[i];
        sk_f.dc[0] += (a * 256 - sk_f.dc[0]) >> 10;
        sk_f.dc[1] += (b * 256 - sk_f.dc[1]) >> 10;
        a -= sk_f.dc[0] >> 8;
        b -= sk_f.dc[1] >> 8;
        sk_f.mlow[0] += sk_q15(a - sk_f.mlow[0], 23000);
        sk_f.mlow[1] += sk_q15(b - sk_f.mlow[1], 23000);
        sk_f.mlow2[0] += sk_q15(sk_f.mlow[0] - sk_f.mlow2[0], 23000);
        sk_f.mlow2[1] += sk_q15(sk_f.mlow[1] - sk_f.mlow2[1], 23000);
        a = sk_f.mlow2[0];
        b = sk_f.mlow2[1];
        a = (a * (int32_t)sk_f.master) >> 12;
        b = (b * (int32_t)sk_f.master) >> 12;
        a = (a * (int32_t)sk_fade) >> 12;
        b = (b * (int32_t)sk_fade) >> 12;
        out[2u * i] = sk_soft(a);
        out[2u * i + 1u] = sk_soft(b);
    }
}
