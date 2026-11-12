/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GartStringer contributors */
/* The chord instrument. Choose a key and a scale and the keys play chords in it.
 *
 * Top layout: the white keys are roots, the notes of the scale (the lowest is the key's own note, and 16 of them span two octaves
 * or more); the black keys pick the kind of chord (eleven buttons). Side layout: the seven lowest keys pick the kind, all the
 * others are roots. A root with no button down plays the chord that belongs to the key: the triad you get by stacking thirds
 * on that note inside the scale. Buttons combine (Sus 4 with 7, Minor with Add 9). Lock keeps the buttons you tapped, so they
 * need not be held. Voicing: close or open, with a bass note under it or not; and the house voicings, which make every chord a
 * seventh or a ninth over a low root (HOUSE 7, HOUSE 9), a wide shell of root, seventh, tenth and ninth (SHELL), or the same minor
 * ninth on every root, moved in parallel the way a sampled chord is (PLANE M9).
 *
 * The chord's notes go to the voices the way keys do (gs_key), so the arpeggiator, hold and the sequencer's transposition all
 * work on chords. Included by gs_ui.c. */

#define CD_NSC 16

enum { CB_OFF, CB_AUTO, CB_MAJ, CB_MIN, CB_SUS2, CB_SUS4, CB_6, CB_7, CB_M7, CB_ADD9, CB_POW, CB_N };

/* the scales: the notes of each as a mask of the twelve (bit 0 the key's own note) */
static const uint16_t CD_SCALE[CD_NSC] = {
    0xAB5, 0x5AD, 0x9AD, 0xAAD, 0x6AD, 0x5AB, 0xAD5, 0x6B5, 0x56B, 0x5B3, 0x9CD, 0x4A9, 0x295, 0x4E9, 0x555, 0xFFF,
};
static const char *const CD_SCNAME[CD_NSC] = {"MAJOR", "MINOR", "HARM MIN", "MEL MIN", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYD",
                                              "LOCRIAN", "PHR DOM", "HUNG MIN", "MIN PENT", "MAJ PENT", "BLUES", "WHOLE", "CHROMA"};
static const char *const CD_NOTE[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *const CD_LAYNAME[4] = {"TOP", "TOP LOCK", "SIDE", "SIDE LCK"};
#define CD_NVOI 8
static const char *const CD_VOINAME[CD_NVOI] = {"CLOSE", "OPEN", "CLS+BASS", "OPN+BASS", "HOUSE 7", "HOUSE 9", "SHELL", "PLANE M9"};
static const char *const CD_BTN[CB_N] = {"OFF", "AUTO", "MAJ", "MIN", "SUS2", "SUS4", "6", "7", "M7", "ADD9", "5"};
static const uint8_t CD_TOPBTN[11] = {CB_OFF, CB_AUTO, CB_MAJ, CB_MIN, CB_SUS2, CB_SUS4, CB_6, CB_7, CB_M7, CB_ADD9, CB_POW};
static const uint8_t CD_SIDEBTN[7] = {CB_AUTO, CB_MAJ, CB_MIN, CB_SUS2, CB_SUS4, CB_7, CB_ADD9};

static struct {
    uint16_t held, locked;               /* buttons down, and latched (Lock) */
    uint8_t sc[12], m;                   /* the scale's notes from the key's own, and how many */
    uint8_t kn[27][6], kc[27];           /* the notes each key put on, and how many */
    uint8_t shown[8], nshown;            /* the chord last played (the screen's piano) */
    int8_t sdeg;                         /* .. its degree in the scale, -1 */
    char name[14];                       /* .. and its name */
    uint8_t sounding;                    /* a root is down */
} cd;

static int cd_black(int k) { return (0x54Au >> ((unsigned)k % 12u)) & 1; }

/* a key's job: 1 a button (*v its CB_ number), 2 a root (*v its degree in the scale), 0 none */
static int cd_role(int k, int lay, int *v)
{
    int i, nb = 0, nw = 0;
    if (lay >= 2) {
        if (k < 7) {
            *v = CD_SIDEBTN[k];
            return 1;
        }
        *v = k - 7;
        return 2;
    }
    for (i = 0; i < k; i++) {
        if (cd_black(i))
            nb++;
        else
            nw++;
    }
    if (cd_black(k)) {
        *v = CD_TOPBTN[nb];
        return 1;
    }
    *v = nw;
    return 2;
}

static void cd_prepare(void)                                         /* the scale of the patch's key and scale */
{
    uint16_t m = CD_SCALE[gs.p[P_CSCL] % CD_NSC];
    int i;
    cd.m = 0;
    for (i = 0; i < 12; i++)
        if ((m >> i) & 1u)
            cd.sc[cd.m++] = (uint8_t)i;
}

/* the note of a degree in the scale, a MIDI note (the key's own note at the lowest C, moved by the octave) */
static int cd_pitch(int deg)
{
    return 48 + 12 * gs.oct + (int)gs.p[P_CKEY] % 12 + 12 * (deg / (int)cd.m) + cd.sc[deg % (int)cd.m];
}

static int cd_in_scale(int pc)                                       /* a pitch class relative to the key's own note */
{
    return (CD_SCALE[gs.p[P_CSCL] % CD_NSC] >> (((pc % 12) + 12) % 12)) & 1;
}

/* the triad that belongs to a degree: the third and fifth above it by stacking thirds inside the scale; a scale of other than
 * seven notes has no such stack, so the third is the one the scale has (the minor one if it has no major) */
static void cd_auto(int deg, int *third, int *fifth, int *seventh)
{
    int r = cd.sc[deg % (int)cd.m], m = (int)cd.m;
    if (m == 7) {
        *third = ((cd.sc[(deg + 2) % 7] - r) % 12 + 12) % 12;
        *fifth = ((cd.sc[(deg + 4) % 7] - r) % 12 + 12) % 12;
        *seventh = ((cd.sc[(deg + 6) % 7] - r) % 12 + 12) % 12;
    } else {
        *third = cd_in_scale(r + 4) ? 4 : cd_in_scale(r + 3) ? 3 : 4;
        *fifth = cd_in_scale(r + 7) ? 7 : 7;
        *seventh = cd_in_scale(r + 10) ? 10 : 11;
    }
}

/* the chord a root asks for with the buttons down: the notes (low to high), their count; *root_pc the root's pitch class */
static int cd_build(int deg, uint16_t set, uint8_t *out, int *root_pc, uint8_t *ivout)
{
    int root = cd_pitch(deg), iv[12], n = 0, i, j, t, f, s7, auto_ = 0, third_;
    cd_auto(deg, &t, &f, &s7);
    *root_pc = root % 12;
    if (set & (1u << CB_OFF)) {                                      /* single notes */
        out[0] = (uint8_t)root;
        if (ivout)
            ivout[0] = 0;
        return 1;
    }
    third_ = 0;
    if (set & ((1u << CB_SUS2) | (1u << CB_SUS4))) {
        if (set & (1u << CB_SUS2))
            iv[n++] = 2;
        if (set & (1u << CB_SUS4))
            iv[n++] = 5;
        iv[n++] = 7;
    } else if (set & (1u << CB_POW)) {
        iv[n++] = 7;
        iv[n++] = 12;
    } else if (set & (1u << CB_MIN)) {
        iv[n++] = 3;
        iv[n++] = 7;
    } else if (set & (1u << CB_MAJ)) {
        iv[n++] = 4;
        iv[n++] = 7;
    } else {
        iv[n++] = t;
        iv[n++] = f;
        auto_ = 1;
    }
    if (set & (1u << CB_6))
        iv[n++] = 9;
    if (set & (1u << CB_7))
        iv[n++] = auto_ ? s7 : 10;
    if (set & (1u << CB_M7))
        iv[n++] = 11;
    if (set & (1u << CB_ADD9))
        iv[n++] = 14;
    for (i = 1; i < n; i++) {                                        /* low to high, no twins */
        int x = iv[i];
        for (j = i; j > 0 && iv[j - 1] > x; j--)
            iv[j] = iv[j - 1];
        iv[j] = x;
    }
    for (i = n - 1; i > 0; i--)
        if (iv[i] == iv[i - 1]) {
            for (j = i; j < n - 1; j++)
                iv[j] = iv[j + 1];
            n--;
        }
    if (n > 4)
        n = 4;
    out[0] = (uint8_t)root;
    if (ivout)
        ivout[0] = 0;
    for (i = 0; i < n; i++) {
        out[i + 1] = (uint8_t)(root + iv[i]);
        if (ivout)
            ivout[i + 1] = (uint8_t)iv[i];
    }
    (void)third_;
    return n + 1;
}

/* the house voicings: the chord a root asked for (its intervals in iv, n of them with the root) made into a seventh or a ninth.
 * nt and iv are written again, low to high; returns the count */
static int cd_house(int deg, uint8_t *nt, uint8_t *iv, int n, int voi)
{
    int root = nt[0], t, f, s7, third = -1, fifth = 7, sev = -1, nin = 14, i, k = 0;
    cd_auto(deg, &t, &f, &s7);
    if (cd.m == 7)
        nin = 12 + ((cd.sc[(deg + 1) % 7] - cd.sc[deg % 7]) % 12 + 12) % 12;
    if (nin < 13 || nin > 14)
        nin = 14;
    for (i = 1; i < n; i++) {                                         /* what the chord has: its third (or the sus note), its fifth, its seventh */
        int x = iv[i];
        if ((x == 3 || x == 4 || x == 2 || x == 5) && third < 0)
            third = x;
        else if (x == 6 || x == 7 || x == 8)
            fifth = x;
        else if (x == 9 || x == 10 || x == 11)
            sev = x;
    }
    if (third < 0)
        third = t;
    if (sev < 0)
        sev = s7;
    if (voi == 7) {                                                   /* the same shape everywhere */
        third = 3;
        fifth = 7;
        sev = 10;
        nin = 14;
    }
    if (voi == 6) {                                                   /* shell: root, seventh, tenth, ninth above it */
        iv[k++] = 0;
        iv[k++] = (uint8_t)sev;
        iv[k++] = (uint8_t)(third + 12);
        iv[k++] = (uint8_t)(nin + 12);
        for (i = 0; i < k; i++)
            nt[i] = (uint8_t)(root + iv[i]);
        iv[2] = (uint8_t)third;                                       /* (the name reads them inside an octave, the ninth as a ninth) */
        iv[3] = 14;
        return k;
    }
    iv[k++] = 0;
    iv[k++] = (uint8_t)third;
    iv[k++] = (uint8_t)fifth;
    iv[k++] = (uint8_t)sev;
    if (voi != 4)
        iv[k++] = (uint8_t)nin;
    nt[0] = (uint8_t)(root >= 24 ? root - 12 : root);                 /* the root goes down an octave; the rest stay where the hand is */
    for (i = 1; i < k; i++)
        nt[i] = (uint8_t)(root + iv[i]);
    return k;
}

/* the voicing: open lifts the third an octave, bass puts the root an octave under */
static int cd_voice(uint8_t *nt, int n)
{
    int voi = gs.p[P_CVOI] & 3, i, j;
    if ((gs.p[P_CVOI] & 7) >= 4)                                      /* (the house voicings are made in cd_house) */
        return n;
    if ((voi & 1) && n >= 3) {
        nt[1] = (uint8_t)(nt[1] + 12);
        for (i = 1; i < n; i++) {
            uint8_t x = nt[i];
            for (j = i; j > 0 && nt[j - 1] > x; j--)
                nt[j] = nt[j - 1];
            nt[j] = x;
        }
    }
    if ((voi & 2) && nt[0] >= 24 && n < 6) {
        for (i = n; i > 0; i--)
            nt[i] = nt[i - 1];
        nt[0] = (uint8_t)(nt[1] - 12);
        n++;
    }
    return n;
}

/* a chord's name from its intervals above the root: Cm7, Fsus4, G7, Ebmaj9, A5 */
static char *cd_name(char *p, int root_pc, const uint8_t *iv, int n)
{
    uint32_t m = 0;
    int i, third = 0, seventh = 0, nin = n > 1 ? n - 1 : 0;
    for (i = 1; i < n; i++)
        m |= 1u << (iv[i] % 12u);
    (void)nin;
    p = ui_str(p, CD_NOTE[root_pc % 12]);
    if (n <= 1)
        return p;
    if ((m & (1u << 3)) && (m & (1u << 6)) && !(m & (1u << 7))) {
        p = ui_str(p, "dim");
        third = 1;
    } else if (m & (1u << 3)) {
        p = ui_str(p, "m");
        third = 1;
    } else if ((m & (1u << 4)) && (m & (1u << 8)) && !(m & (1u << 7))) {
        p = ui_str(p, "aug");
        third = 1;
    } else if (m & (1u << 4))
        third = 1;
    else if (m & (1u << 2)) {
        if (!(m & (1u << 5)))
            p = ui_str(p, "sus2");
        else
            p = ui_str(p, "sus24");
    } else if (m & (1u << 5))
        p = ui_str(p, "sus4");
    else if (m == (1u << 7))
        p = ui_str(p, "5");
    for (i = 1; i < n; i++)
        if (iv[i] == 14)
            seventh |= 4;
    if (m & (1u << 11))
        seventh |= 1;
    else if (m & (1u << 10))
        seventh |= 2;
    if (seventh & 3) {
        if (seventh & 4)
            p = ui_str(p, (seventh & 1) ? "maj9" : "9");
        else
            p = ui_str(p, (seventh & 1) ? "maj7" : "7");
    } else if (m & (1u << 9))
        p = ui_str(p, "6");
    if ((seventh & 4) && !(seventh & 3))
        p = ui_str(p, "add9");
    (void)third;
    return p;
}

static uint16_t cd_set(int lay)                                      /* the buttons that count now */
{
    uint16_t s = (lay & 1) ? cd.locked : cd.held;
    return s ? s : (uint16_t)(1u << CB_AUTO);
}

/* a root goes down (or is built again): its notes onto the voices, and the screen's chord to match */
static void cd_press(int k, int deg, uint32_t on)
{
    uint8_t nt[8], iv[8];
    int n, rpc, i;
    cd_prepare();
    n = cd_build(deg, cd_set(gs.p[P_CLAY]), nt, &rpc, iv);
    if ((gs.p[P_CVOI] & 7) >= 4 && n >= 3)
        n = cd_house(deg, nt, iv, n, gs.p[P_CVOI] & 7);
    {
        char *p = cd.name;
        cd_name(p, rpc, iv, n);
    }
    n = cd_voice(nt, n);
    if (gs.srun) {                                                   /* the sequence runs: the root moves it */
        nt[0] = (uint8_t)cd_pitch(deg);
        n = 1;
    }
    cd.nshown = 0;
    for (i = 0; i < n && i < 7; i++)
        cd.shown[cd.nshown++] = nt[i];
    cd.sdeg = (int8_t)deg;
    cd.sounding = 1;
    for (i = 0; i < n; i++) {
        cd.kn[k][i] = nt[i];
        if (on)
            gs_key(nt[i], 1);
    }
    cd.kc[k] = (uint8_t)n;
}

static void cd_release(int k)
{
    int i;
    for (i = 0; i < cd.kc[k]; i++)
        gs_key(cd.kn[k][i], 0);
    cd.kc[k] = 0;
}

static uint32_t cd_down;                                             /* the roots held: bit k of the key */

static void cd_all_off(void)
{
    int k;
    for (k = 0; k < 27; k++)
        if (cd.kc[k])
            cd_release(k);
    cd_down = 0;
    cd.held = 0;
    cd.sounding = 0;
}

/* the buttons changed while roots are down: the same roots again with the new chord */
static void cd_rebuild(void)
{
    int k, v;
    for (k = 0; k < 27; k++)
        if (cd.kc[k] && cd_role(k, gs.p[P_CLAY], &v) == 2) {
            cd_release(k);
            cd_press(k, v, 1);
        }
}

/* a key goes down or up on the chord instrument */
static void cd_key(int k, int down)
{
    int v, role = cd_role(k, gs.p[P_CLAY], &v);
    if (role == 1) {
        uint16_t bit = (uint16_t)(1u << v);
        if (down) {
            if (gs.p[P_CLAY] & 1) {                                   /* Lock: a lone tap latches it (again: lets it go); with others, it joins */
                if (cd.held)
                    cd.locked |= bit;
                else
                    cd.locked = (cd.locked == bit) ? 0 : bit;
            }
            cd.held |= bit;
        } else
            cd.held &= (uint16_t)~bit;
        cd_rebuild();
    } else if (role == 2) {
        if (down) {
            cd_down |= 1u << k;
            cd_press(k, v, 1);
        } else {
            cd_down &= ~(1u << k);
            cd_release(k);
            if (!cd_down)
                cd.sounding = 0;
        }
    }
}
