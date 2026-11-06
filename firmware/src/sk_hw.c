/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GartStringer contributors; the HAL under it (firmware/hal): Felucca by Leo Kuroshita, Hügelton Instruments */
/* GartStringer's platform on the FM-1: the boot, the audio interrupt, the 10 kHz scan, the panel, the screen's link.
 *
 * Boot order: the watchdog and the boot-loop guard first (two crash resets within 30 s of a boot in a row -> SAFE: the
 * same instrument with the room's heaviest parts off; four -> the update mode), fatal vectors and guards, then the
 * screen, the panel, the MASTER pot, the audio and last the interrupts. The boot stub in front of this code (firmware/stub)
 * holds OCT- at power-on for the update mode; in the instrument OCT- + OCT+ + HOME held five seconds does the same.
 * The sound is all in one interrupt (ALNK0): each half buffer it renders the voices, the room and the master. */

extern uint32_t _data_start[], _data_end[], _data_load[], _bss_start[], _bss_end[];
extern uint32_t _pool_start[], _pool_end[], _rt_start[], _rt_end[], _rt_load[];

#define HALF_FRAMES 128u                  /* the I2S half buffer: 2.9 ms */
#define HALF_WORDS (HALF_FRAMES * 2u)
#define OUT_SHIFT 7                       /* Q15 -> 24 bit, a 6 dB ceiling under full scale */
#define SK_BG_SETTLE_MS 30000u

static int32_t abuf[2u * HALF_WORDS] __attribute__((aligned(4)));
static volatile uint32_t fm1_ms;
/* sk_cpu_pct (the audio interrupt's load, % of a half's time, smoothed), sk_cpu_max and sk_late (halves that overran): declared in sk_ui.c */
static struct bootguard bootguard __attribute__((section(".noinit")));
static uint8_t sk_safe;
static uint32_t sk_stage __attribute__((section(".noinit")));      /* the boot's breadcrumb: survives a crash */
#define CR_STAGE_SET(n) (sk_stage = (n))
static volatile uint8_t sk_hush;          /* 1: the output is silent (a slot is being written) */
static volatile uint8_t sk_in_audio;      /* the audio interrupt is rendering: TIMER5 nests in it and leaves the USB poll to the next tick */
static uint8_t sk_live;                   /* interrupts are on (the flash code must give them back only then) */
#define SK_STORE_OFF 913408u              /* the top sector of the data area, just under the update loader's staging (decimal: the lint reads hex as an address) */
extern void isr_timer5(void);
extern void isr_alnk0(void);

/* ---------------------------------------------------------------- interrupts --- */
void fm1_timer5_irq(void)
{
    static uint32_t last, acc, sub, owed;
    uint32_t t0 = fm1_ticks();
    fm1_timer5_ack();
    fm1_input_tick();
    acc += t0 - last;
    last = t0;
    while (acc >= 1000u * FM1_TICKS_PER_US) {
        acc -= 1000u * FM1_TICKS_PER_US;
        fm1_ms++;
    }
    if (sub % 5u == 0u)
        owed = 1;                                      /* 2 kHz: all USB SIE traffic lives here (usb.c) */
    if (++sub == 10u)
        sub = 0;
    if (sk_in_audio || !owed)
        return;                                        /* nested in a render: the next tick polls (owed stays) */
    owed = 0;
    usb_poll();
}

void fm1_alnk0_irq(void)
{
    uint8_t p = fm1_audio_pending();
    uint32_t t0 = fm1_ticks();
    fm1_audio_ack_aux(p);
    sk_in_audio = 1;
    if (p & FM1_AUDIO_HALF) {
        uint32_t half, b, i, us, load;
        int32_t *o;
        fm1_audio_ack_half();                          /* before the render: a boundary passing during it keeps its interrupt */
        half = fm1_audio_free_half();
        o = &abuf[half * HALF_WORDS];
        for (b = 0; b < HALF_FRAMES; b += SK_CTL) {
            int32_t *q = o + 2u * b;
            sk_render(q, SK_CTL);
            for (i = 0; i < 2u * SK_CTL; i++)
                q[i] *= 1 << OUT_SHIFT;
        }
        us = (fm1_ticks() - t0) / FM1_TICKS_PER_US;
        load = us * 100u / (HALF_FRAMES * 1000000u / SK_FS);
        if (sk_hush)
            for (i = 0; i < HALF_WORDS; i++)
                o[i] = 0;
        sk_cpu_pct = (sk_cpu_pct * 7u + load) / 8u;
        if (load > sk_cpu_max)
            sk_cpu_max = load;
        if (fm1_audio_free_half() != half)
            sk_late++;
    }
    sk_in_audio = 0;
}

/* ------------------------------------------------------------------ the panel --- */
static const uint8_t SK_BTN_ID[SB_N] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 1};   /* label -> matrix id */
static const uint8_t SK_ENC_ID[SE_N] = {0, 1, 6, 2, 3, 4, 5};                              /* role -> encoder */

static int32_t sk_in_enc(uint32_t role) { return fm1_enc_take(SK_ENC_ID[role]); }

static void sk_in_buttons(uint32_t *held, uint32_t *pressed)
{
    uint32_t rel, pr = fm1_input_edges(&rel), h = fm1_in.buttons, i, oh = 0, op = 0;
    (void)rel;
    for (i = 0; i < SB_N; i++) {
        oh |= ((h >> SK_BTN_ID[i]) & 1u) << i;
        op |= ((pr >> SK_BTN_ID[i]) & 1u) << i;
    }
    *held = oh;
    *pressed = op;
}

static uint32_t sk_in_keys(uint32_t *held)
{
    *held = fm1_in.notes;
    return fm1_input_note_edges();
}

/* an LED, four levels: 0 off, 1 faint, 2 dim, 3 lit */
static void sk_led_set(uint32_t id, uint32_t level)
{
    uint32_t p, r;
    for (p = 0; p < FM1_NCOL; p++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][p] == (int8_t)id) {
                uint8_t m = (uint8_t)(1u << r);
                fm1_led[p] = level == 3u ? (uint8_t)(fm1_led[p] | m) : (uint8_t)(fm1_led[p] & ~m);
                fm1_led_dim2[p] = level == 2u ? (uint8_t)(fm1_led_dim2[p] | m) : (uint8_t)(fm1_led_dim2[p] & ~m);
                fm1_led_faint[p] = level == 1u ? (uint8_t)(fm1_led_faint[p] | m) : (uint8_t)(fm1_led_faint[p] & ~m);
            }
}
static void sk_led_btn(uint32_t label, uint32_t level) { sk_led_set(SK_BTN_ID[label], level); }
static void sk_led_key(uint32_t n, uint32_t level) { sk_led_set(14u + n, level); }

/* ---------------------------------------------------------------- the saved slots --- */
/* One 4 KB sector of the SPI flash holds the eight slots (the whole struct is rewritten at every save). The flash is off
 * the bus while it erases (about 50 ms, 400 at most) and the interrupts are off with it, so the output is made silent
 * first and the audio gets a few halves to run out on zeros. */
static uint8_t sk_chk[sizeof sv];
static int sk_store_read(void *dst, uint32_t n)
{
    int rc;
    uint32_t f = irq_save();
    rc = FL_FAR(fl_read_ram)(SK_STORE_OFF, (uint8_t *)dst, n);
    if (sk_live)
        irq_restore(f);
    return rc ? -1 : 0;
}

static int sk_store_write(const void *src, uint32_t n)
{
    uint32_t took = 0, t, i;
    int rc;
    if (n > 4096u)
        return -1;
    sk_hush = 1;
    t = fm1_ms;
    while (fm1_ms - t < 20u)
        ;
    fm1_wdt_feed();
    rc = fl_erase4k(SK_STORE_OFF, &took);
    if (!rc)
        rc = fl_write(SK_STORE_OFF, (const uint8_t *)src, n);
    fm1_wdt_feed();
    if (!rc) {
        uint32_t f = irq_save();
        rc = FL_FAR(fl_read_ram)(SK_STORE_OFF, sk_chk, n);
        irq_restore(f);
        for (i = 0; !rc && i < n; i++)
            if (sk_chk[i] != ((const uint8_t *)src)[i])
                rc = -5;
    }
    sk_hush = 0;
    return rc ? -1 : 0;
}

/* ----------------------------------------------------------------- the screen --- */
static void sk_lcd(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p) { lcd_blit(x, y, w, h, p); }
static void sk_pump(void) {}

/* ------------------------------------------------------------------- faults --- */
static void sk_hexs(char *b, uint32_t v)
{
    uint32_t i;
    for (i = 0; i < 8u; i++)
        b[i] = "0123456789ABCDEF"[(v >> (28u - 4u * i)) & 15u];
    b[8] = 0;
}

static void fm1_fault(const fm1_crash_t *c)
{
    char b[12];
    uint32_t t0;
    fm1_audio_stop();
    fb_theme_snap(0);
    fb_fill(C_BG);
    fb_text(10, 10, "GARTSTRINGER stopped", C_R3);
    sk_hexs(b, c->vec);
    fb_text(10, 40, b, C_INK);
    sk_hexs(b, c->pc);
    fb_text(10, 58, b, C_INK);
    sk_hexs(b, c->rets);
    fb_text(10, 76, b, C_INK);
    sk_hexs(b, sk_stage);
    fb_text(10, 94, b, C_INK2);
    fb_text(10, 130, "restarting...", C_INK2);
    sk_present();
    t0 = fm1_ticks();
    while ((uint32_t)(fm1_ticks() - t0) < 4000u * 1000u * FM1_TICKS_PER_US)
        ;
    fm1_reboot();
}

/* ----------------------------------------------------- the update over USB (ota.c's hooks) --- */
/* The installer page talks USB-MIDI SysEx to the running instrument (usb.c, ota.c): a handshake answers the package
 * identity; the upgrade command pulls the package, stages its update loader in the OTA area and restarts into it.
 * The flash is off the bus while it erases and writes, so the sound is hushed first (as for a saved slot). */
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void ota_idle(void) { fm1_wdt_feed(); }
static int ota_in_area(uint32_t off, uint32_t n) { return FL_IN(off, n, OTA_AREA, OTA_AREA + OTA_AREA_LEN); }
static int ota_erase(uint32_t off)
{
    uint32_t took = 0, t;
    if (!ota_in_area(off, 0x1000u) || (off & 0xFFFu))
        return -8;
    if (sk_live) {
        sk_hush = 1;
        t = fm1_ms;
        while (fm1_ms - t < 20u)
            ;
    }
    return fl_erase4k(off, &took);
}
static int ota_prog(uint32_t off, const void *p, uint32_t n)
{
    if (!ota_in_area(off, n))
        return -8;
    return fl_write(off, (const uint8_t *)p, n);
}
static int ota_fread(uint32_t off, void *p, uint32_t n)
{
    int rc;
    uint32_t f = irq_save();
    rc = FL_FAR(fl_read_ram)(off, (uint8_t *)p, n);
    if (sk_live)
        irq_restore(f);
    return rc ? -1 : 0;
}
static void ota_show(uint32_t step, int32_t code)
{
    static const char *const STEP[] = {"", "PACKAGE", "CHECK HEAD", "LOADER", "CONFIRM", "RESTART"};
    char b[16];
    uint32_t k = 0, v;
    fb_theme_snap(0);
    fb_fill(C_BG);
    fb_text_c(120, 92, "UPDATE", C_INK);
    if (step < 9u) {
        fb_text_c(120, 116, STEP[step < 6u ? step : 0], C_HOT);
        sk_present();
        return;
    }
    if (code == 1) {
        fb_text_c(120, 116, "DRY RUN OK", C_HOT);
    } else {
        v = (uint32_t)-code;
        b[k++] = 'F'; b[k++] = 'A'; b[k++] = 'I'; b[k++] = 'L'; b[k++] = 'E'; b[k++] = 'D'; b[k++] = ' '; b[k++] = '-';
        if (v >= 10u)
            b[k++] = (char)('0' + v / 10u % 10u);
        b[k++] = (char)('0' + v % 10u);
        b[k] = 0;
        fb_text_c(120, 116, b, C_HOT);
    }
    sk_present();
    fm1_delay_ms(1500);
}
static void ota_commit(const uint8_t *parm)
{
    fm1_audio_stop();
    bootguard_intentional(&bootguard);                 /* an intentional reset: not a failed boot */
    usb_detach();
    fm1_delay_ms(30);
    fm1_enter_update(parm);                            /* the record into RAM, a core reset (fm1_sys.h): never returns */
}

/* --------------------------------------------------------------- MASTER, update --- */
static int32_t sk_master_knob = 512 * 16;
static void sk_master_apply(void) { sk_f.master = ((uint32_t)(sk_master_knob / 16) * (uint32_t)(sk_master_knob / 16)) >> 8; }
static void sk_master_poll(void)
{
    int32_t a = fm1_adc_read(FM1_ADC_MASTER);
    if (a >= 0) {
        sk_master_knob += (a * 16 - sk_master_knob) / 8;
        sk_master_apply();
    }
}
static void sk_master_boot(void)
{
    uint32_t i;
    (void)fm1_adc_read(FM1_ADC_MASTER);
    for (i = 0; i < 3u; i++) {
        int32_t a = fm1_adc_read(FM1_ADC_MASTER);
        if (a >= 0) {
            sk_master_knob = a * 16;
            sk_master_apply();
            return;
        }
    }
}

/* OCT- + OCT+ + HOME held five seconds: the update mode (a countdown from two). Returns the seconds left, 0 if none. */
static uint32_t sk_update_poll(uint32_t held)
{
    static uint32_t t0;
    uint32_t all = (1u << SB_OCTDN) | (1u << SB_OCTUP) | (1u << SB_HOME);
    if ((held & all) != all) {
        t0 = fm1_ms;
        return 0;
    }
    if (fm1_ms - t0 > 5000u) {
        fm1_audio_stop();
        fb_theme_snap(0);
        fb_fill(C_BG);
        fb_text_c(120, 108, "UPDATE MODE", C_INK);
        sk_present();
        fm1_delay_ms(30);
        bootguard_intentional(&bootguard);
        fm1_enter_uboot();
    }
    if (fm1_ms - t0 > 2000u)
        return (5000u - (fm1_ms - t0) + 999u) / 1000u;
    return 0;
}

/* -------------------------------------------------------------------- boot --- */
static void sk_splash(uint32_t ms)
{
    uint32_t t;
    for (t = 0; t < ms; t += 40u) {
        int a = (int)(t * 255u / ms);
        fb_theme_snap(1);
        fb_fill(C_BG);
        fb_text_c(120, 104, "GARTSTRINGER", a > 120 ? C_INK : C_INK2);
        fb_text_c(120, 124, sk_safe ? "SAFE MODE" : SK_VERSION, sk_safe ? C_HOT : C_INK2);
        fb_hline(90, 146, a * 60 / 255, C_O5);
        sk_present();
        fm1_delay_ms(40);
    }
}

static void sk_main(void)
{
    CR_STAGE_SET(BS_LCD);
    lcd_init();
    CR_STAGE_SET(BS_INPUT);
    fm1_input_init();
    fm1_adc_init();
    CR_STAGE_SET(BS_SOUNDS);
    ui_init(0);
    ui_store_load();                       /* (interrupts are still off: the flash is read bare) */
    ota_boot_cleanup();                    /* the staging area after an update: our loader stays for the boot stub's rescue */
    if (!sk_safe)
        ui_intro_start();                  /* the opening: the logo, the sound fading in */
    CR_STAGE_SET(BS_AUDIO);
    sk_master_boot();
    {
        uint32_t i;
        for (i = 0; i < 2u * HALF_WORDS; i++)
            abuf[i] = 0;
    }
    fm1_audio_init(abuf, HALF_WORDS, isr_alnk0, 3);
    CR_STAGE_SET(BS_IRQ);
    usb_start();                           /* the USB-MIDI interface, polled from TIMER5 (the installer page finds the unit through it) */
    fm1_timer5_start(isr_timer5, 4);
    fm1_guard_lock_top();
    fm1_irq_enable_all();
    sk_live = 1;
    fm1_delay_ms(30);
    if (sk_safe)
        sk_splash(2500u);
    for (;;) {
        uint32_t m = fm1_ms, left;
        fm1_wdt_feed();
        if (fm1_ms > SK_BG_SETTLE_MS && bootguard.pending)
            bootguard_settled(&bootguard);
        sk_master_poll();
        CR_STAGE_SET(BS_LOOP_INPUT);
        ui_input(m);
        left = sk_update_poll(ui.held);
        usb_retry(fm1_ms);
        ota_service();                                  /* the installer page's handshake */
        if (usb.ota_req) {                              /* its upgrade command: pull the package, stage the loader, restart */
            usb.ota_req = 0;
            sk_hush = 1;
            ota_session();                              /* returns only if nothing was committed */
            sk_hush = 0;
        }
        if (usb.uboot_req) {                            /* the host's soft key: the chip's own USB boot */
            fm1_audio_stop();
            fb_theme_snap(0);
            fb_fill(C_BG);
            fb_text_c(120, 108, "UPDATE MODE", C_INK);
            sk_present();
            fm1_delay_ms(30);
            usb_detach();
            bootguard_intentional(&bootguard);
            fm1_enter_uboot();
        }
        CR_STAGE_SET(BS_LOOP_DRAW);
        ui_draw(m);
        if (left) {
            char b[16], *p = b;
            *p++ = (char)('0' + left);
            *p = 0;
            fb_rbox(60, 90, 120, 40, 6, C_K2);
            fb_text_c(120, 96, "UPDATE MODE IN", C_INK);
            fb_text_c(120, 112, b, C_HOT);
            sk_present();
        }
        CR_STAGE_SET(BS_LOOP_IDLE);
        while (fm1_ms - m < 15u)
            ;
    }
}

void fm1_cstart(void)
{
    uint32_t *s, *d, p3, src, wdt, mode;
    fm1_time_init();
    fm1_reset_reason();
    p3 = fm1_boot.p3_rst;
    src = fm1_boot.rst_src;
    wdt = fm1_boot.wdt_con;
    fm1_wdt_arm(0x0D);
    mode = bootguard_step(&bootguard, bootguard_reason(p3, src));
    if (mode == BOOT_UBOOT)
        fm1_enter_uboot();
    fm1_irq_init();
    for (d = _bss_start; d < _bss_end; d++)
        *d = 0;
    for (d = _pool_start; d < _pool_end; d++)
        *d = 0;
    for (s = _data_load, d = _data_start; d < _data_end; s++, d++)
        *d = *s;
    for (s = _rt_load, d = _rt_start; d < _rt_end; s++, d++)
        *d = *s;
    fm1_mailbox_clear();
    fm1_guard_enable(FM1_GUARD_STACK | FM1_GUARD_WRITE | FM1_GUARD_BUS | FM1_GUARD_PC);
    fm1_boot.p3_rst = (uint8_t)p3;
    fm1_boot.rst_src = src;
    fm1_boot.wdt_con = (uint8_t)wdt;
    sk_safe = mode == BOOT_SAFE;
    sk_main();
    for (;;)
        ;
}
