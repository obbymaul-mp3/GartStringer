#!/usr/bin/env python3
"""The site's two pictures in the instrument's look: the name in its gold lettering (docs/images/title.png) and the castle at dusk
(docs/images/scene.png).   python3 tools/fantasy/site_art.py"""
import os, sys
from PIL import Image
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import lettering, scenes
OUT = os.path.join(HERE, '..', '..', 'docs', 'images')
def hx(s): return tuple(int(s[i:i + 2], 16) for i in (1, 3, 5))
STOPS = [(0, hx('#fff6d0')), (.26, hx('#ffe08a')), (.48, hx('#f2c14e')), (.52, hx('#b9732a')), (.70, hx('#f6cf6a')), (1, hx('#8a4a1c'))]
def gold(r):
    t = min(1, r / 10)
    for (a, ca), (b, cb) in zip(STOPS, STOPS[1:]):
        if a <= t <= b:
            f = (t - a) / (b - a); return tuple(int(ca[i] + (cb[i] - ca[i]) * f) for i in range(3)) + (255,)
    return STOPS[-1][1] + (255,)
s = 'Gartstringer'; w = lettering.width(s) + 4; im = Image.new('RGBA', (w, 18), (0, 0, 0, 0)); px = im.load()
INK = (26, 11, 51, 255)
for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1), (-1, -1), (1, -1), (-1, 1), (1, 1), (1, 2), (0, 2)):
    lettering.draw(lambda x, y, r: px.__setitem__((x, y), INK), 2 + dx, 1 + dy, s)
lettering.draw(lambda x, y, r: px.__setitem__((x, y), gold(r)), 2, 1, s)
im.resize((w * 5, 18 * 5), Image.NEAREST).save(os.path.join(OUT, 'title.png'))
scenes.castle().resize((960, 960), Image.NEAREST).save(os.path.join(OUT, 'scene.png'))
print(w * 5, 18 * 5)
