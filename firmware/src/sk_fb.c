/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GartStringer contributors */
/* GartStringer's screen: an indexed framebuffer, 240 x 240 bytes, one palette index a pixel.
 *
 * Every frame the scene draws the whole picture; sk_present() hashes it in 16 x 16 tiles and sends only the tiles that
 * changed, as RGB565 through two buffers (one converts while the other goes out). The panel's link moves a full screen in
 * ~77 ms, so a still picture costs nothing and the scene's moving parts are what travel. A palette change (a
 * scene's time of day) resends everything. The platform provides sk_lcd(x, y, w, h, pixels).
 *
 * The palette is the look: deep blacks, orange, brushed silver and hints of red. A page picks a theme and the palette
 * blends into it over a second or two. */

#include "sk_fonts.h"

static void sk_lcd(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px);   /* the platform */
static void sk_pump(void);

#define FB_W 240
#define FB_H 240
#define FB_TS 16
#define FB_TX (FB_W / FB_TS)
#define FB_TY (FB_H / FB_TS)
#define FB_PAL 64

/* the palette's indexes: black, an accent ramp (orange; amber in the gate's pages, red in the glitch's), brushed silver,
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
    C_GRID0, C_LED, C_GRID1, C_GRID2, C_DIM, C_DIM2, C_PLATE   /* the scope's grids, a lamp on, dim greys, a plate */
};

#define FB_NTHEME 3
/* three looks of one palette: magenta (the default), cyan (the gate and the arp) and flame (the glitch); only the accent
 * ramp differs */
static const uint32_t SK_THEME[FB_NTHEME][40] = {
    {   /* magenta */
        0x0A0614, 0x120A22, 0x1C1032, 0x2A1C48, 0x2A0A48, 0x4A1478,
        0x7A1CA8, 0xB024D0, 0xE030E8, 0xFF58F0, 0xFF90F4, 0xFFC8FA,
        0x2A2C4A, 0x444870, 0x646A98, 0x868CB8, 0xAAB0D8, 0xCED2F0,
        0xF0F2FF, 0x3A0A30, 0x701058, 0xA81880, 0xE020A8, 0xFF50D0,
        0xB8FFFF, 0x0C0620, 0x160A34, 0x220E4A, 0x341468, 0x4C1C8C,
        0x6A28B0, 0xF4ECFF, 0x9C90C0, 0x1A0E34, 0x40FFD0, 0x34204C,
        0x6A2A9C, 0x4A4470, 0x6A6290, 0x1A1230,
    },
    {   /* cyan */
        0x0A0614, 0x120A22, 0x1C1032, 0x2A1C48, 0x04283A, 0x084860,
        0x0C7088, 0x10A0B4, 0x18D0E0, 0x40F0F8, 0x90FAFF, 0xD0FFFF,
        0x2A2C4A, 0x444870, 0x646A98, 0x868CB8, 0xAAB0D8, 0xCED2F0,
        0xF0F2FF, 0x3A0A30, 0x701058, 0xA81880, 0xE020A8, 0xFF50D0,
        0xFFE8FF, 0x0C0620, 0x160A34, 0x220E4A, 0x341468, 0x4C1C8C,
        0x6A28B0, 0xF4ECFF, 0x9C90C0, 0x1A0E34, 0x40FFD0, 0x34204C,
        0x6A2A9C, 0x4A4470, 0x6A6290, 0x1A1230,
    },
    {   /* flame */
        0x0A0614, 0x120A22, 0x1C1032, 0x2A1C48, 0x3A1204, 0x6A2208,
        0x9C3408, 0xCC4A0C, 0xF26410, 0xFF8418, 0xFFA840, 0xFFD080,
        0x2A2C4A, 0x444870, 0x646A98, 0x868CB8, 0xAAB0D8, 0xCED2F0,
        0xF0F2FF, 0x3A0A30, 0x701058, 0xA81880, 0xE020A8, 0xFF50D0,
        0xFFF0C0, 0x0C0620, 0x160A34, 0x220E4A, 0x341468, 0x4C1C8C,
        0x6A28B0, 0xF4ECFF, 0x9C90C0, 0x1A0E34, 0x40FFD0, 0x34204C,
        0x6A2A9C, 0x4A4470, 0x6A6290, 0x1A1230,
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
    for (i = 0; i < 40u; i++) {
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
    for (i = 0; i < 40u; i++) {
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
