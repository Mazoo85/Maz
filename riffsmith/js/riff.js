/*
 * RIFFSMITH — Engine
 * ------------------
 * Pure logic (no DOM, no audio) so it runs in the browser AND in a headless
 * test.
 *
 * A riff is built in eight steps, each one reshaping the notes the step
 * before it left behind:
 *
 *   1 Progression  long notes, one per chord, chosen for a mood
 *   2 Chords       each long note becomes a chord
 *   3 Arpeggio     chords are broken into notes played one after another
 *   4 Mirror       parts of the riff are flipped upside down or back to front
 *   5 Levels       loudness (velocity) and left/right position (pan)
 *   6 Articulation note lengths: shorter, longer, joined up, repeated
 *   7 Groove       timing feel: swing, shuffle, laid back, humanized
 *   8 Fit          the scale it must stay in and the lowest/highest note
 *
 * Every step can be switched off, and every step has its own random seed. So
 * re-rolling one step changes only that step, and the same settings always
 * give exactly the same riff.
 *
 * Inside the pipeline, pitch is held as a scale degree (0 = the key's root,
 * 7 = the root an octave up), which is what keeps chords, arpeggios and
 * mirrors in key. Degrees become real MIDI note numbers at the end.
 *
 * Time is in ticks, 96 to a beat (a quarter note), 4 beats to a bar.
 *
 * Exposed as: window.Riff (also module.exports for testing).
 */
(function (root) {
  'use strict';

  var PPQ = 96;              // ticks per beat
  var BAR = PPQ * 4;         // ticks per bar
  var BASE_NOTE = 60;        // the key's root sits at middle C's octave

  var NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

  var SCALES = {
    major:      { label: 'Major',            steps: [0, 2, 4, 5, 7, 9, 11] },
    minor:      { label: 'Minor',            steps: [0, 2, 3, 5, 7, 8, 10] },
    harmonic:   { label: 'Harmonic minor',   steps: [0, 2, 3, 5, 7, 8, 11] },
    dorian:     { label: 'Dorian',           steps: [0, 2, 3, 5, 7, 9, 10] },
    phrygian:   { label: 'Phrygian',         steps: [0, 1, 3, 5, 7, 8, 10] },
    lydian:     { label: 'Lydian',           steps: [0, 2, 4, 6, 7, 9, 11] },
    mixolydian: { label: 'Mixolydian',       steps: [0, 2, 4, 5, 7, 9, 10] },
    pentMajor:  { label: 'Pentatonic major', steps: [0, 2, 4, 7, 9] },
    pentMinor:  { label: 'Pentatonic minor', steps: [0, 3, 5, 7, 10] },
    blues:      { label: 'Blues',            steps: [0, 3, 5, 6, 7, 10] },
    chromatic:  { label: 'Any note',         steps: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11] }
  };

  // Each mood picks the scale the riff is written in and a set of chord
  // progressions that suit it. Progressions are 0-based scale degrees:
  // [0, 4, 5, 3] in major is I – V – vi – IV.
  var MOODS = {
    happy:  { label: 'Happy',  scale: 'major', progressions: [
      [0, 4, 5, 3], [0, 5, 3, 4], [0, 3, 4, 3], [3, 4, 2, 5], [0, 3, 0, 4], [0, 2, 3, 4]] },
    chill:  { label: 'Chill',  scale: 'major', progressions: [
      [1, 4, 0, 5], [3, 2, 1, 0], [0, 2, 3, 1], [5, 1, 4, 0], [3, 4, 2, 5]] },
    dreamy: { label: 'Dreamy', scale: 'lydian', progressions: [
      [0, 1, 0, 1], [0, 2, 5, 1], [0, 4, 1, 5], [5, 1, 0, 4]] },
    sad:    { label: 'Sad',    scale: 'minor', progressions: [
      [0, 5, 2, 6], [0, 3, 5, 4], [0, 6, 5, 6], [5, 6, 0, 0], [0, 3, 6, 2]] },
    dark:   { label: 'Dark',   scale: 'phrygian', progressions: [
      [0, 1, 0, 6], [0, 0, 5, 1], [0, 3, 1, 0], [0, 5, 6, 1]] },
    epic:   { label: 'Epic',   scale: 'harmonic', progressions: [
      [0, 5, 3, 4], [5, 3, 0, 4], [0, 3, 5, 4], [0, 5, 0, 4]] }
  };

  var STEPS = [
    { id: 'progression', label: 'Progression', blurb: 'Long notes, one per chord, picked for a mood.' },
    { id: 'chords',      label: 'Chords',      blurb: 'Each long note becomes a chord.' },
    { id: 'arp',         label: 'Arpeggio',    blurb: 'Chords are broken up and played one note at a time.' },
    { id: 'mirror',      label: 'Mirror',      blurb: 'Parts of the riff are flipped upside down or back to front.' },
    { id: 'levels',      label: 'Levels',      blurb: 'How hard each note is hit, and where it sits left to right.' },
    { id: 'art',         label: 'Articulation', blurb: 'How long each note rings: short and choppy, or joined up.' },
    { id: 'groove',      label: 'Groove',      blurb: 'Timing feel: swing, shuffle, laid back, a human touch.' },
    { id: 'fit',         label: 'Fit',         blurb: 'The scale it must stay in, and its lowest and highest notes.' }
  ];

  // The controls each step offers. The app draws its panels from this, the
  // engine clamps loaded settings with it, and "dice" picks from it.
  //   select: options [[value, label], ...]
  //   range:  min, max, step
  //   toggle: true / false
  var CONTROLS = {
    progression: [
      { key: 'mood', label: 'Mood', type: 'select',
        options: Object.keys(MOODS).map(function (k) { return [k, MOODS[k].label]; }) },
      { key: 'pattern', label: 'Progression', type: 'select', options: [['auto', 'Surprise me']] }, // filled per mood
      { key: 'bars', label: 'Bars', type: 'select', options: [[1, '1'], [2, '2'], [4, '4'], [8, '8'], [16, '16']] },
      { key: 'chordBars', label: 'Bars per chord', type: 'select', options: [[0.5, '½'], [1, '1'], [2, '2']] },
      { key: 'variation', label: 'Variation', type: 'range', min: 0, max: 1, step: 0.05, pct: true }
    ],
    chords: [
      { key: 'type', label: 'Chord type', type: 'select', options: [
        ['triad', 'Triads (3 notes)'], ['seventh', 'Sevenths (4 notes)'], ['ninth', 'Ninths (5 notes)'],
        ['sus', 'Suspended'], ['power', 'Power (root + fifth)'], ['mixed', 'Mixed']] },
      { key: 'voicing', label: 'Voicing', type: 'select', options: [
        ['close', 'Close'], ['open', 'Open'], ['spread', 'Spread wide']] },
      { key: 'inversion', label: 'Inversions', type: 'select', options: [
        ['none', 'Root position'], ['smooth', 'Smooth (closest move)'], ['random', 'Random']] },
      { key: 'bass', label: 'Add bass note', type: 'toggle' }
    ],
    arp: [
      { key: 'pattern', label: 'Pattern', type: 'select', options: [
        ['up', 'Up'], ['down', 'Down'], ['updown', 'Up & down'], ['downup', 'Down & up'],
        ['converge', 'Outside in'], ['random', 'Random'], ['stab', 'Chord stabs']] },
      { key: 'rate', label: 'Speed', type: 'select', options: [
        [4, '1/4 notes'], [8, '1/8 notes'], [16, '1/16 notes'], [32, '1/32 notes']] },
      { key: 'triplet', label: 'Triplets', type: 'toggle' },
      { key: 'range', label: 'Octaves', type: 'select', options: [[1, '1'], [2, '2'], [3, '3'], [4, '4']] },
      { key: 'gate', label: 'Gate (note length)', type: 'range', min: 0.1, max: 1, step: 0.05, pct: true },
      { key: 'rests', label: 'Rests', type: 'range', min: 0, max: 0.8, step: 0.05, pct: true },
      { key: 'flip', label: 'Flip', type: 'toggle' },
      { key: 'alternate', label: 'Alternate flip each chord', type: 'toggle' }
    ],
    mirror: [
      { key: 'vertical', label: 'Upside down', type: 'toggle' },
      { key: 'horizontal', label: 'Back to front', type: 'toggle' },
      { key: 'where', label: 'Where', type: 'select', options: [
        ['bar', 'Every 2nd bar'], ['twobar', 'Every 2nd pair of bars'], ['half', 'Second half'], ['random', 'Random bars']] }
    ],
    levels: [
      { key: 'velocity', label: 'Velocity', type: 'range', min: 0.1, max: 1, step: 0.05, pct: true },
      { key: 'velVar', label: 'Velocity variation', type: 'range', min: 0, max: 1, step: 0.05, pct: true },
      { key: 'accent', label: 'Accent on the beat', type: 'range', min: 0, max: 1, step: 0.05, pct: true },
      { key: 'panVar', label: 'Pan spread', type: 'range', min: 0, max: 1, step: 0.05, pct: true }
    ],
    art: [
      { key: 'multiply', label: 'Length', type: 'range', min: 0.25, max: 2, step: 0.05, times: true },
      { key: 'variation', label: 'Length variation', type: 'range', min: 0, max: 1, step: 0.05, pct: true },
      { key: 'legato', label: 'Join notes (legato)', type: 'toggle' },
      { key: 'ratchet', label: 'Ratchets (quick repeats)', type: 'range', min: 0, max: 1, step: 0.05, pct: true }
    ],
    groove: [
      { key: 'template', label: 'Groove', type: 'select', options: [
        ['straight', 'Straight'], ['swing8', 'Swing (8ths)'], ['swing16', 'Swing (16ths)'],
        ['shuffle', 'Shuffle'], ['laidback', 'Laid back'], ['push', 'Pushed (rushing)']] },
      { key: 'amount', label: 'Amount', type: 'range', min: 0, max: 1, step: 0.05, pct: true },
      { key: 'humanize', label: 'Human feel', type: 'range', min: 0, max: 1, step: 0.05, pct: true }
    ],
    fit: [
      { key: 'scale', label: 'Scale', type: 'select', options: [['auto', 'Follow the mood']].concat(
        Object.keys(SCALES).map(function (k) { return [k, SCALES[k].label]; })) },
      { key: 'low', label: 'Lowest note', type: 'range', min: 24, max: 96, step: 1, note: true },
      { key: 'high', label: 'Highest note', type: 'range', min: 24, max: 108, step: 1, note: true },
      { key: 'mode', label: 'Out-of-range notes', type: 'select', options: [
        ['fold', 'Move by octaves'], ['clamp', 'Squash to the edge'], ['drop', 'Remove']] },
      { key: 'octave', label: 'Octave shift', type: 'select', options: [[-2, '−2'], [-1, '−1'], [0, '0'], [1, '+1'], [2, '+2']] }
    ]
  };

  function defaults() {
    return {
      key: 9,          // A
      bpm: 120,
      progression: { on: true, seed: 1, mood: 'sad', pattern: 'auto', bars: 4, chordBars: 1, variation: 0 },
      chords:      { on: true, seed: 2, type: 'triad', voicing: 'close', inversion: 'smooth', bass: false },
      arp:         { on: true, seed: 3, pattern: 'updown', rate: 16, triplet: false, range: 2, gate: 0.7,
                     rests: 0, flip: false, alternate: false },
      mirror:      { on: false, seed: 4, vertical: true, horizontal: false, where: 'bar' },
      levels:      { on: true, seed: 5, velocity: 0.8, velVar: 0.15, accent: 0.3, panVar: 0 },
      art:         { on: true, seed: 6, multiply: 1, variation: 0, legato: false, ratchet: 0 },
      groove:      { on: true, seed: 7, template: 'straight', amount: 0.5, humanize: 0 },
      fit:         { on: true, seed: 8, scale: 'auto', low: 48, high: 84, mode: 'fold', octave: 0 }
    };
  }

  // ---------------------------------------------------------------- random

  // A small, fast, seedable generator: the same seed always gives the same
  // sequence, which is what makes a riff reproducible.
  function mulberry32(a) {
    a = a >>> 0;
    return function () {
      a = (a + 0x6D2B79F5) >>> 0;
      var t = a;
      t = Math.imul(t ^ (t >>> 15), t | 1);
      t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }

  function hashString(s) {
    var h = 2166136261;
    for (var i = 0; i < s.length; i++) {
      h ^= s.charCodeAt(i);
      h = Math.imul(h, 16777619);
    }
    return h >>> 0;
  }

  function stepRng(stepId, seed) {
    return mulberry32(hashString(stepId) ^ Math.imul((seed >>> 0) || 1, 2654435761));
  }

  function pick(rng, list) { return list[Math.floor(rng() * list.length) % list.length]; }

  function clamp(v, lo, hi) { return v < lo ? lo : v > hi ? hi : v; }

  // ------------------------------------------------------------- settings

  function patternOptions(mood) {
    var m = MOODS[mood] || MOODS.sad;
    var scale = SCALES[m.scale];
    var opts = [['auto', 'Surprise me']];
    m.progressions.forEach(function (p, i) {
      opts.push([String(i), p.map(function (d) { return romanFor(scale.steps, d); }).join(' – ')]);
    });
    return opts;
  }

  function controlsFor(stepId, settings) {
    var list = CONTROLS[stepId];
    if (stepId === 'progression' && settings) {
      return list.map(function (c) {
        return c.key === 'pattern'
          ? Object.assign({}, c, { options: patternOptions(settings.progression.mood) })
          : c;
      });
    }
    return list;
  }

  function validValue(control, value, fallback) {
    if (control.type === 'toggle') return typeof value === 'boolean' ? value : fallback;
    if (control.type === 'range') {
      var n = Number(value);
      if (!isFinite(n)) return fallback;
      return clamp(n, control.min, control.max);
    }
    for (var i = 0; i < control.options.length; i++) {
      if (String(control.options[i][0]) === String(value)) return control.options[i][0];
    }
    return fallback;
  }

  // Take settings from anywhere (a save, an old version, a hand-edited file)
  // and return a complete, valid set. Anything missing or out of range falls
  // back to the default.
  function normalize(input) {
    var d = defaults();
    var src = input && typeof input === 'object' ? input : {};
    var out = {
      key: validValue({ type: 'range', min: 0, max: 11 }, src.key, d.key) | 0,
      bpm: Math.round(validValue({ type: 'range', min: 40, max: 240 }, src.bpm, d.bpm))
    };
    STEPS.forEach(function (step) {
      var s = src[step.id] && typeof src[step.id] === 'object' ? src[step.id] : {};
      var def = d[step.id];
      var res = {
        on: typeof s.on === 'boolean' ? s.on : def.on,
        seed: (Number.isInteger(s.seed) && s.seed > 0 && s.seed < 4294967296) ? s.seed : def.seed
      };
      out[step.id] = res;
      CONTROLS[step.id].forEach(function (c) {
        // The progression's pattern depends on the mood, so it is filled in
        // below; the placeholder keeps the keys in their usual order.
        res[c.key] = (step.id === 'progression' && c.key === 'pattern')
          ? null : validValue(c, s[c.key], def[c.key]);
      });
    });
    var pat = src.progression && src.progression.pattern;
    out.progression.pattern = validValue({ type: 'select', options: patternOptions(out.progression.mood) },
      pat, 'auto');
    if (out.fit.high < out.fit.low + 12) out.fit.high = Math.min(108, out.fit.low + 12);
    return out;
  }

  function clone(o) { return JSON.parse(JSON.stringify(o)); }

  function newSeed(rng) { return 1 + Math.floor(rng() * 4294967294); }

  // "Roll the dice": new seeds for every step that isn't locked. Settings stay.
  function reroll(settings, locked, rng) {
    rng = rng || Math.random;
    var out = clone(settings);
    STEPS.forEach(function (s) {
      if (!(locked && locked[s.id])) out[s.id].seed = newSeed(rng);
    });
    return out;
  }

  // "Surprise me" for one step: pick fresh, musical settings for it as well as
  // a new seed. Some choices are weighted so the result is usually usable.
  function randomizeStep(settings, stepId, rng) {
    rng = rng || Math.random;
    var out = clone(settings);
    var s = out[stepId];
    s.seed = newSeed(rng);
    switch (stepId) {
      case 'progression':
        s.mood = pick(rng, Object.keys(MOODS));
        s.pattern = 'auto';
        s.chordBars = pick(rng, [1, 1, 1, 2, 0.5]);
        s.variation = pick(rng, [0, 0, 0.2, 0.4]);
        break;
      case 'chords':
        s.type = pick(rng, ['triad', 'triad', 'seventh', 'seventh', 'ninth', 'sus', 'power', 'mixed']);
        s.voicing = pick(rng, ['close', 'close', 'open', 'spread']);
        s.inversion = pick(rng, ['smooth', 'smooth', 'none', 'random']);
        s.bass = rng() < 0.3;
        break;
      case 'arp':
        s.pattern = pick(rng, ['up', 'down', 'updown', 'downup', 'converge', 'random', 'stab']);
        s.rate = pick(rng, [8, 16, 16, 16, 32]);
        s.triplet = rng() < 0.15;
        s.range = pick(rng, [1, 2, 2, 3]);
        s.gate = pick(rng, [0.3, 0.5, 0.7, 0.9, 1]);
        s.rests = pick(rng, [0, 0, 0, 0.15, 0.3]);
        s.flip = rng() < 0.3;
        s.alternate = rng() < 0.3;
        break;
      case 'mirror':
        s.vertical = rng() < 0.6;
        s.horizontal = !s.vertical || rng() < 0.4;
        s.where = pick(rng, ['bar', 'twobar', 'half', 'random']);
        break;
      case 'levels':
        s.velocity = pick(rng, [0.6, 0.7, 0.8, 0.9]);
        s.velVar = pick(rng, [0, 0.1, 0.2, 0.35]);
        s.accent = pick(rng, [0, 0.2, 0.4, 0.6]);
        s.panVar = pick(rng, [0, 0, 0.2, 0.5]);
        break;
      case 'art':
        s.multiply = pick(rng, [0.5, 0.75, 1, 1, 1.25]);
        s.variation = pick(rng, [0, 0, 0.2, 0.4]);
        s.legato = rng() < 0.2;
        s.ratchet = pick(rng, [0, 0, 0, 0.1, 0.25]);
        break;
      case 'groove':
        s.template = pick(rng, ['straight', 'swing8', 'swing16', 'shuffle', 'laidback', 'push']);
        s.amount = pick(rng, [0.3, 0.5, 0.7]);
        s.humanize = pick(rng, [0, 0.1, 0.2, 0.4]);
        break;
      case 'fit':
        s.scale = 'auto';
        s.octave = pick(rng, [-1, 0, 0, 0, 1]);
        break;
    }
    return normalize(out);
  }

  // -------------------------------------------------------------- pitches

  function moodScaleId(settings) {
    var m = MOODS[settings.progression.mood] || MOODS.sad;
    return m.scale;
  }

  // Scale degree -> semitones above the key's root (can be negative).
  function degreeToSemis(steps, deg) {
    var n = steps.length;
    var oct = Math.floor(deg / n);
    return oct * 12 + steps[((deg % n) + n) % n];
  }

  // The chord quality built on a degree of a 7-note scale, from its third and
  // fifth: '' major, 'm' minor, '°' diminished, '+' augmented.
  function triadQuality(steps, deg) {
    var r = degreeToSemis(steps, deg);
    var third = degreeToSemis(steps, deg + 2) - r;
    var fifth = degreeToSemis(steps, deg + 4) - r;
    if (third === 4 && fifth === 8) return '+';
    if (third === 3 && fifth === 6) return '°';
    return third === 3 ? 'm' : '';
  }

  var ROMAN = ['I', 'II', 'III', 'IV', 'V', 'VI', 'VII'];
  function romanFor(steps, deg) {
    var q = triadQuality(steps, deg);
    var numeral = ROMAN[((deg % 7) + 7) % 7];
    if (q === 'm' || q === '°') numeral = numeral.toLowerCase();
    return numeral + (q === '°' ? '°' : q === '+' ? '+' : '');
  }

  function noteName(midi) {
    return NOTE_NAMES[((midi % 12) + 12) % 12] + (Math.floor(midi / 12) - 1);
  }

  // Snap a MIDI note to the nearest note of a scale in a key. Ties go down,
  // so a note exactly between two scale notes falls rather than rises.
  function snapToScale(midi, keyRoot, steps) {
    if (steps.length === 12) return midi;
    for (var d = 0; d <= 6; d++) {
      var down = midi - d, up = midi + d;
      if (steps.indexOf((((down - keyRoot) % 12) + 12) % 12) !== -1) return down;
      if (steps.indexOf((((up - keyRoot) % 12) + 12) % 12) !== -1) return up;
    }
    return midi;
  }

  // ------------------------------------------------------------ the steps

  // A note while it travels through the pipeline:
  //   t     start, in ticks           len   length, in ticks
  //   deg   scale degree (pitch)      vel   0..1   pan  -1..1
  //   chord index of the chord it belongs to

  function stepProgression(cfg, mood, rng) {
    var m = MOODS[mood] || MOODS.sad;
    var base = cfg.pattern === 'auto' ? pick(rng, m.progressions) : m.progressions[Number(cfg.pattern)];
    var chordLen = Math.round(cfg.chordBars * BAR);
    var count = Math.max(1, Math.round(cfg.bars * BAR / chordLen));
    var pool = [];
    m.progressions.forEach(function (p) { p.forEach(function (d) { if (pool.indexOf(d) === -1) pool.push(d); }); });
    var notes = [];
    for (var i = 0; i < count; i++) {
      var deg = base[i % base.length];
      // Variation swaps a chord for another the mood uses — never the first,
      // which anchors the key.
      if (i > 0 && cfg.variation > 0 && rng() < cfg.variation) deg = pick(rng, pool);
      notes.push({ t: i * chordLen, len: chordLen, deg: deg, vel: 0.8, pan: 0, chord: i });
    }
    return notes;
  }

  function chordShape(type, rng) {
    switch (type) {
      case 'seventh': return [0, 2, 4, 6];
      case 'ninth':   return [0, 2, 4, 6, 8];
      case 'sus':     return rng() < 0.5 ? [0, 3, 4] : [0, 1, 4];
      case 'power':   return [0, 4, 7];
      case 'mixed':   return pick(rng, [[0, 2, 4], [0, 2, 4], [0, 2, 4, 6], [0, 3, 4], [0, 2, 4, 8]]);
      default:        return [0, 2, 4];
    }
  }

  function invert(degs, k) {
    var out = degs.slice();
    for (var i = 0; i < k; i++) out.push(out.shift() + 7);
    return out.sort(function (a, b) { return a - b; });
  }

  function mean(list) {
    var s = 0;
    for (var i = 0; i < list.length; i++) s += list[i];
    return s / list.length;
  }

  function stepChords(notes, cfg, rng) {
    var out = [];
    var prevCentre = null;
    notes.forEach(function (n) {
      var shape = chordShape(cfg.type, rng);
      var degs = shape.map(function (o) { return n.deg + o; });
      if (cfg.inversion === 'random') {
        degs = invert(degs, Math.floor(rng() * shape.length));
      } else if (cfg.inversion === 'smooth' && prevCentre !== null) {
        // Pick the inversion (and octave) that moves least from the last chord.
        var best = degs, bestDist = Infinity;
        for (var k = 0; k < shape.length; k++) {
          for (var o = -1; o <= 0; o++) {
            var cand = invert(degs, k).map(function (d) { return d + o * 7; });
            var dist = Math.abs(mean(cand) - prevCentre);
            if (dist < bestDist - 1e-9) { bestDist = dist; best = cand; }
          }
        }
        degs = best;
      }
      if (cfg.voicing === 'open' && degs.length >= 3) {
        degs = degs.slice();
        degs[1] += 7;
      } else if (cfg.voicing === 'spread') {
        degs = degs.map(function (d, i) { return d + (i % 2 === 1 ? 7 : 0) + (i >= 3 ? 7 : 0); });
      }
      degs.sort(function (a, b) { return a - b; });
      prevCentre = mean(degs);
      if (cfg.bass) degs.unshift(n.deg - 7);
      degs.forEach(function (d) {
        out.push({ t: n.t, len: n.len, deg: d, vel: n.vel, pan: n.pan, chord: n.chord });
      });
    });
    return out;
  }

  // Group notes by the chord they came from, keeping each chord's span.
  function chordsOf(notes) {
    var byChord = {};
    var order = [];
    notes.forEach(function (n) {
      var c = byChord[n.chord];
      if (!c) {
        c = byChord[n.chord] = { start: n.t, end: n.t + n.len, degs: [], index: n.chord };
        order.push(n.chord);
      }
      c.start = Math.min(c.start, n.t);
      c.end = Math.max(c.end, n.t + n.len);
      if (c.degs.indexOf(n.deg) === -1) c.degs.push(n.deg);
    });
    return order.map(function (k) {
      byChord[k].degs.sort(function (a, b) { return a - b; });
      return byChord[k];
    });
  }

  function arpSequence(degs, pattern, rng) {
    var up = degs.slice();
    var down = degs.slice().reverse();
    switch (pattern) {
      case 'down': return down;
      case 'updown': return up.length > 2 ? up.concat(down.slice(1, -1)) : up;
      case 'downup': return down.length > 2 ? down.concat(up.slice(1, -1)) : down;
      case 'converge': {
        var out = [], lo = 0, hi = up.length - 1;
        while (lo <= hi) {
          out.push(up[lo++]);
          if (lo <= hi) out.push(up[hi--]);
        }
        return out;
      }
      case 'random': {
        var r = up.slice();
        for (var i = r.length - 1; i > 0; i--) {
          var j = Math.floor(rng() * (i + 1));
          var tmp = r[i]; r[i] = r[j]; r[j] = tmp;
        }
        return r;
      }
      default: return up;
    }
  }

  function stepArp(notes, cfg, rng) {
    var step = Math.round((BAR / cfg.rate) * (cfg.triplet ? 2 / 3 : 1));
    var len = Math.max(3, Math.round(step * cfg.gate));
    var out = [];
    chordsOf(notes).forEach(function (c, ci) {
      var degs = [];
      for (var o = 0; o < cfg.range; o++) {
        c.degs.forEach(function (d) { if (degs.indexOf(d + o * 7) === -1) degs.push(d + o * 7); });
      }
      degs.sort(function (a, b) { return a - b; });
      var seq = arpSequence(degs, cfg.pattern, rng);
      var flip = cfg.flip;
      if (cfg.alternate && ci % 2 === 1) flip = !flip;
      if (flip) seq = seq.slice().reverse();
      var i = 0;
      for (var t = c.start; t < c.end - 1; t += step, i++) {
        if (cfg.rests > 0 && i > 0 && rng() < cfg.rests) continue; // never rest on a chord's first note
        var noteLen = Math.min(len, c.end - t);
        if (cfg.pattern === 'stab') {
          c.degs.forEach(function (d) {
            out.push({ t: t, len: noteLen, deg: d, vel: 0.8, pan: 0, chord: c.index });
          });
        } else {
          out.push({ t: t, len: noteLen, deg: seq[i % seq.length], vel: 0.8, pan: 0, chord: c.index });
        }
      }
    });
    return out;
  }

  // Which time spans the mirror step works on.
  function mirrorSpans(cfg, total, rng) {
    var spans = [];
    var bars = Math.max(1, Math.round(total / BAR));
    if (cfg.where === 'half') {
      if (total >= 2 * PPQ) spans.push([total / 2, total]);
      return spans;
    }
    var size = cfg.where === 'twobar' ? 2 * BAR : BAR;
    var count = Math.ceil(total / size);
    for (var i = 0; i < count; i++) {
      var on = cfg.where === 'random' ? rng() < 0.5 : i % 2 === 1;
      if (on) spans.push([i * size, Math.min(total, (i + 1) * size)]);
    }
    if (bars === 1 && spans.length === 0 && cfg.where !== 'random') spans.push([total / 2, total]);
    return spans;
  }

  function stepMirror(notes, cfg, total, rng) {
    if (!notes.length || (!cfg.vertical && !cfg.horizontal)) return notes;
    var lo = Infinity, hi = -Infinity;
    notes.forEach(function (n) { lo = Math.min(lo, n.deg); hi = Math.max(hi, n.deg); });
    var axisTwice = lo + hi; // flipping around the middle keeps the riff in its own range
    var spans = mirrorSpans(cfg, total, rng);
    return notes.map(function (n) {
      for (var i = 0; i < spans.length; i++) {
        var s = spans[i];
        if (n.t >= s[0] && n.t < s[1]) {
          var m = Object.assign({}, n);
          if (cfg.vertical) m.deg = axisTwice - n.deg;
          if (cfg.horizontal) {
            m.t = s[0] + s[1] - (n.t + n.len);
            if (m.t < s[0]) { m.len -= s[0] - m.t; m.t = s[0]; }
          }
          return m;
        }
      }
      return n;
    });
  }

  function stepLevels(notes, cfg, rng) {
    return notes.map(function (n) {
      var v = cfg.velocity + (rng() * 2 - 1) * cfg.velVar * 0.5;
      var pos = n.t % PPQ === 0 ? (n.t % BAR === 0 ? 2 : 1) : 0;
      v += cfg.accent * (pos === 2 ? 0.3 : pos === 1 ? 0.15 : -0.12);
      return Object.assign({}, n, {
        vel: clamp(v, 0.05, 1),
        pan: clamp((rng() * 2 - 1) * cfg.panVar, -1, 1)
      });
    });
  }

  function stepArticulation(notes, cfg, rng) {
    var sorted = notes.slice().sort(function (a, b) { return a.t - b.t; });
    var starts = [];
    sorted.forEach(function (n) { if (starts[starts.length - 1] !== n.t) starts.push(n.t); });
    var out = [];
    sorted.forEach(function (n) {
      var len;
      if (cfg.legato) {
        // Ring right up to the next note that starts after this one.
        var next = null;
        for (var i = 0; i < starts.length; i++) if (starts[i] > n.t) { next = starts[i]; break; }
        len = next === null ? n.len : next - n.t;
      } else {
        len = n.len * cfg.multiply;
      }
      len *= 1 + (rng() * 2 - 1) * cfg.variation * 0.6;
      len = Math.max(6, Math.round(len));
      if (cfg.ratchet > 0 && len >= 24 && rng() < cfg.ratchet) {
        var hits = rng() < 0.5 ? 2 : 3;
        var each = Math.floor(len / hits);
        for (var h = 0; h < hits; h++) {
          out.push(Object.assign({}, n, {
            t: n.t + h * each, len: Math.max(3, Math.round(each * 0.6)),
            vel: clamp(n.vel * (1 - h * 0.15), 0.05, 1)
          }));
        }
      } else {
        out.push(Object.assign({}, n, { len: len }));
      }
    });
    return out;
  }

  // Groove templates: how far (in ticks, at full amount) a note is moved
  // depending on which 16th of the beat it falls on: [on, e, &, a].
  var GROOVES = {
    straight: [0, 0, 0, 0],
    swing8:   [0, 4, 16, 4],    // offbeat 8ths pushed to the triplet
    swing16:  [0, 8, 0, 8],     // offbeat 16ths pushed to the triplet
    shuffle:  [0, 10, 16, 10],
    laidback: [3, 6, 6, 6],
    push:     [0, -4, -6, -4]
  };

  function stepGroove(notes, cfg, rng) {
    var shape = GROOVES[cfg.template] || GROOVES.straight;
    var sixteenth = PPQ / 4;
    return notes.map(function (n) {
      var t = n.t;
      var grid = Math.round(t / sixteenth);
      if (Math.abs(grid * sixteenth - t) <= 3) t += shape[((grid % 4) + 4) % 4] * cfg.amount;
      t += (rng() * 2 - 1) * 6 * cfg.humanize;
      t = Math.max(0, Math.round(t));
      return Object.assign({}, n, { t: t });
    });
  }

  // Degrees -> MIDI note numbers. The key always applies; the rest (a scale
  // to snap to, the note range) only when the Fit step is on.
  function stepToMidi(notes, settings, fitOn) {
    var moodSteps = SCALES[moodScaleId(settings)].steps;
    var root = BASE_NOTE + settings.key;
    if (root > 66) root -= 12; // keep every key near middle C
    var fit = settings.fit;
    var out = [];
    notes.forEach(function (n) {
      var p = root + degreeToSemis(moodSteps, n.deg);
      if (fitOn) {
        p += fit.octave * 12;
        if (fit.scale !== 'auto') p = snapToScale(p, settings.key, SCALES[fit.scale].steps);
        if (p < fit.low || p > fit.high) {
          if (fit.mode === 'drop') return;
          if (fit.mode === 'fold') {
            while (p < fit.low) p += 12;
            while (p > fit.high) p -= 12;
          }
          if (p < fit.low || p > fit.high) {
            // Squash to the edge (or a range under an octave): nearest in-scale
            // note inside the range.
            p = clamp(p, fit.low, fit.high);
            var steps = fit.scale === 'auto' ? moodSteps : SCALES[fit.scale].steps;
            var s = snapToScale(p, settings.key, steps);
            if (s >= fit.low && s <= fit.high) p = s;
          }
        }
      }
      out.push({ t: n.t, len: n.len, pitch: clamp(p, 0, 127), vel: n.vel, pan: n.pan });
    });
    return out;
  }

  // The chord names shown above the piano roll, one per chord of the
  // progression.
  function chordLabels(settings, progNotes) {
    var steps = SCALES[moodScaleId(settings)].steps;
    var root = settings.key;
    return progNotes.map(function (n) {
      var pc = (((root + degreeToSemis(steps, n.deg)) % 12) + 12) % 12;
      return { t: n.t, len: n.len, name: NOTE_NAMES[pc] + triadQuality(steps, n.deg),
               roman: romanFor(steps, n.deg) };
    });
  }

  // Build the riff. `upTo` (0..7) stops after that step, to hear the riff as it
  // stood part-way through. Returns notes sorted by start then pitch.
  function generate(input, upTo) {
    var settings = normalize(input);
    var last = upTo === undefined || upTo === null ? STEPS.length - 1 : clamp(upTo | 0, 0, STEPS.length - 1);
    var total = Math.round(settings.progression.bars * BAR);
    var run = function (id, idx) { return idx <= last && settings[id].on; };

    // The progression is the riff's backbone; with it switched off the riff
    // sits on the key's root for the whole length.
    var prog = run('progression', 0)
      ? stepProgression(settings.progression, settings.progression.mood,
          stepRng('progression', settings.progression.seed))
      : [{ t: 0, len: total, deg: 0, vel: 0.8, pan: 0, chord: 0 }];

    var notes = prog;
    if (run('chords', 1)) notes = stepChords(notes, settings.chords, stepRng('chords', settings.chords.seed));
    if (run('arp', 2)) notes = stepArp(notes, settings.arp, stepRng('arp', settings.arp.seed));
    if (run('mirror', 3)) notes = stepMirror(notes, settings.mirror, total, stepRng('mirror', settings.mirror.seed));
    if (run('levels', 4)) notes = stepLevels(notes, settings.levels, stepRng('levels', settings.levels.seed));
    if (run('art', 5)) notes = stepArticulation(notes, settings.art, stepRng('art', settings.art.seed));
    if (run('groove', 6)) notes = stepGroove(notes, settings.groove, stepRng('groove', settings.groove.seed));

    var midi = stepToMidi(notes, settings, run('fit', 7));

    // Tidy: nothing may run past the end, and no two notes may share a pitch
    // and start.
    var seen = {};
    var clean = [];
    midi.forEach(function (n) {
      if (n.t >= total) return;
      var k = n.t + ':' + n.pitch;
      if (seen[k]) return;
      seen[k] = true;
      clean.push({ t: n.t, len: Math.max(1, Math.min(n.len, total - n.t)), pitch: n.pitch,
                   vel: Math.round(n.vel * 1000) / 1000, pan: Math.round(n.pan * 1000) / 1000 });
    });
    clean.sort(function (a, b) { return a.t - b.t || a.pitch - b.pitch; });
    // A note may not still be ringing when the same pitch starts again (a MIDI
    // file can't hold two of the same note at once): cut it short there.
    var lastOf = {};
    clean.forEach(function (n) {
      var prev = lastOf[n.pitch];
      if (prev && prev.t + prev.len > n.t) prev.len = n.t - prev.t;
      lastOf[n.pitch] = n;
    });

    return { notes: clean, length: total, ppq: PPQ, bars: settings.progression.bars,
             chords: chordLabels(settings, prog), settings: settings };
  }

  // ------------------------------------------------------------ MIDI file

  function varLen(n) {
    var bytes = [n & 0x7F];
    while ((n >>= 7) > 0) bytes.unshift((n & 0x7F) | 0x80);
    return bytes;
  }

  function u32(n) { return [(n >>> 24) & 255, (n >>> 16) & 255, (n >>> 8) & 255, n & 255]; }
  function u16(n) { return [(n >>> 8) & 255, n & 255]; }

  // A standard MIDI file (format 0, one track) that any music program opens,
  // FL Studio included. Notes, velocities, tempo and the riff's length.
  function toMidi(riff, opts) {
    opts = opts || {};
    var bpm = opts.bpm || 120;
    var name = opts.name || 'Riffsmith riff';
    var events = [];
    riff.notes.forEach(function (n) {
      var v = clamp(Math.round(n.vel * 127), 1, 127);
      events.push({ t: n.t, order: 1, data: [0x90, n.pitch, v] });
      events.push({ t: n.t + n.len, order: 0, data: [0x80, n.pitch, 0] });
    });
    // Note-offs before note-ons at the same moment, so a repeated note retriggers.
    events.sort(function (a, b) { return a.t - b.t || a.order - b.order; });

    var track = [];
    var nameBytes = [];
    for (var i = 0; i < name.length; i++) nameBytes.push(name.charCodeAt(i) & 0x7F);
    track.push.apply(track, [0, 0xFF, 0x03].concat(varLen(nameBytes.length), nameBytes));
    var mpqn = Math.round(60000000 / bpm);
    track.push(0, 0xFF, 0x51, 0x03, (mpqn >> 16) & 255, (mpqn >> 8) & 255, mpqn & 255);
    track.push(0, 0xFF, 0x58, 0x04, 4, 2, 24, 8); // 4/4
    var now = 0;
    events.forEach(function (e) {
      track.push.apply(track, varLen(e.t - now).concat(e.data));
      now = e.t;
    });
    track.push.apply(track, varLen(Math.max(0, riff.length - now)).concat([0xFF, 0x2F, 0x00]));

    var bytes = [0x4D, 0x54, 0x68, 0x64].concat(u32(6), u16(0), u16(1), u16(riff.ppq),
      [0x4D, 0x54, 0x72, 0x6B], u32(track.length), track);
    return new Uint8Array(bytes);
  }

  // Starting points. Each is a partial settings object laid over the defaults.
  var PRESETS = [
    { id: 'sad-arp', label: 'Sad arp', settings: {} },
    { id: 'pop-chords', label: 'Pop chords', settings: {
      key: 0, progression: { mood: 'happy', pattern: '0' },
      chords: { type: 'triad', voicing: 'open', bass: true },
      arp: { pattern: 'stab', rate: 8, gate: 0.6 },
      groove: { template: 'swing16', amount: 0.3 } } },
    { id: 'trap-pluck', label: 'Trap pluck', settings: {
      key: 1, bpm: 140, progression: { mood: 'dark', chordBars: 2 },
      chords: { type: 'triad' },
      arp: { pattern: 'random', rate: 16, range: 2, gate: 0.5, rests: 0.25 },
      art: { ratchet: 0.2 }, fit: { low: 60, high: 84 } } },
    { id: 'chill-keys', label: 'Chill keys', settings: {
      key: 5, bpm: 85, progression: { mood: 'chill', pattern: '0' },
      chords: { type: 'seventh', voicing: 'open' },
      arp: { pattern: 'up', rate: 8, range: 1, gate: 0.9 },
      groove: { template: 'swing8', amount: 0.6, humanize: 0.3 } } },
    { id: 'epic-lead', label: 'Epic lead', settings: {
      key: 2, bpm: 128, progression: { mood: 'epic', bars: 8 },
      chords: { type: 'triad' },
      arp: { pattern: 'converge', rate: 16, range: 3, gate: 0.8 },
      mirror: { on: true, vertical: true, where: 'twobar' } } },
    { id: 'bassline', label: 'Bassline', settings: {
      key: 4, bpm: 124, progression: { mood: 'dark' },
      chords: { type: 'power' },
      arp: { pattern: 'updown', rate: 16, range: 1, gate: 0.45, rests: 0.2 },
      levels: { accent: 0.6 }, fit: { low: 28, high: 52 } } },
    { id: 'dreamy-bells', label: 'Dreamy bells', settings: {
      key: 7, bpm: 96, progression: { mood: 'dreamy', chordBars: 2, bars: 8 },
      chords: { type: 'ninth', voicing: 'spread' },
      arp: { pattern: 'random', rate: 8, range: 2, gate: 1 },
      levels: { panVar: 0.6, velVar: 0.4 }, groove: { humanize: 0.4 } } }
  ];

  function applyPreset(id) {
    var preset = PRESETS.filter(function (p) { return p.id === id; })[0] || PRESETS[0];
    var s = defaults();
    var p = preset.settings;
    Object.keys(p).forEach(function (k) {
      if (p[k] && typeof p[k] === 'object') Object.assign(s[k], p[k]);
      else s[k] = p[k];
    });
    return normalize(s);
  }

  var api = {
    PPQ: PPQ, BAR: BAR, STEPS: STEPS, CONTROLS: CONTROLS, SCALES: SCALES, MOODS: MOODS,
    NOTE_NAMES: NOTE_NAMES, PRESETS: PRESETS,
    defaults: defaults, normalize: normalize, generate: generate, toMidi: toMidi,
    reroll: reroll, randomizeStep: randomizeStep, applyPreset: applyPreset,
    controlsFor: controlsFor, noteName: noteName, snapToScale: snapToScale,
    mulberry32: mulberry32
  };

  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.Riff = api;
})(typeof window !== 'undefined' ? window : this);
