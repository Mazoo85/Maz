/*
 * genres.js — the taste of the machine.
 *
 * Each genre describes how its music behaves: tempo range, which scales and
 * chord progressions it likes, how the drums sit, how the bass moves, and what
 * the synths sound like. The composer reads these; it does not hard-code any
 * one style.
 *
 * Drum patterns are 16 characters = one bar of sixteenth notes:
 *   X = accent   x = normal   o = soft / ghost   . = rest
 *
 * fx.sidechain is how hard the kick ducks the sustained parts. It is a genre
 * signature, not a polish setting: house lives on it, ambient and chiptune have
 * no such thing.
 */
(function (global) {
  'use strict';

  /* Synth voice presets. `kind` selects the synthesis model in synth.js. */
  function subtractive(o) {
    return Object.assign({
      kind: 'subtractive',
      osc: [{ type: 'sawtooth', detune: 0, gain: 1, octave: 0 }],
      sub: 0,
      filter: { type: 'lowpass', freq: 900, q: 3, env: 1400, attack: 0.004, decay: 0.25, sustain: 0.25 },
      amp: { a: 0.006, d: 0.12, s: 0.7, r: 0.2 },
      drive: 0, glide: 0, gain: 0.5,
      send: { rev: 0.15, del: 0.1 }
    }, o);
  }

  const PRESETS = {
    /* --- basses --- */
    lofiBass: subtractive({
      osc: [{ type: 'triangle', detune: 0, gain: 1, octave: 0 }, { type: 'sine', detune: 0, gain: 0.6, octave: -1 }],
      filter: { type: 'lowpass', freq: 420, q: 2, env: 500, attack: 0.01, decay: 0.3, sustain: 0.3 },
      amp: { a: 0.012, d: 0.3, s: 0.6, r: 0.18 }, gain: 0.62, send: { rev: 0.04, del: 0 }
    }),
    synthBass: subtractive({
      osc: [{ type: 'sawtooth', detune: -6, gain: 1, octave: 0 }, { type: 'sawtooth', detune: 7, gain: 0.8, octave: 0 }],
      sub: 0.5,
      filter: { type: 'lowpass', freq: 520, q: 6, env: 1600, attack: 0.004, decay: 0.16, sustain: 0.18 },
      amp: { a: 0.004, d: 0.1, s: 0.8, r: 0.08 }, drive: 0.25, gain: 0.6, send: { rev: 0.03, del: 0 }
    }),
    houseBass: subtractive({
      osc: [{ type: 'sawtooth', detune: 0, gain: 1, octave: 0 }],
      sub: 0.7,
      filter: { type: 'lowpass', freq: 360, q: 4, env: 900, attack: 0.003, decay: 0.12, sustain: 0.1 },
      amp: { a: 0.004, d: 0.09, s: 0.5, r: 0.06 }, gain: 0.6, send: { rev: 0.03, del: 0 }
    }),
    softBass: subtractive({
      osc: [{ type: 'sine', detune: 0, gain: 1, octave: 0 }],
      filter: { type: 'lowpass', freq: 300, q: 1, env: 200, attack: 0.08, decay: 0.6, sustain: 0.6 },
      amp: { a: 0.35, d: 0.6, s: 0.8, r: 0.9 }, gain: 0.5, send: { rev: 0.2, del: 0 }
    }),
    eight08: {
      kind: 'eight08', gain: 0.85, drive: 0.35, glide: 0.06,
      amp: { a: 0.004, d: 0.9, s: 0.0, r: 0.3 }, send: { rev: 0.02, del: 0 }
    },
    reese: subtractive({
      osc: [{ type: 'sawtooth', detune: -14, gain: 1, octave: 0 }, { type: 'sawtooth', detune: 12, gain: 1, octave: 0 },
            { type: 'square', detune: 0, gain: 0.5, octave: -1 }],
      filter: { type: 'lowpass', freq: 420, q: 8, env: 700, attack: 0.02, decay: 0.5, sustain: 0.5 },
      amp: { a: 0.01, d: 0.3, s: 0.85, r: 0.15 }, drive: 0.4, gain: 0.55, send: { rev: 0.05, del: 0 },
      width: 0.35, filterLfo: { rate: 0.9, depth: 180 }
    }),
    pulseBass: subtractive({
      osc: [{ type: 'square', detune: 0, gain: 1, octave: 0 }],
      filter: { type: 'lowpass', freq: 1400, q: 1, env: 0, attack: 0.001, decay: 0.05, sustain: 1 },
      amp: { a: 0.001, d: 0.05, s: 0.9, r: 0.02 }, gain: 0.42, send: { rev: 0, del: 0 }
    }),

    /* --- chords / pads --- */
    rhodes: {
      kind: 'epiano', gain: 0.4, amp: { a: 0.005, d: 1.6, s: 0.0, r: 0.5 },
      send: { rev: 0.3, del: 0.18 }, tone: 0.5
    },
    warmPad: subtractive({
      osc: [{ type: 'sawtooth', detune: -8, gain: 1, octave: 0 }, { type: 'sawtooth', detune: 9, gain: 1, octave: 0 },
            { type: 'triangle', detune: 0, gain: 0.5, octave: -1 }],
      filter: { type: 'lowpass', freq: 1100, q: 2, env: 700, attack: 0.6, decay: 1.5, sustain: 0.5 },
      amp: { a: 0.7, d: 1.2, s: 0.75, r: 1.4 }, gain: 0.3, send: { rev: 0.55, del: 0.12 },
      width: 0.55, filterLfo: { rate: 0.11, depth: 260 }
    }),
    glassPad: subtractive({
      osc: [{ type: 'triangle', detune: -5, gain: 1, octave: 0 }, { type: 'sine', detune: 6, gain: 0.8, octave: 1 }],
      filter: { type: 'lowpass', freq: 2200, q: 1, env: 600, attack: 1.2, decay: 2.5, sustain: 0.7 },
      amp: { a: 1.4, d: 2.0, s: 0.8, r: 2.6 }, gain: 0.26, send: { rev: 0.75, del: 0.2 },
      width: 0.7, filterLfo: { rate: 0.07, depth: 340 }
    }),
    stab: subtractive({
      osc: [{ type: 'sawtooth', detune: -5, gain: 1, octave: 0 }, { type: 'sawtooth', detune: 6, gain: 1, octave: 0 }],
      filter: { type: 'lowpass', freq: 900, q: 7, env: 2600, attack: 0.002, decay: 0.18, sustain: 0.0 },
      amp: { a: 0.003, d: 0.22, s: 0.0, r: 0.14 }, gain: 0.34, send: { rev: 0.3, del: 0.16 },
      width: 0.42
    }),
    strings: subtractive({
      osc: [{ type: 'sawtooth', detune: -11, gain: 1, octave: 0 }, { type: 'sawtooth', detune: 11, gain: 1, octave: 0 },
            { type: 'sawtooth', detune: 0, gain: 0.7, octave: 1 }],
      filter: { type: 'lowpass', freq: 1500, q: 1.5, env: 900, attack: 0.35, decay: 1.2, sustain: 0.6 },
      amp: { a: 0.42, d: 0.9, s: 0.85, r: 0.9 }, gain: 0.28, send: { rev: 0.6, del: 0.08 },
      width: 0.6, vibrato: { rate: 4.4, depth: 5, delay: 0.5 }
    }),
    chipChord: subtractive({
      osc: [{ type: 'square', detune: 0, gain: 1, octave: 0 }],
      filter: { type: 'lowpass', freq: 4000, q: 0.5, env: 0, attack: 0.001, decay: 0.05, sustain: 1 },
      amp: { a: 0.002, d: 0.08, s: 0.5, r: 0.05 }, gain: 0.2, send: { rev: 0.12, del: 0.1 }
    }),

    /* --- leads --- */
    softLead: subtractive({
      osc: [{ type: 'triangle', detune: 0, gain: 1, octave: 0 }, { type: 'sine', detune: 4, gain: 0.5, octave: 1 }],
      filter: { type: 'lowpass', freq: 1800, q: 2, env: 900, attack: 0.02, decay: 0.4, sustain: 0.4 },
      amp: { a: 0.02, d: 0.35, s: 0.6, r: 0.4 }, gain: 0.36, send: { rev: 0.4, del: 0.32 },
      width: 0.18, vibrato: { rate: 5.0, depth: 8, delay: 0.28 }
    }),
    sawLead: subtractive({
      osc: [{ type: 'sawtooth', detune: -7, gain: 1, octave: 0 }, { type: 'sawtooth', detune: 8, gain: 0.9, octave: 0 }],
      filter: { type: 'lowpass', freq: 2400, q: 5, env: 2200, attack: 0.008, decay: 0.5, sustain: 0.45 },
      amp: { a: 0.01, d: 0.3, s: 0.75, r: 0.35 }, drive: 0.2, gain: 0.34, send: { rev: 0.35, del: 0.35 },
      width: 0.3, vibrato: { rate: 5.6, depth: 11, delay: 0.24 }
    }),
    bell: { kind: 'bell', gain: 0.3, amp: { a: 0.002, d: 2.2, s: 0, r: 0.8 }, send: { rev: 0.55, del: 0.3 } },
    pluck: {
      kind: 'pluck', gain: 0.36, amp: { a: 0.001, d: 0.5, s: 0, r: 0.25 },
      send: { rev: 0.3, del: 0.28 }, tone: 0.55
    },
    chipLead: subtractive({
      osc: [{ type: 'square', detune: 0, gain: 1, octave: 0 }],
      filter: { type: 'lowpass', freq: 6000, q: 0.5, env: 0, attack: 0.001, decay: 0.05, sustain: 1 },
      amp: { a: 0.002, d: 0.06, s: 0.85, r: 0.03 }, gain: 0.3, send: { rev: 0.14, del: 0.26 }
    }),
    piano: { kind: 'epiano', gain: 0.34, amp: { a: 0.003, d: 1.1, s: 0.0, r: 0.35 }, send: { rev: 0.4, del: 0.12 }, tone: 0.8 }
  };

  /*
   * Chord progressions are scale-degree indices (0 = tonic). The same numbers
   * read as I-vi-IV-V in a major scale and i-VI-iv-v in a minor one, which is
   * exactly what we want.
   */
  const PROG = {
    popMajor:   [[0, 5, 3, 4], [5, 3, 0, 4], [0, 4, 5, 3], [0, 3, 5, 4], [3, 4, 5, 0]],
    minorEpic:  [[0, 5, 2, 6], [0, 6, 5, 4], [0, 3, 5, 4], [5, 6, 0, 0], [0, 2, 5, 6]],
    jazzy:      [[1, 4, 0, 0], [1, 4, 2, 5], [0, 5, 1, 4], [3, 6, 1, 4], [5, 1, 4, 0]],
    modal:      [[0, 6, 0, 3], [0, 3, 6, 0], [0, 5, 6, 3], [0, 4, 3, 0]],
    ambientDrift: [[0, 3, 5, 4], [0, 5, 3, 3], [0, 2, 3, 5], [0, 4, 0, 5]],
    darkTrap:   [[0, 5, 3, 6], [0, 0, 5, 4], [0, 6, 3, 3], [0, 3, 6, 5]]
  };

  const GENRES = {
    lofi: {
      id: 'lofi', name: 'Lo-Fi Chill', blurb: 'Dusty keys, swung drums, tape hiss.',
      bpm: [68, 86], swing: 0.17,
      scales: [['dorian', 3], ['minor', 3], ['major', 1], ['mixolydian', 1]],
      progressions: PROG.jazzy.concat(PROG.modal), chordShapes: [['seventh', 4], ['ninth', 3], ['sixth', 1]],
      barsPerChord: [1, 2],
      bass: { style: 'walk', octave: 2, preset: 'lofiBass' },
      chords: { style: 'keys', preset: 'rhodes', octaveLow: 55, octaveHigh: 79 },
      pad: { preset: 'warmPad', gain: 0.5 },
      lead: { preset: 'softLead', octave: 5, density: 0.55, restBias: 0.35 },
      arp: { preset: 'pluck', rate: 0.5, octave: 5, chance: 0.35 },
      builds: false,
      drums: {
        kit: 'lofi',
        intro:  { kick: 'x.......x.......', hh: 'x...x...x...x...', snare: '................' },
        groove: { kick: 'x.......x.x.....', snare: '....x.......x...', hh: 'x.o.x.o.x.o.x.o.', perc: '..............o.' },
        full:   { kick: 'x..x....x.x...x.', snare: '....x.......x..o', hh: 'xoxoxoxoxoxoxoxo', perc: '....o.......o...' },
        fill:   { kick: 'x.......x.......', snare: '........x.x.xxxx', hh: 'x.x.x.x.........' }
      },
      fx: { reverb: 0.35, delay: 0.22, delayTime: 0.5, vinyl: 0.5, master: 1.15, sidechain: 0.14, brightness: 0.75 }
    },

    synthwave: {
      id: 'synthwave', name: 'Synthwave', blurb: 'Neon arpeggios and a driving pulse.',
      bpm: [98, 118], swing: 0,
      scales: [['minor', 4], ['dorian', 2], ['harmonicMinor', 1], ['major', 1]],
      progressions: PROG.minorEpic.concat(PROG.popMajor), chordShapes: [['triad', 3], ['seventh', 2], ['sus4', 1]],
      barsPerChord: [1, 2],
      bass: { style: 'root8', octave: 2, preset: 'synthBass' },
      chords: { style: 'pad', preset: 'warmPad', octaveLow: 55, octaveHigh: 81 },
      pad: { preset: 'strings', gain: 0.45 },
      lead: { preset: 'sawLead', octave: 5, density: 0.7, restBias: 0.2 },
      arp: { preset: 'pluck', rate: 0.25, octave: 5, chance: 0.85 },
      builds: true,
      drums: {
        kit: 'electro',
        intro:  { kick: 'x.......x.......', hh: '..x...x...x...x.', clap: '................' },
        groove: { kick: 'x...x...x...x...', clap: '....x.......x...', hh: 'x.x.x.x.x.x.x.x.', oh: '......x.......x.' },
        full:   { kick: 'x...x...x...x..x', clap: '....x.......x...', hh: 'xoxoxoxoxoxoxoxo', oh: '......x.......x.', crash: 'x...............' },
        fill:   { kick: 'x...x...........', snare: '........x.x.xxxx', crash: '................' }
      },
      fx: { reverb: 0.42, delay: 0.3, delayTime: 0.375, vinyl: 0, master: 0.94, sidechain: 0.42, brightness: 1.05 }
    },

    house: {
      id: 'house', name: 'Deep House', blurb: 'Four on the floor, warm stabs, rolling bass.',
      bpm: [118, 126], swing: 0.06,
      scales: [['minor', 3], ['dorian', 3], ['major', 1]],
      progressions: PROG.jazzy.concat(PROG.minorEpic), chordShapes: [['seventh', 4], ['ninth', 3], ['triad', 1]],
      barsPerChord: [1, 2],
      bass: { style: 'offbeat', octave: 2, preset: 'houseBass' },
      chords: { style: 'stab', preset: 'stab', octaveLow: 57, octaveHigh: 83 },
      pad: { preset: 'warmPad', gain: 0.4 },
      lead: { preset: 'pluck', octave: 5, density: 0.5, restBias: 0.35 },
      arp: { preset: 'pluck', rate: 0.25, octave: 5, chance: 0.5 },
      builds: true,
      drums: {
        kit: 'house',
        intro:  { kick: 'x...x...x...x...', hh: '..x...x...x...x.' },
        groove: { kick: 'x...x...x...x...', clap: '....x.......x...', hh: 'x.o.x.o.x.o.x.o.', oh: '..x...x...x...x.' },
        full:   { kick: 'x...x...x...x...', clap: '....x.......x...', hh: 'xoxoxoxoxoxoxoxo', oh: '..x...x...x...x.', perc: '...x......x.....' },
        fill:   { kick: 'x...x...x.......', snare: '........x.x.xxxx', oh: '..............x.' }
      },
      fx: { reverb: 0.4, delay: 0.24, delayTime: 0.375, vinyl: 0, master: 1.02, sidechain: 0.58, brightness: 1.0 }
    },

    ambient: {
      id: 'ambient', name: 'Ambient', blurb: 'Slow bloom, long tails, almost no drums.',
      bpm: [58, 76], swing: 0,
      scales: [['lydian', 3], ['major', 2], ['dorian', 2], ['minor', 2]],
      progressions: PROG.ambientDrift, chordShapes: [['ninth', 3], ['seventh', 3], ['sus2', 2], ['triad', 1]],
      barsPerChord: [2, 4],
      bass: { style: 'whole', octave: 2, preset: 'softBass' },
      chords: { style: 'pad', preset: 'glassPad', octaveLow: 55, octaveHigh: 84 },
      pad: { preset: 'warmPad', gain: 0.55 },
      lead: { preset: 'bell', octave: 6, density: 0.25, restBias: 0.6 },
      arp: { preset: 'bell', rate: 1.0, octave: 6, chance: 0.4 },
      builds: false,
      drums: {
        kit: 'soft',
        intro:  {},
        groove: { kick: 'x...............', perc: '........o.......' },
        full:   { kick: 'x.......x.......', perc: '....o.......o...', shaker: 'o.o.o.o.o.o.o.o.' },
        fill:   { perc: '............o.o.' }
      },
      fx: { reverb: 0.8, delay: 0.35, delayTime: 0.75, vinyl: 0.15, master: 1.31, sidechain: 0, brightness: 0.9 }
    },

    cinematic: {
      id: 'cinematic', name: 'Cinematic', blurb: 'Strings, piano and a rising pulse.',
      bpm: [76, 96], swing: 0,
      scales: [['minor', 3], ['harmonicMinor', 2], ['dorian', 2], ['major', 1]],
      progressions: PROG.minorEpic.concat(PROG.popMajor), chordShapes: [['triad', 3], ['seventh', 2], ['sus4', 2]],
      barsPerChord: [1, 2],
      bass: { style: 'whole', octave: 2, preset: 'softBass' },
      chords: { style: 'swell', preset: 'strings', octaveLow: 52, octaveHigh: 79 },
      pad: { preset: 'glassPad', gain: 0.5 },
      lead: { preset: 'piano', octave: 5, density: 0.6, restBias: 0.3 },
      arp: { preset: 'piano', rate: 0.5, octave: 5, chance: 0.7 },
      builds: true,
      drums: {
        kit: 'epic',
        intro:  { tom: 'x.......x.......' },
        groove: { kick: 'x..x..x...x.....', tom: '....o.......o...', snare: '................' },
        full:   { kick: 'x.x.x.x.x.x.x.x.', snare: '........x.......', tom: 'o...o...o...o...', crash: 'x...............' },
        fill:   { tom: 'x.x.x.xxx.x.xxxx', crash: '................' }
      },
      fx: { reverb: 0.65, delay: 0.18, delayTime: 0.5, vinyl: 0, master: 1.10, sidechain: 0.12, brightness: 0.95 }
    },

    chiptune: {
      id: 'chiptune', name: 'Chiptune', blurb: '8-bit squares, fast arps, noise drums.',
      bpm: [128, 158], swing: 0,
      scales: [['major', 3], ['minor', 3], ['mixolydian', 1], ['lydian', 1]],
      progressions: PROG.popMajor.concat(PROG.minorEpic), chordShapes: [['triad', 4], ['seventh', 1]],
      barsPerChord: [1, 1],
      bass: { style: 'pulse8', octave: 2, preset: 'pulseBass' },
      chords: { style: 'arpChord', preset: 'chipChord', octaveLow: 60, octaveHigh: 84 },
      pad: { preset: 'chipChord', gain: 0.3 },
      lead: { preset: 'chipLead', octave: 5, density: 0.85, restBias: 0.12 },
      arp: { preset: 'chipChord', rate: 0.25, octave: 5, chance: 0.9 },
      builds: false,
      drums: {
        kit: 'chip',
        intro:  { kick: 'x.......x.......', hh: 'x...x...x...x...' },
        groove: { kick: 'x...x...x...x...', snare: '....x.......x...', hh: 'x.x.x.x.x.x.x.x.' },
        full:   { kick: 'x..xx...x..xx...', snare: '....x.......x...', hh: 'xoxoxoxoxoxoxoxo' },
        fill:   { kick: 'x.......x.......', snare: '....x...x.x.xxxx', hh: '................' }
      },
      fx: { reverb: 0.16, delay: 0.26, delayTime: 0.1875, vinyl: 0, master: 1.38, sidechain: 0, brightness: 1.15 }
    },

    dnb: {
      id: 'dnb', name: 'Drum & Bass', blurb: 'Breakbeats over a growling reese.',
      bpm: [168, 176], swing: 0,
      scales: [['minor', 4], ['phrygian', 2], ['dorian', 2]],
      progressions: PROG.minorEpic.concat(PROG.modal), chordShapes: [['seventh', 3], ['ninth', 2], ['triad', 2]],
      barsPerChord: [2, 4],
      bass: { style: 'sustain', octave: 1, preset: 'reese' },
      chords: { style: 'pad', preset: 'glassPad', octaveLow: 60, octaveHigh: 84 },
      pad: { preset: 'warmPad', gain: 0.4 },
      lead: { preset: 'bell', octave: 5, density: 0.35, restBias: 0.5 },
      arp: { preset: 'pluck', rate: 0.25, octave: 5, chance: 0.45 },
      builds: true,
      drums: {
        kit: 'break',
        intro:  { kick: 'x.......x.......', hh: 'x.x.x.x.x.x.x.x.' },
        groove: { kick: 'x.....x...x.....', snare: '....x.......x...', hh: 'x.x.x.x.x.x.x.x.' },
        full:   { kick: 'x.....x..x.x....', snare: '....x..o....x..o', hh: 'xoxoxoxoxoxoxoxo', oh: '..........x.....' },
        fill:   { kick: 'x.......x.......', snare: '....x...x.x.xxxx', hh: '................' }
      },
      fx: { reverb: 0.45, delay: 0.28, delayTime: 0.375, vinyl: 0, master: 1.19, sidechain: 0.34, brightness: 1.05 }
    },

    trap: {
      id: 'trap', name: 'Trap', blurb: 'Sliding 808s, hi-hat rolls, half-time snare.',
      bpm: [130, 150], swing: 0,
      scales: [['minor', 3], ['phrygian', 3], ['harmonicMinor', 2]],
      progressions: PROG.darkTrap, chordShapes: [['triad', 3], ['seventh', 2], ['sus4', 1]],
      barsPerChord: [2, 2],
      bass: { style: 'slide808', octave: 1, preset: 'eight08' },
      chords: { style: 'pad', preset: 'glassPad', octaveLow: 60, octaveHigh: 84 },
      pad: { preset: 'warmPad', gain: 0.35 },
      lead: { preset: 'bell', octave: 6, density: 0.45, restBias: 0.45 },
      arp: { preset: 'bell', rate: 0.5, octave: 5, chance: 0.55 },
      hatRolls: true,
      builds: true,
      drums: {
        kit: 'trap',
        intro:  { kick: 'x.......x.......', hh: 'x...x...x...x...' },
        groove: { kick: 'x.....x...x.....', snare: '........x.......', hh: 'x.x.x.x.x.x.x.x.' },
        full:   { kick: 'x..x..x...x..x..', snare: '........x.......', hh: 'xoxoxoxoxoxoxoxo', oh: '..............x.' },
        fill:   { kick: 'x.......x.......', snare: '........x...xxxx', hh: 'x.x.x.x.........' }
      },
      fx: { reverb: 0.35, delay: 0.2, delayTime: 0.375, vinyl: 0, master: 1.17, sidechain: 0.46, brightness: 0.95 }
    }
  };

  /*
   * Moods bend a genre without replacing it: brighter or darker scale choices,
   * busier or sparser parts, more or less air around everything.
   */
  const MOODS = {
    chill:     { name: 'Chill',     bpm: -6, density: -0.15, brightness: 0.85, reverb: 1.15, scaleBias: 'minorish', extBias: 0.6, blurb: 'relaxed and unhurried' },
    uplifting: { name: 'Uplifting', bpm: +4, density: +0.1,  brightness: 1.15, reverb: 0.95, scaleBias: 'bright',   extBias: 0.3, blurb: 'bright and open' },
    dark:      { name: 'Dark',      bpm: -2, density: 0.0,   brightness: 0.7,  reverb: 1.05, scaleBias: 'dark',     extBias: 0.35, blurb: 'heavy and shadowed' },
    dreamy:    { name: 'Dreamy',    bpm: -4, density: -0.1,  brightness: 0.95, reverb: 1.35, scaleBias: 'bright',   extBias: 0.7, blurb: 'soft-focus and floating' },
    driving:   { name: 'Driving',   bpm: +6, density: +0.2,  brightness: 1.1,  reverb: 0.85, scaleBias: 'minorish', extBias: 0.35, blurb: 'urgent and forward-leaning' }
  };

  global.Genres = {
    GENRES: GENRES,
    MOODS: MOODS,
    PRESETS: PRESETS,
    list: function () { return Object.keys(GENRES).map(function (k) { return GENRES[k]; }); },
    moodList: function () { return Object.keys(MOODS).map(function (k) { return Object.assign({ id: k }, MOODS[k]); }); }
  };
})(window);
