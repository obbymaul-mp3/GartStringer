# GartStringer licensing

GartStringer is free software under the GNU General Public License, version 3 only (`GPL-3.0-only`; the full text is in
`LICENSE`). That covers the firmware, the tools and the web pages in this tree. A few bundled parts keep their own
licences; their texts are in `LICENSES/`.

The firmware built from this tree is GPL-3.0-only as a whole. If you distribute it, you must also give your
recipients its complete corresponding source under the same licence.

## GartStringer's own work

The sound engine, effects, scene renderer, physics sequencer, interface, browser version and web pages
(`firmware/src/sk_*.c`, `firmware/src/gartstringer.c`, `tools/web/`, `web/`) are new work, GPL-3.0-only.

## The hardware layer

The FM-1 platform code (`firmware/hal`, `firmware/stub`, `firmware/loader`, the C runtime, the update loader and the
installer protocol) comes from **Felucca** by Leo Kuroshita (Hügelton Instruments,
<https://github.com/hugelton/Felucca>), through **ChoralRoot FM-1** by Quixotic7. It is GPL-3.0-only and keeps its
copyright lines in the source files. The small 2D text/LCD driver and C library routines (`lcd.c`, `libc.c`) are
from the same lineage.

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Sora font (The Sora Project Authors): the screens' and web pages' typeface | SIL OFL 1.1 | `assets/fonts/Sora[wght].ttf`, `web/fonts/`, `LICENSES/OFL-Sora.txt` |
| Departure Mono (Helena Zhang): a few arrow and symbol glyphs in the screens' font | SIL OFL 1.1 | `assets/fonts/DepartureMono-Regular.otf`, `LICENSES/OFL-DepartureMono.txt` |
| JieLi AC79 SDK files `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin`, unmodified, inside the packages | Apache-2.0 | `tools/sdk/`, `LICENSES/Apache-2.0.txt` |

The SDK files keep JieLi Technology's copyright and are not under the GPL.

## Trademarks

"M-VAVE" and "FM-1" are trademarks of their respective owners. GartStringer is independent firmware that runs on FM-1
hardware. It is not affiliated with, endorsed by or supported by them.
