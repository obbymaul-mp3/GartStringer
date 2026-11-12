/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GartStringer contributors */
/* GartStringer's screen: an indexed framebuffer, 240 x 240 bytes, one palette index a pixel.
 *
 * Every frame the scene draws the whole picture; sk_present() hashes it in 16 x 16 tiles and sends only the tiles that
 * changed, as RGB565 through two buffers (one converts while the other goes out). The panel's link moves a full screen in
 * ~77 ms, so a still picture costs nothing and the scene's moving parts are what travel. A palette change (a
 * scene's time of day) resends everything. The platform provides sk_lcd(x, y, w, h, pixels).
 *
 * The palette is the look, and there are two worlds of it (trance: a cold blue void; house: dusk). A page picks a theme and
 * the palette blends into it over a second or two. */

#include "sk_fonts.h"

static void sk_lcd(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px);   /* the platform */
static void sk_pump(void);

#define FB_W 240
#define FB_H 240
#define FB_TS 16
#define FB_TX (FB_W / FB_TS)
#define FB_TY (FB_H / FB_TS)
#define FB_PAL 64

/* the palette's indexes: black, an accent ramp (it changes with the world and the page), glass greys,
 * red, a glow of dark reds for the backdrop, and the text */
enum {
    C_BG = 0,
    C_K1, C_K2, C_K3,                                          /* blacks: the panels */
    C_O0, C_O1, C_O2, C_O3, C_O4, C_O5, C_O6, C_O7,            /* the accent, dark to bright */
    C_S0, C_S1, C_S2, C_S3, C_S4, C_S5, C_S6,                  /* silver, dark to bright */
    C_R0, C_R1, C_R2, C_R3, C_R4,                              /* red */
    C_HOT,                                                     /* a white-hot yellow */
    C_B0, C_B1, C_B2, C_B3, C_B4, C_B5,                        /* the backdrop's glow, dark to warm */
    C_INK, C_INK2,                                             /* text: warm white, warm grey */
    C_GRID0, C_LED, C_GRID1, C_GRID2, C_DIM, C_DIM2, C_PLATE,  /* the scope's grids, a lamp on, dim greys, a plate */
    C_BT0, C_BT1, C_BT2, C_BT3, C_BT4, C_BT5                   /* the battery orb: green, lime, yellow, amber, orange, red (the same in every look) */
};
#define FB_NCOL 46u

#define FB_NTHEME 6
/* six looks of one palette: three for each world. TRANCE is a cold void: blue-blacks, electric blue (violet on the pages that
 * run in time, rose on the glitch). HOUSE is dusk: plum-blacks, amber (mint, orchid). The glass greys, the reds and the battery's
 * colours are the same in all of them. */
static const uint32_t SK_THEME[FB_NTHEME][46] = {
    {   /* trance: blue (the instrument) */
        0x02040C, 0x070A18, 0x0D1226, 0x161D38, 0x0A1848, 0x102A78,
        0x1840A8, 0x2060D8, 0x3888F8, 0x60B0FF, 0x98D4FF, 0xD0F0FF,
        0x1A2238, 0x2A3552, 0x3E4C70, 0x566890, 0x7C8EB4, 0xA8B8D8,
        0xDCE6F8, 0x380A14, 0x701024, 0xA81834, 0xE02848, 0xFF5870,
        0xF0FAFF, 0x050818, 0x08102C, 0x0C1840, 0x122458, 0x183274,
        0x204090, 0xE8F0FF, 0x8090B8, 0x0E1634, 0x40FFD0, 0x1C2C5C,
        0x38D8FF, 0x34406A, 0x566890, 0x0A0F22, 0x30E060, 0xA0E030,
        0xF0E030, 0xFFB020, 0xFF6A18, 0xFF2030,
    },
    {   /* trance: violet (the gate, the arp, the sequence) */
        0x02040C, 0x070A18, 0x0D1226, 0x161D38, 0x1C0A48, 0x341478,
        0x5420A8, 0x7C30D8, 0xA050F8, 0xC080FF, 0xDCB0FF, 0xF2DCFF,
        0x1A2238, 0x2A3552, 0x3E4C70, 0x566890, 0x7C8EB4, 0xA8B8D8,
        0xDCE6F8, 0x380A14, 0x701024, 0xA81834, 0xE02848, 0xFF5870,
        0xF0FAFF, 0x050818, 0x08102C, 0x0C1840, 0x122458, 0x183274,
        0x204090, 0xE8F0FF, 0x8090B8, 0x0E1634, 0x40FFD0, 0x1C2C5C,
        0x38D8FF, 0x34406A, 0x566890, 0x0A0F22, 0x30E060, 0xA0E030,
        0xF0E030, 0xFFB020, 0xFF6A18, 0xFF2030,
    },
    {   /* trance: rose (the glitch) */
        0x02040C, 0x070A18, 0x0D1226, 0x161D38, 0x30081C, 0x580C30,
        0x881048, 0xB81860, 0xE82C78, 0xFF5C98, 0xFF98BC, 0xFFD0E0,
        0x1A2238, 0x2A3552, 0x3E4C70, 0x566890, 0x7C8EB4, 0xA8B8D8,
        0xDCE6F8, 0x380A14, 0x701024, 0xA81834, 0xE02848, 0xFF5870,
        0xF0FAFF, 0x050818, 0x08102C, 0x0C1840, 0x122458, 0x183274,
        0x204090, 0xE8F0FF, 0x8090B8, 0x0E1634, 0x40FFD0, 0x1C2C5C,
        0x38D8FF, 0x34406A, 0x566890, 0x0A0F22, 0x30E060, 0xA0E030,
        0xF0E030, 0xFFB020, 0xFF6A18, 0xFF2030,
    },
    {   /* house: amber (the instrument) */
        0x08040A, 0x120A12, 0x1C101C, 0x2C1A2A, 0x301004, 0x582008,
        0x88340C, 0xB84C10, 0xE86C1C, 0xFF9438, 0xFFBC70, 0xFFE2B8,
        0x1A2238, 0x2A3552, 0x3E4C70, 0x566890, 0x7C8EB4, 0xA8B8D8,
        0xDCE6F8, 0x380A14, 0x701024, 0xA81834, 0xE02848, 0xFF5870,
        0xFFF6E0, 0x140810, 0x200C18, 0x341224, 0x4C1A2E, 0x682438,
        0x883040, 0xFFF0E4, 0xB89C94, 0x28121E, 0x40FFD0, 0x4A2030,
        0xFFB860, 0x5A3C48, 0x86606A, 0x160C14, 0x30E060, 0xA0E030,
        0xF0E030, 0xFFB020, 0xFF6A18, 0xFF2030,
    },
    {   /* house: mint (the gate, the arp, the sequence) */
        0x08040A, 0x120A12, 0x1C101C, 0x2C1A2A, 0x05281C, 0x084830,
        0x0C6C48, 0x109464, 0x20BC84, 0x50DCA8, 0x90F0CC, 0xD0FFEC,
        0x1A2238, 0x2A3552, 0x3E4C70, 0x566890, 0x7C8EB4, 0xA8B8D8,
        0xDCE6F8, 0x380A14, 0x701024, 0xA81834, 0xE02848, 0xFF5870,
        0xFFF6E0, 0x140810, 0x200C18, 0x341224, 0x4C1A2E, 0x682438,
        0x883040, 0xFFF0E4, 0xB89C94, 0x28121E, 0x40FFD0, 0x4A2030,
        0xFFB860, 0x5A3C48, 0x86606A, 0x160C14, 0x30E060, 0xA0E030,
        0xF0E030, 0xFFB020, 0xFF6A18, 0xFF2030,
    },
    {   /* house: orchid (the glitch) */
        0x08040A, 0x120A12, 0x1C101C, 0x2C1A2A, 0x2C0830, 0x501058,
        0x7C1888, 0xA824B4, 0xD03CD8, 0xEC6CEC, 0xF8A4F8, 0xFFD8FF,
        0x1A2238, 0x2A3552, 0x3E4C70, 0x566890, 0x7C8EB4, 0xA8B8D8,
        0xDCE6F8, 0x380A14, 0x701024, 0xA81834, 0xE02848, 0xFF5870,
        0xFFF6E0, 0x140810, 0x200C18, 0x341224, 0x4C1A2E, 0x682438,
        0x883040, 0xFFF0E4, 0xB89C94, 0x28121E, 0x40FFD0, 0x4A2030,
        0xFFB860, 0x5A3C48, 0x86606A, 0x160C14, 0x30E060, 0xA0E030,
        0xF0E030, 0xFFB020, 0xFF6A18, 0xFF2030,
    },
};
static uint8_t fb[FB_W * FB_H] SK_POOL;
static uint16_t fb_lut[FB_PAL];                  /* index -> RGB565, byte-swapped as the panel takes it */
static uint32_t fb_hash[FB_TX * FB_TY];
static uint16_t fb_dma[2][FB_W * FB_TS] SK_POOL;
static uint8_t fb_full = 1;                       /* resend every tile at the next present */
static int16_t fb_cx0, fb_cy0, fb_cx1 = FB_W, fb_cy1 = FB_H;   /* the clip rectangle */

static uint8_t fb_cur[48][3], fb_tgt[48][3];     /* the palette now, and where it is going: R G B */
static uint8_t fb_fade_on, fb_started;
static uint8_t fb_tint[5] = {0, 0, 0, 0, 128};    /* a preset's tint over its world's palette: r g b, how much (0..255), brightness (128 = as is) */

static void fb_lut_set(uint32_t i)
{
    uint32_t v = ((uint32_t)(fb_cur[i][0] & 0xF8u) << 8) | ((uint32_t)(fb_cur[i][1] & 0xFCu) << 3) | (fb_cur[i][2] >> 3);
    uint16_t s = (uint16_t)(((v >> 8) & 0xFFu) | ((v & 0xFFu) << 8));
    if (fb_lut[i] != s) {
        fb_lut[i] = s;
        fb_full = 1;
    }
}

/* a theme becomes the target; the first one is taken at once, the next ones are blended in by fb_fade() */
static void fb_theme(uint32_t t)
{
    uint32_t i;
    const uint32_t *rgb = SK_THEME[t % FB_NTHEME];
    for (i = 0; i < FB_NCOL; i++) {
        uint32_t k;
        for (k = 0; k < 3u; k++) {
            int c = (int)((rgb[i] >> (16 - 8 * k)) & 255u);
            if (i >= 1u && i <= 24u) {                                    /* sky, water, land, the sun, the stars: not the lights and the interface */
                c = c * fb_tint[4] / 128;
                c = c > 255 ? 255 : c;
                c += ((int)fb_tint[k] - c) * (int)fb_tint[3] / 256;
            }
            fb_tgt[i][k] = (uint8_t)c;
        }
        if (!fb_started) {
            fb_cur[i][0] = fb_tgt[i][0];
            fb_cur[i][1] = fb_tgt[i][1];
            fb_cur[i][2] = fb_tgt[i][2];
            fb_lut_set(i);
        }
    }
    fb_started = 1;
    fb_fade_on = 1;
}

static void fb_theme_snap(uint32_t t)            /* at once (the crash and update screens, the splash) */
{
    fb_started = 0;
    fb_tint[0] = fb_tint[1] = fb_tint[2] = fb_tint[3] = 0;
    fb_tint[4] = 128;
    fb_theme(t);
}

/* every few frames: each colour moves a part of the way to its target (about 1/24, at least one step), so a change of
 * mood takes several seconds and never jumps */
static void fb_fade(void)
{
    uint32_t i, k, moving = 0;
    if (!fb_fade_on)
        return;
    for (i = 0; i < FB_NCOL; i++) {
        for (k = 0; k < 3u; k++) {
            int d = (int)fb_tgt[i][k] - (int)fb_cur[i][k];
            if (d) {
                int st = d / 24;
                if (!st)
                    st = d > 0 ? 1 : -1;
                fb_cur[i][k] = (uint8_t)(fb_cur[i][k] + st);
                moving = 1;
            }
        }
        fb_lut_set(i);
    }
    fb_fade_on = (uint8_t)moving;
}

static void fb_clip(int x0, int y0, int x1, int y1)
{
    fb_cx0 = (int16_t)(x0 < 0 ? 0 : x0);
    fb_cy0 = (int16_t)(y0 < 0 ? 0 : y0);
    fb_cx1 = (int16_t)(x1 > FB_W ? FB_W : x1);
    fb_cy1 = (int16_t)(y1 > FB_H ? FB_H : y1);
}
static void fb_unclip(void) { fb_clip(0, 0, FB_W, FB_H); }

static inline void fb_pset(int x, int y, uint8_t c)
{
    if (x >= fb_cx0 && x < fb_cx1 && y >= fb_cy0 && y < fb_cy1)
        fb[y * FB_W + x] = c;
}
static inline uint8_t fb_get(int x, int y)
{
    return (x >= 0 && x < FB_W && y >= 0 && y < FB_H) ? fb[y * FB_W + x] : 0;
}

static void fb_rect(int x, int y, int w, int h, uint8_t c)
{
    int x1 = x + w, y1 = y + h, i, j;
    if (x < fb_cx0)
        x = fb_cx0;
    if (y < fb_cy0)
        y = fb_cy0;
    if (x1 > fb_cx1)
        x1 = fb_cx1;
    if (y1 > fb_cy1)
        y1 = fb_cy1;
    for (j = y; j < y1; j++) {
        uint8_t *p = fb + j * FB_W;
        for (i = x; i < x1; i++)
            p[i] = c;
    }
}
static void fb_fill(uint8_t c) { fb_rect(0, 0, FB_W, FB_H, c); }
static void fb_hline(int x, int y, int w, uint8_t c) { fb_rect(x, y, w, 1, c); }
static void fb_vline(int x, int y, int h, uint8_t c) { fb_rect(x, y, 1, h, c); }
static void fb_frame(int x, int y, int w, int h, uint8_t c)
{
    fb_hline(x, y, w, c);
    fb_hline(x, y + h - 1, w, c);
    fb_vline(x, y, h, c);
    fb_vline(x + w - 1, y, h, c);
}
/* a pixel-art rounded box: the corners cut (r 1) or stepped (r 2) */
static void fb_rbox(int x, int y, int w, int h, int r, uint8_t c)
{
    if (r <= 0 || w < 4 || h < 4) {
        fb_rect(x, y, w, h, c);
        return;
    }
    fb_rect(x + r, y, w - 2 * r, h, c);
    fb_rect(x, y + r, r, h - 2 * r, c);
    fb_rect(x + w - r, y + r, r, h - 2 * r, c);
    if (r >= 2) {
        fb_pset(x + 1, y + 1, c);
        fb_pset(x + w - 2, y + 1, c);
        fb_pset(x + 1, y + h - 2, c);
        fb_pset(x + w - 2, y + h - 2, c);
    }
}
static void fb_line(int x0, int y0, int x1, int y1, uint8_t c)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;) {
        int e2 = 2 * e;
        fb_pset(x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        if (e2 >= dy) {
            e += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            e += dx;
            y0 += sy;
        }
    }
}
/* a checkered (50 %) rectangle: a dithered shade over what is there */
static void fb_checker(int x, int y, int w, int h, uint8_t c)
{
    int i, j;
    for (j = y; j < y + h; j++)
        for (i = x + ((j ^ x) & 1); i < x + w; i += 2)
            fb_pset(i, j, c);
}
/* a filled disc */
static void fb_disc(int cx, int cy, int r, uint8_t c)
{
    int i, j;
    for (j = -r; j <= r; j++)
        for (i = -r; i <= r; i++)
            if (i * i + j * j <= r * r)
                fb_pset(cx + i, cy + j, c);
}

/* ------------------------------------------------------------------ text --- */
static int fb_glyph(const sk_font_t *f, uint8_t ch, int x, int y, uint8_t c)
{
    const sk_glyph_t *g = &f->g[ch];
    const uint8_t *b;
    int r, i, adv = g->adv;
    if (!adv)
        return 0;
    b = f->bits + g->off;
    for (r = 0; r < f->h; r++) {
        uint32_t row = ((uint32_t)b[2 * r] << 8) | b[2 * r + 1];
        for (i = 0; row; i++, row = (row << 1) & 0xFFFFu)
            if (row & 0x8000u)
                fb_pset(x + i, y + r, c);
    }
    return adv;
}
static int fb_text(int x, int y, const char *s, uint8_t c)
{
    int x0 = x;
    while (*s)
        x += fb_glyph(&SK_FONT, (uint8_t)*s++, x, y, c);
    return x - x0;
}
/* the title's letters: 2x, with a wide tracking */
static int fb_glyph2(const sk_font_t *f, uint8_t ch, int x, int y, uint8_t c)
{
    const sk_glyph_t *g = &f->g[ch];
    const uint8_t *b;
    int r, i;
    if (!g->adv)
        return 0;
    b = f->bits + g->off;
    for (r = 0; r < f->h; r++) {
        uint32_t row = ((uint32_t)b[2 * r] << 8) | b[2 * r + 1];
        for (i = 0; row; i++, row = (row << 1) & 0xFFFFu)
            if (row & 0x8000u) {
                fb_pset(x + 2 * i, y + 2 * r, c);
                fb_pset(x + 2 * i + 1, y + 2 * r, c);
                fb_pset(x + 2 * i, y + 2 * r + 1, c);
                fb_pset(x + 2 * i + 1, y + 2 * r + 1, c);
            }
    }
    return 2 * g->adv;
}
/* the title's letters at three times the size */
static int fb_text3_w(const char *s, int track)
{
    int w = 0;
    for (; *s; s++)
        w += 3 * SK_FONT.g[(uint8_t)*s].adv + track;
    return w;
}
static void fb_text3_c(int cx, int y, const char *s, uint8_t c, int track)
{
    int x = cx - fb_text3_w(s, track) / 2;
    for (; *s; s++) {
        const sk_glyph_t *g = &SK_FONT.g[(uint8_t)*s];
        const uint8_t *b = SK_FONT.bits + g->off;
        int r, i, j, k;
        for (r = 0; g->adv && r < SK_FONT.h; r++) {
            uint32_t row = ((uint32_t)b[2 * r] << 8) | b[2 * r + 1];
            for (i = 0; row; i++, row = (row << 1) & 0xFFFFu)
                if (row & 0x8000u)
                    for (j = 0; j < 3; j++)
                        for (k = 0; k < 3; k++)
                            fb_pset(x + 3 * i + k, y + 3 * r + j, c);
        }
        x += 3 * g->adv + track;
    }
}
static int fb_text2_w(const char *s, int track)
{
    int w = 0;
    for (; *s; s++)
        w += 2 * SK_FONT.g[(uint8_t)*s].adv + track;
    return w - track;
}
static void fb_text2_c(int cx, int y, const char *s, uint8_t c, int track)
{
    int x = cx - fb_text2_w(s, track) / 2;
    while (*s)
        x += fb_glyph2(&SK_FONT, (uint8_t)*s++, x, y, c) + track;
}
static int fb_text_w(const char *s)
{
    int w = 0;
    for (; *s; s++)
        w += SK_FONT.g[(uint8_t)*s].adv;
    return w;
}
static int fb_text_c(int cx, int y, const char *s, uint8_t c) { return fb_text(cx - fb_text_w(s) / 2, y, s, c); }
static int fb_text_r(int xr, int y, const char *s, uint8_t c) { return fb_text(xr - fb_text_w(s), y, s, c); }
/* text with a one-pixel halo (readable over the picture) */
static int fb_text_halo(int x, int y, const char *s, uint8_t c, uint8_t halo)
{
    int dx, dy;
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
            if (dx || dy)
                fb_text(x + dx, y + dy, s, halo);
    return fb_text(x, y, s, c);
}

/* the small face: 3 x 5 capitals and digits, for the labels (lower case is drawn as capitals; what it has no glyph for is a gap) */
static int fb_micro_adv(uint8_t ch)
{
    if (ch >= 'a' && ch <= 'z' && !SK_MICRO.g[ch].adv)
        ch = (uint8_t)(ch - 32);
    return SK_MICRO.g[ch].adv ? SK_MICRO.g[ch].adv : 4;
}
static int fb_micro_w(const char *s, int track)
{
    int w = 0;
    for (; *s; s++)
        w += fb_micro_adv((uint8_t)*s) + track;
    return w ? w - track - 1 : 0;
}
static int fb_micro(int x, int y, const char *s, uint8_t c, int track)
{
    int x0 = x;
    for (; *s; s++) {
        uint8_t ch = (uint8_t)*s;
        const sk_glyph_t *g;
        int r, i;
        if (ch >= 'a' && ch <= 'z' && !SK_MICRO.g[ch].adv)
            ch = (uint8_t)(ch - 32);
        g = &SK_MICRO.g[ch];
        if (g->adv) {
            const uint8_t *b = SK_MICRO.bits + g->off;
            for (r = 0; r < 5; r++)
                for (i = 0; i < 6; i++)
                    if (b[r] & (0x80u >> i))
                        fb_pset(x + i, y + r, c);
        }
        x += fb_micro_adv((uint8_t)*s) + track;
    }
    return x - x0;
}
static int fb_micro_c(int cx, int y, const char *s, uint8_t c, int track) { return fb_micro(cx - fb_micro_w(s, track) / 2, y, s, c, track); }
static int fb_micro_r(int xr, int y, const char *s, uint8_t c, int track) { return fb_micro(xr - fb_micro_w(s, track), y, s, c, track); }
/* ------------------------------------------------------------- present --- */
static uint32_t fb_tile_hash(int tx, int ty)
{
    const uint8_t *p = fb + ty * FB_TS * FB_W + tx * FB_TS;
    uint32_t h = 2166136261u;
    int r, k;
    for (r = 0; r < FB_TS; r++, p += FB_W) {
        const uint8_t *q = p;
        for (k = 0; k < FB_TS; k += 4, q += 4) {
            uint32_t w = (uint32_t)q[0] | (uint32_t)q[1] << 8 | (uint32_t)q[2] << 16 | (uint32_t)q[3] << 24;
            h = (h ^ w) * 16777619u;
            h = h << 13 | h >> 19;                  /* (the rotation: a change in a word's high byte must reach the hash) */
        }
    }
    return h;
}

static void sk_present(void)
{
    int tx, ty, which = 0;
    for (ty = 0; ty < FB_TY; ty++) {
        uint16_t dirty = 0;
        for (tx = 0; tx < FB_TX; tx++) {
            uint32_t h = fb_tile_hash(tx, ty);
            if (fb_full || h != fb_hash[ty * FB_TX + tx]) {
                fb_hash[ty * FB_TX + tx] = h;
                dirty |= (uint16_t)(1u << tx);
            }
        }
        tx = 0;
        while (dirty && tx < FB_TX) {
            int t0, t1, x0, w, r, i;
            uint16_t *d;
            if (!((dirty >> tx) & 1u)) {
                tx++;
                continue;
            }
            t0 = tx;
            while (tx < FB_TX && ((dirty >> tx) & 1u))
                tx++;
            t1 = tx;
            x0 = t0 * FB_TS;
            w = (t1 - t0) * FB_TS;
            d = fb_dma[which];
            which ^= 1;
            for (r = 0; r < FB_TS; r++) {
                const uint8_t *s = fb + (ty * FB_TS + r) * FB_W + x0;
                for (i = 0; i < w; i++)
                    *d++ = fb_lut[s[i] & (FB_PAL - 1)];
            }
            sk_lcd((uint32_t)x0, (uint32_t)(ty * FB_TS), (uint32_t)w, FB_TS, fb_dma[which ^ 1]);
        }
        sk_pump();                                  /* the keys between tile rows */
    }
    fb_full = 0;
}
