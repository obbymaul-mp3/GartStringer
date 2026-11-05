// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 GartStringer contributors
// GartStringer in an AudioWorklet: gartstringer.wasm is the instrument itself (tools/web/gs_web.c). Each render quantum asks it for
// 128 frames, which also runs its clock; about 30 times a second the screen (when it changed) and the LEDs go to the page.
class GartStringer extends AudioWorkletProcessor {
  constructor() {
    super();
    this.ex = null;
    this.serial = -1;
    this.lastPub = 0;
    this.port.onmessage = (e) => this.onMessage(e.data);
  }

  async onMessage(m) {
    if (m.type === "load") {
      try {
        const { instance } = await WebAssembly.instantiate(m.wasm, {});
        this.ex = instance.exports;
        if (m.store && m.store.length) {
          new Uint8Array(this.ex.memory.buffer, this.ex.web_store_ptr(), m.store.length).set(m.store);
          this.ex.web_store_set(m.store.length);
        }
        this.storeGen = this.ex.web_store_gen_get();
        this.ex.web_boot();
        this.ex.web_intro();
        if (m.master != null) this.ex.web_master(m.master | 0);
        this.port.postMessage({ type: "ready" });
      } catch (err) {
        this.port.postMessage({ type: "error", message: String(err) });
      }
      return;
    }
    const ex = this.ex;
    if (!ex) return;
    switch (m.type) {
      case "btn": ex.web_btn(m.label >>> 0, m.down ? 1 : 0); break;
      case "enc": ex.web_enc(m.role >>> 0, m.steps | 0); break;
      case "key": ex.web_key(m.n >>> 0, m.down ? 1 : 0); break;
      case "master": ex.web_master(m.q12 >>> 0); break;
    }
  }

  process(inputs, outputs) {
    const ex = this.ex;
    if (!ex) return true;
    const out = outputs[0], n = out[0].length;
    ex.web_render(n);
    out[0].set(new Float32Array(ex.memory.buffer, ex.web_out_l(), n));
    if (out[1]) out[1].set(new Float32Array(ex.memory.buffer, ex.web_out_r(), n));
    if (currentTime - this.lastPub >= 1 / 30) {
      this.lastPub = currentTime;
      const msg = { type: "frame", leds: new Uint8Array(ex.memory.buffer, ex.web_leds(), 41).slice() };
      const transfer = [];
      const s = ex.web_fb_serial();
      if (s !== this.serial) {
        this.serial = s;
        msg.fb = new Uint32Array(ex.memory.buffer, ex.web_fb(), 57600).slice();
        transfer.push(msg.fb.buffer);
      }
      const g = ex.web_store_gen_get();
      if (g !== this.storeGen) {            // a slot was saved: the page keeps the bytes
        this.storeGen = g;
        msg.store = new Uint8Array(ex.memory.buffer, ex.web_store_ptr(), ex.web_store_len_get()).slice();
      }
      this.port.postMessage(msg, transfer);
    }
    return true;
  }
}
registerProcessor("gartstringer", GartStringer);
