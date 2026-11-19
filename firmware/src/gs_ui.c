/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GartStringer contributors */
/* GartStringer's panel and screen: black, orange, brushed silver and hints of red; a window in the middle that shows what the
 * page is about, a patch bar, and four silver plates for the knobs.
 *
 *   K1..K4      the four values of the page, named on the plates
 *   ALGORITHM   the page's choice: the oscillator, the filter kind, the voice mode, the echo time, the arp's switch...
 *   PRESETS     the factory patch (32), SELECT the tempo
 *   HOME        PLAY page: CUTOFF RESO DETUNE F.ENV      EDIT: DETUNE MIX SUB COLOUR      SCL: the filter
 *   ENV         the amp envelope, again: the filter envelope      LFO: the one LFO
 *   FX          ensemble, echo, feedback, reverb; again: room size, crush, drive, glide
 *   ARP         the arpeggio (again: on/off)      GLO: the glitch (the chance of one, its length, kind, crush); again: the trance gate
 *   SEQ         the step sequencer: sixteen steps, ALGORITHM picks the step, K1 note, K2 rest / note / accent / slide, K3 shift all,
 *               K4 dice; keys write the step when stopped; PLAY runs it (keys then move it); again: rate, length, swing, gate, direction
 *   REC         held: stutter / reverse / tape stop / drop-out, whatever the glitch page says
 *   PLAY        hold (latch the notes)      OCT- / OCT+  the octave of the keys
 *   SAVE        eight slots in the flash: K1 slot, K2 load / save / clear, SAVE again does it
 *   keys        27 notes from C (OCT moves them)
 *
 * The platform provides (sk_hw.c, the host harness): sk_in_enc(role), sk_in_buttons(&held, &pressed), sk_in_keys(&held),
 * sk_led_btn(label, level), sk_led_key(n, level), sk_store_read / write. */

/* the panel's labels and encoders */
enum { SB_FX, SB_SCL, SB_ENV, SB_LFO, SB_EDIT, SB_GLO, SB_HOME, SB_SAVE, SB_ARP, SB_SEQ, SB_PLAY, SB_REC, SB_OCTDN, SB_OCTUP, SB_N };
enum { SE_SELECT, SE_ALGO, SE_PRESET, SE_K1, SE_K2, SE_K3, SE_K4, SE_N };
enum { PG_HOME, PG_OSC, PG_OSC2, PG_FLT, PG_ENV, PG_ENV2, PG_LFO, PG_MOD, PG_FX, PG_FX2, PG_ARP, PG_GATE, PG_GLITCH, PG_SEQ, PG_SEQ4, PG_SEQ2, PG_SEQ3, PG_CHORD, PG_PERF, PG_SYNC, PG_SAVE, PG_N };
#define UI_SEQP() (ui.page == PG_SEQ || ui.page == PG_SEQ4 || ui.page == PG_SEQ2 || ui.page == PG_SEQ3)   /* one of the sequencer's pages is open */

static int32_t sk_in_enc(uint32_t role);
static void sk_in_buttons(uint32_t *held, uint32_t *pressed);
static uint32_t sk_in_keys(uint32_t *held);
static void sk_led_btn(uint32_t label, uint32_t level);
static void sk_led_key(uint32_t n, uint32_t level);
static int sk_store_read(uint32_t sec, void *dst, uint32_t n);            /* the saved slots, a 4 KB sector at a time: 0 ok */
static int sk_store_write(uint32_t sec, const void *src, uint32_t n);
static void sk_midi_note(int on, uint32_t note, uint32_t vel);     /* a key, sent to the USB host */

/* the audio interrupt's load, % of the time it has (the hardware layer fills these; 0 on a computer) */
static volatile uint32_t sk_cpu_pct, sk_cpu_max, sk_late;
static volatile uint32_t sk_rate_mhz;         /* the converter's rate as last measured against the crystal, in millihertz (0: not yet; always 0 on a computer) */

typedef struct {
    const char *name;
    const char *k[4];                    /* the plates' labels */
    uint8_t p[4];                        /* the parameters under them */
    uint8_t algo;                        /* what ALGORITHM turns */
    uint8_t theme;
} ui_page_t;

static const ui_page_t UI_PG[PG_N] = {
    {"PLAY", {"CUTOFF", "RESO", "DETUNE", "F.ENV"}, {P_CUT, P_RES, P_DET, P_FENV}, 254, 0},      /* (ALGORITHM here: the world) */
    {"OSCILLATOR", {"DETUNE", "MIX", "SUB", "COLOUR"}, {P_DET, P_MIX, P_SUB, P_COL}, P_MODE, 0},
    {"PITCH", {"DRIFT", "OCTAVE", "TAPE", "GLIDE"}, {P_DRIFT, P_OCT, P_TAPE, P_GLIDE}, P_MODE, 0},
    {"FILTER", {"CUTOFF", "RESO", "KEY", "DRIVE"}, {P_CUT, P_RES, P_KEY, P_DRV}, P_FTYPE, 0},
    {"AMP ENVELOPE", {"ATTACK", "DECAY", "SUS", "REL"}, {P_AA, P_AD, P_AS, P_AR}, P_VMODE, 0},
    {"FILTER ENV", {"ATTACK", "DECAY", "SUS", "REL"}, {P_FA, P_FD, P_FS, P_FR}, P_FTYPE, 0},
    {"LFO", {"RATE", "AMOUNT", "TARGET", "SHAPE"}, {P_LRATE, P_LAMT, P_LDST, P_LSHP}, P_LDST, 0},
    {"MATRIX", {"ROUTE", "SOURCE", "TO", "AMOUNT"}, {255, 255, 255, 255}, 255, 2},
    {"EFFECTS", {"CHOR", "ECHO", "FDBK", "REV"}, {P_CHOR, P_DLY, P_DFB, P_REV}, P_DTIM, 0},
    {"EFFECTS 2", {"ROOM", "CRUSH", "TAPE", "GLIDE"}, {P_RSZ, P_CRU, P_TAPE, P_GLIDE}, P_VMODE, 0},
    {"ARPEGGIO", {"PATT", "RATE", "OCTS", "GATE"}, {P_APAT, P_ARATE, P_ARNG, P_AGATE}, P_AON, 1},
    {"GATE + PUMP", {"PATT", "RATE", "DEPTH", "SMOOTH"}, {P_GPAT, P_GRATE, P_GDEP, P_GSMO}, P_GPAT, 1},
    {"GLITCH", {"ODDS", "LENGTH", "KIND", "CRUSH"}, {P_GLP, P_GLL, P_GLT, P_CRU}, P_GLT, 2},
    {"SEQUENCER", {"STEP", "NOTE", "TYPE", "LENGTH"}, {255, 255, 255, 255}, 255, 1},
    {"SEQ STEP", {"STEP", "REPEAT", "CHANCE", "SOUND"}, {255, 255, 255, 255}, 255, 1},
    {"SEQ SETUP", {"RATE", "SWING", "GATE", "CLICK"}, {P_SRATE, P_SSWG, P_SGATE, P_SMET}, 255, 1},
    {"SEQ TOOLS", {"CLEAR", "COPY", "FILL", "SHIFT"}, {255, 255, 255, 255}, 255, 1},
    {"CHORDS", {"KEY", "SCALE", "LAYOUT", "VOICE"}, {P_CKEY, P_CSCL, P_CLAY, P_CVOI}, P_CON, 0},
    {"PERFORM", {"MACRO", "KIND", "DJ FILT", "PUMP"}, {255, 255, 255, P_GDEP}, 255, 0},
    {"CLOCK", {"SYNC", "CLOCK", "TEMPO", "SONG"}, {255, 255, 255, 255}, 255, 0},
    {"SAVE", {"SLOT", "DO", "KEYS", "BATTERY"}, {255, 255, 255, 255}, 255, 0},
};

static struct {
    uint8_t page, theme_on;
    uint32_t held;
    uint32_t k_t[4];                     /* when a knob was last touched */
    uint32_t pop_t, hud_t;               /* the patch name's pop, the HUD */
    uint8_t slot, act;
    uint8_t cur;                         /* the sequencer page's step, 0..127 */
    uint32_t cur_t;                      /* when it was last moved (the view follows the playhead after a while) */
    int8_t shift;                        /* what the pattern has been shifted by (shown) */
    uint8_t keys_down[27], kchord[27], con_prev;
    uint8_t mrow;                        /* the matrix page: the route the knobs are on */
    uint8_t g_dirty;                     /* the sync was changed and is not written yet */
    uint32_t g_t, arp_t;                 /* when it was changed; when ARP went down on its own page (held: the clock page) */
    uint8_t bank;                        /* MIDI: the bank a program change picks from (128 patches each) */
    uint8_t silent;                      /* the keys go to the USB host only: nothing here sounds (not saved: off at power-on) */
    uint8_t kraw[27], kmute[27];         /* each key held: the note it sent to the host, and that it was silent here */
    uint8_t mot_k;                       /* knob recording: the parameter a hand is on */
    uint32_t mot_t;                      /* .. and when it last moved (0: none) */
    uint32_t keys_held;
    uint32_t toast_t;
    char toast[20];
    uint8_t intro;
    uint32_t intro_t;
    uint32_t spl_t;                      /* a world was just entered: its title shows for a moment (0: not) */
    uint8_t flash;                       /* a patch change: counts down */
    uint8_t wlast[GS_NWORLD];            /* the patch last used in each world (ALGORITHM on the home page steps through the worlds) */
    uint8_t rec_seen;                    /* gs.rec_n as last seen: a change flashes the step written */
    uint32_t rec_t;                      /* when */
} ui;

#define SV_PER 3                                          /* slots to a sector */
#define SV_SECT 6                                         /* sectors */
#define SV_N (SV_PER * SV_SECT)                           /* eighteen slots */
#define SV_SLOT (4 + P_N55 + GS_NP + GS_NP * GS_NS)         /* a slot: the patch's header, every parameter, the eight patterns' lengths and steps */
static struct {                                           /* the sector in hand (a save rewrites only its own sector) */
    uint32_t magic;
    uint8_t slot[SV_PER][SV_SLOT];
    uint32_t sum;
} sv;
static uint8_t sv_sec = 255;                              /* which sector sv holds */
/* 1.2 keeps a slot to a sector, in eighteen sectors of its own under the six above. A slot now also holds each step's repeat and
 * chance, each pattern's own sound, and room kept for a recorded knob. The six older sectors are only ever read: a slot that has no
 * sector of its own yet is taken from them, so what an older version saved is still there after any number of updates and saves. */
#define SV_NEW0 SV_SECT                                   /* the first of the new sectors */
/* A slot's layout, for a count of parameters pn. Every version since 1.2 holds the same things in the same order; only pn has grown
 * (55 in 1.2 to 2.0, "GST4"; 56 in 2.1, "GST5" with its first byte 1; from 2.2 the slot's first byte IS pn, so a later version that
 * adds parameters needs no new format). All are read; what an older slot lacks takes its default. The sum follows the slot. */
#define SVL_LEN(pn) (4u + (pn))                           /* the patterns' lengths */
#define SVL_SP(pn) (SVL_LEN(pn) + GS_NP)                  /* their steps */
#define SVL_SX(pn) (SVL_SP(pn) + GS_NP * GS_NS)           /* the steps' extras: repeat, chance */
#define SVL_PSND(pn) (SVL_SX(pn) + GS_NP * GS_NS)         /* which patterns keep a sound */
#define SVL_PPRE(pn) (SVL_PSND(pn) + GS_NP)               /* the patch each began as */
#define SVL_PP(pn) (SVL_PPRE(pn) + GS_NP)                 /* their parameters */
#define SVL_MOT(pn) (SVL_PP(pn) + GS_NP * (pn))           /* the recorded knob: a parameter a pattern, then a value a step */
#define SVL_SLOT(pn) (SVL_MOT(pn) + GS_NP + GS_NP * GS_NS)
#define SV4_SLOT SVL_SLOT(P_N55)
#define SV5_SLOT SVL_SLOT(P_N)
#define P_N56 56u                                         /* 2.1's count */
#define SVL_SUM(pn) ((SVL_SLOT(pn) + 3u) & ~3u)           /* where the sum sits in d[] */
static struct {
    uint32_t magic;
    uint8_t d[4092];                                      /* the slot, then its sum: a whole sector */
} s4;
static uint8_t s4_slot = 255;                             /* which slot s4 holds */
static uint8_t s4_pn = P_N;                               /* .. and the layout it is in: how many parameters */
#define SV4_MAGIC 0x34545347u                /* "GST4" */
#define SV5_MAGIC 0x35545347u                /* "GST5" */
_Static_assert(sizeof s4 == 4096 && SVL_SUM(P_N) + 4u <= sizeof s4.d && P_N >= 57 && P_N < 128, "a slot is a sector; its first byte is P_N");
static uint32_t sv_full;                                  /* a bit a slot: kept */
#define SV_MAGIC 0x33545347u                 /* "GST3" */

static void ui_seq_grow(uint32_t now);
static int ui_clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static char *ui_num(char *p, int v)
{
    char t[12];
    int n = 0;
    if (v < 0) {
        *p++ = '-';
        v = -v;
    }
    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    while (n)
        *p++ = t[--n];
    *p = 0;
    return p;
}
static char *ui_str(char *p, const char *s)
{
    while (*s)
        *p++ = *s++;
    *p = 0;
    return p;
}

/* the world a patch belongs to: trance (the first 32 patches), house, techno, garage (16 each). It sets the look and nothing else. */
static int ui_world(void) { return GS_PWORLD[gs.preset % GS_NPRE]; }
/* MIDI that changes settings (control changes, program changes), handed over from the audio interrupt (which takes MIDI in) to the
 * main loop: one writer, one reader. An event is status | data 1 << 8 | data 2 << 16. */
static volatile uint32_t ui_mq[32], ui_mq_w, ui_mq_r;
static void ui_midi_push(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (ui_mq_w - ui_mq_r < 32u) {
        ui_mq[ui_mq_w % 32u] = (st & 0xFFu) | ((d1 & 127u) << 8) | ((d2 & 127u) << 16);
        ui_mq_w++;
    }
}
static const char *const UI_WORLD[GS_NWORLD] = {"TRANCE", "HOUSE", "TECHNO", "GARAGE", "AMBIENT", "BASS"};

#include "gs_chord.c"

/* a parameter as the plate shows it: text for a choice, a number for the rest */
static const char *ui_val(uint32_t idx, char *buf)
{
    int v = gs.p[idx];
    switch (idx) {
    case P_MODE: return GS_MODE_NAME[v % GS_NMODE];
    case P_FTYPE: return GS_FT_NAME[v % GS_NFT];
    case P_LDST: return GS_LDST_NAME[v % 5];
    case P_LSHP: return GS_LSHP_NAME[v & 3];
    case P_VMODE: return GS_VM_NAME[v % 3];
    case P_DTIM: return GS_DT_NAME[v % 6];
    case P_GPAT: return GS_GT_NAME[v % GS_NPAT];
    case P_GRATE: return GS_GR_NAME[v & 3];
    case P_APAT: return GS_AP_NAME[v & 7];
    case P_ARATE: return GS_AR_NAME[v % 5];
    case P_GLL: return GS_GLL_NAME[v % 5];
    case P_GLT: return GS_GL_NAME[v & 3];
    case P_SMET: return GS_MET_NAME[v & 3];
    case P_CKEY: return CD_NOTE[v % 12];
    case P_CSCL: return CD_SCNAME[v % CD_NSC];
    case P_CLAY: return CD_LAYNAME[v & 3];
    case P_CVOI: return CD_VOINAME[v % CD_NVOI];
    case P_CON: return v == 2 ? "SCALE LOCK" : v ? "ON" : "OFF";
    case P_SRATE: return GS_GR_NAME[v & 3];
    case P_AON: return v ? "ON" : "OFF";
    case P_OCT: ui_num(buf, v - 2); return buf;
    case P_GLIDE: if (!v) return "OFF"; break;
    default: break;
    }
    ui_num(buf, v);
    return buf;
}
/* a parameter's name: the label of the knob that sets it */
static const char *ui_pname(uint32_t idx)
{
    uint32_t g, k;
    for (g = 0; g < PG_N; g++)
        for (k = 0; k < 4u; k++)
            if (UI_PG[g].p[k] == idx)
                return UI_PG[g].k[k];
    return "?";
}
static int ui_is_enum(uint32_t idx)
{
    return idx == P_M1S || idx == P_M1D || idx == P_M2S || idx == P_M2D || idx == P_M3S || idx == P_M3D || idx == P_MODE || idx == P_FTYPE || idx == P_LDST || idx == P_LSHP || idx == P_VMODE || idx == P_DTIM || idx == P_GPAT ||
           idx == P_GRATE || idx == P_APAT || idx == P_ARATE || idx == P_GLL || idx == P_GLT || idx == P_AON || idx == P_SRATE || idx == P_SMET || idx == P_CKEY || idx == P_CSCL || idx == P_CLAY || idx == P_CVOI || idx == P_CON;
}
static int ui_maxv(uint32_t idx)
{
    switch (idx) {
    case P_MODE: return GS_NMODE - 1;
    case P_FTYPE: return GS_NFT - 1;
    case P_LDST: return 4;
    case P_M1S: case P_M2S: case P_M3S: return GS_NMSRC - 1;
    case P_M1D: case P_M2D: case P_M3D: return GS_NMDST - 1;
    case P_LSHP: return 3;
    case P_VMODE: return 2;
    case P_DTIM: return 5;
    case P_GPAT: return GS_NPAT - 1;
    case P_GRATE: return 3;
    case P_APAT: return 7;
    case P_ARATE: return 4;
    case P_ARNG: return 3;
    case P_AON: return 1;
    case P_SRATE: return 3;
    case P_CON: return 2;
    case P_CKEY: return 11;
    case P_CSCL: return CD_NSC - 1;
    case P_CLAY: return 3;
    case P_CVOI: return CD_NVOI - 1;
    case P_SMET: return 3;
    case P_GLL: return 4;
    case P_GLT: return 3;
    case P_OCT: return 4;
    default: return 127;
    }
}

static void ui_toast(const char *a, uint32_t now);
static void ui_toast(const char *a, uint32_t now)
{
    char *p = ui.toast;
    while (*a && p < ui.toast + 18)
        *p++ = *a++;
    *p = 0;
    ui.toast_t = now;
}

/* writing a step past the end of the pattern stretches the pattern to it, so a long one is built by just writing on */
static void ui_seq_grow(uint32_t now)
{
    if (ui.cur >= gs.spl[gs.pcur]) {
        char b[20], *p = ui_str(b, "LENGTH ");
        gs.spl[gs.pcur] = (uint8_t)(ui.cur + 1);
        ui_num(p, ui.cur + 1);
        ui_toast(b, now);
    }
}

/* ----------------------------------------------------------------- the saved slots --- */
static uint32_t sv_sum(void)
{
    const uint8_t *p = (const uint8_t *)sv.slot;
    uint32_t i, h = 0x811C9DC5u;
    for (i = 0; i < sizeof sv.slot; i++)
        h = (h ^ p[i]) * 16777619u;
    return h;
}
static void sv_pack(uint8_t *d)                             /* (always this version's layout) */
{
    uint32_t i, k;
    d[0] = (uint8_t)P_N;                                    /* (in use, and how many parameters follow) */
    d[1] = gs.preset;
    d[2] = gs.bpm;
    for (i = 0; i < P_N; i++)
        d[4u + i] = gs.p[i];
    if (gs.psnd[gs.pcur]) {                                 /* the pattern playing keeps its sound: as it is now */
        for (i = 0; i < P_N; i++)
            gs.pp[gs.pcur][i] = gs.p[i];
        gs.ppre[gs.pcur] = gs.preset;
    }
    for (i = 0; i < GS_NP; i++) {
        d[SVL_LEN(P_N) + i] = gs.spl[i];
        for (k = 0; k < GS_NS; k++) {
            d[SVL_SP(P_N) + i * GS_NS + k] = gs.sp[i][k];
            d[SVL_SX(P_N) + i * GS_NS + k] = gs.sx[i][k];
            d[SVL_MOT(P_N) + GS_NP + i * GS_NS + k] = gs.mot[i][k];
        }
        d[SVL_PSND(P_N) + i] = gs.psnd[i];
        d[SVL_PPRE(P_N) + i] = gs.ppre[i];
        d[SVL_MOT(P_N) + i] = gs.mot_id[i];
        for (k = 0; k < P_N; k++)
            d[SVL_PP(P_N) + i * P_N + k] = gs.pp[i][k];
    }
    d[3] = (uint8_t)(((uint32_t)gs.oct & 7u) | ((uint32_t)gs.pcur << 3));     /* the octave, and the pattern it was on */
}
/* a slot into the instrument; pn: the layout d is in (a parameter the slot has not got takes its default) */
static void sv_unpack(const uint8_t *d, uint32_t pn)
{
    uint32_t i, k;
    gs_all_off();
    gs.acur = -1;
    gs.preset = (uint8_t)(d[1] % GS_NPRE);
    gs.bpm = (uint8_t)ui_clamp(d[2], 40, 200);
    {
        int o = (int)(d[3] & 7u);                           /* (three bits, signed: an older slot's whole byte reads the same) */
        gs.oct = (int8_t)ui_clamp(o > 3 ? o - 8 : o, -2, 2);
    }
    for (i = 0; i < P_N; i++)
        gs.p[i] = i < pn ? (uint8_t)ui_clamp(d[4u + i] & 127, 0, ui_maxv(i)) : GS_DEF[i];
    if (gs.p[P_ARNG] < 1u)
        gs.p[P_ARNG] = 1;
    for (i = 0; i < GS_NP; i++) {
        uint32_t id = d[SVL_MOT(pn) + i];
        gs.spl[i] = (uint8_t)ui_clamp(d[SVL_LEN(pn) + i], 1, GS_NS);
        gs.mot_id[i] = (uint8_t)(id <= P_N ? id : 0u);
        for (k = 0; k < GS_NS; k++) {
            uint8_t b = d[SVL_SP(pn) + i * GS_NS + k], m = d[SVL_MOT(pn) + GS_NP + i * GS_NS + k];
            gs.sp[i][k] = (uint8_t)((b & 0xC0u) | (uint8_t)ui_clamp(b & 63, 0, 37));
            gs.sx[i][k] = (uint8_t)(d[SVL_SX(pn) + i * GS_NS + k] & 15u);
            gs.mot[i][k] = (uint8_t)(gs.mot_id[i] && m <= 128u ? m : 0u);
        }
        gs.psnd[i] = (uint8_t)(d[SVL_PSND(pn) + i] & 1u);
        gs.ppre[i] = (uint8_t)(d[SVL_PPRE(pn) + i] % GS_NPRE);
        for (k = 0; k < P_N; k++)
            gs.pp[i][k] = k < pn ? (uint8_t)ui_clamp(d[SVL_PP(pn) + i * pn + k] & 127, 0, ui_maxv(k)) : GS_DEF[k];
        if (gs.pp[i][P_ARNG] < 1u)
            gs.pp[i][P_ARNG] = 1;
    }
    gs.pcur = (d[3] & 0xC0u) ? 0u : (uint8_t)((d[3] >> 3) % GS_NP);      /* (an older slot, with the octave below zero, has every bit set) */
    gs.pnext = 255;
    gs.snote = -1;
    gs.sslide = 0;
    gs.pdirty = 1;                                          /* the notes are the slot's: a change of patch keeps them */
    gs_mode_changed();
}
/* 0.6.0 and 0.6.1 kept six slots, each with four patterns, in one sector ("GST2"). While that sector is still in flash, its slots are
 * read from it and carried over to the new layout: slots 1-3 into sector 0 and 4-6 into sector 1. Sector 1 is written before sector 0
 * is touched, so a save never destroys the old slots until the carried-over copy of the rest is safe. */
#define SV_OLD_SLOT (4 + P_N55 + 4 + 4 * GS_NS)
#define SV_OLD_MAGIC 0x32545347u
static struct {
    uint32_t magic;
    uint8_t slot[6][SV_OLD_SLOT];
    uint32_t sum;
} sv_old;
static uint8_t sv_old_ok;                                  /* sector 0 still holds the old store */
static void sv_old_load(void)
{
    const uint8_t *p = (const uint8_t *)sv_old.slot;
    uint32_t i, h = 0x811C9DC5u;
    sv_old_ok = 0;
    if (sk_store_read(0, &sv_old, sizeof sv_old) != 0 || sv_old.magic != SV_OLD_MAGIC)
        return;
    for (i = 0; i < sizeof sv_old.slot; i++)
        h = (h ^ p[i]) * 16777619u;
    sv_old_ok = h == sv_old.sum;
}
static void sv_from_old(uint32_t sec)                      /* sector sec (0 or 1) of the new layout, made from the old slots */
{
    uint32_t i, k, j;
    for (i = 0; i < SV_PER; i++) {
        const uint8_t *o = sv_old.slot[sec * SV_PER + i];
        uint8_t *d = sv.slot[i];
        if (!o[0])
            continue;
        for (k = 0; k < 4u + P_N55; k++)                      /* the header and every parameter */
            d[k] = o[k];
        for (j = 0; j < GS_NP; j++) {                       /* patterns A-D as they were, E-H empty */
            d[4u + P_N55 + j] = j < 4u ? o[4u + P_N55 + j] : 16;
            for (k = 0; k < GS_NS; k++)
                d[4u + P_N55 + GS_NP + j * GS_NS + k] = j < 4u ? o[4u + P_N55 + 4u + j * GS_NS + k] : 0;
        }
    }
    sv.magic = SV_MAGIC;
    sv.sum = sv_sum();
}
static void ui_sv_sec(uint32_t sec)                    /* bring a sector in (an empty one if it was never written or is damaged) */
{
    uint32_t i;
    if (sv_sec == sec)
        return;
    if (sk_store_read(sec, &sv, sizeof sv) != 0 || sv.magic != SV_MAGIC || sv.sum != sv_sum()) {
        for (i = 0; i < sizeof sv; i++)
            ((uint8_t *)&sv)[i] = 0;
        if (sv_old_ok && sec < 2u)
            sv_from_old(sec);
    }
    sv_sec = (uint8_t)sec;
}
static uint32_t s4_sum(uint32_t n)
{
    uint32_t i, h = 0x811C9DC5u;
    for (i = 0; i < n; i++)
        h = (h ^ s4.d[i]) * 16777619u;
    return h;
}
static int s4_ok(uint32_t pn)                               /* the sum that follows a slot of pn parameters is right */
{
    const uint8_t *q = s4.d + SVL_SUM(pn);
    return ((uint32_t)q[0] | ((uint32_t)q[1] << 8) | ((uint32_t)q[2] << 16) | ((uint32_t)q[3] << 24)) == s4_sum(SVL_SLOT(pn));
}
/* bring a slot in: from its own sector if it has one (in any of the layouts above), else from where an older version kept it
 * (s4.d[0] 0: nothing there). s4_pn says which layout came. */
static void ui_sv_slot(uint32_t slot)
{
    uint32_t i;
    const uint8_t *o;
    if (s4_slot == slot)
        return;
    s4_slot = (uint8_t)slot;
    if (sk_store_read(SV_NEW0 + slot, &s4, sizeof s4) == 0) {
        if (s4.magic == SV4_MAGIC && s4_ok(P_N55)) {
            s4_pn = P_N55;
            return;
        }
        if (s4.magic == SV5_MAGIC) {
            uint32_t pn = s4.d[0];
            if (pn == 0u) {                                     /* a cleared slot: written by 2.1, or by this layout */
                s4_pn = P_N;
                if (s4_ok(P_N56) || s4_ok(P_N))
                    return;
            } else {
                if (pn == 1u)
                    pn = P_N56;
                if (pn >= P_N56 && pn <= 100u && s4_ok(pn)) {   /* (more than this version knows: a later one's; the ones it knows are read) */
                    s4_pn = (uint8_t)pn;
                    return;
                }
            }
        }
    }
    for (i = 0; i < sizeof s4; i++)
        ((uint8_t *)&s4)[i] = 0;
    s4_pn = P_N55;
    ui_sv_sec(slot / SV_PER);
    o = sv.slot[slot % SV_PER];
    if (!o[0])
        return;
    for (i = 0; i < SV_SLOT; i++)                           /* an older slot is the start of a 1.2 one: the rest is "nothing extra" */
        s4.d[i] = o[i];
}
/* What belongs to the instrument and not to a slot (so far: the sync) has a sector of its own, after the slots'. It is read at
 * power-on and written a moment after it changes, once nothing is playing (a write makes the output silent for a moment). */
#define SV_GLOB (SV_NEW0 + SV_N)
#define SG_MAGIC 827806535u                                 /* "GSG1" */
static struct { uint32_t magic; uint8_t sync, r[3]; uint32_t chk; } sg;
static uint32_t sg_chk(void) { return (sg.magic ^ ((uint32_t)sg.sync * 2654435761u)) + 97u; }
static void ui_glob_load(void)
{
    if (sk_store_read(SV_GLOB, &sg, sizeof sg) == 0 && sg.magic == SG_MAGIC && sg.chk == sg_chk() && sg.sync < GS_NSYNC)
        gs_sync = sg.sync;
}
static void ui_glob_tick(uint32_t now)
{
    if (!ui.g_dirty || now - ui.g_t < 2000u || gs.srun || gs.nh || gs_ext.on)
        return;
    ui.g_dirty = 0;
    sg.magic = SG_MAGIC;
    sg.sync = gs_sync;
    sg.r[0] = sg.r[1] = sg.r[2] = 0;
    sg.chk = sg_chk();
    sk_store_write(SV_GLOB, &sg, sizeof sg);
}
static void ui_store_load(void)
{
    uint32_t i;
    sv_full = 0;
    ui_glob_load();
    sv_old_load();
    sv_sec = 255;
    for (i = 0; i < SV_N; i++) {
        s4_slot = 255;
        ui_sv_slot(i);
        if (s4.d[0])
            sv_full |= 1u << i;
    }
}
static void ui_do_save(uint32_t now)
{
    uint32_t bit = 1u << ui.slot, i;
    ui_sv_slot(ui.slot);
    if (ui.act == 0u) {
        if (!s4.d[0])
            ui_toast("EMPTY", now);
        else {
            sv_unpack(s4.d, s4_pn);
            ui_toast("LOADED", now);
            ui.pop_t = now;
        }
        return;
    }
    for (i = 0; i < sizeof s4.d; i++)                       /* (a cleared slot has a sector too: it hides the older copy) */
        s4.d[i] = 0;
    if (ui.act == 1u)
        sv_pack(s4.d);
    s4_pn = P_N;
    s4.magic = SV5_MAGIC;
    {
        uint32_t sum = s4_sum(SV5_SLOT);
        uint8_t *q = s4.d + SVL_SUM(P_N);
        q[0] = (uint8_t)sum; q[1] = (uint8_t)(sum >> 8); q[2] = (uint8_t)(sum >> 16); q[3] = (uint8_t)(sum >> 24);
    }
    if (sk_store_write(SV_NEW0 + ui.slot, &s4, sizeof s4) == 0) {
        sv_full = ui.act == 1u ? sv_full | bit : sv_full & ~bit;
        ui_toast(ui.act == 1u ? "SAVED" : "CLEARED", now);
    } else {
        s4_slot = 255;
        ui_toast("FAILED", now);
    }
}

/* ------------------------------------------------------------------------ input --- */
static void ui_theme(uint32_t t)
{
    if (ui.theme_on != t || !fb_started) {
        ui.theme_on = (uint8_t)t;
        fb_theme(t);
    }
}

static void ui_init(uint32_t safe)
{
    uint32_t i;
    (void)safe;
    gs_init();
    for (i = 0; i < 4u; i++)
        ui.k_t[i] = 0;
    ui.page = PG_HOME;
    gs_seq_chord = cd_seq_notes;
    {
        uint32_t w;
        for (w = 0; w < GS_NWORLD; w++)
            ui.wlast[w] = GS_ORD[GS_W0[w]];
    }
    ui.theme_on = 99;
    fb_theme_snap(0);
    ui.theme_on = 0;
}

static void ui_intro_start(void)
{
    ui.intro = 1;
    ui.intro_t = 0;
    gs_intro_n = 1;
    gs_intro_lp = 0;
    sk_fade = 0;
    sk_fade_to = 4096;
}

static void ui_load_patch(uint32_t i, uint32_t now)
{
    gs_load(i);
    ui.con_prev = gs.p[P_CON];                                      /* (a patch brings its own chords setting: no need to announce it) */
    ui.pop_t = now;
    ui.flash = 10;
}

static int32_t ui_enc(uint32_t role)
{
    int32_t d = sk_in_enc(role);
    if (d && ui.intro)
        ui.intro = 0, gs_intro_n = 0;
    if (d && role != SE_ALGO)
        ui.spl_t = 0;                                               /* (any other knob: the world's title steps aside) */
    return d;
}

static void ui_page_press(uint32_t a, uint32_t b)           /* the button of a page, or a pair of pages: open, swap, close */
{
    if (ui.page == a)
        ui.page = (uint8_t)(b != 255u ? b : PG_HOME);
    else if (ui.page == b && b != 255u)
        ui.page = PG_HOME;
    else
        ui.page = (uint8_t)a;
}

/* Control from outside, on any MIDI channel. A program change picks a patch, in the order PRESETS shows them (bank select, CC 0 or
 * 32: 0 the first 128, 1 the rest). Control changes: 16 to 19 are K1 to K4 of the page that is open; the rest are fixed. */
static void ui_midi(uint32_t now)
{
    static const uint8_t CC[][2] = {
        {1, P_LAMT}, {5, P_GLIDE}, {70, P_COL}, {71, P_RES}, {72, P_AR}, {73, P_AA}, {74, P_CUT}, {75, P_AD}, {76, P_LRATE}, {77, P_DRV},
        {78, P_FENV}, {79, P_AS}, {85, P_DRIFT}, {86, P_TAPE}, {87, P_CRU}, {88, P_MIX}, {89, P_SUB}, {91, P_REV}, {92, P_DLY}, {93, P_CHOR},
        {94, P_DET}, {95, P_DFB},
    };
    while (ui_mq_r != ui_mq_w) {
        uint32_t e = ui_mq[ui_mq_r % 32u], st = e & 0xF0u, d1 = (e >> 8) & 127u, d2 = (e >> 16) & 127u, k;
        ui_mq_r++;
        if (st == 0xC0u) {
            ui_load_patch(GS_ORD[(d1 + 128u * ui.bank) % GS_NPRE], now);
            continue;
        }
        if (st != 0xB0u)
            continue;
        if (d1 == 0u || d1 == 32u)
            ui.bank = (uint8_t)(d2 ? 1 : 0);
        else if (d1 == 12u)
            gs.mac = (uint8_t)d2;
        else if (d1 == 13u)
            gs.djf = (uint8_t)d2;
        else if (d1 == 120u || d1 == 123u) {
            gs_all_off();
            cd_all_off();
        } else if (d1 >= 16u && d1 <= 19u) {
            uint32_t idx = UI_PG[ui.page].p[d1 - 16u];
            if (idx < P_N) {
                gs_set(idx, (int)(d2 * (uint32_t)ui_maxv(idx) + 63u) / 127);
                ui.k_t[d1 - 16u] = now;
            }
        } else
            for (k = 0; k < sizeof CC / sizeof CC[0]; k++)
                if (CC[k][0] == d1) {
                    gs_set(CC[k][1], (int)(d2 * (uint32_t)ui_maxv(CC[k][1]) + 63u) / 127);
                    break;
                }
    }
}

static void ui_input(uint32_t now)
{
    uint32_t held, pr, keys, kp, i;
    int32_t d;
    ui_midi(now);
    ui_glob_tick(now);
    sk_in_buttons(&held, &pr);
    keys = 0;
    kp = sk_in_keys(&keys);
    ui.held = held;
    if (ui.intro && (pr || kp))
        ui.intro = 0, gs_intro_n = 0;
    if (pr || kp)
        ui.spl_t = 0;
    /* pages */
    if (pr & (1u << SB_HOME)) {                                     /* HOME, and again: the chords */
        ui.page = ui.page == PG_HOME ? PG_CHORD : ui.page == PG_CHORD ? PG_PERF : PG_HOME;      /* HOME, the chords, the performance page */
        if (ui.page == PG_CHORD)
            ui_toast(GS_SLOCK() ? "SCALE LOCK" : GS_CHORDS() ? "CHORDS ON" : "ALGO: CHORDS, LOCK", now);
    }
    if (pr & (1u << SB_EDIT))
        ui_page_press(PG_OSC, PG_OSC2);                             /* EDIT: the oscillator; again: its pitch (drift, octave, tape, glide) */
    if (pr & (1u << SB_SCL))
        ui_page_press(PG_FLT, 255);
    if (pr & (1u << SB_ENV))
        ui_page_press(PG_ENV, PG_ENV2);
    if (pr & (1u << SB_LFO))
        ui_page_press(PG_LFO, PG_MOD);                              /* LFO: the LFO; again: the modulation matrix */
    if (pr & (1u << SB_FX))
        ui_page_press(PG_FX, PG_FX2);
    if (pr & (1u << SB_SEQ)) {                                      /* the sequencer: edit, setup, tools, out */
        if (ui.page == PG_SEQ)
            ui.page = PG_SEQ4;
        else if (ui.page == PG_SEQ4)
            ui.page = PG_SEQ2;
        else if (ui.page == PG_SEQ2)
            ui.page = PG_SEQ3;
        else if (ui.page == PG_SEQ3)
            ui.page = PG_SYNC;                                      /* .. the clock (the sync), out */
        else if (ui.page == PG_SYNC)
            ui.page = PG_HOME;
        else {
            ui.page = PG_SEQ;
            ui.cur_t = now;
            ui_toast(gs.srun ? "RUNNING" : "KEYS WRITE STEPS", now);
        }
    }
    if (pr & (1u << SB_GLO))
        ui_page_press(PG_GLITCH, PG_GATE);
    if (pr & (1u << SB_ARP)) {
        if (ui.page == PG_ARP) {                                    /* on the page: it switches the arp (held: the clock page, below) */
            gs_set(P_AON, !gs.p[P_AON]);
            ui_toast(gs.p[P_AON] ? "ARP ON" : "ARP OFF", now);
            ui.arp_t = now ? now : 1u;
        } else if (ui.page == PG_SYNC)
            ui.page = PG_ARP;                                       /* (back from the clock page: the arp is left as it is) */
        else {
            ui.page = PG_ARP;
            if (!gs.p[P_AON]) {
                gs_set(P_AON, 1);
                ui_toast("ARP ON", now);
            }
        }
    }
    if (ui.arp_t) {                                                 /* ARP held on its page: the press is taken back, and the clock page opens */
        if (!(held & (1u << SB_ARP)) || ui.page != PG_ARP)
            ui.arp_t = 0;
        else if (now - ui.arp_t > 600u) {
            ui.arp_t = 0;
            gs_set(P_AON, !gs.p[P_AON]);
            ui.page = PG_SYNC;
            ui_toast("CLOCK", now);
        }
    }
    if (pr & (1u << SB_SAVE)) {
        if (ui.page == PG_SAVE)
            ui_do_save(now);
        else
            ui.page = PG_SAVE;
    }
    if (pr & (1u << SB_PLAY)) {
        if (gs.srun || UI_SEQP()) {   /* the sequence runs, or its pages are open: PLAY runs and stops it (from any page once it runs) */
            if (gs.srun) {
                gs_seq_stop();
                ui_toast("STOP", now);
            } else {
                if (gs.p[P_AON])
                    gs_set(P_AON, 0);
                gs_release_all();
                gs.sroot = (uint8_t)(60 + 12 * gs.oct);
                gs_seq_start();
                if (!gs.srec)                                       /* (recording shows its count-in instead) */
                    ui_toast("PLAY", now);
            }
        } else {
            gs_set_hold(!gs.hold);
            ui_toast(gs.hold ? "HOLD ON" : "HOLD OFF", now);
        }
    }
    if ((pr & (1u << SB_REC)) && UI_SEQP()) {   /* on the sequencer's pages REC arms recording */
        gs.srec = (uint8_t)!gs.srec;
        ui_toast(gs.srec ? (gs.srun ? "RECORDING" : "REC ARMED") : "REC OFF", now);
    }
    gl.manual = (uint8_t)(((held >> SB_REC) & 1u) && !UI_SEQP());   /* (the sequencer's pages use REC to record) */
    if (pr & (1u << SB_OCTDN)) {
        if (gs.srun && gs.sroot >= 12u)
            gs.sroot = (uint8_t)(gs.sroot - 12u);
        gs.oct = (int8_t)ui_clamp(gs.oct - 1, -2, 2);
        ui.hud_t = now;
    }
    if (pr & (1u << SB_OCTUP)) {
        if (gs.srun && gs.sroot <= 108u)
            gs.sroot = (uint8_t)(gs.sroot + 12u);
        gs.oct = (int8_t)ui_clamp(gs.oct + 1, -2, 2);
        ui.hud_t = now;
    }
    if (!GS_CHORDS() && (cd_down || cd.held || cd.sounding))          /* switched off: let go of what the chords hold */
        cd_all_off();
    if (gs.p[P_CON] != ui.con_prev) {
        ui.con_prev = gs.p[P_CON];
        ui_toast(GS_SLOCK() ? "SCALE LOCK" : GS_CHORDS() ? "CHORDS ON" : "CHORDS OFF", now);
        gs_all_off();                                               /* (what the keys hold was played the other way) */
    }
    /* the keys: 27 notes from C3, moved by the octave. Each also goes to the USB host as a note (the key itself, whatever the chords or
     * the sequencer make of it here) */
    for (i = 0; i < 27u; i++) {
        uint32_t on = (keys >> i) & 1u;
        int note = 48 + 12 * gs.oct + (int)i;
        if (note < 0 || note > 120)
            continue;
        if (on && !ui.keys_down[i]) {
            sk_midi_note(1, (uint32_t)note, 100);
            ui.kraw[i] = (uint8_t)note;
            if (ui.silent) {                                        /* SILENT: the key goes to the host and nothing here sounds or is written */
                ui.keys_down[i] = (uint8_t)note;
                ui.kmute[i] = 1;
                continue;
            }
            if (GS_SLOCK())                                         /* the scale lock: a key off the scale plays the scale's note under it */
                while (!cd_in_scale(((note - (int)gs.p[P_CKEY]) % 12 + 12) % 12) && note > 0)
                    note--;
            ui.keys_down[i] = (uint8_t)note;
            if (ui.page == PG_SEQ && !gs.srun) {                    /* stopped, on the sequencer's page: a key writes the step (key 13 is the root) */
                int n = ui_clamp(note - (48 + 12 * gs.oct + 12) + 12, 0, 36), v = 0, wr = 1;
                if (GS_CHORDS()) {                                  /* the chords are on: a root key writes its root (the step plays its chord), a chord button writes nothing */
                    cd_prepare();
                    wr = cd_role((int)i, gs.p[P_CLAY], &v) == 2;
                    n = ui_clamp(cd_pitch(v) - ((int)gs.sroot - 12), 0, 36);
                }
                if (wr) {
                    gs.pdirty = 1;
                    gs.sp[gs.pcur][ui.cur] = (uint8_t)((gs.sp[gs.pcur][ui.cur] & 0xC0u) | (uint8_t)(n + 1));
                    ui_seq_grow(now);
                    ui.cur = (uint8_t)ui_clamp((int)ui.cur + 1, 0, GS_NS - 1);
                    ui.cur_t = now;
                }
            }
            if (GS_CHORDS()) {
                ui.kchord[i] = 1;
                cd_key((int)i, 1);
            } else
                gs_key((uint32_t)note, 1);
        } else if (!on && ui.keys_down[i]) {
            sk_midi_note(0, ui.kraw[i], 0);
            if (ui.kmute[i])
                ui.kmute[i] = 0;
            else if (ui.kchord[i]) {
                ui.kchord[i] = 0;
                cd_key((int)i, 0);
            } else
                gs_key(ui.keys_down[i], 0);
            ui.keys_down[i] = 0;
        }
    }
    /* encoders */
    d = ui_enc(SE_PRESET);
    if (d) {
        int w0 = ui_world();
        ui_load_patch(GS_ORD[((int)GS_POS[gs.preset] + (d > 0 ? 1 : GS_NPRE - 1)) % GS_NPRE], now);      /* (the next in the shown order) */
        if (ui_world() != w0)                                       /* the list runs through both worlds: say so at the border */
            ui_toast(UI_WORLD[ui_world()], now);
    }
    d = ui_enc(SE_SELECT);
    if (d) {
        gs.bpm = (uint8_t)ui_clamp((int)gs.bpm + d, 40, 200);
        ui.hud_t = now;
    }
    d = ui_enc(SE_ALGO);
    if (d && (UI_SEQP() || ui.page == PG_PERF)) {   /* the pattern: A to H (running, the one picked starts at the end of this pass); on the performance page too */
        int cur = gs.pnext != 255u ? gs.pnext : gs.pcur, nx = ui_clamp(cur + (d > 0 ? 1 : -1), 0, GS_NP - 1);
        char tb[20], *tp;
        if (nx != cur || (gs.srun && gs.pnext != 255u)) {
            gs_pat_select((uint32_t)nx);
            if (!gs.srun) {
                ui.cur = 0;
                ui.shift = 0;
            }
            tp = ui_str(tb, gs.srun && gs.pnext != 255u ? "NEXT " : "PATTERN ");
            *tp++ = (char)('A' + nx);
            *tp = 0;
            ui_toast(tb, now);
        }
        ui.k_t[0] = now;
    } else if (d && UI_PG[ui.page].algo == 254u) {                 /* the home page: the other world, on the patch last used there */
        int w = ui_world(), to = (w + (d > 0 ? 1 : GS_NWORLD - 1)) % GS_NWORLD;   /* the next world round, or the one before */
        ui.wlast[w] = gs.preset;
        ui_load_patch(ui.wlast[to], now);
        ui.spl_t = now ? now : 1u;                                  /* the world's title, for a moment */
        ui_toast(UI_WORLD[to], now);
        ui.k_t[0] = now;
    } else if (d && UI_PG[ui.page].algo != 255u) {
        uint32_t idx = UI_PG[ui.page].algo;
        gs_set(idx, (int)gs.p[idx] + (d > 0 ? 1 : -1));
        ui.k_t[0] = now;
    }
    for (i = 0; i < 4u; i++) {
        d = ui_enc(SE_K1 + i);
        if (!d)
            continue;
        if (ui.page == PG_SEQ4) {                                   /* the step's extras, and the pattern's sound */
            int sg = d > 0 ? 1 : -1, quick = (d > 2 || d < -2);
            uint8_t *x = &gs.sx[gs.pcur][ui.cur];
            ui.k_t[i] = now;
            ui.cur_t = now;
            if (i == 0u)
                ui.cur = (uint8_t)ui_clamp((int)ui.cur + (quick ? 16 : 1) * sg, 0, GS_NS - 1);
            else if (i == 1u) {                                     /* REPEAT: the step sounds once, or two, three, four times in its time */
                *x = (uint8_t)((*x & ~3u) | (uint32_t)ui_clamp((int)(*x & 3u) + sg, 0, 3));
                gs.pdirty = 1;
            } else if (i == 2u) {                                   /* CHANCE: always, or three times in four, half, one in four */
                *x = (uint8_t)((*x & ~12u) | ((uint32_t)ui_clamp(SX_CH(*x) - sg, 0, 3) << 2));
                gs.pdirty = 1;
            } else {                                                /* SOUND: this pattern keeps a sound of its own (taken from the one playing now), or shares */
                uint32_t k;
                if (sg > 0 && !gs.psnd[gs.pcur]) {
                    for (k = 0; k < P_N; k++)
                        gs.pp[gs.pcur][k] = gs.p[k];
                    gs.ppre[gs.pcur] = gs.preset;
                    gs.psnd[gs.pcur] = 1;
                    gs.pdirty = 1;
                    ui_toast("OWN SOUND KEPT", now);
                } else if (sg < 0 && gs.psnd[gs.pcur]) {
                    gs.psnd[gs.pcur] = 0;
                    ui_toast("SOUND SHARED", now);
                }
            }
            continue;
        }
        if (ui.page == PG_MOD) {                                    /* the matrix: K1 picks the route, the others set it */
            uint32_t b = P_M1S + 3u * ui.mrow;
            ui.k_t[i] = now;
            if (i == 0u)
                ui.mrow = (uint8_t)ui_clamp((int)ui.mrow + (d > 0 ? 1 : -1), 0, GS_NMOD - 1);
            else if (i == 3u) {
                int v = ui_clamp((int)gs.p[b + 2u] + d, 0, 127);
                if ((gs.p[b + 2u] < 64u && v > 64) || (gs.p[b + 2u] > 64u && v < 64))   /* (it stops at none on the way through) */
                    v = 64;
                gs_set(b + 2u, v);
            } else
                gs_set(b + i - 1u, (int)gs.p[b + i - 1u] + (d > 0 ? 1 : -1));
            continue;
        }
        if (ui.page == PG_SYNC) {                                   /* the clock: K1 is the sync; the other rows only tell */
            if (i == 0u) {
                int v = ui_clamp((int)gs_sync + (d > 0 ? 1 : -1), 0, GS_NSYNC - 1);
                ui.k_t[0] = now;
                if (v != (int)gs_sync) {
                    gs_sync = (uint8_t)v;
                    ui.g_dirty = 1;
                    ui.g_t = now;
                }
            }
            continue;
        }
        if (ui.page == PG_PERF) {
            ui.k_t[i] = now;
            if (i == 0u)
                gs.mac = (uint8_t)ui_clamp((int)gs.mac + d, 0, 127);
            else if (i == 1u)
                gs.mkind = (uint8_t)ui_clamp((int)gs.mkind + (d > 0 ? 1 : -1), 0, GS_NMAC - 1);
            else if (i == 2u) {
                int v = ui_clamp((int)gs.djf + d, 0, 127);
                if ((gs.djf < 64u && v > 64) || (gs.djf > 64u && v < 64))    /* (it stops at the middle on the way through) */
                    v = 64;
                gs.djf = (uint8_t)v;
            } else {                                                /* PUMP: the gate's depth; with the gate open, it becomes the 1/4 pump */
                if (d > 0 && gs.p[P_GPAT] == 13u)
                    gs_set(P_GPAT, GS_PUMP0);
                gs_set(P_GDEP, (int)gs.p[P_GDEP] + d);
            }
            continue;
        }
        if (ui.page == PG_SEQ) {
            int sg = d > 0 ? 1 : -1, quick = (d > 2 || d < -2);
            uint8_t *st = &gs.sp[gs.pcur][ui.cur], f = *st;
            ui.k_t[i] = now;
            ui.cur_t = now;
            if (i)
                gs.pdirty = 1;
            if (i == 0u) {                                          /* STEP: the cursor (a quick turn moves a bar) */
                ui.cur = (uint8_t)ui_clamp((int)ui.cur + (quick ? 16 : 1) * sg, 0, GS_NS - 1);
            } else if (i == 1u) {                                   /* NOTE: the pitch of the step (a rest starts on the root) */
                int n = SP_ON(f) ? SP_NOTE(f) : 12;
                n = ui_clamp(n + d, 0, 36);
                *st = SP_MAKE(n, SP_ACC(f), SP_SLD(f));
                ui_seq_grow(now);
            } else if (i == 2u) {                                   /* TYPE: rest, note, accent, slide, both */
                int n = SP_ON(f) ? SP_NOTE(f) : 12, t = !SP_ON(f) ? 0 : SP_SLD(f) ? (SP_ACC(f) ? 4 : 3) : SP_ACC(f) ? 2 : 1;
                t = ui_clamp(t + sg, 0, 4);
                *st = t == 0 ? 0u : SP_MAKE(n, t == 2 || t == 4, t == 3 || t == 4);
                if (t)
                    ui_seq_grow(now);
            } else {                                                /* LENGTH: how many steps the pattern has (a quick turn: four) */
                int len = ui_clamp((int)gs.spl[gs.pcur] + (quick ? 4 : 1) * sg, 1, GS_NS);
                gs.spl[gs.pcur] = (uint8_t)len;
                if (ui.cur >= len)
                    ui.cur = (uint8_t)(len - 1);
            }
            continue;
        }
        if (ui.page == PG_SEQ3) {                                   /* tools: clear, copy from the next or last pattern, fill, shift */
            int sg = d > 0 ? 1 : -1, k;
            char tb[24], *tp;
            ui.k_t[i] = now;
            gs.pdirty = 1;
            if (i == 0u) {
                if (sg < 0) {                                       /* (to the left: only the recorded knob goes) */
                    for (k = 0; k < GS_NS; k++)
                        gs.mot[gs.pcur][k] = 0;
                    ui_toast(gs.mot_id[gs.pcur] ? "KNOB REC CLEARED" : "NO KNOB REC", now);
                    gs.mot_id[gs.pcur] = 0;
                }
                if (sg > 0) {
                    for (k = 0; k < GS_NS; k++)
                        gs.sp[gs.pcur][k] = gs.sx[gs.pcur][k] = gs.mot[gs.pcur][k] = 0;
                    gs.mot_id[gs.pcur] = 0;
                    tp = ui_str(tb, "PATTERN ");
                    *tp++ = (char)('A' + gs.pcur);
                    ui_str(tp, " CLEARED");
                    ui_toast(tb, now);
                }
            } else if (i == 1u) {
                int src = (gs.pcur + (sg > 0 ? 1 : GS_NP - 1)) % GS_NP;
                for (k = 0; k < GS_NS; k++) {
                    gs.sp[gs.pcur][k] = gs.sp[src][k];
                    gs.sx[gs.pcur][k] = gs.sx[src][k];
                    gs.mot[gs.pcur][k] = gs.mot[src][k];
                }
                gs.mot_id[gs.pcur] = gs.mot_id[src];
                gs.spl[gs.pcur] = gs.spl[src];
                tp = ui_str(tb, "COPIED FROM ");
                *tp++ = (char)('A' + src);
                *tp = 0;
                ui_toast(tb, now);
            } else if (i == 2u) {                                   /* fill: a new pattern. Trance: a busy line in the minor. House: a sparser bass line that leans on the off-beats */
                static const uint8_t SC[9] = {0, 0, 3, 5, 7, 7, 10, 12, 15}, HS[8] = {0, 0, 0, 3, 7, 10, 12, 12};
                if (sg > 0) {
                    int house = ui_world();
                    for (k = 0; k < gs.spl[gs.pcur]; k++) {
                        uint32_t r = sk_rand();
                        int on = (k & 3) == 0 ? (r & 15u) < 14u : (r & 15u) < 10u;
                        if (house)
                            on = (k & 3) == 2 ? (r & 15u) < 14u : (k & 3) == 0 ? (r & 15u) < 7u : (r & 15u) < 5u;
                        gs.sp[gs.pcur][k] = !on ? 0u : house ? SP_MAKE(12 + HS[(r >> 4) % 8u], (k & 3) == 2 && (r >> 16) % 3u == 0u, (r >> 20) % 10u == 0u)
                                                            : SP_MAKE(12 + SC[(r >> 4) % 9u] - ((r >> 12) % 6u == 0u ? 12 : 0), (r >> 16) % 5u == 0u, (r >> 20) % 8u == 0u);
                    }
                    ui.shift = 0;
                    ui_toast("FILLED", now);
                }
            } else {                                                /* shift every note, if none would leave the range */
                int ok = 1;
                for (k = 0; k < GS_NS; k++)
                    if (SP_ON(gs.sp[gs.pcur][k]) && (SP_NOTE(gs.sp[gs.pcur][k]) + sg < 0 || SP_NOTE(gs.sp[gs.pcur][k]) + sg > 36))
                        ok = 0;
                if (ok) {
                    for (k = 0; k < GS_NS; k++) {
                        uint8_t b = gs.sp[gs.pcur][k];
                        if (SP_ON(b))
                            gs.sp[gs.pcur][k] = SP_MAKE(SP_NOTE(b) + sg, SP_ACC(b), SP_SLD(b));
                    }
                    ui.shift = (int8_t)ui_clamp(ui.shift + sg, -99, 99);
                }
            }
            continue;
        }
        if (ui.page == PG_SAVE) {
            if (i == 0u)
                ui.slot = (uint8_t)ui_clamp((int)ui.slot + (d > 0 ? 1 : -1), 0, SV_N - 1);
            else if (i == 1u)
                ui.act = (uint8_t)ui_clamp((int)ui.act + (d > 0 ? 1 : -1), 0, 2);
            else if (i == 2u && (d > 0) != ui.silent) {             /* KEYS: SOUND, or SILENT (they play the USB host only) */
                ui.silent = (uint8_t)(d > 0);
                gs_all_off();
                cd_all_off();
                ui_toast(ui.silent ? "KEYS SILENT" : "KEYS SOUND", now);
            }
            ui.k_t[i] = now;
            continue;
        }
        {
            uint32_t idx = UI_PG[ui.page].p[i];
            int step = ui_is_enum(idx) || idx == P_ARNG || idx == P_OCT ? (d > 0 ? 1 : -1) : d * (d > 2 || d < -2 ? 3 : 1);
            gs_set(idx, (int)gs.p[idx] + step);
            ui.k_t[i] = now;
            if (gs.srec && gs.srun && idx < P_N && !ui_is_enum(idx)) {   /* recording, and a knob turns: it is recorded with the notes */
                ui.mot_k = (uint8_t)idx;
                ui.mot_t = now ? now : 1u;
            }
        }
    }
    /* Knob recording. While the sequence records, a sound knob that is turned is written into the pattern, a value a step, for as
     * long as it keeps moving (and a third of a second after); each pattern keeps one knob, and a different knob starts it afresh.
     * Played back, each step sets the knob to what was recorded there. */
    if (ui.mot_t && gs.srec && gs.srun && now - ui.mot_t < 350u) {
        uint32_t pc = gs.pcur, st = gs.sstep;
        gs.mot_hold = 1;
        if (gs.mot_id[pc] != ui.mot_k + 1u) {
            uint32_t k;
            for (k = 0; k < GS_NS; k++)
                gs.mot[pc][k] = 0;
            gs.mot_id[pc] = (uint8_t)(ui.mot_k + 1u);
        }
        if (st < GS_NS && !gs.scount) {
            gs.mot[pc][st] = (uint8_t)(gs.p[ui.mot_k] + 1u);
            gs.pdirty = 1;
        }
    } else {
        ui.mot_t = 0;
        gs.mot_hold = 0;
    }
    /* the lamps: the page's button, the arp, the hold, the glitch */
    {
        static const uint8_t LB[SB_N] = {PG_FX, PG_FLT, PG_ENV, PG_LFO, PG_OSC, PG_GLITCH, PG_HOME, PG_SAVE, PG_ARP, PG_SEQ, 255, 255, 255, 255};
        for (i = 0; i < SB_N; i++) {
            uint32_t lv = 0;
            if (LB[i] != 255u && (ui.page == LB[i] || (i == SB_ENV && ui.page == PG_ENV2) || (i == SB_EDIT && ui.page == PG_OSC2) || (i == SB_LFO && ui.page == PG_MOD) || (i == SB_FX && ui.page == PG_FX2) || (i == SB_GLO && ui.page == PG_GATE) || (i == SB_SEQ && UI_SEQP()) || ((i == SB_SEQ || i == SB_ARP) && ui.page == PG_SYNC) || (i == SB_HOME && (ui.page == PG_CHORD || ui.page == PG_PERF))))
                lv = 3;
            else if (i == SB_ARP && gs.p[P_AON])
                lv = 2;
            else if (i == SB_SEQ && gs.srun)
                lv = 2;
            else if (i == SB_GLO && (gs.p[P_GLP] || (gs.p[P_GDEP] && gs.p[P_GPAT] != 13u)))
                lv = 1;
            else if (i == SB_PLAY && gs.hold)
                lv = 3;
            else if (i == SB_REC && gs.srec)
                lv = gs.scount ? (((now >> 7) & 1u) ? 3u : 0u) : 3u;
            else if (i == SB_REC && gl.on)
                lv = 3;
            sk_led_btn(i, lv);
        }
        for (i = 0; i < 27u; i++) {
            int note = 48 + 12 * gs.oct + (int)i;
            uint32_t lv = 0;
            if (ui.keys_down[i] || (gs.srun && gs.snote >= 0 && gs.snote == note))   /* a key you press, or the step the sequence is playing */
                lv = 3;
            else if (GS_SLOCK()) {                                   /* the scale lock: the notes of the scale, its own note brighter */
                int rel = ((note - (int)gs.p[P_CKEY]) % 12 + 12) % 12;
                lv = rel == 0 ? 2u : cd_in_scale(rel) ? 1u : 0u;
            } else if (GS_CHORDS()) {                                /* the chord keys: the buttons that count, the roots in the key's own note */
                int v, r = cd_role((int)i, gs.p[P_CLAY], &v);
                cd_prepare();
                if (r == 1 && (cd_set(gs.p[P_CLAY]) >> v) & 1u)
                    lv = 2;
                else if (r == 2 && v % (int)cd.m == 0)
                    lv = 1;
            } else if (gs.acur == note || gs_has((uint32_t)note))
                lv = 2;
            else if (i % 12u == 0u)
                lv = 1;
            sk_led_key(i, lv);
        }
    }
}

/* ---------------------------------------------------------------------- drawing --- */
#define VX 9                                     /* the window's inside */
#define VY 26
#define VW 222
#define VH 90

/* a panel that floats over the rest (a toast, a pop-up): ink, a line of its edge colour, a dark ground. Square: nothing here is round. */
static void ui_glass(int x, int y, int w, int h, uint8_t edge)
{
    fb_rect(x + 2, y + 2, w, h, C_OUT);                                       /* its hard shadow */
    fb_rect(x, y, w, h, C_OUT);
    fb_rect(x + 1, y + 1, w - 2, h - 2, C_K1);
    fb_frame(x + 1, y + 1, w - 2, h - 2, edge == C_S0 ? C_INK : edge);
}

/* the window's frame: the scene or the well sits inside its inner line */
static void ui_window(void)
{
    fb_win(VX - 4, VY - 4, VW + 8, VH + 8);
}
/* text with its hard shadow, a pixel down and right */
static int ui_text(int x, int y, const char *s, uint8_t c)
{
    fb_text(x + 1, y + 1, s, C_BG);
    return fb_text(x, y, s, c);
}
static int ui_text_r(int xr, int y, const char *s, uint8_t c) { return ui_text(xr - fb_text_w(s), y, s, c); }
/* a tab of gold with its name in ink: a window's or a page's name */
static void ui_tab(int x, int y, const char *s)
{
    int w = fb_text_w(s) + 7;
    fb_rect(x, y, w, 13, C_OUT);
    fb_rect(x, y, w - 1, 12, C_O5);
    fb_hline(x, y, w - 1, C_O7);
    fb_text(x + 3, y - 1, s, C_OUT);
}

#include "gs_3d.c"

static int ui_lv(uint32_t idx) { return gs.p[idx]; }

static uint32_t ui_hash(uint32_t x);
/* the backdrop of the window. The home page looks out on the world's scene. Every other page is a well of night; under what stands
 * up in it (the towers, the bars, the steps) there is a floor: a horizon, lines that run to it, and lines that come toward you at
 * the tempo. */
static void vz_back(uint32_t now)
{
    int i, hy = VY + 32, cx = VX + VW / 2, fh = VY + VH - hy;
    int flat = ui.page == PG_HOME || ui.page == PG_FLT || ui.page == PG_ENV || ui.page == PG_ENV2 || ui.page == PG_LFO || ui.page == PG_MOD ||
               ui.page == PG_PERF || ui.page == PG_SAVE || ui.page == PG_CHORD || ui.page == PG_FX || ui.page == PG_FX2 || ui.page == PG_SYNC || UI_SEQP();
    uint32_t scroll = (now * (uint32_t)(gs.bpm ? gs.bpm : 120) / 40u) & 2047u;
    fb_clip(VX, VY, VX + VW, VY + VH);
    fb_rect(VX, VY, VW, VH, C_BG);
    if (ui.page == PG_HOME)
        fb_scene(ui_world(), SK_SCENE[(uint32_t)ui_world() % FB_NTHEME].wy, VX, VY, VW, VH);
    else if (!flat) {
        for (i = 0; i < 40; i++) {                                           /* a few stars over the horizon, some of them blinking in steps */
            uint32_t h = ui_hash((uint32_t)i * 7919u + 11u);
            if (((h >> 20) & 7u) != ((now >> 9) & 7u))
                fb_pset(VX + (int)(h % (uint32_t)VW), VY + (int)((h >> 9) % 30u), (h >> 16) & 1u ? C_S4 : C_S2);
        }
        fb_hline(VX, hy, VW, C_S2);
        for (i = -8; i <= 8; i++)
            fb_line(cx, hy, cx + i * 34, VY + VH, C_GRID0);
        for (i = 0; i < 8; i++) {
            uint32_t u = ((uint32_t)i * 256u + scroll) & 2047u;
            int y = hy + 1 + (int)((uint32_t)fh * ((u * u) >> 11) >> 11);
            fb_hline(VX, y, VW, y > hy + fh / 2 ? C_GRID1 : C_GRID0);
        }
    }
    fb_unclip();
}

static uint32_t ui_hash(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    return x ^ (x >> 16);
}

/* a thick line in a well: gold, lit along its top, ink under it */
static void vz_dot(int x, int y, uint8_t core, uint8_t glow)
{
    (void)core;
    (void)glow;
    fb_pset(x, y - 1, C_O7);
    fb_pset(x, y, C_O5);
    fb_pset(x, y + 1, C_O3);
    fb_pset(x, y + 2, C_OUT);
}

/* the oscillator as towers on the floor: a tower to a saw (or pulse, or sine) of the patch's oscillator, each rising at its
 * own rate so that the detune is seen as the towers drifting in and out of step */
static int vz_tower_h(int mode, int j, uint32_t now, int det, int col, int fbk)
{
    uint32_t r = 1300u + (uint32_t)((j - 3) * det * 2 + 3 * det * 2) / 2u;
    uint32_t ph = (now * r / 1000u) & 1023u;
    int h = 0;
    switch (mode) {
    case 0: h = (int)(ph * 54u / 1024u); break;
    case 1: h = (int)(((j & 1) ? ph * (uint32_t)(2 + col / 32) : ph) & 1023u) * 54 / 1024; break;
    case 2: h = (int)ph < 100 + col * 6 ? 54 : 12; break;
    case 3: h = j == 1 ? ((int)ph < 512 ? 54 : 8) : (int)(ph * 54u / 1024u); break;
    case 5: h = 16 + (SK_SIN[(ph / 2u + (uint32_t)j * 170u) & 511u] * (20 + col / 4)) / 32768; break;      /* tines: each rings up and falls */
    case 6: {                                                                                             /* drawbars: 1, 2, 3, 4, and the pair a little off */
        static const uint8_t W[6] = {40, 0, 0, 0, 30, 0};
        int w = j == 1 || j == 5 ? 12 + col * 3 / 10 : j == 2 ? 6 + col * 2 / 5 : j == 3 ? (col > 50 ? (col - 50) * 2 / 5 + 6 : 6) : W[j];
        h = w + SK_SIN[(ph + (uint32_t)j * 200u) & 1023u] * 2 / 32768;
        break;
    }
    case 7: h = 6 + (SK_SIN[(ph * 2u) & 511u] * 26) / 32768; break;                                      /* a ball that is struck and bounces */
    case 8: h = 14 + (int)(ui_hash((uint32_t)j * 77u + (now >> 7)) % (uint32_t)(10 + col / 3)) + (j & 1) * 12; break;   /* metal: shards that jitter */
    case 9: h = 4 + (int)(ui_hash((uint32_t)j * 131u + (now >> 5)) % 44u); break;                         /* noise: every bar somewhere new, all the time */
    case 10: h = 30 + SK_SIN[(ph / 3u + (uint32_t)j * 512u) & 1023u] * (8 + det / 6) / 32768; break;      /* reese: two slow masses leaning against each other */
    default: h = 30 + SK_SIN[(ph + (uint32_t)(SK_SIN[(ph * 2u) & 1023u] * fbk / 32768)) & 1023u] * 24 / 32768; break;
    }
    return h < 6 ? 6 : h;
}

static void vz_towers(uint32_t now)
{
    static const uint8_t NT[GS_NMODE] = {7, 6, 3, 3, 5, 5, 6, 3, 4, 7, 3};
    int mode = gs.p[P_MODE] % GS_NMODE, n = NT[mode], j, det = ui_lv(P_DET), mix = ui_lv(P_MIX), col = ui_lv(P_COL);
    int sp = 190 / n, wpx = n > 5 ? 13 : (n > 3 ? 18 : 28), bias = gs.level > 14 ? 2 : 0;
    g3_cam(VX + VW / 2, VY + 60, 250, 250, 40);
    for (j = 0; j < n; j++) {
        int h = vz_tower_h(mode, j, now, det, col, ui_lv(P_COL)) * 3 / 4, w = wpx;
        if (mode == 0 && j == 3)
            w = wpx * (70 + mix) / 120;
        if (mode == 5)                                                             /* FM keys: gems that ring */
            g3_draw(&G3_OCT, (j - 2) * sp, h - 2, 0, 4, 7, 4, 0, (int)(now >> 3) + j * 120, 0, G3_CHR, 8, bias, C_O6);
        else if (mode == 8)                                                        /* metal: thin blades */
            g3_draw(&G3_PYR, (j * 2 - 3) * sp / 2, h / 2, 0, 3, h / 4 + 1, 5, 0, (int)(now >> 3) + j * 200, 0, G3_CHR, 8, bias, C_HOT);
        else if (mode == 7)                                                        /* donk: three balls bouncing a ratio apart */
            g3_draw(&G3_ICO, (j - 1) * sp, h + 6, 0, 6, 6 - (h < 12 ? (12 - h) / 3 : 0), 6, (int)(now >> 3), (int)(now >> 4) + j * 90, 0, G3_CHR, 8, bias, j == 1 ? C_HOT : C_O6);
        else
            g3_draw(&G3_CUBE, (j - (n - 1) / 2) * sp + (n & 1 ? 0 : sp / 2), h / 2, 0, w / 4 + 1, h / 4 + 1, w / 4 + 1,
                    0, mode == 6 ? 0 : (int)(now >> 4) + j * 90, 0, G3_CHR, 8, bias, mode == 0 && j == 3 ? C_HOT : C_O6);
    }
}

/* a flame at (x, y), its foot there: k its size, h how tall it stands now */
static void vz_flame(int x, int y, int k, int h)
{
    int r;
    for (r = 0; r < h; r++) {
        int hw = k * (h - r) / h;
        fb_hline(x - hw, y - r, 2 * hw + 1, r * 3 < h ? C_G4 : C_O5);
        if (hw > 1 && r * 2 < h)
            fb_hline(x - hw / 2, y - r, 2 * (hw / 2) + 1, r * 4 < h ? C_O7 : C_HOT);
    }
}
/* The home page looks out on the world's scene (vz_back drew it), and the scene answers the notes. Each world has its own way:
 * the castle's windows light, the cathedral's candles leap, the dungeon's ghosts rise and fall, the shrine's gem and braziers flare,
 * the forest's fireflies wake, the forge's lava spits. At rest each still moves a little, in steps. */
static void vz_home(uint32_t now)
{
    int w = (int)((uint32_t)ui_world() % FB_NTHEME), n = SCN_NSPOT[w], i, e[16], emax = 0;
    for (i = 0; i < 16; i++)
        e[i] = 0;
    for (i = 0; i < GS_NV; i++) {                                           /* a sounding voice wakes the place its note falls on (and, where */
        int lv = gs.v[i].on ? (int)(gs.v[i].ae >> 13) + 1 : 0, a = gs.v[i].note % n, c = (gs.v[i].note * 7 + 5) % n;   /* there are many, a second) */
        if (lv > e[a])
            e[a] = lv;                                                      /* 0: silent, else 1..8 */
        if (n > 8 && lv > e[c])
            e[c] = lv;
        if (lv > emax)
            emax = lv;
    }
    for (i = 0; i < (int)gs.nh && i < 8; i++) {                             /* and so does every key that is held (a patch of one voice still lights a chord) */
        int a = gs.hn[i] % n, c = (gs.hn[i] * 7 + 5) % n;
        if (e[a] < 5)
            e[a] = 5;
        if (n > 8 && e[c] < 5)
            e[c] = 5;
        if (emax < 5)
            emax = 5;
    }
    for (i = 0; i < n; i++) {
        int x = VX + SCN_SPOT[w][i][0], y = VY + SCN_SPOT[w][i][1], k = SCN_SPOT[w][i][2], ev = e[i];
        uint32_t h = ui_hash((uint32_t)i * 2654435761u + (uint32_t)w);
        switch (w) {
        case 0:                                                              /* a window of the towers for each voice */
            if (ev) {
                fb_rect(x - 1, y - 1, 4, 6, C_O5);
                fb_rect(x, y, 2, 4, C_HOT);
                if (ev > 4)
                    fb_spark(x, y + 1, 3, C_O7);
            } else if (((now >> 10) + h) % 9u == 0u)                         /* at rest: now and then one is dark */
                fb_rect(x, y, 2, 4, C_O2);
            break;
        case 1:                                                              /* the candles: they flicker, and leap with a voice */
            vz_flame(x, y, k + (ev > 3), k * 2 + 2 + (int)(((now >> 7) + h) & 1u) + ev * (k + 1) / 2);
            break;
        case 2: {                                                            /* the ghosts: they hang, and bob with the notes */
            const uint8_t *g = SPR_GHOST[k];
            int gw = g[0], gh = g[1], amp = 1, j, xx, yy, dy;
            (void)j;
            if (ev)
                amp = 1 + ev;
            dy = SK_SIN[((now * (uint32_t)(amp > 1 ? 3 : 1)) / 4u + h) & 1023u] * (amp > 1 ? 2 + amp : 2) / 32768;
            for (yy = 0; yy < gh; yy++)
                for (xx = 0; xx < gw; xx++) {
                    uint8_t c = g[2 + yy * gw + xx];
                    if (c)
                        fb_pset(x - gw / 2 + xx, y - gh / 3 + yy + dy, c == 1 ? C_S5 : c == 2 ? C_HOT : amp > 1 ? C_R3 : C_OUT);
                }
            break;
        }
        case 3:                                                              /* the shrine: the gem, the braziers, motes that rise from the sword */
            if (i == 0) {
                if (emax)
                    fb_spark(x, y, 2 + emax, C_R3);
                fb_rect(x - 1, y - 1, 3, 3, emax ? C_HOT : ((now >> 9) & 1u) ? C_R4 : C_R3);
            } else if (k == 4)
                vz_flame(x, y, 3 + (emax > 4), 7 + (int)(((now >> 7) + h) & 1u) + emax);
            else if (ev)
                fb_pset(x + (int)((now >> 8) & 1u), y - (int)(((now >> 6) + h) % 14u), ((now >> 6) + h) % 14u > 9u ? C_R1 : C_R4);
            break;
        case 4: {                                                            /* the fireflies: a few glow at rest, a voice wakes its own */
            uint32_t ph = ((now >> 8) + h) & 15u;
            if (ev) {
                int dx = (int)((now >> 7) + h) % 3 - 1;
                fb_pset(x + dx - 1, y, C_WORLD); fb_pset(x + dx + 1, y, C_WORLD); fb_pset(x + dx, y - 1, C_WORLD); fb_pset(x + dx, y + 1, C_WORLD);
                fb_pset(x + dx, y, C_HOT);
            } else if (ph < 3u)
                fb_pset(x, y, ph == 1u ? C_WORLD : C_G3);
            break;
        }
        default:                                                             /* the forge: a voice throws lava up in an arc; at rest it bubbles */
            if (ev) {
                int j;
                for (j = 0; j < 3; j++) {
                    int t = (int)((now * 2u + h + (uint32_t)j * 170u) & 511u), hgt = 26 + ev * 5 - j * 6;
                    int yy = y - hgt * t * (511 - t) / 65280, xx = x + (t - 256) * ((int)(h >> 8 & 15u) - 7) / 96 + j * 3 - 3;
                    fb_rect(xx - 1, yy - 1, 3, 3, t < 300 ? C_O6 : C_G4);
                    fb_pset(xx, yy, t < 200 ? C_HOT : C_O7);
                }
            } else if (((now >> 8) + h) % 7u == 0u)
                fb_rect(x + (int)(h & 7u), y - 1, 2, 2, C_O6);
            break;
        }
    }
}

/* the gate's strip along the bottom of the window: sixteen cells, the open ones lit, the step the playhead is on bright */
static void vz_gate_strip(int y, int h)
{
    int i;
    const char *pat = GS_GT_STR[gs.p[P_GPAT] % GS_NPAT];
    for (i = 0; i < 16; i++) {
        char c = pat[i];
        int on = c == '?' ? (int)((gs.rmask >> i) & 1u) : c == 'x';
        int x = VX + 4 + i * 13;
        if (!gs.p[P_GDEP])
            on = 1;
        if (on)
            fb_chrome_tile(x, y, 11, h);
        else
            fb_rect(x, y, 11, h, C_K3);
        if (i == (gs.gstep & 15))
            fb_rect(x + 1, y + 1, 9, h - 2, C_O5);
    }
}

/* the filter's curve: the cutoff along a log axis (the table index), its resonance as a bump, its slope by the kind */
static const uint8_t UI_VOWX[5][2] = {{68, 75}, {60, 84}, {47, 87}, {59, 70}, {50, 65}};
static int vz_filter_db(int xi, int cut, int res, int ft)
{
    int dist = xi - cut, a = dist < 0 ? -dist : dist, db = 0, bump = res * 22 / 100;
    if (ft == 0)
        db = dist > 0 ? -dist * 9 / 10 : 0;
    else if (ft == 1)
        db = dist > 0 ? -dist * 9 / 5 : 0;
    else if (ft == 2)
        db = -a / 2;
    else if (ft == 3)
        db = dist < 0 ? dist * 9 / 10 : 0;
    if (ft != 4 && a < 8)
        db += bump * (64 - a * a) / 64;
    if (ft == 4) {
        int pos = cut * 4 / 127, vf, d1, d2, f1, f2;
        pos = ui_clamp(pos, 0, 3);
        vf = cut * 4 * 256 / 127 - pos * 256;
        f1 = UI_VOWX[pos][0] + (UI_VOWX[pos + 1][0] - UI_VOWX[pos][0]) * vf / 256;
        f2 = UI_VOWX[pos][1] + (UI_VOWX[pos + 1][1] - UI_VOWX[pos][1]) * vf / 256;
        d1 = xi - f1;
        d2 = xi - f2;
        d1 = d1 < 0 ? -d1 : d1;
        d2 = d2 < 0 ? -d2 : d2;
        db = -32 + (d1 < 8 ? (8 - d1) * 4 : 0) + (d2 < 8 ? (8 - d2) * 3 : 0) + bump / 2;
    }
    return db;
}

static void vz_filter(uint32_t now)
{
    int x, cut = ui_lv(P_CUT), res = ui_lv(P_RES), ft = ui_lv(P_FTYPE), live = (int)(gs.cut_idx >> 8), py = -1;
    (void)now;
    if (gs.note_t || gs.v[0].on || gs.v[1].on || gs.v[2].on)
        cut = live > 0 ? live : cut;
    for (x = 0; x < VW; x++) {
        int xi = x * 127 / VW, db = vz_filter_db(xi, cut, res, ft), y = VY + 22 + (30 - db) * (VH - 30) / 100;
        y = ui_clamp(y, VY + 3, VY + VH - 2);
        if (py >= 0 && (y - py > 1 || py - y > 1)) {
            int lo = y < py ? y : py, hi = y < py ? py : y, k;
            for (k = lo; k <= hi; k++)
                fb_pset(VX + x, k, C_S5);
        }
        fb_vline(VX + x, y, VY + VH - y, (x & 1) ? C_O1 : C_O0);
        vz_dot(VX + x, y, 0, 0);
        py = y;
    }
    fb_vline(VX + cut * VW / 127, VY, VH, C_O5);
}

/* an envelope: attack, decay to the sustain, a hold, the release */
static void vz_env(uint32_t a, uint32_t d, uint32_t s, uint32_t r, uint32_t now, int fenv)
{
    int pa = 6 + (int)gs.p[a] * 46 / 127, pd = 6 + (int)gs.p[d] * 62 / 127, pr = 6 + (int)gs.p[r] * 56 / 127, ph = 36;
    int x0 = VX + 6, bot = VY + VH - 6, top = VY + 8, sus = bot - (bot - top) * (int)gs.p[s] / 127;
    int xs[6], ys[6], i, x, y;
    xs[0] = x0; ys[0] = bot;
    xs[1] = x0 + pa; ys[1] = top;
    xs[2] = xs[1] + pd; ys[2] = sus;
    xs[3] = xs[2] + ph; ys[3] = sus;
    xs[4] = xs[3] + pr; ys[4] = bot;
    for (i = 0; i < 4; i++) {
        int dx = xs[i + 1] - xs[i];
        for (x = 0; x <= dx; x++) {
            int yy = ys[i] + (ys[i + 1] - ys[i]) * x / (dx ? dx : 1);
            if (i == 0)
                yy = ys[0] + (ys[1] - ys[0]) * ((x * x) / (dx ? dx : 1)) / (dx ? dx : 1) ;
            fb_vline(xs[i] + x, yy, bot - yy, ((xs[i] + x) & 1) ? C_O1 : C_O0);
            vz_dot(xs[i] + x, yy, 0, 0);
        }
    }
    fb_hline(xs[3], bot, 1, C_O5);
    y = 0;
    for (i = 0; i < GS_NV; i++)                                               /* the level now, at the left */
        if (gs.v[i].on)
            y = (int)((fenv ? gs.v[i].fe : gs.v[i].ae) >> 9) > y ? (int)((fenv ? gs.v[i].fe : gs.v[i].ae) >> 9) : y;
    y = y > 127 ? 127 : y;
    fb_rect(VX + VW - 10, bot - (bot - top) * y / 127, 6, (bot - top) * y / 127, C_O5);
    fb_frame(VX + VW - 11, top - 1, 8, bot - top + 2, C_S2);
    (void)now;
}

static void ui_fit_c(int cx, int y, const char *s, uint8_t c, int w);
/* the matrix: its three routes, each a line: the source, an arrow as long as the amount (to the left of the middle when it pulls
 * the other way), the destination. The route the knobs are on is lit. */
static void vz_matrix(uint32_t now)
{
    uint32_t r;
    for (r = 0; r < GS_NMOD; r++) {
        uint32_t b = P_M1S + 3u * r, src = gs.p[b] % GS_NMSRC, to = gs.p[b + 1u] % GS_NMDST;
        int a = (int)gs.p[b + 2u] - 64, y = VY + 17 + (int)r * 24, sel = r == ui.mrow, live = src && a, x0 = VX + 76, x1 = VX + 144, k, tw;
        char nb[6], *q = nb;
        fb_rect(VX + 3, y, VW - 6, 22, sel ? C_SEL : C_ROW);
        fb_frame(VX + 3, y, VW - 6, 22, sel ? C_R3 : C_S2);
        if (sel)
            fb_sword(VX + 6, y + 7);
        else {
            ui_num(nb, (int)r + 1);
            fb_text(VX + 9, y + 4, nb, C_INK2);
        }
        ui_fit_c(VX + 48, y + 4, GS_MSRC_NAME[src], src ? C_HOT : C_DIM2, 52);
        fb_rect(x0, y + 9, x1 - x0, 5, C_OUT);                                /* the string: a vein from the source to what it moves */
        fb_rect(x0, y + 10, x1 - x0, 3, live ? C_R1 : C_VEIN);
        if (live)                                                            /* alive: its blood runs toward what it moves (back, when the amount is negative) */
            for (k = 0; k < x1 - x0; k++) {
                int ph = (k + (a < 0 ? (int)(now >> 6) : -(int)(now >> 6))) & 15;
                if (ph < 6)
                    fb_pset(x0 + k, y + 11, C_R4);
            }
        if (src) {                                                           /* the amount, on a tag in the middle of it */
            if (a > 0)
                *q++ = '+';
            ui_num(q, a);
            tw = fb_text_w(nb) + 8;
            fb_rect((x0 + x1) / 2 - tw / 2, y + 4, tw, 14, C_OUT);
            fb_frame((x0 + x1) / 2 - tw / 2, y + 4, tw, 14, sel ? C_R3 : C_INK);
            fb_text_c((x0 + x1) / 2, y + 4, nb, a ? C_HOT : C_DIM);
        }
        ui_fit_c(VX + 182, y + 4, GS_MDST_NAME[to], src ? C_HOT : C_DIM2, 60);
    }
}

static void vz_lfo(uint32_t now)
{
    int x, cy = VY + 40, amp = 4 + ui_lv(P_LAMT) * 30 / 127, shape = ui_lv(P_LSHP), prev = 0;
    uint32_t ph0 = (uint32_t)(((uint64_t)now * GS_LR[ui_lv(P_LRATE)] * 1378u) >> 10);
    for (x = 0; x < VW; x++) {
        uint32_t ph = ph0 + (uint32_t)x * 38000000u;
        int v, y;
        switch (shape) {
        case 0: v = SK_SIN[(ph >> 22) & 1023u]; break;
        case 1: { int t = (int)(ph >> 16) & 0xFFFF; v = (t < 32768 ? t : 65535 - t) * 2 - 32768; break; }
        case 2: v = (ph & 0x80000000u) ? -28000 : 28000; break;
        default: v = (int)((ui_hash(ph >> 24)) & 0xFFFFu) - 32768; break;
        }
        y = cy - v * amp / 32768;
        if (x && (y - prev > 1 || prev - y > 1)) {
            int lo = y < prev ? y : prev, hi = y < prev ? prev : y, k;
            for (k = lo; k <= hi; k++)
                vz_dot(VX + x, k, C_O6, C_O3);
        } else
            vz_dot(VX + x, y, C_O6, C_O3);
        prev = y;
    }
    fb_hline(VX, cy, VW, C_GRID1);
}

/* The effects, as what they do to a note. Page 1: the note (gold) and its echoes (pink), as many and as tall as the echo and its
 * feedback make them and as far apart as its time; the chorus stands a paler copy beside each; the reverb is a tail of dots that
 * falls away behind them. Page 2: a wave, cut into steps by the crush, shaken sideways by the tape, the room a box round it. */
static void vz_fx(uint32_t now)
{
    int i, k, base = VY + VH - 8;
    fb_well(VX + 2, VY + 15, VW - 4, VH - 17);
    if (ui.page == PG_FX) {
        static const uint8_t SPC[6] = {16, 24, 30, 36, 20, 28};
        int dly = ui_lv(P_DLY), fb_ = ui_lv(P_DFB), cho = ui_lv(P_CHOR), rev = ui_lv(P_REV), rsz = ui_lv(P_RSZ);
        int sp = SPC[gs.p[P_DTIM] % 6u], h = 56, beat = (int)((now * (uint32_t)(gs.bpm ? gs.bpm : 120) / 15000u) % 8u);
        ui_text_r(VX + VW - 12, VY + 1, GS_DT_NAME[gs.p[P_DTIM] % 6u], C_INK2);
        for (k = 0; k < rev * (60 + rsz) / 127; k += 3) {                    /* the hall's tail */
            int x = VX + 22 + k, y = base - 2 - (30 * rev / 127) * (200 - k) / 200;
            if (x < VX + VW - 6)
                fb_pset(x, y < base - 2 ? y : base - 2, C_S4);
        }
        for (i = 0; i < 8; i++) {
            int x = VX + 8 + i * sp, hh = i ? h * dly / 127 : h, y;
            for (k = 1; k < i; k++)
                hh = hh * (30 + fb_ * 90 / 127) / 127;
            if (x > VX + VW - 18 || hh < 3)
                break;
            if (cho > 8)                                                     /* the chorus: a paler copy, a little aside */
                for (y = base - hh + 2; y < base; y += 4)
                    fb_rect(x + 3 + cho / 26, y, 9, 2, i ? C_R0 : C_O2);
            for (y = base - hh; y < base; y += 4)
                fb_rect(x, y, 9, 3, i ? (i == beat ? C_R4 : i > 3 ? C_R1 : C_R3) : C_O5);
            if (!i)
                fb_hline(x, base - hh, 9, C_O7);
        }
        fb_hline(VX + 6, base, VW - 12, C_S2);
    } else {
        int cr = ui_lv(P_CRU), tp = ui_lv(P_TAPE), rsz = ui_lv(P_RSZ), cy = VY + 53, st = 1 + cr * 9 / 127, q = 1 + cr * 12 / 127, py = 0;
        int rw = 40 + rsz * 60 / 127;
        fb_frame(VX + VW / 2 - rw, cy - 12 - rsz * 22 / 127, 2 * rw, 24 + rsz * 44 / 127, C_K3);            /* the room */
        for (k = 0; k < VW - 16; k++) {
            int wob = tp ? SK_SIN[((now >> 1) + (uint32_t)k * 9u) & 1023u] * (tp / 12) / 32768 : 0;
            int x = VX + 8 + k, y = cy - SK_SIN[((uint32_t)(k + wob) * 10u + (now >> 2)) & 1023u] * 26 / 32768;
            fb_pset(x, y, C_S2);                                             /* the wave as it was */
            if (k % st == 0)
                py = cy + ((y - cy) / q) * q;                                /* and as the crush leaves it: held, and rounded to a step */
            fb_rect(x, py - 1, 1, 3, C_O5);
            fb_pset(x, py - 1, C_O7);
            if (k % st == 0 && st > 2)
                fb_vline(x, py < cy ? py : cy, py < cy ? cy - py : py - cy, C_O3);
        }
    }
}

/* the arpeggio: the sequence its pattern makes of the notes held (a minor chord to show while none are), a bar on the floor a step */
static void vz_arp(uint32_t now)
{
    static const int8_t DEMO[3] = {0, 3, 7};
    int n = gs.nh ? (int)gs.nh : 3, i, nr = gs.p[P_ARNG] ? gs.p[P_ARNG] : 1, pat = gs.p[P_APAT], total = n * nr, steps = 16;
    uint8_t sorted[8];
    int base = 100;
    g3_cam(VX + VW / 2, VY + 62, 260, 260, 60);
    for (i = 0; i < n; i++)
        sorted[i] = gs.nh ? gs.hn[i] : (uint8_t)(60 + DEMO[i]);
    for (i = 1; i < n; i++) {
        uint8_t t = sorted[i];
        int j = i;
        for (; j > 0 && sorted[j - 1] > t; j--)
            sorted[j] = sorted[j - 1];
        sorted[j] = t;
    }
    for (i = 0; i < steps; i++) {
        int idx, note;
        if (pat == 1)
            idx = total - 1 - (i % total);
        else if (pat == 2) {
            int per = total > 1 ? 2 * total - 2 : 1, ph = i % per;
            idx = ph < total ? ph : per - ph;
        } else if (pat == 3)
            idx = (int)(ui_hash((uint32_t)i * 31u + (now >> 9)) % (uint32_t)total);
        else
            idx = i % total;
        if (pat == 6)
            note = sorted[0] + ((i & 1) ? 12 : 0);
        else if (pat == 7)
            note = sorted[0] + GS_ROLL[i & 7];
        else if (pat == 5)
            note = sorted[(i % n)];
        else
            note = sorted[idx % n] + 12 * (idx / n);
        {
            int h = 8 + (note - base + 40) * 2, on = gs.p[P_AON], cur = i == (gs.astep ? (int)((gs.astep - 1) & 15) : 0);
            h = ui_clamp(h, 8, VH - 30);
            g3_draw(&G3_CUBE, i * 13 - 97, h / 2, 0, 3, h / 4 + 1, 4, 0, 0, 0, on ? G3_CHR : C_S0, on ? 8 : 4, cur ? 5 : 0, on ? (cur ? C_HOT : C_O6) : C_S3);
        }
    }
}

/* the gate's pattern, big: sixteen slabs on the floor, the open ones tall, the closed ones as low as the depth leaves them */
static void vz_gate(uint32_t now)
{
    int i, depth = ui_lv(P_GDEP);
    const char *pat = GS_GT_STR[gs.p[P_GPAT] % GS_NPAT];
    (void)now;
    g3_cam(VX + VW / 2, VY + 62, 260, 260, 60);
    for (i = 0; i < 16; i++) {
        char c = pat[i];
        int on = c == '?' ? (int)((gs.rmask >> i) & 1u) : c == 'x', cur = i == (gs.gstep & 15);
        int h = on ? 40 : 6 + 34 * (127 - depth) / 127;
        if (gs.p[P_GPAT] >= GS_PUMP0) {                                            /* a pump: the slabs trace the duck and the swell back */
            int per = gs.p[P_GPAT] == GS_PUMP0 ? 4 : 2, t = 1024 + ui_lv(P_GSMO) * 24, x = ((i % per) * 4096 / per) * 4096 / t;
            int down = x >= 4096 ? 0 : 4096 - x;
            down = (down * down) >> 12;
            h = 40 - 34 * depth / 127 * down / 4096;
            on = (i % per) != 0;
        }
        g3_draw(&G3_CUBE, i * 13 - 97, h / 2, 0, 3, h / 4 + 1, 4, 0, 0, 0, G3_CHR, 8, cur ? 5 : 0, cur ? C_HOT : (on ? C_O6 : C_O3));
    }
}

/* a note's name: the key's own note plus the step's semitones (the FM-1's lowest key is C3, note 48) */
static const char *const UI_NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static char *ui_note_name(char *p, int note)
{
    note = note < 0 ? 0 : note > 127 ? 127 : note;
    p = ui_str(p, UI_NN[note % 12]);
    return ui_num(p, note / 12 - 1);
}
static int ui_step_note(int s) { return (int)gs.sroot + SP_NOTE(gs.sp[gs.pcur][s]) - 12; }

/* the sequencer: the pattern tabs A to D along the top (the one playing lit, the one queued blinking), the bar you are in as sixteen
 * columns on the floor (tall by the note, a rest a flat slab, an accent hot, a slide with a bridge to the next), a beat ruler under it
 * with the playhead running along it, the note's name over the step you are on, and under that a map of the whole pattern: the bar in view
 * framed, the playhead lit, the steps past the length dim. While it runs the view follows the playhead; while you record, the steps you
 * write flash as they land, and a bar of count-in numbers comes first. */
/* the key the pattern is in: every note's pitch class is counted and the twenty-four major and minor scales are scored on how many
 * notes fall inside them, the tonic and the fifth weighing more, and the third telling major from its relative minor.
 * Returns the tonic 0..11 and sets *minor, or -1 when there are fewer than three different notes to go on. */
static int ui_seq_key(int pc, int *minor)
{
    static const uint8_t MAJ[12] = {1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1}, MIN[12] = {1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0};
    int cnt[12] = {0}, i, t, m, best = -1, bs = -1, distinct = 0, len = gs.spl[pc];
    for (i = 0; i < len; i++)
        if (SP_ON(gs.sp[pc][i]))
            cnt[(gs.sroot + SP_NOTE(gs.sp[pc][i]) - 12 + 1200) % 12]++;
    for (i = 0; i < 12; i++)
        distinct += cnt[i] > 0;
    if (distinct < 3)
        return -1;
    for (t = 0; t < 12; t++)
        for (m = 0; m < 2; m++) {
            const uint8_t *sc = m ? MIN : MAJ;
            int in = 0, sco;
            for (i = 0; i < 12; i++)
                in += sc[(i - t + 12) % 12] ? cnt[i] : 0;
            sco = 8 * in + 3 * cnt[t] + cnt[(t + 7) % 12] + 2 * cnt[(t + (m ? 3 : 4)) % 12];
            if (sco > bs) {
                bs = sco;
                best = t;
                *minor = m;
            }
        }
    return best;
}

static void vz_seq(uint32_t now)
{
    int i, pc = gs.pcur, len = gs.spl[pc], run = gs.srun, edit = ui.page != PG_SEQ2;
    int follow = run && (ui.page != PG_SEQ || now - ui.cur_t > 2500u), pos = (gs.scount || gs.sstep == 255u) ? 0 : gs.sstep;
    int vb = (follow ? pos : ui.cur) / 16, b0 = vb * 16, nb = (len + 15) / 16, showcur = edit && !follow;
    char b[24], *p;
    if (ui.rec_seen != gs.rec_n) {
        ui.rec_seen = gs.rec_n;
        ui.rec_t = now;
    }
    if (nb < vb + 1)
        nb = vb + 1;
    for (i = 0; i < 16; i++) {                                              /* the notes: a gem a step, higher for a higher note, a colour a beat */
        int s = b0 + i, out = s >= len, f = gs.sp[pc][s], on = SP_ON(f), x = VX + 6 + i * 13, y = VY + 53 - SP_NOTE(f) * 21 / 36;
        int playing = run && !gs.scount && s == pos, fresh = gs.rec_n && s == gs.rec_step && now - ui.rec_t < 350u;
        fb_vline(x - 1, VY + 31, 29, (i & 3) ? C_GRID0 : C_K3);
        if (playing) {                                                      /* the playhead: a line down the column, a pink diamond on top */
            fb_vline(x + 5, VY + 31, 29, C_INK);
            fb_diamond(x + 5, VY + 32, 2, C_R3);
        }
        if (!on) {
            if (!out)
                fb_hline(x + 3, VY + 58, 5, C_K3);
            continue;
        }
        if (!out && SP_SLD(f) && i < 15 && s + 1 < len)                      /* a slide: a bridge to the next */
            fb_hline(x + 9, y + 3, 6, C_O4);
        if (out)
            fb_gem(x, y, 11, 7, C_K3, C_K3, C_K2);
        else if (fresh)
            fb_gem(x, y, 11, 7, C_HOT, C_HOT, C_R4);
        else if (SP_ACC(f))
            fb_gem(x, y - 1, 11, 8, C_R3, C_R4, C_R1);
        else
            fb_gem(x, y, 11, 7, (uint8_t)(C_G0 + (i >> 2)), (uint8_t)(C_G0 + (i >> 2)), (uint8_t)(C_GD0 + (i >> 2)));
        if (playing) {
            fb_frame(x - 1, y - 1, 13, 9, C_R3);
            fb_frame(x, y, 11, 7, C_HOT);
        }
    }
    /* the pattern tabs: A to H */
    for (i = 0; i < GS_NP; i++) {
        int x = VX + 5 + i * 15, cur = i == pc, nxt = gs.pnext == i, filled = 0, k;
        char t[2];
        t[0] = (char)('A' + i);
        t[1] = 0;
        for (k = 0; k < GS_NS && !filled; k++)
            filled = SP_ON(gs.sp[i][k]);
        if (cur)
            fb_rect(x, VY + 3, 13, 12, run ? C_HOT : C_O5);
        else
            fb_rect(x, VY + 3, 13, 12, C_K1);
        fb_frame(x, VY + 3, 13, 12, nxt && ((now >> 7) & 1u) ? C_HOT : (cur ? C_O6 : C_S1));
        if (nxt && ((now >> 7) & 1u))
            fb_rect(x + 1, VY + 4, 11, 10, C_O3);
        fb_text_c(x + 7, VY + 5, t, cur ? C_K1 : (nxt ? C_HOT : (filled ? C_O6 : C_INK2)));
        if (filled && !cur)
            fb_pset(x + 11, VY + 5, C_O6);                                     /* a pattern with notes in it */
        if (gs.psnd[i])
            fb_hline(x + 2, VY + 13, 9, cur ? C_K1 : C_R3);                    /* a pattern with a sound of its own */
    }
    {                                                                       /* where it is: the step, the length, and what it is doing */
        char *q = ui_str(b, gs.srec ? "REC " : run ? "PLAY " : "STOP ");
        q = ui_num(q, (showcur ? ui.cur : pos) + 1);
        q = ui_str(q, "/");
        ui_num(q, len);
        fb_text_r(VX + VW - 6, VY + 5, b, gs.srec ? C_R3 : (run ? C_HOT : C_INK));
        if (gs.srec && ((now >> 8) & 1u))
            fb_rect(VX + VW - 6 - fb_text_w(b) - 7, VY + 7, 4, 4, C_R3);
    }
    {                                                                       /* the key the notes are in */
        int mn = 0, k = ui_seq_key(pc, &mn);
        char *q = ui_str(b, "KEY ");
        if (k < 0)
            q = ui_str(q, "--");
        else {
            q = ui_str(q, CD_NOTE[k]);
            q = ui_str(q, mn ? " MIN" : " MAJ");
        }
        fb_rect(VX + VW - 5 - fb_micro_w(b, 1) - 8, VY + 17, fb_micro_w(b, 1) + 8, 11, C_K1);
        fb_frame(VX + VW - 5 - fb_micro_w(b, 1) - 8, VY + 17, fb_micro_w(b, 1) + 8, 11, k < 0 ? C_S1 : C_O5);
        fb_micro_r(VX + VW - 9, VY + 20, b, k < 0 ? C_S4 : C_HOT, 1);
    }
    if (gs.scount) {                                                        /* the count-in */
        char d[2];
        int beat = gs.scount > 16u ? 1 : 1 + (16 - (int)gs.scount) / 4;
        d[0] = (char)('0' + (beat > 4 ? 4 : beat));
        d[1] = 0;
        ui_glass(VX + VW / 2 - 15, VY + 22, 30, 34, C_R3);
        fb_text2_c(VX + VW / 2, VY + 25, d, C_HOT, 0);
    }
    /* the ruler: a cell a step, the beats brighter, the playhead and the cursor marked */
    for (i = 0; i < 16; i++) {
        int s = b0 + i, x = VX + 6 + i * 13, out = s >= len;
        uint8_t c = out ? C_K3 : ((i & 3) == 0 ? C_S3 : C_S1);
        if (run && !gs.scount && s == pos)
            c = C_HOT;
        else if (showcur && s == ui.cur)
            c = ((now >> 8) & 1u) ? C_HOT : C_O6;
        fb_rect(x, VY + 61, 11, 3, c);
        if (!out && gs.mot_id[pc] && gs.mot[pc][s]) {                        /* a recorded knob: its value at this step, a point over the column */
            int my = VY + 57 - ((int)gs.mot[pc][s] - 1) * 34 / 127;
            fb_rect(x + 4, my, 3, 2, C_HOT);
            fb_hline(x + 3, my + 2, 5, C_O5);
        }
        if (!out && SP_ON(gs.sp[pc][s])) {
            int r = SX_RAT(gs.sx[pc][s]), ch = SX_CH(gs.sx[pc][s]), k;
            for (k = 1; k < r; k++)                                          /* a repeated step: its cell cut into as many */
                fb_vline(x + k * 11 / r, VY + 61, 3, C_K1);
            if (ch)                                                          /* a step left to chance: a dotted cell */
                for (k = 0; k < 11; k += 2)
                    fb_pset(x + k, VY + 62, C_K1);
        }
        if ((i & 3) == 0) {
            char nb_[3];
            nb_[0] = (char)('1' + i / 4);
            nb_[1] = 0;
            fb_text(x + 3, VY + 63, nb_, out ? C_S0 : C_INK2);
        }
    }
    if (showcur && ui.cur >= b0 && ui.cur < b0 + 16) {                       /* the step you are on: its name over it */
        int s = ui.cur, i2 = s - b0, x = VX + 6 + i2 * 13 + 5, f = gs.sp[pc][s];
        char *q = b;
        if (SP_ON(f))
            q = ui_note_name(q, ui_step_note(s));
        else
            q = ui_str(q, "REST");
        if (SP_ACC(f))
            q = ui_str(q, "!");
        if (ui.page == PG_SEQ4 && SP_ON(f)) {                                 /* on the step page: what the step does */
            static const char *const CH[4] = {"", " 75%", " 50%", " 25%"};
            q = ui_str(b, "x");
            q = ui_num(q, SX_RAT(gs.sx[pc][s]));
            q = ui_str(q, CH[SX_CH(gs.sx[pc][s])]);
        }
        {
            int hw = fb_text_w(b) / 2 + 4;
            if (hw < 18) hw = 18;
            if (x < VX + 6 + hw) x = VX + 6 + hw;
            if (x > VX + VW - 74 - hw) x = VX + VW - 74 - hw;               /* (clear of the key's tag at the right) */
            fb_rect(x - hw, VY + 17, 2 * hw, 13, C_K1);
            fb_frame(x - hw, VY + 17, 2 * hw, 13, C_HOT);
        }
        fb_text_c(x, VY + 17, b, C_HOT);
        if (s >= len)
            fb_text_c(VX + VW / 2, VY + 33, "PAST THE LENGTH", C_S4);
    }
    /* the map of the pattern */
    for (i = 0; i < GS_NS; i++) {
        int s = i, x = VX + 5 + s + (s >> 4) * 3, f = gs.sp[pc][s], h = SP_ON(f) ? 2 + SP_NOTE(f) * 5 / 36 : 1;
        uint8_t c = s >= len ? C_K3 : SP_ACC(f) ? C_R3 : SP_ON(f) ? C_O5 : C_S1;
        if (run && !gs.scount && s == pos)
            c = C_HOT;
        fb_vline(x, VY + 83 - h, h, c);
        if (s >> 4 == vb)
            fb_pset(x, VY + 84, C_O6);
        if (showcur && s == ui.cur)
            fb_pset(x, VY + 77, C_HOT);
    }
    p = ui_str(b, "BAR ");
    p = ui_num(p, vb + 1);
    p = ui_str(p, "/");
    p = ui_num(p, nb);
    fb_text_r(VX + VW - 4, VY + 74, b, C_O6);
    if (gs.mot_id[pc]) {                                                     /* the knob this pattern has recorded */
        p = ui_str(b, "KNOB ");
        ui_str(p, ui_pname((uint32_t)gs.mot_id[pc] - 1u));
        fb_micro_c(VX + VW / 2 + 6, VY + 79, b, gs.mot_hold ? C_HOT : C_O6, 1);
    }
}

/* the chords page: the key and scale at the top left, the chord you are playing at the top right, a chip for each degree of the key
 * with the chord that belongs to it, and a piano under it all: the notes of the key marked (the key's own note red), the chord's
 * notes lit. The piano starts at the lowest key and follows the octave. */
static void vz_chord(uint32_t now)
{
    static const char *const RN[8] = {"I", "II", "III", "IV", "V", "VI", "VII", "VIII"};
    int i, nchip, cw, lo = 48 + 12 * gs.oct, on = GS_CHORDS();
    char b[24], *p;
    (void)now;
    cd_prepare();
    fb_rect(VX, VY, VW, VH, C_K1);
    for (i = 0; i < 4; i++)
        fb_rect(VX, VY + i * 3, VW, 3, (uint8_t)(C_B0 + i));
    /* the key, the scale, the layout */
    p = ui_str(b, CD_NOTE[gs.p[P_CKEY] % 12]);
    p = ui_str(p, " ");
    p = ui_str(p, CD_SCNAME[gs.p[P_CSCL] % CD_NSC]);
    fb_text(VX + 6, VY + 3, b, C_O6);
    p = ui_str(b, CD_LAYNAME[gs.p[P_CLAY] & 3]);
    p = ui_str(p, on ? "  ON" : "  OFF");
    fb_text(VX + 6, VY + 16, b, on ? C_INK : C_INK2);
    /* the chord */
    if (cd.sounding || cd.nshown)
        fb_text2_c(VX + 158, VY + 5, cd.name, cd.sounding ? C_HOT : C_O4, 0);
    else
        fb_text_c(VX + 158, VY + 9, on ? "PLAY A KEY" : GS_SLOCK() ? "SCALE LOCK" : "CHORDS OFF", GS_SLOCK() ? C_O6 : C_S4);
    /* a chip a degree */
    nchip = cd.m > 8 ? 0 : (int)cd.m;
    cw = nchip ? (VW - 8) / nchip : 0;
    if (nchip > 7)
        cw = 26;
    for (i = 0; i < nchip; i++) {
        uint8_t nt[8], iv[8];
        int n, rpc, x = VX + 4 + i * cw, lit = cd.sounding && cd.sdeg >= 0 && cd.sdeg % (int)cd.m == i;
        n = cd_build(i, (uint16_t)(1u << CB_AUTO), nt, &rpc, iv);
        cd_name(b, rpc, iv, n);
        fb_rect(x, VY + 29, cw - 2, 24, lit ? C_O3 : C_K2);
        fb_frame(x, VY + 29, cw - 2, 24, lit ? C_HOT : (i == 0 ? C_O5 : C_S1));
        fb_text_c(x + (cw - 2) / 2, VY + 30, nchip > 7 ? RN[i] : RN[i], lit ? C_BG : (i == 0 ? C_O6 : C_INK2));
        fb_text_c(x + (cw - 2) / 2, VY + 41, b, lit ? C_BG : C_INK);
    }
    if (!nchip)
        fb_text_c(VX + VW / 2, VY + 36, "EVERY NOTE", C_S4);
    /* the piano: three octaves from the lowest key */
    {
        int px = VX + 6, py = VY + 57, wn = 0, oc, pc;
        for (oc = 0; oc < 3; oc++)
            for (pc = 0; pc < 12; pc++) {
                int note = lo + oc * 12 + pc, lit = 0, rel = (note - lo - (int)gs.p[P_CKEY] % 12 + 120) % 12;
                if (cd_black(pc))
                    continue;
                for (i = 0; i < cd.nshown; i++)
                    if (cd.shown[i] == note)
                        lit = 1;
                fb_rect(px + wn * 10, py, 9, 27, lit ? (cd.sounding ? C_HOT : C_O4) : C_S3);
                if (cd_in_scale(rel))
                    fb_rect(px + wn * 10 + 3, py + 21, 3, 3, rel == 0 ? C_R3 : C_O5);
                wn++;
            }
        wn = 0;
        for (oc = 0; oc < 3; oc++)
            for (pc = 0; pc < 12; pc++) {
                int note = lo + oc * 12 + pc, lit = 0, rel = (note - lo - (int)gs.p[P_CKEY] % 12 + 120) % 12;
                if (!cd_black(pc)) {
                    wn++;
                    continue;
                }
                for (i = 0; i < cd.nshown; i++)
                    if (cd.shown[i] == note)
                        lit = 1;
                fb_rect(px + wn * 10 - 3, py, 6, 16, lit ? (cd.sounding ? C_R3 : C_O2) : C_K1);
                if (cd_in_scale(rel))
                    fb_rect(px + wn * 10 - 1, py + 12, 2, 2, C_O5);
            }
    }
}

/* the glitch: a cube of eight shards that comes apart as the kind says, beside what the kind does to a wave */
static void vz_glitch(uint32_t now)
{
    int cx = VX + 56, cy = VY + VH / 2 + 6, i, kind = ui_lv(P_GLT), len = ui_lv(P_GLL), ex = 0, shown = 8, spin;
    static const uint8_t LENS[5] = {16, 8, 32, 4, 11};
    (void)len;
    spin = gl.on ? (int)(now >> 2) : (int)(now >> 4);
    if (gl.on) {
        if (kind == 0)
            ex = (int)((now / 110u) & 3u) * 5;
        else if (kind == 1) {
            ex = 14 - (int)((now / 22u) % 15u);
            spin = -spin;
        } else if (kind == 2) {
            int t = (int)((now % 1500u) * 1024u / 1500u);
            spin = (t * (2048 - t)) >> 11;
            ex = 3 + t / 90;
        } else
            shown = 8 - (int)((now / 110u) % 9u);
    }
    g3_cam(cx, cy, 190, 190, 110);
    for (i = 0; i < 8; i++) {
        int sx = (i & 1) ? 1 : -1, sy = (i & 2) ? 1 : -1, sz = (i & 4) ? 1 : -1;
        if (i >= shown)
            continue;
        g3_draw(&G3_CUBE, sx * (9 + ex), sy * (9 + ex), sz * (9 + ex), 4, 4, 4, spin / 2, spin, 0, gl.on ? C_R0 : C_O0, gl.on ? 5 : 8, 1,
                gl.on ? C_HOT : C_O6);
    }
    (void)LENS;
    /* the kind, drawn as what it does to a wave */
    {
        int x, y0 = cy - 6;
        for (x = 0; x < 112; x++) {
            int v = SK_SIN[((uint32_t)x * 6u * 1024u / 112u * 3u) & 1023u] * 22 / 32768, y;
            if (kind == 0) {                                                     /* stutter: the same slice, again */
                int xs = x % 28;
                v = SK_SIN[((uint32_t)xs * 24u) & 1023u] * 22 / 32768;
            } else if (kind == 1) {                                              /* reverse: the slice backwards */
                int xs = 27 - (x % 28);
                v = SK_SIN[((uint32_t)xs * 24u) & 1023u] * 22 / 32768;
            } else if (kind == 2) {                                              /* tape stop: the wave stretches out */
                v = SK_SIN[((uint32_t)(x * x / 14) * 7u) & 1023u] * 22 / 32768;
            } else {                                                             /* drop-out: it falls to nothing */
                v = v * (112 - x) / 112;
                if (x > 70)
                    v = v / 4;
            }
            y = y0 - v;
            fb_pset(VX + 104 + x, y, C_R4);
            fb_pset(VX + 104 + x, y + 1, C_R2);
        }
    }
}

/* the save page: a memory card. Eight slots in two rows, each a shape that turns: a lit solid for a patch kept, a bare frame for
 * a free slot; the chosen one is bigger, quicker and bracketed, and the strip under them says what it holds */
static void vz_save(uint32_t now)
{
    int i, bx = VX + 3, by = VY + 2, k;
    fb_rect(bx, by, 216, 82, C_S2);
    for (k = 0; k < 82; k += 2)
        fb_hline(bx, by + k, 216, C_S3);
    fb_frame(bx, by, 216, 82, C_S5);
    for (k = 0; k < 8; k++)                                                    /* the cut corner */
        fb_hline(bx + 216 - 8 + k, by + k, 8 - k, C_K1);
    fb_line(bx + 208, by, bx + 215, by + 7, C_S6);
    for (k = 0; k < 8; k++)                                                    /* the contacts */
        fb_rect(bx + 40 + k * 17, by + 76, 11, 5, C_O6);
    fb_rect(bx + 5, by + 5, 206, 54, C_K1);
    fb_frame(bx + 5, by + 5, 206, 54, C_S0);
    for (i = 0; i < SV_N; i++) {
        int x = bx + 7 + (i % 6) * 33, y = by + 7 + (i / 6) * 17, full = (int)((sv_full >> i) & 1u), sel = i == ui.slot;
        char b[3], *q = b;
        fb_rect(x, y, 31, 16, sel ? C_O1 : C_K2);
        fb_frame(x, y, 31, 16, sel ? C_HOT : C_S0);
        fb_clip(x + 1, y + 1, x + 30, y + 15);
        g3_cam(x + 8, y + 9, 150, 150, 120);
        {
            int s = sel ? 3 : 2, spin = (int)(now >> (sel ? 3 : 5)) + i * 70, bob = sel ? g3_sin((int)(now >> 2)) * 3 / 32768 : 0;
            if (full)
                g3_draw(&G3_ICO, 0, bob, 0, s, s, s, spin / 3, spin, 0, G3_CHR, 8, sel ? 2 : 0, sel ? C_HOT : C_O6);
            else
                g3_draw(&G3_CUBE, 0, bob, 0, s, s, s, spin / 3, spin, 0, 255, 0, 0, sel ? C_S6 : C_S3);
        }
        fb_clip(VX, VY, VX + VW, VY + VH);
        ui_num(q, i + 1);
        fb_text(x + 15, y + 4, b, sel ? C_HOT : (full ? C_O6 : C_INK2));
        if (sel) {                                                             /* brackets */
            fb_hline(x - 2, y - 2, 5, C_HOT); fb_vline(x - 2, y - 2, 5, C_HOT);
            fb_hline(x + 28, y + 17, 5, C_HOT); fb_vline(x + 32, y + 13, 5, C_HOT);
        }
    }
    {
        const uint8_t *d;
        char b[40];
        int n = 0;
        const char *nm;
        ui_sv_slot(ui.slot);
        d = s4.d;
        nm = d[0] ? GS_PRE[d[1] % GS_NPRE].name : "FREE";
        n = ui_num(b, ui.slot + 1) - b;
        b[n++] = ' ';
        while (*nm && n < 38)
            b[n++] = *nm++;
        b[n] = 0;
        fb_rect(bx + 5, by + 61, 206, 13, C_K1);
        fb_text(bx + 9, by + 62, "MEMORY CARD", C_S4);
        fb_text_r(bx + 207, by + 62, b, d[0] ? C_O6 : C_INK2);
    }
}

/* the performance page: the macro as a column that fills, with what it is doing named; the DJ filter as the band it lets through; the
 * pump as a bar that ducks on the beat */
/* The clock page: whose time is being kept. The tempo, large (the outside clock's, measured, while it is followed); under it what
 * is happening in words; along the foot the 24 pulses of the beat, filling as they come, and the bar's four beats as diamonds. */
static const char *ui_sync_state(void)
{
    return gs_sync == GS_SYNC_INT ? "OWN TEMPO" : gs_ext.on ? "LOCKED" : gs_sync == GS_SYNC_EXT ? "WAITING" : "NO CLOCK";
}
static void vz_sync(uint32_t now)
{
    char b[8];
    int i, on = gs_ext.on, wait = GS_EXT_WAIT(), beat = (gs.sixteenth >> 2) & 3;
    fb_well(VX + 2, VY + 15, VW - 4, VH - 17);
    if (wait)
        ui_str(b, "---");
    else
        ui_num(b, gs.bpm);
    fb_text2_c(VX + 56, VY + 24, b, wait ? C_DIM : on ? C_HOT : C_O5, 0);
    fb_micro_c(VX + 56, VY + 50, on ? "BPM FROM CLOCK" : wait ? "NO CLOCK YET" : "BPM", on ? C_R4 : C_INK2, 1);
    if (on && ((gs.sixteenth & 3u) < 2u))
        fb_heart(VX + 14, VY + 30);
    ui_text(VX + 112, VY + 22, on ? "LOCKED TO" : wait ? "WAITING FOR" : gs_sync == GS_SYNC_INT ? "KEEPING ITS" : "NO CLOCK:", on ? C_HOT : C_INK2);
    ui_text(VX + 112, VY + 36, on || wait ? "MIDI CLOCK" : "OWN TEMPO", on ? C_HOT : wait ? (((now >> 9) & 1u) ? C_R4 : C_R2) : C_O5);
    for (i = 0; i < 24; i++) {                                               /* the beat's pulses */
        int x = VX + 10 + i * 7 + (i / 6) * 9, lit = on && (uint32_t)i <= gs_ext.beat_n % 24u;
        fb_rect(x, VY + 66, 5, 8, lit ? (i % 6 ? C_O5 : C_R3) : C_K3);
        if (lit)
            fb_hline(x, VY + 66, 5, i % 6 ? C_O7 : C_R4);
    }
    for (i = 0; i < 4; i++)
        fb_diamond(VX + 28 + i * 51, VY + 80, 2, !wait && (gs.srun || on) && i == beat ? C_R3 : C_K3);
    (void)now;
}
static void vz_perf(uint32_t now)
{
    int m = gs.mac, f = gs.djf, k, x0 = VX + 92, w = 122, y0 = VY + 14, h = 44;
    char b[8];
    ui_glass(VX + 4, VY + 18, 80, 68, m ? C_O5 : C_S1);
    fb_text_c(VX + 44, VY + 21, GS_MAC_NAME[gs.mkind % GS_NMAC], m ? C_HOT : C_O6);
    g3_cam(VX + 44, VY + 55, 200, 200, 100);
    {
        int sz = 5 + m / 10, spin = (int)(now >> 3) * (1 + m / 24);
        fb_clip(VX + 6, VY + 35, VX + 82, VY + 76);
        g3_draw(&G3_ICO, 0, 0, 0, sz, sz, sz, spin / 3, spin, 0, m ? C_O0 : 255, 8, m > 100 ? 4 : (m ? 1 : 0), m ? C_O6 : C_S3);
        fb_clip(VX, VY, VX + VW, VY + VH);
    }
    fb_rect(VX + 10, VY + 79, 68, 3, C_K2);
    fb_rect(VX + 10, VY + 79, m * 68 / 127, 3, m > 100 ? C_R3 : C_O5);
    /* the filter: flat where the sound passes, falling away where it is cut */
    ui_glass(x0, VY + 4, w + 4, 62, f != 64 ? C_O5 : C_S1);
    fb_micro(x0 + 5, VY + 9, "DJ FILTER", C_S4, 1);
    fb_micro_r(x0 + w - 1, VY + 9, f < 64 ? "LOW PASS" : f > 64 ? "HIGH PASS" : "OPEN", f != 64 ? C_HOT : C_S3, 1);
    {
        int cut = f < 64 ? 6 + f * (w - 12) / 64 : 6 + (f - 64) * (w - 12) / 64, py = -1;
        for (k = 2; k < w; k++) {
            int d = f < 64 ? k - cut : f > 64 ? cut - k : 0, y = d <= 0 ? 6 : 6 + d * 3;
            if (d > -7 && d <= 0 && f != 64)                                 /* the bump at the corner */
                y = 6 - (7 + d) / 2;
            if (y > h - 2)
                y = h - 2;
            if (py >= 0)
                fb_line(x0 + k - 1, y0 + py, x0 + k, y0 + y, f != 64 ? C_HOT : C_O5);
            if (y < h - 2 && (k & 1))
                fb_vline(x0 + k, y0 + y + 2, h - 2 - y, C_O1);
            py = y;
        }
        fb_hline(x0 + 2, y0 + h, w, C_S1);
    }
    /* the pump */
    {
        int dep = gs.p[P_GDEP];
        ui_glass(x0, VY + 70, w + 4, 16, C_S1);
        fb_micro(x0 + 5, VY + 76, "PUMP", C_S4, 1);
        for (k = 0; k < 16; k++) {
            int beat = (k & 3), lv = 6 - (3 - beat) * dep / 64;
            if (lv < 1)
                lv = 1;
            fb_rect(x0 + 36 + k * 5, VY + 82 - lv, 4, lv, beat == 0 ? C_O3 : C_O5);
        }
        ui_num(b, dep);
        fb_micro_r(x0 + w - 1, VY + 76, b, dep ? C_HOT : C_S3, 1);
    }
}

/* ---------------------------------------------------------------------- the frame --- */
static const char *const UI_SAVE_ACT[3] = {"LOAD", "SAVE", "CLEAR"};

static void ui_fit_c(int cx, int y, const char *s, uint8_t c, int maxw)
{
    int w = fb_text_w(s), n = 0, x;
    const char *q;
    char one[2];
    for (q = s; *q; q++)
        n++;
    if (w <= maxw || n < 2) {
        fb_text_c(cx, y, s, c);
        return;
    }
    x = cx - (w - n) / 2;
    one[1] = 0;
    for (q = s; *q; q++) {
        one[0] = *q;
        x += fb_text(x, y, one, c) - 1;
    }
}

/* which of the four knobs was turned last (-1: none yet): its row carries the sword */
static int ui_ksel(void)
{
    int i, b = -1;
    for (i = 0; i < 4; i++)
        if (ui.k_t[i] && (b < 0 || (int32_t)(ui.k_t[i] - ui.k_t[b]) > 0))
            b = i;
    return b;
}
/* A knob's row, K1 at the top to K4 at the foot: its number (or the sword, on the one turned last), its name, a meter of twelve
 * cells when it is an amount (mx > 0: v of mx), and what it is set to at the right. fl: 1 the value in gold (a choice), 2 the value
 * lit (something is on), 4 the value dim. */
static void ui_row(int i, const char *label, const char *val, int v, int mx, uint32_t now, int fl)
{
    int x = 6, y = 157 + i * 19, w = 228, h = 18, k, lit;
    int sel = i == ui_ksel(), hot = ui.k_t[i] && now - ui.k_t[i] < 700u;
    char d[2];
    fb_rect(x, y, w, h, sel ? C_SEL : C_ROW);
    fb_frame(x, y, w, h, sel ? C_R3 : C_S2);
    if (sel)
        fb_sword(x + 3, y + 5);
    else {
        d[0] = (char)('1' + i);
        d[1] = 0;
        fb_micro(x + 8, y + 7, d, C_DIM2, 0);
    }
    ui_text(x + 21, y + 2, label, sel ? C_HOT : C_INK2);
    if (mx > 0) {
        lit = (v * 12 + mx / 2) / mx;
        if (v > 0 && !lit)
            lit = 1;
        fb_well(104, y + 3, 88, 12);
        for (k = 0; k < 12; k++) {
            int cx = 107 + k * 7, pk = hot && k < lit && k >= lit - 2;
            fb_rect(cx, y + 5, 5, 8, k < lit ? (pk ? C_R3 : C_O5) : C_K3);
            if (k < lit)
                fb_hline(cx, y + 5, 5, pk ? C_R4 : C_O7);
        }
        ui_text_r(x + w - 4, y + 2, val, fl & 4 ? C_DIM : fl & 2 ? C_O6 : C_HOT);
    } else
        ui_text_r(x + w - 4, y + 2, val, fl & 4 ? C_DIM : hot ? C_HOT : fl & 2 ? C_R4 : C_O5);
}
/* the row of a parameter */
static void ui_plate(int i, const char *label, uint32_t idx, uint32_t now, int accent)
{
    char buf[16];
    (void)accent;
    if (idx == 255u)
        ui_row(i, label, "", 0, 0, now, 0);
    else if (ui_is_enum(idx) || (idx == P_GLIDE && !gs.p[idx]))
        ui_row(i, label, ui_val(idx, buf), 0, 0, now, 1);
    else
        ui_row(i, label, ui_val(idx, buf), gs.p[idx], ui_maxv(idx), now, 0);
}

/* the battery, top right: a glass orb filled to the charge, green when full, then lime, yellow, amber and orange as it drains, and
 * red and blinking when it is nearly gone. sk_batt is the smoothed ADC reading (0 = not read yet); the divider is assumed to be
 * half of the cell on a 3.3 V reference, so 3.3 V (empty) is about 511 and 4.2 V (full) about 651. */
static uint16_t sk_batt;
static void ui_battery(int cx, int cy, uint32_t now)
{
    int pct, h, dx, dy, top;
    uint8_t hue;
    if (!sk_batt)
        return;
    pct = ui_clamp(((int)sk_batt - 511) * 100 / (651 - 511), 0, 100);
    h = 5 + pct * 12 / 100;                                                 /* the liquid's height in the 17-pixel orb: never quite empty */
    hue = pct > 75 ? C_BT0 : pct > 55 ? C_BT1 : pct > 38 ? C_BT2 : pct > 22 ? C_BT3 : pct > 10 ? C_BT4 : C_BT5;
    if (pct <= 10 && ((now >> 8) & 1u))                                     /* nearly gone: the glow blinks */
        hue = C_K3;
    top = cy + 8 - h;
    for (dy = -8; dy <= 8; dy++)
        for (dx = -8; dx <= 8; dx++) {
            int d2 = dx * dx + dy * dy, x = cx + dx, y = cy + dy, shade = dx + dy > 4, chk = (x + y) & 1;
            uint8_t c;
            if (d2 > 70)
                continue;
            if (d2 > 52)
                c = (dx + dy < -2) ? C_S5 : (dx + dy > 4 ? C_S1 : C_S3);                /* the glass rim, lit from the top left */
            else if (y >= top)
                c = (y == top && d2 < 40) ? C_HOT : (shade && chk ? C_K2 : (dx + dy > 8 ? C_K2 : hue));   /* the liquid, its surface bright, the far side in shadow */
            else
                c = shade && chk ? C_K1 : C_K2;                                           /* empty glass */
            fb_pset(x, y, c);
        }
    fb_pset(cx - 4, cy - 4, C_INK);                                          /* the glint */
    fb_pset(cx - 3, cy - 5, C_INK);
    fb_pset(cx - 5, cy - 3, C_INK);
    fb_pset(cx - 4, cy - 3, C_S6);
}

/* the top bar, a window's strip: the world's mark and name at the left, the tempo in gold and the battery at the right */
static void ui_header(uint32_t now)
{
    char b[12];
    int w = ui_world();
    fb_win2(0, 0, 240, 22, 0);
    fb_diamond(11, 10, 4, C_WORLD);
    ui_text(21, 3, UI_WORLD[w], C_INK2);
    if (gs.mac || gs.djf != 64u)                                                     /* the performance page is doing something */
        fb_micro(152, 9, "FX", ((now >> 8) & 1u) ? C_HOT : C_R3, 0);
    if (GS_EXT_WAIT()) {                                                            /* only an outside clock is followed, and none comes */
        ui_text_r(199, 3, "---", C_DIM);
        fb_micro(202, 9, "EXT", ((now >> 9) & 1u) ? C_R4 : C_R2, 0);
    } else {
        ui_num(b, gs.bpm);
        ui_text_r(199, 3, b, gs_ext.on || (ui.hud_t && now - ui.hud_t < 600u) ? C_HOT : C_O5);
        fb_micro(202, 9, gs_ext.on ? "EXT" : "BPM", gs_ext.on ? C_R4 : C_O5, 0);     /* (EXT: the tempo is an outside clock's) */
        if (gs_ext.on && (gs.sixteenth & 3u) < 2u)                                  /* locked: a heart that beats with it */
            fb_heart(165, 7);
    }
    ui_battery(228, 10, now);
}

/* under the window: the patch (its number in its world on a gold tile, its name in the lettering), the octave; then four lamps */
static void ui_status(uint32_t now)
{
    char b[28], *p;
    int i, w = ui_world(), num = (int)GS_POS[gs.preset] + 1 - (int)GS_W0[w], pop = ui.pop_t && now - ui.pop_t < 400u;
    b[0] = (char)('0' + num / 10);
    b[1] = (char)('0' + num % 10);
    b[2] = 0;
    fb_gem(6, 121, 24, 15, pop ? C_HOT : C_O5, C_O7, pop ? C_O7 : C_O4);
    fb_text_c(18, 122, b, C_OUT);
    fb_bl(36, 122, GS_PRE[gs.preset].name, 1, C_INK, 0);
    p = ui_str(b, "OCT ");
    if (gs.oct > 0)
        *p++ = '+';
    ui_num(p, gs.oct);
    if (ui.silent)                                                         /* (the keys are silent here: say so where it is always seen) */
        fb_micro_r(233, 126, "KEYS SILENT", ((now >> 9) & 1u) ? C_R4 : C_R2, 1);
    else
        fb_micro_r(233, 126, b, gs.oct ? C_O6 : C_DIM, 1);
    {
        static const char *const LN[4] = {"ARP", "CHORD", "HOLD", "GLITCH"};
        uint8_t on[4];
        on[0] = gs.p[P_AON] || gs.srun;
        on[1] = gs.p[P_CON] != 0;
        on[2] = gs.hold;
        on[3] = gl.on;
        for (i = 0; i < 4; i++) {
            int x = 6 + i * 58;
            if (on[i])                                                     /* on: gold (the glitch: pink, it is alive) */
                fb_gem(x + 3, 140, 48, 11, i == 3 ? C_R3 : C_O5, i == 3 ? C_R4 : C_O7, i == 3 ? C_R2 : C_O4);
            else {
                fb_rect(x + 3, 140, 48, 11, C_ROW);
                fb_frame(x + 3, 140, 48, 11, C_S2);
            }
            fb_micro_c(x + 28, 143, i == 0 && gs.srun ? "SEQ" : LN[i], on[i] ? (i == 3 ? C_HOT : C_OUT) : C_DIM, 1);
        }
    }
}

static void ui_cpu(void)
{
    char b[28], *p = b;
    if (!sk_late)                                                           /* (only when a block of sound has come late) */
        return;
    p = ui_str(p, "CPU ");
    p = ui_num(p, (int)sk_cpu_pct);
    *p++ = '/';
    p = ui_num(p, (int)sk_cpu_max);
    if (sk_late) {
        p = ui_str(p, " LATE ");
        p = ui_num(p, (int)sk_late);
    }
    *p = 0;
    fb_micro_r(VX + VW - 4, VY + VH - 8, b, C_R4, 0);
}

#ifndef SK_VERSION
#define SK_VERSION "GARTSTRINGER"
#endif
/* The title, at power-on: the castle at dusk, the name in gold lettering over its hill, whose synth it is, the version. a: the
 * milliseconds since it began (the name, then the line under it, arrive in turn). */
static void ui_title(int a, uint32_t now)
{
    static const char VER[] = SK_VERSION;
    const char *v = VER, *q;
    if (ui.theme_on != 0u) {
        fb_theme_snap(0);
        ui.theme_on = 0;
    }
    fb_scene(0, 0, 0, 0, 240, 240);
    if (a > 300)
        fb_bl(120 - fb_bl_w("Gartstringer", 2) / 2, 146, "Gartstringer", 2, 0, 1);
    if (a > 900) {
        fb_text_c(121, 181, "SIR OBBY'S SYNTH", C_BG);
        fb_text_c(120, 180, "SIR OBBY'S SYNTH", C_INK);
    }
    if (a > 1300) {
        fb_sword(120 - 7 + (int)((now >> 8) & 1u) * 3 - 44, 207);
        fb_heart(120 + 36, 207);
    }
    for (q = v; *q; q++)                                                     /* the version: what follows the last space of the build's name */
        if (*q == ' ')
            v = q + 1;
    if (v != VER && *v >= '0' && *v <= '9') {
        fb_text_r(236, 225, v, C_BG);
        fb_text_r(235, 224, v, C_O5);
    }
    sk_present();
}
/* A world's card, for a moment when ALGORITHM (or a patch) enters another world: its scene, and under it a window with its name
 * in the gold lettering and which of the six it is. */
static void ui_splash(int w, int a, uint32_t now)
{
    static const char *const NM[GS_NWORLD] = {"Trance", "House", "Techno", "Garage", "Ambient", "Bass"};
    char b[16], *p;
    (void)a;
    w = (int)((uint32_t)w % GS_NWORLD);
    if (ui.theme_on != (uint32_t)w) {                                         /* its colours at once, not faded in */
        fb_theme_snap((uint32_t)w);
        ui.theme_on = (uint8_t)w;
    }
    fb_fill(C_OUT);
    fb_scene(w, 0, 0, 8, 240, 160);
    fb_hline(0, 7, 240, C_INK);
    fb_hline(0, 168, 240, C_INK);
    fb_win(4, 176, 232, 58);
    fb_bl(120 - fb_bl_w(NM[w], 2) / 2, 184, NM[w], 2, 0, 1);
    p = ui_str(b, "WORLD ");
    p = ui_num(p, w + 1);
    ui_str(p, " OF 6");
    fb_micro_c(120, 222, b, C_INK2, 1);
    fb_diamond(20, 224, 3, C_WORLD);
    fb_diamond(220, 224, 3, C_WORLD);
    fb_sword(26 + (int)((now >> 8) & 1u) * 3, 196);
    sk_present();
}

static void ui_draw(uint32_t now)
{
    static uint8_t fadet;
    const ui_page_t *pg = &UI_PG[ui.page];
    uint32_t i;
    if ((++fadet & 1u) == 0)
        fb_fade();
    fb_unclip();
    if (ui.intro) {
        if (!ui.intro_t)
            ui.intro_t = now;
        if (now - ui.intro_t > 2600u)
            ui.intro = 0, gs_intro_n = 0;
        else {
            ui_title((int)(now - ui.intro_t), now);
            return;
        }
    }
    if (ui.spl_t) {                                                          /* a world just entered: its title, briefly */
        if (now - ui.spl_t > 900u)
            ui.spl_t = 0;
        else {
            ui_splash(ui_world(), 2600, now);
            return;
        }
    }
    ui_theme((uint32_t)ui_world());
    fb_fill(C_BG);
    ui_header(now);
    ui_window();
    vz_back(now);
    fb_clip(VX, VY, VX + VW, VY + VH);
    switch (ui.page) {
    case PG_HOME:
        vz_home(now);
        if (gs.p[P_GDEP])
            vz_gate_strip(VY + VH - 9, 5);
        break;
    case PG_OSC:
    case PG_OSC2:
        vz_towers(now);
        vz_gate_strip(VY + VH - 14, 8);
        break;
    case PG_FLT:
        vz_filter(now);
        break;
    case PG_ENV:
        vz_env(P_AA, P_AD, P_AS, P_AR, now, 0);
        break;
    case PG_ENV2:
        vz_env(P_FA, P_FD, P_FS, P_FR, now, 1);
        break;
    case PG_MOD:
        vz_matrix(now);
        break;
    case PG_LFO:
        vz_lfo(now);
        break;
    case PG_FX:
    case PG_FX2:
        vz_fx(now);
        break;
    case PG_ARP:
        vz_arp(now);
        break;
    case PG_GATE:
        vz_gate(now);
        break;
    case PG_GLITCH:
        vz_glitch(now);
        break;
    case PG_SEQ:
    case PG_SEQ4:
    case PG_SEQ2:
    case PG_SEQ3:
        vz_seq(now);
        break;
    case PG_PERF:
        vz_perf(now);
        break;
    case PG_SYNC:
        vz_sync(now);
        break;
    case PG_CHORD:
        vz_chord(now);
        break;
    default:
        vz_save(now);
        break;
    }
    fb_unclip();
    /* the page's name in the window's corner, the level at its right */
    if (ui.page != PG_SAVE && ui.page != PG_CHORD && !UI_SEQP())
    {
        ui_tab(VX, VY, pg->name);
        if (ui.page == PG_ARP)                                              /* (how to get to the sync from here) */
            fb_micro_r(VX + VW - 12, VY + 4, "HOLD ARP: CLOCK", C_DIM, 1);
    }
    if (ui.page == PG_HOME || ui.page == PG_OSC) {
        int mw = fb_text_w(GS_MODE_NAME[gs.p[P_MODE] % GS_NMODE]);
        fb_rect(VX + VW - 12 - mw - 4, VY, mw + 8, 13, C_OUT);
        fb_text_r(VX + VW - 12, VY, GS_MODE_NAME[gs.p[P_MODE] % GS_NMODE], C_INK);
    } else if (ui.page == PG_FLT)
        ui_text_r(VX + VW - 4, VY + 1, GS_FT_NAME[gs.p[P_FTYPE] % GS_NFT], C_INK2);
    {
        int lv = gs.level * (VH - 8) / 255;
        int q;
        for (q = 0; q < lv; q += 4)                                           /* the level: stepped cells, pink at the top */
            fb_rect(VX + VW - 6, VY + VH - 6 - q, 3, 3, q > 68 ? C_R3 : C_O5);
    }
    ui_status(now);
    ui_cpu();
    fb_win(2, 153, 236, 83);
    for (i = 0; i < 4u; i++) {
        char b[16], *q = b;
        int k = (int)i;
        if (ui.page == PG_SAVE) {
            if (i == 0u) {
                ui_num(b, ui.slot + 1);
                ui_row(0, "SLOT", b, ui.slot, SV_N - 1, now, 0);
            } else if (i == 1u)
                ui_row(1, "DO", UI_SAVE_ACT[ui.act], 0, 0, now, 1);
            else if (i == 2u)                                           /* the keys: sounding here, or silent (the USB host only) */
                ui_row(2, "KEYS", ui.silent ? "SILENT" : "SOUND", 0, 0, now, ui.silent ? 2 : 1);
            else if (sk_batt) {
                int pct = ui_clamp(((int)sk_batt - 511) * 100 / (651 - 511), 0, 100);
                q = ui_num(b, pct);
                ui_str(q, "%");
                ui_row(3, "BATTERY", b, pct, 100, now, 0);
            } else
                ui_row(3, "BATTERY", "NO READING", 0, 0, now, 4);
        } else if (ui.page == PG_SEQ) {
            uint8_t f = gs.sp[gs.pcur][ui.cur];
            static const char *const TN[5] = {"REST", "NOTE", "ACCENT", "SLIDE", "ACC+SLD"};
            int t = !SP_ON(f) ? 0 : SP_SLD(f) ? (SP_ACC(f) ? 4 : 3) : SP_ACC(f) ? 2 : 1;
            if (i == 0u) {
                ui_num(b, ui.cur + 1);
                ui_row(0, pg->k[0], b, ui.cur, GS_NS - 1, now, 0);
            } else if (i == 1u) {
                if (SP_ON(f))
                    ui_note_name(b, ui_step_note(ui.cur));
                else
                    ui_str(b, "-");
                ui_row(1, pg->k[1], b, SP_ON(f) ? SP_NOTE(f) : 0, 36, now, SP_ON(f) ? 0 : 4);
            } else if (i == 2u)
                ui_row(2, pg->k[2], TN[t], 0, 0, now, t ? 1 : 4);
            else {
                ui_num(b, gs.spl[gs.pcur]);
                ui_row(3, pg->k[3], b, gs.spl[gs.pcur] - 1, GS_NS - 1, now, 0);
            }
        } else if (ui.page == PG_SEQ4) {
            static const char *const CH[4] = {"ALWAYS", "75%", "50%", "25%"};
            uint8_t x = gs.sx[gs.pcur][ui.cur];
            if (i == 0u) {
                ui_num(b, ui.cur + 1);
                ui_row(0, pg->k[0], b, ui.cur, GS_NS - 1, now, 0);
            } else if (i == 1u) {
                q = ui_str(b, "x");
                ui_num(q, SX_RAT(x));
                ui_row(1, pg->k[1], b, 0, 0, now, SX_RAT(x) > 1 ? 2 : 1);
            } else if (i == 2u)
                ui_row(2, pg->k[2], CH[SX_CH(x)], SX_CH(x) ? 4 - SX_CH(x) : 0, SX_CH(x) ? 4 : 0, now, SX_CH(x) ? 2 : 1);
            else
                ui_row(3, pg->k[3], gs.psnd[gs.pcur] ? "OWN" : "SHARED", 0, 0, now, gs.psnd[gs.pcur] ? 2 : 1);
        } else if (ui.page == PG_MOD) {
            uint32_t mb = P_M1S + 3u * ui.mrow;
            int a = (int)gs.p[mb + 2u] - 64;
            if (i == 0u) {
                ui_num(b, ui.mrow + 1);
                ui_row(0, pg->k[0], b, 0, 0, now, 1);
            } else if (i == 1u)
                ui_row(1, pg->k[1], GS_MSRC_NAME[gs.p[mb] % GS_NMSRC], 0, 0, now, gs.p[mb] ? 1 : 4);
            else if (i == 2u)
                ui_row(2, pg->k[2], GS_MDST_NAME[gs.p[mb + 1u] % GS_NMDST], 0, 0, now, 1);
            else {
                if (a > 0)
                    *q++ = '+';
                ui_num(q, a);
                ui_row(3, pg->k[3], b, gs.p[mb + 2u], 127, now, a ? 2 : 0);
            }
        } else if (ui.page == PG_SYNC) {
            if (i == 0u)
                ui_row(0, pg->k[0], GS_SYNC_NAME[gs_sync % GS_NSYNC], 0, 0, now, 1);
            else if (i == 1u)
                ui_row(1, pg->k[1], ui_sync_state(), 0, 0, now, gs_ext.on ? 2 : 4);
            else if (i == 2u) {
                if (GS_EXT_WAIT())
                    ui_str(b, "---");
                else
                    ui_num(b, gs.bpm);
                ui_row(2, pg->k[2], b, 0, 0, now, gs_ext.on ? 2 : 4);
            } else
                ui_row(3, pg->k[3], gs.srun ? "PLAYING" : "STOPPED", 0, 0, now, gs.srun ? 2 : 4);
        } else if (ui.page == PG_PERF && i < 3u) {
            if (i == 0u) {
                ui_num(b, gs.mac);
                ui_row(0, pg->k[0], b, gs.mac, 127, now, gs.mac ? 2 : 0);
            } else if (i == 1u)
                ui_row(1, pg->k[1], GS_MAC_NAME[gs.mkind % GS_NMAC], 0, 0, now, 1);
            else {
                int v = (int)gs.djf - 64;
                if (v > 0)
                    *q++ = '+';
                ui_num(q, v);
                ui_row(2, pg->k[2], b, gs.djf, 127, now, v ? 2 : 0);
            }
        } else if (ui.page == PG_SEQ3) {
            static const char *const TH[3] = {"TURN >", "A < B", "ROLL"};
            if (i == 1u) {
                *q++ = (char)('A' + (gs.pcur + GS_NP - 1) % GS_NP);
                q = ui_str(q, " < ");
                *q++ = (char)('A' + gs.pcur);
                q = ui_str(q, " > ");
                *q++ = (char)('A' + (gs.pcur + 1) % GS_NP);
                *q = 0;
                ui_row(1, pg->k[1], b, 0, 0, now, 1);
            } else if (i == 3u) {
                if (ui.shift > 0)
                    *q++ = '+';
                ui_num(q, ui.shift);
                ui_row(3, pg->k[3], b, 0, 0, now, 1);
            } else
                ui_row(k, pg->k[i], TH[i == 0u ? 0 : 2], 0, 0, now, 1);
        } else
            ui_plate(k, pg->k[i], pg->p[i], now, 1);
    }
    if (ui.toast_t && now - ui.toast_t < 1200u) {
        int w = fb_text_w(ui.toast) + 20;
        int ty = UI_SEQP() ? 54 : ui.page == PG_CHORD ? 40 : 92;
        ui_glass(120 - w / 2, ty, w, 20, C_O5);
        fb_text_c(120, ty + 3, ui.toast, C_HOT);
    }
    sk_present();
    (void)now;
}
