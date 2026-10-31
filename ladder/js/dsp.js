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
  const DRUM_VOICES = 6; // the drum grid has its own, so beats never steal a melody's voices
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
      this.wrap = -1; // where this sample a new cycle began (0..1 of the step), or -1
    }

    // Hard sync: restart the cycle at the moment the master oscillator did.
    syncTo(frac, freq, sr) {
      this.phase = frac * Math.min(Math.abs(freq) / sr, 0.5);
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
      this.wrap = -1;
      if (p >= 1) {
        p -= 1;
        this.wrap = dt > 0 ? p / dt : 0;
      }
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
      this.out = [0, 0, 0, 0];
    }

    reset() {
      this.s[0] = this.s[1] = this.s[2] = this.s[3] = 0;
    }

    // k is the feedback amount: 0 = none, about 4 = self-oscillation.
    // mode picks the response by mixing the four stages, as the classic
    // multimode ladders do: low-pass 24 or 12 dB, band-pass, high-pass 12 or 24 dB.
    process(x, cutoff, k, sr, mode) {
      const fc = Math.min(Math.max(cutoff, 8), sr * 0.45);
      const g = Math.tan((Math.PI * fc) / sr);
      const G = g / (1 + g);
      const beta = 1 / (1 + g);
      const s = this.s;
      const G2 = G * G;
      const G4 = G2 * G2;
      const S = (G2 * G * s[0] + G2 * s[1] + G * s[2] + s[3]) * beta;
      const y4 = (G4 * x + S) / (1 + k * G4);
      const u = Math.tanh(x - k * y4);
      let y = u;
      const st = this.out;
      for (let i = 0; i < 4; i++) {
        const v = (y - s[i]) * G;
        const out = v + s[i];
        s[i] = out + v;
        y = out;
        st[i] = out;
      }
      if (!(y === y) || Math.abs(y) > 1e6) {
        this.reset();
        return 0;
      }
      switch (mode) {
        case 'lp12':
          return st[1];
        case 'bp':
          return 2 * (st[0] - st[1]);
        case 'hp12':
          return u - 2 * st[0] + st[1];
        case 'hp24':
          return u - 4 * st[0] + 6 * st[1] - 4 * st[2] + st[3];
        default:
          return y;
      }
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

  // A sound's settings that hold still for a block, worked out once. Every
  // voice plays from one of these: the panel's, or its own drum's in a kit.
  function soundConsts(p, sr) {
    return {
      p,
      fc: envCoefs(p.fA, p.fD, p.fS, p.fR, sr),
      ac: envCoefs(p.aA, p.aD, p.aS, p.aR, sr),
      k: p.res * 4.15,
      comp: 1 + p.res * 1.4,
      drive: 0.6 + p.drive * 5,
      kt: p.keyTrack / 12,
      pitchEnv: (p.pitchEnv * LN2) / 12,
      mods: modRoutes(p),
    };
  }

  // The modulation matrix ("patch cables"): up to four routes from a source to
  // a destination, each with a signed amount. Codes keep the per-sample loop cheap.
  const MOD_SOURCES = ['off', 'lfo', 'osc3', 'fenv', 'aenv', 'vel', 'wheel', 'key', 'noise', 'random'];
  const MOD_DESTS = ['pitch', 'osc2', 'cutoff', 'res', 'pw', 'fm', 'fold', 'level'];
  // How far a full-amount route moves each destination.
  const MOD_SCALE = [12, 12, 5, 1, 0.45, 1, 1, 1];
  const MOD_SLOTS = 4;

  function modRoutes(p) {
    const out = [];
    for (let i = 1; i <= MOD_SLOTS; i++) {
      const src = MOD_SOURCES.indexOf(p['mod' + i + 'Src']);
      const dst = MOD_DESTS.indexOf(p['mod' + i + 'Dst']);
      const amt = p['mod' + i + 'Amt'] || 0;
      if (src > 0 && dst >= 0 && amt !== 0) out.push({ src, dst, amt: amt * MOD_SCALE[dst] });
    }
    return out;
  }

  // A drum kit spans 24 keys from C3 and repeats every two octaves.
  const KIT_BASE = 48;
  const KIT_KEYS = 24;
  const kitSlot = (note) => (((note - KIT_BASE) % KIT_KEYS) + KIT_KEYS) % KIT_KEYS;

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
      this.drum = null; // the kit drum this voice is playing, if any
      this.rand = 0; // a random value picked at each note, for the Random mod source
      this.level = 1; // the theremin pad's volume; 1 for everything else
      this.lvl = 1;
      this.theremin = false;
      this.md = new Float64Array(MOD_DESTS.length);
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
      this.drumVoices = [];
      for (let i = 0; i < DRUM_VOICES; i++) this.drumVoices.push(new Voice(VOICES + i));
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
      this.kit = null;
      this.wheelMod = 0;
      this.thereminVoice = null;
      // Step sequencer and drum grid, sharing one clock.
      this.seqRunning = false;
      this.seqPattern = { length: 16, steps: [], swing: 0, gate: 0.5 };
      this.drumPattern = { length: 16, rows: [] };
      this.drumKit = null;
      this.seqPos = 0;
      this.seqCounter = 0;
      this.seqGateLeft = 0;
      this.seqNote = -1;
      this.seqSlid = false;
      this.stepNow = -1;
    }

    // The drums of a kit, one per key: { slot: { params, play, pan, choke } }.
    makeKit(kit) {
      const out = {};
      const sr = this.sr;
      for (const slot of Object.keys(kit || {})) {
        const d = kit[slot];
        const c = soundConsts(d.params, sr);
        // Choked: cut off fast, like a closed hi-hat stopping an open one.
        const choked = Object.assign({}, c, { ac: Object.assign({}, c.ac, { rel: Math.exp(-4.6 / (0.012 * sr)) }) });
        out[slot] = { c, choked, play: d.play, pan: d.pan || 0, choke: d.choke || null, gain: d.gain || 1 };
      }
      return out;
    }

    setKit(kit) {
      this.kit = this.makeKit(kit);
    }

    kitHit(note, vel) {
      this.hitDrum(this.kit[kitSlot(note)], this.voices, note, vel);
    }

    hitDrum(d, pool, note, vel) {
      if (!d) return;
      if (d.choke) {
        for (const v of pool) {
          if (v.active && v.drum && v.drum.choke === d.choke) {
            v.kc = v.drum.choked;
            v.fenv.gateOff();
            v.aenv.gateOff();
          }
        }
      }
      const v = pool.find((x) => !x.active) || this.oldest(pool);
      this.freshVoice(v);
      v.drum = d;
      v.kc = d.c;
      v.cur = v.target = d.play;
      v.note = note;
      v.vel = vel;
      v.gate = true;
      v.age = this.clock;
      v.detune = 0;
      v.setPan(d.pan);
      v.fenv.gateOn();
      v.aenv.gateOn();
      this.updatePitch(v, 0);
    }

    setParams(next) {
      const p = this.p;
      const modeChanged =
        ('voiceMode' in next && next.voiceMode !== p.voiceMode) || ('kit' in next && next.kit !== p.kit);
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
      if (this.p.kit && this.kit) {
        this.kitHit(note, vel);
        return;
      }
      if (mode === 'poly') {
        let v = this.voices.find((x) => x.gate && x.note === note);
        if (!v) v = this.voices.find((x) => !x.active);
        if (!v) v = this.oldest(this.voices.filter((x) => !x.gate)) || this.oldest(this.voices);
        this.freshVoice(v);
        v.drum = null;
        v.kc = null;
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
      if (this.p.voiceMode === 'poly' || (this.p.kit && this.kit)) {
        for (const v of this.voices) {
          if (v.gate && v.note === note) {
            v.gate = false;
            if (v.drum) continue; // a drum is a one-shot: it rings out on its own
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
        if (v.theremin) this.freshVoice(v);
        v.drum = null;
        v.kc = null;
        if (!v.gate) {
          v.cur = this.glideFrom(note);
          v.rand = Math.random() * 2 - 1;
        }
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

    freshVoice(v) {
      v.theremin = false;
      v.level = 1;
      v.lvl = 1;
      v.rand = Math.random() * 2 - 1;
      if (this.thereminVoice === v) this.thereminVoice = null;
    }

    // The theremin pad: one voice whose pitch and volume follow the pointer
    // smoothly, for as long as it is held. note may fall between keys.
    theremin(on, note, level) {
      let v = this.thereminVoice;
      if (!on) {
        if (v) {
          v.gate = false;
          v.fenv.gateOff();
          v.aenv.gateOff();
          this.thereminVoice = null;
        }
        return;
      }
      if (!v) {
        v = this.voices.find((x) => !x.active) || this.oldest(this.voices);
        this.freshVoice(v);
        v.drum = null;
        v.kc = null;
        v.cur = note;
        v.detune = 0;
        v.vel = 1;
        v.note = -2;
        v.lvl = level;
        v.theremin = true;
        v.gate = true;
        v.age = ++this.clock;
        v.setPan(0);
        v.fenv.gateOn();
        v.aenv.gateOn();
        this.thereminVoice = v;
      }
      v.target = note;
      v.level = Math.max(0, Math.min(1, level));
      this.updatePitch(v, 0);
    }

    // ---- step sequencer and drum grid

    setTransport(running) {
      if (running && !this.seqRunning) {
        this.seqPos = 0;
        this.seqCounter = 0;
      }
      if (!running && this.seqNote >= 0) {
        this.stopNote(this.seqNote);
        this.seqNote = -1;
      }
      this.seqRunning = running;
      if (!running) this.stepNow = -1;
    }

    seqTick() {
      if (this.seqGateLeft > 0 && --this.seqGateLeft === 0 && this.seqNote >= 0) {
        this.stopNote(this.seqNote);
        this.seqNote = -1;
      }
      if (--this.seqCounter > 0) return;
      const mel = this.seqPattern;
      const pos = this.seqPos;
      // Sixteenth notes at the panel tempo; swing pushes every second one late.
      const base = (this.sr * 60 * 0.25) / this.p.bpm;
      const swing = mel.swing || 0;
      const dur = Math.max(1, Math.round(base * (pos % 2 === 0 ? 1 + swing : 1 - swing)));
      this.seqCounter = dur;

      const st = mel.steps.length ? mel.steps[pos % mel.length] : null;
      const prev = this.seqNote;
      if (st && st.note !== null && st.note !== undefined) {
        if (!(prev === st.note && this.seqSlid)) {
          this.startNote(st.note, st.accent ? 1 : 0.75);
          // Starting the new note before letting go of a slid one makes a mono sound glide.
          if (prev >= 0 && prev !== st.note) this.stopNote(prev);
        }
        this.seqNote = st.note;
        this.seqSlid = !!st.slide;
        this.seqGateLeft = st.slide ? 0 : Math.max(1, Math.round(dur * (mel.gate || 0.5)));
      } else if (prev >= 0) {
        this.stopNote(prev);
        this.seqNote = -1;
        this.seqSlid = false;
      }

      const dr = this.drumPattern;
      if (this.drumKit && dr.rows.length) {
        for (const row of dr.rows) {
          const hit = row.steps[pos % dr.length];
          if (hit) this.hitDrum(this.drumKit[row.slot], this.drumVoices, -3, hit === 2 ? 1 : 0.7);
        }
      }
      this.stepNow = pos;
      this.seqPos = pos + 1;
    }

    oldest(list) {
      let best = null;
      for (const v of list) if (!best || v.age < best.age) best = v;
      return best;
    }

    // Glide and the three oscillator frequencies, once per block.
    updatePitch(v, glideCoef) {
      const p = v.kc ? v.kc.p : this.p;
      v.cur += (v.target - v.cur) * glideCoef;
      if (Math.abs(v.target - v.cur) < 1e-4) v.cur = v.target;
      const base = v.cur + this.bend * this.p.bendRange + (v.kc ? 0 : this.vibrato) + v.detune / 100;
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
      const panel = soundConsts(p, sr);
      const unisonGain = p.voiceMode === 'unison' ? 0.55 : 1;

      for (const v of this.voices) if (v.active) this.updatePitch(v, v.theremin ? 0.35 : glideCoef);
      for (const v of this.drumVoices) if (v.active) this.updatePitch(v, 1);
      this.wheelMod = wheelMod;

      for (let i = 0; i < n; i++) {
        if (p.arpOn) this.arpTick();
        if (this.seqRunning) this.seqTick();
        this.noiseMod = this.noiseMod * 0.999 + (Math.random() * 2 - 1) * 0.04;
        let l = 0;
        let r = 0;
        for (let j = 0; j < VOICES; j++) {
          const v = this.voices[j];
          if (v.aenv.stage === IDLE) continue;
          const out = this.voiceSample(v, v.kc || panel, unisonGain);
          l += out * v.gl;
          r += out * v.gr;
        }
        for (let j = 0; j < DRUM_VOICES; j++) {
          const v = this.drumVoices[j];
          if (v.aenv.stage === IDLE) continue;
          const out = this.voiceSample(v, v.kc, 1);
          l += out * v.gl;
          r += out * v.gr;
        }
        L[i] += l * 0.32;
        R[i] += r * 0.32;
      }
    }

    // One sample of one voice, before panning.
    voiceSample(v, c, unisonGain) {
      const q = c.p;
      const sr = this.sr;
      const wheelMod = this.wheelMod;
      const fe = v.fenv.next(c.fc);
      const ae = v.aenv.next(c.ac);
      const mod = q.modSource === 'osc3' ? v.osc[2].out : q.modSource === 'noise' ? this.noiseMod * 4 : this.lfoOut;

      // The modulation matrix: each route adds its source, scaled, to a destination.
      const md = v.md;
      md.fill(0);
      for (const m of c.mods) {
        let x;
        switch (m.src) {
          case 1: x = this.lfoOut; break;
          case 2: x = v.osc[2].out; break;
          case 3: x = fe; break;
          case 4: x = ae; break;
          case 5: x = v.vel; break;
          case 6: x = this.wheel; break;
          case 7: x = (v.cur - 60) / 24; break;
          case 8: x = this.noiseMod * 4; break;
          default: x = v.rand;
        }
        md[m.dst] += x * m.amt;
      }

      const pw = Math.min(0.95, Math.max(0.05, q.pw + mod * q.modPw * wheelMod + md[4]));
      // The filter contour also bends the pitch: the drop that makes a kick or a zap.
      const bend = (c.pitchEnv ? Math.exp(c.pitchEnv * fe) : 1) * (md[0] ? Math.exp((md[0] / 12) * LN2) : 1);
      const o3 = v.osc[2].next(q.o3KeyOff ? v.f[2] : v.f[2] * bend, sr, q.o3Wave, pw);
      // FM: oscillator 3 swings oscillator 1's frequency, for bells, clangs and growls.
      const fm = Math.max(0, q.fm + md[5]);
      const o1 = v.osc[0].next(v.f[0] * bend * (fm ? 1 + 3 * fm * o3 : 1), sr, q.o1Wave, pw);
      // Hard sync: oscillator 2 restarts every time oscillator 1 does, for the tearing sync lead.
      const f2 = v.f[1] * bend * (md[1] ? Math.exp((md[1] / 12) * LN2) : 1);
      if (q.sync && v.osc[0].wrap >= 0) v.osc[1].syncTo(v.osc[0].wrap, f2, sr);
      const o2 = v.osc[1].next(f2, sr, q.o2Wave, pw);
      const nz = Math.random() * 2 - 1;
      let mix = (o1 * q.m1 + o2 * q.m2 + o3 * q.m3 + nz * q.noise) * 0.45 * c.drive;
      // Wavefolder: past full scale the wave folds back on itself, adding bright overtones.
      const fold = Math.min(1, Math.max(0, q.fold + md[6]));
      if (fold > 0) mix = Math.sin(mix * (1 + fold * 6) * 1.2) * (0.8 + 0.2 * (1 - fold));
      const velF = 1 - q.velFilter + q.velFilter * v.vel;
      const oct = q.fEnvAmt * fe * velF + c.kt * (v.cur - 60) + q.modFilter * wheelMod * mod + md[2];
      const cutoff = q.cutoff * Math.exp(oct * LN2);
      let k = c.k;
      let comp = c.comp;
      if (md[3]) {
        const res = Math.min(1, Math.max(0, q.res + md[3]));
        k = res * 4.15;
        comp = 1 + res * 1.4;
      }
      const y = v.filter.process(mix, cutoff, k, sr, q.filterMode) * comp;
      v.lvl += (v.level - v.lvl) * 0.003;
      const level = Math.max(0, 1 + md[7]) * v.lvl;
      return y * ae * (1 - q.velAmp + q.velAmp * v.vel) * (v.drum ? v.drum.gain : unisonGain) * level;
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
        case 'kit':
          this.setKit(m.kit);
          break;
        case 'theremin':
          this.theremin(m.on, m.note, m.level);
          break;
        case 'transport':
          this.setTransport(!!m.running);
          break;
        case 'pattern':
          this.seqPattern = m.pattern;
          break;
        case 'drums':
          this.drumPattern = m.drums;
          break;
        case 'drumkit':
          this.drumKit = this.makeKit(m.kit);
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
        this.lastStep = -1;
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
        if (this.synth.stepNow !== this.lastStep) {
          this.lastStep = this.synth.stepNow;
          this.port.postMessage({ type: 'step', step: this.lastStep });
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

    // The Record button: passes the finished sound straight through and, while
    // recording, sends a copy of every block to the page to be saved as a WAV.
    class RecorderProcessor extends AudioWorkletProcessor {
      constructor() {
        super();
        this.recording = false;
        this.port.onmessage = (e) => (this.recording = !!e.data.recording);
      }

      process(inputs, outputs) {
        const input = inputs[0];
        const output = outputs[0];
        for (let ch = 0; ch < output.length; ch++) {
          if (input[ch]) output[ch].set(input[ch]);
        }
        if (this.recording && input.length) {
          const l = new Float32Array(input[0]);
          const r = new Float32Array(input[1] || input[0]);
          this.port.postMessage({ l, r }, [l.buffer, r.buffer]);
        }
        return true;
      }
    }
    registerProcessor('ladder-recorder', RecorderProcessor);
  }

  // A spring reverb's impulse response, made rather than recorded: springs
  // carry high frequencies faster than low ones, so every echo arrives as a
  // falling "boing" chirp, repeating each time the wave crosses the spring and
  // back, inside a darker diffuse wash.
  function springImpulse(sr, seconds) {
    const n = Math.max(1, Math.round(sr * seconds));
    const out = [new Float32Array(n), new Float32Array(n)];
    const transit = [0.031, 0.037]; // two springs of different length, one per side
    for (let ch = 0; ch < 2; ch++) {
      const d = out[ch];
      let seed = ch ? 0x9e3779b9 : 0x7f4a7c15;
      const rnd = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296) * 2 - 1;
      // Diffuse wash: noise, smoothed, dying away exponentially.
      let lp = 0;
      for (let i = 0; i < n; i++) {
        lp += (rnd() - lp) * 0.25;
        d[i] = lp * 0.25 * Math.exp((-5 * i) / n);
      }
      // The chirps: 4 kHz falling to 300 Hz over 25 ms, at every transit.
      const chirpLen = Math.round(0.025 * sr);
      for (let echo = 0, t0 = Math.round(transit[ch] * sr * 0.5); t0 < n; echo++, t0 += Math.round(transit[ch] * sr)) {
        const amp = 0.9 * Math.exp((-4.5 * t0) / n) * (echo % 2 ? -1 : 1);
        let phase = 0;
        for (let i = 0; i < chirpLen && t0 + i < n; i++) {
          const f = 4000 * Math.pow(300 / 4000, i / chirpLen);
          phase += (2 * Math.PI * f) / sr;
          d[t0 + i] += amp * Math.sin(phase) * Math.sin((Math.PI * i) / chirpLen);
        }
      }
    }
    return out;
  }

  const api = {
    springImpulse,
    VOICES,
    DRUM_VOICES,
    UNISON,
    MOD_SOURCES,
    MOD_DESTS,
    MOD_SLOTS,
    BEATS,
    KIT_BASE,
    KIT_KEYS,
    kitSlot,
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
