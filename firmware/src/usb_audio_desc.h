/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * ChoralRoot changes (six capture channels, PCM16 capture, the channel names, the playback function removed; 0.19:
 * the second sampling frequency)
 * Copyright (C) 2026 ChoralRoot FM-1 contributors
 * GartStringer changes (a stereo line input at 48 kHz only: two channels, one sampling frequency, the smaller endpoint) */
/* Melodee's UAC1 recording function (usb_audio_desc.h, Melodee 0.11.1), 44.1 and 48 kHz. ChoralRoot has no playback
 * function (Melodee's "Out": the computer's audio through the FM-1, removed in 0.14).
 * IF2 control + IF3 streaming: recording "ChoralRoot In" (string 3), EP2 IN, alternate 1 = PCM16 (six channels:
 * master L R, CHORD L R, BASS L R; their names are strings 4..9, the input terminal's iChannelNames).
 * Included inside usb.c's CFG_DESC (after the MIDI function), whose length is CFG_LEN. */
/* GartStringer: IF2 control + IF3 streaming, the recording "GartStringer In" (string 3): a line input of two channels
 * (left, right), PCM16, 48000 Hz and no other rate, EP2 IN. As plain as a capture device gets: what a phone expects. */
    8, 0x0B, 2, 2, 1, 1, 0, 3,                      /* IAD: recording (IF 2-3) */
    9, 4, 2, 0, 0, 1, 1, 0, 3,
    9, 0x24, 1, 0x00, 0x01, 30, 0, 1, 3,            /* AC header: one stream, IF 3 */
    12, 0x24, 2, 3, 0x03, 0x06, 0, 2, 0x03, 0x00, 0, 0,   /* input terminal 3: a line connector, two channels, left and right front */
    9, 0x24, 3, 4, 0x01, 0x01, 0, 3, 0,             /* output terminal 4: USB streaming, from 3 */

    9, 4, 3, 0, 0, 1, 2, 0, 0,                      /* alternate 0: no endpoint (the stream closed) */
    9, 4, 3, 1, 1, 1, 2, 0, 0,                      /* alternate 1: the stream */
    7, 0x24, 1, 4, 1, 1, 0,                         /* PCM */
    11, 0x24, 2, 1, 2, 2, 16, 1, 0x80, 0xBB, 0x00,  /* type I: 2 channels, 2 bytes, 16 bits, one rate: 48000 Hz */
    9, 5, 0x82, 0x05, UA_EP_MAXP & 0xFF, UA_EP_MAXP >> 8, 1, 0, 0,   /* EP2 IN isochronous async: packets of 48 frames
                                                     * (47 or 49 for the drift): up to 196 bytes, 232 reserved (bit stuffing) */
    7, 0x25, 1, 1, 0, 0, 0,                         /* sampling-frequency control (the one rate: SET_CUR is taken, GET_CUR says 48000) */
