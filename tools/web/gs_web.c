/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GartStringer contributors */
/* GartStringer in the browser: the instrument's own sources (the voices, the effects, the patches, the pages) compiled to a
 * freestanding WebAssembly module, no C library. The page's AudioWorklet (web/play/worklet.js) asks it for audio, 128
 * frames a call; the call runs the instrument's clock forward by as much, reads the panel's input the page has queued,
 * and about thirty times a second redraws the screen.
 *
 *   sh tools/build_web.sh  ->  build/web/gartstringer.wasm
 * The panel: web_btn(label, down), web_enc(role, steps), web_key(n, down). The screen: web_fb() is 240 x 240 RGBA,
 * web_fb_serial() changes when it does. LEDs: web_leds(): 14 buttons then 27 keys, a level 0..3. */
#include <stdint.h>

void *memset(void *d, int c, unsigned n)
{
    unsigned char *p = d;
    while (n--)
        *p++ = (unsigned char)c;
    return d;
}
void *memcpy(void *d, const void *s, unsigned n)
{
    unsigned char *p = d;
    const unsigned char *q = s;
    while (n--)
        *p++ = *q++;
    return d;
}

#define SK_POOL
#include "gs_synth.c"
#include "gs_fx.c"
#include "gs_patch.c"

static uint32_t web_px[240 * 240];
static uint32_t web_serial;
static void sk_lcd(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) {
            uint16_t s = p[j * w + i], v = (uint16_t)((s >> 8) | (s << 8));
            uint32_t r = ((uint32_t)(v >> 11) & 31u) * 255u / 31u, g = ((uint32_t)(v >> 5) & 63u) * 255u / 63u,
                     b = ((uint32_t)v & 31u) * 255u / 31u;
            web_px[(y + j) * 240u + x + i] = 0xFF000000u | (b << 16) | (g << 8) | r;
        }
    web_serial++;
}
static void sk_pump(void) {}

#include "sk_fb.c"
#include "gs_ui.c"

static int32_t w_enc[SE_N];
static uint32_t w_held, w_pr, w_keys, w_kp;
static uint8_t w_ledb[SB_N], w_ledk[27], w_leds[SB_N + 27];
static int32_t sk_in_enc(uint32_t r)
{
    int32_t d = w_enc[r];
    w_enc[r] = 0;
    return d;
}
static void sk_in_buttons(uint32_t *held, uint32_t *pressed)
{
    *held = w_held;
    *pressed = w_pr;
    w_pr = 0;
}
static uint32_t sk_in_keys(uint32_t *held)
{
    uint32_t p = w_kp;
    *held = w_keys;
    w_kp = 0;
    return p;
}
static void sk_led_btn(uint32_t l, uint32_t v) { w_ledb[l] = (uint8_t)v; }
static void sk_led_key(uint32_t n, uint32_t v) { w_ledk[n] = (uint8_t)v; }

#define WEB_EXPORT __attribute__((used, visibility("default")))
static float out_l[1024], out_r[1024];
static uint32_t dev_samples, last_ui, booted;
static uint32_t web_cpu;

/* the saved slots live in this buffer; the page keeps them in localStorage (web_store_set before web_boot to restore) */
static uint8_t web_store[1024];
static uint32_t web_store_len, web_store_gen;
static int sk_store_read(void *dst, uint32_t n)
{
    uint32_t i;
    if (!web_store_len || n > web_store_len)
        return -1;
    for (i = 0; i < n; i++)
        ((uint8_t *)dst)[i] = web_store[i];
    return 0;
}
static int sk_store_write(const void *src, uint32_t n)
{
    uint32_t i;
    if (n > sizeof web_store)
        return -1;
    for (i = 0; i < n; i++)
        web_store[i] = ((const uint8_t *)src)[i];
    web_store_len = n;
    web_store_gen++;
    return 0;
}
WEB_EXPORT uint8_t *web_store_ptr(void) { return web_store; }
WEB_EXPORT void web_store_set(uint32_t n) { web_store_len = n > sizeof web_store ? sizeof web_store : n; }
WEB_EXPORT uint32_t web_store_len_get(void) { return web_store_len; }
WEB_EXPORT uint32_t web_store_gen_get(void) { return web_store_gen; }

WEB_EXPORT void web_intro(void) { ui_intro_start(); }          /* the opening: the page calls it for the instrument that plays */
WEB_EXPORT void web_boot(void)
{
    ui_init(0);
    ui_store_load();
    booted = 1;
}
WEB_EXPORT float *web_out_l(void) { return out_l; }
WEB_EXPORT float *web_out_r(void) { return out_r; }
WEB_EXPORT uint32_t *web_fb(void) { return web_px; }
WEB_EXPORT uint32_t web_fb_serial(void) { return web_serial; }
WEB_EXPORT uint8_t *web_leds(void)
{
    uint32_t i;
    for (i = 0; i < SB_N; i++)
        w_leds[i] = w_ledb[i];
    for (i = 0; i < 27u; i++)
        w_leds[SB_N + i] = w_ledk[i];
    return w_leds;
}
WEB_EXPORT void web_btn(uint32_t label, uint32_t down)
{
    if (label >= SB_N)
        return;
    if (down) {
        w_pr |= 1u << label;
        w_held |= 1u << label;
    } else
        w_held &= ~(1u << label);
}
WEB_EXPORT void web_enc(uint32_t role, int32_t steps)
{
    if (role < SE_N)
        w_enc[role] += steps;
}
WEB_EXPORT void web_key(uint32_t n, uint32_t down)
{
    if (n >= 27u)
        return;
    if (down) {
        w_kp |= 1u << n;
        w_keys |= 1u << n;
    } else
        w_keys &= ~(1u << n);
}
WEB_EXPORT void web_master(uint32_t q12) { sk_f.master = q12; }
WEB_EXPORT uint32_t web_master_get(void) { return sk_f.master; }
WEB_EXPORT uint32_t web_cpu_get(void) { return web_cpu; }
WEB_EXPORT uint32_t web_patch(void) { return gs.preset; }

/* n frames of audio (a multiple of 32, at most 1024), the clock advanced with them */
WEB_EXPORT void web_render(uint32_t n)
{
    uint32_t i = 0;
    int32_t blk[2 * SK_CTL];
    if (!booted)
        web_boot();
    while (i < n) {
        uint32_t k, ms;
        sk_render(blk, SK_CTL);
        for (k = 0; k < SK_CTL; k++) {
            out_l[i + k] = (float)blk[2 * k] * (1.0f / 32768.0f);
            out_r[i + k] = (float)blk[2 * k + 1] * (1.0f / 32768.0f);
        }
        i += SK_CTL;
        dev_samples += SK_CTL;
        ms = (uint32_t)((uint64_t)dev_samples * 1000u / 44100u);
        if (ms - last_ui >= 33u) {
            last_ui = ms;
            ui_input(ms);
            ui_draw(ms);
        }
    }
}
