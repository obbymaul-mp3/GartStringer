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
enum { PG_HOME, PG_OSC, PG_FLT, PG_ENV, PG_ENV2, PG_LFO, PG_FX, PG_FX2, PG_ARP, PG_GATE, PG_GLITCH, PG_SEQ, PG_SEQ2, PG_SEQ3, PG_CHORD, PG_SAVE, PG_N };

static int32_t sk_in_enc(uint32_t role);
static void sk_in_buttons(uint32_t *held, uint32_t *pressed);
static uint32_t sk_in_keys(uint32_t *held);
static void sk_led_btn(uint32_t label, uint32_t level);
static void sk_led_key(uint32_t n, uint32_t level);
static int sk_store_read(uint32_t sec, void *dst, uint32_t n);            /* the saved slots, a 4 KB sector at a time: 0 ok */
static int sk_store_write(uint32_t sec, const void *src, uint32_t n);

/* the audio interrupt's load, % of the time it has (the hardware layer fills these; 0 on a computer) */
static volatile uint32_t sk_cpu_pct, sk_cpu_max, sk_late;

typedef struct {
    const char *name;
    const char *k[4];                    /* the plates' labels */
    uint8_t p[4];                        /* the parameters under them */
    uint8_t algo;                        /* what ALGORITHM turns */
    uint8_t theme;
} ui_page_t;

static const ui_page_t UI_PG[PG_N] = {
    {"PLAY", {"CUTOFF", "RESO", "DETUNE", "F.ENV"}, {P_CUT, P_RES, P_DET, P_FENV}, P_MODE, 0},
    {"OSCILLATOR", {"DETUNE", "MIX", "SUB", "COLOUR"}, {P_DET, P_MIX, P_SUB, P_COL}, P_MODE, 0},
    {"FILTER", {"CUTOFF", "RESO", "KEY", "DRIVE"}, {P_CUT, P_RES, P_KEY, P_DRV}, P_FTYPE, 0},
    {"AMP ENVELOPE", {"ATTACK", "DECAY", "SUS", "REL"}, {P_AA, P_AD, P_AS, P_AR}, P_VMODE, 0},
    {"FILTER ENV", {"ATTACK", "DECAY", "SUS", "REL"}, {P_FA, P_FD, P_FS, P_FR}, P_FTYPE, 0},
    {"LFO", {"RATE", "AMOUNT", "TARGET", "SHAPE"}, {P_LRATE, P_LAMT, P_LDST, P_LSHP}, P_LDST, 0},
    {"EFFECTS", {"CHOR", "ECHO", "FDBK", "REV"}, {P_CHOR, P_DLY, P_DFB, P_REV}, P_DTIM, 0},
    {"EFFECTS 2", {"ROOM", "CRUSH", "DRIVE", "GLIDE"}, {P_RSZ, P_CRU, P_DRV, P_GLIDE}, P_VMODE, 0},
    {"ARPEGGIO", {"PATT", "RATE", "OCTS", "GATE"}, {P_APAT, P_ARATE, P_ARNG, P_AGATE}, P_AON, 1},
    {"TRANCE GATE", {"PATT", "RATE", "DEPTH", "SMOOTH"}, {P_GPAT, P_GRATE, P_GDEP, P_GSMO}, P_GPAT, 1},
    {"GLITCH", {"ODDS", "LENGTH", "KIND", "CRUSH"}, {P_GLP, P_GLL, P_GLT, P_CRU}, P_GLT, 2},
    {"SEQUENCER", {"STEP", "NOTE", "TYPE", "LENGTH"}, {255, 255, 255, 255}, 255, 1},
    {"SEQ SETUP", {"RATE", "SWING", "GATE", "CLICK"}, {P_SRATE, P_SSWG, P_SGATE, P_SMET}, 255, 1},
    {"SEQ TOOLS", {"CLEAR", "COPY", "FILL", "SHIFT"}, {255, 255, 255, 255}, 255, 1},
    {"CHORDS", {"KEY", "SCALE", "LAYOUT", "VOICE"}, {P_CKEY, P_CSCL, P_CLAY, P_CVOI}, P_CON, 0},
    {"SAVE", {"SLOT", "DO", "", ""}, {255, 255, 255, 255}, 255, 0},
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
    uint32_t keys_held;
    uint32_t toast_t;
    char toast[20];
    uint8_t intro;
    uint32_t intro_t;
    uint8_t flash;                       /* a patch change: counts down */
    uint8_t rec_seen;                    /* gs.rec_n as last seen: a change flashes the step written */
    uint32_t rec_t;                      /* when */
} ui;

#define SV_PER 3                                          /* slots to a sector */
#define SV_SECT 6                                         /* sectors */
#define SV_N (SV_PER * SV_SECT)                           /* eighteen slots */
#define SV_SLOT (4 + P_N + GS_NP + GS_NP * GS_NS)         /* a slot: the patch's header, every parameter, the eight patterns' lengths and steps */
static struct {                                           /* the sector in hand (a save rewrites only its own sector) */
    uint32_t magic;
    uint8_t slot[SV_PER][SV_SLOT];
    uint32_t sum;
} sv;
static uint8_t sv_sec = 255;                              /* which sector sv holds */
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
    case P_CVOI: return CD_VOINAME[v & 3];
    case P_CON: return v ? "ON" : "OFF";
    case P_SRATE: return GS_GR_NAME[v & 3];
    case P_AON: return v ? "ON" : "OFF";
    case P_OCT: ui_num(buf, v - 2); return buf;
    case P_GLIDE: if (!v) return "OFF"; break;
    default: break;
    }
    ui_num(buf, v);
    return buf;
}
static int ui_is_enum(uint32_t idx)
{
    return idx == P_MODE || idx == P_FTYPE || idx == P_LDST || idx == P_LSHP || idx == P_VMODE || idx == P_DTIM || idx == P_GPAT ||
           idx == P_GRATE || idx == P_APAT || idx == P_ARATE || idx == P_GLL || idx == P_GLT || idx == P_AON || idx == P_SRATE || idx == P_SMET || idx == P_CKEY || idx == P_CSCL || idx == P_CLAY || idx == P_CVOI || idx == P_CON;
}
static int ui_maxv(uint32_t idx)
{
    switch (idx) {
    case P_MODE: return GS_NMODE - 1;
    case P_FTYPE: return GS_NFT - 1;
    case P_LDST: return 4;
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
    case P_CON: return 1;
    case P_CKEY: return 11;
    case P_CSCL: return CD_NSC - 1;
    case P_CLAY: return 3;
    case P_CVOI: return 3;
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
static void sv_pack(uint8_t *d)
{
    uint32_t i;
    d[0] = 1;
    d[1] = gs.preset;
    d[2] = gs.bpm;
    d[3] = (uint8_t)gs.oct;
    for (i = 0; i < P_N; i++)
        d[4u + i] = gs.p[i];
    for (i = 0; i < GS_NP; i++) {
        uint32_t k;
        d[4u + P_N + i] = gs.spl[i];
        for (k = 0; k < GS_NS; k++)
            d[4u + P_N + GS_NP + i * GS_NS + k] = gs.sp[i][k];
    }
}
static void sv_unpack(const uint8_t *d)
{
    uint32_t i;
    gs_all_off();
    gs.acur = -1;
    gs.preset = (uint8_t)(d[1] % GS_NPRE);
    gs.bpm = (uint8_t)ui_clamp(d[2], 40, 200);
    gs.oct = (int8_t)ui_clamp((int8_t)d[3], -2, 2);
    for (i = 0; i < P_N; i++) {
        int v = d[4u + i] & 127;
        gs.p[i] = (uint8_t)ui_clamp(v, 0, ui_maxv(i));
    }
    if (gs.p[P_ARNG] < 1u)
        gs.p[P_ARNG] = 1;
    for (i = 0; i < GS_NP; i++) {
        uint32_t k;
        gs.spl[i] = (uint8_t)ui_clamp(d[4u + P_N + i], 1, GS_NS);
        for (k = 0; k < GS_NS; k++) {
            uint8_t b = d[4u + P_N + GS_NP + i * GS_NS + k];
            gs.sp[i][k] = (uint8_t)((b & 0xC0u) | (uint8_t)ui_clamp(b & 63, 0, 37));
        }
    }
    gs.pcur = 0;
    gs.pnext = 255;
    gs.snote = -1;
    gs.sslide = 0;
}
static void ui_sv_sec(uint32_t sec)                    /* bring a sector in (an empty one if it was never written or is damaged) */
{
    uint32_t i;
    if (sv_sec == sec)
        return;
    if (sk_store_read(sec, &sv, sizeof sv) != 0 || sv.magic != SV_MAGIC || sv.sum != sv_sum()) {
        for (i = 0; i < sizeof sv; i++)
            ((uint8_t *)&sv)[i] = 0;
    }
    sv_sec = (uint8_t)sec;
}
static void ui_store_load(void)
{
    uint32_t sec, i;
    sv_full = 0;
    for (sec = 0; sec < SV_SECT; sec++) {
        sv_sec = 255;
        ui_sv_sec(sec);
        for (i = 0; i < SV_PER; i++)
            if (sv.slot[i][0])
                sv_full |= 1u << (sec * SV_PER + i);
    }
}
static void ui_do_save(uint32_t now)
{
    uint32_t sec = ui.slot / SV_PER, bit = 1u << ui.slot;
    uint8_t *d;
    ui_sv_sec(sec);
    d = sv.slot[ui.slot % SV_PER];
    if (ui.act == 0u) {
        if (!d[0])
            ui_toast("EMPTY", now);
        else {
            sv_unpack(d);
            ui_toast("LOADED", now);
            ui.pop_t = now;
        }
        return;
    }
    if (ui.act == 1u)
        sv_pack(d);
    else {
        uint32_t i;
        for (i = 0; i < SV_SLOT; i++)
            d[i] = 0;
    }
    sv.magic = SV_MAGIC;
    sv.sum = sv_sum();
    if (sk_store_write(sec, &sv, sizeof sv) == 0) {
        sv_full = ui.act == 1u ? sv_full | bit : sv_full & ~bit;
        ui_toast(ui.act == 1u ? "SAVED" : "CLEARED", now);
    } else
        ui_toast("FAILED", now);
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
    ui.pop_t = now;
    ui.flash = 10;
}

static int32_t ui_enc(uint32_t role)
{
    int32_t d = sk_in_enc(role);
    if (d && ui.intro)
        ui.intro = 0, gs_intro_n = 0;
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

static void ui_input(uint32_t now)
{
    uint32_t held, pr, keys, kp, i;
    int32_t d;
    sk_in_buttons(&held, &pr);
    keys = 0;
    kp = sk_in_keys(&keys);
    ui.held = held;
    if (ui.intro && (pr || kp))
        ui.intro = 0, gs_intro_n = 0;
    /* pages */
    if (pr & (1u << SB_HOME)) {                                     /* HOME, and again: the chords */
        ui.page = ui.page == PG_HOME ? PG_CHORD : PG_HOME;
        if (ui.page == PG_CHORD)
            ui_toast(gs.p[P_CON] ? "CHORDS ON" : "ALGO TURN: CHORDS", now);
    }
    if (pr & (1u << SB_EDIT))
        ui_page_press(PG_OSC, 255);
    if (pr & (1u << SB_SCL))
        ui_page_press(PG_FLT, 255);
    if (pr & (1u << SB_ENV))
        ui_page_press(PG_ENV, PG_ENV2);
    if (pr & (1u << SB_LFO))
        ui_page_press(PG_LFO, 255);
    if (pr & (1u << SB_FX))
        ui_page_press(PG_FX, PG_FX2);
    if (pr & (1u << SB_SEQ)) {                                      /* the sequencer: edit, setup, tools, out */
        if (ui.page == PG_SEQ)
            ui.page = PG_SEQ2;
        else if (ui.page == PG_SEQ2)
            ui.page = PG_SEQ3;
        else if (ui.page == PG_SEQ3)
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
        if (ui.page == PG_ARP) {                                    /* on the page: it switches the arp */
            gs_set(P_AON, !gs.p[P_AON]);
            ui_toast(gs.p[P_AON] ? "ARP ON" : "ARP OFF", now);
        } else {
            ui.page = PG_ARP;
            if (!gs.p[P_AON]) {
                gs_set(P_AON, 1);
                ui_toast("ARP ON", now);
            }
        }
    }
    if (pr & (1u << SB_SAVE)) {
        if (ui.page == PG_SAVE)
            ui_do_save(now);
        else
            ui.page = PG_SAVE;
    }
    if (pr & (1u << SB_PLAY)) {
        if (gs.srun || ui.page == PG_SEQ || ui.page == PG_SEQ2 || ui.page == PG_SEQ3) {   /* the sequence runs, or its pages are open: PLAY runs and stops it (from any page once it runs) */
            if (gs.srun) {
                gs_seq_stop();
                ui_toast("STOP", now);
            } else {
                if (gs.p[P_AON])
                    gs_set(P_AON, 0);
                gs_release_all();
                gs.sroot = (uint8_t)(60 + 12 * gs.oct);
                gs_seq_start();
                ui_toast(gs.srec ? "COUNT IN" : "PLAY", now);
            }
        } else {
            gs_set_hold(!gs.hold);
            ui_toast(gs.hold ? "HOLD ON" : "HOLD OFF", now);
        }
    }
    if ((pr & (1u << SB_REC)) && (ui.page == PG_SEQ || ui.page == PG_SEQ2 || ui.page == PG_SEQ3)) {   /* on the sequencer's pages REC arms recording */
        gs.srec = (uint8_t)!gs.srec;
        ui_toast(gs.srec ? (gs.srun ? "RECORDING" : "REC ARMED") : "REC OFF", now);
    }
    gl.manual = (uint8_t)(((held >> SB_REC) & 1u) && !(ui.page == PG_SEQ || ui.page == PG_SEQ2 || ui.page == PG_SEQ3));   /* (the sequencer's pages use REC to record) */
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
    if (!gs.p[P_CON] && (cd_down || cd.held || cd.sounding))          /* switched off: let go of what the chords hold */
        cd_all_off();
    if (gs.p[P_CON] != ui.con_prev) {
        ui.con_prev = gs.p[P_CON];
        ui_toast(gs.p[P_CON] ? "CHORDS ON" : "CHORDS OFF", now);
    }
    /* the keys: 27 notes from C3, moved by the octave */
    for (i = 0; i < 27u; i++) {
        uint32_t on = (keys >> i) & 1u;
        int note = 48 + 12 * gs.oct + (int)i;
        if (note < 0 || note > 120)
            continue;
        if (on && !ui.keys_down[i]) {
            ui.keys_down[i] = (uint8_t)note;
            if (ui.page == PG_SEQ && !gs.srun) {                    /* stopped, on the sequencer's page: a key writes the step (key 13 is the root) */
                int n = ui_clamp(note - (48 + 12 * gs.oct + 12) + 12, 0, 36);
                gs.sp[gs.pcur][ui.cur] = (uint8_t)((gs.sp[gs.pcur][ui.cur] & 0xC0u) | (uint8_t)(n + 1));
                ui_seq_grow(now);
                ui.cur = (uint8_t)ui_clamp((int)ui.cur + 1, 0, GS_NS - 1);
                ui.cur_t = now;
            }
            if (gs.p[P_CON] && !(ui.page == PG_SEQ && !gs.srun)) {
                ui.kchord[i] = 1;
                cd_key((int)i, 1);
            } else
                gs_key((uint32_t)note, 1);
        } else if (!on && ui.keys_down[i]) {
            if (ui.kchord[i]) {
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
        ui_load_patch((uint32_t)((int)gs.preset + (d > 0 ? 1 : GS_NPRE - 1)), now);
    }
    d = ui_enc(SE_SELECT);
    if (d) {
        gs.bpm = (uint8_t)ui_clamp((int)gs.bpm + d, 40, 200);
        ui.hud_t = now;
    }
    d = ui_enc(SE_ALGO);
    if (d && (ui.page == PG_SEQ || ui.page == PG_SEQ2 || ui.page == PG_SEQ3)) {   /* the pattern: A to D (running, the one picked starts at the end of this pass) */
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
    } else if (d && UI_PG[ui.page].algo != 255u) {
        uint32_t idx = UI_PG[ui.page].algo;
        gs_set(idx, (int)gs.p[idx] + (d > 0 ? 1 : -1));
        ui.k_t[0] = now;
    }
    for (i = 0; i < 4u; i++) {
        d = ui_enc(SE_K1 + i);
        if (!d)
            continue;
        if (ui.page == PG_SEQ) {
            int sg = d > 0 ? 1 : -1, quick = (d > 2 || d < -2);
            uint8_t *st = &gs.sp[gs.pcur][ui.cur], f = *st;
            ui.k_t[i] = now;
            ui.cur_t = now;
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
            if (i == 0u) {
                if (sg > 0) {
                    for (k = 0; k < GS_NS; k++)
                        gs.sp[gs.pcur][k] = 0;
                    tp = ui_str(tb, "PATTERN ");
                    *tp++ = (char)('A' + gs.pcur);
                    ui_str(tp, " CLEARED");
                    ui_toast(tb, now);
                }
            } else if (i == 1u) {
                int src = (gs.pcur + (sg > 0 ? 1 : GS_NP - 1)) % GS_NP;
                for (k = 0; k < GS_NS; k++)
                    gs.sp[gs.pcur][k] = gs.sp[src][k];
                gs.spl[gs.pcur] = gs.spl[src];
                tp = ui_str(tb, "COPIED FROM ");
                *tp++ = (char)('A' + src);
                *tp = 0;
                ui_toast(tb, now);
            } else if (i == 2u) {                                   /* fill: a new pattern in the trance minor */
                static const uint8_t SC[9] = {0, 0, 3, 5, 7, 7, 10, 12, 15};
                if (sg > 0) {
                    for (k = 0; k < gs.spl[gs.pcur]; k++) {
                        uint32_t r = sk_rand();
                        int on = (k & 3) == 0 ? (r & 15u) < 14u : (r & 15u) < 10u;
                        gs.sp[gs.pcur][k] = on ? SP_MAKE(12 + SC[(r >> 4) % 9u] - ((r >> 12) % 6u == 0u ? 12 : 0), (r >> 16) % 5u == 0u, (r >> 20) % 8u == 0u) : 0u;
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
            continue;
        }
        {
            uint32_t idx = UI_PG[ui.page].p[i];
            int step = ui_is_enum(idx) || idx == P_ARNG || idx == P_OCT ? (d > 0 ? 1 : -1) : d * (d > 2 || d < -2 ? 3 : 1);
            gs_set(idx, (int)gs.p[idx] + step);
            ui.k_t[i] = now;
        }
    }
    /* the lamps: the page's button, the arp, the hold, the glitch */
    {
        static const uint8_t LB[SB_N] = {PG_FX, PG_FLT, PG_ENV, PG_LFO, PG_OSC, PG_GLITCH, PG_HOME, PG_SAVE, PG_ARP, PG_SEQ, 255, 255, 255, 255};
        for (i = 0; i < SB_N; i++) {
            uint32_t lv = 0;
            if (LB[i] != 255u && (ui.page == LB[i] || (i == SB_ENV && ui.page == PG_ENV2) || (i == SB_FX && ui.page == PG_FX2) || (i == SB_GLO && ui.page == PG_GATE) || (i == SB_SEQ && (ui.page == PG_SEQ2 || ui.page == PG_SEQ3))))
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
            else if (gs.p[P_CON]) {                                  /* the chord keys: the buttons that count, the roots in the key's own note */
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
#define VY 30
#define VW 222
#define VH 86

/* brushed silver with a lit top and a dark bottom */
static void ui_metal(int x, int y, int w, int h)
{
    int r;
    fb_rect(x, y, w, h, C_S2);
    for (r = 1; r < h; r += 2)
        fb_hline(x, y + r, w, C_S3);
    for (r = 5; r < h - 4; r += 6)
        fb_hline(x + (r * 7) % 13, y + r, 14, C_S4);
    fb_hline(x, y, w, C_S5);
    fb_vline(x, y, h, C_S4);
    fb_hline(x, y + h - 1, w, C_S0);
    fb_vline(x + w - 1, y, h, C_S1);
}
static void ui_screw(int x, int y)
{
    fb_rect(x, y, 3, 3, C_S4);
    fb_pset(x + 1, y + 1, C_S0);
    fb_pset(x + 2, y + 2, C_S1);
}

#include "gs_3d.c"

static int ui_lv(uint32_t idx) { return gs.p[idx]; }

/* the backdrop of the window: a warm sky down to a horizon, a perspective floor of lines that run toward you at the tempo */
static void vz_back(uint32_t now)
{
    int i, hy = VY + 30, cx = VX + VW / 2, fh = VY + VH - hy;
    uint32_t scroll = (now * (uint32_t)(gs.bpm ? gs.bpm : 120) / 40u) & 2047u;
    fb_rect(VX, VY, VW, VH, C_K1);
    for (i = 0; i < 6; i++)                                                  /* the sky warms toward the horizon */
        fb_rect(VX, hy - 5 * (6 - i), VW, 5, (uint8_t)(C_B0 + i));
    fb_rect(VX, hy, VW, fh, C_BG);
    fb_hline(VX, hy, VW, C_O3);
    for (i = -8; i <= 8; i++)
        fb_line(cx, hy, cx + i * 34, VY + VH, i == 0 ? C_GRID1 : C_GRID0);
    for (i = 0; i < 8; i++) {
        uint32_t u = ((uint32_t)i * 256u + scroll) & 2047u;
        int y = hy + 1 + (int)((uint32_t)fh * ((u * u) >> 11) >> 11);
        fb_hline(VX, y, VW, y > hy + fh / 2 ? C_O1 : C_GRID0);
    }
}

static uint32_t ui_hash(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    return x ^ (x >> 16);
}

/* a thick phosphor line: the core, and a dimmer pixel under it */
static void vz_dot(int x, int y, uint8_t core, uint8_t glow)
{
    fb_pset(x, y, core);
    fb_pset(x, y + 1, glow);
    fb_pset(x, y - 1, glow);
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
    default: h = 30 + SK_SIN[(ph + (uint32_t)(SK_SIN[(ph * 2u) & 1023u] * fbk / 32768)) & 1023u] * 24 / 32768; break;
    }
    return h < 6 ? 6 : h;
}

static void vz_towers(uint32_t now)
{
    static const uint8_t NT[5] = {7, 6, 3, 3, 5};
    int mode = gs.p[P_MODE] % GS_NMODE, n = NT[mode], j, det = ui_lv(P_DET), mix = ui_lv(P_MIX), col = ui_lv(P_COL);
    int sp = 190 / n, wpx = n > 5 ? 13 : (n > 3 ? 18 : 28), bias = gs.level > 14 ? 2 : 0;
    g3_cam(VX + VW / 2, VY + 52, 250, 250, 40);
    for (j = 0; j < n; j++) {
        int h = vz_tower_h(mode, j, now, det, col, ui_lv(P_COL)), w = wpx;
        if (mode == 0 && j == 3)
            w = wpx * (70 + mix) / 120;
        g3_draw(&G3_CUBE, (j - (n - 1) / 2) * sp + (n & 1 ? 0 : sp / 2), h / 2, 0, w / 4 + 1, h / 4 + 1, w / 4 + 1,
                0, (int)(now >> 4) + j * 90, 0, C_O0, 8, bias, mode == 0 && j == 3 ? C_HOT : C_O6);
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
        fb_rect(x, y, 11, h, on ? C_O2 : C_K2);
        if (i == (gs.gstep & 15))
            fb_rect(x, y, 11, h, on ? C_HOT : C_S2);
        else if (on)
            fb_hline(x, y, 11, C_O5);
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
                fb_pset(VX + x, k, C_O6);
        }
        fb_vline(VX + x, y, VY + VH - y, (x & 1) ? C_O1 : C_O2);
        fb_pset(VX + x, y, C_HOT);
        fb_pset(VX + x, y + 1, C_O5);
        py = y;
    }
    fb_vline(VX + cut * VW / 127, VY, VH, C_R3);
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
            fb_vline(xs[i] + x, yy, bot - yy, ((xs[i] + x) & 1) ? C_O1 : C_O2);
            fb_pset(xs[i] + x, yy, C_HOT);
            fb_pset(xs[i] + x, yy + 1, C_O5);
        }
    }
    fb_hline(xs[3], bot, 1, C_O5);
    y = 0;
    for (i = 0; i < GS_NV; i++)                                               /* the level now, at the left */
        if (gs.v[i].on)
            y = (int)((fenv ? gs.v[i].fe : gs.v[i].ae) >> 9) > y ? (int)((fenv ? gs.v[i].fe : gs.v[i].ae) >> 9) : y;
    y = y > 127 ? 127 : y;
    fb_rect(VX + VW - 10, bot - (bot - top) * y / 127, 6, (bot - top) * y / 127, C_R3);
    fb_frame(VX + VW - 11, top - 1, 8, bot - top + 2, C_S2);
    (void)now;
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

/* FX: four small scenes, one above each plate; each is a shape that turns, and what it does shows its setting */
static void vz_fx(uint32_t now)
{
    int i, k;
    for (i = 0; i < 4; i++) {
        int x0 = 6 + i * 58, y0 = VY + 6, w = 54, h = VH - 12;
        fb_rect(x0, y0, w, h, C_K1);
        fb_rect(x0 + 1, y0 + h - 22, w - 2, 21, C_B1);
        fb_frame(x0, y0, w, h, C_S1);
        fb_clip(x0 + 1, y0 + 1, x0 + w - 1, y0 + h - 1);
        g3_cam(x0 + w / 2, y0 + h / 2 + 8, 170, 170, 150);
        if (ui.page == PG_FX) {
            switch (i) {
            case 0: {                                                          /* the ensemble: three that orbit, wider with the mix */
                int rad = 6 + ui_lv(P_CHOR) * 18 / 127, ox[3], oz[3], o[3] = {0, 1, 2}, t;
                for (k = 0; k < 3; k++) {
                    int a = (int)(now >> 3) + k * 341;
                    ox[k] = g3_cos(a) * rad / 32768;
                    oz[k] = g3_sin(a) * rad / 32768;
                }
                if (oz[o[0]] < oz[o[1]]) { t = o[0]; o[0] = o[1]; o[1] = t; }
                if (oz[o[1]] < oz[o[2]]) { t = o[1]; o[1] = o[2]; o[2] = t; }
                if (oz[o[0]] < oz[o[1]]) { t = o[0]; o[0] = o[1]; o[1] = t; }
                for (k = 0; k < 3; k++)
                    g3_draw(&G3_OCT, ox[o[k]], 0, oz[o[k]], 5, 6, 5, 0, (int)(now >> 3) + o[k] * 200, 0, C_O0, 8, 0, C_O6);
                break;
            }
            case 1: {                                                          /* the echo: copies falling away down the floor */
                static const uint8_t SP[6] = {8, 14, 20, 26, 11, 17};
                int n = 2 + ui_lv(P_DFB) * 4 / 127, s0 = 3 + ui_lv(P_DLY) * 4 / 127, act = (int)((now / (uint32_t)(SP[gs.p[P_DTIM] % 6u] * 22u)) % (uint32_t)n);
                for (k = n - 1; k >= 0; k--) {
                    int s = s0 * (n + 1 - k) / (n + 1);
                    g3_draw(&G3_CUBE, 0, s * 2, -30 + k * 60 / n, s, s, s, 0, (int)(now >> 4) + k * 60, 0, C_O0, 8, k == act ? 3 : -1, k == act ? C_HOT : C_O5);
                }
                break;
            }
            case 2: {                                                          /* the feedback: a stack that shrinks by how much comes back */
                int fbk = ui_lv(P_DFB), r = 115 + fbk * 115 / 127, s = 7 * 16, oy = -26;
                for (k = 0; k < 5; k++) {
                    int sc = s / 16 < 1 ? 1 : s / 16;
                    g3_draw(&G3_CUBE, 0, oy + sc * 2, 0, sc, sc, sc, 0, (int)(now >> 4) * (k + 1) + k * 90, 0, C_O0, 8, 0, C_O6);
                    oy += sc * 4;
                    s = s * r / 256;
                }
                break;
            }
            default: {                                                         /* the reverb: shells that grow and thin away */
                int n = 2 + ui_lv(P_REV) * 3 / 127, unit = 3 + ui_lv(P_RSZ) * 5 / 127;
                for (k = 0; k < n; k++) {
                    int g = (int)((((uint32_t)k * 256u + (now >> 2)) % ((uint32_t)n * 256u)) * 256u / ((uint32_t)n * 256u));   /* 0..255 */
                    int s = 1 + g * unit / 120;
                    g3_draw(&G3_CUBE, 0, 0, 0, s, s, s, (int)(now >> 5), -(int)(now >> 4), 0, 255, 0, 0, (uint8_t)(C_O7 - g * 5 / 256));
                }
                break;
            }
            }
        } else {
            switch (i) {
            case 0: {                                                          /* the room: a box as long as the size, a ball between its ends */
                int sz = 4 + ui_lv(P_RSZ) * 10 / 127, lim = sz * 2 - 5, bz = g3_sin((int)(now >> 2)) * lim / 32768;
                g3_draw(&G3_CUBE, 0, 0, 0, 8, 5, sz, 0, 0, 0, 255, 0, 0, C_S5);
                g3_draw(&G3_OCT, 0, 0, bz, 2, 2, 2, (int)(now >> 3), (int)(now >> 3), 0, C_O0, 8, 2, C_HOT);
                break;
            }
            case 1: {                                                          /* the crusher: it turns in steps, in fewer shades */
                int cr = ui_lv(P_CRU), steps = 3 + (127 - cr) * 20 / 127, a = (int)(now >> 3) & 1023, rl = 2 + (127 - cr) * 6 / 127;
                a = a * steps / 1024 * 1024 / steps;
                g3_draw(&G3_CUBE, 0, 0, 0, 8, 8, 8, a / 2, a, 0, C_O0, rl, 0, C_O6);
                break;
            }
            case 2: {                                                          /* the drive: a spike that grows, hotter, and shakes */
                int drv = ui_lv(P_DRV), hot = drv > 80;
                int jx = (int)(ui_hash(now >> 5) % (uint32_t)(drv / 40 + 1));
                g3_draw(&G3_PYR, jx, 6 + drv * 14 / 127, 0, 8, 6 + drv * 14 / 127, 8, 0, (int)(now >> 3), 0,
                        hot ? C_R0 : C_O0, hot ? 5 : 8, 1, hot ? C_HOT : C_O6);
                break;
            }
            default: {                                                         /* the glide: a slide from one note to the next, ghosts behind it */
                uint32_t dur = 120u + (uint32_t)ui_lv(P_GLIDE) * 9u, ph = now % (dur + 600u);
                int f = ph < dur ? (int)(ph * 1000u / dur) : 1000;
                for (k = 3; k >= 0; k--) {
                    int g = f - k * 90;
                    g = g < 0 ? 0 : g;
                    g3_draw(&G3_OCT, -18 + 36 * g / 1000, 0, -22 + 44 * g / 1000, 5, 5, 5, 0, (int)(now >> 3), 0, C_O0, 8, k ? -2 : 2, k ? C_O3 : C_HOT);
                }
                break;
            }
            }
        }
        fb_clip(VX, VY, VX + VW, VY + VH);
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
            g3_draw(&G3_CUBE, i * 13 - 97, h / 2, 0, 3, h / 4 + 1, 4, 0, 0, 0, on ? C_O0 : C_S0, on ? 8 : 4, cur ? 5 : 0, on ? (cur ? C_HOT : C_O6) : C_S3);
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
        g3_draw(&G3_CUBE, i * 13 - 97, h / 2, 0, 3, h / 4 + 1, 4, 0, 0, 0, C_O0, 8, cur ? 5 : 0, cur ? C_HOT : (on ? C_O6 : C_O3));
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
    g3_cam(VX + VW / 2, VY + 58, 260, 260, 60);
    for (i = 0; i < 16; i++) {
        int s = b0 + i, out = s >= len, f = gs.sp[pc][s], on = SP_ON(f), h = on ? 6 + SP_NOTE(f) * 34 / 36 : 3, x = i * 13 - 97;
        int playing = run && !gs.scount && s == pos, fresh = gs.rec_n && s == gs.rec_step && now - ui.rec_t < 350u;
        if (!out && on && SP_SLD(f) && i < 15 && s + 1 < len)                  /* the slide's bridge */
            g3_draw(&G3_CUBE, x + 7, h / 2, 0, 3, 1, 1, 0, 0, 0, C_O0, 8, 2, C_O4);
        if (out)
            g3_draw(&G3_CUBE, x, h / 2, 0, 3, h / 4 + 1, 4, 0, 0, 0, C_S0, 4, 0, C_S1);
        else if (fresh)
            g3_draw(&G3_CUBE, x, h / 2, 0, 4, h / 4 + 2, 5, 0, 0, 0, C_R0, 5, 5, C_HOT);
        else if (on && SP_ACC(f))
            g3_draw(&G3_CUBE, x, h / 2, 0, 3, h / 4 + 1, 4, 0, 0, 0, C_R0, 5, playing ? 4 : 1, C_HOT);
        else
            g3_draw(&G3_CUBE, x, h / 2, 0, 3, h / 4 + 1, 4, 0, 0, 0, C_O0, 8, playing ? 5 : (on ? 1 : -1), playing ? C_HOT : (on ? C_O6 : C_O2));
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
        fb_rect(VX + VW - 6 - 84, VY + 17, 84, 13, C_K1);
        fb_frame(VX + VW - 6 - 84, VY + 17, 84, 13, k < 0 ? C_S1 : C_O5);
        fb_text_c(VX + VW - 6 - 42, VY + 19, b, k < 0 ? C_INK2 : C_O6);
        {                                                                   /* the octave the keys are in */
            char *o = ui_str(b, "OCT ");
            if (gs.oct > 0)
                *o++ = '+';
            ui_num(o, gs.oct);
            fb_rect(VX + VW - 6 - 84, VY + 32, 84, 13, C_K1);
            fb_frame(VX + VW - 6 - 84, VY + 32, 84, 13, C_S1);
            fb_text_c(VX + VW - 6 - 42, VY + 34, b, C_INK);
        }
    }
    if (gs.scount) {                                                        /* the count-in */
        char d[2];
        int beat = gs.scount > 16u ? 1 : 1 + (16 - (int)gs.scount) / 4;
        d[0] = (char)('0' + (beat > 4 ? 4 : beat));
        d[1] = 0;
        fb_text2_c(VX + VW / 2, VY + 26, d, C_HOT, 0);
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
        if (x < VX + 22) x = VX + 22;
        if (x > VX + VW - 22) x = VX + VW - 22;
        fb_rect(x - 14, VY + 17, 28, 12, C_K1);
        fb_frame(x - 14, VY + 17, 28, 12, C_HOT);
        fb_text_c(x, VY + 18, b, C_HOT);
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
}

/* the chords page: the key and scale at the top left, the chord you are playing at the top right, a chip for each degree of the key
 * with the chord that belongs to it, and a piano under it all: the notes of the key marked (the key's own note red), the chord's
 * notes lit. The piano starts at the lowest key and follows the octave. */
static void vz_chord(uint32_t now)
{
    static const char *const RN[8] = {"I", "II", "III", "IV", "V", "VI", "VII", "VIII"};
    int i, nchip, cw, lo = 48 + 12 * gs.oct, on = gs.p[P_CON];
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
        fb_text_c(VX + 158, VY + 9, on ? "PLAY A KEY" : "CHORDS OFF", C_S4);
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
        g3_draw(&G3_CUBE, sx * (9 + ex), sy * (9 + ex), sz * (9 + ex), 4, 4, 4, spin / 2, spin, 0, gl.on ? C_R0 : C_S0, gl.on ? 5 : 7, 1,
                gl.on ? C_HOT : C_S5);
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
                g3_draw(&G3_ICO, 0, bob, 0, s, s, s, spin / 3, spin, 0, C_O0, 8, sel ? 2 : 0, sel ? C_HOT : C_O6);
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
        ui_sv_sec(ui.slot / SV_PER);
        d = sv.slot[ui.slot % SV_PER];
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

/* ---------------------------------------------------------------------- the frame --- */
static const char *const UI_SAVE_ACT[3] = {"LOAD", "SAVE", "CLEAR"};

static int ui_ov_v, ui_ov_m;                                   /* the bar of the next plate, when it is not a parameter (ui_ov_m 0: none) */
/* an engraved label: if it is wider than its strip the letters close up by a pixel each */
static void ui_label(int cx, int y, const char *s, uint8_t c)
{
    int w = fb_text_w(s), n = 0, x;
    const char *q;
    char one[2];
    for (q = s; *q; q++)
        n++;
    if (w <= 41 || n < 2) {
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
static void ui_plate(int i, const char *label, uint32_t idx, uint32_t now, int accent)
{
    int x = 6 + i * 58, y = 156, w = 54, h = 76;
    int hot = ui.k_t[i] && now - ui.k_t[i] < 700u;
    char buf[16];
    const char *val;
    int v = idx == 255u ? 0 : gs.p[idx], mx = idx == 255u ? 1 : ui_maxv(idx), k, seg = 16, lit;
    if (ui_ov_m) {                                                                  /* a plate that is not a parameter: its bar is given */
        v = ui_ov_v;
        mx = ui_ov_m;
    }
    fb_rbox(x, y, w, h, 5, C_K1);
    fb_frame(x, y, w, h, hot ? C_R3 : C_S1);
    ui_metal(x + 3, y + 3, w - 6, 15);                                              /* the engraved label strip */
    ui_label(x + w / 2, y + 2, label, C_BG);
    val = idx == 255u ? "" : ui_val(idx, buf);
    if (idx != 255u && !ui_is_enum(idx) && idx != P_GLIDE)
        fb_text2_c(x + w / 2, y + 27, val, hot ? C_HOT : accent ? C_O6 : C_INK, 0);
    else if (idx != 255u)
        fb_text_c(x + w / 2, y + 31, val, hot ? C_HOT : C_O6);
    lit = mx ? (v * seg + mx / 2) / mx : 0;
    for (k = 0; k < seg; k++) {                                                      /* the bar: sixteen cells, cooler to hotter */
        uint8_t c = k < lit ? (uint8_t)(C_O2 + (k * 5) / seg + (hot ? 1 : 0)) : C_K3;
        if (k >= 12 && k < lit)
            c = C_R3;
        fb_rect(x + 4 + k * 3, y + 60, 2, 10, c);
        if (k < lit)
            fb_hline(x + 4 + k * 3, y + 60, 2, k >= 12 ? C_R4 : C_HOT);
    }
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

static void ui_header(uint32_t now)
{
    char b[24], *p;
    ui_metal(0, 0, 240, 25);
    ui_screw(3, 3);
    ui_screw(234, 3);
    fb_text2_c(112, 2, "GARTSTRINGER", C_BG, 0);                                    /* engraved: a dark cut with the bright edge under it */
    fb_text2_c(112, 3, "GARTSTRINGER", C_B3, 0);
    fb_text2_c(112, 2, "GARTSTRINGER", C_O5, 0);
    fb_text2_c(111, 1, "GARTSTRINGER", C_HOT, 0);
    (void)b;
    (void)p;
    ui_battery(216, 12, now);
}

static void ui_status(uint32_t now)
{
    char b[28], *p;
    int x = 8, i;
    /* the patch: its number and name; at the right the tempo and the octave */
    b[0] = (char)('0' + (gs.preset + 1u) / 10u);
    b[1] = (char)('0' + (gs.preset + 1u) % 10u);
    b[2] = 0;
    fb_rect(6, 120, 28, 16, C_O3);
    fb_frame(6, 120, 28, 16, ui.pop_t && now - ui.pop_t < 400u ? C_HOT : C_O5);
    fb_text_c(20, 121, b, C_BG);
    fb_text(40, 121, GS_PRE[gs.preset].name, C_INK);
    p = ui_num(b, gs.bpm);
    p = ui_str(p, " BPM   OCT ");
    p = ui_num(p, gs.oct);
    fb_text_r(234, 121, b, C_O6);
    {
        static const char *const LN[4] = {"ARP", "CHORD", "HOLD", "GLITCH"};
        uint8_t on[4];
        on[0] = gs.p[P_AON] || gs.srun;
        on[1] = gs.p[P_CON];
        on[2] = gs.hold;
        on[3] = gl.on;
        for (i = 0; i < 4; i++) {
            int w = fb_text_w(LN[i]) + 12;
            fb_rect(x, 141, 8, 8, on[i] ? (i == 3 ? C_R3 : C_O5) : C_K3);
            fb_frame(x - 1, 140, 10, 10, C_S1);
            if (on[i])
                fb_pset(x + 2, 143, C_HOT);
            fb_text(x + 12, 139, i == 0 && gs.srun ? "SEQ" : LN[i], on[i] ? C_INK : C_INK2);
            x += w + 10;
        }
    }
}

static void ui_cpu(void)
{
    char b[28], *p = b;
    if (!sk_cpu_max && !sk_late)
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
    fb_text_r(234, 139, b, C_INK2);
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
        int a;
        if (!ui.intro_t)
            ui.intro_t = now;
        a = (int)(now - ui.intro_t);
        if (a > 2600)
            ui.intro = 0, gs_intro_n = 0;
        else {
            int y, x, lv = a < 1100 ? a * 7 / 1100 : (2500 - a) * 7 / 1400, f = a / 6;     /* the title swells and fades with the saw */
            lv = ui_clamp(lv, 0, 7);
            fb_fill(C_BG);
            for (y = 0; y < 240; y += 3)                               /* the sky: bands of violet */
                fb_hline(0, y, 240, (uint8_t)(C_B0 + ui_clamp(y * 6 / 160, 0, 5)));
            fb_disc(120, 112, 46, (uint8_t)(C_B4));                    /* a low sun behind the name */
            fb_disc(120, 112, 38, (uint8_t)(C_B5));
            for (y = 0; y < 240; y += 4)
                fb_hline(0, y, 240, C_BG);
            fb_rect(0, 150, 240, 90, C_BG);                            /* the floor: a grid running to the horizon */
            for (y = 0; y < 9; y++) {
                int yy = 150 + y * y * 90 / 81 + ((f * (y + 1)) & 3);
                fb_hline(0, ui_clamp(yy, 150, 239), 240, (uint8_t)(C_O2 + (y >> 1)));
            }
            for (x = -9; x <= 9; x++)
                fb_line(120 + x * 6, 150, 120 + x * 34, 239, (uint8_t)(C_O2 + (x & 1)));
            g3_cam(120, 112, 230, 230, 130);
            g3_draw(&G3_ICO, 0, 0, 0, 9, 9, 9, a / 5, a / 3, 0, C_O0, 8, 1, C_O6);
            g3_draw(&G3_CUBE, 0, 0, 0, 14, 14, 14, a / 7, -a / 4, 0, 255, 0, 0, C_S3);
            if (lv > 0) {
                static const char *const TL[2] = {"GART", "STRINGER"};
                int li;
                for (li = 0; li < 2; li++) {                               /* a drop shadow, a cyan edge, then the neon */
                    fb_text3_c(123, 36 + li * 44, TL[li], (uint8_t)(C_B3), 0);
                    fb_text3_c(121, 34 + li * 44, TL[li], (uint8_t)(C_GRID2), 0);
                    fb_text3_c(120, 33 + li * 44, TL[li], (uint8_t)(C_O1 + lv), 0);
                }
            }
            if (sk_batt) {                                             /* the raw battery reading, for checking the scale */
                char rb[8];
                ui_num(rb, sk_batt);
                fb_text_r(236, 228, rb, C_S1);
            }
            if (a > 1200 && lv > 0)
            {
                fb_rect(52, 184, 136, 18, C_BG);
                fb_text_c(120, 189, "Firmware by Obby Maul.", (uint8_t)(C_S2 + ui_clamp(lv - 1, 0, 4)));
            }
            ui_theme(0);
            sk_present();
            return;
        }
    }
    ui_theme(pg->theme);
    fb_fill(C_BG);
    ui_header(now);
    /* the window: a silver bezel round a dark glass */
    ui_metal(6, 27, 228, 92);
    fb_rect(VX - 1, VY - 1, VW + 2, VH + 2, C_S0);
    vz_back(now);
    fb_clip(VX, VY, VX + VW, VY + VH);
    switch (ui.page) {
    case PG_HOME:
    case PG_OSC:
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
    case PG_SEQ2:
    case PG_SEQ3:
        vz_seq(now);
        break;
    case PG_CHORD:
        vz_chord(now);
        break;
    default:
        vz_save(now);
        break;
    }
    fb_unclip();
    ui_screw(10, 110);
    ui_screw(227, 110);
    /* the page's name in the window's corner, the level at its right */
    if (ui.page != PG_FX && ui.page != PG_FX2 && ui.page != PG_SAVE && ui.page != PG_CHORD && ui.page != PG_SEQ && ui.page != PG_SEQ2 && ui.page != PG_SEQ3)
    {
        if (ui.page == PG_ENV || ui.page == PG_ENV2)                         /* the envelope's first slope runs under the name */
            fb_rect(VX + 2, VY + 1, fb_text_w(pg->name) + 5, 11, C_K1);
        fb_text(VX + 4, VY + 3, pg->name, C_O6);
    }
    if (ui.page == PG_HOME || ui.page == PG_OSC) {
        fb_rect(VX + VW - 12 - fb_text_w(GS_MODE_NAME[gs.p[P_MODE] % GS_NMODE]) - 3, VY + 1, fb_text_w(GS_MODE_NAME[gs.p[P_MODE] % GS_NMODE]) + 6, 11, C_K1);
        fb_text_r(VX + VW - 12, VY + 3, GS_MODE_NAME[gs.p[P_MODE] % GS_NMODE], C_INK);
    } else if (ui.page == PG_FLT)
        fb_text_r(VX + VW - 4, VY + 3, GS_FT_NAME[gs.p[P_FTYPE] % GS_NFT], C_INK);
    {
        int lv = gs.level * (VH - 8) / 255;
        fb_rect(VX + VW - 6, VY + VH - 4 - lv, 3, lv, lv > 72 ? C_R3 : C_O5);
    }
    ui_status(now);
    ui_cpu();
    for (i = 0; i < 4u; i++) {
        if (ui.page == PG_SAVE) {
            char b[12];
            if (i == 0u) {
                b[0] = (char)('1' + ui.slot);
                b[1] = 0;
                ui_plate(0, "SLOT", 255, now, 1);
                fb_text2_c(6 + 27, 156 + 27, b, C_O6, 0);
            } else if (i == 1u) {
                ui_plate(1, "DO", 255, now, 1);
                fb_text_c(6 + 58 + 27, 156 + 31, UI_SAVE_ACT[ui.act], C_O6);
            } else
                ui_plate((int)i, "", 255, now, 0);
        } else if (ui.page == PG_SEQ) {
            char b[12];
            uint8_t f = gs.sp[gs.pcur][ui.cur];
            static const char *const TN[5] = {"REST", "NOTE", "ACCENT", "SLIDE", "ACC+SLD"};
            int t = !SP_ON(f) ? 0 : SP_SLD(f) ? (SP_ACC(f) ? 4 : 3) : SP_ACC(f) ? 2 : 1, cx = 6 + (int)i * 58 + 27;
            if (i == 0u) {
                ui_num(b, ui.cur + 1);
                ui_ov_v = ui.cur;
                ui_ov_m = GS_NS - 1;
                ui_plate(0, pg->k[0], 255, now, 1);
                fb_text2_c(cx, 156 + 27, b, C_O6, 0);
            } else if (i == 1u) {
                if (SP_ON(f))
                    ui_note_name(b, ui_step_note(ui.cur));
                else
                    ui_str(b, "-");
                ui_ov_v = SP_ON(f) ? SP_NOTE(f) : 0;
                ui_ov_m = 36;
                ui_plate(1, pg->k[1], 255, now, 1);
                fb_text2_c(cx, 156 + 27, b, SP_ON(f) ? C_O6 : C_INK2, 0);
            } else if (i == 2u) {
                ui_ov_v = t;
                ui_ov_m = 4;
                ui_plate(2, pg->k[2], 255, now, 1);
                fb_text_c(cx, 156 + 31, TN[t], C_O6);
            } else {
                ui_num(b, gs.spl[gs.pcur]);
                ui_ov_v = gs.spl[gs.pcur] - 1;
                ui_ov_m = GS_NS - 1;
                ui_plate(3, pg->k[3], 255, now, 1);
                fb_text2_c(cx, 156 + 27, b, C_O6, 0);
            }
            ui_ov_m = 0;
        } else if (ui.page == PG_SEQ3) {
            static const char *const TH[3] = {"TURN >", "A < B", "ROLL"};
            char b[12], *p = b;
            ui_ov_v = 0;
            ui_ov_m = 1;
            ui_plate((int)i, pg->k[i], 255, now, 1);
            ui_ov_m = 0;
            if (i == 1u) {
                p = b;
                *p++ = (char)('A' + (gs.pcur + GS_NP - 1) % GS_NP);
                p = ui_str(p, "<");
                *p++ = (char)('A' + gs.pcur);
                p = ui_str(p, ">");
                *p++ = (char)('A' + (gs.pcur + 1) % GS_NP);
                *p = 0;
                fb_text_c(6 + 58 + 27, 156 + 31, b, C_O6);
            } else if (i == 3u) {
                if (ui.shift > 0)
                    *p++ = '+';
                ui_num(p, ui.shift);
                fb_text2_c(6 + 3 * 58 + 27, 156 + 27, b, C_O6, 0);
            } else
                fb_text_c(6 + (int)i * 58 + 27, 156 + 31, TH[i == 0u ? 0 : 2], C_O6);
        } else
            ui_plate((int)i, pg->k[i], pg->p[i], now, 1);
    }
    if (ui.toast_t && now - ui.toast_t < 1200u) {
        int w = fb_text_w(ui.toast) + 20;
        int ty = (ui.page == PG_SEQ || ui.page == PG_SEQ2 || ui.page == PG_SEQ3) ? 54 : ui.page == PG_CHORD ? 40 : 92;
        fb_rbox(120 - w / 2, ty, w, 20, 5, C_K1);
        fb_frame(120 - w / 2, ty, w, 20, C_O5);
        fb_text_c(120, ty + 5, ui.toast, C_HOT);
    }
    sk_present();
    (void)now;
}
