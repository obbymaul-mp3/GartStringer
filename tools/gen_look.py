#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The look (2.0, "metalheart"): the four worlds' palettes and their chrome emblems, written to firmware/src/gs_art.h.

    python3 tools/gen_look.py > firmware/src/gs_art.h          (needs numpy, scipy and Pillow; the header is committed)

A palette is 46 colours in the order of sk_fb.c's enum. An emblem is drawn here as a shape with a rounded edge, lit as chrome
(what faces up shows a pale sky, what faces down a darker ground, with a dark horizon between), and stored as a picture of
CHROME LEVELS, 0 (the brightest sky) to 15 (the darkest ground), not of colours: the firmware turns a level into the world's
own chrome through FB_CHR, so one picture serves every world. Each picture: width, height, then runs of (count, value);
value 16 is nothing (see-through), 17 the dark rim, 18 ink (letters cut into a plate)."""
import os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from scipy import ndimage as ndi

R = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
N = 240; S = 4; H = N * S
yy, xx = np.mgrid[0:H, 0:H].astype(np.float32)
X = (xx + 0.5) / S; Y = (yy + 0.5) / S
BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], dtype=np.float32) / 16

def hx(h): return np.array([int(h[i:i + 2], 16) for i in (1, 3, 5)], dtype=np.float32)
def lerp(a, b, t): return a + (b - a) * t

# ---------------------------------------------------------------------------------------------- the palettes
#        name      accent     accent light  chrome: low      mid        sky        backdrop    ink
WORLDS = [
    ('TRANCE', '#FF3C96', '#FF96D2', '#22163A', '#8E78C8', '#EAD9FF', '#3A0C44', '#F4E2FF'),
    ('HOUSE',  '#FF7A1C', '#FFC480', '#3C2010', '#C68C58', '#FFEBD2', '#40200A', '#FFF0DE'),
    ('TECHNO', '#96FF28', '#CEFF96', '#161E1A', '#7C948A', '#E8F2EC', '#0E3418', '#E8F8EC'),
    ('GARAGE', '#3CC8FF', '#A4E8FF', '#12203E', '#6C90CA', '#DCEEFF', '#0C2458', '#E4F2FF'),
]
def palette(w):
    _, acc, accl, low, mid, sky, back, ink = w
    acc, accl, low, mid, sky, back, ink = map(hx, (acc, accl, low, mid, sky, back, ink))
    k = np.zeros(3, np.float32); wh = np.full(3, 255, np.float32)
    p = [k,                                                   # C_BG
         lerp(k, back, .16), lerp(k, back, .34), lerp(k, back, .62),                       # K1..K3: the panels
         acc * .14, acc * .24, acc * .38, acc * .56, acc * .76, acc, accl, lerp(accl, wh, .6),   # O0..O7: the accent
         low, lerp(low, mid, .4), lerp(low, mid, .75), mid, lerp(mid, sky, .4), lerp(mid, sky, .75), sky,   # S0..S6: the chrome
         hx('#380A14'), hx('#701024'), hx('#A81834'), hx('#E02848'), hx('#FF5870'),         # R0..R4
         wh,                                                  # C_HOT
         back * .22, back * .42, back * .64, back * .86, lerp(back, acc, .12), lerp(back, acc, .22),   # B0..B5: the backdrop's glow
         ink, lerp(ink, mid, .55),                            # INK, INK2
         lerp(k, acc, .20), accl, lerp(k, acc, .40), acc,     # GRID0, LED, GRID1, GRID2
         lerp(low, mid, .55), lerp(mid, sky, .2), lerp(k, back, .2),      # DIM, DIM2, PLATE
         hx('#30E060'), hx('#A0E030'), hx('#F0E030'), hx('#FFB020'), hx('#FF6A18'), hx('#FF2030')]     # the battery
    assert len(p) == 46
    return [tuple(int(round(float(v))) for v in np.clip(c, 0, 255)) for c in p]

# ---------------------------------------------------------------------------------------------- chrome
def chrome(mask, bevel, tilt=0.65, pos=0.4):
    """a shape (hi-res bool) to chrome levels 0..15 at screen size, 17 on its rim, 16 outside it"""
    d = ndi.distance_transform_edt(mask) / S
    u = np.clip(d / bevel, 0, 1); h = np.sqrt(1 - (1 - u) ** 2) * bevel
    h = ndi.gaussian_filter(h, S * 0.6)
    gy, gx = np.gradient(h); gy *= S; gx *= S
    n = np.sqrt(gx * gx + gy * gy + 1); ny = -gy / n; nx = -gx / n
    ys = np.where(mask.any(1))[0]; cy = (ys.min() + ys.max()) / 2 / S; hh = max(1, (ys.max() - ys.min()) / S)
    t = 0.40 + tilt * ny + pos * (Y - cy) / hh
    spec = (-ny * 0.8 - nx * 0.6) > 0.80
    def dn(a): return a.reshape(N, S, N, S).mean(axis=(1, 3))
    a = dn(mask.astype(np.float32)); w = np.maximum(a, 1e-3)
    t = dn(t * mask) / w; dd = dn(d * mask) / w; sp = dn(spec * mask) / w
    v = np.interp(np.clip(t, 0, 1), [0, 0.22, 0.46, 0.495, 0.52, 0.60, 0.82, 1], [0, 2, 6, 7, 8, 9, 12, 15])     # (the sky is long and pale, the horizon thin)
    b = np.tile(BAYER, (N // 4, N // 4))
    lv = np.where(v < 6.5, np.minimum(np.floor(v + b), 6), np.where(v < 7.5, 7, np.maximum(np.floor(v + b), 8)))
    lv = np.clip(lv, 0, 15).astype(np.uint8)
    lv[sp > 0.5] = 0
    lv[(dd < 0.75) & (a >= 0.5)] = 17
    lv[a < 0.5] = 16
    return lv
def over(dst, src):
    return np.where(src != 16, src, dst)
def star(cx, cy, rx, ry): return (np.sqrt(np.abs(X - cx) / rx) + np.sqrt(np.abs(Y - cy) / ry)) <= 1
def circ(cx, cy, r): return np.hypot(X - cx, Y - cy) <= r
EMPTY = np.full((N, N), 16, np.uint8)

def m_star(k, cx=120, cy=120):                    # trance: a four-point star between two blades
    a = EMPTY
    for s in (-1, 1):
        x = (X - cx) * s; y = Y - cy
        m = (((x - 30 * k) / (62 * k)) ** 2 + ((y + 6 * k) / (70 * k)) ** 2 <= 1) & ~(((x - 12 * k) / (60 * k)) ** 2 + ((y + 22 * k) / (74 * k)) ** 2 <= 1) & (x > 8 * k)
        a = over(a, chrome(m, max(2, 7 * k), 0.6, 0.45))
    st = star(cx, cy, 34 * k, 78 * k) | star(cx, cy, 70 * k, 20 * k) | circ(cx, cy, 9 * k)
    return over(a, chrome(st, max(2.2, 7 * k), 0.7, 0.3))
def m_heart(k, cx=120, cy=120):                   # house: a heart with a ring through it
    ang = 0.38; xr = (X - cx) * np.cos(ang) + (Y - cy - 6 * k) * np.sin(ang); yr = -(X - cx) * np.sin(ang) + (Y - cy - 6 * k) * np.cos(ang)
    ring = ((xr / (104 * k)) ** 2 + (yr / (24 * k)) ** 2 <= 1) & ((xr / (97 * k - 1)) ** 2 + (yr / (17.5 * k - 1)) ** 2 >= 1)
    a = over(EMPTY, chrome(ring & (yr < 0), max(1.4, 3.2 * k), 0.6, 0.2))
    x = (X - cx) / (50 * k); y = -(Y - cy) / (50 * k) + 0.15
    a = over(a, chrome((x * x + y * y - 1) ** 3 - x * x * y ** 3 <= 0, max(5, 17 * k), 0.62, 0.42))
    return over(a, chrome(ring & (yr >= 0), max(1.4, 3.2 * k), 0.6, 0.2))
def m_planet(k, cx=120, cy=120):                  # techno: a sphere in a gyroscope
    r = np.hypot(X - cx, Y - cy)
    a = over(EMPTY, chrome((r <= 70 * k) & (r >= 70 * k - max(2.2, 5 * k)), max(1.2, 2.6 * k), 0.6, 0.45))
    ang = -0.5; xr = (X - cx) * np.cos(ang) + (Y - cy) * np.sin(ang); yr = -(X - cx) * np.sin(ang) + (Y - cy) * np.cos(ang)
    ring = ((xr / (84 * k)) ** 2 + (yr / (24 * k)) ** 2 <= 1) & ((xr / (78 * k - 1)) ** 2 + (yr / (17 * k - 1)) ** 2 >= 1)
    a = over(a, chrome(ring & (yr < 0), max(1.3, 3 * k), 0.6, 0.2))
    a = over(a, chrome(r <= 42 * k, 42 * k, 0.5, 0.0))
    return over(a, chrome(ring & (yr >= 0), max(1.3, 3 * k), 0.6, 0.2))
def m_moon(k, cx=120, cy=120):                    # garage: a crescent moon and a star
    moon = (np.hypot(X - cx + 8 * k, Y - cy) <= 62 * k) & ~(np.hypot(X - cx - 18 * k, Y - cy + 12 * k) <= 52 * k)
    a = over(EMPTY, chrome(moon, max(3, 12 * k), 0.62, 0.4))
    st = star(cx + 24 * k, cy - 10 * k, 17 * k, 34 * k) | star(cx + 24 * k, cy - 10 * k, 34 * k, 10 * k) | circ(cx + 24 * k, cy - 10 * k, 4.5 * k)
    a = over(a, chrome(st, max(1.8, 4 * k), 0.7, 0.3))
    return over(a, chrome(star(cx + 56 * k, cy + 34 * k, 7 * k, 14 * k) | star(cx + 56 * k, cy + 34 * k, 14 * k, 4.5 * k), max(1.2, 2 * k), 0.7, 0.3))
def plate():                                      # the name, cut into a bar of chrome
    px = np.abs(X - 120); py = np.abs(Y - 120)
    a = chrome((px <= 98) & (py <= 14) & ((px + py) <= 104), 5, 0.7, 0.3)
    f = ImageFont.truetype(os.path.join(R, 'assets/fonts/Sora[wght].ttf'), 17 * S)
    try: f.set_variation_by_axes([800])
    except Exception: pass
    im = Image.new('L', (H * 2, H), 0); d = ImageDraw.Draw(im); x = 40
    for ch in 'GARTSTRINGER':
        d.text((x, H // 2), ch, font=f, fill=255, anchor='ls'); x += d.textlength(ch, font=f) + 0.3 * S
    im = im.crop(im.getbbox()); w, h = im.size; sh = -0.18; pad = int(abs(sh) * h) + 2
    big = Image.new('L', (w + pad, h), 0); big.paste(im, (0, 0))
    im = big.transform(big.size, Image.AFFINE, (1, sh, 0, 0, 1, 0), Image.BICUBIC)
    out = Image.new('L', (H, H), 0); out.paste(im, (int(120 * S - im.size[0] / 2), int(120 * S - im.size[1] / 2)))
    t = (np.asarray(out) > 127).reshape(N, S, N, S).mean(axis=(1, 3)) >= 0.5
    a[t & (a != 16)] = 18
    return a

def crop(a):
    ys = np.where((a != 16).any(1))[0]; xs = np.where((a != 16).any(0))[0]
    return a[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
def rle(a):
    h, w = a.shape; out = [w, h]; flat = a.reshape(-1); i = 0
    while i < len(flat):
        j = i
        while j < len(flat) and flat[j] == flat[i] and j - i < 255: j += 1
        out += [j - i, int(flat[i])]; i = j
    return out

ART = [('STAR', m_star), ('HEART', m_heart), ('PLANET', m_planet), ('MOON', m_moon)]
def build():
    pics = []
    for name, fn in ART:
        pics.append(('ART_%s' % name, crop(fn(0.86))))
        pics.append(('ART_%s_S' % name, crop(fn(0.36))))
    pics.append(('ART_PLATE', crop(plate())))
    return pics

def preview(pics, path):
    """the pictures in each world's chrome, to look at"""
    CHR = [24, 18, 18, 17, 17, 16, 15, 1, 24, 18, 17, 16, 15, 14, 13, 12]
    sheet = Image.new('RGB', (4 * 240, 240 + 90 + 40), (0, 0, 0))
    for wi, w in enumerate(WORLDS):
        pal = palette(w)
        def paint(a, ox, oy):
            hh, ww = a.shape
            for y in range(hh):
                for x in range(ww):
                    v = a[y, x]
                    if v == 16: continue
                    sheet.putpixel((ox + x, oy + y), pal[CHR[v]] if v < 16 else pal[1] if v == 17 else pal[1])
        big = pics[wi * 2][1]; sm = pics[wi * 2 + 1][1]; pl = pics[8][1]
        paint(big, wi * 240 + 120 - big.shape[1] // 2, 110 - big.shape[0] // 2)
        paint(sm, wi * 240 + 120 - sm.shape[1] // 2, 280 - sm.shape[0] // 2)
        paint(pl, wi * 240 + 120 - pl.shape[1] // 2, 335)
    sheet.resize((sheet.size[0] * 2, sheet.size[1] * 2), Image.NEAREST).save(path)

if __name__ == '__main__':
    pics = build()
    if len(sys.argv) > 1:
        preview(pics, sys.argv[1])
    o = ['/* SPDX-License-Identifier: GPL-3.0-only */', '/* generated by tools/gen_look.py: do not edit */',
         '/* The four worlds\' palettes (three rows a world, as sk_fb.c counts its looks; the three are the same colours), and the chrome',
         ' * emblems: width, height, then runs of (count, value); 0..15 a chrome level (FB_CHR), 16 nothing, 17 the rim, 18 ink. */',
         '#define FB_RGB(r, g, b) (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))',
         'static const uint32_t SK_THEME[FB_NTHEME][46] = {']
    for w in WORLDS:
        pal = palette(w)
        for k in range(3):
            o.append('    {   /* %s */' % w[0].lower())
            for i in range(0, 46, 6):
                o.append('        ' + ' '.join('FB_RGB(%d, %d, %d),' % c for c in pal[i:i + 6]))
            o.append('    },')
    o.append('};')
    total = 0
    for name, a in pics:
        d = rle(a); total += len(d)
        o.append('static const uint8_t %s[%d] = {' % (name, len(d)))
        for i in range(0, len(d), 32):
            o.append('    ' + ', '.join(map(str, d[i:i + 32])) + ',')
        o.append('};')
    o.append('static const uint8_t *const ART_WORLD[4] = {ART_STAR, ART_HEART, ART_PLANET, ART_MOON};')
    o.append('static const uint8_t *const ART_WORLD_S[4] = {ART_STAR_S, ART_HEART_S, ART_PLANET_S, ART_MOON_S};')
    print('\n'.join(o))
    sys.stderr.write('art: %d bytes; %s\n' % (total, ', '.join('%s %dx%d' % (n, a.shape[1], a.shape[0]) for n, a in pics)))
