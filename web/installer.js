// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 ChoralRoot FM-1 contributors (a fork of Felucca); Copyright (C) 2026 GartStringer contributors
// GartStringer's installer, the module of web/index.html: it picks a version (hosted on this site and checked against its
// SHA-256, or a .fwsc file the user chooses), checks the package here, and writes it to the FM-1 over WebMIDI with the
// update protocol of fm1ota.js. The FM-1 either runs a firmware that answers (the official one, an older one) or is
// already in update mode (OCT- held while switching on, or OCT- + OCT+ + HOME held five seconds in GartStringer).
import { Package, productOf, logicalImage, sha256hex, crc16, validateStockPackage, STOCK_FILES, STOCK_APP_SIZE } from "./fm1pkg.js";
import { Updater, classifyFirmware, refusalText } from "./fm1ota.js";
import { InfoConnection, deviceInfo } from "./fm1info.js";

const $ = (id) => document.getElementById(id);
const PRODUCT = "FM-1_930";                      // GartStringer's identity (tools/build.py PRODUCT): every package, every version
const MODEL = PRODUCT.split("_")[0];
const LOADER_MARK = "FELUCCA-LOADER-1";          // the update loader (tools/fm1pkg_make.py ota.bin): never a vendor one
const PKG_FLASH = 0x93000, PKG_APP_BASE = 0x4000; // the FM-1 flash layout of every fm1pkg_make.py package
const PKG_MAX = 4 << 20;                          // (a package is under a megabyte: anything far larger is not one)
const RELEASES = "firmware/releases.json";        // the versions hosted on this site; missing: only a file is offered
const FIRMWARE_DIR = "firmware/";

const TEXT = {
  nomidi: "This browser can’t reach the FM-1. Open this page in Chrome or Edge on a computer.",
  denied: "The browser blocked MIDI. Click the icon left of the address bar, allow MIDI devices, then press the button again.",
  notfound: "No FM-1 found. Check the cable (a data cable, straight to the computer), switch the FM-1 on, quit other MIDI apps and try again. If it already runs GartStringer: hold OCT−, OCT+ and HOME for five seconds (or hold OCT− while switching on) to put it in update mode, then press Install.",
  model: "The device you connected isn’t an FM-1.",
  lost: "The USB connection dropped. Plug the cable back in and press Install again.",
  stopped: "The FM-1 stopped answering. Plug the cable back in and press Install again.",
  noloader: "The FM-1 didn’t switch to update mode. Plug the cable back in and press Install again. Once in update mode, the FM-1 stays there until writing finishes.",
  noreturn: "The FM-1 didn’t come back after the write. Switch it off and on.",
  mismatch: "Installed, but the FM-1 now reports different firmware: ",
  badreq: "The FM-1 asked for data the file doesn’t have. Plug the cable back in and press Install again.",
  error: "Error: ",
  leave: "Writing to the FM-1. Don’t close this page.",
  cancelled: "Cancelled. Nothing was written.",
  start: "Starting",
  verify: "The FM-1 is checking the package",
  loader: "Switching to update mode",
  write: "Writing. Don’t unplug the FM-1.",
  reboot: "Restarting",
  done: "Done",
  installV: "Install {name}",
  installNone: "Install on your FM-1",
  noFile: "No file chosen",
  newest: "Newest",
  noRelease: "No release yet: the first one is still being tested.",
  pkgChecking: "Checking {name} …",
  pkgOk: "Ready to install {name}. Your browser checked it: a complete, undamaged GartStringer build.",
  pkgRelOk: "Ready to install GartStringer {version}. The file matches the released one.",
  pkgSum: "SHA-256 {sum}",
  pkgNotFm1: "{name} isn’t an FM-1 firmware file (.fwsc). Choose another file.",
  pkgStock: "{name} is the official M-VAVE firmware ({id}), not GartStringer. To go back to it, use “Return to the official firmware” below.",
  pkgIdentity: "{name} isn’t a GartStringer package: it’s made for {id}. Choose a GartStringer build.",
  pkgNoLoader: "{name} isn’t a complete GartStringer build. Choose another file.",
  pkgLayout: "{name} isn’t a complete GartStringer build. Choose another file.",
  pkgDamaged: "{name} is damaged ({detail}). Download it again.",
  pkgBadHash: "The file for GartStringer {version} doesn’t match the released one. Reload the page.",
  pkgRead: "Couldn’t read {name}: {detail}",
  download: "Download {file}",
  forceConfirm: "Install GartStringer over {name} anyway? This isn’t supported. An install like this has already left one FM-1 unable to start (recovery: github.com/Quixotic7/MvaveFM1Unbricker). Continue only if you can recover the FM-1 yourself.",
  cnTesting: "Looking for the FM-1. Nothing gets written.",
  cnOwn: "Your FM-1 runs {name}. You can install this version over it.",
  cnOther: "Your FM-1 runs {name}. You can install GartStringer over it.",
  cnLoader: "Your FM-1 is in update mode ({identity}). Either an update was cut off or someone started it by hand. Press Install to finish writing.",
  dnPlain: "Done. GartStringer is installed.",
  dnOther: "Written, but the FM-1 now says it runs {name}.",
  stockNeedFlag: "An FM-1 in update mode: tick “I’m finishing an update that was cut off” to write the official firmware to it.",
  stockValid: "The official firmware file is valid. Ready.",
  stockConfirm: "Install the official firmware on the FM-1? GartStringer will be replaced.",
  stockError: "Not the official firmware file: ",
  stockWrongFile: "Choose the unmodified official FM-1 V15 or V16 file (FM-1.fwsc).",
  stockDnLine: "The FM-1 now runs the official firmware ({identity}).",
};
const t = (k) => TEXT[k];
const tv = (k, vals) => Object.entries(vals).reduce((s, [a, v]) => s.split(`{${a}}`).join(String(v)), t(k));
const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", "\"": "&quot;" })[c]);
const MONTHS = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"];
const dateText = (iso) => { const m = /^(\d{4})-(\d\d)-(\d\d)$/.exec(iso || ""); return m ? `${Number(m[3])} ${MONTHS[Number(m[2]) - 1]} ${m[1]}` : ""; };

// where the messages go: the installer's own status line, or the return-to-official tool's (its own status and log)
const OUT_TOP = { status: "status", log: null }, OUT_STOCK = { status: "stock-status", log: "stock-log" };
let out = OUT_TOP;
const sayAt = (where, fn) => { const was = out; out = where; try { return fn(); } finally { out = was; } };
const sayK = (k, arg = "") => { $(out.status).textContent = t(k) + arg; };
const say = (txt) => { $(out.status).textContent = txt; };
const log = (line) => { $("log").textContent += line + "\n"; if (out.log) $(out.log).textContent += line + "\n"; };

let writing = false;
let image = null;                                // the logical image Install writes, set only after its checks passed
let pick = null;                                 // what Install writes: { rel, label, file } (a release) or { label, file }
let releases = [];                               // firmware/releases.json, newest first, each { version, sha256, file, url }
let pkgEpoch = 0;                                // (a later choice wins over a check still running)
let stock = null, stockFileEpoch = 0;
let unit = null;                                 // "What is on my FM-1?": what was read (identify(), or { loader })
const canMidi = typeof navigator !== "undefined" && !!navigator.requestMIDIAccess;
const midiAccess = () => navigator.requestMIDIAccess({ sysex: true });

const lockInstaller = (on) => {
  writing = on;
  $("go").disabled = on || !canMidi || !image;
  $("pkg-pick").disabled = on; $("pkg-file").disabled = on; $("force-risk").disabled = on;
  $("check-go").disabled = on || !canMidi;
  $("stock-go").disabled = on || !canMidi || !stock; $("stock-file").disabled = on; $("stock-pick").disabled = on; $("stock-recovery").disabled = on;
  renderPick();
};
window.addEventListener("beforeunload", (e) => { if (writing) { e.preventDefault(); e.returnValue = t("leave"); } });

const coded = (code, detail = "") => Object.assign(new Error(code + (detail ? ": " + detail : "")), { code, detail });

// the running firmware (handshake identity + INFO version) -> classifyFirmware, or null (no FM-1 or another model)
async function identify(up) {
  const dev = await up.find((id) => !/^ota-/i.test(id.model));
  if (!dev) return null;
  const input = dev.link.input, output = dev.link.output, id = dev.id; dev.link.close();
  if (id.model !== MODEL) return null;
  const link = new InfoConnection(input, output);
  let info = null;
  try { info = await deviceInfo((r, o) => link.request(r, o)); } finally { link.close(); }
  return { ...classifyFirmware(id.text, info && info.version), identity: id.text, version: info && info.version };
}
const failSay = (e) => {
  if (e && e.code && TEXT[e.code]) sayK(e.code, e.detail || "");
  else sayK("error", e && e.message);
  log(String(e && e.stack || e));
};

// ---- the package: GartStringer's identity, then the package's own structure (every CRC: a damaged download fails here),
// the flash layout, and the update loader in its ota.bin (the loader the FM-1 stages and runs).
const hasText = (bytes, text) => {
  const n = [...text].map((c) => c.charCodeAt(0));
  outer: for (let i = 0; i + n.length <= bytes.length; i++) {
    for (let k = 0; k < n.length; k++) if (bytes[i + k] !== n[k]) continue outer;
    return true;
  }
  return false;
};
function checkPackage(bytes) {
  if (!bytes || bytes.length < 20 * 48 + 0x400 || bytes.length > PKG_MAX) throw coded("pkgNotFm1");
  const product = productOf(bytes);
  if (product !== PRODUCT) {
    if (/^FM-1_0\d\d$/.test(product)) throw coded("pkgStock", product);
    if (/^FM-1_\d+$/.test(product)) throw coded("pkgIdentity", product);
    throw coded("pkgNotFm1");
  }
  let pkg;
  try { pkg = new Package(bytes); } catch (e) { throw coded("pkgDamaged", e.message); }
  const ota = pkg.entries.find((e) => e.type === 100 && e.name === "ota.bin");
  if (!ota || pkg.flashSize !== PKG_FLASH || pkg.appBase !== PKG_APP_BASE || pkg.app.size !== STOCK_APP_SIZE) throw coded("pkgLayout");
  const at = ota.dataOff + pkg.skew;
  if (at + ota.size > bytes.length) throw coded("pkgDamaged", "ota.bin is cut short");
  const loader = bytes.subarray(at, at + ota.size);
  if (crc16(loader) !== ota.dcrc) throw coded("pkgDamaged", "ota.bin CRC mismatch");
  if (!hasText(loader, LOADER_MARK)) throw coded("pkgNoLoader");
  return { product, image: logicalImage(bytes) };
}
function pkgSay(text, state = "", sum = "") {
  for (const id of ["pkg", "pkg-own"]) { $(id).textContent = text; $(id).className = state ? `pkg ${state}` : "pkg"; }
  $("pkg-sum").textContent = sum ? tv("pkgSum", { sum }) : "";
}
function pkgFail(e, name) {
  const known = ["pkgNotFm1", "pkgStock", "pkgIdentity", "pkgNoLoader", "pkgLayout", "pkgDamaged"];
  if (e && known.includes(e.code)) pkgSay(tv(e.code, { name, id: e.detail, detail: e.detail }), "bad");
  else pkgSay(tv("pkgRead", { name, detail: e && e.message }), "bad");
  log(`package ${name}: ${e && e.message}`);
}

// ---- the choice: a version of firmware/releases.json (chips), or a file from this computer ----
const pkgFileName = (version) => `gartstringer-${String(version).replace(/[^A-Za-z0-9.-]/g, "-")}.fwsc`;
async function loadReleases() {                  // -> the usable entries, newest first; [] when there is no list
  let data;
  try {
    const r = await fetch(RELEASES, { cache: "no-cache" });
    if (!r.ok) throw new Error(`HTTP ${r.status}`);
    data = await r.json();
  } catch (e) { log(`releases: ${RELEASES}: ${e.message} (only a file from this computer is offered)`); return []; }
  const list = data && Array.isArray(data.releases) ? data.releases : [];
  const ok = list.filter((r) => r && typeof r.version === "string" && /^[0-9a-f]{64}$/.test(r.sha256 || ""));
  if (ok.length !== list.length) log(`releases: ${list.length - ok.length} entries without a version or a SHA-256 are not offered`);
  return ok.map((r) => {                         // (only files of this site's firmware folder: a name, never a URL)
    const file = typeof r.pkg === "string" && r.pkg ? r.pkg.split("/").pop() : pkgFileName(r.version);
    return { ...r, file, url: FIRMWARE_DIR + file };
  });
}
function notesOf(r) {
  const list = (items) => (Array.isArray(items) && items.length ? `<ul>${items.map((b) => `<li>${esc(b)}</li>`).join("")}</ul>` : "");
  return `<p class="when">${[r.title, dateText(r.date)].filter(Boolean).map(esc).join(" · ")}</p>` + list(r.bullets) + list(r.notes) +
    `<p class="dl"><a href="${esc(r.url)}" download>${esc(tv("download", { file: r.file }))}</a><br>sha256 ${esc(r.sha256)}</p>`;
}
function renderPick() {
  const any = releases.length > 0;
  $("versions").hidden = !any; $("rel-some").hidden = !any; $("rel-none").hidden = any;
  $("versions").innerHTML = releases.map((r, i) =>
    `<button type="button" class="chip" data-ver="${esc(r.version)}" aria-pressed="${!!(pick && pick.rel === r)}"${writing ? " disabled" : ""}>` +
    `<b>GartStringer ${esc(r.version)}</b><span>${[i === 0 ? t("newest") : "", dateText(r.date)].filter(Boolean).map(esc).join(" · ")}</span></button>`).join("");
  $("go").textContent = pick && image ? tv("installV", { name: pick.label }) : t("installNone");
  $("ver").textContent = pick ? [pick.label, pick.rel ? dateText(pick.rel.date) : ""].filter(Boolean).join(" · ") : any ? "" : t("noRelease");
  $("notes").innerHTML = pick && pick.rel ? notesOf(pick.rel) : "";
}
function newChoice(p) {                          // a new choice: nothing checked yet, the last run's panels go
  pick = p; image = null; ++pkgEpoch;
  $("done").hidden = true; $("interrupted").hidden = true; $("force-box").hidden = true; $("force-risk").checked = false;
  $("bar").value = 0; say("");
  lockInstaller(writing);
  return pkgEpoch;
}
async function loadRelease(r) {
  const epoch = newChoice({ rel: r, label: `GartStringer ${r.version}`, file: r.file });
  $("pkg-name").textContent = t("noFile"); $("pkg-file").value = "";
  pkgSay(tv("pkgChecking", { name: r.file }));
  try {
    const res = await fetch(r.url);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const bytes = new Uint8Array(await res.arrayBuffer());
    const sum = await sha256hex(bytes);
    if (epoch !== pkgEpoch) return;
    if (sum !== r.sha256) {                       // not the released file: never offered to the FM-1
      pkgSay(tv("pkgBadHash", { version: r.version }), "bad");
      log(`sha256 of ${r.url}: ${sum}, the list says ${r.sha256}`);
      return;
    }
    const ok = checkPackage(bytes);
    image = ok.image;
    pkgSay(tv("pkgRelOk", { version: r.version }), "ok", sum);
    log(`release ${r.version}: ${r.file}, ${bytes.length} B, ${ok.product}, sha256 ${sum}`);
  } catch (e) { if (epoch === pkgEpoch) pkgFail(e, r.file); }
  finally { if (epoch === pkgEpoch) lockInstaller(writing); }
}
async function loadFile(file) {
  const epoch = newChoice({ label: file.name, file: file.name });
  $("pkg-name").textContent = file.name;
  pkgSay(tv("pkgChecking", { name: file.name }));
  try {
    if (file.size > PKG_MAX) throw coded("pkgNotFm1");
    const bytes = new Uint8Array(await file.arrayBuffer());
    if (epoch !== pkgEpoch) return;
    const ok = checkPackage(bytes);
    const sum = await sha256hex(bytes);
    if (epoch !== pkgEpoch) return;
    image = ok.image;
    pkgSay(tv("pkgOk", { name: file.name }), "ok", sum);
    log(`file ${file.name}: ${bytes.length} B, ${ok.product}, sha256 ${sum}`);
  } catch (e) { if (epoch === pkgEpoch) pkgFail(e, file.name); }
  finally { if (epoch === pkgEpoch) lockInstaller(writing); }
}
$("versions").addEventListener("click", (e) => {
  const b = e.target && e.target.closest ? e.target.closest("[data-ver]") : null;
  const r = b && !b.disabled && releases.find((x) => x.version === b.dataset.ver);
  if (r && !writing && !(pick && pick.rel === r)) return loadRelease(r);
});
$("pkg-pick").addEventListener("click", () => { if (!writing) $("pkg-file").click(); });
$("pkg-file").addEventListener("change", () => {
  const file = $("pkg-file").files && $("pkg-file").files[0];
  if (file && !writing) return loadFile(file);
});

// ---- "What is on my FM-1?": the handshake and the INFO request the install itself starts with; nothing is written
function showUnit() {
  const el = $("unit");
  let bad = false, text;
  if (!unit) { bad = true; text = t("notfound"); }
  else if (unit.loader) text = tv("cnLoader", { identity: unit.identity });
  else if (unit.verdict === "refuse") { bad = true; text = refusalText(unit.name); }
  else text = tv(unit.kind === "gartstringer" ? "cnOwn" : "cnOther", { name: unit.name });
  el.hidden = false; el.className = bad ? "unit bad" : "unit"; el.textContent = text;
}
async function checkUnit() {
  if (writing) return;
  lockInstaller(true);
  const el = $("unit");
  el.hidden = false; el.className = "unit"; el.textContent = t("cnTesting");
  let said = null;                               // a reason other than what showUnit() says of `unit`
  try {
    let midi;
    try { midi = await midiAccess(); } catch (e) { unit = null; said = t("denied"); return; }
    const up = new Updater(midi, log);
    const dev = await up.find();
    unit = null;
    if (!dev) return;
    dev.link.close();
    if (dev.id.model.replace(/^ota-/i, "") !== MODEL) { said = t("model"); return; }
    if (/^ota-/i.test(dev.id.model)) unit = { loader: true, identity: dev.id.text };
    else unit = await identify(up);
    if (unit) log(`check: ${unit.identity}${unit.version ? " " + unit.version : ""} -> ${unit.loader ? "loader" : unit.verdict}`);
  } catch (e) { unit = null; said = t("error") + (e && e.message); log(String(e && e.stack || e)); }
  finally {
    lockInstaller(false);
    if (said) { el.className = "unit bad"; el.textContent = said; } else showUnit();
  }
}
$("check-go").addEventListener("click", () => checkUnit());

// ---- the install: the two steps of fm1ota.js (the running firmware checks the package and stages the loader; the
// loader writes), then the FM-1's identity read back
async function readBack(up) {                    // -> the INFO version of what runs now, or null
  try { const fw = await identify(up); log(`read back: ${fw ? fw.identity + " " + (fw.version || "(no INFO)") : "no answer"}`); return fw && fw.version || null; }
  catch (e) { log(`read back: ${e.message}`); return null; }
}
function finishInstall(runs) {
  unit = null; $("unit").hidden = true;
  $("done-line").textContent = !runs || /^gartstringer\b/i.test(runs) ? t("dnPlain") : tv("dnOther", { name: runs });
  $("done").hidden = false;
}
$("go").addEventListener("click", async () => {
  if (writing || !image) return;
  const what = pick, pkgImage = image;           // what the button named when it was pressed
  let began = false, okDone = false, halted = false, runs = null;
  lockInstaller(true);
  $("interrupted").hidden = true; $("done").hidden = true; $("bar").value = 0;
  let midi;
  try { midi = await midiAccess(); }
  catch (e) { sayK("denied"); lockInstaller(false); return; }
  try {
    const up = new Updater(midi, log);
    const reads = 74, writes = 1167;             // (the reads of each step for this package layout: an estimate)
    const bar = (v) => { $("bar").value = Math.max(Number($("bar").value) || 0, Math.min(99, v)); };
    const step = (k, a) => {
      log(`${k} ${a ?? ""}`);
      if (k === "start") { began = true; sayK("start"); }
      if (k === "verify") { sayK("verify"); bar(a / (reads + writes) * 100); }
      if (k === "loader") sayK("loader");
      if (k === "write") { began = true; sayK("write"); bar((reads + a) / (reads + writes) * 100); }
      if (k === "reboot") sayK("reboot");
      if (k === "done") { $("bar").value = 100; sayK("done"); }
    };
    const ota = await up.find((id) => /^ota-/i.test(id.model));
    if (ota) {                                   // an update that was cut off, or started by hand: resume it
      ota.link.close();
      began = true;
      log(`resume: ${what.label}`);
      okDone = await up.resume(pkgImage, step, PRODUCT);
      sayK(okDone ? "done" : "stopped");
      halted = !okDone;
      if (okDone) runs = await readBack(up);
    } else {
      const fw = await identify(up);             // which firmware runs: Sloop, the 0.x betas, unknown ones are refused
      if (fw) {
        log(`running: ${fw.identity}${fw.version ? " " + fw.version : ""} -> ${fw.verdict} (${fw.kind})`);
        if (fw.verdict === "refuse") {
          if (!$("force-risk").checked) {        // the override shows only after a refusal
            $("force-box").hidden = false; say(refusalText(fw.name)); lockInstaller(false); return;
          }
          if (!confirm(tv("forceConfirm", { name: fw.name }))) { sayK("cancelled"); lockInstaller(false); return; }
          log(`override: installing over ${fw.name} (not supported)`);
        }
      }
      log(`install: ${what.label} (${what.file})`);
      await up.install(pkgImage, PRODUCT, step);
      okDone = true;
      runs = await readBack(up);
    }
  } catch (e) { failSay(e); halted = began && !okDone && e.code !== "mismatch"; }
  lockInstaller(false);
  if (okDone) finishInstall(runs);
  else if (halted) $("interrupted").hidden = false;   // what to do now: the cases, and the way to the help
});

// ---- the return to the official V15. It reports in its own section (OUT_STOCK): the install card is out of sight
// from there. The section is opened whenever something is shown in it, and stays open while a run is going.
let stockBusy = false;
function stockShow(scroll) {
  const d = $("stock"), s = $("stock-status");
  d.open = true;
  if (!scroll || !s.getBoundingClientRect || !s.scrollIntoView) return;
  const r = s.getBoundingClientRect();
  if (r.top < 0 || r.bottom > window.innerHeight) s.scrollIntoView({ block: "center" });
}
$("stock").addEventListener("toggle", () => {
  if (!$("stock").open && (stockBusy || !$("stock-done").hidden || !$("stock-interrupted").hidden)) $("stock").open = true;
});
function clearTop() {
  $("bar").value = 0; $("done").hidden = true; $("interrupted").hidden = true;
  unit = null; $("unit").hidden = true; sayAt(OUT_TOP, () => say(""));
}
$("stock-pick").addEventListener("click", () => { if (!writing) $("stock-file").click(); });
$("stock-file").addEventListener("change", async () => {
  const epoch = ++stockFileEpoch, file = $("stock-file").files[0]; stock = null; lockInstaller(writing);
  if (!file) return;
  $("stock-name").textContent = file.name || t("noFile");
  try {
    if (!STOCK_FILES.some((f) => f.size === file.size)) throw new Error(t("stockWrongFile"));
    const verified = await validateStockPackage(new Uint8Array(await file.arrayBuffer()));
    if (epoch !== stockFileEpoch) return;
    stock = verified; sayAt(OUT_STOCK, () => sayK("stockValid")); stockShow(false); lockInstaller(writing);
  } catch (e) { if (epoch === stockFileEpoch) { sayAt(OUT_STOCK, () => sayK("stockError", e.message)); stockShow(false); } }
});
$("stock-go").addEventListener("click", async () => {
  if (!stock || writing) return;
  const selected = stock; lockInstaller(true);
  let began = false, okDone = false, halted = false, identity = "";
  out = OUT_STOCK; stockBusy = true;
  $("stock-bar").value = 0; $("stock-done").hidden = true; $("stock-interrupted").hidden = true; $("stock-log").textContent = "";
  stockShow(true);
  try {
    let midi;
    try { midi = await midiAccess(); } catch (e) { sayK("denied"); return; }
    const up = new Updater(midi, log);
    const reads = Math.ceil(selected.image.length / 512);
    const bar = (v) => { const b = $("stock-bar"); b.value = Math.max(Number(b.value) || 0, Math.min(99, v)); };
    const step = (k, n) => {
      log(`${k} ${n ?? ""}`);
      if (["start", "verify", "loader", "write", "reboot", "done"].includes(k)) sayK(k);
      if (k === "start" || k === "write") began = true;
      if (k === "verify") bar(Math.min(n, 74) / 74 * 6);
      if (k === "loader") bar(6);
      if (k === "write") bar(6 + (n / reads) * 93);
      if (k === "done") { identity = n; $("stock-bar").value = 100; }
    };
    const loader = await up.find((id) => id.model === "ota-" + MODEL);
    if (loader) {
      loader.link.close();
      if (!$("stock-recovery").checked) { sayK("stockNeedFlag"); return; }
      if (!confirm(t("stockConfirm"))) { sayK("cancelled"); return; }
      clearTop(); began = true;
      okDone = await up.resume(selected.image, step, selected.product);
      if (!okDone) { sayK("stopped"); halted = true; }
    } else {
      if (!confirm(t("stockConfirm"))) { sayK("cancelled"); return; }
      clearTop();
      await up.install(selected.image, selected.product, step);
      okDone = true;
    }
  } catch (e) { failSay(e); halted = began && !okDone && e.code !== "mismatch"; }
  finally {
    if (okDone) { $("stock-done-line").textContent = tv("stockDnLine", { identity: identity || selected.product }); $("stock-done").hidden = false; }
    else if (halted) $("stock-interrupted").hidden = false;
    stockShow(false);
    out = OUT_TOP; stockBusy = false; lockInstaller(false);
  }
});

// ---- start: no Web MIDI -> say so (the file can still be checked); then the releases, if this site lists any
$("nomidi").hidden = canMidi;
if (!canMidi) sayK("nomidi");
lockInstaller(false);
const ready = loadReleases().then((list) => {
  releases = list;
  renderPick();
  if (!releases.length || pick) return;
  // no Web MIDI (a phone, Safari, Firefox): the newest version is named, but its package is not downloaded
  if (canMidi) return loadRelease(releases[0]);
  pick = { rel: releases[0], label: `GartStringer ${releases[0].version}`, file: releases[0].file }; renderPick();
});
window.installerReady = ready;                    // (the tests wait for it)
