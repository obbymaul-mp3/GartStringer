#!/usr/bin/env python3
"""Generate GartStringer's pixel font as a C header (1-bit glyphs, no anti-aliasing).

    python3 tools/gen_pixfont.py OUT.h

Two faces:
  UI     Departure Mono (OFL, assets/fonts) rendered at 11 px without anti-aliasing: the screens' text. The screens
         draw it at 1x and at 2x (the big chord name), so the text keeps the plant's chunky pixels.
  MICRO  a hand-drawn 3x5 face (5x5 for M, m, W, w) for the key caps: note names, degrees, chord labels. Bytes
         0x10..0x19 are small raised digits (3x4) for added degrees, 0xB0 the diminished ring, 0xF8 the
         half-diminished sign; '#' and 'b' draw as compact sharp and flat signs.

Each glyph: an advance and rows of bits (bit 7 = the leftmost pixel). No Raqm, no kerning: identical on every machine.
"""
import pathlib
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = pathlib.Path(__file__).resolve().parent.parent
UI_FONT = ROOT / "assets" / "fonts" / "DepartureMono-Regular.otf"      # (the arrows and the sharp/flat fallbacks)
SORA = ROOT / "assets" / "fonts" / "Sora[wght].ttf"                     # GartStringer's face
UI_PX = 11

# extra code points of the UI face, stored at these byte values
UI_EXTRA = {0xB0: "\u00b0", 0xF8: "\u00f8", 0x82: "\u2192", 0x83: "\u2190", 0x84: "\u2191", 0x85: "\u2193",
            0x86: "\u00b7"}
# glyphs the font does not have, drawn here in its style: the sharp and the flat (rows 0..13, cap height 3..10)
UI_DRAWN = {
    0x80: ["", "", "", "..#.#..", "..#.#..", ".#####.", "..#.#..", "..#.#..", ".#####.", "..#.#..", "..#.#..", "", "", ""],
    0x81: ["", "", ".#.....", ".#.....", ".#.....", ".#.##..", ".##..#.", ".#...#.", ".#..#..", ".#.#...", ".##....", "", "", ""],
}

MICRO = {
    " ": ["...", "...", "...", "...", "..."],
    "0": ["###", "#.#", "#.#", "#.#", "###"],
    "1": [".#.", "##.", ".#.", ".#.", "###"],
    "2": ["##.", "..#", ".#.", "#..", "###"],
    "3": ["##.", "..#", ".#.", "..#", "##."],
    "4": ["#.#", "#.#", "###", "..#", "..#"],
    "5": ["###", "#..", "##.", "..#", "##."],
    "6": [".##", "#..", "###", "#.#", "###"],
    "7": ["###", "..#", ".#.", ".#.", ".#."],
    "8": ["###", "#.#", "###", "#.#", "###"],
    "9": ["###", "#.#", "###", "..#", "##."],
    "A": [".#.", "#.#", "###", "#.#", "#.#"],
    "B": ["##.", "#.#", "##.", "#.#", "##."],
    "C": [".##", "#..", "#..", "#..", ".##"],
    "D": ["##.", "#.#", "#.#", "#.#", "##."],
    "E": ["###", "#..", "##.", "#..", "###"],
    "F": ["###", "#..", "##.", "#..", "#.."],
    "G": [".##", "#..", "#.#", "#.#", ".##"],
    "I": ["###", ".#.", ".#.", ".#.", "###"],
    "V": ["#.#", "#.#", "#.#", "#.#", ".#."],
    "X": ["#.#", "#.#", ".#.", "#.#", "#.#"],
    "O": ["###", "#.#", "#.#", "#.#", "###"],
    "N": ["#.#", "###", "###", "#.#", "#.#"],
    "P": ["##.", "#.#", "##.", "#..", "#.."],
    "R": ["##.", "#.#", "##.", "#.#", "#.#"],
    "S": [".##", "#..", ".#.", "..#", "##."],
    "T": ["###", ".#.", ".#.", ".#.", ".#."],
    "L": ["#..", "#..", "#..", "#..", "###"],
    "H": ["#.#", "#.#", "###", "#.#", "#.#"],
    "K": ["#.#", "#.#", "##.", "#.#", "#.#"],
    "U": ["#.#", "#.#", "#.#", "#.#", "###"],
    "Y": ["#.#", "#.#", ".#.", ".#.", ".#."],
    "Z": ["###", "..#", ".#.", "#..", "###"],
    "J": ["..#", "..#", "..#", "#.#", ".#."],
    "Q": ["###", "#.#", "#.#", "###", "..#"],
    "M": ["#...#", "##.##", "#.#.#", "#...#", "#...#"],
    "W": ["#...#", "#...#", "#.#.#", "##.##", "#...#"],
    "m": [".....", "##.#.", "#.#.#", "#.#.#", "#.#.#"],
    "w": [".....", "#...#", "#.#.#", "#.#.#", ".#.#."],
    "i": [".#.", "...", ".#.", ".#.", ".#."],
    "v": ["...", "...", "#.#", "#.#", ".#."],
    "x": ["...", "...", "#.#", ".#.", "#.#"],
    "s": ["...", ".##", "#..", ".#.", "##."],
    "d": ["..#", "..#", ".##", "#.#", ".##"],
    "a": ["...", "##.", ".##", "#.#", ".##"],
    "u": ["...", "...", "#.#", "#.#", ".##"],
    "g": ["...", ".##", "#.#", ".##", "##."],
    "o": ["...", "...", "###", "#.#", "###"],
    "n": ["...", "...", "##.", "#.#", "#.#"],
    "f": [".#", "#.", "##", "#.", "#."],
    "b": ["#..", "#..", "##.", "#.#", "##."],
    "#": ["#.#", "###", "#.#", "###", "#.#"],
    "+": ["...", ".#.", "###", ".#.", "..."],
    "-": ["...", "...", "###", "...", "..."],
    "/": ["..#", "..#", ".#.", "#..", "#.."],
    "(": [".#", "#.", "#.", "#.", ".#"],
    ")": ["#.", ".#", ".#", ".#", "#."],
    ".": [".", ".", ".", ".", "#"],
    ":": [".", "#", ".", "#", "."],
    "\xb0": ["###", "#.#", "###", "...", "..."],
    "\xf8": [".#.#", "#.#.", "#.##", ".#.#", "#.#."],
    "?": ["##.", "..#", ".#.", "...", ".#."],
    "%": ["#.#", "..#", ".#.", "#..", "#.#"],
    "<": ["..#", ".#.", "#..", ".#.", "..#"],
    ">": ["#..", ".#.", "..#", ".#.", "#.."],
    "!": ["#", "#", "#", ".", "#"],
    "=": ["...", "###", "...", "###", "..."],
    ",": [".", ".", ".", "#", "#"],
    "*": ["...", "#.#", ".#.", "#.#", "..."],
    "'": ["#", "#", ".", ".", "."],
}
SMALL = {  # small raised digits, 3x4 (top-aligned in the 5-row cell)
    "0": ["###", "#.#", "#.#", "###"],
    "1": [".#.", "##.", ".#.", ".#."],
    "2": ["##.", "..#", ".#.", "###"],
    "3": ["###", ".##", "..#", "###"],
    "4": ["#.#", "###", "..#", "..#"],
    "5": ["###", "##.", "..#", "##."],
    "6": ["#..", "###", "#.#", "###"],
    "7": ["###", "..#", ".#.", ".#."],
    "8": ["###", "###", "#.#", "###"],
    "9": ["###", "#.#", "###", "..#"],
}


def rows_bits(rows):
    out = []
    for r in rows:
        v = 0
        for i, ch in enumerate(r):
            if ch == "#":
                v |= 0x80 >> i
        out.append(v)
    return out


def micro_glyphs():
    g = {}
    for ch, rows in MICRO.items():
        g[ord(ch)] = (len(rows[0]) + 1, rows_bits(rows + ["." * len(rows[0])] * (5 - len(rows))))
    for ch, rows in SMALL.items():
        g[0x10 + int(ch)] = (len(rows[0]) + 1, rows_bits(rows + ["..."]))
    return g


def ui_glyphs():
    """GartStringer's face: Departure Mono (OFL), 11 px, 1-bit, rows of 16 bits (bit 15 = the leftmost pixel)."""
    dep = ImageFont.truetype(str(UI_FONT), UI_PX, layout_engine=ImageFont.Layout.BASIC)
    font = dep                                                    # 1.0: the whole face is Departure Mono, a true pixel face
    asc, desc = font.getmetrics()
    dasc, ddesc = dep.getmetrics()
    h = asc + desc
    glyphs = {}
    chars = {c: chr(c) for c in range(32, 127)}
    chars.update(UI_EXTRA)

    def raster(fnt, fasc, ch, adv, shift):
        im = Image.new("1", (adv + 6, h + 8), 0)
        d = ImageDraw.Draw(im)
        d.fontmode = "1"
        d.text((0, shift), ch, font=fnt, fill=1)
        rows = []
        for y in range(h):
            v = 0
            for x in range(min(16, adv + 6)):
                if im.getpixel((x, y)):
                    v |= 0x8000 >> x
            rows.append(v)
        return rows

    for code, ch in chars.items():
        fnt, fasc, shift = (font, asc, 0)
        if code >= 0x80 or ch in "\u00b0\u00f8\u2192\u2190\u2191\u2193\u00b7":
            fnt, fasc, shift = (dep, dasc, asc - dasc)
        adv = int(round(fnt.getlength(ch)))
        if code == 32:
            adv = 4
        if adv <= 0:
            continue
        glyphs[code] = (adv, raster(fnt, fasc, ch, adv, shift))
    for code, rows in UI_DRAWN.items():
        pad = [r if r else "......." for r in rows]
        pad = (["......."] * max(0, asc - 10) + pad)[:h]
        pad += ["......."] * (h - len(pad))
        glyphs[code] = (7, [v << 8 for v in rows_bits(pad)])
    return glyphs, h, asc


def emit(f, name, glyphs, h, base, rb=1):
    codes = sorted(glyphs)
    f.write(f"/* {name}: {len(codes)} glyphs, {h} rows, baseline {base} */\n")
    f.write(f"static const uint8_t {name}_BITS[] = {{\n")
    offs = {}
    pos = 0
    for c in codes:
        adv, rows = glyphs[c]
        offs[c] = pos
        if rb == 2:
            bs = [x for v in rows for x in (v >> 8, v & 255)]
        else:
            bs = rows
        f.write("    " + ", ".join(f"0x{v:02X}" for v in bs) + f",  /* {c:#04x} {chr(c)!r} */\n")
        pos += len(bs)
    f.write("};\n")
    f.write(f"static const sk_glyph_t {name}_GLYPHS[256] = {{\n")
    for c in codes:
        adv, rows = glyphs[c]
        f.write(f"    [{c:#04x}] = {{{offs[c]}, {adv}}},\n")
    f.write("};\n")
    f.write(f"static const sk_font_t {name} = {{{name}_BITS, {name}_GLYPHS, {h}, {base}, {rb}}};\n\n")


def main():
    out = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "build" / "gen" / "sk_fonts.h")
    ui, h, asc = ui_glyphs()
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("w") as f:
        f.write("/* Generated by tools/gen_pixfont.py - do not edit. Sora and Departure Mono: SIL OFL 1.1 "
                "(LICENSES/OFL-Sora.txt, OFL-DepartureMono.txt). */\n")
        f.write("#ifndef SK_FONTS_H\n#define SK_FONTS_H\n#include <stdint.h>\n")
        f.write("typedef struct { uint16_t off; uint8_t adv; } sk_glyph_t;   /* adv 0: no glyph */\n")
        f.write("typedef struct { const uint8_t *bits; const sk_glyph_t *g; uint8_t h, base, rb; } sk_font_t;   /* rb: bytes per row (0: 1) */\n\n")
        emit(f, "SK_FONT", ui, h, asc, 2)
        emit(f, "SK_MICRO", micro_glyphs(), 5, 5, 1)
        f.write("#endif\n")
    print(f"pixfont: {len(ui)} glyphs {h} rows -> {out}")


if __name__ == "__main__":
    main()
