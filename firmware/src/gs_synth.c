/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GartStringer contributors */
/* GartStringer's voices: six voices of a supersaw synthesizer, with the things that make a trance lead and a trance bass.
 *
 *   oscillators  SUPERSAW (seven detuned saws, the middle one apart from the six), SYNC (saws hard-synced to a master),
 *                PWM (three pulses), BASS (a saw, a square an octave down and a detuned saw), FEEDBACK (a sine that
 *                modulates itself: sine to saw to rough)
 *   filter       a state-variable filter (Zavalishin's topology) in five kinds: 12 dB, 24 dB, band, high, VOWEL (two
 *                formant bands that move through a, e, i, o, u as the cutoff opens)
 *   envelopes    amplitude and filter, ADSR; one LFO to pitch, filter, amplitude, colour or detune
 *   gate         the trance gate: sixteen steps that chop the whole sound, with a smoothing that goes from clicks to swells
 *   arp          eight patterns on the notes held, one to four octaves, six rates, a gate length
 *   keys         poly (six voices), mono, or legato mono with glide; HOLD latches them
 *
 * Every number is an integer; the audio is Q15 in int32. One block is SK_CTL samples. Included by gartstringer.c. */

#include "sky_tables.h"

#define SK_CTL 32                        /* samples a block */
#define SK_FS 44100
#define GS_NV 6                          /* voices */
#define GS_NPRE 48                       /* factory patches: 32 trance, 16 house */
#define GS_NTRANCE 32                    /* the first house patch */
#define GS_NS 128                        /* the longest pattern: 128 steps */
#define GS_NP 8                          /* patterns A..H */

/* the patch: every sound parameter is one byte, 0..127 unless it is a choice */
enum {
    P_MODE, P_DET, P_MIX, P_SUB, P_COL, P_OCT,              /* oscillators: kind, detune, middle-to-sides, sub level, colour, octave 0..4 */
    P_CUT, P_RES, P_FENV, P_KEY, P_FTYPE, P_DRV,            /* filter: cutoff, resonance, envelope amount, key tracking, kind, drive */
    P_AA, P_AD, P_AS, P_AR, P_FA, P_FD, P_FS, P_FR,         /* envelopes: amplitude A D S R, filter A D S R */
    P_LRATE, P_LAMT, P_LDST, P_LSHP,                        /* LFO: rate, amount, destination, shape */
    P_GLIDE, P_VMODE,                                       /* glide, voice mode (poly, mono, legato) */
    P_CHOR, P_DLY, P_DTIM, P_DFB, P_REV, P_RSZ,             /* effects: ensemble, echo, its time and feedback, reverb, its size */
    P_GPAT, P_GRATE, P_GDEP, P_GSMO,                        /* trance gate: pattern, rate, depth, smoothing */
    P_APAT, P_ARATE, P_ARNG, P_AGATE, P_AON,                /* arpeggio: pattern, rate, octaves, gate length, on */
    P_GLP, P_GLL, P_GLT, P_CRU,                             /* glitch: chance, length, kind, crush */
    P_SRATE, P_SMET, P_SSWG, P_SGATE, P_TAPE,               /* sequencer: rate, metronome (off, soft, mid, loud), swing, gate length; and the tape (wow, flutter, a darker top) */
    P_CON, P_CKEY, P_CSCL, P_CLAY, P_CVOI,                  /* chords: on, key, scale, layout (top, top lock, side, side lock), voicing */
    P_N
};
#define GS_NMODE 8
#define GS_NFT 5
static const char *const GS_MODE_NAME[GS_NMODE] = {"SUPERSAW", "HYPERSYNC", "PWM STACK", "ROLL BASS", "FEEDBACK", "FM KEYS", "ORGAN", "DONK"};
static const char *const GS_FT_NAME[GS_NFT] = {"LOW 12", "LOW 24", "BAND", "HIGH", "VOWEL"};
static const char *const GS_LDST_NAME[5] = {"PITCH", "FILTER", "AMP", "COLOUR", "DETUNE"};
static const char *const GS_LSHP_NAME[4] = {"SINE", "TRI", "SQUARE", "S+H"};
static const char *const GS_VM_NAME[3] = {"POLY", "MONO", "LEGATO"};
static const char *const GS_DT_NAME[6] = {"1/16", "1/8", "DOT 1/8", "1/4", "1/8 T", "3/16"};
static const char *const GS_AP_NAME[8] = {"UP", "DOWN", "UP-DOWN", "RANDOM", "PLAYED", "CHORD", "BOUNCE", "ROLLING"};
static const char *const GS_AR_NAME[5] = {"1/8", "1/8 T", "1/16", "1/16 T", "1/32"};
static const char *const GS_GR_NAME[4] = {"1/8", "1/16", "1/32", "1/16 T"};
static const char *const GS_GL_NAME[4] = {"STUTTER", "REVERSE", "TAPE STOP", "DROP OUT"};
static const char *const GS_MET_NAME[4] = {"OFF", "SOFT", "MEDIUM", "LOUD"};
static const char *const GS_GLL_NAME[5] = {"1/16", "1/32", "1/8", "1/64", "1/16 T"};
#define GS_NPAT 16
#define GS_PUMP0 14                       /* the first pump: these duck on the beat and swell back, like a compressor keyed by a kick */
static const char *const GS_GT_STR[GS_NPAT] = {
    "..x...x...x...x.", "x.x.x.x.x.x.x.x.", "xx.xx.xx.xx.xx.x", "x.xx.xx.x.xx.xx.", "xxx.xxx.xxx.xx.x",
    "x..x..x.x..x..x.", "xxxx..xxxxxx..xx", "xx..xx..xx..xx..", "x..x.x..x..x.x..", "x.x..x..x.x..x..",
    "x...x.x.x...x.xx", "x.......xx......", "????????????????", "xxxxxxxxxxxxxxxx",
    ".xxx.xxx.xxx.xxx", ".x.x.x.x.x.x.x.x",
};
static const char *const GS_GT_NAME[GS_NPAT] = {"OFFBEAT", "BOUNCE", "ROLLING", "TRANCE A", "TRANCE B", "3-3-2", "STUTTER", "DOUBLE", "SYNCOPE", "SKIP", "SPARSE", "HAUNT", "RANDOM", "OPEN", "PUMP 1/4", "PUMP 1/8"};

/* the voice mode as played: the chord instrument needs every voice, so it plays polyphonic whatever the patch says */
#define GS_VM() ((uint32_t)(gs.p[P_CON] ? 0u : gs.p[P_VMODE]))

static const uint8_t GS_DEF[P_N] = {
    0, 54, 64, 20, 60, 2,   60, 40, 70, 30, 0, 12,
    4, 50, 110, 40, 8, 60, 40, 40,
    40, 0, 0, 0,   0, 0,
    60, 40, 2, 50, 40, 70,
    1, 1, 0, 40,   0, 2, 1, 90, 0,
    0, 0, 0, 0,
    1, 0, 0, 70, 0,
    0, 0, 1, 1, 0,
};

/* ------------------------------------------------------------------ helpers --- */
static inline int32_t sk_clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static uint32_t sk_rs = 0x2545F491u;
static uint32_t sk_rand(void)
{
    sk_rs ^= sk_rs << 13;
    sk_rs ^= sk_rs >> 17;
    sk_rs ^= sk_rs << 5;
    return sk_rs;
}

/* ------------------------------------------------------------------- state --- */
typedef struct {
    uint8_t on, gate, note, st_a, st_f, vel;
    uint32_t age;
    int32_t pitch;                       /* Q8 semitones, with glide */
    int32_t ae, fe;                      /* the envelope levels, Q16 */
    int32_t ga;                          /* the gain the last block ended on, Q16 */
    uint32_t ph[7], sub;
    int32_t fb[3];                       /* the feedback oscillators' last outputs */
    int32_t ic[4];                       /* the filter's states: stage 1 (two), stage 2 (two) */
    int32_t fr;
} gs_voice_t;

static struct {
    uint8_t p[P_N];
    uint8_t bpm;
    int8_t oct;                          /* performance octave, -2..+2 */
    uint8_t hold;
    uint8_t preset;                      /* the factory patch this started from */
    uint8_t user;                        /* the last slot loaded or saved, 0 = none */
    gs_voice_t v[GS_NV];
    uint32_t cut_idx;                    /* the last cutoff (Q8 of the table index) for the display */
    int32_t lfo;                         /* LFO now, Q15 */
    uint32_t lfo_ph;
    int32_t lfo_sh;
    int32_t gate_g, gate_t;              /* the trance gate's gain now and wanted, Q15 */
    uint8_t gstep, gon, greset;          /* the step and whether it is open; 1: start the clocks over */
    uint16_t rmask;                      /* the random pattern */
    uint32_t gacc;
    uint32_t aacc, aoff;                 /* the arp's clock and the note-off time */
    int8_t acur;                         /* the note it plays, -1 none */
    uint8_t astep, adir;
    uint8_t hn[8], nh, phys;             /* the notes held, in the order played; the keys really down */
    uint8_t chord[8], nchord;
    uint8_t sixteenth;                   /* counts 0..15 */
    uint32_t sacc;                       /* the sixteenth's clock */
    uint32_t beat;                       /* counts sixteenths since the start */
    int32_t level;                       /* the output's level, 0..255 (the visuals) */
    uint8_t glitch;                      /* 1 while a stutter or a stop is playing (set by the effects) */
    uint8_t note_t;                      /* a note on: the visuals flash (counts down) */
    /* the step sequencer: four patterns of up to 128 steps. A step is one byte: 0 a rest, else bits 0..5 the note + 1
     * (0..36, 12 = the root), bit 6 an accent, bit 7 a slide */
    uint8_t sp[GS_NP][GS_NS];
    uint8_t spl[GS_NP];                  /* each pattern's length, 1..128 */
    uint8_t pcur, pnext;                 /* the pattern playing (and shown), the one queued for the end of this pass (255 none) */
    uint8_t srec, scount;                /* recording armed; the count-in's sixteenths left (a bar and one) */
    uint8_t srun, sstep, sslide;         /* running, the step now (255 before the first), the step now holds into the next */
    uint8_t rec_step, rec_n;             /* the step last written by a key, and how many have been (the screen flashes it) */
    int8_t snote;                        /* the note sounding, -1 none */
    uint8_t sroot;                       /* the note a step of 12 plays (the keys move it while it runs) */
    uint32_t sclk, soff;                 /* samples into the step, and when its note ends */
    uint32_t wow_ph, flu_ph;             /* the tape: its slow wow and quick flutter */
    int32_t tape_pq;                     /* .. the pitch they add now, Q8 semitones */
    uint32_t met_ph, met_inc;            /* the metronome's click: phase, step */
    int32_t met_env;                     /* its envelope, Q15 */
} gs;

/* The clock. Every musical length is kept in 1/256 of a sample, so no tempo rounds to whole samples, and it is worked out from
 * the sample rate the hardware really runs at (gs_rate, in millihertz: the platform measures the converter against the 24 MHz
 * crystal and corrects it), so a bar lasts what the tempo says by the crystal, whatever the audio clock does. */
static uint32_t gs_rate = 44100000u;                               /* samples a second, x1000 */
#define GS_Q 8                                                     /* the clocks' fraction bits */
static uint32_t gs_s16(void)                                       /* a sixteenth, in 1/256 samples */
{
    uint32_t bpm = gs.bpm < 40u ? 40u : gs.bpm;
    return (gs_rate * 96u) / (25u * bpm);                           /* rate/1000 * 15/bpm * 256 (the product stays under 2^32 up to 44.7 kHz) */
}
static uint32_t gs_s16s(void) { return gs_s16() >> GS_Q; }          /* .. in whole samples (the echo, the glitch) */

/* ---------------------------------------------------------------- voices --- */
static const int32_t GS_DT[7] = {-7210, -4121, -1279, 0, 1305, 4074, 7042};     /* a supersaw's seven detunes, as a fraction of the pitch (Q16) */

static void gs_voice_on(uint32_t i, uint32_t note, uint32_t vel, int retrig)
{
    gs_voice_t *v = &gs.v[i];
    uint32_t k;
    if (v->on && (retrig || v->st_a == 4u)) {
        v->st_a = 1;
        v->st_f = 1;
    }
    if (!v->on) {
        v->st_a = 1;
        v->st_f = 1;
        {
            v->ae = 0;
            v->fe = 0;
            v->ga = 0;
            for (k = 0; k < 7u; k++)
                v->ph[k] = sk_rand();                    /* free-running: the saws start anywhere */
            for (k = 0; k < 4u; k++)
                v->ic[k] = 0;
            v->fb[0] = v->fb[1] = v->fb[2] = 0;
            v->pitch = (int32_t)note << 8;
        }
    }
    v->on = 1;
    v->gate = 1;
    v->note = (uint8_t)note;
    v->vel = (uint8_t)vel;
    v->age = 0;
    if (!gs.p[P_GLIDE] || !v->ae)
        v->pitch = (int32_t)note << 8;
    gs.note_t = 8;
}

static void gs_voice_off(uint32_t i)
{
    gs_voice_t *v = &gs.v[i];
    if (v->on && v->gate) {
        v->gate = 0;
        v->st_a = 4;
        v->st_f = 4;
    }
}

static uint32_t gs_alloc(void)
{
    uint32_t i, best = 0, bage = 0;
    for (i = 0; i < GS_NV; i++)
        if (!gs.v[i].on)
            return i;
    for (i = 0; i < GS_NV; i++) {                               /* steal: a released voice first, then the oldest */
        uint32_t a = gs.v[i].age + (gs.v[i].gate ? 0u : 1000000u);
        if (a >= bage) {
            bage = a;
            best = i;
        }
    }
    return best;
}

/* a note to the voices (the arp and the keys both end here) */
static void gs_note_on(uint32_t note, uint32_t vel)
{
    uint32_t i, any = 0;
    if (GS_VM() == 0u) {
        for (i = 0; i < GS_NV; i++)                              /* the same note again: retrigger it */
            if (gs.v[i].on && gs.v[i].gate && gs.v[i].note == note) {
                gs_voice_on(i, note, vel, 1);
                return;
            }
        gs_voice_on(gs_alloc(), note, vel, 1);
        return;
    }
    for (i = 1; i < GS_NV; i++)
        gs_voice_off(i);
    any = gs.v[0].on && gs.v[0].gate;
    gs_voice_on(0, note, vel, !(GS_VM() == 2u && any));
}

static void gs_note_off(uint32_t note)
{
    uint32_t i;
    if (GS_VM() == 0u) {
        for (i = 0; i < GS_NV; i++)
            if (gs.v[i].on && gs.v[i].gate && gs.v[i].note == note)
                gs_voice_off(i);
        return;
    }
    if (gs.v[0].on && gs.v[0].gate && gs.v[0].note == note)
        gs_voice_off(0);
}

static void gs_all_off(void)
{
    uint32_t i;
    for (i = 0; i < GS_NV; i++)
        gs_voice_off(i);
}

/* one-pole step of an envelope toward a target, Q16, GS_K the rate */
static inline int32_t gs_ease(int32_t e, int32_t target, uint32_t k) { return e + (int32_t)(((int64_t)(target - e) * (int32_t)k) >> 16); }

static void gs_env(gs_voice_t *v, int32_t *e, uint8_t *st, uint32_t a, uint32_t d, uint32_t s, uint32_t r)
{
    int32_t sus = (int32_t)s * 516;                      /* 0..65532 */
    switch (*st) {
    case 1:
        *e = gs_ease(*e, 75000, GS_K[a]);
        if (*e >= 65536) {
            *e = 65536;
            *st = 2;
        }
        break;
    case 2:
        *e = gs_ease(*e, sus, GS_K[d]);
        break;
    case 4:
        *e = gs_ease(*e, 0, GS_K[r]);
        break;
    default:
        break;
    }
    (void)v;
}

/* the filter: one TPT state-variable step, Q16 coefficients; outputs low in *lo, band in *bd */
static inline void gs_svf(int32_t x, int32_t *ic, int32_t a1, int32_t a2, int32_t a3, int32_t *lo, int32_t *bd)
{
    int32_t v3 = x - ic[1];
    int32_t v1 = (int32_t)(((int64_t)a1 * ic[0] + (int64_t)a2 * v3) >> 16);
    int32_t v2 = ic[1] + (int32_t)(((int64_t)a2 * ic[0] + (int64_t)a3 * v3) >> 16);
    ic[0] = sk_clampi(2 * v1 - ic[0], -(1 << 23), 1 << 23);
    ic[1] = sk_clampi(2 * v2 - ic[1], -(1 << 23), 1 << 23);
    *lo = v2;
    *bd = v1;
}

static void gs_coef(uint32_t g, uint32_t k, int32_t *a1, int32_t *a2, int32_t *a3)
{
    uint32_t den = 65536u + (uint32_t)(((uint64_t)g * (g + k)) >> 16);
    uint32_t x1 = 0xFFFFFFFFu / den;
    *a1 = (int32_t)x1;
    *a2 = (int32_t)(((uint64_t)g * x1) >> 16);
    *a3 = (int32_t)(((uint64_t)g * (uint32_t)*a2) >> 16);
}

/* the five vowels as pairs of formants: g = tan(pi f / fs) in Q16 (a 800/1200, e 530/1840, i 270/2140, o 500/900, u 325/700) */
static const uint16_t GS_VOW[5][2] = {{3739, 5616}, {2476, 8640}, {1261, 10070}, {2336, 4207}, {1517, 3271}};

/* LFO, once a block; Q15 */
static void gs_lfo_step(void)
{
    uint32_t sh = gs.p[P_LSHP], ph;
    uint32_t old = gs.lfo_ph;
    gs.lfo_ph += GS_LR[gs.p[P_LRATE]];
    ph = gs.lfo_ph;
    switch (sh) {
    case 0: gs.lfo = SK_SIN[(ph >> 22) & 1023u]; break;
    case 1: {
        int32_t t = (int32_t)(ph >> 16);
        gs.lfo = (t < 32768 ? t : 65535 - t) * 2 - 32768;
        break;
    }
    case 2: gs.lfo = ph < 0x80000000u ? 28000 : -28000; break;
    default:
        if (ph < old)
            gs.lfo_sh = (int32_t)(sk_rand() >> 17) - 16384;
        gs.lfo = gs.lfo_sh * 2;
        break;
    }
    if (!gs.p[P_LAMT])
        gs.lfo = 0;
    /* the tape: a slow wow and a quick flutter on every voice's pitch (up to about a third of a semitone) */
    gs.wow_ph += 1714000u;                                         /* 0.55 Hz at the block rate */
    gs.flu_ph += 19000000u;                                        /* 6.1 Hz */
    gs.tape_pq = gs.p[P_TAPE] ? (int32_t)((SK_SIN[(gs.wow_ph >> 22) & 1023u] * ((int32_t)gs.p[P_TAPE] * 90 / 127) +
                                           SK_SIN[(gs.flu_ph >> 22) & 1023u] * ((int32_t)gs.p[P_TAPE] * 12 / 127)) >> 15) : 0;
}

static inline int32_t gs_pitch_inc(int32_t pq8, uint32_t *inc, uint32_t *base)
{
    int32_t n = pq8 >> 8, fr = pq8 & 255;
    uint32_t a, b;
    if (n < 0) {
        n = 0;
        fr = 0;
    }
    if (n > 118) {
        n = 118;
        fr = 0;
    }
    a = SK_INC[n];
    b = SK_INC[n + 1];
    *inc = a + (uint32_t)(((uint64_t)(b - a) * (uint32_t)fr) >> 8);
    *base = *inc;
    return n;
}

/* the voices of a block into mix[] (Q15, about +-12000 a voice) */
static void gs_voices(int32_t *mix, uint32_t n)
{
    const uint8_t *p = gs.p;
    uint32_t vi, i, k;
    int32_t lfo = gs.lfo, amt = p[P_LAMT];
    uint32_t dst = p[P_LDST];
    for (i = 0; i < n; i++)
        mix[i] = 0;
    for (vi = 0; vi < GS_NV; vi++) {
        gs_voice_t *v = &gs.v[vi];
        int32_t tgt, vib = 0, cutq, col, det, mixv, ga, dga, ga_new;
        uint32_t inc, base, finc[7], thr = 0;
        int32_t a1, a2, a3, a1b = 0, a2b = 0, a3b = 0, kk, kv = 0;
        uint32_t fmi = 0, fmt = 0;                              /* the FM kinds' modulation depths this block */
        uint32_t ftype = p[P_FTYPE];
        if (!v->on)
            continue;
        v->age++;
        /* envelopes */
        gs_env(v, &v->ae, &v->st_a, p[P_AA], p[P_AD], p[P_AS], p[P_AR]);
        gs_env(v, &v->fe, &v->st_f, p[P_FA], p[P_FD], p[P_FS], p[P_FR]);
        if (v->st_a == 4u && v->ae < 40) {
            v->on = 0;
            v->ae = 0;
            continue;
        }
        /* glide, then the pitch with the LFO and the octave */
        tgt = (int32_t)v->note << 8;
        if (p[P_GLIDE])
            v->pitch += (int32_t)(((int64_t)(tgt - v->pitch) * (int32_t)GS_K[p[P_GLIDE] / 2u + 30u]) >> 16);
        else
            v->pitch = tgt;
        if (dst == 0u)
            vib = (lfo * amt) >> 13;
        gs_pitch_inc(v->pitch + vib + gs.tape_pq + ((int32_t)p[P_OCT] - 2) * 12 * 256, &inc, &base);
        /* colour, detune */
        col = p[P_COL];
        det = p[P_DET];
        if (dst == 3u)
            col = sk_clampi(col + ((lfo * amt) >> 16), 0, 127);
        if (dst == 4u)
            det = sk_clampi(det + ((lfo * amt) >> 16), 0, 127);
        mixv = p[P_MIX];
        /* the filter */
        cutq = (int32_t)p[P_CUT] * 256;
        cutq += (int32_t)(((int64_t)v->fe * p[P_FENV] * 807) >> 18);
        cutq += (int32_t)(((int64_t)(v->pitch - 15360) * p[P_KEY] * 142) >> 14);
        if (dst == 1u)
            cutq += (lfo * amt) >> 9;
        cutq = sk_clampi(cutq, 0, 127 * 256 - 1);
        gs.cut_idx = (uint32_t)cutq;
        {
            uint32_t ci = (uint32_t)cutq >> 8, fr = (uint32_t)cutq & 255u;
            uint32_t g = GS_G[ci] + (uint32_t)(((uint64_t)(GS_G[ci + 1u] - GS_G[ci]) * fr) >> 8);
            uint32_t kd = GS_D[p[P_RES]];
            kk = (int32_t)kd;
            gs_coef(g, kd, &a1, &a2, &a3);
            if (ftype == 1u)
                gs_coef(g, 65536u, &a1b, &a2b, &a3b);
            else if (ftype == 4u) {                           /* the vowel: the cutoff walks a e i o u */
                uint32_t pos = (uint32_t)cutq * 4u * 256u / (127u * 256u), vv = pos >> 8, vf = pos & 255u;
                uint32_t g1, g2;
                if (vv > 3u) {
                    vv = 3u;
                    vf = 255u;
                }
                g1 = GS_VOW[vv][0] + (uint32_t)(((int32_t)GS_VOW[vv + 1u][0] - (int32_t)GS_VOW[vv][0]) * (int32_t)vf >> 8);
                g2 = GS_VOW[vv][1] + (uint32_t)(((int32_t)GS_VOW[vv + 1u][1] - (int32_t)GS_VOW[vv][1]) * (int32_t)vf >> 8);
                kv = kd / 2u + 4000u;
                gs_coef(g1, (uint32_t)kv, &a1, &a2, &a3);
                gs_coef(g2, (uint32_t)kv, &a1b, &a2b, &a3b);
            }
        }
        /* amplitude of this block, with the tremolo */
        ga_new = (int32_t)(((int64_t)v->ae * (int32_t)v->vel) >> 7);
        if (dst == 2u) {
            int32_t uni = (lfo + 32768) >> 1;                 /* 0..32767 */
            ga_new = (int32_t)(((int64_t)ga_new * (32768 - (int32_t)(((int64_t)amt * (32767 - uni)) >> 7))) >> 15);
        }
        ga = v->ga;
        dga = (ga_new - ga) / (int32_t)n;
        /* the oscillators' steps */
        switch (p[P_MODE]) {
        case 0: {                                                /* supersaw */
            for (k = 0; k < 7u; k++)
                finc[k] = inc + (uint32_t)(int32_t)(((int64_t)((int64_t)inc * GS_DT[k] >> 16) * det) >> 7);
            break;
        }
        case 1: {                                                /* hypersync: masters detuned, slaves at a ratio */
            uint32_t ratio = 65536u + (uint32_t)col * 2u * 1024u;       /* 1 .. 4 */
            for (k = 0; k < 3u; k++) {
                int32_t d = k == 0u ? 0 : (k == 1u ? GS_DT[1] : GS_DT[5]);
                finc[k] = inc + (uint32_t)(int32_t)(((int64_t)((int64_t)inc * d >> 16) * det) >> 7);
                finc[3u + k] = (uint32_t)(((uint64_t)finc[k] * ratio) >> 16);
            }
            break;
        }
        case 2: {                                                /* pwm stack */
            for (k = 0; k < 3u; k++) {
                int32_t d = k == 0u ? 0 : (k == 1u ? GS_DT[1] : GS_DT[5]);
                finc[k] = inc + (uint32_t)(int32_t)(((int64_t)((int64_t)inc * d >> 16) * det) >> 7);
            }
            thr = 214748364u + (uint32_t)col * 14500000u;
            if (dst == 3u)
                thr = 214748364u + (uint32_t)sk_clampi(col, 0, 127) * 14500000u;
            break;
        }
        case 3:                                                  /* roll bass */
            finc[0] = inc;
            finc[1] = inc + (uint32_t)(((uint64_t)inc * (uint32_t)det) >> 11);
            finc[2] = inc >> 1;
            break;
        case 5: {                                                /* FM keys: two tines a little apart, each a sine bent by a sine; the bend dies with the filter envelope */
            uint32_t e = 16384u + (uint32_t)((v->fe * 3) >> 2), e2 = (uint32_t)(((int64_t)v->fe * v->fe) >> 16);
            finc[0] = inc;
            finc[1] = inc + (uint32_t)(((uint64_t)inc * (uint32_t)det) >> 14);
            finc[2] = finc[0];
            finc[3] = finc[1];
            finc[4] = inc * 14u;                                 /* the bell of the hammer */
            fmi = ((uint32_t)col * 700u * e) >> 16;
            fmt = ((uint32_t)mixv * 300u * e2) >> 16;
            break;
        }
        case 6:                                                  /* organ: drawbars at 1, 2, 3 and 4 times the note, and a second pair a little off */
            finc[0] = inc;
            finc[1] = inc * 2u;
            finc[2] = inc * 3u;
            finc[3] = inc * 4u;
            finc[4] = inc + (uint32_t)(((uint64_t)inc * (uint32_t)det) >> 14);
            finc[5] = finc[4] * 2u;
            fmi = (uint32_t)((v->fe >> 9) * mixv) >> 7;          /* the key click: the third drawbar pushed while the filter envelope is up */
            break;
        case 7: {                                                /* donk: a sine struck by a sine at a ratio; the strike falls with the filter envelope */
            static const uint16_t RT[8] = {256, 512, 768, 896, 1024, 1280, 1792, 2304};
            uint32_t e = 6000u + (uint32_t)(((int64_t)v->fe * 59536) >> 16);
            finc[0] = inc;
            finc[1] = inc + (uint32_t)(((uint64_t)inc * (uint32_t)det) >> 13);
            finc[2] = (uint32_t)(((uint64_t)inc * RT[(col >> 4) & 7]) >> 8);
            fmi = ((uint32_t)mixv * 1000u * e) >> 16;
            break;
        }
        default:                                                 /* feedback */
            for (k = 0; k < 3u; k++) {
                int32_t d = k == 0u ? 0 : (k == 1u ? GS_DT[1] : GS_DT[5]);
                finc[k] = inc + (uint32_t)(int32_t)(((int64_t)((int64_t)inc * d >> 16) * det) >> 8);
            }
            break;
        }
        {
            int32_t cg = 26000 - mixv * 130, sg = 1800 + mixv * 50;
            int32_t subg = (int32_t)p[P_SUB] * 110;
            uint32_t subinc = base >> 1;
            for (i = 0; i < n; i++) {
                int32_t s = 0, x, lo, bd, lo2, bd2;
                switch (p[P_MODE]) {
                case 0: {
                    int32_t sum = 0;
                    for (k = 0; k < 7u; k++) {
                        v->ph[k] += finc[k];
                        if (k != 3u)
                            sum += (int32_t)(v->ph[k] >> 16) - 32768;
                    }
                    s = (int32_t)(((int64_t)cg * ((int32_t)(v->ph[3] >> 16) - 32768) + (int64_t)sg * sum) >> 15);
                    break;
                }
                case 1: {
                    int32_t sum = 0;
                    for (k = 0; k < 3u; k++) {
                        uint32_t o = v->ph[k];
                        v->ph[k] += finc[k];
                        if (v->ph[k] < o)                          /* the master wrapped: the slave restarts, scaled */
                            v->ph[3u + k] = (uint32_t)(((uint64_t)v->ph[k] * (65536u + (uint32_t)col * 2048u)) >> 16);
                        v->ph[3u + k] += finc[3u + k];
                        sum += (int32_t)(v->ph[3u + k] >> 16) - 32768;
                    }
                    s = (sum * (6000 + mixv * 40)) >> 15;
                    break;
                }
                case 2: {
                    int32_t sum = 0;
                    for (k = 0; k < 3u; k++) {
                        v->ph[k] += finc[k];
                        sum += v->ph[k] < thr ? 24000 : -24000;
                    }
                    s = (sum * (8000 + mixv * 60)) >> 15;
                    break;
                }
                case 3: {
                    int32_t a, b, q;
                    v->ph[0] += finc[0];
                    v->ph[1] += finc[1];
                    v->ph[2] += finc[2];
                    a = (int32_t)(v->ph[0] >> 16) - 32768;
                    b = (int32_t)(v->ph[1] >> 16) - 32768;
                    q = v->ph[2] < 0x80000000u ? 28000 : -28000;
                    s = (int32_t)(((int64_t)a * (127 - col) + (int64_t)q * col) >> 7);
                    s = (s * 17000 + b * (mixv * 70)) >> 15;
                    break;
                }
                case 5: {
                    int32_t m1, m2, mt;
                    for (k = 0; k < 5u; k++)
                        v->ph[k] += finc[k];
                    m1 = SK_SIN[(v->ph[2] >> 22) & 1023u];
                    m2 = SK_SIN[(v->ph[3] >> 22) & 1023u];
                    mt = SK_SIN[(v->ph[4] >> 22) & 1023u];
                    s = (SK_SIN[((v->ph[0] + (uint32_t)m1 * fmi) >> 22) & 1023u] * 12000 + SK_SIN[((v->ph[1] + (uint32_t)m2 * fmi) >> 22) & 1023u] * 12000 +
                         SK_SIN[((v->ph[0] + (uint32_t)mt * fmt) >> 22) & 1023u] * 5000) >> 15;
                    break;
                }
                case 6: {
                    int32_t w2 = 30 + col * 3 / 4, w3 = col + (int32_t)fmi, w4 = col > 50 ? col - 50 : 0, sum;
                    for (k = 0; k < 6u; k++)
                        v->ph[k] += finc[k];
                    sum = SK_SIN[(v->ph[0] >> 22) & 1023u] * 120 + SK_SIN[(v->ph[1] >> 22) & 1023u] * w2 + SK_SIN[(v->ph[2] >> 22) & 1023u] * w3 +
                          SK_SIN[(v->ph[3] >> 22) & 1023u] * w4 + ((SK_SIN[(v->ph[4] >> 22) & 1023u] * 120 + SK_SIN[(v->ph[5] >> 22) & 1023u] * w2) >> 1);
                    s = (int32_t)(((int64_t)sum * 62) >> 15);
                    break;
                }
                case 7: {
                    int32_t m;
                    v->ph[0] += finc[0];
                    v->ph[1] += finc[1];
                    v->ph[2] += finc[2];
                    m = SK_SIN[(v->ph[2] >> 22) & 1023u];
                    s = ((SK_SIN[((v->ph[0] + (uint32_t)m * fmi) >> 22) & 1023u] + SK_SIN[((v->ph[1] + (uint32_t)m * fmi) >> 22) & 1023u]) * 14000) >> 15;
                    break;
                }
                default: {
                    int32_t sum = 0;
                    for (k = 0; k < 3u; k++) {
                        uint32_t ph = v->ph[k] + (uint32_t)(v->fb[k] * (col << 9));
                        v->ph[k] += finc[k];
                        v->fb[k] = SK_SIN[(ph >> 22) & 1023u];
                        sum += v->fb[k] * (k == 0u ? 14000 : 5000 + mixv * 20) >> 15;
                    }
                    s = sum;
                    break;
                }
                }
                if (subg) {
                    v->sub += subinc;
                    s += (int32_t)(((int64_t)(v->sub < 0x80000000u ? 28000 : -28000) * subg) >> 15) >> 1;
                }
                /* the filter */
                x = s;
                if (ftype == 4u) {
                    gs_svf(x, &v->ic[0], a1, a2, a3, &lo, &bd);
                    gs_svf(x, &v->ic[2], a1b, a2b, a3b, &lo2, &bd2);
                    x = (int32_t)(((int64_t)(bd + bd2) * kv * 5) >> 18);                       /* (each band normalised, then a little up) */
                } else if (ftype == 2u) {
                    gs_svf(x, &v->ic[0], a1, a2, a3, &lo, &bd);
                    x = (int32_t)(((int64_t)bd * kk) >> 15);
                } else if (ftype == 3u) {
                    gs_svf(x, &v->ic[0], a1, a2, a3, &lo, &bd);
                    x = x - (int32_t)(((int64_t)bd * kk) >> 16) - lo;
                } else {
                    gs_svf(x, &v->ic[0], a1, a2, a3, &lo, &bd);
                    x = lo;
                    if (ftype == 1u) {
                        gs_svf(x, &v->ic[2], a1b, a2b, a3b, &lo2, &bd2);
                        x = lo2;
                    }
                }
                ga += dga;
                mix[i] += (int32_t)(((int64_t)x * ga) >> 16);
            }
        }
        v->ga = ga_new;
    }
}

/* ---------------------------------------------------------- clock, gate, arp --- */
static void gs_arp_list(void)
{
    uint32_t i, j;
    gs.nchord = gs.nh;
    for (i = 0; i < gs.nh; i++)
        gs.chord[i] = gs.hn[i];
    for (i = 1; i < gs.nchord; i++) {                              /* sorted, lowest first */
        uint8_t t = gs.chord[i];
        for (j = i; j > 0 && gs.chord[j - 1] > t; j--)
            gs.chord[j] = gs.chord[j - 1];
        gs.chord[j] = t;
    }
}

static const int8_t GS_ROLL[8] = {0, 0, 12, 0, 0, 12, 0, 7};

static void gs_arp_step(void)
{
    uint32_t pat = gs.p[P_APAT], nr = gs.p[P_ARNG] ? gs.p[P_ARNG] : 1u, total, idx, oct, n;
    int note;
    if (gs.acur >= 0) {
        gs_note_off((uint32_t)gs.acur);
        gs.acur = -1;
    }
    if (!gs.nh)
        return;
    gs_arp_list();
    n = gs.nchord;
    total = n * nr;
    if (pat == 5u) {                                               /* chord: all together */
        uint32_t i;
        for (i = 0; i < n; i++)
            gs_voice_on(gs_alloc(), (uint32_t)gs.chord[i], 110u, 1);
        gs.acur = -1;
        return;
    }
    if (pat == 6u) {                                               /* bounce: the lowest note and its octave */
        note = gs.chord[0] + ((gs.astep & 1u) ? 12 : 0);
        gs.astep++;
    } else if (pat == 7u) {                                        /* rolling: the lowest note's bass line */
        note = gs.chord[0] + GS_ROLL[gs.astep & 7u];
        gs.astep++;
    } else {
        if (pat == 3u) {
            idx = sk_rand() % total;
        } else if (pat == 1u) {
            idx = total - 1u - (gs.astep % total);
            gs.astep++;
        } else if (pat == 2u) {
            uint32_t per = total > 1u ? 2u * total - 2u : 1u, ph = gs.astep % per;
            idx = ph < total ? ph : per - ph;
            gs.astep++;
        } else {
            idx = gs.astep % total;
            gs.astep++;
        }
        oct = idx / n;
        idx = idx % n;
        note = (pat == 4u ? gs.hn[idx] : gs.chord[idx]) + 12 * (int)oct;
    }
    note = note > 120 ? 120 : note;
    gs.acur = (int8_t)note;
    gs_note_on((uint32_t)note, gs.astep % 4u == 1u ? 127u : 104u);
}

static uint32_t gs_arp_int(void)                                   /* between arp steps, in 1/256 samples */
{
    static const uint16_t SIXT[5] = {512, 341, 256, 171, 128};     /* in 1/256 of a sixteenth * 2: 1/8 = 2 sixteenths ... */
    return (uint32_t)(((uint64_t)gs_s16() * SIXT[gs.p[P_ARATE] % 5u]) >> 8) >> 0;
}

static const uint16_t GS_GRATE[4] = {512, 256, 128, 171};          /* x 1/256 of a sixteenth */


/* ------------------------------------------------------------- the sequencer --- */
#define SP_ON(b) (((b) & 63u) != 0u)
#define SP_NOTE(b) ((int)((b) & 63u) - 1)
#define SP_ACC(b) (((b) & 64u) != 0u)
#define SP_SLD(b) (((b) & 128u) != 0u)
#define SP_MAKE(n, acc, sld) ((uint8_t)(((n) + 1) | ((acc) ? 64 : 0) | ((sld) ? 128 : 0)))
#define CK_LO 121739375u                                           /* 1250 Hz per sample, Q32 */
#define CK_HI 175304700u                                           /* 1800 Hz: the first beat of a bar */

/* the metronome: a short pitched tick (not a drum), on the beats while the sequence runs; recording makes it at least medium */
static uint32_t gs_met_level(void)
{
    uint32_t m = gs.p[P_SMET];
    if (gs.srec && m < 2u)
        m = 2u;
    return m;
}
static void gs_click(uint32_t strong)
{
    gs.met_inc = strong ? CK_HI : CK_LO;
    gs.met_ph = 0;
    gs.met_env = 32767;
}
static int32_t gs_click_sample(void)                               /* one sample of the tick, Q15-ish */
{
    int32_t t, v;
    uint32_t m;
    if (gs.met_env < 64)
        return 0;
    gs.met_ph += gs.met_inc;
    t = (int32_t)(gs.met_ph >> 17) - 16384;                        /* a triangle -16384..16383 */
    t = t < 0 ? -t : t;
    t = (t - 8192) * 2;
    gs.met_env -= (gs.met_env >> 8) + 24;                          /* about 25 ms */
    m = gs_met_level();
    v = (int32_t)(((int64_t)t * gs.met_env) >> 15);
    return (v * (int32_t)(m * 2u + 2u)) >> 4;
}

static void gs_seq_stop(void)
{
    gs.srun = 0;
    gs.scount = 0;
    gs.srec = 0;
    if (gs.snote >= 0)
        gs_note_off((uint32_t)gs.snote);
    gs.snote = -1;
    gs.sslide = 0;
    gs.pnext = 255;
}

/* start: with recording armed a bar of clicks comes first (the sequence waits for its downbeat) */
static void gs_seq_start(void)
{
    gs.srun = 1;
    gs.sstep = 255;                                                /* the first tick plays step 1 */
    gs.sclk = 0;
    gs.snote = -1;
    gs.sslide = 0;
    gs.pnext = 255;
    gs.scount = gs.srec ? 17u : 0u;
    gs.greset = 1;                                                 /* the gate and the clock start with it */
}

/* a pattern to play: at once if stopped, at the end of this pass if running */
static void gs_pat_select(uint32_t p)
{
    if (p >= GS_NP)
        return;
    if (!gs.srun) {
        gs.pcur = (uint8_t)p;
        gs.pnext = 255;
    } else
        gs.pnext = p == gs.pcur ? 255u : (uint8_t)p;
}

static uint32_t gs_seq_len(void)                                   /* how long the step now playing lasts, in 1/256 samples: the rate, the swing on the odd ones */
{
    uint32_t base = (uint32_t)(((uint64_t)gs_s16() * GS_GRATE[gs.p[P_SRATE] & 3u]) >> 8), sw = (uint32_t)gs.p[P_SSWG] * 30u / 127u;
    base = base / 100u * ((gs.sstep & 1u) ? 100u - sw : 100u + sw) + base % 100u * ((gs.sstep & 1u) ? 100u - sw : 100u + sw) / 100u;
    return base < (64u << GS_Q) ? (64u << GS_Q) : base;
}

static void gs_seq_step(void)
{
    uint32_t len = gs.spl[gs.pcur], s, f;
    int old = gs.snote, oldslide = gs.sslide;
    if (len < 1u || len > GS_NS)
        len = GS_NS;
    if (gs.sstep == 255u)
        s = 0u;
    else
        s = gs.sstep + 1u >= len ? 0u : gs.sstep + 1u;
    if (s == 0u && gs.pnext != 255u) {                             /* the end of the pass: the queued pattern starts */
        gs.pcur = gs.pnext;
        gs.pnext = 255;
    }
    gs.sstep = (uint8_t)s;
    f = gs.sp[gs.pcur][s];
    {                                                              /* (the clock is the caller's: what ran over the last step counts toward this one) */
        uint32_t cur = gs_seq_len();
        gs.soff = (uint32_t)(((uint64_t)cur * (uint32_t)(8 + gs.p[P_SGATE])) >> 7);
        if (gs.soff > cur - (8u << GS_Q))
            gs.soff = cur - (8u << GS_Q);                           /* (a note ends before the next step starts) */
    }
    gs.sslide = (uint8_t)(SP_ON(f) && SP_SLD(f));
    if (SP_ON(f)) {
        int note = (int)gs.sroot + SP_NOTE(f) - 12;
        note = note < 0 ? 0 : note > 120 ? 120 : note;
        gs_note_on((uint32_t)note, SP_ACC(f) ? 127u : 92u);
        if (old >= 0 && !(oldslide && old == note))                /* the note before ends as this one starts (a slide into the same note just goes on) */
            gs_note_off((uint32_t)old);
        gs.snote = (int8_t)note;
    } else {
        if (old >= 0)
            gs_note_off((uint32_t)old);
        gs.snote = -1;
    }
}

/* a key played while recording: it is written on the step nearest in time, and sounds as usual */
static void gs_seq_rec(uint32_t note)
{
    uint32_t len = gs.spl[gs.pcur], s;
    int n;
    if (len < 1u || len > GS_NS)
        len = GS_NS;
    if (gs.scount || gs.sstep == 255u)
        s = 0u;
    else {
        s = gs.sstep;
        if (gs.sclk * 2u >= gs_seq_len())
            s = s + 1u >= len ? 0u : s + 1u;
    }
    n = (int)note - ((int)gs.sroot - 12);
    n = n < 0 ? 0 : n > 36 ? 36 : n;
    gs.sp[gs.pcur][s] = SP_MAKE(n, 0, 0);
    gs.rec_step = (uint8_t)s;
    gs.rec_n++;
}

static void gs_tick(uint32_t n)
{
    uint32_t s16 = gs_s16(), step_len, nq = n << GS_Q;
    gs.sacc += nq;
    while (gs.sacc >= s16) {                                       /* the sixteenth clock: the visuals and the glitch's beat */
        gs.sacc -= s16;
        gs.sixteenth = (uint8_t)((gs.sixteenth + 1u) & 15u);
        gs.beat++;
        if (gs.srun) {
            if ((gs.sixteenth & 3u) == 0u && gs_met_level())
                gs_click(gs.sixteenth == 0u);
            if (gs.scount && --gs.scount == 0u) {                       /* the count-in is over: the sequence starts on this downbeat */
                gs.sstep = 255;
                gs.sclk = gs.sacc;                                      /* (what this block ran past the beat is already into step 1) */
            }
        }
    }
    /* the trance gate */
    step_len = (uint32_t)(((uint64_t)s16 * GS_GRATE[gs.p[P_GRATE] & 3u]) >> 8);
    if (step_len < (64u << GS_Q))
        step_len = 64u << GS_Q;
    if (gs.greset) {                                               /* the arp's first key: the gate and the clock start on step 0 */
        gs.greset = 0;
        gs.gstep = 15;
        gs.gacc = step_len;
        gs.sacc = s16;                                              /* the next block is the downbeat (the metronome, the count-in) */
        gs.sixteenth = 15;
    }
    gs.gacc += nq;
    while (gs.gacc >= step_len) {
        gs.gacc -= step_len;
        gs.gstep = (uint8_t)((gs.gstep + 1u) & 15u);
        if (gs.gstep == 0u && gs.p[P_GPAT] == 12u)
            gs.rmask = (uint16_t)((sk_rand() & sk_rand()) | (sk_rand() & 0x1111u));
    }
    {
        const char *pat = GS_GT_STR[gs.p[P_GPAT] % GS_NPAT];
        char c = pat[gs.gstep & 15u];
        int on = c == '?' ? (int)((gs.rmask >> (gs.gstep & 15u)) & 1u) : c == 'x';
        gs.gon = (uint8_t)on;
        if (gs.p[P_GPAT] >= GS_PUMP0) {                            /* a pump: down on the beat, then a curve back up to full by the next one */
            uint32_t per = gs.p[P_GPAT] == GS_PUMP0 ? 4u : 2u;
            uint32_t ph = (((gs.gstep % per) << 12) + ((gs.gacc >> 8) << 12) / ((step_len >> 8) ? (step_len >> 8) : 1u)) / per;   /* 0..4095 through the beat */
            uint32_t t = 1024u + (uint32_t)gs.p[P_GSMO] * 24u, x = (ph << 12) / t;       /* SMOOTH: how much of the beat the swell back takes (a quarter of it to all of it) */
            int32_t down = x >= 4096u ? 0 : 4096 - (int32_t)x;
            down = (down * down) >> 12;
            gs.gate_t = 32767 - (int32_t)(((int64_t)gs.p[P_GDEP] * 258 * down) >> 12);
            if (gs.gate_t < 0)
                gs.gate_t = 0;
            gs.gate_g += (gs.gate_t - gs.gate_g) >> 1;              /* (just enough smoothing that the drop does not click) */
        } else {
            gs.gate_t = on ? 32767 : 32767 - (int32_t)gs.p[P_GDEP] * 258;
            if (gs.gate_t < 0)
                gs.gate_t = 0;
            gs.gate_g += (int32_t)(((int64_t)(gs.gate_t - gs.gate_g) * (int32_t)GS_K[gs.p[P_GSMO]]) >> 16);
        }
    }
    /* the sequencer */
    if (gs.srun && !gs.scount) {
        if (gs.sstep == 255u)                                       /* the start: step 1 sounds at once */
            gs_seq_step();
        else {
            uint32_t len;
            gs.sclk += nq;
            if (gs.snote >= 0 && !gs.sslide && gs.sclk >= gs.soff) {
                gs_note_off((uint32_t)gs.snote);
                gs.snote = -1;
            }
            while (gs.sclk >= (len = gs_seq_len())) {              /* a step ends: what ran over it is kept, so the steps never lose time */
                gs.sclk -= len;
                gs_seq_step();
            }
        }
    }
    /* the arp */
    if (gs.p[P_AON]) {
        uint32_t ai = gs_arp_int();
        gs.aacc += nq;
        if (gs.acur >= 0 && gs.aacc >= gs.aoff) {
            gs_note_off((uint32_t)gs.acur);
            gs.acur = -1;
        }
        while (gs.aacc >= ai) {
            gs.aacc -= ai;
            gs_arp_step();
            gs.aoff = (uint32_t)(((uint64_t)ai * (uint32_t)(8 + gs.p[P_AGATE])) >> 7);
        }
    }
    gs_lfo_step();
}

/* ------------------------------------------------------------------- keys --- */
static int gs_has(uint32_t note)
{
    uint32_t i;
    for (i = 0; i < gs.nh; i++)
        if (gs.hn[i] == note)
            return 1;
    return 0;
}
static void gs_del(uint32_t note)
{
    uint32_t i, j = 0;
    for (i = 0; i < gs.nh; i++)
        if (gs.hn[i] != note)
            gs.hn[j++] = gs.hn[i];
    gs.nh = (uint8_t)j;
}

static void gs_key(uint32_t note, int down)
{
    if (gs.srun && gs.srec) {                                      /* recording: the key sounds and is written where it falls */
        if (down && !gs.scount)
            gs_seq_rec(note);
    } else if (gs.srun) {                                          /* while the sequence runs a key moves it: the key is the note step 12 plays */
        if (down)
            gs.sroot = (uint8_t)note;
        return;
    }
    if (down) {
        if (gs.hold && gs.phys == 0u) {                            /* a fresh chord replaces the held one */
            uint32_t i;
            for (i = 0; i < gs.nh; i++)
                if (!gs.p[P_AON])
                    gs_note_off(gs.hn[i]);
            gs.nh = 0;
        }
        gs.phys++;
        if (gs.nh == 0u && gs.p[P_AON]) {                          /* the arp starts on the beat of the first key */
            gs.aacc = gs_arp_int();
            gs.astep = 0;
            gs.greset = 1;
        }
        if (!gs_has(note) && gs.nh < 8u)
            gs.hn[gs.nh++] = (uint8_t)note;
        if (!gs.p[P_AON])
            gs_note_on(note, 110u);
    } else {
        if (gs.phys)
            gs.phys--;
        if (gs.hold)
            return;
        gs_del(note);
        if (gs.p[P_AON]) {
            if (!gs.nh && gs.acur >= 0) {
                gs_note_off((uint32_t)gs.acur);
                gs.acur = -1;
            }
            return;
        }
        if (GS_VM() == 0u)
            gs_note_off(note);
        else if (gs.nh)
            gs_note_on(gs.hn[gs.nh - 1u], 110u);                   /* back to the key still held */
        else
            gs_note_off(note);
    }
}

static void gs_release_all(void)
{
    gs.nh = 0;
    gs.phys = 0;
    gs_all_off();
    gs.acur = -1;
}

static void gs_set_hold(uint32_t on)
{
    gs.hold = (uint8_t)on;
    if (!on && !gs.phys)
        gs_release_all();
}

/* a patch's voice mode or the arp being switched: let go of what sounds */
static void gs_mode_changed(void)
{
    gs_all_off();
    gs.acur = -1;
    gs.snote = -1;
    gs.sslide = 0;
}
