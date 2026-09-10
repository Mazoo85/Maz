/*
 * theory.js — deterministic randomness + the music-theory primitives the
 * composer builds on: scales, diatonic chord construction, voice leading,
 * and human-readable chord names.
 *
 * Everything here is pure: same seed in, same music out. That is what makes a
 * song shareable by its seed alone.
 */
(function (global) {
  'use strict';

  /* ------------------------------------------------------------------ *
   * Seeded RNG
   * ------------------------------------------------------------------ */

  function hashSeed(str) {
    let h = 2166136261 >>> 0;
    for (let i = 0; i < str.length; i++) {
      h ^= str.charCodeAt(i);
      h = Math.imul(h, 16777619);
    }
    return h >>> 0;
  }

  function mulberry32(a) {
    return function () {
      a = (a + 0x6d2b79f5) | 0;
      let t = Math.imul(a ^ (a >>> 15), 1 | a);
      t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }

  function Rng(seed) {
    this.seed = String(seed);
    this._next = mulberry32(hashSeed(this.seed));
  }
  Rng.prototype.next = function () { return this._next(); };
  Rng.prototype.int = function (n) { return Math.floor(this._next() * n); };
  Rng.prototype.intRange = function (a, b) { return a + Math.floor(this._next() * (b - a + 1)); };
  Rng.prototype.range = function (a, b) { return a + this._next() * (b - a); };
  Rng.prototype.pick = function (arr) { return arr[Math.floor(this._next() * arr.length)]; };
  Rng.prototype.chance = function (p) { return this._next() < p; };
  Rng.prototype.shuffle = function (arr) {
    const a = arr.slice();
    for (let i = a.length - 1; i > 0; i--) {
      const j = Math.floor(this._next() * (i + 1));
      const t = a[i]; a[i] = a[j]; a[j] = t;
    }
    return a;
  };
  /** items: [[value, weight], ...] */
  Rng.prototype.weighted = function (items) {
    let total = 0;
    for (let i = 0; i < items.length; i++) total += items[i][1];
    let r = this._next() * total;
    for (let i = 0; i < items.length; i++) {
      r -= items[i][1];
      if (r <= 0) return items[i][0];
    }
    return items[items.length - 1][0];
  };

  /** A short, pronounceable seed like "VELVET-7318". */
  const SEED_WORDS = [
    'NEON', 'VELVET', 'MIDNIGHT', 'GLASS', 'AMBER', 'STATIC', 'CHROME', 'HOLLOW',
    'SUNKEN', 'PAPER', 'ORBIT', 'FROST', 'EMBER', 'HAZE', 'CIPHER', 'DRIFT',
    'LANTERN', 'MARBLE', 'RIPTIDE', 'SABLE', 'TUNDRA', 'VAPOR', 'WILLOW', 'ZENITH',
    'COBALT', 'DUSK', 'ECHO', 'FERN', 'GRAVITY', 'HALCYON', 'INDIGO', 'JUNIPER'
  ];
  function randomSeed() {
    const w = SEED_WORDS[Math.floor(Math.random() * SEED_WORDS.length)];
    return w + '-' + (1000 + Math.floor(Math.random() * 9000));
  }

  /* ------------------------------------------------------------------ *
   * Notes & scales
   * ------------------------------------------------------------------ */

  const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

  const SCALES = {
    major:        { name: 'Major',            steps: [0, 2, 4, 5, 7, 9, 11], minorish: false },
    minor:        { name: 'Natural Minor',    steps: [0, 2, 3, 5, 7, 8, 10], minorish: true },
    dorian:       { name: 'Dorian',           steps: [0, 2, 3, 5, 7, 9, 10], minorish: true },
    phrygian:     { name: 'Phrygian',         steps: [0, 1, 3, 5, 7, 8, 10], minorish: true },
    lydian:       { name: 'Lydian',           steps: [0, 2, 4, 6, 7, 9, 11], minorish: false },
    mixolydian:   { name: 'Mixolydian',       steps: [0, 2, 4, 5, 7, 9, 10], minorish: false },
    harmonicMinor:{ name: 'Harmonic Minor',   steps: [0, 2, 3, 5, 7, 8, 11], minorish: true },
    aeolianPent:  { name: 'Minor Pentatonic', steps: [0, 3, 5, 7, 10],       minorish: true },
    majorPent:    { name: 'Major Pentatonic', steps: [0, 2, 4, 7, 9],        minorish: false }
  };

  /** MIDI note number for a pitch class in a given octave (C4 = 60). */
  function midi(pc, octave) { return 12 * (octave + 1) + pc; }

  function midiToName(m) {
    return NOTE_NAMES[((m % 12) + 12) % 12] + (Math.floor(m / 12) - 1);
  }

  function midiToFreq(m) { return 440 * Math.pow(2, (m - 69) / 12); }

  /**
   * Pitch of a scale degree, where degree can run past the end of the scale
   * (degree 7 in a 7-note scale is the root an octave up).
   */
  function degreePitch(scaleSteps, rootMidi, degree) {
    const n = scaleSteps.length;
    const oct = Math.floor(degree / n);
    const idx = ((degree % n) + n) % n;
    return rootMidi + scaleSteps[idx] + 12 * oct;
  }

  /** Snap an arbitrary pitch to the nearest note in the scale. */
  function snapToScale(pitch, scaleSteps, rootMidi) {
    const rel = pitch - rootMidi;
    const oct = Math.floor(rel / 12);
    const pc = ((rel % 12) + 12) % 12;
    let best = scaleSteps[0], bestD = 99;
    for (let i = 0; i < scaleSteps.length; i++) {
      const d = Math.abs(scaleSteps[i] - pc);
      if (d < bestD) { bestD = d; best = scaleSteps[i]; }
    }
    return rootMidi + oct * 12 + best;
  }

  /* ------------------------------------------------------------------ *
   * Chords
   * ------------------------------------------------------------------ */

  const CHORD_SHAPES = {
    triad:   [0, 2, 4],
    sixth:   [0, 2, 4, 5],
    seventh: [0, 2, 4, 6],
    ninth:   [0, 2, 4, 6, 8],
    sus2:    [0, 1, 4],
    sus4:    [0, 3, 4],
    power:   [0, 4]
  };

  /**
   * Build a chord by stacking scale degrees. Returns MIDI pitches starting at
   * the chord root in the octave of `rootMidi`.
   */
  function buildChord(scaleSteps, rootMidi, degree, shape) {
    const offsets = CHORD_SHAPES[shape] || CHORD_SHAPES.triad;
    return offsets.map(function (o) { return degreePitch(scaleSteps, rootMidi, degree + o); });
  }

  /**
   * Soften the two chords that fall out of a scale but rarely sound good bare.
   * An augmented triad has its fifth pulled back to natural; a bare diminished
   * triad gains a minor seventh, turning it half-diminished — a far easier
   * sound to sit on for a whole bar.
   */
  function sweetenChord(pitches) {
    const out = pitches.slice();
    // Only bare triads are touched. Four-note diatonic chords are already
    // coherent, and "fixing" them turns sixths into clusters.
    if (out.length !== 3) return out;
    const root = out[0];
    const iv = function (p) { return ((p - root) % 12 + 12) % 12; };
    const a = iv(out[1]), b = iv(out[2]);
    const isAug = a === 4 && b === 8;
    const isDim = a === 3 && b === 6;
    if (isAug) out[2] -= 1;             // augmented triad → plain major
    if (isDim) out.push(root + 10);     // diminished triad → half-diminished
    return out;
  }

  /**
   * Reject chords that contradict themselves. Stacking scale degrees is a
   * blunt instrument: on some degrees of some modes it produces a root with
   * both a flat and a natural fifth, or a flat ninth grinding against the
   * root. Those are not colour, they are mistakes.
   */
  function chordIsSound(pitches) {
    const root = pitches[0];
    const set = {};
    for (let i = 0; i < pitches.length; i++) set[((pitches[i] - root) % 12 + 12) % 12] = true;
    if (set[6] && set[7]) return false;     // flat and natural fifth together
    if (set[3] && set[4]) return false;     // both thirds
    if (set[10] && set[11]) return false;   // both sevenths
    if (set[1]) return false;               // flat ninth against the root
    if (set[6] && set[8]) return false;     // flat fifth under an added sixth
    return true;
  }

  /** Name a chord from its actual pitches, e.g. "Am7", "Fmaj7", "Bm7♭5". */
  function chordName(pitches) {
    const root = pitches[0];
    const set = {};
    for (let i = 0; i < pitches.length; i++) set[((pitches[i] - root) % 12 + 12) % 12] = true;
    const has = function (n) { return !!set[n]; };
    const letter = NOTE_NAMES[((root % 12) + 12) % 12];

    const third = has(3) ? 'm' : has(4) ? 'M' : has(2) ? 'sus2' : has(5) ? 'sus4' : null;
    const fifth = has(7) ? 'P' : has(6) ? 'd' : has(8) ? 'A' : null;
    const seventh = has(11) ? 'M' : has(10) ? 'm' : null;

    // Chords whose colour comes from an altered fifth get their own names.
    if (third === 'm' && fifth === 'd') {
      if (seventh === 'm') return letter + 'm7♭5';
      if (has(9)) return letter + 'dim7';
      return letter + 'dim';
    }
    if (third === 'M' && fifth === 'A') return letter + 'aug' + (seventh === 'm' ? '7' : '');

    const base = third === 'm' ? 'm' : third === 'M' ? '' : third === null ? '5' : third;
    const ninth = has(2) && third !== 'sus2';

    let ext = '';
    // A minor triad with a major seventh is bracketed, so it reads Am(maj7)
    // rather than the unreadable "Ammaj7".
    if (seventh === 'M') ext = base === 'm' ? (ninth ? '(maj9)' : '(maj7)') : (ninth ? 'maj9' : 'maj7');
    else if (seventh === 'm') ext = ninth ? '9' : '7';
    else if (has(9)) ext = '6';

    return letter + base + ext;
  }

  /** Roman numeral for a scale degree, cased by the chord's own third. */
  const ROMAN = ['I', 'II', 'III', 'IV', 'V', 'VI', 'VII'];
  function romanNumeral(scaleSteps, degree, pitches) {
    const n = scaleSteps.length;
    const base = ROMAN[((degree % n) + n) % n] || 'I';
    const third = ((pitches[1] - pitches[0]) % 12 + 12) % 12;
    const minorish = third === 3;
    let r = minorish ? base.toLowerCase() : base;
    const fifth = pitches.length > 2 ? ((pitches[2] - pitches[0]) % 12 + 12) % 12 : 7;
    if (minorish && fifth === 6) r += '°';
    return r;
  }

  /**
   * Voice a chord in a comfortable range, moving as little as possible from the
   * previous voicing. This is what stops the chord track sounding like it is
   * jumping around at random.
   */
  function voiceChord(pitches, prev, low, high) {
    low = low || 55; high = high || 81;
    const pcs = pitches.map(function (p) { return ((p % 12) + 12) % 12; });
    const targets = [];
    const center = (low + high) / 2;
    for (let i = 0; i < pcs.length; i++) {
      if (prev && prev.length) targets.push(prev[Math.min(i, prev.length - 1)]);
      else targets.push(center - 6 + i * 4);
    }
    const used = {};
    const out = [];
    for (let i = 0; i < pcs.length; i++) {
      let best = null, bestD = 1e9;
      for (let p = low; p <= high; p++) {
        if (((p % 12) + 12) % 12 !== pcs[i]) continue;
        if (used[p]) continue;
        const d = Math.abs(p - targets[i]);
        if (d < bestD) { bestD = d; best = p; }
      }
      if (best === null) best = pcs[i] + 12 * (Math.floor(center / 12));
      used[best] = true;
      out.push(best);
    }
    out.sort(function (a, b) { return a - b; });
    return out;
  }

  /** Nearest chord tone to a pitch (used to keep melodies consonant). */
  function nearestChordTone(pitch, chordPitches) {
    let best = chordPitches[0], bestD = 1e9;
    const pcs = chordPitches.map(function (p) { return ((p % 12) + 12) % 12; });
    for (let oct = -2; oct <= 3; oct++) {
      for (let i = 0; i < pcs.length; i++) {
        const cand = pcs[i] + 12 * (Math.floor(pitch / 12) + oct);
        const d = Math.abs(cand - pitch);
        if (d < bestD) { bestD = d; best = cand; }
      }
    }
    return best;
  }

  global.Theory = {
    Rng: Rng,
    randomSeed: randomSeed,
    hashSeed: hashSeed,
    NOTE_NAMES: NOTE_NAMES,
    SCALES: SCALES,
    midi: midi,
    midiToName: midiToName,
    midiToFreq: midiToFreq,
    degreePitch: degreePitch,
    snapToScale: snapToScale,
    buildChord: buildChord,
    sweetenChord: sweetenChord,
    chordIsSound: chordIsSound,
    chordName: chordName,
    romanNumeral: romanNumeral,
    voiceChord: voiceChord,
    nearestChordTone: nearestChordTone
  };
})(window);
