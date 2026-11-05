// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 GartStringer contributors
// The play page: the screen, the panel (knobs, buttons, 27 keys from C3), the computer keyboard, and the AudioWorklet (worklet.js)
// that runs gartstringer.wasm. Before "Start the sound" a copy of the instrument runs here without sound, to show its screen.
"use strict";
(() => {
  const $ = (id) => document.getElementById(id);
  const BTN = ["FX", "SCL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC", "OCT−", "OCT+"];
  const SUB = ["fx", "filter", "env", "lfo", "osc", "glitch · gate", "home", "save", "arp", "sequencer", "hold · run", "glitch", "oct", "oct"];
  const USED = new Set([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13]);
  const ROLES = [["SELECT", "tempo"], ["ALGORITHM", "page"], ["PRESETS", "patch"], ["K1", ""], ["K2", ""], ["K3", ""], ["K4", ""]];
  const BLACK = (k) => (0x54a >> ((k + 5) % 12)) & 1;     // key 0 is C
  const KEYMAP = { Backquote: ["b", 11], Space: ["b", 10], Minus: ["b", 12], Equal: ["b", 13] };
  "Digit1 Digit2 Digit3 Digit4 Digit5 Digit6 Digit7 Digit8 Digit9 Digit0".split(" ").forEach((c, i) => { KEYMAP[c] = ["b", i]; });
  "KeyZ KeyX KeyC KeyV KeyB KeyN KeyM Comma Period Slash KeyA KeyS KeyD KeyF KeyG KeyH KeyJ KeyK KeyL Semicolon Quote KeyQ KeyW KeyE KeyR KeyT KeyY".split(" ")
    .forEach((c, i) => { KEYMAP[c] = ["k", i]; });

  let node = null, ctx = null, running = false;
  const send = (m) => { if (node) node.port.postMessage(m); else if (pre) preInput(m); };

  // ------------------------------------------------------------------ the screen
  const lcd = $("lcd"), g = lcd.getContext("2d"), img = g.createImageData(240, 240), px = new Uint32Array(img.data.buffer);
  const draw = (fb) => { px.set(fb); g.putImageData(img, 0, 0); };

  // ------------------------------------------------------------------ the panel
  const knobEl = [], btnEl = [], keyEl = [];
  let lastKnob = 3;
  const turn = (role, steps) => { send({ type: "enc", role, steps }); const e = knobEl[role]; e.ang = (e.ang + steps * 18) % 360; e.cap.style.setProperty("--a", e.ang + "deg"); lastKnob = role; };
  ROLES.forEach(([name, sub], role) => {
    const el = document.createElement("div");
    el.className = "knob"; el.tabIndex = 0; el.setAttribute("role", "slider"); el.setAttribute("aria-label", name + " (" + sub + ")");
    el.innerHTML = `<div class="cap"></div><b>${name}</b><small>${sub}</small>`;
    $("knobs").append(el);
    const k = { el, cap: el.firstChild, ang: 0, acc: 0, y: 0 };
    knobEl.push(k);
    el.addEventListener("pointerdown", (e) => { el.setPointerCapture(e.pointerId); k.y = e.clientY; k.drag = true; el.classList.add("on"); lastKnob = role; });
    el.addEventListener("pointermove", (e) => { if (!k.drag) return; k.acc += (k.y - e.clientY) / 9; k.y = e.clientY; const s = Math.trunc(k.acc); if (s) { k.acc -= s; turn(role, s); } });
    const up = () => { k.drag = false; el.classList.remove("on"); };
    el.addEventListener("pointerup", up); el.addEventListener("pointercancel", up);
    el.addEventListener("wheel", (e) => { e.preventDefault(); turn(role, e.deltaY < 0 ? 1 : -1); }, { passive: false });
    el.addEventListener("keydown", (e) => {
      if (e.key === "ArrowUp" || e.key === "ArrowRight") { turn(role, 1); e.preventDefault(); }
      if (e.key === "ArrowDown" || e.key === "ArrowLeft") { turn(role, -1); e.preventDefault(); }
    });
  });
  $("knobs").style.gridTemplateColumns = "repeat(7, 1fr)";
  BTN.forEach((name, i) => {
    const b = document.createElement("button");
    b.type = "button"; b.className = "b"; b.innerHTML = `${name}<small>${SUB[i]}</small>`; b.disabled = false;
    if (!USED.has(i)) b.style.opacity = ".4";
    $("btns").append(b); btnEl.push(b);
    const down = (e) => { e.preventDefault(); send({ type: "btn", label: i, down: 1 }); };
    const up = () => send({ type: "btn", label: i, down: 0 });
    b.addEventListener("pointerdown", down); b.addEventListener("pointerup", up); b.addEventListener("pointerleave", up);
  });
  for (let k = 0; k < 27; k++) {
    const e = document.createElement("div");
    e.className = "k" + (BLACK(k) ? " blk" : ""); e.innerHTML = "<i></i>" + (k % 12 === 0 ? `<small>C${3 + k / 12}</small>` : "");
    $("keys").append(e); keyEl.push(e);
    const on = (e2) => { e2.preventDefault(); send({ type: "key", n: k, down: 1 }); };
    const off = () => send({ type: "key", n: k, down: 0 });
    e.addEventListener("pointerdown", on); e.addEventListener("pointerup", off); e.addEventListener("pointerleave", off);
  }
  const leds = (L) => {
    btnEl.forEach((b, i) => { b.className = "b" + (L[i] ? " l" + L[i] : ""); });
    keyEl.forEach((e, k) => { e.className = "k" + (BLACK(k) ? " blk" : "") + (L[14 + k] ? " l" + L[14 + k] : ""); });
  };
  $("master").addEventListener("input", (e) => send({ type: "master", q12: master(+e.target.value) }));
  const master = (v) => (v * v) >> 8;

  // ------------------------------------------------------------------ the computer keyboard
  const held = new Set();
  addEventListener("keydown", (e) => {
    if (e.repeat || e.ctrlKey || e.metaKey || e.altKey) return;
    if (e.target && /INPUT|TEXTAREA/.test(e.target.tagName)) return;
    if (e.code === "Tab") { lastKnob = (lastKnob + (e.shiftKey ? 6 : 1)) % 7; knobEl[lastKnob].el.focus(); e.preventDefault(); return; }
    if (e.code === "ArrowUp" || e.code === "ArrowRight") { turn(lastKnob, 1); e.preventDefault(); return; }
    if (e.code === "ArrowDown" || e.code === "ArrowLeft") { turn(lastKnob, -1); e.preventDefault(); return; }
    const m = KEYMAP[e.code];
    if (!m) return;
    e.preventDefault(); held.add(e.code);
    if (m[0] === "b") send({ type: "btn", label: m[1], down: 1 }); else send({ type: "key", n: m[1], down: 1 });
  });
  addEventListener("keyup", (e) => {
    const m = KEYMAP[e.code];
    if (!m || !held.delete(e.code)) return;
    if (m[0] === "b") send({ type: "btn", label: m[1], down: 0 }); else send({ type: "key", n: m[1], down: 0 });
  });

  // ------------------------------------------------------------------ a silent copy that shows the screen at first
  let pre = null;
  const preInput = (m) => {
    const ex = pre; if (!ex) return;
    if (m.type === "btn") ex.web_btn(m.label, m.down); else if (m.type === "enc") ex.web_enc(m.role, m.steps); else if (m.type === "key") ex.web_key(m.n, m.down);
  };
  let preTimer = 0;
  const runPre = () => {
    if (!pre || running) return;
    pre.web_render(1024);
    draw(new Uint32Array(pre.memory.buffer, pre.web_fb(), 57600));
    leds(new Uint8Array(pre.memory.buffer, pre.web_leds(), 41));
  };
  // the saved slots (the SAVE page) are kept in this browser
  const STORE_KEY = "gartstringer.slots.v1";
  const readStore = () => { try { const t = localStorage.getItem(STORE_KEY); return t ? Uint8Array.from(atob(t), (c) => c.charCodeAt(0)) : null; } catch (e) { return null; } };
  const writeStore = (b) => { try { localStorage.setItem(STORE_KEY, btoa(String.fromCharCode(...b))); } catch (e) { /* private window: slots last for this visit only */ } };
  const loadStore = (ex) => { const b = readStore(); if (b && b.length) { new Uint8Array(ex.memory.buffer, ex.web_store_ptr(), b.length).set(b); ex.web_store_set(b.length); } };
  const bytes = fetch("gartstringer.wasm").then((r) => { if (!r.ok) throw new Error("HTTP " + r.status); return r.arrayBuffer(); });
  bytes.then(async (wasm) => {
    const { instance } = await WebAssembly.instantiate(wasm, {});
    pre = instance.exports; loadStore(pre); pre.web_boot();
    for (let i = 0; i < 16; i++) pre.web_render(1024);
    runPre();
    preTimer = setInterval(() => { for (let i = 0; i < 2; i++) pre.web_render(704); runPre(); }, 33);
  }).catch((e) => { $("status").textContent = "Couldn’t load the instrument: " + e.message; });

  // ------------------------------------------------------------------ start the sound
  $("startbtn").addEventListener("click", async () => {
    $("startbtn").disabled = true; $("status").textContent = "Starting…";
    try {
      const wasm = await bytes;
      ctx = new AudioContext({ latencyHint: "interactive", sampleRate: 44100 });
      await ctx.audioWorklet.addModule("worklet.js");
      node = new AudioWorkletNode(ctx, "gartstringer", { outputChannelCount: [2] });
      node.connect(ctx.destination);
      node.port.onmessage = (e) => {
        const m = e.data;
        if (m.type === "frame") { if (m.fb) draw(m.fb); leds(m.leds); if (m.store) writeStore(m.store); }
        else if (m.type === "ready") { running = true; clearInterval(preTimer); $("start").hidden = true; $("status").textContent = ""; }
        else if (m.type === "error") $("status").textContent = "Error: " + m.message;
      };
      node.port.postMessage({ type: "load", wasm, master: master(+$("master").value), store: readStore() });
      if (ctx.state !== "running") await ctx.resume();
    } catch (e) { $("status").textContent = "Couldn’t start the sound: " + e.message; $("startbtn").disabled = false; }
  });
  // (the silent copy follows the master slider too, so the first sound is at the level shown)
  if (pre) pre.web_master(master(+$("master").value));
  window.gartstringer = { send, turn };
})();
