#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# GartStringer: the 112 factory patches (the first 80: 32 trance, then 16 each of house, techno and garage; then eight more a world) (firmware/src/gs_presets.h), written as differences from a plain supersaw.
#   python3 tools/gen_presets.py > firmware/src/gs_presets.h
import sys

ORDER = ["MODE", "DET", "MIX", "SUB", "COL", "OCT", "CUT", "RES", "FENV", "KEY", "FTYPE", "DRV",
         "AA", "AD", "AS", "AR", "FA", "FD", "FS", "FR", "LRATE", "LAMT", "LDST", "LSHP", "GLIDE", "VMODE",
         "CHOR", "DLY", "DTIM", "DFB", "REV", "RSZ", "GPAT", "GRATE", "GDEP", "GSMO", "APAT", "ARATE", "ARNG", "AGATE",
         "AON", "GLP", "GLL", "GLT", "CRU", "SRATE", "SMET", "SSWG", "SGATE", "TAPE", "CON", "CKEY", "CSCL", "CLAY", "CVOI", "DRIFT",
         "M1S", "M1D", "M1A", "M2S", "M2D", "M2A", "M3S", "M3D", "M3A"]
BASE = dict(DRIFT=0, M1S=0, M1D=0, M1A=64, M2S=0, M2D=0, M2A=64, M3S=0, M3D=0, M3A=64, MODE=0, DET=54, MIX=64, SUB=0, COL=60, OCT=2, CUT=80, RES=30, FENV=40, KEY=30, FTYPE=0, DRV=10,
            AA=4, AD=60, AS=100, AR=50, FA=6, FD=60, FS=40, FR=50, LRATE=40, LAMT=0, LDST=0, LSHP=0, GLIDE=0, VMODE=0,
            CHOR=60, DLY=30, DTIM=2, DFB=50, REV=40, RSZ=70, GPAT=13, GRATE=1, GDEP=0, GSMO=40,
            APAT=0, ARATE=2, ARNG=1, AGATE=90, AON=0, GLP=0, GLL=0, GLT=0, CRU=0,
            SRATE=1, SMET=0, SSWG=0, SGATE=70, TAPE=0,
            CON=0, CKEY=0, CSCL=1, CLAY=1, CVOI=0)
# modes: 0 supersaw 1 hypersync 2 pwm 3 roll bass 4 feedback 5 fm keys 6 organ 7 donk 8 metal 9 noise 10 reese; filter 5 = acid; filters: 0 lp12 1 lp24 2 band 3 high 4 vowel
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
 # ================= HOUSE (33..48): hazy keys, organs, donks, pumped pads
 # ---- keys and chords (the chord instrument is on: one key, one chord)
 ("Haze Keys", 122, dict(MODE=5, DET=20, MIX=36, COL=46, CUT=112, RES=10, FENV=10, KEY=40, AA=0, AD=84, AS=40, AR=62, FA=0, FD=66, FS=14, FR=50, CHOR=70, DLY=34, DTIM=2, DFB=44, REV=58, RSZ=84, TAPE=58, CON=1, CSCL=4, CVOI=5, SSWG=40)),
 ("Velvet Keys", 120, dict(MODE=5, DET=12, MIX=14, COL=28, CUT=96, RES=12, FENV=16, AA=0, AD=90, AS=56, AR=70, FA=0, FD=78, FS=20, CHOR=84, DLY=22, DTIM=3, DFB=40, REV=70, RSZ=92, TAPE=40, CON=1, CSCL=1, CVOI=4)),
 ("Bell Tines", 124, dict(MODE=5, DET=30, MIX=100, COL=40, CUT=118, RES=8, FENV=8, AA=0, AD=74, AS=20, AR=66, FA=0, FD=54, FS=6, CHOR=60, DLY=66, DTIM=2, DFB=58, REV=76, RSZ=96, TAPE=24)),
 ("Lo-Fi Stab", 124, dict(MODE=0, DET=30, MIX=70, CUT=64, RES=30, FTYPE=1, FENV=62, AA=0, AD=50, AS=10, AR=40, FA=0, FD=48, FS=8, CHOR=64, DLY=40, DTIM=2, DFB=50, REV=52, RSZ=78, TAPE=84, CRU=22, DRV=22, CON=1, CSCL=1, CVOI=7, SSWG=46)),
 ("Filter House", 126, dict(MODE=0, DET=44, MIX=84, CUT=50, RES=48, FTYPE=1, FENV=20, AA=2, AD=70, AS=104, AR=44, LRATE=10, LAMT=90, LDST=1, LSHP=1, GPAT=14, GRATE=1, GDEP=96, GSMO=70, CHOR=72, DLY=24, DTIM=2, REV=40, RSZ=70, DRV=10, CON=1, CSCL=4, CVOI=4)),
 ("Pump Organ", 124, dict(MODE=6, DET=40, MIX=50, COL=64, CUT=104, RES=10, FENV=10, AA=1, AD=70, AS=108, AR=30, FA=0, FD=30, FS=0, GPAT=14, GRATE=1, GDEP=110, GSMO=84, CHOR=90, DLY=18, DTIM=2, REV=46, RSZ=72, TAPE=20, CON=1, CSCL=1, CVOI=4)),
 ("Sidechain Pad", 122, dict(MODE=2, DET=48, MIX=70, COL=44, CUT=70, RES=22, FENV=30, AA=40, AD=84, AS=112, AR=90, GPAT=14, GRATE=1, GDEP=120, GSMO=100, LRATE=24, LAMT=50, LDST=3, CHOR=110, DLY=30, DTIM=3, DFB=44, REV=96, RSZ=104, TAPE=36, CON=1, CSCL=4, CVOI=5)),
 ("Tape Pad", 118, dict(MODE=0, DET=36, MIX=64, CUT=58, RES=20, FTYPE=1, FENV=26, AA=56, AD=90, AS=112, AR=104, CHOR=104, DLY=36, DTIM=3, DFB=46, REV=104, RSZ=110, TAPE=110, CRU=14, CON=1, CSCL=1, CVOI=6)),
 # ---- organs and basses
 ("Garage Organ", 128, dict(MODE=6, DET=10, MIX=90, SUB=30, COL=20, OCT=1, CUT=92, RES=14, FENV=24, AA=0, AD=40, AS=70, AR=22, FA=0, FD=26, FS=0, VMODE=1, CHOR=20, DLY=10, REV=16, RSZ=44, DRV=26, SSWG=58)),
 ("Deep Organ", 122, dict(MODE=6, DET=24, MIX=40, COL=44, OCT=2, CUT=84, RES=16, FENV=30, AA=0, AD=52, AS=60, AR=34, FA=0, FD=34, FS=4, CHOR=56, DLY=26, DTIM=2, DFB=44, REV=44, RSZ=70, TAPE=30)),
 ("Donk Bass", 130, dict(MODE=7, DET=6, MIX=70, SUB=20, COL=20, OCT=1, CUT=100, RES=10, FENV=20, AA=0, AD=40, AS=40, AR=24, FA=0, FD=34, FS=0, VMODE=1, CHOR=0, DLY=0, REV=12, RSZ=36, DRV=30, SSWG=52)),
 ("Rubber Donk", 128, dict(MODE=7, DET=14, MIX=96, COL=40, OCT=2, CUT=106, RES=10, FENV=14, AA=0, AD=44, AS=10, AR=30, FA=0, FD=40, FS=0, CHOR=30, DLY=40, DTIM=2, DFB=48, REV=30, RSZ=56, DRV=18, SSWG=52)),
 ("Sub Roller", 124, dict(MODE=4, DET=4, MIX=20, SUB=96, COL=10, OCT=1, CUT=46, RES=18, FENV=22, AA=1, AD=60, AS=104, AR=30, GLIDE=24, VMODE=2, CHOR=0, DLY=0, REV=8, RSZ=30, DRV=20)),
 # ---- leads and plucks
 ("Night Bus", 120, dict(MODE=4, DET=26, MIX=50, COL=34, CUT=78, RES=24, FTYPE=1, FENV=30, AA=10, AD=70, AS=96, AR=70, GLIDE=30, VMODE=2, LRATE=46, LAMT=20, CHOR=80, DLY=60, DTIM=2, DFB=60, REV=84, RSZ=96, TAPE=70)),
 ("Vowel Talk", 126, dict(MODE=3, DET=30, MIX=60, SUB=20, COL=60, OCT=2, CUT=40, RES=80, FTYPE=4, FENV=90, AA=0, AD=44, AS=60, AR=30, FA=0, FD=56, FS=10, VMODE=1, GLIDE=14, CHOR=40, DLY=30, DTIM=2, DFB=46, REV=30, RSZ=60, DRV=70, SSWG=50)),
 ("Swung Pluck", 124, dict(MODE=2, DET=30, MIX=60, COL=56, CUT=78, RES=34, FENV=74, AA=0, AD=42, AS=8, AR=36, FA=0, FD=40, FS=4, AON=1, APAT=4, ARATE=2, ARNG=2, AGATE=50, CHOR=70, DLY=62, DTIM=2, DFB=60, REV=64, RSZ=84, TAPE=30, SSWG=60)),
 # ================= TECHNO (49..64): acid, metal, drones, noise
 ("Acid Line", 134, dict(MODE=3, DET=0, MIX=20, SUB=0, COL=30, OCT=1, CUT=44, RES=104, FTYPE=5, FENV=84, KEY=20, AA=0, AD=46, AS=30, AR=20, FA=0, FD=40, FS=0, FR=20, VMODE=2, GLIDE=22, CHOR=0, DLY=30, DTIM=2, DFB=46, REV=14, RSZ=40, DRV=44)),
 ("Acid Square", 136, dict(MODE=2, DET=0, MIX=0, COL=64, OCT=1, CUT=40, RES=110, FTYPE=5, FENV=96, AA=0, AD=40, AS=40, AR=18, FA=0, FD=34, FS=0, VMODE=2, GLIDE=26, CHOR=0, DLY=36, DTIM=4, DFB=50, REV=12, RSZ=40, DRV=56)),
 ("Hoover Acid", 138, dict(MODE=1, DET=40, MIX=60, COL=40, OCT=2, CUT=52, RES=90, FTYPE=5, FENV=70, AA=0, AD=60, AS=70, AR=30, FA=2, FD=56, FS=10, VMODE=2, GLIDE=34, CHOR=40, DLY=30, DTIM=2, REV=30, RSZ=60, DRV=50)),
 ("Warehouse Stab", 132, dict(MODE=0, DET=50, MIX=90, CUT=70, RES=30, FTYPE=1, FENV=60, AA=0, AD=40, AS=0, AR=34, FA=0, FD=36, FS=0, CHOR=40, DLY=44, DTIM=2, DFB=56, REV=60, RSZ=84, DRV=30)),
 ("Metal Stab", 134, dict(MODE=8, DET=20, MIX=50, COL=36, CUT=96, RES=20, FENV=30, AA=0, AD=44, AS=0, AR=40, FA=0, FD=38, FS=0, CHOR=20, DLY=50, DTIM=2, DFB=60, REV=50, RSZ=76, DRV=20)),
 ("Bell Factory", 130, dict(MODE=8, DET=40, MIX=90, COL=84, CUT=110, RES=10, FENV=10, AA=0, AD=70, AS=10, AR=64, FA=0, FD=60, FS=6, CHOR=30, DLY=60, DTIM=2, DFB=62, REV=80, RSZ=100)),
 ("Rumble", 132, dict(MODE=10, DET=30, MIX=110, SUB=60, OCT=1, CUT=30, RES=30, FENV=20, AA=4, AD=70, AS=110, AR=50, VMODE=1, GPAT=14, GRATE=1, GDEP=110, GSMO=60, CHOR=0, DLY=20, DTIM=2, DFB=40, REV=70, RSZ=110, DRV=50)),
 ("Drone Hall", 128, dict(MODE=4, DET=40, MIX=80, COL=60, OCT=1, CUT=50, RES=36, FTYPE=1, FENV=10, AA=80, AD=90, AS=120, AR=110, LRATE=6, LAMT=70, LDST=1, CHOR=60, DLY=40, DTIM=3, DFB=60, REV=110, RSZ=120, DRV=0)),
 ("Hiss Sweep", 132, dict(MODE=9, MIX=0, COL=100, OCT=3, CUT=30, RES=70, FTYPE=2, FENV=0, AA=60, AD=80, AS=120, AR=80, LRATE=4, LAMT=120, LDST=1, LSHP=1, CHOR=40, DLY=50, DTIM=3, DFB=60, REV=80, RSZ=100, DRV=40)),
 ("Noise Pulse", 136, dict(MODE=9, MIX=30, COL=40, OCT=2, CUT=80, RES=60, FTYPE=2, FENV=50, AA=0, AD=30, AS=0, AR=20, FA=0, FD=30, FS=0, AON=1, APAT=0, ARATE=2, ARNG=1, AGATE=30, CHOR=0, DLY=40, DTIM=4, DFB=50, REV=40, RSZ=70)),
 ("Sync Lead", 138, dict(MODE=1, DET=20, MIX=70, COL=90, CUT=90, RES=40, FENV=50, AA=0, AD=60, AS=90, AR=40, VMODE=2, GLIDE=20, LRATE=40, LAMT=60, LDST=3, CHOR=30, DLY=50, DTIM=2, DFB=56, REV=40, RSZ=70, DRV=40)),
 ("Dub Chord", 126, dict(MODE=0, DET=30, MIX=60, CUT=56, RES=40, FTYPE=2, FENV=50, AA=0, AD=46, AS=0, AR=40, FA=0, FD=44, FS=0, CHOR=50, DLY=96, DTIM=5, DFB=90, REV=70, RSZ=100, CON=1, CSCL=1, CVOI=4, TAPE=30)),
 ("Dub Stab Dark", 124, dict(MODE=2, DET=26, MIX=40, COL=50, CUT=44, RES=50, FTYPE=1, FENV=64, AA=0, AD=50, AS=0, AR=44, FA=0, FD=50, FS=0, CHOR=40, DLY=100, DTIM=5, DFB=96, REV=84, RSZ=110, CON=1, CSCL=1, CVOI=6, TAPE=44)),
 ("Pulse Bass", 134, dict(MODE=2, DET=4, MIX=0, SUB=50, COL=30, OCT=1, CUT=56, RES=40, FENV=50, AA=0, AD=36, AS=50, AR=16, FA=0, FD=30, FS=0, VMODE=1, CHOR=0, DLY=0, REV=10, RSZ=30, DRV=40)),
 ("Siren", 136, dict(MODE=4, DET=10, MIX=30, COL=50, OCT=3, CUT=96, RES=30, FENV=0, AA=10, AD=60, AS=110, AR=50, VMODE=2, GLIDE=60, LRATE=30, LAMT=100, LDST=0, LSHP=1, CHOR=30, DLY=70, DTIM=2, DFB=70, REV=60, RSZ=90)),
 ("Machine Arp", 138, dict(MODE=8, DET=10, MIX=20, COL=20, CUT=84, RES=44, FTYPE=1, FENV=60, AA=0, AD=34, AS=0, AR=24, FA=0, FD=30, FS=0, AON=1, APAT=5, ARATE=2, ARNG=2, AGATE=40, CHOR=20, DLY=56, DTIM=4, DFB=60, REV=40, RSZ=70)),
 # ================= GARAGE (65..80): reese and organ basses, bell and organ stabs, shuffled
 ("Reese Bass", 132, dict(MODE=10, DET=60, MIX=60, SUB=30, OCT=1, CUT=50, RES=30, FTYPE=1, FENV=30, AA=2, AD=60, AS=110, AR=30, VMODE=2, GLIDE=24, LRATE=8, LAMT=40, LDST=1, CHOR=0, DLY=0, REV=12, RSZ=40, DRV=40, SSWG=70)),
 ("Warp Reese", 134, dict(MODE=10, DET=96, MIX=40, SUB=40, OCT=1, CUT=40, RES=60, FTYPE=2, FENV=40, AA=0, AD=56, AS=100, AR=30, VMODE=2, GLIDE=40, LRATE=20, LAMT=90, LDST=1, LSHP=1, CHOR=30, DLY=10, REV=20, RSZ=50, DRV=56, SSWG=70)),
 ("Organ Bass", 132, dict(MODE=6, DET=6, MIX=100, SUB=40, COL=10, OCT=1, CUT=90, RES=10, FENV=20, AA=0, AD=36, AS=50, AR=18, FA=0, FD=24, FS=0, VMODE=1, CHOR=10, DLY=0, REV=14, RSZ=40, DRV=30, SSWG=74)),
 ("Sub Wobble", 134, dict(MODE=4, DET=6, MIX=20, SUB=110, COL=10, OCT=1, CUT=40, RES=40, FENV=10, AA=1, AD=60, AS=110, AR=30, VMODE=2, GLIDE=20, LRATE=44, LAMT=96, LDST=1, CHOR=0, DLY=0, REV=8, RSZ=30, DRV=30, SSWG=70)),
 ("2-Step Organ", 132, dict(MODE=6, DET=30, MIX=70, COL=70, CUT=104, RES=10, FENV=10, AA=0, AD=40, AS=0, AR=30, FA=0, FD=30, FS=0, CHOR=70, DLY=40, DTIM=5, DFB=46, REV=40, RSZ=64, CON=1, CSCL=1, CVOI=4, SSWG=74)),
 ("Bell Stab", 132, dict(MODE=5, DET=30, MIX=110, COL=60, CUT=116, RES=8, FENV=8, AA=0, AD=50, AS=0, AR=44, FA=0, FD=40, FS=0, CHOR=50, DLY=50, DTIM=5, DFB=50, REV=50, RSZ=72, CON=1, CSCL=1, CVOI=5, SSWG=74)),
 ("Garage Piano", 130, dict(MODE=5, DET=14, MIX=30, COL=70, CUT=110, RES=10, FENV=14, AA=0, AD=64, AS=20, AR=50, FA=0, FD=50, FS=8, CHOR=40, DLY=24, DTIM=5, REV=44, RSZ=70, CON=1, CSCL=1, CVOI=4, SSWG=72)),
 ("Skip Pluck", 134, dict(MODE=2, DET=20, MIX=50, COL=60, CUT=84, RES=30, FENV=60, AA=0, AD=36, AS=0, AR=30, FA=0, FD=34, FS=0, AON=1, APAT=4, ARATE=2, ARNG=2, AGATE=40, CHOR=50, DLY=60, DTIM=5, DFB=56, REV=50, RSZ=76, SSWG=76)),
 ("Vox Chop", 132, dict(MODE=3, DET=20, MIX=50, COL=50, OCT=3, CUT=60, RES=84, FTYPE=4, FENV=70, AA=0, AD=40, AS=40, AR=26, FA=0, FD=46, FS=10, VMODE=1, GLIDE=10, CHOR=40, DLY=44, DTIM=5, DFB=50, REV=40, RSZ=66, DRV=100, SSWG=74)),
 ("Vox Pad", 130, dict(MODE=0, DET=40, MIX=70, CUT=70, RES=70, FTYPE=4, FENV=20, AA=40, AD=80, AS=110, AR=80, LRATE=14, LAMT=70, LDST=1, CHOR=90, DLY=30, DTIM=5, REV=84, RSZ=100, CON=1, CSCL=1, CVOI=6)),
 ("Speed Stab", 138, dict(MODE=0, DET=40, MIX=84, CUT=74, RES=30, FTYPE=1, FENV=56, AA=0, AD=38, AS=0, AR=30, FA=0, FD=34, FS=0, CHOR=50, DLY=44, DTIM=5, DFB=50, REV=44, RSZ=70, CON=1, CSCL=1, CVOI=7, DRV=24, SSWG=70)),
 ("Bassline Donk", 138, dict(MODE=7, DET=8, MIX=90, SUB=20, COL=36, OCT=1, CUT=104, RES=10, FENV=20, AA=0, AD=36, AS=30, AR=20, FA=0, FD=30, FS=0, VMODE=1, CHOR=0, DLY=0, REV=12, RSZ=36, DRV=40, SSWG=60)),
 ("Night Keys", 128, dict(MODE=5, DET=20, MIX=20, COL=36, CUT=100, RES=10, FENV=12, AA=0, AD=84, AS=50, AR=66, FA=0, FD=70, FS=16, CHOR=80, DLY=36, DTIM=5, DFB=44, REV=70, RSZ=90, TAPE=30, CON=1, CSCL=4, CVOI=5, SSWG=70)),
 ("String Lift", 132, dict(MODE=0, DET=50, MIX=80, CUT=84, RES=16, FENV=20, AA=30, AD=80, AS=110, AR=70, CHOR=100, DLY=20, DTIM=5, REV=80, RSZ=96, CON=1, CSCL=1, CVOI=5)),
 ("Flute Lead", 132, dict(MODE=6, DET=10, MIX=0, COL=0, OCT=3, CUT=100, RES=10, FENV=0, AA=8, AD=60, AS=110, AR=40, VMODE=2, GLIDE=20, LRATE=50, LAMT=24, LDST=0, CHOR=40, DLY=50, DTIM=5, DFB=50, REV=60, RSZ=84, DRV=30, SSWG=70)),
 ("Dark Roller", 136, dict(MODE=10, DET=40, MIX=90, SUB=70, OCT=1, CUT=36, RES=44, FTYPE=1, FENV=40, AA=0, AD=50, AS=96, AR=24, VMODE=2, GLIDE=16, CHOR=0, DLY=0, REV=10, RSZ=36, DRV=60, SSWG=70)),
]

# 1.2: eight more a world, added after the first eighty so that a saved slot's patch number still means the same patch.
# (world, name, tempo, what differs)
NEW = [
 # ---- trance: the pumped wall, melodic plucks, the hard end
 (0, "Pump Supersaw", 140, dict(MODE=0, DET=80, MIX=104, CUT=102, RES=18, FENV=24, AA=2, AD=70, AS=112, AR=58, GPAT=14, GRATE=1, GDEP=112, GSMO=84, CHOR=96, DLY=34, DTIM=2, DFB=48, REV=74, RSZ=90, CON=1, CSCL=1, CVOI=1)),
 (0, "Melodic Pluck", 126, dict(MODE=2, DET=28, MIX=50, COL=62, CUT=70, RES=36, FTYPE=1, FENV=78, AA=0, AD=48, AS=6, AR=50, FA=0, FD=46, FS=4, FR=50, CHOR=60, DLY=84, DTIM=2, DFB=70, REV=92, RSZ=104, LRATE=18, LAMT=30, LDST=1)),
 (0, "Psy Bass", 145, dict(MODE=3, DET=6, MIX=30, SUB=40, COL=30, OCT=1, CUT=52, RES=44, FTYPE=1, FENV=74, KEY=10, AA=0, AD=30, AS=0, AR=14, FA=0, FD=28, FS=0, FR=14, VMODE=1, CHOR=0, DLY=0, REV=6, RSZ=24, DRV=36, SGATE=50)),
 (0, "Hard Lead", 150, dict(MODE=1, DET=70, MIX=80, COL=104, CUT=100, RES=56, FENV=40, AA=0, AD=60, AS=104, AR=40, GLIDE=22, VMODE=2, LRATE=66, LAMT=30, LDST=3, CHOR=50, DLY=44, DTIM=2, DFB=52, REV=52, RSZ=76, DRV=84)),
 (0, "Rave Piano", 138, dict(MODE=5, DET=16, MIX=44, COL=96, CUT=120, RES=8, FENV=8, KEY=50, AA=0, AD=62, AS=24, AR=46, FA=0, FD=50, FS=10, CHOR=44, DLY=30, DTIM=2, DFB=44, REV=56, RSZ=80, DRV=20, CON=1, CSCL=1, CVOI=0)),
 (0, "Hard Acid", 145, dict(MODE=3, DET=0, MIX=10, SUB=0, COL=20, OCT=1, CUT=48, RES=112, FTYPE=5, FENV=104, KEY=30, AA=0, AD=40, AS=30, AR=18, FA=0, FD=36, FS=0, FR=18, VMODE=2, GLIDE=20, CHOR=0, DLY=40, DTIM=2, DFB=56, REV=26, RSZ=56, DRV=86)),
 (0, "Dream Bells", 132, dict(MODE=5, DET=34, MIX=112, COL=50, CUT=118, RES=8, FENV=6, AA=0, AD=80, AS=16, AR=76, FA=0, FD=60, FS=6, CHOR=76, DLY=78, DTIM=2, DFB=66, REV=100, RSZ=110, AON=1, APAT=2, ARATE=2, ARNG=2, AGATE=80)),
 (0, "Neo Lead", 140, dict(MODE=0, DET=50, MIX=84, SUB=20, CUT=92, RES=36, FTYPE=1, FENV=50, AA=1, AD=64, AS=100, AR=48, GLIDE=30, VMODE=2, GPAT=15, GRATE=1, GDEP=60, GSMO=70, LRATE=56, LAMT=16, CHOR=70, DLY=56, DTIM=2, DFB=58, REV=66, RSZ=86, DRV=30)),
 # ---- house: tech and bass house, piano, afro, disco
 (1, "Tech Bass", 126, dict(MODE=4, DET=4, MIX=36, SUB=70, COL=30, OCT=1, CUT=58, RES=30, FTYPE=1, FENV=60, AA=0, AD=36, AS=0, AR=20, FA=0, FD=30, FS=0, FR=20, VMODE=1, GLIDE=8, CHOR=0, DLY=0, REV=8, RSZ=30, DRV=34, SSWG=30)),
 (1, "Piano House", 124, dict(MODE=5, DET=14, MIX=36, COL=84, CUT=118, RES=8, FENV=8, KEY=50, AA=0, AD=76, AS=30, AR=54, FA=0, FD=60, FS=12, CHOR=36, DLY=22, DTIM=2, DFB=40, REV=52, RSZ=78, TAPE=12, CON=1, CSCL=1, CVOI=4)),
 (1, "Afro Pluck", 122, dict(MODE=5, DET=10, MIX=70, COL=24, CUT=100, RES=14, FENV=30, AA=0, AD=40, AS=0, AR=36, FA=0, FD=36, FS=0, CHOR=40, DLY=72, DTIM=5, DFB=62, REV=70, RSZ=92, SSWG=20)),
 (1, "Growl Bass", 128, dict(MODE=10, DET=70, MIX=80, SUB=40, OCT=1, CUT=44, RES=76, FTYPE=4, FENV=70, AA=0, AD=46, AS=70, AR=22, FA=0, FD=44, FS=10, VMODE=2, GLIDE=14, LRATE=74, LAMT=60, LDST=1, LSHP=1, CHOR=10, DLY=0, REV=12, RSZ=36, DRV=96)),
 (1, "Disco Strings", 124, dict(MODE=0, DET=46, MIX=76, CUT=96, RES=14, FENV=20, AA=24, AD=76, AS=112, AR=62, FA=20, FD=70, FS=80, CHOR=112, DLY=18, DTIM=2, REV=70, RSZ=90, TAPE=16, CON=1, CSCL=1, CVOI=1)),
 (1, "Melodic Arp", 123, dict(MODE=2, DET=30, MIX=56, COL=58, CUT=76, RES=34, FTYPE=1, FENV=70, AA=0, AD=46, AS=10, AR=44, FA=0, FD=44, FS=6, AON=1, APAT=4, ARATE=2, ARNG=2, AGATE=60, GPAT=14, GRATE=1, GDEP=50, GSMO=80, CHOR=70, DLY=74, DTIM=2, DFB=64, REV=80, RSZ=100)),
 (1, "Soul Chords", 120, dict(MODE=5, DET=18, MIX=20, COL=30, CUT=92, RES=12, FENV=14, AA=0, AD=92, AS=60, AR=72, FA=0, FD=80, FS=24, CHOR=96, DLY=20, DTIM=3, DFB=40, REV=64, RSZ=90, TAPE=50, LRATE=60, LAMT=14, LDST=2, CON=1, CSCL=4, CVOI=5)),
 (1, "Classic Stab", 126, dict(MODE=6, DET=30, MIX=80, COL=80, CUT=106, RES=10, FENV=20, AA=0, AD=44, AS=0, AR=34, FA=0, FD=36, FS=0, CHOR=64, DLY=46, DTIM=2, DFB=52, REV=50, RSZ=76, DRV=20, CON=1, CSCL=1, CVOI=4)),
 # ---- techno: hard and hypnotic
 (2, "Screech", 150, dict(MODE=1, DET=60, MIX=60, COL=118, OCT=2, CUT=84, RES=80, FTYPE=2, FENV=50, AA=0, AD=56, AS=96, AR=30, VMODE=2, GLIDE=16, LRATE=80, LAMT=40, LDST=3, LSHP=1, CHOR=20, DLY=30, DTIM=2, DFB=46, REV=50, RSZ=84, DRV=112, CRU=16)),
 (2, "Hypno Stab", 134, dict(MODE=8, DET=14, MIX=30, COL=50, CUT=76, RES=40, FTYPE=1, FENV=56, AA=0, AD=36, AS=0, AR=34, FA=0, FD=32, FS=0, CHOR=20, DLY=96, DTIM=5, DFB=92, REV=60, RSZ=96)),
 (2, "Peak Lead", 136, dict(MODE=0, DET=36, MIX=70, SUB=20, CUT=70, RES=50, FTYPE=1, FENV=64, AA=0, AD=56, AS=70, AR=44, FA=0, FD=52, FS=20, GLIDE=24, VMODE=2, LRATE=30, LAMT=40, LDST=1, CHOR=40, DLY=70, DTIM=2, DFB=66, REV=84, RSZ=106, DRV=30)),
 (2, "Off Bass", 140, dict(MODE=4, DET=4, MIX=24, SUB=104, COL=14, OCT=1, CUT=44, RES=20, FENV=20, AA=1, AD=60, AS=110, AR=20, VMODE=1, GPAT=0, GRATE=1, GDEP=127, GSMO=30, CHOR=0, DLY=0, REV=30, RSZ=80, DRV=40)),
 (2, "Acid 16ths", 140, dict(MODE=0, DET=0, MIX=0, SUB=0, OCT=1, CUT=50, RES=106, FTYPE=5, FENV=76, KEY=30, AA=0, AD=30, AS=0, AR=14, FA=0, FD=30, FS=0, FR=14, VMODE=1, CHOR=0, DLY=50, DTIM=5, DFB=60, REV=30, RSZ=60, DRV=60, SGATE=50)),
 (2, "Detroit Chord", 128, dict(MODE=2, DET=40, MIX=70, COL=50, CUT=74, RES=24, FTYPE=1, FENV=44, AA=4, AD=70, AS=70, AR=60, FA=10, FD=70, FS=30, LRATE=12, LAMT=50, LDST=1, CHOR=90, DLY=60, DTIM=5, DFB=60, REV=76, RSZ=100, CON=1, CSCL=1, CVOI=7)),
 (2, "Minimal Blip", 132, dict(MODE=5, DET=6, MIX=96, COL=70, CUT=110, RES=20, FENV=10, AA=0, AD=26, AS=0, AR=20, FA=0, FD=24, FS=0, LRATE=90, LAMT=80, LDST=3, LSHP=3, AON=1, APAT=3, ARATE=2, ARNG=2, AGATE=30, CHOR=10, DLY=60, DTIM=5, DFB=56, REV=36, RSZ=64)),
 (2, "Iron Drone", 145, dict(MODE=8, DET=60, MIX=70, COL=70, OCT=1, CUT=56, RES=24, FTYPE=2, FENV=10, AA=40, AD=90, AS=84, AR=90, LRATE=8, LAMT=100, LDST=1, LSHP=1, CHOR=20, DLY=40, DTIM=3, DFB=60, REV=84, RSZ=118, DRV=24, CRU=24)),
 # ---- garage: the new school and the speed end
 (3, "Wub Bass", 134, dict(MODE=5, DET=4, MIX=96, COL=20, OCT=1, CUT=70, RES=30, FTYPE=1, FENV=60, AA=0, AD=50, AS=60, AR=22, FA=0, FD=44, FS=10, VMODE=2, GLIDE=18, CHOR=0, DLY=0, REV=10, RSZ=36, DRV=50, SSWG=70)),
 (3, "Sine Sub", 132, dict(MODE=4, DET=0, MIX=0, SUB=127, COL=0, OCT=1, CUT=36, RES=10, FENV=10, AA=1, AD=64, AS=112, AR=30, VMODE=2, GLIDE=30, CHOR=0, DLY=0, REV=4, RSZ=24, DRV=24, SSWG=70)),
 (3, "Glass Pluck", 132, dict(MODE=8, DET=20, MIX=100, COL=90, CUT=112, RES=10, FENV=10, AA=0, AD=44, AS=0, AR=44, FA=0, FD=40, FS=0, CHOR=50, DLY=66, DTIM=5, DFB=58, REV=64, RSZ=88, SSWG=74)),
 (3, "Dawn Pad", 130, dict(MODE=2, DET=44, MIX=64, COL=40, CUT=64, RES=20, FTYPE=1, FENV=30, AA=50, AD=86, AS=112, AR=96, LRATE=16, LAMT=44, LDST=3, CHOR=110, DLY=30, DTIM=5, DFB=44, REV=100, RSZ=108, TAPE=60, CON=1, CSCL=4, CVOI=5)),
 (3, "Niche Lead", 138, dict(MODE=2, DET=6, MIX=0, COL=64, OCT=2, CUT=96, RES=30, FENV=30, AA=0, AD=50, AS=90, AR=26, VMODE=2, GLIDE=26, LRATE=58, LAMT=18, CHOR=30, DLY=44, DTIM=5, DFB=50, REV=36, RSZ=60, DRV=50, SSWG=60)),
 (3, "Brass Stab", 132, dict(MODE=0, DET=30, MIX=70, CUT=60, RES=24, FTYPE=1, FENV=70, AA=2, AD=50, AS=30, AR=36, FA=14, FD=50, FS=30, CHOR=50, DLY=36, DTIM=5, DFB=46, REV=46, RSZ=70, DRV=24, CON=1, CSCL=1, CVOI=4, SSWG=72)),
 (3, "Stretch Vox", 134, dict(MODE=3, DET=24, MIX=50, COL=56, OCT=3, CUT=56, RES=90, FTYPE=4, FENV=60, AA=0, AD=50, AS=80, AR=30, FA=0, FD=60, FS=20, VMODE=2, GLIDE=30, GLP=70, GLL=1, GLT=0, CHOR=50, DLY=40, DTIM=5, DFB=50, REV=50, RSZ=76, DRV=90, SSWG=74)),
 (3, "Soul Organ", 130, dict(MODE=6, DET=30, MIX=40, COL=50, CUT=100, RES=10, FENV=10, AA=2, AD=70, AS=104, AR=40, FA=0, FD=40, FS=0, LRATE=64, LAMT=12, LDST=2, CHOR=104, DLY=20, DTIM=5, REV=50, RSZ=76, TAPE=20, CON=1, CSCL=4, CVOI=5, SSWG=72)),
]
# 2.1: two more worlds, 24 patches each, added after everything before them (a saved slot's patch number keeps its meaning).
PAD = dict(AA=70, AD=90, AS=112, AR=104, FA=60, FD=90, FS=70, FR=100, REV=110, RSZ=118, CHOR=96, DLY=40, DTIM=3, DFB=56)
def pad(**k):
    d = dict(PAD); d.update(k); return d
BASS = dict(OCT=1, VMODE=2, GLIDE=14, AA=0, AR=20, CHOR=0, DLY=0, REV=6, RSZ=24, SGATE=80)
def bass(**k):
    d = dict(BASS); d.update(k); return d
NEW2 = [
 # ---- ambient: slow, wide, drifting
 (4, "Slow Dawn", 84, pad(MODE=2, DET=44, MIX=64, COL=40, CUT=60, RES=18, FTYPE=1, FENV=30, LRATE=10, LAMT=40, LDST=3, TAPE=40, DRIFT=70, CON=1, CSCL=0, CVOI=5)),
 (4, "Glass Halo", 76, pad(MODE=5, DET=30, MIX=104, COL=44, CUT=112, RES=8, FENV=6, AA=30, AD=100, AS=60, AR=110, DLY=70, DFB=66, DRIFT=50)),
 (4, "Deep Drone", 70, pad(MODE=10, DET=40, MIX=60, SUB=60, OCT=1, CUT=40, RES=30, FTYPE=1, FENV=10, AA=90, AR=116, LRATE=6, LAMT=60, LDST=1, CHOR=70, DRIFT=90, DRV=24)),
 (4, "Tape Choir", 80, pad(MODE=0, DET=50, MIX=70, CUT=66, RES=60, FTYPE=4, FENV=20, LRATE=8, LAMT=44, LDST=1, TAPE=70, DRIFT=60, CON=1, CSCL=1, CVOI=1)),
 (4, "Still Water", 72, pad(MODE=5, DET=14, MIX=24, COL=30, CUT=96, RES=10, FENV=12, AA=2, AD=96, AS=40, AR=100, DLY=84, DTIM=5, DFB=70, TAPE=30, DRIFT=40)),
 (4, "Aurora", 88, pad(MODE=0, DET=70, MIX=90, CUT=58, RES=30, FTYPE=1, FENV=20, LRATE=5, LAMT=70, LDST=1, CHOR=110, DRIFT=60, CON=1, CSCL=4, CVOI=5)),
 (4, "Dust Organ", 78, pad(MODE=6, DET=30, MIX=44, COL=40, CUT=84, RES=10, FENV=10, AA=40, LRATE=58, LAMT=10, LDST=2, TAPE=90, DRIFT=50, CON=1, CSCL=0, CVOI=5)),
 (4, "Cloud Keys", 82, pad(MODE=5, DET=20, MIX=40, COL=50, CUT=88, RES=12, FENV=20, AA=6, AD=90, AS=50, AR=96, FA=0, DLY=60, DFB=60, TAPE=44, DRIFT=44, CON=1, CSCL=0, CVOI=4)),
 (4, "Sine Sea", 70, pad(MODE=4, DET=24, MIX=30, SUB=40, COL=20, CUT=70, RES=10, FENV=10, AA=96, AR=120, LRATE=4, LAMT=50, LDST=2, DRIFT=100)),
 (4, "Metal Rain", 90, pad(MODE=8, DET=20, MIX=100, COL=96, CUT=112, RES=10, FENV=10, AA=0, AD=50, AS=0, AR=70, FA=0, FD=40, FS=0, DLY=90, DTIM=5, DFB=76, AON=1, APAT=3, ARATE=1, ARNG=3, AGATE=40, DRIFT=30)),
 (4, "Night Wind", 70, pad(MODE=9, MIX=80, COL=60, CUT=56, RES=76, FTYPE=2, FENV=10, AA=100, AR=120, LRATE=6, LAMT=110, LDST=1, LSHP=3, DRIFT=127, DRV=44)),
 (4, "Far Bell", 74, pad(MODE=5, DET=10, MIX=118, COL=70, CUT=120, RES=6, FENV=0, AA=0, AD=110, AS=0, AR=118, FA=0, DLY=76, DFB=70, DRIFT=30)),
 (4, "Warm Bed", 80, pad(MODE=0, DET=40, MIX=60, SUB=30, CUT=44, RES=14, FTYPE=1, FENV=16, TAPE=50, DRIFT=64, CON=1, CSCL=1, CVOI=1)),
 (4, "Shimmer", 86, pad(MODE=1, DET=60, MIX=70, COL=90, OCT=3, CUT=96, RES=20, FENV=10, AA=96, LRATE=14, LAMT=30, LDST=3, CHOR=120, DLY=70, DFB=70, DRIFT=50)),
 (4, "Sub Tide", 70, pad(MODE=4, DET=6, MIX=10, SUB=120, COL=0, OCT=1, CUT=34, RES=10, FENV=10, AA=80, AR=110, VMODE=1, CHOR=20, REV=60, DRIFT=40)),
 (4, "Lullaby", 76, pad(MODE=5, DET=12, MIX=60, COL=20, CUT=100, RES=8, FENV=10, AA=0, AD=84, AS=20, AR=90, FA=0, DLY=64, DTIM=5, DFB=60, TAPE=36, DRIFT=30, CON=1, CSCL=0, CVOI=0)),
 (4, "Frozen", 72, pad(MODE=2, DET=20, MIX=30, COL=84, CUT=80, RES=40, FTYPE=2, FENV=10, AA=110, AR=124, LRATE=3, LAMT=80, LDST=3, DRIFT=80, CRU=10, DRV=40)),
 (4, "Hollow", 78, pad(MODE=3, DET=40, MIX=50, COL=50, OCT=2, CUT=60, RES=70, FTYPE=2, FENV=20, LRATE=7, LAMT=60, LDST=1, DRIFT=70, DRV=44, CON=1, CSCL=4, CVOI=6)),
 (4, "Drift Strings", 84, pad(MODE=0, DET=56, MIX=80, CUT=84, RES=14, FENV=20, AA=50, AR=90, FA=40, CHOR=112, DRIFT=96, TAPE=20, CON=1, CSCL=1, CVOI=1)),
 (4, "Pulse Fog", 92, pad(MODE=2, DET=44, MIX=64, COL=30, CUT=56, RES=24, FTYPE=1, FENV=24, GPAT=14, GRATE=0, GDEP=70, GSMO=110, DRIFT=50, CON=1, CSCL=1, CVOI=5)),
 (4, "Moth", 96, pad(MODE=2, DET=20, MIX=40, COL=64, CUT=74, RES=30, FTYPE=1, FENV=70, AA=0, AD=46, AS=0, AR=60, FA=0, FD=44, FS=0, DLY=96, DTIM=5, DFB=80, AON=1, APAT=4, ARATE=1, ARNG=2, AGATE=40, DRIFT=40)),
 (4, "Cathedral", 70, pad(MODE=6, DET=20, MIX=90, COL=90, CUT=96, RES=8, FENV=10, AA=30, AR=110, REV=127, RSZ=127, DRIFT=30, CON=1, CSCL=1, CVOI=3)),
 (4, "Low Sun", 74, pad(MODE=0, DET=90, MIX=100, SUB=20, CUT=50, RES=20, FTYPE=1, FENV=20, TAPE=110, DRIFT=80, LRATE=4, LAMT=40, LDST=1)),
 (4, "Ghost Tones", 80, pad(MODE=4, DET=40, MIX=70, COL=70, CUT=72, RES=80, FTYPE=4, FENV=20, AA=90, LRATE=5, LAMT=90, LDST=1, LSHP=1, DRIFT=127, DLY=70, DFB=70)),
 # ---- bass: loud, distorted, sliding
 (5, "Dirt 808", 150, bass(MODE=4, DET=0, MIX=20, SUB=120, COL=10, CUT=50, RES=14, FENV=40, AD=96, AS=40, AR=40, FA=0, FD=40, FS=0, GLIDE=40, DRV=96)),
 (5, "Victim", 160, bass(MODE=0, DET=30, MIX=60, SUB=60, CUT=56, RES=70, FTYPE=5, FENV=80, AD=50, AS=70, FA=0, FD=40, FS=10, DRV=110, CRU=24)),
 (5, "Donk Smash", 150, bass(MODE=7, DET=10, MIX=90, COL=70, OCT=2, CUT=90, RES=20, FENV=30, AD=40, AS=0, AR=24, VMODE=1, DRV=100, SGATE=50)),
 (5, "Hyper Reese", 160, bass(MODE=10, DET=90, MIX=90, SUB=50, CUT=60, RES=40, FTYPE=1, FENV=40, AD=60, AS=100, CHOR=60, DRV=56, DRIFT=30)),
 (5, "Blown Sub", 140, bass(MODE=4, DET=0, MIX=0, SUB=127, COL=0, CUT=40, RES=10, FENV=20, AD=80, AS=90, AR=30, GLIDE=30, DRV=127, CRU=30)),
 (5, "Growler", 150, bass(MODE=10, DET=70, MIX=80, SUB=40, CUT=46, RES=84, FTYPE=4, FENV=60, AD=60, LRATE=80, LAMT=70, LDST=1, LSHP=1, DRV=110, AS=110)),
 (5, "Wobble", 140, bass(MODE=0, DET=20, MIX=40, SUB=70, CUT=40, RES=60, FTYPE=1, FENV=10, AD=60, AS=110, LRATE=76, LAMT=110, LDST=1, DRV=70)),
 (5, "Screech Bass", 165, bass(MODE=1, DET=50, MIX=60, COL=110, SUB=40, CUT=70, RES=90, FTYPE=2, FENV=50, AD=50, AS=90, LRATE=84, LAMT=40, LDST=3, LSHP=1, DRV=110)),
 (5, "FM Punch", 155, bass(MODE=5, DET=4, MIX=110, COL=30, CUT=84, RES=20, FTYPE=1, FENV=70, AD=40, AS=20, FA=0, FD=30, FS=0, VMODE=1, DRV=70, SGATE=50)),
 (5, "Rubber", 145, bass(MODE=3, DET=10, MIX=30, SUB=50, COL=40, CUT=56, RES=50, FTYPE=1, FENV=70, AD=44, AS=40, FA=0, FD=40, FS=0, GLIDE=36, DRV=50)),
 (5, "Crushed Sine", 160, bass(MODE=4, DET=0, MIX=10, SUB=110, COL=10, CUT=70, RES=10, FENV=10, AD=70, AS=90, GLIDE=24, DRV=60, CRU=70)),
 (5, "Laser Bass", 170, bass(MODE=3, DET=0, MIX=10, COL=20, CUT=40, RES=116, FTYPE=5, FENV=120, AD=36, AS=20, FA=0, FD=26, FS=0, VMODE=1, DRV=90, SGATE=50)),
 (5, "Metal Sub", 150, bass(MODE=8, DET=30, MIX=50, COL=40, SUB=110, CUT=64, RES=30, FTYPE=1, FENV=40, AD=50, AS=60, DRV=84)),
 (5, "Square Push", 150, bass(MODE=2, DET=4, MIX=0, COL=64, SUB=60, CUT=70, RES=30, FENV=30, AD=50, AS=100, GLIDE=20, DRV=70)),
 (5, "Hoover Low", 160, bass(MODE=0, DET=112, MIX=110, SUB=40, CUT=76, RES=30, FENV=30, AA=2, AD=60, AS=100, GLIDE=44, CHOR=80, DRV=80, REV=20)),
 (5, "Pump Bass", 150, bass(MODE=0, DET=40, MIX=70, SUB=80, CUT=56, RES=30, FTYPE=1, FENV=20, AA=1, AD=60, AS=112, AR=30, GPAT=14, GRATE=1, GDEP=120, GSMO=60, DRV=70)),
 (5, "Bit Donk", 165, bass(MODE=7, DET=20, MIX=110, COL=100, OCT=2, CUT=100, RES=30, FENV=20, AD=36, AS=0, AR=20, VMODE=1, DRV=80, CRU=60, SGATE=50)),
 (5, "Noise Bass", 150, bass(MODE=9, MIX=60, COL=30, SUB=110, CUT=50, RES=70, FTYPE=2, FENV=60, AD=44, AS=50, FA=0, FD=40, FS=10, DRV=90)),
 (5, "Acid Smash", 155, bass(MODE=0, DET=0, MIX=0, SUB=30, CUT=44, RES=112, FTYPE=5, FENV=90, KEY=30, AD=40, AS=30, FA=0, FD=36, FS=0, GLIDE=20, DRV=120)),
 (5, "Sub Glide", 140, bass(MODE=4, DET=0, MIX=0, SUB=127, COL=0, CUT=36, RES=10, FENV=10, AA=1, AD=70, AS=112, AR=40, GLIDE=70, DRV=30)),
 (5, "Fuzz Organ", 150, bass(MODE=6, DET=20, MIX=30, COL=20, CUT=80, RES=14, FENV=20, AD=40, AS=40, VMODE=1, DRV=110, SGATE=60)),
 (5, "Tear Out", 170, bass(MODE=10, DET=110, MIX=100, SUB=30, CUT=60, RES=70, FTYPE=2, FENV=40, AD=60, AS=100, LRATE=96, LAMT=90, LDST=1, LSHP=2, DRV=110, CRU=30)),
 (5, "Chip Bass", 165, bass(MODE=2, DET=0, MIX=0, COL=20, SUB=40, OCT=2, CUT=110, RES=10, FENV=10, AD=40, AS=80, VMODE=1, DRV=40, CRU=84, SGATE=60)),
 (5, "Wall", 160, bass(MODE=0, DET=100, MIX=96, SUB=70, CUT=84, RES=24, FENV=30, AA=1, AD=70, AS=110, AR=30, CHOR=90, DRV=50, REV=24)),
]
WORLD0 = [0, 32, 48, 64, 80]                      # the first eighty: where each world starts

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
 # house
 "Haze Keys": "0 - - 0 - - 3 - - - 0 - - 5 - -", "Velvet Keys": "0 - - - - - 0 - - 7 - - - - 5 -",
 "Bell Tines": "12 - 19 - 15 - - 14 - - 12 - 10 - 7 -", "Lo-Fi Stab": "- - 0a - - 0 - - 0a - - - - 0 - 3",
 "Filter House": "0 - - 0 - - 0 - - - 0 - - 0 - 0", "Pump Organ": "0s 0s 0s 0 - - 0 - 0s 0s 0 - - 0 - -",
 "Sidechain Pad": SEQ_PAD, "Tape Pad": SEQ_PAD,
 "Garage Organ": "0a - - 0 - 12 - 0 - - 0a - 10 - 12 -", "Deep Organ": "0 - 7 - - 0 - 3 - 0 - 7 - - 5 -",
 "Donk Bass": "0a - 12 0 - 0 7 - 0a - 12 0 - 10 - 12", "Rubber Donk": "0 12 - 7 - 12 0 - 3 15 - 10 - 15 3 -",
 "Sub Roller": "0 - - 0s 3 - 0 - - - 0 - -2s 0 - -", "Night Bus": "12 - - - 10 - - 7 - - - 5 - 7 - -",
 "Vowel Talk": "0a 0 - 12 - 0 3s 5 - 0a - 10 0 - 7 -", "Swung Pluck": "12 - 15 12 - 19 - 15 12 - 14 10 - 17 - 14",
 # techno
 "Acid Line": "0a 0 12s 0 0 3a 0s 12 0 0a 10s 12 0 0 3 15a", "Acid Square": "0 0a 0 12s 10 0 0a 3 0 12a 0s 0 7 0 10s 12a",
 "Hoover Acid": "0a - 0 12s 0 - 3a - 0 0s 12 - 0a 10 - 7s", "Warehouse Stab": "- - 0a - - - 0 - - - 0a - - 0 - -",
 "Metal Stab": "0 - - 0 - - 0a - - 0 - - 3 - 0a -", "Bell Factory": "12 - - 19 - - 15 - - 12 - 22 - - 19 -",
 "Rumble": "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0", "Drone Hall": SEQ_PAD, "Hiss Sweep": SEQ_PAD,
 "Noise Pulse": "- - 0 - - - 0 - - - 0 - - - 0 0", "Sync Lead": "12 - 12s 15 - 12 - 10s 12 - - 19a - 15 - 10",
 "Dub Chord": "- - 0 - - - - - - - 0a - - - - -", "Dub Stab Dark": "- - - 0 - - - - - - - - 0a - - -",
 "Pulse Bass": "0 - 0 0 - 0 - 0 0 - 0 0 - 0 3 -", "Siren": "12s 12s 12s 24 - - - - 12s 12s 12s 19 - - - -",
 "Machine Arp": "0 12 0 15 0 12 0 19 0 12 0 15 0 22 0 19",
 # garage
 "Reese Bass": "0 - - 0s 3 - - 0 - - 10s 12 - - 0 -", "Warp Reese": "0s 0 - - 0a - 7s 0 - - 0s 3 - 0a - -",
 "Organ Bass": "0a - - 0 - 12 - - 0 - 10a - 12 - 0 -", "Sub Wobble": "0 - - - 0s 0 - - 3 - - - 0s -2 - -",
 "2-Step Organ": "- - 0 - - 0a - - - - 0 - 3 - - -", "Bell Stab": "0 - - 0 - - - 3a - - 0 - - - 5 -",
 "Garage Piano": "0 - - 0a - - 0 - - 3 - - 0a - 5 -", "Skip Pluck": "12 - 15 - - 12 19 - 12 - - 15 - 22 19 -",
 "Vox Chop": "12a - - 12 - 15s 12 - - 10a - 12 - - 7 -", "Vox Pad": SEQ_PAD,
 "Speed Stab": "0 - - 0 - - 0a - - 0 - - 0 - 3a -", "Bassline Donk": "0a - 12 0 - - 0 7 - 0a 12 - 0 - 10 12",
 "Night Keys": "0 - - - - - 0 - - 5 - - - - 3 -", "String Lift": SEQ_PAD,
 "Flute Lead": "12 - - 15 - 12 - - 10 - 12 - - 7 - -", "Dark Roller": "0 - 0s 0 - - 3 0 - 0s 0 - -2a - 0 -",
 # 1.2
 "Pump Supersaw": SEQ_PAD, "Melodic Pluck": "12 - 15 - 19 - - 12 - 15 - 22 - 19 - 15",
 "Psy Bass": "- 0 0 0 - 0 0 0 - 0 0 0 - 0 0 0", "Hard Lead": "12a - 12 15s 12 - 19a - 12 - 10s 12 - 15a - 7",
 "Rave Piano": "0a - - 0 - - 0a - - 0 - 0a - - 5 -", "Hard Acid": "0a 0 12s 0 0a 3 0s 15a 0 0 12a 0s 10 0a 3 12s",
 "Dream Bells": "12 - - 19 - - 15 - 22 - - 19 - 15 - -", "Neo Lead": "12 - - 15s 14 - 12 - 10s 12 - - 7 - 10s 12",
 "Tech Bass": "0a - - 0 - 0 - 3s 0 - - 0a - 10 - 0", "Piano House": "0a - - 0 - - 0a - - - 5 - - 5a - -",
 "Afro Pluck": "12 - 15 12 - 19 - 12 15 - 12 - 17 - 15 -", "Growl Bass": "0a - 0s 0 - - 12a - 0 - 0s 3 - 0a - -",
 "Disco Strings": SEQ_PAD, "Melodic Arp": "12 - 19 - 15 - 12 - 22 - 19 - 15 - 19 -",
 "Soul Chords": "0 - - - - - 0 - - - 3 - - - - -", "Classic Stab": "- - 0a - - 0 - - 0a - - - 5 - 0a -",
 "Screech": "12a - - 12s 15 - - 12a - - 13s 12 - - 19a -", "Hypno Stab": "- - 0 - - - - 0a - - - - 0 - - -",
 "Peak Lead": "12 - - - 15s 14 - - 12 - - 10s 12 - 7 -", "Off Bass": SEQ_PAD,
 "Acid 16ths": "0a 0 0 12 0a 0 3 0 0a 0 12a 0 0 10 0a 15", "Detroit Chord": "- - 0 - - - - - 0a - - - - - 5 -",
 "Minimal Blip": "12 - - 12 - 19 - - 12 - - - 15 - 12 -", "Iron Drone": SEQ_PAD,
 "Wub Bass": "0a - - 0s 12 - - 0 - - 0a - 3s 0 - -", "Sine Sub": "0 - - - - - 0s 3 - - 0 - - -2s 0 -",
 "Glass Pluck": "12 - - 15 - 12 - - 19 - 15 - - 12 - 10", "Dawn Pad": SEQ_PAD,
 "Niche Lead": "12a - 12 - 15s 12 - 10a - 12 - - 7s 10 - 12", "Brass Stab": "0a - - 0 - - - 3a - - 0 - - 0a - -",
 "Stretch Vox": "12a - - 12 - 15s 12 - - - 10a - 12s 7 - -", "Soul Organ": "0 - - - - 0 - - - - 3 - - - 5 -",

 # 2.1
 "Dirt 808": "0a - - 0 - 0s 12 - 0a - 0 - 3s 0 - -",
 "Victim": "0a 0 - 0 12a - 0 0s 10 - 0a 0 - 15s 12 -",
 "Donk Smash": "0a - 0 - - 0a - 0 - 0s 7 - 0a - 3 -",
 "Hyper Reese": "0 - - - 0s 3 - - 0 - - - -2s 0 - 5s",
 "Blown Sub": "0a - - 0 - 0s 12 - 0a - 0 - 3s 0 - -",
 "Growler": "0a 0 - 0 12a - 0 0s 10 - 0a 0 - 15s 12 -",
 "Wobble": "0a - 0 - - 0a - 0 - 0s 7 - 0a - 3 -",
 "Screech Bass": "0 - - - 0s 3 - - 0 - - - -2s 0 - 5s",
 "FM Punch": "0a - - 0 - 0s 12 - 0a - 0 - 3s 0 - -",
 "Rubber": "0a 0 - 0 12a - 0 0s 10 - 0a 0 - 15s 12 -",
 "Crushed Sine": "0a - 0 - - 0a - 0 - 0s 7 - 0a - 3 -",
 "Laser Bass": "0 - - - 0s 3 - - 0 - - - -2s 0 - 5s",
 "Metal Sub": "0a - - 0 - 0s 12 - 0a - 0 - 3s 0 - -",
 "Square Push": "0a 0 - 0 12a - 0 0s 10 - 0a 0 - 15s 12 -",
 "Hoover Low": "0a - 0 - - 0a - 0 - 0s 7 - 0a - 3 -",
 "Pump Bass": "0 - - - 0s 3 - - 0 - - - -2s 0 - 5s",
 "Bit Donk": "0a - - 0 - 0s 12 - 0a - 0 - 3s 0 - -",
 "Noise Bass": "0a 0 - 0 12a - 0 0s 10 - 0a 0 - 15s 12 -",
 "Acid Smash": "0a - 0 - - 0a - 0 - 0s 7 - 0a - 3 -",
 "Sub Glide": "0 - - - 0s 3 - - 0 - - - -2s 0 - 5s",
 "Fuzz Organ": "0a - - 0 - 0s 12 - 0a - 0 - 3s 0 - -",
 "Tear Out": "0a 0 - 0 12a - 0 0s 10 - 0a 0 - 15s 12 -",
 "Chip Bass": "0a - 0 - - 0a - 0 - 0s 7 - 0a - 3 -",
 "Wall": "0 - - - 0s 3 - - 0 - - - -2s 0 - 5s",
 "Slow Dawn": SEQ_PAD,
 "Glass Halo": "12 - - - - - 19 - - - - - 15 - - -",
 "Deep Drone": SEQ_PAD,
 "Tape Choir": SEQ_PAD,
 "Still Water": "12 - - - 15 - - - - - 19 - - - 14 -",
 "Aurora": SEQ_PAD,
 "Dust Organ": SEQ_PAD,
 "Cloud Keys": SEQ_PAD,
 "Sine Sea": SEQ_PAD,
 "Metal Rain": "12 - 19 - 15 - 22 - 12 - 17 - 24 - 19 -",
 "Night Wind": SEQ_PAD,
 "Far Bell": "12 - - - - - - - 19 - - - - - - -",
 "Warm Bed": SEQ_PAD,
 "Shimmer": SEQ_PAD,
 "Sub Tide": SEQ_PAD,
 "Lullaby": SEQ_PAD,
 "Frozen": SEQ_PAD,
 "Hollow": SEQ_PAD,
 "Drift Strings": SEQ_PAD,
 "Pulse Fog": SEQ_PAD,
 "Moth": "12 - - 19 - - 15 - - 22 - - 19 - - -",
 "Cathedral": SEQ_PAD,
 "Low Sun": SEQ_PAD,
 "Ghost Tones": SEQ_PAD,
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

assert len(P) == 80, len(P)
world = []
for i in range(80):
    world.append(max(w for w in range(4) if i >= WORLD0[w]))
for w, name, bpm, d in NEW + NEW2:
    P.append((name, bpm, d))
    world.append(w)
order = [i for w in range(6) for i in range(len(P)) if world[i] == w]     # the list as the PRESETS knob walks it: a world at a time
w0 = [sum(1 for x in world if x < w) for w in range(7)]
assert len(set(n for n, _, _ in P)) == len(P)
out = ["/* SPDX-License-Identifier: GPL-3.0-only */", "/* generated by tools/gen_presets.py: do not edit */",
       "/* A patch's number is its place in GS_PRE and never changes (a saved slot holds it). The order they are shown in is GS_ORD: a world",
       " * at a time; GS_POS is a patch's place in that order, GS_PWORLD its world, GS_W0 where each world starts in GS_ORD. */",
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
pos = [order.index(i) for i in range(len(P))]
out.append("static const unsigned char GS_PWORLD[GS_NPRE] = {%s};" % ", ".join(map(str, world)))
out.append("static const unsigned char GS_ORD[GS_NPRE] = {%s};" % ", ".join(map(str, order)))
out.append("static const unsigned char GS_POS[GS_NPRE] = {%s};" % ", ".join(map(str, pos)))
out.append("static const unsigned char GS_W0[GS_NWORLD + 1] = {%s};" % ", ".join(map(str, w0)))
assert len(P) == 160
print("\n".join(out))
