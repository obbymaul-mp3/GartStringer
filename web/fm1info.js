// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 ChoralRoot FM-1 contributors (a fork of Felucca); Copyright (C) 2026 GartStringer contributors
// What a running FM-1 firmware says it is: the INFO request of the editor protocol (SysEx F0 7D 46 4C cmd .. F7) and
// its answer. Used by the installer to tell which firmware is on the unit before it writes. A firmware that does not
// answer (the official one, GartStringer) gives null.
export const INFO_CMD = 1;

export async function deviceInfo(request) {
  let a;
  try { a = await request([INFO_CMD, []], { timeout: 1500, retries: 0 }); } catch (_) { return null; }
  const end = a.indexOf(0), version = String.fromCharCode(...a.slice(0, end < 0 ? a.length : end));
  return { version };
}

// One MIDI in/out pair for requests: nothing else owns this input meanwhile.
export class InfoConnection {
  constructor(input, output) {
    this.input = input; this.output = output; this.pending = null;
    input.onmidimessage = (e) => {
      const d = e.data;
      if (d.length < 6 || d[0] !== 240 || d[1] !== 125 || d[2] !== 70 || d[3] !== 76 || d.at(-1) !== 247) return;
      if (this.pending?.cmd === d[4]) { const p = this.pending; this.pending = null; clearTimeout(p.timer); p.resolve(Array.from(d.slice(5, -1))); }
    };
  }
  async request([cmd, args], opt = {}) {
    if (this.pending || this.closed || this.input.state === "disconnected" || this.output.state === "disconnected") throw new Error("Connection closed or busy");
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => { this.pending = null; reject(new Error("The FM-1 did not answer.")); }, opt.timeout || 1200);
      this.pending = { cmd, timer, resolve, reject };
      try { this.output.send([240, 125, 70, 76, cmd, ...args, 247]); }
      catch (e) { clearTimeout(timer); this.pending = null; reject(e); }
    });
  }
  close() {
    this.closed = true; this.input.onmidimessage = null;
    if (this.pending) { const p = this.pending; this.pending = null; clearTimeout(p.timer); p.reject(new Error("Connection closed")); }
  }
}
