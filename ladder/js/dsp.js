/*
 * Ladder 24 — the sound engine.
 *
 * Everything that makes sound lives here: band-limited oscillators, the
 * four-pole transistor-ladder filter, the two contour generators, voice
 * allocation, glide and the arpeggiator. It runs inside an AudioWorklet in the
 * browser, and the same file loads in plain Node so the tests can play notes
 * and measure what comes out.
 */
(function ladderDsp(root) {
  'use strict';

  const TAU = Math.PI * 2;
  const LN2 = Math.LN2;
  const VOICES = 8;
  const UNISON = 5;

  function midiToFreq(note) {
    return 440 * Math.exp(((note - 69) / 12) * LN2);
  }

  // Polynomial band-limited step: smooths the jump in a saw or pulse so high
  // notes do not alias into a metallic whine.
  function polyBlep(t, dt) {
    if (t < dt) {
      t /= dt;
      return t + t - t * t - 1;
    }
    if (t > 1 - dt) {
      t = (t - 1) / dt;
      return t * t + t + t + 1;
    }
    return 0;
  }

  class Oscillator {
    constructor(phase) {
      this.phase = phase === undefined ? Math.random() : phase;
      this.out = 0;
    }

    // wave: 'tri' | 'saw' | 'ramp' | 'square' | 'pulse' | 'sine'
    next(freq, sr, wave, pw) {
      const dt = Math.min(Math.abs(freq) / sr, 0.5);
      const t = this.phase;
      let y;
      switch (wave) {
        case 'saw':
          y = 2 * t - 1 - polyBlep(t, dt);
          break;
        case 'ramp':
          y = 1 - 2 * t + polyBlep(t, dt);
          break;
        case 'square':
        case 'pulse': {
          const w = wave === 'square' ? 0.5 : pw;
          y = t < w ? 1 : -1;
          y += polyBlep(t, dt);
          y -= polyBlep((t - w + 1) % 1, dt);
          y -= 2 * w - 1; // remove the DC a lopsided pulse carries
          break;
        }
        case 'sine':
          y = Math.sin(TAU * t);
          break;
        default:
          y = 4 * Math.abs(t - 0.5) - 1;
      }
      let p = t + dt;
      if (p >= 1) p -= 1;
      this.phase = p;
      this.out = y;
      return y;
    }
  }

  // Four one-pole stages in a loop with negative feedback: the 24 dB/octave
  // transistor ladder. Solved without a unit delay in the loop (the
  // "zero-delay feedback" form) so it stays in tune and stable at every
  // cutoff, with a tanh at the input for the way the real circuit saturates.
  // Near full resonance it rings on its own as a sine at the cutoff.
  class LadderFilter {
    constructor() {
      this.s = [0, 0, 0, 0];
    }

    reset() {
      this.s[0] = this.s[1] = this.s[2] = this.s[3] = 0;
    }

    // k is the feedback amount: 0 = none, about 4 = self-oscillation.
    process(x, cutoff, k, sr) {
      const fc = Math.min(Math.max(cutoff, 8), sr * 0.45);
      const g = Math.tan((Math.PI * fc) / sr);
      const G = g / (1 + g);
      const beta = 1 / (1 + g);
      const s = this.s;
      const G2 = G * G;
      const G4 = G2 * G2;
      const S = (G2 * G * s[0] + G2 * s[1] + G * s[2] + s[3]) * beta;
      const y4 = (G4 * x + S) / (1 + k * G4);
      let y = Math.tanh(x - k * y4);
      for (let i = 0; i < 4; i++) {
        const v = (y - s[i]) * G;
        const out = v + s[i];
        s[i] = out + v;
        y = out;
      }
      if (!(y === y) || Math.abs(y) > 1e6) {
        this.reset();
        return 0;
      }
      return y;
    }
  }

  const IDLE = 0;
  const ATTACK = 1;
  const DECAY = 2;
  const RELEASE = 3;

  // ADSR with the curved segments of an analog contour generator: the attack
  // charges toward a target above full scale, so it bends like a capacitor.
  class Envelope {
    constructor() {
      this.stage = IDLE;
      this.value = 0;
    }

    gateOn() {
      this.stage = ATTACK;
    }

    gateOff() {
      if (this.stage !== IDLE) this.stage = RELEASE;
    }

    reset() {
      this.stage = IDLE;
      this.value = 0;
    }

    next(c) {
      switch (this.stage) {
        case ATTACK:
          this.value += (1.3 - this.value) * c.att;
          if (this.value >= 1) {
            this.value = 1;
            this.stage = DECAY;
          }
          break;
        case DECAY:
          this.value = c.sus + (this.value - c.sus) * c.dec;
          break;
        case RELEASE:
          this.value *= c.rel;
          if (this.value < 1e-5) {
            this.value = 0;
            this.stage = IDLE;
          }
          break;
        default:
          break;
      }
      return this.value;
    }
  }

  function envCoefs(a, d, s, r, sr) {
    return {
      att: 1 - Math.exp(-1.466 / (Math.max(a, 0.0005) * sr)),
      dec: Math.exp(-4.6 / (Math.max(d, 0.001) * sr)),
      sus: s,
      rel: Math.exp(-4.6 / (Math.max(r, 0.001) * sr)),
    };
  }

  // The order an arpeggiator walks through the held notes.
  // notes: in the order they were played. Returns the full cycle.
  function arpSequence(notes, mode, octaves) {
    if (!notes.length) return [];
    const base = mode === 'order' ? notes.slice() : notes.slice().sort((a, b) => a - b);
    const up = [];
    for (let o = 0; o < Math.max(1, octaves); o++) {
      for (const n of base) up.push(n + 12 * o);
    }
    if (mode === 'down') return up.reverse();
    if (mode === 'updown') {
      if (up.length < 3) return up;
      return up.concat(up.slice(1, -1).reverse());
    }
    return up; // 'up', 'order', and the pool 'random' draws from
  }

  // Note lengths in beats (quarter notes), for everything that follows the tempo:
  // the arpeggiator, a synced LFO, and the page's synced echo.
  const BEATS = {
    '4 bars': 16, '2 bars': 8, '1 bar': 4,
    '1/2': 2, '1/4.': 1.5, '1/4': 1, '1/4T': 2 / 3,
    '1/8.': 0.75, '1/8': 0.5, '1/8T': 1 / 3,
    '1/16.': 0.375, '1/16': 0.25, '1/16T': 1 / 6, '1/32': 0.125,
  };

  class Voice {
    constructor(index) {
      this.index = index;
      this.osc = [new Oscillator(), new Oscillator(), new Oscillator()];
      this.filter = new LadderFilter();
      this.fenv = new Envelope();
      this.aenv = new Envelope();
      this.note = -1;
      this.cur = 60;
      this.target = 60;
      this.vel = 1;
      this.gate = false;
      this.age = 0;
      this.detune = 0;
      this.gl = Math.SQRT1_2;
      this.gr = Math.SQRT1_2;
      this.f = [0, 0, 0];
    }

    get active() {
      return this.aenv.stage !== IDLE;
    }

    setPan(pan) {
      const a = ((pan + 1) * Math.PI) / 4;
      this.gl = Math.cos(a);
      this.gr = Math.sin(a);
    }
  }

  class Synth {
    constructor(sr, params) {
      this.sr = sr;
      this.p = Object.assign({}, params);
      this.voices = [];
      for (let i = 0; i < VOICES; i++) this.voices.push(new Voice(i));
      this.bend = 0;
      this.wheel = 0;
      this.lfoPhase = 0;
      this.lfoOut = 0;
      this.vibrato = 0;
      this.shValue = 0;
      this.noiseMod = 0;
      this.clock = 0;
      this.lastNote = -1;
      this.stack = []; // held notes in mono / legato / unison
      this.physDown = new Set();
      this.arpHeld = [];
      this.arpSeq = [];
      this.arpDirty = true;
      this.arpStep = 0;
      this.arpCounter = 0;
      this.arpGateLeft = 0;
      this.arpNote = -1;
      this.arpVel = 1;
    }

    setParams(next) {
      const p = this.p;
      const modeChanged = 'voiceMode' in next && next.voiceMode !== p.voiceMode;
      const arpChanged = 'arpOn' in next && next.arpOn !== p.arpOn;
      const latchOff = 'latch' in next && !next.latch && p.latch;
      Object.assign(p, next);
      if (modeChanged || arpChanged) this.allNotesOff();
      if (latchOff) {
        this.arpHeld = this.arpHeld.filter((n) => this.physDown.has(n));
        this.arpDirty = true;
      }
      if ('arpMode' in next || 'arpOct' in next) this.arpDirty = true;
    }

    setBend(v) {
      this.bend = Math.max(-1, Math.min(1, v));
    }

    setWheel(v) {
      this.wheel = Math.max(0, Math.min(1, v));
    }

    allNotesOff() {
      for (const v of this.voices) {
        v.gate = false;
        v.note = -1;
        v.fenv.gateOff();
        v.aenv.gateOff();
      }
      this.stack = [];
      this.arpNote = -1;
      if (!this.p.arpOn) {
        this.arpHeld = [];
        this.physDown.clear();
      }
      this.arpDirty = true;
    }

    panic() {
      this.allNotesOff();
      this.arpHeld = [];
      this.physDown.clear();
      for (const v of this.voices) {
        v.fenv.reset();
        v.aenv.reset();
        v.filter.reset();
      }
    }

    noteOn(note, vel) {
      if (vel === undefined) vel = 1;
      if (this.p.latch && this.physDown.size === 0) this.arpHeld = [];
      this.physDown.add(note);
      if (!this.arpHeld.includes(note)) this.arpHeld.push(note);
      this.arpDirty = true;
      if (this.p.arpOn) {
        this.arpVel = vel;
        if (this.arpNote < 0 && this.arpHeld.length === 1) {
          this.arpStep = 0;
          this.arpCounter = 0;
        }
        return;
      }
      this.startNote(note, vel);
    }

    noteOff(note) {
      this.physDown.delete(note);
      if (!this.p.latch || !this.p.arpOn) {
        const i = this.arpHeld.indexOf(note);
        if (i >= 0) this.arpHeld.splice(i, 1);
        this.arpDirty = true;
      }
      if (this.p.arpOn) return;
      this.stopNote(note);
    }

    glideFrom(note) {
      return this.p.glide > 0 && this.lastNote >= 0 ? this.lastNote : note;
    }

    startNote(note, vel) {
      const mode = this.p.voiceMode;
      this.clock++;
      if (mode === 'poly') {
        let v = this.voices.find((x) => x.gate && x.note === note);
        if (!v) v = this.voices.find((x) => !x.active);
        if (!v) v = this.oldest(this.voices.filter((x) => !x.gate)) || this.oldest(this.voices);
        v.cur = this.glideFrom(note);
        v.target = note;
        v.note = note;
        v.vel = vel;
        v.gate = true;
        v.age = this.clock;
        v.detune = 0;
        v.setPan((((v.index * 5) % VOICES) / (VOICES - 1) * 2 - 1) * this.p.spread * 0.6);
        v.fenv.gateOn();
        v.aenv.gateOn();
        this.updatePitch(v, 0);
      } else {
        const i = this.stack.indexOf(note);
        if (i >= 0) this.stack.splice(i, 1);
        this.stack.push(note);
        this.monoTo(note, vel, mode !== 'legato' || !this.voices[0].gate);
      }
      this.lastNote = note;
    }

    stopNote(note) {
      if (this.p.voiceMode === 'poly') {
        for (const v of this.voices) {
          if (v.gate && v.note === note) {
            v.gate = false;
            v.fenv.gateOff();
            v.aenv.gateOff();
          }
        }
        return;
      }
      const i = this.stack.indexOf(note);
      if (i < 0) return;
      const wasTop = i === this.stack.length - 1;
      this.stack.splice(i, 1);
      if (!wasTop) return;
      if (this.stack.length) {
        const back = this.stack[this.stack.length - 1];
        this.monoTo(back, this.voices[0].vel, this.p.voiceMode === 'mono');
        this.lastNote = back;
      } else {
        for (const v of this.monoVoices()) {
          v.gate = false;
          v.fenv.gateOff();
          v.aenv.gateOff();
        }
      }
    }

    monoVoices() {
      return this.p.voiceMode === 'unison' ? this.voices.slice(0, UNISON) : this.voices.slice(0, 1);
    }

    monoTo(note, vel, retrigger) {
      const group = this.monoVoices();
      const n = group.length;
      group.forEach((v, i) => {
        const spread = n > 1 ? (2 * i) / (n - 1) - 1 : 0;
        if (!v.gate) v.cur = this.glideFrom(note);
        v.target = note;
        v.note = note;
        v.vel = vel;
        v.detune = spread * this.p.unisonDetune;
        v.setPan(spread * this.p.spread);
        v.age = this.clock;
        if (retrigger || !v.gate) {
          v.fenv.gateOn();
          v.aenv.gateOn();
        }
        v.gate = true;
        if (this.p.glide <= 0) v.cur = note;
        this.updatePitch(v, 0);
      });
    }

    oldest(list) {
      let best = null;
      for (const v of list) if (!best || v.age < best.age) best = v;
      return best;
    }

    // Glide and the three oscillator frequencies, once per block.
    updatePitch(v, glideCoef) {
      const p = this.p;
      v.cur += (v.target - v.cur) * glideCoef;
      if (Math.abs(v.target - v.cur) < 1e-4) v.cur = v.target;
      const base = v.cur + this.bend * p.bendRange + this.vibrato + v.detune / 100;
      v.f[0] = midiToFreq(base + 12 * p.o1Range);
      v.f[1] = midiToFreq(base + 12 * p.o2Range + p.o2Tune);
      v.f[2] = p.o3KeyOff
        ? midiToFreq(60 + 12 * p.o3Range + p.o3Tune) / 64
        : midiToFreq(base + 12 * p.o3Range + p.o3Tune);
    }

    lfoStep(n) {
      const p = this.p;
      const rate = p.lfoSync ? p.bpm / 60 / (BEATS[p.lfoDiv] || 1) : p.lfoRate;
      let ph = this.lfoPhase + (rate * n) / this.sr;
      if (ph >= 1) {
        ph -= Math.floor(ph);
        this.shValue = Math.random() * 2 - 1;
      }
      this.lfoPhase = ph;
      switch (p.lfoShape) {
        case 'saw':
          this.lfoOut = 2 * ph - 1;
          break;
        case 'square':
          this.lfoOut = ph < 0.5 ? 1 : -1;
          break;
        case 'sh':
          this.lfoOut = this.shValue;
          break;
        default:
          this.lfoOut = 1 - 4 * Math.abs(ph - 0.5);
      }
    }

    arpTick() {
      if (this.arpGateLeft > 0 && --this.arpGateLeft === 0 && this.arpNote >= 0) {
        this.stopNote(this.arpNote);
        this.arpNote = -1;
      }
      if (!this.arpHeld.length) {
        if (this.arpNote >= 0) {
          this.stopNote(this.arpNote);
          this.arpNote = -1;
        }
        this.arpStep = 0;
        this.arpCounter = 0;
        return;
      }
      if (--this.arpCounter > 0) return;
      const p = this.p;
      if (this.arpDirty) {
        this.arpSeq = arpSequence(this.arpHeld, p.arpMode, p.arpOct);
        this.arpDirty = false;
      }
      const seq = this.arpSeq;
      const note =
        p.arpMode === 'random' ? seq[Math.floor(Math.random() * seq.length)] : seq[this.arpStep % seq.length];
      this.arpStep = (this.arpStep + 1) % Math.max(1, seq.length);
      if (this.arpNote >= 0) this.stopNote(this.arpNote);
      this.startNote(note, this.arpVel);
      this.arpNote = note;
      const step = Math.max(1, Math.round((this.sr * 60 * (BEATS[p.arpDiv] || 0.25)) / p.bpm));
      this.arpCounter = step;
      this.arpGateLeft = Math.max(1, Math.round(step * p.arpGate));
      if (p.arpGate >= 1) this.arpGateLeft = 0; // full gate: notes tie into the next
    }

    // Adds n samples of stereo sound into L and R.
    render(L, R, n) {
      const p = this.p;
      const sr = this.sr;
      this.lfoStep(n);
      const wheelMod = Math.min(1, p.modDepth + this.wheel);
      const lfoForPitch = p.modSource === 'lfo' ? this.lfoOut : p.modSource === 'noise' ? this.noiseMod : 0;
      this.vibrato = lfoForPitch * wheelMod * p.modPitch;
      const glideCoef = p.glide > 0 ? 1 - Math.exp(-n / (p.glide * sr * 0.3)) : 1;
      const fc = envCoefs(p.fA, p.fD, p.fS, p.fR, sr);
      const ac = envCoefs(p.aA, p.aD, p.aS, p.aR, sr);
      const k = p.res * 4.15;
      const comp = 1 + p.res * 1.4;
      const drive = 0.6 + p.drive * 5;
      const unisonGain = p.voiceMode === 'unison' ? 0.55 : 1;
      const pwBase = p.pw;
      const w1 = p.o1Wave;
      const w2 = p.o2Wave;
      const w3 = p.o3Wave;
      const kt = p.keyTrack / 12;
      const modF = p.modFilter * wheelMod;
      const modPw = p.modPw * wheelMod;
      const pitchEnv = (p.pitchEnv * LN2) / 12;

      for (const v of this.voices) if (v.active) this.updatePitch(v, glideCoef);

      for (let i = 0; i < n; i++) {
        if (p.arpOn) this.arpTick();
        this.noiseMod = this.noiseMod * 0.999 + (Math.random() * 2 - 1) * 0.04;
        let l = 0;
        let r = 0;
        for (let j = 0; j < VOICES; j++) {
          const v = this.voices[j];
          if (v.aenv.stage === IDLE) continue;
          const mod =
            p.modSource === 'osc3' ? v.osc[2].out : p.modSource === 'noise' ? this.noiseMod * 4 : this.lfoOut;
          const pw = Math.min(0.95, Math.max(0.05, pwBase + mod * modPw));
          const fe = v.fenv.next(fc);
          const ae = v.aenv.next(ac);
          // The filter contour also bends the pitch: the drop that makes a kick or a zap.
          const bendUp = pitchEnv ? Math.exp(pitchEnv * fe) : 1;
          const o1 = v.osc[0].next(v.f[0] * bendUp, sr, w1, pw);
          const o2 = v.osc[1].next(v.f[1] * bendUp, sr, w2, pw);
          const o3 = v.osc[2].next(p.o3KeyOff ? v.f[2] : v.f[2] * bendUp, sr, w3, pw);
          const nz = Math.random() * 2 - 1;
          const mix = (o1 * p.m1 + o2 * p.m2 + o3 * p.m3 + nz * p.noise) * 0.45 * drive;
          const velF = 1 - p.velFilter + p.velFilter * v.vel;
          const oct = p.fEnvAmt * fe * velF + kt * (v.cur - 60) + modF * mod;
          const cutoff = p.cutoff * Math.exp(oct * LN2);
          const y = v.filter.process(mix, cutoff, k, sr) * comp;
          const out = y * ae * (1 - p.velAmp + p.velAmp * v.vel) * unisonGain;
          l += out * v.gl;
          r += out * v.gr;
        }
        L[i] += l * 0.32;
        R[i] += r * 0.32;
      }
    }

    // One message from the page: a note, a wheel, new settings.
    handle(m) {
      switch (m.type) {
        case 'params':
          this.setParams(m.params);
          break;
        case 'on':
          this.noteOn(m.note, m.vel);
          break;
        case 'off':
          this.noteOff(m.note);
          break;
        case 'bend':
          this.setBend(m.value);
          break;
        case 'wheel':
          this.setWheel(m.value);
          break;
        case 'panic':
          this.panic();
          break;
        default:
          break;
      }
    }

    activeMask() {
      let m = 0;
      for (const v of this.voices) if (v.active) m |= 1 << v.index;
      return m;
    }
  }

  if (typeof registerProcessor === 'function') {
    class LadderProcessor extends AudioWorkletProcessor {
      constructor(options) {
        super();
        this.synth = new Synth(sampleRate, options.processorOptions.params);
        this.sinceReport = 0;
        this.lastMask = -1;
        this.port.onmessage = (e) => this.synth.handle(e.data);
      }

      process(inputs, outputs) {
        const out = outputs[0];
        const L = out[0];
        const R = out[1];
        if (!R) {
          const scratch = new Float32Array(L.length);
          this.synth.render(L, scratch, L.length);
          for (let i = 0; i < L.length; i++) L[i] = (L[i] + scratch[i]) * 0.5;
        } else {
          this.synth.render(L, R, L.length);
        }
        this.sinceReport += L.length;
        if (this.sinceReport > 2048) {
          this.sinceReport = 0;
          const mask = this.synth.activeMask();
          if (mask !== this.lastMask) {
            this.lastMask = mask;
            this.port.postMessage({ type: 'voices', mask });
          }
        }
        return true;
      }
    }
    registerProcessor('ladder-24', LadderProcessor);
  }

  const api = {
    VOICES,
    UNISON,
    BEATS,
    midiToFreq,
    polyBlep,
    Oscillator,
    LadderFilter,
    Envelope,
    envCoefs,
    arpSequence,
    Synth,
  };
  // The page hands this same source to the AudioWorklet as a blob, so the
  // engine needs no second file and works when opened straight from disk.
  api.source = '(' + ladderDsp.toString() + ')(globalThis);';
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.LadderDSP = api;
})(typeof globalThis !== 'undefined' ? globalThis : this);
