#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# GartStringer: the 32 factory patches (firmware/src/gs_presets.h), written as differences from a plain supersaw.
#   python3 tools/gen_presets.py > firmware/src/gs_presets.h
import sys

ORDER = ["MODE", "DET", "MIX", "SUB", "COL", "OCT", "CUT", "RES", "FENV", "KEY", "FTYPE", "DRV",
         "AA", "AD", "AS", "AR", "FA", "FD", "FS", "FR", "LRATE", "LAMT", "LDST", "LSHP", "GLIDE", "VMODE",
         "CHOR", "DLY", "DTIM", "DFB", "REV", "RSZ", "GPAT", "GRATE", "GDEP", "GSMO", "APAT", "ARATE", "ARNG", "AGATE",
         "AON", "GLP", "GLL", "GLT", "CRU", "SRATE", "SLEN", "SSWG", "SGATE", "SDIR", "CON", "CKEY", "CSCL", "CLAY", "CVOI"]
BASE = dict(MODE=0, DET=54, MIX=64, SUB=0, COL=60, OCT=2, CUT=80, RES=30, FENV=40, KEY=30, FTYPE=0, DRV=10,
            AA=4, AD=60, AS=100, AR=50, FA=6, FD=60, FS=40, FR=50, LRATE=40, LAMT=0, LDST=0, LSHP=0, GLIDE=0, VMODE=0,
            CHOR=60, DLY=30, DTIM=2, DFB=50, REV=40, RSZ=70, GPAT=13, GRATE=1, GDEP=0, GSMO=40,
            APAT=0, ARATE=2, ARNG=1, AGATE=90, AON=0, GLP=0, GLL=0, GLT=0, CRU=0,
            SRATE=1, SLEN=15, SSWG=0, SGATE=70, SDIR=0,
            CON=0, CKEY=0, CSCL=1, CLAY=1, CVOI=0)
# modes: 0 supersaw 1 hypersync 2 pwm 3 roll bass 4 feedback; filters: 0 lp12 1 lp24 2 band 3 high 4 vowel
# lfo dest: 0 pitch 1 filter 2 amp 3 colour 4 detune; shapes: 0 sine 1 tri 2 square 3 s+h; voice: 0 poly 1 mono 2 legato
# echo: 0 1/16 1 1/8 2 dotted 1/8 3 1/4 4 1/8T 5 3/16; arp: 0 up 1 down 2 updown 3 random 4 played 5 chord 6 bounce 7 rolling
# arp rate: 0 1/8 1/8T 1/16 1/16T 1/32; gate rate: 0 1/8 1 1/16 2 1/32 3 1/16T; gate patterns: see gs_synth.c
# glitch kind: 0 stutter 1 reverse 2 tape stop 3 drop out; length: 0 1/16 1 1/32 2 1/8 3 1/64 4 1/16T
P = [
 # ---- leads: from the energetic to the haunting
 ("Sunrise Anthem", 138, dict(MODE=0, DET=72, MIX=92, CUT=96, RES=34, FENV=44, AA=2, AD=70, AS=104, AR=52, GLIDE=12, VMODE=2, CHOR=84, DLY=46, DTIM=2, DFB=56, REV=62, RSZ=80, LRATE=62, LAMT=10)),
 ("Heartstring", 136, dict(MODE=0, DET=40, MIX=58, SUB=0, CUT=74, RES=42, FTYPE=1, FENV=30, AA=34, AD=70, AS=104, AR=88, GLIDE=26, VMODE=2, LRATE=52, LAMT=34, LDST=0, CHOR=104, DLY=64, DTIM=2, DFB=62, REV=96, RSZ=96)),
 ("Hypernova", 140, dict(MODE=1, DET=64, MIX=70, COL=66, CUT=96, RES=46, FENV=52, AA=1, AD=64, AS=96, AR=44, GLIDE=8, VMODE=1, CHOR=70, DLY=50, DTIM=2, DFB=54, REV=50, RSZ=72, LRATE=44, LAMT=22, LDST=3)),
 ("Laser Rain", 138, dict(MODE=0, DET=60, MIX=78, CUT=100, RES=40, FENV=70, AA=0, AD=46, AS=28, AR=36, FD=42, FS=20, AON=1, APAT=0, ARATE=2, ARNG=2, AGATE=60, CHOR=70, DLY=70, DTIM=2, DFB=64, REV=70, RSZ=84)),
 ("Midnight Choir", 130, dict(MODE=0, DET=34, MIX=70, CUT=60, RES=70, FTYPE=4, FENV=60, AA=52, AD=80, AS=110, AR=96, FA=60, FD=90, FS=60, LRATE=22, LAMT=40, LDST=1, LSHP=0, CHOR=110, DLY=40, DTIM=3, DFB=48, REV=110, RSZ=104)),
 ("Ghost Signal", 126, dict(MODE=4, DET=36, MIX=60, COL=44, CUT=84, RES=34, FTYPE=1, FENV=36, AA=30, AD=76, AS=100, AR=96, GLIDE=40, VMODE=2, LRATE=48, LAMT=46, LDST=0, CHOR=94, DLY=76, DTIM=2, DFB=72, REV=100, RSZ=100)),
 ("Rave Stab", 138, dict(MODE=0, DET=66, MIX=100, CUT=88, RES=22, FENV=70, AA=0, AD=48, AS=14, AR=42, FD=44, FS=10, CHOR=70, DLY=56, DTIM=2, DFB=58, REV=48, RSZ=70, DRV=26)),
 ("Euphoria Gate", 138, dict(MODE=0, DET=62, MIX=88, CUT=92, RES=28, FENV=30, AA=6, AD=70, AS=110, AR=60, GPAT=3, GRATE=1, GDEP=112, GSMO=24, CHOR=96, DLY=52, DTIM=2, DFB=56, REV=76, RSZ=88)),
 ("Ethereal Pluck", 134, dict(MODE=2, DET=40, MIX=60, COL=70, CUT=76, RES=30, FENV=70, AA=0, AD=50, AS=20, AR=48, FD=46, FS=14, AON=1, APAT=2, ARATE=2, ARNG=2, AGATE=70, LRATE=36, LAMT=40, LDST=3, CHOR=90, DLY=66, DTIM=2, DFB=62, REV=84, RSZ=90)),
 ("Solar Flare", 140, dict(MODE=1, DET=80, MIX=60, COL=80, CUT=92, RES=52, FENV=64, AA=1, AD=70, AS=100, AR=50, GLIDE=18, VMODE=1, LRATE=70, LAMT=60, LDST=3, LSHP=1, CHOR=60, DLY=44, DTIM=1, DFB=50, REV=44, RSZ=70, DRV=34)),
 ("Cathedral Lead", 124, dict(MODE=0, DET=28, MIX=50, CUT=40, RES=26, FTYPE=1, FENV=76, AA=64, AD=90, AS=110, AR=100, FA=66, FD=80, FS=70, VMODE=2, GLIDE=18, CHOR=100, DLY=48, DTIM=3, DFB=50, REV=116, RSZ=114)),
 ("Neon Cry", 142, dict(MODE=1, DET=58, MIX=60, COL=70, CUT=96, RES=40, FENV=40, AA=6, AD=70, AS=104, AR=54, GLIDE=36, VMODE=1, LRATE=54, LAMT=58, LDST=0, CHOR=80, DLY=60, DTIM=2, DFB=62, REV=66, RSZ=80)),
 # ---- basses
 ("Rolling Thunder", 140, dict(MODE=3, DET=20, MIX=50, SUB=70, COL=40, OCT=1, CUT=56, RES=50, FTYPE=1, FENV=66, KEY=20, AA=0, AD=40, AS=80, AR=24, FD=40, FS=10, AON=1, APAT=7, ARATE=2, ARNG=1, AGATE=70, VMODE=1, CHOR=20, DLY=16, DTIM=2, REV=18, RSZ=40, DRV=30)),
 ("Offbeat Pump", 138, dict(MODE=3, DET=24, MIX=60, SUB=60, COL=50, OCT=1, CUT=60, RES=40, FTYPE=1, FENV=50, AA=0, AD=50, AS=104, AR=30, GPAT=0, GRATE=1, GDEP=124, GSMO=16, VMODE=1, CHOR=10, DLY=0, REV=14, RSZ=40, DRV=24)),
 ("Acid Trance", 136, dict(MODE=3, DET=10, MIX=20, SUB=30, COL=10, OCT=2, CUT=42, RES=96, FTYPE=1, FENV=100, KEY=40, AA=0, AD=44, AS=50, AR=26, FD=40, FS=4, GLIDE=32, VMODE=2, CHOR=0, DLY=36, DTIM=2, DFB=60, REV=24, RSZ=50, DRV=60)),
 ("Sub Reactor", 140, dict(MODE=4, DET=6, MIX=30, SUB=90, COL=18, OCT=1, CUT=50, RES=20, FTYPE=0, FENV=20, AA=2, AD=70, AS=110, AR=44, VMODE=1, CHOR=0, DLY=0, REV=10, RSZ=30)),
 ("Hoover Bass", 138, dict(MODE=0, DET=96, MIX=100, SUB=40, OCT=1, CUT=70, RES=30, FENV=56, KEY=40, AA=0, AD=56, AS=100, AR=34, FD=50, FS=30, GLIDE=26, VMODE=2, LRATE=40, LAMT=26, LDST=0, CHOR=50, DLY=18, DTIM=2, REV=24, RSZ=50, DRV=64)),
 ("Gator Gate", 140, dict(MODE=3, DET=30, MIX=70, SUB=64, COL=70, OCT=1, CUT=64, RES=56, FTYPE=1, FENV=60, AA=0, AD=60, AS=104, AR=26, GPAT=6, GRATE=1, GDEP=127, GSMO=10, VMODE=1, DLY=22, DTIM=1, DFB=44, REV=20, RSZ=40, DRV=40)),
 ("Pitch Drop", 134, dict(MODE=3, DET=40, MIX=60, SUB=80, COL=40, OCT=1, CUT=58, RES=60, FTYPE=1, FENV=70, AA=0, AD=60, AS=90, AR=50, GLIDE=70, VMODE=2, LRATE=30, LAMT=70, LDST=0, LSHP=1, DLY=40, DTIM=2, DFB=56, REV=36, RSZ=60, DRV=36)),
 ("Zap Bass", 142, dict(MODE=2, DET=36, MIX=60, SUB=70, COL=60, OCT=1, CUT=62, RES=64, FTYPE=1, FENV=76, AA=0, AD=40, AS=70, AR=26, FD=36, FS=10, LRATE=70, LAMT=84, LDST=3, LSHP=1, VMODE=1, CHOR=36, DLY=24, DTIM=2, REV=22, RSZ=44, DRV=44)),
 # ---- pads and strings
 ("Silver Strings", 128, dict(MODE=0, DET=34, MIX=60, CUT=70, RES=18, FENV=24, AA=70, AD=90, AS=116, AR=104, FA=70, FD=80, FS=90, CHOR=118, DLY=24, DTIM=3, DFB=40, REV=96, RSZ=100)),
 ("Orange Haze", 124, dict(MODE=1, DET=50, MIX=50, COL=40, CUT=62, RES=26, FTYPE=1, FENV=40, AA=66, AD=90, AS=110, AR=100, LRATE=30, LAMT=40, LDST=1, CHOR=108, DLY=44, DTIM=2, DFB=50, REV=104, RSZ=108)),
 ("Aurora Pad", 120, dict(MODE=0, DET=44, MIX=70, CUT=56, RES=80, FTYPE=4, FENV=30, AA=72, AD=90, AS=110, AR=108, LRATE=14, LAMT=60, LDST=1, LSHP=1, CHOR=112, DLY=50, DTIM=3, DFB=52, REV=116, RSZ=118)),
 ("Dream Machine", 126, dict(MODE=2, DET=46, MIX=60, COL=40, CUT=66, RES=24, FENV=34, AA=60, AD=84, AS=110, AR=100, LRATE=30, LAMT=70, LDST=3, LSHP=0, CHOR=112, DLY=40, DTIM=2, DFB=46, REV=100, RSZ=104)),
 ("Cold Breath", 118, dict(MODE=4, DET=30, MIX=40, COL=30, CUT=60, RES=40, FTYPE=1, FENV=20, AA=76, AD=92, AS=108, AR=112, LRATE=20, LAMT=50, LDST=0, CHOR=108, DLY=36, DTIM=3, DFB=48, REV=112, RSZ=116)),
 ("Velvet Choir", 122, dict(MODE=0, DET=38, MIX=76, CUT=24, RES=84, FTYPE=4, FENV=70, AA=54, AD=84, AS=108, AR=100, FA=50, FD=80, FS=60, LRATE=26, LAMT=36, LDST=0, CHOR=110, DLY=30, DTIM=3, DFB=40, REV=108, RSZ=110)),
 # ---- arps, gates, glitch
 ("Trance Arp", 138, dict(MODE=0, DET=62, MIX=80, CUT=82, RES=34, FENV=70, AA=0, AD=44, AS=24, AR=38, FD=40, FS=14, AON=1, APAT=0, ARATE=2, ARNG=3, AGATE=56, CHOR=84, DLY=62, DTIM=2, DFB=60, REV=64, RSZ=80, VMODE=1)),
 ("Pluck Cascade", 140, dict(MODE=2, DET=34, MIX=60, COL=50, CUT=88, RES=30, FENV=80, AA=0, AD=40, AS=10, AR=34, FD=36, FS=6, AON=1, APAT=3, ARATE=4, ARNG=3, AGATE=40, CHOR=70, DLY=70, DTIM=1, DFB=66, REV=72, RSZ=84, VMODE=1)),
 ("Gate Runner", 138, dict(MODE=0, DET=60, MIX=90, CUT=94, RES=30, FENV=30, AA=2, AD=70, AS=110, AR=50, GPAT=2, GRATE=1, GDEP=127, GSMO=14, CHOR=90, DLY=60, DTIM=2, DFB=58, REV=70, RSZ=84)),
 ("Glitch Engine", 140, dict(MODE=1, DET=56, MIX=60, COL=70, CUT=96, RES=44, FENV=60, AA=0, AD=48, AS=30, AR=36, FD=42, FS=12, AON=1, APAT=3, ARATE=2, ARNG=2, AGATE=50, GLP=76, GLL=1, GLT=0, CRU=40, CHOR=70, DLY=64, DTIM=2, DFB=62, REV=60, RSZ=76, VMODE=1)),
 ("Tape Stop Lead", 138, dict(MODE=0, DET=64, MIX=90, CUT=98, RES=34, FENV=40, AA=2, AD=70, AS=104, AR=54, GLIDE=14, VMODE=2, GLP=64, GLL=2, GLT=2, CHOR=88, DLY=48, DTIM=2, DFB=56, REV=64, RSZ=84)),
 ("Breakdown Pad", 132, dict(MODE=0, DET=48, MIX=70, CUT=66, RES=26, FENV=36, AA=40, AD=84, AS=108, AR=96, GLP=72, GLL=2, GLT=3, CHOR=104, DLY=60, DTIM=2, DFB=66, REV=110, RSZ=112, GPAT=7, GDEP=0)),
]

# the sequences: sixteen steps, "-" a rest, a number the semitones from the root (negative below), a: accent, s: slide
SEQ_DEFAULT = "0 - - - 0 - - - 0 - - - 0 - - -"
SEQ_PAD = "0s 0s 0s 0s 0s 0s 0s 0s 0s 0s 0s 0s 0s 0s 0s 0"
SEQ = {
 "Sunrise Anthem": "12 - - 12 - 15 - 14 - - 12 - 10 - 7 -",
 "Heartstring": "7 - - 10 - - 12 - - 15 - - 14 - 12 10",
 "Hypernova": "0a 0 12 0 15a 0 12 0 14a 0 12 0 10a 0 7 0",
 "Laser Rain": "12 15 19 15 12 15 19 15 10 14 17 14 10 14 17 14",
 "Ghost Signal": "0 - - - 7 - - - 5 - - - 3 - - -",
 "Rave Stab": "0a - - 0a - - 0a - 0a - - 0a - 7a - -",
 "Euphoria Gate": "0 - 7 - 12 - 15 - 14 - 12 - 10 - 7 -",
 "Ethereal Pluck": "12 - 15 - 19 - 15 - 12 - 14 - 17 - 14 -",
 "Solar Flare": "0 7 12 7 0 7 15 7 0 7 14 7 0 7 10 7",
 "Cathedral Lead": "12s 12s 12 - 15s 15s 14 - 12s 12s 10 - 7 - - -",
 "Neon Cry": "7 - 12 - 15as 14 - 12 - 10 - 7 - 5 - 7",
 "Rolling Thunder": "- 0 0 0 - 0 0 0 - 0 0 0 - 0 0 0",
 "Offbeat Pump": "- - 0 - - - 0 - - - 0 - - - 0 -",
 "Acid Trance": "0 0a 12 0 3s 5 - 0a 7 0 12as 10 0 - 3 0a",
 "Sub Reactor": "0 - - - - - - - -2 - - - - - 0 -",
 "Hoover Bass": "0a - 0 - 12 - 0 - 0a - 0 - 10 - 7 -",
 "Gator Gate": "0 0 0 0 0 0 0 0 7 7 7 7 5 5 3 3",
 "Pitch Drop": "12s 10s 7s 5s 3s 0s 0 - 12s 10s 7s 5s 3s 0s -12 -",
 "Zap Bass": "0 - 0 12 0 - 0 7 0 - 0 12 0 - 10 7",
 "Trance Arp": "0 7 12 7 15 7 12 7 0 7 12 7 14 7 10 7",
 "Pluck Cascade": "12 7 15 12 19 15 12 7 10 5 14 10 17 14 10 5",
 "Gate Runner": "0 0 0 0 7 7 7 7 5 5 5 5 3 3 3 3",
 "Glitch Engine": "0 12 0 15 0 12 0 14 0 12 0 10 0 12 0 7",
 "Tape Stop Lead": "12 - - 15 - - 14 - - 12 - - 10 - - 7",
 "Silver Strings": SEQ_PAD, "Orange Haze": SEQ_PAD, "Aurora Pad": SEQ_PAD, "Dream Machine": SEQ_PAD,
 "Cold Breath": SEQ_PAD, "Velvet Choir": SEQ_PAD, "Breakdown Pad": SEQ_PAD,
 "Midnight Choir": "0s 0s 0s 0 7s 7s 7s 7 5s 5s 5s 5 3s 3s 3s 3",
}


def seq_bytes(txt):
    t = txt.split()
    assert len(t) == 16, txt
    n, f = [], []
    for x in t:
        if x == "-":
            n.append(12)
            f.append(0)
            continue
        fl = 1
        while x[-1] in "as":
            fl |= 2 if x[-1] == "a" else 4
            x = x[:-1]
        v = 12 + int(x)
        assert 0 <= v <= 36, txt
        n.append(v)
        f.append(fl)
    return n + f

assert len(P) == 32, len(P)
out = ["/* SPDX-License-Identifier: GPL-3.0-only */", "/* generated by tools/gen_presets.py: do not edit */",
       "static const struct { const char *name; uint8_t bpm; uint8_t p[P_N]; uint8_t sq[32]; } GS_PRE[GS_NPRE] = {"]
for name, bpm, d in P:
    assert len(name) <= 15, name
    for k in d:
        assert k in BASE, k
    v = dict(BASE)
    v.update(d)
    for k, x in v.items():
        assert 0 <= x <= 127, (name, k, x)
    out.append('    {"%s", %d, {%s}, {%s}},' % (name, bpm, ", ".join("[P_%s] = %d" % (k, v[k]) for k in ORDER),
                                                  ", ".join(str(x) for x in seq_bytes(SEQ.get(name, SEQ_DEFAULT)))))
out.append("};")
print("\n".join(out))
