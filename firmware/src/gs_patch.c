/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GartStringer contributors */
/* The factory patches and loading a patch. Included by gartstringer.c after gs_fx.c. */

#include "gs_presets.h"

static void gs_load(uint32_t i)
{
    uint32_t k;
    gs_all_off();
    gs.acur = -1;
    gs.preset = (uint8_t)(i % GS_NPRE);
    for (k = 0; k < P_N; k++)
        gs.p[k] = GS_PRE[gs.preset].p[k];
    gs.bpm = GS_PRE[gs.preset].bpm;
    gs.gstep = 15;
    gs.gacc = 0;
    gs.astep = 0;
}

static void gs_init(void)
{
    gs.oct = 0;
    gs.hold = 0;
    gs.acur = -1;
    gs.rmask = 0x5555u;
    gs_load(0);
}

/* a parameter, kept in its range */
static void gs_set(uint32_t idx, int v)
{
    static const uint8_t MAXV[P_N] = {
        [P_MODE] = GS_NMODE - 1, [P_OCT] = 4, [P_FTYPE] = GS_NFT - 1, [P_LDST] = 4, [P_LSHP] = 3, [P_VMODE] = 2, [P_DTIM] = 5,
        [P_GPAT] = GS_NPAT - 1, [P_GRATE] = 3, [P_APAT] = 7, [P_ARATE] = 4, [P_ARNG] = 3, [P_AON] = 1, [P_GLL] = 4, [P_GLT] = 3,
    };
    int mx = MAXV[idx] ? MAXV[idx] : 127;
    if (v < 0)
        v = 0;
    if (v > mx)
        v = mx;
    if (idx == P_ARNG && v < 1)
        v = 1;
    if (gs.p[idx] != (uint8_t)v && (idx == P_VMODE || idx == P_AON))
        gs_mode_changed();
    gs.p[idx] = (uint8_t)v;
}
