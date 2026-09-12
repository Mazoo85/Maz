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

  /*
   * The scales.
   *
   * `steps` are semitones from the root, and the melody is written from them.
   * `chords`, where present, names a *different* scale to build the harmony
   * from, and that is the important part of this table.
   *
   * Chords here are made by stacking every other note of a scale. That works
   * beautifully for any seven-note scale — it is where the whole idea of a
   * triad comes from — and falls apart everywhere else. Stack every other note
   * of a five-note scale and you get fourths; of the whole-tone scale and every
   * chord is augmented; of the chromatic scale and every chord is a cluster of
   * neighbouring semitones. Naming a parent scale is not a workaround, it is
   * what the music actually does: a blues melody is played over dominant
   * chords, a minor-pentatonic riff over ordinary minor harmony, and nobody
   * has ever written a chromatic chord progression by stacking semitones.
   */
  const SCALES = {
    /* --- the seven modes of the major scale --- */
    major:        { name: 'Major',              steps: [0, 2, 4, 5, 7, 9, 11], minorish: false },
    minor:        { name: 'Natural Minor',      steps: [0, 2, 3, 5, 7, 8, 10], minorish: true },
    dorian:       { name: 'Dorian',             steps: [0, 2, 3, 5, 7, 9, 10], minorish: true },
    phrygian:     { name: 'Phrygian',           steps: [0, 1, 3, 5, 7, 8, 10], minorish: true },
    lydian:       { name: 'Lydian',             steps: [0, 2, 4, 6, 7, 9, 11], minorish: false },
    mixolydian:   { name: 'Mixolydian',         steps: [0, 2, 4, 5, 7, 9, 10], minorish: false },
    locrian:      { name: 'Locrian',            steps: [0, 1, 3, 5, 6, 8, 10], minorish: true },

    /* --- melodic and harmonic minor, and the modes worth having --- */
    harmonicMinor:  { name: 'Harmonic Minor',   steps: [0, 2, 3, 5, 7, 8, 11], minorish: true },
    melodicMinor:   { name: 'Melodic Minor',    steps: [0, 2, 3, 5, 7, 9, 11], minorish: true },
    phrygianDom:    { name: 'Phrygian Dominant', steps: [0, 1, 4, 5, 7, 8, 10], minorish: false },
    lydianDominant: { name: 'Lydian Dominant',  steps: [0, 2, 4, 6, 7, 9, 10], minorish: false },
    lydianAug:      { name: 'Lydian Augmented', steps: [0, 2, 4, 6, 8, 9, 11], minorish: false },
    mixolydianB6:   { name: 'Mixolydian ♭6',    steps: [0, 2, 4, 5, 7, 8, 10], minorish: false },
    locrianNat2:    { name: 'Locrian ♮2',       steps: [0, 2, 3, 5, 6, 8, 10], minorish: true },
    altered:        { name: 'Altered',          steps: [0, 1, 3, 4, 6, 8, 10], minorish: true },
    dorianB2:       { name: 'Dorian ♭2',        steps: [0, 1, 3, 5, 7, 9, 10], minorish: true },
    ionianSharp5:   { name: 'Ionian ♯5',        steps: [0, 2, 4, 5, 8, 9, 11], minorish: false },
    ukrainianDorian:{ name: 'Ukrainian Dorian', steps: [0, 2, 3, 6, 7, 9, 10], minorish: true },
    lydianSharp2:   { name: 'Lydian ♯2',        steps: [0, 3, 4, 6, 7, 9, 11], minorish: false },
    locrianNat6:    { name: 'Locrian ♮6',       steps: [0, 1, 3, 5, 6, 9, 10], minorish: true },
    alteredDim:     { name: 'Altered Diminished', steps: [0, 1, 3, 4, 6, 8, 9], minorish: true },

    /* --- seven-note scales from further afield --- */
    harmonicMajor:  { name: 'Harmonic Major',   steps: [0, 2, 4, 5, 7, 8, 11], minorish: false },
    doubleHarmonic: { name: 'Double Harmonic',  steps: [0, 1, 4, 5, 7, 8, 11], minorish: false },
    hungarianMinor: { name: 'Hungarian Minor',  steps: [0, 2, 3, 6, 7, 8, 11], minorish: true },
    neapolitanMajor:{ name: 'Neapolitan Major', steps: [0, 1, 3, 5, 7, 9, 11], minorish: true },
    neapolitanMinor:{ name: 'Neapolitan Minor', steps: [0, 1, 3, 5, 7, 8, 11], minorish: true },
    persian:        { name: 'Persian',          steps: [0, 1, 4, 5, 6, 8, 11], minorish: true },
    enigmatic:      { name: 'Enigmatic',        steps: [0, 1, 4, 6, 8, 10, 11], minorish: false },

    /* --- bebop scales: seven notes plus a passing note --- */
    bebopDominant:{ name: 'Bebop Dominant', steps: [0, 2, 4, 5, 7, 9, 10, 11], minorish: false,
                    chords: 'mixolydian' },
    bebopMajor:   { name: 'Bebop Major',    steps: [0, 2, 4, 5, 7, 8, 9, 11],  minorish: false,
                    chords: 'major' },
    bebopDorian:  { name: 'Bebop Dorian',   steps: [0, 2, 3, 4, 5, 7, 9, 10],  minorish: true,
                    chords: 'dorian' },

    /* --- symmetric scales: no chords of their own worth the name --- */
    wholeTone:    { name: 'Whole Tone',       steps: [0, 2, 4, 6, 8, 10],          minorish: false,
                    chords: 'lydianDominant' },
    diminishedWH: { name: 'Diminished',       steps: [0, 2, 3, 5, 6, 8, 9, 11],    minorish: true,
                    chords: 'harmonicMinor' },
    diminishedHW: { name: 'Dominant Diminished', steps: [0, 1, 3, 4, 6, 7, 9, 10], minorish: false,
                    chords: 'mixolydian' },
    augmentedScale: { name: 'Augmented',      steps: [0, 3, 4, 7, 8, 11],          minorish: false,
                    chords: 'harmonicMinor' },
    tritoneScale: { name: 'Tritone',          steps: [0, 1, 4, 6, 7, 10],          minorish: false,
                    chords: 'mixolydian' },
    chromatic:    { name: 'Chromatic',        steps: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11],
                    minorish: false, chords: 'major' },

    /* --- five- and six-note scales, with the harmony they are played over --- */
    majorPent:    { name: 'Major Pentatonic', steps: [0, 2, 4, 7, 9],     minorish: false,
                    chords: 'major' },
    aeolianPent:  { name: 'Minor Pentatonic', steps: [0, 3, 5, 7, 10],    minorish: true,
                    chords: 'minor' },
    blues:        { name: 'Blues',            steps: [0, 3, 5, 6, 7, 10], minorish: true,
                    chords: 'mixolydian' },
    majorBlues:   { name: 'Major Blues',      steps: [0, 2, 3, 4, 7, 9],  minorish: false,
                    chords: 'major' },
    suspendedPent:{ name: 'Suspended Pentatonic', steps: [0, 2, 5, 7, 10], minorish: false,
                    chords: 'mixolydian' },
    manGong:      { name: 'Man Gong',         steps: [0, 3, 5, 8, 10],    minorish: true,
                    chords: 'phrygian' },
    ritusen:      { name: 'Ritusen',          steps: [0, 2, 5, 7, 9],     minorish: false,
                    chords: 'major' },
    hirajoshi:    { name: 'Hirajoshi',        steps: [0, 2, 3, 7, 8],     minorish: true,
                    chords: 'harmonicMinor' },
    inSen:        { name: 'In Sen',           steps: [0, 1, 5, 7, 10],    minorish: true,
                    chords: 'phrygian' },
    iwato:        { name: 'Iwato',            steps: [0, 1, 5, 6, 10],    minorish: true,
                    chords: 'locrian' },
    kumoi:        { name: 'Kumoi',            steps: [0, 2, 3, 7, 9],     minorish: true,
                    chords: 'dorian' },
    balinese:     { name: 'Balinese Pelog',   steps: [0, 1, 3, 7, 8],     minorish: true,
                    chords: 'phrygian' },
    prometheus:   { name: 'Prometheus',       steps: [0, 2, 4, 6, 9, 10], minorish: false,
                    chords: 'lydianDominant' }
  };

  /*
   * The scales, grouped for the picker. Forty-odd names in one flat list is a
   * wall of text; grouped, you can find the one you want without knowing the
   * whole of music theory first. Every id in SCALES appears exactly once here,
   * which the test suite checks — a scale missing from the groups would simply
   * vanish from the app without anything failing.
   */
  const SCALE_GROUPS = [
    { name: 'The seven modes',
      ids: ['major', 'minor', 'dorian', 'phrygian', 'lydian', 'mixolydian', 'locrian'] },
    { name: 'Minor, and what comes out of it',
      ids: ['harmonicMinor', 'melodicMinor', 'phrygianDom', 'lydianDominant', 'lydianAug',
            'mixolydianB6', 'locrianNat2', 'altered', 'dorianB2', 'ionianSharp5',
            'ukrainianDorian', 'lydianSharp2', 'locrianNat6', 'alteredDim'] },
    { name: 'Further afield',
      ids: ['harmonicMajor', 'doubleHarmonic', 'hungarianMinor', 'neapolitanMajor',
            'neapolitanMinor', 'persian', 'enigmatic'] },
    { name: 'Bebop',
      ids: ['bebopDominant', 'bebopMajor', 'bebopDorian'] },
    { name: 'Symmetric',
      ids: ['wholeTone', 'diminishedWH', 'diminishedHW', 'augmentedScale', 'tritoneScale',
            'chromatic'] },
    { name: 'Five and six notes',
      ids: ['majorPent', 'aeolianPent', 'blues', 'majorBlues', 'suspendedPent', 'manGong',
            'ritusen', 'hirajoshi', 'inSen', 'iwato', 'kumoi', 'balinese', 'prometheus'] }
  ];

  /** The steps a scale's chords are built from — its own, or its parent's. */
  function chordStepsFor(scaleId) {
    const sc = SCALES[scaleId];
    if (!sc) return SCALES.major.steps;
    const parent = sc.chords && SCALES[sc.chords];
    return parent ? parent.steps : sc.steps;
  }

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

  /*
   * Chord shapes, as scale degrees above the chord's root.
   *
   * Every one of these is a stack of thirds — 0, 2, 4 is a triad because
   * skipping a degree each time *is* a third — carried further for the richer
   * chords: 6 adds the seventh, 8 the ninth, 10 the eleventh, 12 the
   * thirteenth. The suspensions replace the third rather than adding to it,
   * and quartal stacks fourths instead, which is why it sounds like nothing
   * else here.
   */
  const CHORD_SHAPES = {
    power:      [0, 4],
    triad:      [0, 2, 4],
    sus2:       [0, 1, 4],
    sus4:       [0, 3, 4],
    sixth:      [0, 2, 4, 5],
    seventh:    [0, 2, 4, 6],
    add9:       [0, 2, 4, 8],
    ninth:      [0, 2, 4, 6, 8],
    sixNine:    [0, 2, 4, 5, 8],
    eleventh:   [0, 2, 4, 6, 8, 10],
    /* A thirteenth with the eleventh left out — which is how one is actually
       played. The full stack would put an eleventh over the third, and that is
       the clash chordIsSound rejects; leaving it out is not a compromise, it
       is the voicing every chart means by "13". */
    thirteenth: [0, 2, 4, 6, 8, 12],
    /* Root, fifth and seventh — no third at all. The chord a jazz pianist
       plays with the left hand, because the third is the bass player's or the
       horn's and two of them in the same place is mud. */
    shell:      [0, 4, 6],
    /* Stacked fourths. Belongs to nobody's key in particular, which is exactly
       why it sounds modern rather than merely unresolved. */
    quartal:    [0, 3, 6]
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
    /* A major third with an eleventh over it. The two are a semitone apart
       once the eleventh is folded into the octave, and these chords are voiced
       close together, so it is a grinding second rather than the airy stack it
       is on paper. Every player who uses an eleventh chord over a major third
       leaves the third out; this engine has no way to, so it declines the
       chord and takes a ninth instead. Over a *minor* third there is no clash,
       which is why m11 is everywhere and maj11 is not. */
    if (set[4] && set[5]) return false;

    /*
     * The two intervals harmony actually forbids, checked between the notes as
     * they are stacked rather than as pitch classes: a semitone, and a minor
     * ninth — a semitone with an octave added, which stays just as harsh.
     *
     * These are about the *voicing*, which is why they cannot be spotted in the
     * pitch-class set above. A major seventh chord holds a B against a C and is
     * the most consonant chord there is, because they are eleven semitones
     * apart and not one. Stack the same two notes a semitone apart and it is a
     * cluster.
     *
     * This is what catches the sixth chords built on a degree whose sixth is
     * flat: the ♭6 lands directly against the fifth. Those were being accepted
     * *and* mis-named, because the naming rules have no symbol for a ♭6 and
     * silently left it out — so the chord strip claimed a plain minor triad
     * while the pad played a clash.
     */
    for (let i = 0; i < pitches.length; i++) {
      for (let j = i + 1; j < pitches.length; j++) {
        const gap = Math.abs(pitches[j] - pitches[i]);
        if (gap === 1 || gap === 13) return false;
      }
    }
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

    /* No third at all is a power chord — unless there is a seventh above it,
       in which case it is a shell voicing and everyone reads it as the chord
       it implies, third omitted: Cmaj7, not "C5maj7". */
    const bare = third === null;
    const base = third === 'm' ? 'm'
      : third === 'M' ? ''
      : bare ? (seventh ? '' : '5') : third;
    const ninth = has(2) && third !== 'sus2';
    const eleventh = has(5) && third !== 'sus4';
    const thirteenth = has(9);

    /*
     * Extended chords are named for the highest note in the stack, and every
     * note below it is taken as included: a thirteenth chord has a seventh, a
     * ninth and an eleventh in it, so nobody writes "C7913". Which means the
     * name is decided by looking from the top down and stopping at the first
     * note present.
     */
    let top = '';
    if (seventh && thirteenth && ninth) top = '13';
    else if (seventh && eleventh && ninth) top = '11';
    else if (seventh && ninth) top = '9';
    else if (seventh) top = '7';

    let ext = '';
    // A minor chord with a major seventh is bracketed, so it reads Am(maj7)
    // rather than the unreadable "Ammaj7".
    if (seventh === 'M') ext = base === 'm' ? '(maj' + top + ')' : 'maj' + top;
    else if (seventh === 'm') ext = top;
    else if (thirteenth) ext = ninth ? '6/9' : '6';
    else if (ninth) ext = 'add9';

    /* A suspension takes its seventh in front of it — C7sus4, never Csus47 —
       because the sus is describing what replaced the third, and that reads
       last. */
    if ((base === 'sus2' || base === 'sus4') && ext) return letter + ext + base;
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
    CHORD_SHAPES: CHORD_SHAPES,
    SCALE_GROUPS: SCALE_GROUPS,
    chordStepsFor: chordStepsFor,
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
