/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GartStringer contributors; the HAL: Felucca by Leo Kuroshita, Hügelton Instruments */
/* GartStringer: one compilation unit (the HAL is header-only). A supersaw lead and bass synthesizer for the FM-1: six voices,
 * oscillators that stack and sync and feed back, a filter that can sing a vowel, a trance gate, an arpeggio, an ensemble,
 * echo and reverb, and a glitch that stutters, reverses, stops the tape and drops the floor out. No drums.
 *
 *   gs_synth.c   the voices, the gate, the arp      gs_fx.c     the effects and the master
 *   gs_patch.c   the factory patches                gs_ui.c     the knobs, pages and pictures
 *   sk_fb.c      the screen (framebuffer, palette, text)        sk_hw.c  the FM-1: boot, audio, panel
 *
 * Host check: tools/host/gs_host.c builds the same sources into a program for a PC. The device build is tools/build.py. */

#ifndef FELUCCA_ID
#define FELUCCA_ID "FM-1_930"            /* package identity (build.py: the .fwsc marker string) */
#endif
#ifndef SK_VERSION
#define SK_VERSION "GARTSTRINGER"
#endif
#define SK_POOL __attribute__((section(".pool")))

#include <stdint.h>
#include "fm1_time.h"
#include "fm1_sys.h"
#include "fm1_irq.h"
#include "fm1_guard.h"
#include "fm1_input.h"
#include "fm1_timer.h"
#include "fm1_audio.h"
#include "fm1_adc.h"
#include "fm1_lcd_hw.h"
#include "fm1_flash.h"
#include "sk_bootguard.h"

#include "libc.c"
#include "lcd.c"

#include "gs_synth.c"
#include "gs_fx.c"
#include "gs_patch.c"
#include "sk_fb.c"
#include "gs_ui.c"
#include "sk_hw.c"
