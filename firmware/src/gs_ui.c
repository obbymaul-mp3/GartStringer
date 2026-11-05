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
enum { PG_HOME, PG_OSC, PG_FLT, PG_ENV, PG_ENV2, PG_LFO, PG_FX, PG_FX2, PG_ARP, PG_GATE, PG_GLITCH, PG_SEQ, PG_SEQ2, PG_SAVE, PG_N };

static int32_t sk_in_enc(uint32_t role);
static void sk_in_buttons(uint32_t *held, uint32_t *pressed);
static uint32_t sk_in_keys(uint32_t *held);
static void sk_led_btn(uint32_t label, uint32_t level);
static void sk_led_key(uint32_t n, uint32_t level);
static int sk_store_read(void *dst, uint32_t n);            /* the saved slots: 0 ok */
static int sk_store_write(const void *src, uint32_t n);

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
    {"SEQUENCER", {"NOTE", "TYPE", "SHIFT", "DICE"}, {255, 255, 255, 255}, 255, 1},
    {"SEQ SETUP", {"RATE", "LENGTH", "SWING", "GATE"}, {P_SRATE, P_SLEN, P_SSWG, P_SGATE}, P_SDIR, 1},
    {"SAVE", {"SLOT", "DO", "", ""}, {255, 255, 255, 255}, 255, 0},
};

static struct {
    uint8_t page, theme_on;
    uint32_t held;
    uint32_t k_t[4];                     /* when a knob was last touched */
    uint32_t pop_t, hud_t;               /* the patch name's pop, the HUD */
    uint8_t slot, act;
    uint8_t cur;                         /* the sequencer page's step */
    int8_t shift;                        /* what the pattern has been shifted by (shown) */
    uint8_t keys_down[27];
    uint32_t keys_held;
    uint32_t toast_t;
    char toast[20];
    uint8_t intro;
    uint32_t intro_t;
    uint8_t flash;                       /* a patch change: counts down */
} ui;

#define SV_N 8
#define SV_SLOT 96
static struct {
    uint32_t magic;
    uint8_t slot[SV_N][SV_SLOT];
    uint32_t sum;
} sv;
#define SV_MAGIC 0x31545347u                 /* "GST1" */

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
    case P_SRATE: return GS_GR_NAME[v & 3];
    case P_SDIR: return GS_SD_NAME[v & 3];
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
           idx == P_GRATE || idx == P_APAT || idx == P_ARATE || idx == P_GLL || idx == P_GLT || idx == P_AON || idx == P_SRATE || idx == P_SDIR;
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
    case P_SLEN: return 16;
    case P_SDIR: return 3;
    case P_GLL: return 4;
    case P_GLT: return 3;
    case P_OCT: return 4;
    default: return 127;
    }
}

static void ui_toast(const char *a, uint32_t now)
{
    char *p = ui.toast;
    while (*a && p < ui.toast + 18)
        *p++ = *a++;
    *p = 0;
    ui.toast_t = now;
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
    for (i = 0; i < 16u; i++) {
        d[4u + P_N + i] = gs.sq_n[i];
        d[4u + P_N + 16u + i] = gs.sq_f[i];
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
    if (gs.p[P_SLEN] < 1u)
        gs.p[P_SLEN] = 16;
    for (i = 0; i < 16u; i++) {
        gs.sq_n[i] = (uint8_t)ui_clamp(d[4u + P_N + i], 0, 36);
        gs.sq_f[i] = d[4u + P_N + 16u + i] & 7u;
    }
    gs.snote = -1;
    gs.sslide = 0;
}
static void ui_store_load(void)
{
    uint32_t i;
    if (sk_store_read(&sv, sizeof sv) != 0 || sv.magic != SV_MAGIC || sv.sum != sv_sum()) {
        for (i = 0; i < sizeof sv; i++)
            ((uint8_t *)&sv)[i] = 0;
    }
}
static void ui_do_save(uint32_t now)
{
    uint8_t *d = sv.slot[ui.slot];
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
    ui_toast(sk_store_write(&sv, sizeof sv) == 0 ? (ui.act == 1u ? "SAVED" : "CLEARED") : "FAILED", now);
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
        ui.intro = 0;
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
        ui.intro = 0;
    /* pages */
    if (pr & (1u << SB_HOME))
        ui.page = PG_HOME;
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
    if (pr & (1u << SB_SEQ))
        ui_page_press(PG_SEQ, PG_SEQ2);
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
        if (ui.page == PG_SEQ || ui.page == PG_SEQ2) {              /* on the sequencer's pages PLAY runs it */
            if (gs.srun) {
                gs_seq_stop();
                ui_toast("SEQ STOP", now);
            } else {
                if (gs.p[P_AON])
                    gs_set(P_AON, 0);
                gs_release_all();
                gs.sroot = (uint8_t)(60 + 12 * gs.oct);
                gs_seq_start();
                ui_toast("SEQ RUN", now);
            }
        } else {
            gs_set_hold(!gs.hold);
            ui_toast(gs.hold ? "HOLD ON" : "HOLD OFF", now);
        }
    }
    gl.manual = (uint8_t)((held >> SB_REC) & 1u);
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
                gs.sq_n[ui.cur] = (uint8_t)n;
                gs.sq_f[ui.cur] = (uint8_t)((gs.sq_f[ui.cur] & 6u) | 1u);
                ui.cur = (uint8_t)((ui.cur + 1u) & 15u);
            }
            gs_key((uint32_t)note, 1);
        } else if (!on && ui.keys_down[i]) {
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
    if (d && ui.page == PG_SEQ) {
        ui.cur = (uint8_t)ui_clamp((int)ui.cur + (d > 0 ? 1 : -1), 0, 15);
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
            int sg = d > 0 ? 1 : -1;
            ui.k_t[i] = now;
            if (i == 0u) {
                gs.sq_n[ui.cur] = (uint8_t)ui_clamp((int)gs.sq_n[ui.cur] + d, 0, 36);
                gs.sq_f[ui.cur] |= 1u;
            } else if (i == 1u) {
                uint8_t f = gs.sq_f[ui.cur];
                int t = !(f & 1u) ? 0 : (f & 4u) ? ((f & 2u) ? 4 : 3) : (f & 2u) ? 2 : 1;
                static const uint8_t TF[5] = {0, 1, 3, 5, 7};
                gs.sq_f[ui.cur] = TF[ui_clamp(t + sg, 0, 4)];
            } else if (i == 2u) {                                   /* shift the whole pattern, if none would leave the range */
                int k, ok = 1;
                for (k = 0; k < 16; k++)
                    if ((gs.sq_f[k] & 1u) && ((int)gs.sq_n[k] + sg < 0 || (int)gs.sq_n[k] + sg > 36))
                        ok = 0;
                if (ok) {
                    for (k = 0; k < 16; k++)
                        if (gs.sq_f[k] & 1u)
                            gs.sq_n[k] = (uint8_t)((int)gs.sq_n[k] + sg);
                    ui.shift = (int8_t)ui_clamp(ui.shift + sg, -99, 99);
                }
            } else {                                                /* dice: a new pattern in the trance minor */
                static const uint8_t SC[9] = {0, 0, 3, 5, 7, 7, 10, 12, 15};
                int k;
                for (k = 0; k < 16; k++) {
                    uint32_t r = sk_rand();
                    int on = (k & 3) == 0 ? (r & 15u) < 14u : (r & 15u) < 10u;
                    gs.sq_n[k] = (uint8_t)(12 + SC[(r >> 4) % 9u] - ((r >> 12) % 6u == 0u ? 12 : 0));
                    gs.sq_f[k] = on ? (uint8_t)(1u | (((r >> 16) % 5u == 0u) ? 2u : 0u) | (((r >> 20) % 8u == 0u) ? 4u : 0u)) : 0u;
                }
                ui.shift = 0;
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
            if (LB[i] != 255u && (ui.page == LB[i] || (i == SB_ENV && ui.page == PG_ENV2) || (i == SB_FX && ui.page == PG_FX2) || (i == SB_GLO && ui.page == PG_GATE) || (i == SB_SEQ && ui.page == PG_SEQ2)))
                lv = 3;
            else if (i == SB_ARP && gs.p[P_AON])
                lv = 2;
            else if (i == SB_SEQ && gs.srun)
                lv = 2;
            else if (i == SB_GLO && (gs.p[P_GLP] || (gs.p[P_GDEP] && gs.p[P_GPAT] != 13u)))
                lv = 1;
            else if (i == SB_PLAY && gs.hold)
                lv = 3;
            else if (i == SB_REC && gl.on)
                lv = 3;
            sk_led_btn(i, lv);
        }
        for (i = 0; i < 27u; i++) {
            int note = 48 + 12 * gs.oct + (int)i;
            uint32_t lv = 0;
            if (ui.keys_down[i])
                lv = 3;
            else if (gs.acur == note || gs_has((uint32_t)note))
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

/* the sequencer: sixteen columns on the floor, tall by their note; a rest is a flat slab, an accent burns red, a slide has a bridge to the
 * next; the step playing glows, steps past the length are dim silver, and on the edit page a marker shows the step the knobs write */
static void vz_seq(uint32_t now)
{
    int i, len = gs.p[P_SLEN] ? gs.p[P_SLEN] : 16, run = gs.srun;
    g3_cam(VX + VW / 2, VY + 62, 260, 260, 60);
    for (i = 0; i < 16; i++) {
        uint8_t f = gs.sq_f[i];
        int on = f & 1, h = on ? 8 + gs.sq_n[i] * 48 / 36 : 3, x = i * 13 - 97, playing = run && i == gs.sstep;
        int out = i >= len;
        if (!out && on && (f & 4) && i + 1 < len)                              /* the slide's bridge */
            g3_draw(&G3_CUBE, x + 7, h / 2, 0, 3, 1, 1, 0, 0, 0, C_O0, 8, 2, C_O4);
        if (out)
            g3_draw(&G3_CUBE, x, h / 2, 0, 3, h / 4 + 1, 4, 0, 0, 0, C_S0, 4, 0, C_S1);
        else if (on && (f & 2))
            g3_draw(&G3_CUBE, x, h / 2, 0, 3, h / 4 + 1, 4, 0, 0, 0, C_R0, 5, playing ? 4 : 1, C_HOT);
        else
            g3_draw(&G3_CUBE, x, h / 2, 0, 3, h / 4 + 1, 4, 0, 0, 0, C_O0, 8, playing ? 5 : (on ? 1 : -1), playing ? C_HOT : (on ? C_O6 : C_O2));
    }
    if (ui.page == PG_SEQ) {                                                    /* the step the knobs write */
        int x = VX + VW / 2 + ui.cur * 13 - 97, y = VY + VH - 8;
        fb_rect(x - 5, y, 11, 3, ((now >> 8) & 1u) ? C_HOT : C_O6);
        fb_pset(x, y - 1, C_HOT);
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
    fb_rect(bx + 5, by + 5, 206, 58, C_K1);
    fb_frame(bx + 5, by + 5, 206, 58, C_S0);
    for (i = 0; i < SV_N; i++) {
        int x = bx + 8 + (i & 3) * 51, y = by + 8 + (i >> 2) * 28, full = sv.slot[i][0] != 0, sel = i == ui.slot;
        char b[2];
        fb_rect(x, y, 48, 26, sel ? C_O1 : C_K2);
        fb_frame(x, y, 48, 26, sel ? C_HOT : C_S0);
        fb_clip(x + 1, y + 1, x + 47, y + 25);
        g3_cam(x + 14, y + 14, 150, 150, 120);
        {
            int s = sel ? 6 : 4, spin = (int)(now >> (sel ? 3 : 5)) + i * 70, bob = sel ? g3_sin((int)(now >> 2)) * 3 / 32768 : 0;
            if (full)
                g3_draw(&G3_ICO, 0, bob, 0, s, s, s, spin / 3, spin, 0, C_O0, 8, sel ? 2 : 0, sel ? C_HOT : C_O6);
            else
                g3_draw(&G3_CUBE, 0, bob, 0, s, s, s, spin / 3, spin, 0, 255, 0, 0, sel ? C_S6 : C_S3);
        }
        fb_clip(VX, VY, VX + VW, VY + VH);
        b[0] = (char)('1' + i);
        b[1] = 0;
        fb_text(x + 31, y + 8, b, sel ? C_HOT : (full ? C_O6 : C_INK2));
        if (sel) {                                                             /* brackets */
            fb_hline(x - 2, y - 2, 6, C_HOT); fb_vline(x - 2, y - 2, 6, C_HOT);
            fb_hline(x + 44, y + 27, 6, C_HOT); fb_vline(x + 49, y + 22, 6, C_HOT);
        }
    }
    {
        const uint8_t *d = sv.slot[ui.slot];
        char b[40];
        int n = 0;
        const char *nm = d[0] ? GS_PRE[d[1] % GS_NPRE].name : "FREE";
        b[n++] = (char)('1' + ui.slot);
        b[n++] = ' ';
        while (*nm && n < 38)
            b[n++] = *nm++;
        b[n] = 0;
        fb_rect(bx + 5, by + 65, 206, 12, C_K1);
        fb_text(bx + 9, by + 66, "MEMORY CARD", C_S4);
        fb_text_r(bx + 207, by + 66, b, d[0] ? C_O6 : C_INK2);
    }
}

/* ---------------------------------------------------------------------- the frame --- */
static const char *const UI_SAVE_ACT[3] = {"LOAD", "SAVE", "CLEAR"};

static int ui_ov_v, ui_ov_m;                                   /* the bar of the next plate, when it is not a parameter (ui_ov_m 0: none) */
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
    fb_text_c(x + w / 2, y + 2, label, C_BG);
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

static void ui_header(uint32_t now)
{
    char b[24], *p;
    ui_metal(0, 0, 240, 25);
    ui_screw(3, 3);
    ui_screw(234, 3);
    fb_text2_c(112, 2, "GARTSTRINGER", C_BG, 0);                                    /* engraved: a dark cut with the bright edge under it */
    fb_text2_c(112, 3, "GARTSTRINGER", C_S5, 0);
    fb_text2_c(112, 2, "GARTSTRINGER", C_O2, 0);
    fb_text2_c(111, 1, "GARTSTRINGER", C_O3, 0);
    (void)b;
    (void)p;
    (void)now;
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
        static const char *const LN[4] = {"ARP", "GATE", "HOLD", "GLITCH"};
        uint8_t on[4];
        on[0] = gs.p[P_AON] || gs.srun;
        on[1] = gs.p[P_GDEP] && gs.p[P_GPAT] != 13u;
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
            ui.intro = 0;
        else {
            int w = ui_clamp(a * 220 / 1400, 0, 220), y;
            fb_fill(C_BG);
            for (y = 0; y < 240; y += 4)
                fb_hline(0, y, 240, C_K1);
            for (i = 0; i < 7u; i++)
                fb_hline(10, 96 + (int)i * 4, w, (uint8_t)(C_O1 + i));
            g3_cam(120, 200, 230, 230, 130);
            g3_draw(&G3_ICO, 0, 0, 0, 7, 7, 7, a / 5, a / 3, 0, C_O0, 8, 1, C_O6);
            g3_draw(&G3_CUBE, 0, 0, 0, 11, 11, 11, a / 7, -a / 4, 0, 255, 0, 0, C_S3);
            fb_text2_c(120, 70, "GARTSTRINGER", a > 700 ? C_O6 : C_O3, 1);
            if (a > 1200) {
                fb_text_c(120, 130, "SUPERSAW TRANCE FOR THE FM-1", C_S4);
                fb_text_c(120, 148, "NO DRUMS", C_R3);
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
        vz_seq(now);
        break;
    default:
        vz_save(now);
        break;
    }
    fb_unclip();
    ui_screw(10, 110);
    ui_screw(227, 110);
    /* the page's name in the window's corner, the level at its right */
    if (ui.page != PG_FX && ui.page != PG_FX2 && ui.page != PG_SAVE)
        fb_text(VX + 4, VY + 3, pg->name, C_O6);
    if (ui.page == PG_HOME || ui.page == PG_OSC)
        fb_text_r(VX + VW - 12, VY + 3, GS_MODE_NAME[gs.p[P_MODE] % GS_NMODE], C_INK);
    else if (ui.page == PG_SEQ || ui.page == PG_SEQ2) {
        char b[16], *p = b;
        if (ui.page == PG_SEQ) {
            p = ui_str(p, "STEP ");
            p = ui_num(p, ui.cur + 1);
            p = ui_str(p, gs.srun ? "  RUN" : "  STOP");
        } else
            p = ui_str(p, gs.srun ? "RUN" : "STOP");
        fb_text_r(VX + VW - 12, VY + 3, b, gs.srun ? C_HOT : C_INK);
    }
    else if (ui.page == PG_FLT)
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
            uint8_t f = gs.sq_f[ui.cur];
            static const char *const TN[5] = {"REST", "NOTE", "ACCENT", "SLIDE", "ACC+SLD"};
            int t = !(f & 1u) ? 0 : (f & 4u) ? ((f & 2u) ? 4 : 3) : (f & 2u) ? 2 : 1, cx = 6 + (int)i * 58 + 27;
            if (i == 0u) {
                int n = (int)gs.sq_n[ui.cur] - 12;
                char *p = b;
                if (n > 0)
                    *p++ = '+';
                ui_num(p, n);
                ui_ov_v = gs.sq_n[ui.cur];
                ui_ov_m = 36;
                ui_plate(0, pg->k[0], 255, now, 1);
                fb_text2_c(cx, 156 + 27, b, f & 1u ? C_O6 : C_INK2, 0);
            } else if (i == 1u) {
                ui_ov_v = t;
                ui_ov_m = 4;
                ui_plate(1, pg->k[1], 255, now, 1);
                fb_text_c(cx, 156 + 31, TN[t], C_O6);
            } else if (i == 2u) {
                char *p = b;
                if (ui.shift > 0)
                    *p++ = '+';
                ui_num(p, ui.shift);
                ui_ov_v = 18 + ui.shift;
                ui_ov_m = 36;
                ui_plate(2, pg->k[2], 255, now, 1);
                fb_text2_c(cx, 156 + 27, b, C_O6, 0);
            } else {
                ui_ov_v = 0;
                ui_ov_m = 1;
                ui_plate(3, pg->k[3], 255, now, 1);
                fb_text_c(cx, 156 + 31, "ROLL", C_O6);
            }
            ui_ov_m = 0;
        } else
            ui_plate((int)i, pg->k[i], pg->p[i], now, 1);
    }
    if (ui.toast_t && now - ui.toast_t < 1200u) {
        int w = fb_text_w(ui.toast) + 20;
        fb_rbox(120 - w / 2, 92, w, 20, 5, C_K1);
        fb_frame(120 - w / 2, 92, w, 20, C_O5);
        fb_text_c(120, 97, ui.toast, C_HOT);
    }
    sk_present();
    (void)now;
}
