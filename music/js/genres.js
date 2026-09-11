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
 * A part's `alts` are other instruments that suit the style. The composer picks
 * from them per song, so two lo-fi tracks are not the same four sounds twice.
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

    /* --- strings & mallets --- */
    guitar: subtractive({
      osc: [{ wave: 'guitar', detune: -4, gain: 1, octave: 0 },
            { wave: 'guitar', detune: 5, gain: 0.7, octave: 0 }],
      filter: { type: 'lowpass', freq: 1400, q: 2, env: 2600, attack: 0.003, decay: 0.35, sustain: 0.12 },
      amp: { a: 0.004, d: 0.7, s: 0.12, r: 0.35 }, gain: 0.34, width: 0.25,
      send: { rev: 0.28, del: 0.14 }
    }),
    harp: subtractive({
      osc: [{ wave: 'glass', detune: 0, gain: 1, octave: 0 },
            { wave: 'guitar', detune: 6, gain: 0.4, octave: 0 }],
      filter: { type: 'lowpass', freq: 2600, q: 1.2, env: 2200, attack: 0.002, decay: 0.6, sustain: 0.1 },
      amp: { a: 0.003, d: 1.5, s: 0.0, r: 0.9 }, gain: 0.3, width: 0.4,
      send: { rev: 0.5, del: 0.2 }
    }),
    nylon: subtractive({
      osc: [{ wave: 'reed', detune: 0, gain: 1, octave: 0 },
            { wave: 'guitar', detune: -6, gain: 0.5, octave: 0 }],
      filter: { type: 'lowpass', freq: 1000, q: 1.5, env: 1500, attack: 0.004, decay: 0.3, sustain: 0.1 },
      amp: { a: 0.005, d: 0.8, s: 0.08, r: 0.4 }, gain: 0.32, width: 0.18,
      send: { rev: 0.34, del: 0.1 }
    }),
    marimba: {
      kind: 'mallet', gain: 0.42, amp: { a: 0.001, d: 0.5, s: 0, r: 0.3 },
      partials: [[1, 1, 1], [3.9, 0.4, 0.45], [9.2, 0.16, 0.22]],
      send: { rev: 0.3, del: 0.12 }
    },
    vibes: {
      kind: 'mallet', gain: 0.4, amp: { a: 0.002, d: 1.8, s: 0, r: 0.9 },
      partials: [[1, 1, 1], [4, 0.35, 0.6], [10, 0.12, 0.3]],
      tremolo: { rate: 4.6, depth: 0.3 },
      send: { rev: 0.5, del: 0.18 }
    },

    /* --- winds, voices & keys --- */
    organ: subtractive({
      osc: [{ wave: 'organ', detune: 0, gain: 1, octave: 0 },
            { wave: 'organ', detune: 7, gain: 0.5, octave: 1 }],
      filter: { type: 'lowpass', freq: 3200, q: 0.6, env: 0, attack: 0.002, decay: 0.1, sustain: 1 },
      amp: { a: 0.02, d: 0.1, s: 0.95, r: 0.12 }, gain: 0.26, width: 0.3,
      tremolo: { rate: 5.6, depth: 0.16 },
      send: { rev: 0.3, del: 0.08 }
    }),
    brass: subtractive({
      osc: [{ wave: 'brass', detune: -6, gain: 1, octave: 0 },
            { wave: 'brass', detune: 7, gain: 0.8, octave: 0 }],
      filter: { type: 'lowpass', freq: 700, q: 2, env: 3200, attack: 0.06, decay: 0.5, sustain: 0.5 },
      amp: { a: 0.05, d: 0.3, s: 0.85, r: 0.25 }, gain: 0.3, width: 0.35,
      vibrato: { rate: 5.2, depth: 6, delay: 0.4 },
      send: { rev: 0.4, del: 0.1 }
    }),
    reedLead: subtractive({
      osc: [{ wave: 'reed', detune: 0, gain: 1, octave: 0 }],
      filter: { type: 'lowpass', freq: 2000, q: 1.4, env: 1200, attack: 0.03, decay: 0.4, sustain: 0.6 },
      amp: { a: 0.04, d: 0.2, s: 0.9, r: 0.2 }, gain: 0.32, width: 0.12,
      vibrato: { rate: 5.4, depth: 9, delay: 0.35 },
      send: { rev: 0.42, del: 0.2 }
    }),
    choirPad: {
      kind: 'choir', gain: 0.3, amp: { a: 0.9, d: 1.2, s: 0.85, r: 1.6 },
      formants: [[730, 1], [1090, 0.5], [2440, 0.22]], formantQ: 8, breath: 0.05,
      vibrato: { rate: 4.4, depth: 5, delay: 0.6 },
      send: { rev: 0.7, del: 0.14 }
    },
    voxLead: {
      kind: 'choir', gain: 0.3, amp: { a: 0.12, d: 0.4, s: 0.8, r: 0.5 },
      formants: [[660, 1], [1720, 0.6], [2410, 0.3]], formantQ: 10, breath: 0.09,
      vibrato: { rate: 5.6, depth: 10, delay: 0.28 },
      send: { rev: 0.5, del: 0.28 }
    },
    fmKeys: {
      kind: 'fm', ratio: 2, index: 2.4, gain: 0.34,
      amp: { a: 0.003, d: 1.3, s: 0.12, r: 0.5 },
      send: { rev: 0.34, del: 0.16 }
    },
    fmBright: {
      kind: 'fm', ratio: 3, index: 3.2, gain: 0.3,
      amp: { a: 0.004, d: 0.9, s: 0.2, r: 0.4 },
      send: { rev: 0.4, del: 0.3 }
    },
    glassBell: {
      kind: 'fm', ratio: 5.1, index: 4, gain: 0.26,
      amp: { a: 0.002, d: 2.6, s: 0, r: 1.2 },
      send: { rev: 0.6, del: 0.3 }
    },
    supersaw: subtractive({
      osc: [{ type: 'sawtooth', detune: -22, gain: 0.8, octave: 0 },
            { type: 'sawtooth', detune: -9, gain: 1, octave: 0 },
            { type: 'sawtooth', detune: 0, gain: 1, octave: 0 },
            { type: 'sawtooth', detune: 10, gain: 1, octave: 0 },
            { type: 'sawtooth', detune: 23, gain: 0.8, octave: 0 }],
      filter: { type: 'lowpass', freq: 1400, q: 3, env: 3400, attack: 0.01, decay: 0.7, sustain: 0.5 },
      amp: { a: 0.02, d: 0.4, s: 0.8, r: 0.5 }, gain: 0.22, width: 0.6,
      send: { rev: 0.35, del: 0.3 }
    }),

    /* --- more bottom end --- */
    fmBass: {
      kind: 'fm', ratio: 1, index: 1.4, gain: 0.6, drive: 0.2,
      amp: { a: 0.004, d: 0.35, s: 0.5, r: 0.12 },
      send: { rev: 0.03, del: 0 }
    },
    pickBass: subtractive({
      osc: [{ wave: 'guitar', detune: 0, gain: 1, octave: 0 },
            { type: 'sine', detune: 0, gain: 0.7, octave: -1 }],
      filter: { type: 'lowpass', freq: 620, q: 3, env: 1400, attack: 0.004, decay: 0.22, sustain: 0.18 },
      amp: { a: 0.005, d: 0.35, s: 0.45, r: 0.15 }, gain: 0.58,
      send: { rev: 0.05, del: 0 }
    }),
    organBass: subtractive({
      osc: [{ wave: 'organ', detune: 0, gain: 1, octave: 0 }],
      filter: { type: 'lowpass', freq: 500, q: 1.2, env: 300, attack: 0.01, decay: 0.2, sustain: 0.7 },
      amp: { a: 0.01, d: 0.15, s: 0.9, r: 0.12 }, gain: 0.5,
      send: { rev: 0.04, del: 0 }
    }),
    piano: { kind: 'epiano', gain: 0.34, amp: { a: 0.003, d: 1.1, s: 0.0, r: 0.35 }, send: { rev: 0.4, del: 0.12 }, tone: 0.8 }
  };

  /* Human names for the presets, and which ones suit which part. A bass patch
     on the lead is a legitimate choice; a pad on the bass is just mud. */
  const PRESET_LABEL = {
    lofiBass: 'Soft bass', synthBass: 'Synth bass', houseBass: 'Sub bass',
    softBass: 'Sine bass', eight08: '808', reese: 'Reese growl', pulseBass: 'Chip bass',
    fmBass: 'FM bass', pickBass: 'Picked bass', organBass: 'Organ bass',
    rhodes: 'Electric piano', warmPad: 'Warm pad', glassPad: 'Glass pad',
    stab: 'Stab', strings: 'Strings', chipChord: 'Chip chords', piano: 'Piano',
    softLead: 'Soft lead', sawLead: 'Saw lead', bell: 'Bell', pluck: 'Pluck',
    chipLead: 'Chip lead',
    guitar: 'Guitar', harp: 'Harp', nylon: 'Nylon guitar',
    marimba: 'Marimba', vibes: 'Vibraphone',
    organ: 'Organ', brass: 'Brass', reedLead: 'Reed',
    choirPad: 'Choir', voxLead: 'Voice',
    fmKeys: 'FM keys', fmBright: 'FM lead', glassBell: 'Glass bell',
    supersaw: 'Supersaw'
  };

  const PRESET_GROUPS = {
    bass:   ['lofiBass', 'synthBass', 'houseBass', 'softBass', 'eight08', 'reese',
             'pulseBass', 'fmBass', 'pickBass', 'organBass'],
    chords: ['rhodes', 'piano', 'warmPad', 'glassPad', 'stab', 'strings', 'chipChord',
             'pluck', 'guitar', 'nylon', 'organ', 'brass', 'choirPad', 'fmKeys', 'vibes', 'supersaw'],
    arp:    ['pluck', 'bell', 'chipChord', 'chipLead', 'piano', 'softLead', 'rhodes',
             'guitar', 'harp', 'marimba', 'vibes', 'glassBell', 'organ', 'fmKeys'],
    lead:   ['softLead', 'sawLead', 'bell', 'pluck', 'chipLead', 'piano', 'rhodes',
             'guitar', 'nylon', 'brass', 'reedLead', 'voxLead', 'fmBright', 'glassBell',
             'supersaw', 'marimba', 'vibes', 'organ'],
    pad:    ['warmPad', 'glassPad', 'strings', 'chipChord', 'rhodes', 'choirPad',
             'organ', 'brass', 'harp', 'supersaw']
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
    darkTrap:   [[0, 5, 3, 6], [0, 0, 5, 4], [0, 6, 3, 3], [0, 3, 6, 5]],
    // I-IV-V and the flat-seven turnarounds rock lives on.
    rock:       [[0, 3, 4, 4], [0, 5, 3, 4], [0, 6, 3, 0], [5, 3, 0, 4], [0, 3, 0, 4]],
    // Twelve-bar-ish motion, plus the dominant-heavy moves blues shares.
    blues:      [[0, 0, 3, 0], [0, 3, 0, 4], [3, 3, 0, 0], [4, 3, 0, 0]],
    // One or two chords held for a long time — funk is rhythm, not harmony.
    funk:       [[0, 0, 3, 0], [0, 3, 0, 0], [0, 0, 0, 4], [1, 4, 0, 0]],
    // ii-V-I chains and the descending seconds of bossa nova.
    bossa:      [[1, 4, 0, 0], [1, 4, 3, 6], [0, 6, 1, 4], [3, 6, 1, 4], [0, 1, 4, 0]],
    // The four chords behind most gospel and soul: plagal, warm, resolving.
    gospel:     [[0, 3, 4, 0], [0, 5, 1, 4], [3, 4, 5, 0], [0, 3, 0, 4]],
    // Relentless and static — techno moves by texture, not by chord.
    techno:     [[0, 0, 0, 0], [0, 0, 5, 5], [0, 5, 0, 5], [0, 0, 3, 3]],
    // Disco and its descendants: busy, major-leaning, always turning over.
    disco:      [[1, 4, 0, 5], [0, 5, 1, 4], [5, 1, 4, 0], [0, 3, 1, 4]],
    // Two-chord loops with a minor pull, the backbone of drill and afrobeat.
    loop2:      [[0, 5, 0, 5], [0, 3, 0, 3], [0, 6, 0, 6], [0, 4, 0, 4]],
    // Country: major, honest, and home by the end of the bar.
    country:    [[0, 4, 0, 0], [0, 3, 4, 0], [0, 0, 4, 4], [5, 3, 0, 4]]
  };

  const GENRES = {
    lofi: {
      id: 'lofi', name: 'Lo-Fi Chill', blurb: 'Dusty keys, swung drums, tape hiss.',
      bpm: [68, 86], swing: 0.17,
      meters: [['4/4', 9], ['3/4', 1]],
      modulates: 0.0, borrow: 0.14,
      riff: 0.15,
      halfTime: 0.15,
      scales: [['dorian', 3], ['minor', 3], ['major', 1], ['mixolydian', 1]],
      progressions: PROG.jazzy.concat(PROG.modal), chordShapes: [['seventh', 4], ['ninth', 3], ['sixth', 1]],
      barsPerChord: [1, 2],
      bass: { style: 'walk', octave: 2, preset: 'lofiBass', alts: ['pickBass', 'organBass'] },
      chords: { style: 'keys', preset: 'rhodes', octaveLow: 55, octaveHigh: 79, alts: ['nylon', 'vibes', 'organ'] },
      pad: { preset: 'warmPad', gain: 0.5 },
      counter: { chance: 0.35, preset: 'nylon', octave: 4, alts: ['vibes', 'rhodes', 'marimba'] },
      lead: { preset: 'softLead', octave: 5, density: 0.55, restBias: 0.35, alts: ['nylon', 'vibes', 'reedLead'] },
      arp: { preset: 'pluck', rate: 0.5, octave: 5, chance: 0.35, alts: ['guitar', 'marimba', 'harp'] },
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
      meters: [['4/4', 1]],
      modulates: 0.12, borrow: 0.05,
      riff: 0.3,
      halfTime: 0.15,
      scales: [['minor', 4], ['dorian', 2], ['harmonicMinor', 1], ['major', 1]],
      progressions: PROG.minorEpic.concat(PROG.popMajor), chordShapes: [['triad', 3], ['seventh', 2], ['sus4', 1]],
      barsPerChord: [1, 2],
      bass: { style: 'root8', octave: 2, preset: 'synthBass', alts: ['fmBass'] },
      chords: { style: 'pad', preset: 'warmPad', octaveLow: 55, octaveHigh: 81 },
      pad: { preset: 'strings', gain: 0.45, alts: ['supersaw', 'choirPad'] },
      counter: { chance: 0.3, preset: 'softLead', octave: 4, alts: ['bell', 'fmKeys'] },
      lead: { preset: 'sawLead', octave: 5, density: 0.7, restBias: 0.2, alts: ['supersaw', 'fmBright', 'brass'] },
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
      meters: [['4/4', 1]],
      modulates: 0.05, borrow: 0.04,
      riff: 0.2,
      halfTime: 0.1,
      scales: [['minor', 3], ['dorian', 3], ['major', 1]],
      progressions: PROG.jazzy.concat(PROG.minorEpic), chordShapes: [['seventh', 4], ['ninth', 3], ['triad', 1]],
      barsPerChord: [1, 2],
      bass: { style: 'offbeat', octave: 2, preset: 'houseBass', alts: ['organBass', 'fmBass'] },
      chords: { style: 'stab', preset: 'stab', octaveLow: 57, octaveHigh: 83, alts: ['organ', 'rhodes', 'fmKeys'] },
      pad: { preset: 'warmPad', gain: 0.4 },
      counter: { chance: 0.25, preset: 'pluck', octave: 4, alts: ['bell', 'fmKeys'] },
      lead: { preset: 'pluck', octave: 5, density: 0.5, restBias: 0.35, alts: ['voxLead', 'organ', 'marimba'] },
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
      meters: [['4/4', 5], ['3/4', 2], ['6/8', 2], ['5/4', 1]],
      modulates: 0.0, borrow: 0.02,
      riff: 0.05,
      halfTime: 0.05,
      scales: [['lydian', 3], ['major', 2], ['dorian', 2], ['minor', 2]],
      progressions: PROG.ambientDrift, chordShapes: [['ninth', 3], ['seventh', 3], ['sus2', 2], ['triad', 1]],
      barsPerChord: [2, 4],
      bass: { style: 'whole', octave: 2, preset: 'softBass' },
      chords: { style: 'pad', preset: 'glassPad', octaveLow: 55, octaveHigh: 84, alts: ['choirPad', 'organ'] },
      pad: { preset: 'warmPad', gain: 0.55 },
      counter: { chance: 0.4, preset: 'glassBell', octave: 5, alts: ['bell', 'harp'] },
      lead: { preset: 'bell', octave: 6, density: 0.25, restBias: 0.6, alts: ['glassBell', 'vibes', 'harp'] },
      arp: { preset: 'bell', rate: 1.0, octave: 6, chance: 0.4, alts: ['harp', 'marimba', 'glassBell'] },
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
      meters: [['4/4', 5], ['3/4', 2], ['6/8', 1], ['5/4', 1], ['7/8', 1]],
      modulates: 0.3, borrow: 0.1,
      riff: 0.1,
      halfTime: 0.3,
      scales: [['minor', 3], ['harmonicMinor', 2], ['dorian', 2], ['major', 1]],
      progressions: PROG.minorEpic.concat(PROG.popMajor), chordShapes: [['triad', 3], ['seventh', 2], ['sus4', 2]],
      barsPerChord: [1, 2],
      bass: { style: 'whole', octave: 2, preset: 'softBass' },
      chords: { style: 'swell', preset: 'strings', octaveLow: 52, octaveHigh: 79, alts: ['choirPad', 'brass'] },
      pad: { preset: 'glassPad', gain: 0.5, alts: ['choirPad', 'brass'] },
      counter: { chance: 0.55, preset: 'strings', octave: 4, alts: ['brass', 'choirPad', 'harp'] },
      lead: { preset: 'piano', octave: 5, density: 0.6, restBias: 0.3, alts: ['brass', 'voxLead', 'reedLead'] },
      arp: { preset: 'piano', rate: 0.5, octave: 5, chance: 0.7, alts: ['harp', 'marimba'] },
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
      meters: [['4/4', 8], ['3/4', 1], ['7/8', 1]],
      modulates: 0.25, borrow: 0.06,
      riff: 0.4,
      halfTime: 0.1,
      scales: [['major', 3], ['minor', 3], ['mixolydian', 1], ['lydian', 1]],
      progressions: PROG.popMajor.concat(PROG.minorEpic), chordShapes: [['triad', 4], ['seventh', 1]],
      barsPerChord: [1, 1],
      bass: { style: 'pulse8', octave: 2, preset: 'pulseBass' },
      chords: { style: 'arpChord', preset: 'chipChord', octaveLow: 60, octaveHigh: 84 },
      pad: { preset: 'chipChord', gain: 0.3 },
      counter: { chance: 0.45, preset: 'chipLead', octave: 4, alts: ['chipChord'] },
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
      meters: [['4/4', 9], ['7/8', 1]],
      modulates: 0.05, borrow: 0.05,
      riff: 0.3,
      halfTime: 0.35,
      scales: [['minor', 4], ['phrygian', 2], ['dorian', 2]],
      progressions: PROG.minorEpic.concat(PROG.modal), chordShapes: [['seventh', 3], ['ninth', 2], ['triad', 2]],
      barsPerChord: [2, 4],
      bass: { style: 'sustain', octave: 1, preset: 'reese' },
      chords: { style: 'pad', preset: 'glassPad', octaveLow: 60, octaveHigh: 84, alts: ['choirPad', 'supersaw'] },
      pad: { preset: 'warmPad', gain: 0.4 },
      counter: { chance: 0.25, preset: 'bell', octave: 5, alts: ['pluck', 'glassBell'] },
      lead: { preset: 'bell', octave: 5, density: 0.35, restBias: 0.5, alts: ['glassBell', 'voxLead', 'fmBright'] },
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
      meters: [['4/4', 1]],
      modulates: 0.0, borrow: 0.03,
      riff: 0.25,
      halfTime: 0.4,
      scales: [['minor', 3], ['phrygian', 3], ['harmonicMinor', 2]],
      progressions: PROG.darkTrap, chordShapes: [['triad', 3], ['seventh', 2], ['sus4', 1]],
      barsPerChord: [2, 2],
      bass: { style: 'slide808', octave: 1, preset: 'eight08' },
      chords: { style: 'pad', preset: 'glassPad', octaveLow: 60, octaveHigh: 84, alts: ['choirPad', 'organ'] },
      pad: { preset: 'warmPad', gain: 0.35 },
      counter: { chance: 0.3, preset: 'bell', octave: 5, alts: ['glassBell', 'pluck'] },
      lead: { preset: 'bell', octave: 6, density: 0.45, restBias: 0.45, alts: ['glassBell', 'marimba', 'fmKeys'] },
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
    },

    rock: {
      id: 'rock', name: 'Rock', blurb: 'Driven guitars, a kit hit hard, no apologies.',
      bpm: [116, 148], swing: 0.02,
      meters: [['4/4', 8], ['3/4', 1], ['7/8', 1]],
      modulates: 0.18, borrow: 0.1,
      riff: 0.65,
      halfTime: 0.2,
      scales: [['minor', 3], ['mixolydian', 3], ['major', 2], ['dorian', 1]],
      progressions: PROG.rock.concat(PROG.popMajor), chordShapes: [['power', 3], ['triad', 4], ['sus4', 1]],
      barsPerChord: [1, 2],
      bass: { style: 'root8', octave: 2, preset: 'pickBass', alts: ['synthBass', 'organBass'] },
      chords: { style: 'stab', preset: 'guitar', octaveLow: 52, octaveHigh: 76, alts: ['organ', 'piano', 'brass'] },
      pad: { preset: 'strings', gain: 0.3 },
      counter: { chance: 0.35, preset: 'guitar', octave: 4, alts: ['organ', 'brass'] },
      lead: { preset: 'guitar', octave: 5, density: 0.7, restBias: 0.2, alts: ['sawLead', 'organ', 'brass'] },
      arp: { preset: 'guitar', rate: 0.5, octave: 5, chance: 0.2, alts: ['piano', 'organ'] },
      builds: true,
      drums: {
        kit: 'rock',
        intro:  { kick: 'x.......x.......', hh: 'x.x.x.x.x.x.x.x.', snare: '....x.......x...' },
        groove: { kick: 'x.....x.x.......', snare: '....x.......x...', hh: 'x.x.x.x.x.x.x.x.', crash: 'x...............' },
        full:   { kick: 'x..x..x.x...x...', snare: '....x.......x...', hh: 'xxxxxxxxxxxxxxxx', crash: 'x.......x.......', ride: '..x...x...x...x.' },
        fill:   { kick: 'x.......x.......', snare: '........xxx.xxxx', tom: '............x.x.', crash: '...............x' }
      },
      fx: { reverb: 0.3, delay: 0.14, delayTime: 0.375, vinyl: 0, master: 0.92, sidechain: 0.08, brightness: 1.1 }
    },

    funk: {
      id: 'funk', name: 'Funk', blurb: 'One chord, sixteen notes, all of it on the one.',
      bpm: [96, 116], swing: 0.12,
      meters: [['4/4', 1]],
      modulates: 0.05, borrow: 0.16,
      riff: 0.7,
      halfTime: 0.15,
      scales: [['dorian', 4], ['mixolydian', 3], ['minor', 2], ['aeolianPent', 1]],
      progressions: PROG.funk.concat(PROG.jazzy), chordShapes: [['ninth', 4], ['seventh', 3], ['sixth', 2]],
      barsPerChord: [2, 4],
      bass: { style: 'pulse8', octave: 2, preset: 'pickBass', alts: ['synthBass', 'lofiBass'] },
      chords: { style: 'stab', preset: 'guitar', octaveLow: 55, octaveHigh: 79, alts: ['organ', 'rhodes', 'brass'] },
      pad: { preset: 'strings', gain: 0.22 },
      counter: { chance: 0.5, preset: 'brass', octave: 4, alts: ['organ', 'guitar'] },
      lead: { preset: 'brass', octave: 5, density: 0.62, restBias: 0.34, alts: ['organ', 'sawLead', 'guitar'] },
      arp: { preset: 'guitar', rate: 0.25, octave: 5, chance: 0.45, alts: ['rhodes', 'organ', 'marimba'] },
      builds: false,
      drums: {
        kit: 'acoustic',
        intro:  { kick: 'x.......x.......', hh: 'x.x.x.x.x.x.x.x.', snare: '....x.......x...' },
        groove: { kick: 'x.....x...x.....', snare: '....x.......x...', hh: 'xoxoxoxoxoxoxoxo', perc: '..........o.....' },
        full:   { kick: 'x..x..x...x..x..', snare: '....x..o....x..o', hh: 'xoxoxoxoxoxoxoxo', tamb: '..x...x...x...x.', conga: '......o...o...o.' },
        fill:   { kick: 'x.......x.......', snare: '......x.x.xxx.xx', tom: '..........x.x...' }
      },
      fx: { reverb: 0.22, delay: 0.18, delayTime: 0.375, vinyl: 0.08, master: 1.0, sidechain: 0.1, brightness: 1.05 }
    },

    jazz: {
      id: 'jazz', name: 'Jazz', blurb: 'Brushed ride, walking bass, chords that keep moving.',
      bpm: [104, 148], swing: 0.34,
      meters: [['4/4', 7], ['3/4', 2], ['5/4', 1]],
      modulates: 0.1, borrow: 0.34,
      riff: 0.15,
      halfTime: 0.05,
      scales: [['dorian', 3], ['major', 3], ['mixolydian', 2], ['minor', 2], ['harmonicMinor', 1]],
      progressions: PROG.jazzy.concat(PROG.bossa), chordShapes: [['ninth', 4], ['seventh', 4], ['sixth', 2]],
      barsPerChord: [1, 2],
      bass: { style: 'walk', octave: 2, preset: 'pickBass', alts: ['lofiBass', 'organBass'] },
      chords: { style: 'keys', preset: 'piano', octaveLow: 55, octaveHigh: 81, alts: ['rhodes', 'guitar', 'vibes'] },
      pad: { preset: 'strings', gain: 0.18 },
      counter: { chance: 0.6, preset: 'brass', octave: 4, alts: ['reedLead', 'vibes', 'piano'] },
      lead: { preset: 'reedLead', octave: 5, density: 0.66, restBias: 0.3, alts: ['brass', 'vibes', 'piano', 'guitar'] },
      arp: { preset: 'vibes', rate: 0.5, octave: 5, chance: 0.3, alts: ['piano', 'guitar', 'harp'] },
      builds: false,
      drums: {
        kit: 'jazz',
        intro:  { ride: 'x..x..x...x...x.', hh: '....x.......x...' },
        groove: { kick: 'x.............x.', snare: '......o.....o...', ride: 'x..xx.x..xx.x..x', hh: '....x.......x...' },
        full:   { kick: 'x.....x.......x.', snare: '....o.o...o.o..o', ride: 'x.xxx.xx.xxx.xxx', hh: '....x.......x...', rim: '..........x.....' },
        fill:   { snare: '..o.x.oxx.xoxxxx', tom: '..........x.x...', crash: '...............x' }
      },
      fx: { reverb: 0.42, delay: 0.12, delayTime: 0.5, vinyl: 0.12, master: 1.2, sidechain: 0, brightness: 0.9 }
    },

    disco: {
      id: 'disco', name: 'Disco', blurb: 'Four on the floor, strings on top, hats everywhere.',
      bpm: [112, 126], swing: 0.06,
      meters: [['4/4', 1]],
      modulates: 0.22, borrow: 0.12,
      riff: 0.3,
      halfTime: 0.05,
      scales: [['minor', 3], ['major', 3], ['dorian', 2]],
      progressions: PROG.disco.concat(PROG.jazzy), chordShapes: [['seventh', 4], ['ninth', 3], ['sixth', 1]],
      barsPerChord: [1, 2],
      bass: { style: 'offbeat', octave: 2, preset: 'pickBass', alts: ['synthBass', 'organBass'] },
      chords: { style: 'stab', preset: 'guitar', octaveLow: 55, octaveHigh: 79, alts: ['rhodes', 'piano', 'brass'] },
      pad: { preset: 'strings', gain: 0.42 },
      counter: { chance: 0.45, preset: 'strings', octave: 5, alts: ['brass', 'piano'] },
      lead: { preset: 'brass', octave: 5, density: 0.6, restBias: 0.32, alts: ['sawLead', 'voxLead', 'piano'] },
      arp: { preset: 'piano', rate: 0.25, octave: 5, chance: 0.55, alts: ['guitar', 'harp', 'marimba'] },
      builds: true,
      drums: {
        kit: 'acoustic',
        intro:  { kick: 'x...x...x...x...', hh: '..x...x...x...x.' },
        groove: { kick: 'x...x...x...x...', snare: '....x.......x...', hh: '..x...x...x...x.', oh: '..x...x...x...x.' },
        full:   { kick: 'x...x...x...x...', snare: '....x.......x...', hh: 'xxxxxxxxxxxxxxxx', oh: '..x...x...x...x.', tamb: '....x.......x...', conga: '..o...o...o...o.' },
        fill:   { kick: 'x...x...........', snare: '........x.x.xxxx', tom: '............x.x.' }
      },
      fx: { reverb: 0.36, delay: 0.2, delayTime: 0.375, vinyl: 0.06, master: 0.98, sidechain: 0.3, brightness: 1.05 }
    },

    techno: {
      id: 'techno', name: 'Techno', blurb: 'A relentless kick and a filter that never stops moving.',
      bpm: [128, 142], swing: 0,
      meters: [['4/4', 1]],
      modulates: 0.0, borrow: 0.02,
      riff: 0.35,
      halfTime: 0.15,
      scales: [['minor', 4], ['phrygian', 2], ['dorian', 2]],
      progressions: PROG.techno.concat(PROG.loop2), chordShapes: [['power', 3], ['triad', 3], ['seventh', 2]],
      barsPerChord: [4, 8],
      bass: { style: 'root8', octave: 1, preset: 'reese', alts: ['synthBass', 'houseBass', 'fmBass'] },
      chords: { style: 'stab', preset: 'stab', octaveLow: 52, octaveHigh: 76, alts: ['supersaw', 'organ', 'fmKeys'] },
      pad: { preset: 'glassPad', gain: 0.34 },
      counter: { chance: 0.2, preset: 'pluck', octave: 5, alts: ['bell', 'fmKeys'] },
      lead: { preset: 'fmBright', octave: 5, density: 0.5, restBias: 0.45, alts: ['sawLead', 'supersaw', 'bell'] },
      arp: { preset: 'pluck', rate: 0.25, octave: 5, chance: 0.6, alts: ['chipChord', 'fmKeys', 'glassBell'] },
      builds: true,
      drums: {
        kit: 'nine09',
        intro:  { kick: 'x...x...x...x...', hh: '..x...x...x...x.' },
        groove: { kick: 'x...x...x...x...', clap: '....x.......x...', hh: '..x...x...x...x.', oh: '......x.......x.' },
        full:   { kick: 'x...x...x...x...', clap: '....x.......x...', hh: 'xxxxxxxxxxxxxxxx', oh: '..x...x...x...x.', ride: '....x.......x...', cowbell: '..........x.....' },
        fill:   { kick: 'x...x...x.x.x.x.', clap: '........x.x.xxxx', crash: '...............x' }
      },
      fx: { reverb: 0.3, delay: 0.26, delayTime: 0.375, vinyl: 0, master: 0.96, sidechain: 0.62, brightness: 1.0 }
    },

    drill: {
      id: 'drill', name: 'Drill', blurb: 'Sliding 808s, skittering hats, cold and spacious.',
      bpm: [138, 146], swing: 0.04,
      meters: [['4/4', 1]],
      modulates: 0.0, borrow: 0.03,
      riff: 0.4,
      halfTime: 0.45,
      scales: [['minor', 3], ['phrygian', 3], ['harmonicMinor', 2]],
      progressions: PROG.loop2.concat(PROG.darkTrap), chordShapes: [['triad', 3], ['seventh', 3], ['power', 2]],
      barsPerChord: [2, 4],
      bass: { style: 'slide808', octave: 1, preset: 'eight08', alts: ['fmBass', 'reese'] },
      chords: { style: 'arpChord', preset: 'pluck', octaveLow: 55, octaveHigh: 79, alts: ['bell', 'nylon', 'glassBell'] },
      pad: { preset: 'choirPad', gain: 0.34 },
      counter: { chance: 0.3, preset: 'glassBell', octave: 5, alts: ['bell', 'harp'] },
      lead: { preset: 'bell', octave: 5, density: 0.42, restBias: 0.5, alts: ['glassBell', 'nylon', 'voxLead'] },
      arp: { preset: 'harp', rate: 0.25, octave: 5, chance: 0.5, alts: ['bell', 'pluck', 'marimba'] },
      builds: true,
      drums: {
        kit: 'trap',
        intro:  { kick: 'x.......x.......', hh: 'x.x.x.x.x.x.x.x.' },
        groove: { kick: 'x.....x...x.....', snare: '........x.......', hh: 'x.xxx.x.x.xxx.x.' },
        full:   { kick: 'x.....x...x...x.', snare: '........x.......', hh: 'xxxxx.xxxxxxx.xx', oh: '..........x.....', perc: '............o...' },
        fill:   { kick: 'x.......x.......', snare: '........xxxxxxxx', riser: 'x...............' }
      },
      fx: { reverb: 0.4, delay: 0.24, delayTime: 0.375, vinyl: 0, master: 1.12, sidechain: 0.4, brightness: 0.92 }
    },

    afrobeat: {
      id: 'afrobeat', name: 'Afrobeat', blurb: 'Interlocking percussion, a bassline that never sits still.',
      bpm: [100, 118], swing: 0.09,
      meters: [['4/4', 9], ['6/8', 1]],
      modulates: 0.05, borrow: 0.08,
      riff: 0.45,
      halfTime: 0.1,
      scales: [['major', 3], ['mixolydian', 3], ['dorian', 2], ['minor', 1]],
      progressions: PROG.loop2.concat(PROG.popMajor), chordShapes: [['seventh', 3], ['triad', 3], ['ninth', 2]],
      barsPerChord: [2, 4],
      bass: { style: 'pulse8', octave: 2, preset: 'pickBass', alts: ['synthBass', 'organBass'] },
      chords: { style: 'stab', preset: 'guitar', octaveLow: 55, octaveHigh: 79, alts: ['organ', 'rhodes', 'marimba'] },
      pad: { preset: 'warmPad', gain: 0.24 },
      counter: { chance: 0.5, preset: 'marimba', octave: 4, alts: ['guitar', 'brass'] },
      lead: { preset: 'brass', octave: 5, density: 0.55, restBias: 0.38, alts: ['reedLead', 'organ', 'marimba'] },
      arp: { preset: 'marimba', rate: 0.25, octave: 5, chance: 0.6, alts: ['guitar', 'harp', 'vibes'] },
      builds: false,
      drums: {
        kit: 'latin',
        intro:  { conga: 'o...o...o...o...', shaker: 'x.x.x.x.x.x.x.x.' },
        groove: { kick: 'x.....x...x.....', rim: '....x.......x...', shaker: 'xoxoxoxoxoxoxoxo', conga: '..o.o...o.o.o...' },
        full:   { kick: 'x..x..x...x..x..', rim: '....x.......x...', shaker: 'xoxoxoxoxoxoxoxo', conga: 'o.oo.o.oo.o.oo.o', cowbell: 'x...x...x...x...', tamb: '..x...x...x...x.' },
        fill:   { conga: 'oo.oo.oooo.ooooo', kick: 'x.......x.......', crash: '...............x' }
      },
      fx: { reverb: 0.28, delay: 0.16, delayTime: 0.375, vinyl: 0.05, master: 1.05, sidechain: 0.08, brightness: 1.05 }
    },

    bossa: {
      id: 'bossa', name: 'Bossa Nova', blurb: 'Nylon guitar, brushed rim, and nowhere to be.',
      bpm: [122, 142], swing: 0.05,
      meters: [['4/4', 1]],
      modulates: 0.08, borrow: 0.3,
      riff: 0.1,
      halfTime: 0.05,
      scales: [['major', 3], ['dorian', 2], ['mixolydian', 2], ['minor', 1]],
      progressions: PROG.bossa.concat(PROG.jazzy), chordShapes: [['ninth', 4], ['seventh', 4], ['sixth', 2]],
      barsPerChord: [1, 2],
      bass: { style: 'offbeat', octave: 2, preset: 'pickBass', alts: ['lofiBass', 'softBass'] },
      chords: { style: 'keys', preset: 'nylon', octaveLow: 55, octaveHigh: 79, alts: ['guitar', 'rhodes', 'piano'] },
      pad: { preset: 'warmPad', gain: 0.18 },
      counter: { chance: 0.5, preset: 'nylon', octave: 4, alts: ['vibes', 'reedLead'] },
      lead: { preset: 'nylon', octave: 5, density: 0.5, restBias: 0.42, alts: ['reedLead', 'vibes', 'voxLead'] },
      arp: { preset: 'nylon', rate: 0.5, octave: 5, chance: 0.4, alts: ['guitar', 'harp', 'vibes'] },
      builds: false,
      drums: {
        kit: 'latin',
        intro:  { rim: '..x...x...x...x.', shaker: 'x.x.x.x.x.x.x.x.' },
        groove: { kick: 'x.....x.x.....x.', rim: '..x..x..x..x..x.', shaker: 'xoxoxoxoxoxoxoxo' },
        full:   { kick: 'x.....x.x.....x.', rim: '..x..x..x..x..x.', shaker: 'xoxoxoxoxoxoxoxo', conga: '....o.......o...', tamb: '........x.......' },
        fill:   { rim: '..x.x.x.xx.xxx.x', conga: '..........o.o...' }
      },
      fx: { reverb: 0.4, delay: 0.14, delayTime: 0.5, vinyl: 0.1, master: 1.22, sidechain: 0, brightness: 0.95 }
    },

    gospel: {
      id: 'gospel', name: 'Gospel Soul', blurb: 'Organ, choir, and chords that resolve like they mean it.',
      bpm: [72, 96], swing: 0.2,
      meters: [['4/4', 6], ['6/8', 3], ['3/4', 1]],
      modulates: 0.35, borrow: 0.26,
      riff: 0.2,
      halfTime: 0.1,
      scales: [['major', 4], ['mixolydian', 3], ['dorian', 1]],
      progressions: PROG.gospel.concat(PROG.popMajor), chordShapes: [['seventh', 4], ['ninth', 3], ['sixth', 2]],
      barsPerChord: [1, 2],
      bass: { style: 'walk', octave: 2, preset: 'organBass', alts: ['pickBass', 'lofiBass'] },
      chords: { style: 'keys', preset: 'organ', octaveLow: 55, octaveHigh: 81, alts: ['piano', 'rhodes', 'brass'] },
      pad: { preset: 'choirPad', gain: 0.46 },
      counter: { chance: 0.55, preset: 'organ', octave: 4, alts: ['brass', 'choirPad', 'piano'] },
      lead: { preset: 'voxLead', octave: 5, density: 0.58, restBias: 0.36, alts: ['organ', 'brass', 'piano'] },
      arp: { preset: 'piano', rate: 0.5, octave: 5, chance: 0.35, alts: ['organ', 'rhodes', 'harp'] },
      builds: true,
      drums: {
        kit: 'acoustic',
        intro:  { kick: 'x.......x.......', hh: 'x...x...x...x...' },
        groove: { kick: 'x.....x.x.......', snare: '....x.......x...', hh: 'x.o.x.o.x.o.x.o.', tamb: '....x.......x...' },
        full:   { kick: 'x..x..x.x...x...', snare: '....x..o....x..o', hh: 'xoxoxoxoxoxoxoxo', tamb: '..x.x.....x.x...', ride: '....x.......x...' },
        fill:   { kick: 'x.......x.......', snare: '......x.x.xxxxxx', tom: '..........x.x...', crash: '...............x' }
      },
      fx: { reverb: 0.5, delay: 0.16, delayTime: 0.5, vinyl: 0.08, master: 1.08, sidechain: 0.1, brightness: 0.95 }
    },

    country: {
      id: 'country', name: 'Country', blurb: 'Acoustic strum, brushed snare, a story in every line.',
      bpm: [88, 124], swing: 0.14,
      meters: [['4/4', 7], ['3/4', 3]],
      modulates: 0.28, borrow: 0.12,
      riff: 0.3,
      halfTime: 0.1,
      scales: [['major', 4], ['mixolydian', 3], ['minor', 1]],
      progressions: PROG.country.concat(PROG.popMajor), chordShapes: [['triad', 4], ['sus4', 1], ['sixth', 2], ['seventh', 2]],
      barsPerChord: [1, 2],
      bass: { style: 'root8', octave: 2, preset: 'pickBass', alts: ['organBass', 'lofiBass'] },
      chords: { style: 'stab', preset: 'guitar', octaveLow: 52, octaveHigh: 76, alts: ['nylon', 'piano', 'organ'] },
      pad: { preset: 'strings', gain: 0.22 },
      counter: { chance: 0.45, preset: 'nylon', octave: 4, alts: ['guitar', 'reedLead'] },
      lead: { preset: 'guitar', octave: 5, density: 0.56, restBias: 0.36, alts: ['nylon', 'reedLead', 'piano'] },
      arp: { preset: 'guitar', rate: 0.25, octave: 5, chance: 0.5, alts: ['nylon', 'harp', 'marimba'] },
      builds: false,
      drums: {
        kit: 'acoustic',
        intro:  { kick: 'x.......x.......', hh: 'x...x...x...x...' },
        groove: { kick: 'x.......x.......', snare: '....x.......x...', hh: 'x.x.x.x.x.x.x.x.' },
        full:   { kick: 'x.....x.x.......', snare: '....x.......x...', hh: 'x.x.x.x.x.x.x.x.', tamb: '..x...x...x...x.', ride: '....x.......x...' },
        fill:   { kick: 'x.......x.......', snare: '........x.x.xxxx', tom: '............x.x.' }
      },
      fx: { reverb: 0.34, delay: 0.14, delayTime: 0.5, vinyl: 0.1, master: 1.1, sidechain: 0.06, brightness: 1.0 }
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
    PRESET_LABEL: PRESET_LABEL,
    PRESET_GROUPS: PRESET_GROUPS,
    list: function () { return Object.keys(GENRES).map(function (k) { return GENRES[k]; }); },
    moodList: function () { return Object.keys(MOODS).map(function (k) { return Object.assign({ id: k }, MOODS[k]); }); }
  };
})(window);
