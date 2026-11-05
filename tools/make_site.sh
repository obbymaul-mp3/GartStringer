#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# GartStringer's site, put together from its sources:   sh tools/make_site.sh OUT
#   OUT/            the first page (web/index.html: what it is, and its installer: installer.js, fm1*.js), site.css, fonts/,
#                   images/ (the screens)
#   OUT/play/       the browser version (build/web: sh tools/build_web.sh first)
#   OUT/install/    the old installer address, forwarding to /#install
# OUT/firmware/ (the released packages and releases.json) is not touched: pages.yml fills it from the releases.
# A new site.css or module is a new address (?v=hash): a browser cannot keep an old one beside a new page.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:?usage: sh tools/make_site.sh OUT}
cd "$ROOT"
WEB=build/web
for f in index.html play.js worklet.js gartstringer.wasm; do
    [ -f "$WEB/$f" ] || { echo "$WEB/$f missing: run sh tools/build_web.sh first"; exit 1; }
done
mkdir -p "$OUT/fonts" "$OUT/images" "$OUT/install" "$OUT/play"
cp web/index.html web/site.css web/favicon.svg web/installer.js web/fm1pkg.js web/fm1ota.js web/fm1info.js "$OUT/"
cp web/fonts/Sora.ttf web/fonts/OFL-Sora.txt "$OUT/fonts/"
cp docs/images/*.png "$OUT/images/"
cp web/install/index.html "$OUT/install/"
cp "$WEB/index.html" "$WEB/play.js" "$WEB/worklet.js" "$WEB/gartstringer.wasm" "$OUT/play/"
STAMP=$(cat web/site.css | shasum | cut -c1-10)
sed -i.bak "s|href=\"site.css\"|href=\"site.css?v=$STAMP\"|" "$OUT/index.html"
sed -i.bak "s|href=\"../site.css\"|href=\"../site.css?v=$STAMP\"|" "$OUT/play/index.html"
for m in installer.js fm1pkg.js fm1ota.js fm1info.js; do
    MS=$(cat "web/$m" | shasum | cut -c1-10)
    sed -i.bak "s|\"./$m\"|\"./$m?v=$MS\"|" "$OUT/index.html" "$OUT/installer.js" "$OUT/fm1ota.js" "$OUT/fm1pkg.js" 2>/dev/null || true
    sed -i.bak "s|src=\"$m\"|src=\"$m?v=$MS\"|" "$OUT/index.html"
done
rm -f "$OUT"/*.bak "$OUT/play"/*.bak
touch "$OUT/.nojekyll"
echo "site: $OUT"
