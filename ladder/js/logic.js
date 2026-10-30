/*
 * Ladder 24 — the panel's rules, with no sound and no page.
 *
 * Every control (its range, its curve, how its value reads), the factory
 * patches, the computer-keyboard map, and the patch code you can copy and
 * paste. Plain functions, so the tests run them in Node.
 */
(function (root) {
  'use strict';

  // ---------------------------------------------------------------- drum kits
  //
  // A kit puts a different drum on every key for two octaves from C3, laid out
  // the way drum machines and General MIDI do (kick on C, snare on D, hi-hats on
  // the black keys F#, G# and A#), repeating up and down the keyboard.
  //
  // Each kit is a short description of its character; the recipes below turn it
  // into 24 synthesized drums, the way an analog drum machine builds every sound
  // from oscillators, noise and filters.

  // [role, name, short label, choke group, pan]
  const KIT_SLOTS = [
    ['kick', 'Kick', 'Kick', null, 0],
    ['rim', 'Rimshot', 'Rim', null, -0.2],
    ['snare', 'Snare', 'Snare', null, 0],
    ['clap', 'Hand clap', 'Clap', null, 0.1],
    ['snare2', 'Snare 2', 'Snr 2', null, 0],
    ['tomL', 'Low tom', 'Tom L', null, 0.35],
    ['chh', 'Closed hi-hat', 'Hat', 'hat', -0.3],
    ['tomM', 'Mid tom', 'Tom M', null, 0.1],
    ['phh', 'Pedal hi-hat', 'Pedal', 'hat', -0.3],
    ['tomH', 'High tom', 'Tom H', null, -0.15],
    ['ohh', 'Open hi-hat', 'Open', 'hat', -0.3],
    ['crash', 'Crash cymbal', 'Crash', null, 0.4],
    ['kick2', 'Long kick', 'Kick 2', null, 0],
    ['clave', 'Clave', 'Clave', null, 0.3],
    ['kick3', 'Hard kick', 'Kick 3', null, 0],
    ['cowbell', 'Cowbell', 'Bell', null, 0.25],
    ['congaL', 'Low conga', 'Cga L', null, 0.3],
    ['ride', 'Ride cymbal', 'Ride', null, -0.35],
    ['shaker', 'Shaker', 'Shake', null, 0.35],
    ['congaM', 'Mid conga', 'Cga M', null, 0.1],
    ['tamb', 'Tambourine', 'Tamb', null, 0.4],
    ['congaH', 'High conga', 'Cga H', null, -0.2],
    ['fx', 'Effect', 'FX', null, 0],
    ['splash', 'Splash cymbal', 'Splash', null, -0.4],
  ];

  // What every kit starts from; a kit only lists what it changes.
  const KIT_BASE = {
    kick: { hz: 55, sweep: 12, sweepTime: 0.04, decay: 0.5, click: 0.05, drive: 0.3, wave: 'sine' },
    snare: { hz: 190, body: 0.7, noise: 0.9, cutoff: 6000, mode: 'lp24', decay: 0.2, sweep: 3, drive: 0.6, attack: 0.001 },
    clap: { cutoff: 1300, res: 0.4, decay: 0.22, mode: 'bp' },
    rim: { hz: 450, decay: 0.035, cutoff: 1800 },
    tom: { hz: 90, sweep: 5, decay: 0.5, noise: 0.05, wave: 'sine', steps: [1, 1.33, 1.78] },
    hat: { hz: 400, metal: 0.5, noise: 0.6, cutoff: 7500, res: 0.25, mode: 'hp12', closed: 0.05, pedal: 0.1, open: 0.45, drive: 0.6 },
    cym: { crash: 1.6, ride: 1.1, splash: 0.7, cutoff: 5000, china: false },
    conga: { hz: 165, decay: 0.25, sweep: 3, wave: 'sine', steps: [1, 1.5, 2.2] },
    clave: { hz: 2500, decay: 0.03 },
    cowbell: { hz: 540, decay: 0.35 },
    shaker: { decay: 0.05, cutoff: 6000 },
    tamb: { decay: 0.2 },
    fx: 'zap',
    room: 0.12,
    maxCutoff: 18000,
  };

  const KITS = [
    {
      // The classic analog drum machine: a deep, clean sine kick with a long
      // tail, a two-tone snare, metallic hats from detuned square waves, and
      // its cowbell, clave, maracas and congas.
      name: 'Classic 808',
      kick: { hz: 52, sweep: 7, sweepTime: 0.03, decay: 0.9, click: 0.03, drive: 0.15 },
      snare: { hz: 180, body: 0.8, noise: 0.65, cutoff: 9000, decay: 0.2, sweep: 2, drive: 0.3 },
      clap: { cutoff: 1100, res: 0.35, decay: 0.25 },
      rim: { hz: 455, decay: 0.03, cutoff: 1700 },
      tom: { hz: 92, sweep: 4, decay: 0.45, noise: 0.04, steps: [1, 1.42, 1.9] },
      hat: { metal: 0.55, noise: 0.35, cutoff: 7000, res: 0.3, closed: 0.045, pedal: 0.09, open: 0.4, drive: 0.5 },
      cym: { crash: 1.5, ride: 1, splash: 0.6, cutoff: 5500 },
      conga: { hz: 165, decay: 0.22, sweep: 2, steps: [1, 1.5, 2.25] },
      clave: { hz: 2500, decay: 0.025 },
      cowbell: { hz: 540, decay: 0.3 },
      shaker: { decay: 0.04, cutoff: 7000 },
      fx: 'sub',
      names: { shaker: ['Maracas', 'Mrcs'], fx: ['808 sub boom', 'Sub'], kick2: ['Long 808 kick', '808 L'] },
      room: 0.08,
    },
    {
      name: 'Trap 808',
      kick: { hz: 45, sweep: 12, decay: 1.8, drive: 0.5 },
      snare: { hz: 200, body: 0.5, noise: 1, cutoff: 7000, mode: 'hp12', decay: 0.15 },
      clap: { cutoff: 1500, decay: 0.18 },
      hat: { closed: 0.03, pedal: 0.06, open: 0.3, cutoff: 9000 },
      fx: 'laser',
      room: 0.1,
    },
    {
      name: 'Techno 909',
      kick: { hz: 50, sweep: 20, sweepTime: 0.03, decay: 0.45, click: 0.25, drive: 0.6 },
      snare: { hz: 200, body: 0.5, noise: 1, cutoff: 8000, mode: 'lp12', decay: 0.2, drive: 0.8 },
      clap: { cutoff: 1300, decay: 0.25 },
      tom: { hz: 110, sweep: 6, decay: 0.35, noise: 0.15 },
      hat: { metal: 0.3, noise: 0.9, cutoff: 9000, mode: 'hp24', closed: 0.06, open: 0.5 },
      cym: { crash: 2, ride: 1.4 },
      room: 0.1,
    },
    {
      name: 'House',
      kick: { hz: 52, sweep: 16, decay: 0.4, click: 0.15, drive: 0.4 },
      clap: { cutoff: 1600, res: 0.3, decay: 0.3 },
      hat: { cutoff: 8000, closed: 0.05, open: 0.35 },
      fx: 'vox',
      room: 0.15,
    },
    {
      name: 'Minimal Techno',
      kick: { hz: 55, sweep: 10, decay: 0.25, click: 0.05, drive: 0.2 },
      snare: { body: 0.3, noise: 0.6, cutoff: 4000, decay: 0.1 },
      rim: { decay: 0.02 },
      hat: { closed: 0.025, pedal: 0.05, open: 0.2, metal: 0.2, noise: 0.8, cutoff: 10000 },
      conga: { decay: 0.15 },
      fx: 'blip',
      room: 0.05,
    },
    {
      name: 'Rock',
      kick: { hz: 58, sweep: 14, decay: 0.35, click: 0.35, drive: 0.6 },
      snare: { hz: 190, body: 0.6, noise: 1, cutoff: 5000, decay: 0.3, drive: 1 },
      tom: { hz: 85, sweep: 3, decay: 0.7, noise: 0.1 },
      hat: { metal: 0.6, noise: 0.7, cutoff: 6000, closed: 0.08, open: 0.6 },
      cym: { crash: 2.5, ride: 1.6 },
      fx: 'gong',
      room: 0.25,
    },
    {
      name: 'Jazz Brushes',
      kick: { hz: 60, sweep: 4, decay: 0.35, click: 0, drive: 0.05 },
      snare: { body: 0.2, noise: 0.9, cutoff: 3000, mode: 'lp12', decay: 0.35, sweep: 0, drive: 0.2, attack: 0.02 },
      tom: { hz: 100, decay: 0.6 },
      hat: { metal: 0.5, noise: 0.4, cutoff: 6000, closed: 0.08, open: 0.8 },
      cym: { ride: 2, crash: 2.5 },
      fx: 'snap',
      names: { snare: ['Brush snare', 'Brush'], snare2: ['Brush swish', 'Swish'] },
      room: 0.3,
    },
    {
      name: 'Funk',
      kick: { hz: 60, sweep: 10, decay: 0.3, click: 0.2, drive: 0.4 },
      snare: { hz: 220, body: 0.6, noise: 0.9, cutoff: 6500, decay: 0.18 },
      hat: { closed: 0.04, open: 0.3, cutoff: 8000 },
      fx: 'snap',
      room: 0.12,
    },
    {
      name: 'Boom Bap',
      kick: { hz: 55, sweep: 10, decay: 0.5, click: 0.15, drive: 0.7 },
      snare: { hz: 190, body: 0.6, noise: 1, cutoff: 4500, decay: 0.25, drive: 1 },
      hat: { cutoff: 6000, closed: 0.05, open: 0.35 },
      fx: 'vox',
      room: 0.15,
    },
    {
      name: 'Lo-Fi',
      kick: { hz: 55, sweep: 8, decay: 0.4, drive: 0.6 },
      snare: { cutoff: 2500, decay: 0.2, noise: 0.8 },
      hat: { cutoff: 4500, mode: 'bp', closed: 0.05, open: 0.3 },
      cym: { cutoff: 3000 },
      fx: 'reverse',
      maxCutoff: 4500,
      room: 0.2,
    },
    {
      name: 'Electro',
      kick: { hz: 50, sweep: 18, decay: 0.6, drive: 0.4 },
      snare: { hz: 250, body: 0.4, noise: 1, cutoff: 7000, mode: 'hp12', decay: 0.15, sweep: 12 },
      clap: { cutoff: 1800, decay: 0.2 },
      tom: { hz: 110, sweep: 12, decay: 0.3, wave: 'tri' },
      hat: { closed: 0.03, open: 0.25, cutoff: 9000 },
      room: 0.08,
    },
    {
      name: 'Synthwave 80s',
      kick: { hz: 55, sweep: 12, decay: 0.45, click: 0.1, drive: 0.4 },
      snare: { hz: 200, body: 0.6, noise: 1, cutoff: 6000, decay: 0.45, drive: 0.8 },
      clap: { decay: 0.35 },
      tom: { hz: 90, sweep: 12, decay: 0.6, wave: 'tri', noise: 0.1 },
      fx: 'laser',
      room: 0.45,
    },
    {
      name: 'Disco',
      kick: { hz: 58, sweep: 10, decay: 0.35, click: 0.1 },
      snare: { hz: 210, decay: 0.22 },
      hat: { open: 0.35, closed: 0.05, cutoff: 8500 },
      tom: { hz: 95, sweep: 14, decay: 0.5 },
      fx: 'laser',
      room: 0.2,
    },
    {
      name: 'Drum & Bass',
      kick: { hz: 58, sweep: 18, decay: 0.3, click: 0.25, drive: 0.6 },
      snare: { hz: 230, body: 0.5, noise: 1, cutoff: 9000, mode: 'lp12', decay: 0.18, drive: 1 },
      hat: { closed: 0.03, open: 0.25, cutoff: 10000, metal: 0.3, noise: 0.9 },
      fx: 'reverse',
      room: 0.15,
    },
    {
      name: 'Dubstep',
      kick: { hz: 50, sweep: 20, decay: 0.5, click: 0.2, drive: 0.8 },
      snare: { hz: 180, body: 0.7, noise: 1, cutoff: 5000, decay: 0.35, drive: 1 },
      clap: { decay: 0.3 },
      fx: 'vox',
      room: 0.3,
    },
    {
      name: 'Industrial',
      kick: { hz: 50, sweep: 24, decay: 0.4, click: 0.6, drive: 1 },
      snare: { noise: 1, body: 0.6, cutoff: 4000, mode: 'bp', decay: 0.3, drive: 1 },
      hat: { mode: 'bp', cutoff: 5000, res: 0.6, drive: 1 },
      cym: { china: true },
      fx: 'thunder',
      room: 0.25,
    },
    {
      name: 'Latin Percussion',
      kick: { hz: 65, sweep: 5, decay: 0.3 },
      tom: { hz: 180, wave: 'tri', decay: 0.35, noise: 0.2, sweep: 2 },
      conga: { hz: 190, decay: 0.3, sweep: 2 },
      cowbell: { hz: 700, decay: 0.4 },
      clave: { hz: 2200, decay: 0.04 },
      shaker: { decay: 0.08 },
      fx: 'snap',
      names: { kick: ['Bombo', 'Bombo'], tomL: ['Low timbale', 'Tim L'], tomM: ['Mid timbale', 'Tim M'], tomH: ['High timbale', 'Tim H'] },
      room: 0.2,
    },
    {
      name: 'Afrobeat',
      kick: { hz: 60, sweep: 8, decay: 0.35 },
      snare: { hz: 240, decay: 0.15, cutoff: 5000 },
      tom: { hz: 110, decay: 0.4, sweep: 3 },
      conga: { hz: 150, decay: 0.35 },
      cowbell: { hz: 600 },
      shaker: { decay: 0.07 },
      fx: 'snap',
      room: 0.18,
    },
    {
      name: 'Reggae Dub',
      kick: { hz: 52, sweep: 6, decay: 0.6 },
      snare: { hz: 300, body: 0.8, noise: 0.4, cutoff: 3500, decay: 0.15 },
      rim: { hz: 500, decay: 0.05 },
      fx: 'laser',
      room: 0.35,
      patch: { delayMix: 0.3, delayDiv: '1/8.', delayFb: 0.55 },
    },
    {
      name: 'Big Room EDM',
      kick: { hz: 50, sweep: 24, decay: 0.5, click: 0.3, drive: 0.9 },
      snare: { hz: 200, body: 0.5, noise: 1, cutoff: 9000, decay: 0.25 },
      clap: { cutoff: 1500, decay: 0.35 },
      hat: { cutoff: 10000 },
      fx: 'reverse',
      room: 0.3,
    },
    {
      name: 'Chiptune',
      kick: { hz: 60, sweep: 24, sweepTime: 0.06, decay: 0.2, wave: 'square', click: 0 },
      snare: { body: 0, noise: 1, cutoff: 18000, decay: 0.12 },
      hat: { metal: 0, noise: 1, cutoff: 12000, mode: 'lp24', closed: 0.03, pedal: 0.05, open: 0.15 },
      tom: { wave: 'square', sweep: 12, decay: 0.2 },
      conga: { wave: 'square', decay: 0.12 },
      fx: 'blip',
      room: 0,
    },
    {
      name: 'Vintage Rhythm Box',
      kick: { hz: 70, sweep: 2, decay: 0.25, click: 0 },
      snare: { hz: 250, body: 0.5, noise: 0.6, cutoff: 3500, decay: 0.15 },
      hat: { metal: 0.1, noise: 1, cutoff: 9000, closed: 0.03, open: 0.2 },
      cym: { crash: 0.8, ride: 0.6, splash: 0.4 },
      tom: { hz: 120, decay: 0.3, sweep: 1 },
      conga: { hz: 220 },
      clave: { hz: 2400 },
      fx: 'blip',
      room: 0.15,
    },
    {
      name: 'UK Garage',
      kick: { hz: 55, sweep: 12, decay: 0.3, click: 0.2 },
      snare: { hz: 220, decay: 0.15, cutoff: 8000 },
      clap: { cutoff: 1700, decay: 0.2 },
      hat: { closed: 0.03, open: 0.25 },
      shaker: { decay: 0.05 },
      fx: 'vox',
      room: 0.12,
    },
    {
      name: 'Ambient',
      kick: { hz: 50, sweep: 5, decay: 0.7, click: 0, drive: 0.1 },
      snare: { body: 0.4, noise: 0.5, cutoff: 2500, decay: 0.5, drive: 0.1 },
      hat: { cutoff: 6000, closed: 0.08, open: 1 },
      cym: { crash: 3, ride: 2.2 },
      fx: 'gong',
      room: 0.55,
    },
    {
      name: 'Metal',
      kick: { hz: 62, sweep: 16, decay: 0.2, click: 0.7, drive: 0.8 },
      snare: { hz: 220, body: 0.6, noise: 1, cutoff: 7000, decay: 0.25, drive: 1 },
      tom: { hz: 80, sweep: 6, decay: 0.6, noise: 0.15 },
      hat: { metal: 0.6, cutoff: 7000, open: 0.5 },
      cym: { crash: 2.2, china: true },
      fx: 'gong',
      room: 0.2,
    },
    {
      name: 'Glitch Beats',
      kick: { hz: 60, sweep: 24, sweepTime: 0.01, decay: 0.15, click: 0.4 },
      snare: { noise: 1, body: 0.3, cutoff: 12000, mode: 'hp24', decay: 0.06 },
      clap: { decay: 0.08 },
      tom: { sweep: 24, decay: 0.15, wave: 'square' },
      hat: { closed: 0.028, pedal: 0.045, open: 0.12, mode: 'hp24' },
      fx: 'blip',
      room: 0.05,
    },
    {
      name: 'Cinematic',
      kick: { hz: 40, sweep: 6, decay: 2, click: 0.1, drive: 0.4 },
      snare: { body: 0.4, noise: 1, cutoff: 3000, decay: 0.6 },
      tom: { hz: 70, sweep: 2, decay: 1.4, noise: 0.1 },
      cym: { crash: 3.5 },
      fx: 'timpani',
      names: { kick: ['Bass drum', 'Bass'], tomL: ['Low taiko', 'Tko L'], tomM: ['Mid taiko', 'Tko M'], tomH: ['High taiko', 'Tko H'], fx: ['Timpani', 'Timp'] },
      room: 0.6,
    },
    {
      name: 'Tribal',
      kick: { hz: 50, sweep: 4, decay: 0.8, click: 0 },
      snare: { hz: 160, body: 0.8, noise: 0.3, cutoff: 2000, decay: 0.3 },
      tom: { hz: 75, sweep: 3, decay: 0.9, noise: 0.05 },
      conga: { hz: 120, decay: 0.45 },
      shaker: { decay: 0.12 },
      fx: 'timpani',
      room: 0.3,
    },
    {
      name: 'Electronic Toms 80s',
      kick: { hz: 55, sweep: 16, decay: 0.5 },
      snare: { noise: 1, body: 0.4, cutoff: 6000, decay: 0.35 },
      tom: { hz: 100, sweep: 18, decay: 0.7, wave: 'tri', noise: 0.15 },
      conga: { hz: 180, sweep: 14, decay: 0.4, wave: 'tri' },
      fx: 'laser',
      room: 0.3,
    },
    {
      name: 'Hand Percussion',
      kick: { hz: 80, sweep: 4, decay: 0.3, click: 0 },
      snare: { hz: 350, body: 0.6, noise: 0.6, cutoff: 4000, decay: 0.1 },
      clap: { decay: 0.15 },
      tom: { hz: 200, sweep: 3, decay: 0.18 },
      conga: { hz: 180, decay: 0.3, sweep: 2 },
      cowbell: { hz: 900 },
      shaker: { decay: 0.1 },
      tamb: { decay: 0.3 },
      fx: 'snap',
      names: { kick: ['Djembe bass', 'Djm B'], snare: ['Djembe slap', 'Slap'], tomL: ['Low bongo', 'Bgo L'], tomM: ['Mid bongo', 'Bgo M'], tomH: ['High bongo', 'Bgo H'], cowbell: ['Agogo', 'Agogo'] },
      room: 0.2,
    },
  ];

  const KIT_NAMES = KITS.map((k) => k.name);

  const WAVES = ['tri', 'saw', 'ramp', 'square', 'pulse', 'sine'];

  // type: knob | enum | bool. curve: lin | log | pow (cubic, fine control near 0).
  // unit picks how the value reads on the panel.
  const PARAMS = [
    // Controllers
    { id: 'glide', label: 'Glide', type: 'knob', min: 0, max: 2, def: 0, curve: 'pow', unit: 's' },
    { id: 'bendRange', label: 'Bend', type: 'knob', min: 1, max: 12, def: 2, step: 1, unit: 'st' },
    { id: 'kit', label: 'Drum kit', type: 'bool', def: false },
    { id: 'kitId', label: 'Kit', type: 'enum', options: KIT_NAMES, def: KIT_NAMES[0] },
    { id: 'voiceMode', label: 'Voices', type: 'enum', options: ['poly', 'mono', 'legato', 'unison'], def: 'poly' },
    { id: 'unisonDetune', label: 'Detune', type: 'knob', min: 0, max: 50, def: 12, unit: 'ct' },
    { id: 'spread', label: 'Width', type: 'knob', min: 0, max: 1, def: 0.5, unit: '%' },
    // Oscillator bank
    { id: 'o1Range', label: 'Range', type: 'knob', min: -2, max: 2, def: 0, step: 1, unit: 'feet' },
    { id: 'o1Wave', label: 'Wave', type: 'enum', options: WAVES, def: 'saw' },
    { id: 'o2Range', label: 'Range', type: 'knob', min: -2, max: 2, def: 0, step: 1, unit: 'feet' },
    { id: 'o2Tune', label: 'Tune', type: 'knob', min: -7, max: 7, def: 0.08, unit: 'semi', bipolar: true },
    { id: 'o2Wave', label: 'Wave', type: 'enum', options: WAVES, def: 'saw' },
    { id: 'o3Range', label: 'Range', type: 'knob', min: -2, max: 2, def: -1, step: 1, unit: 'feet' },
    { id: 'o3Tune', label: 'Tune', type: 'knob', min: -7, max: 7, def: -0.06, unit: 'semi', bipolar: true },
    { id: 'o3Wave', label: 'Wave', type: 'enum', options: WAVES, def: 'square' },
    { id: 'o3KeyOff', label: 'Osc 3 as LFO', type: 'bool', def: false },
    { id: 'pitchEnv', label: 'Pitch env', type: 'knob', min: -24, max: 24, def: 0, unit: 'semi', bipolar: true },
    { id: 'pw', label: 'Pulse width', type: 'knob', min: 0.05, max: 0.5, def: 0.3, unit: '%' },
    // Mixer
    { id: 'm1', label: 'Osc 1', type: 'knob', min: 0, max: 1, def: 0.8, unit: 'level' },
    { id: 'm2', label: 'Osc 2', type: 'knob', min: 0, max: 1, def: 0.6, unit: 'level' },
    { id: 'm3', label: 'Osc 3', type: 'knob', min: 0, max: 1, def: 0, unit: 'level' },
    { id: 'noise', label: 'Noise', type: 'knob', min: 0, max: 1, def: 0, unit: 'level' },
    { id: 'drive', label: 'Drive', type: 'knob', min: 0, max: 1, def: 0.2, unit: 'level' },
    // Filter
    { id: 'cutoff', label: 'Cutoff', type: 'knob', min: 20, max: 18000, def: 1200, curve: 'log', unit: 'hz' },
    { id: 'filterMode', label: 'Mode', type: 'enum', options: ['lp24', 'lp12', 'bp', 'hp12', 'hp24'], def: 'lp24' },
    { id: 'res', label: 'Emphasis', type: 'knob', min: 0, max: 1, def: 0.35, unit: 'level' },
    { id: 'fEnvAmt', label: 'Contour', type: 'knob', min: -6, max: 6, def: 3, unit: 'oct', bipolar: true },
    { id: 'keyTrack', label: 'Key track', type: 'knob', min: 0, max: 1, def: 0.5, unit: '%' },
    { id: 'fA', label: 'Attack', type: 'knob', min: 0.001, max: 10, def: 0.005, curve: 'log', unit: 's' },
    { id: 'fD', label: 'Decay', type: 'knob', min: 0.005, max: 10, def: 0.4, curve: 'log', unit: 's' },
    { id: 'fS', label: 'Sustain', type: 'knob', min: 0, max: 1, def: 0.3, unit: 'level' },
    { id: 'fR', label: 'Release', type: 'knob', min: 0.005, max: 10, def: 0.3, curve: 'log', unit: 's' },
    // Loudness
    { id: 'aA', label: 'Attack', type: 'knob', min: 0.001, max: 10, def: 0.003, curve: 'log', unit: 's' },
    { id: 'aD', label: 'Decay', type: 'knob', min: 0.005, max: 10, def: 0.3, curve: 'log', unit: 's' },
    { id: 'aS', label: 'Sustain', type: 'knob', min: 0, max: 1, def: 0.8, unit: 'level' },
    { id: 'aR', label: 'Release', type: 'knob', min: 0.005, max: 10, def: 0.25, curve: 'log', unit: 's' },
    { id: 'velAmp', label: 'Touch vol', type: 'knob', min: 0, max: 1, def: 0.5, unit: '%' },
    { id: 'velFilter', label: 'Touch tone', type: 'knob', min: 0, max: 1, def: 0.3, unit: '%' },
    // Modulation
    { id: 'lfoRate', label: 'LFO rate', type: 'knob', min: 0.05, max: 30, def: 5, curve: 'log', unit: 'hz' },
    { id: 'lfoSync', label: 'Sync', type: 'bool', def: false },
    { id: 'lfoDiv', label: 'LFO length', type: 'enum', options: ['4 bars', '2 bars', '1 bar', '1/2', '1/4', '1/8', '1/8T', '1/16'], def: '1/4' },
    { id: 'lfoShape', label: 'Shape', type: 'enum', options: ['tri', 'saw', 'square', 'sh'], def: 'tri' },
    { id: 'modSource', label: 'Source', type: 'enum', options: ['lfo', 'osc3', 'noise'], def: 'lfo' },
    { id: 'modDepth', label: 'Amount', type: 'knob', min: 0, max: 1, def: 0, unit: '%' },
    { id: 'modPitch', label: '→ Pitch', type: 'knob', min: 0, max: 12, def: 0.3, curve: 'pow', unit: 'semi' },
    { id: 'modFilter', label: '→ Filter', type: 'knob', min: 0, max: 4, def: 0, unit: 'oct' },
    { id: 'modPw', label: '→ Width', type: 'knob', min: 0, max: 0.45, def: 0, unit: '%' },
    // Tempo: drives the arpeggiator, a synced LFO and a synced echo
    { id: 'bpm', label: 'Tempo', type: 'knob', min: 40, max: 240, def: 120, step: 1, unit: 'bpm' },
    // Arpeggiator
    { id: 'arpOn', label: 'Arp', type: 'bool', def: false },
    { id: 'latch', label: 'Latch', type: 'bool', def: false },
    { id: 'arpMode', label: 'Order', type: 'enum', options: ['up', 'down', 'updown', 'order', 'random'], def: 'up' },
    { id: 'arpDiv', label: 'Step', type: 'enum', options: ['1/4', '1/8', '1/8T', '1/16', '1/32'], def: '1/16' },
    { id: 'arpOct', label: 'Octaves', type: 'knob', min: 1, max: 4, def: 2, step: 1, unit: 'x' },
    { id: 'arpGate', label: 'Gate', type: 'knob', min: 0.05, max: 1, def: 0.5, unit: '%' },
    // Effects and output (these never reach the engine)
    { id: 'delaySync', label: 'Sync', type: 'bool', def: true, fx: true },
    { id: 'delayDiv', label: 'Echo length', type: 'enum', options: ['1/16', '1/8T', '1/8', '1/8.', '1/4T', '1/4', '1/4.', '1/2'], def: '1/8.', fx: true },
    { id: 'delayTime', label: 'Echo time', type: 'knob', min: 0.02, max: 1.5, def: 0.36, curve: 'log', unit: 's', fx: true },
    { id: 'delayFb', label: 'Repeats', type: 'knob', min: 0, max: 0.9, def: 0.35, unit: '%', fx: true },
    { id: 'delayMix', label: 'Echo', type: 'knob', min: 0, max: 1, def: 0.15, unit: '%', fx: true },
    { id: 'reverbSize', label: 'Room', type: 'knob', min: 0.3, max: 6, def: 2.2, curve: 'log', unit: 's', fx: true },
    { id: 'reverbMix', label: 'Space', type: 'knob', min: 0, max: 1, def: 0.18, unit: '%', fx: true },
    { id: 'master', label: 'Volume', type: 'knob', min: 0, max: 1, def: 0.7, unit: '%', fx: true },
  ];

  const BY_ID = {};
  for (const p of PARAMS) BY_ID[p.id] = p;

  const OPTION_LABELS = {
    tri: 'Tri',
    saw: 'Saw',
    ramp: 'Ramp',
    square: 'Square',
    pulse: 'Pulse',
    sine: 'Sine',
    poly: 'Poly',
    mono: 'Mono',
    legato: 'Legato',
    unison: 'Unison',
    sh: 'S&H',
    lfo: 'LFO',
    osc3: 'Osc 3',
    noise: 'Noise',
    up: 'Up',
    down: 'Down',
    updown: 'Up-down',
    order: 'Played',
    random: 'Random',
    lp24: 'LP 24',
    lp12: 'LP 12',
    bp: 'BP',
    hp12: 'HP 12',
    hp24: 'HP 24',
    '1/8.': '1/8 dot',
    '1/4.': '1/4 dot',
    '1/8T': '1/8 trip',
    '1/4T': '1/4 trip',
  };

  function defaults() {
    const out = {};
    for (const p of PARAMS) out[p.id] = p.def;
    return out;
  }

  function engineParams(params) {
    const out = {};
    for (const p of PARAMS) if (!p.fx) out[p.id] = params[p.id];
    return out;
  }

  function clampParam(id, value) {
    const p = BY_ID[id];
    if (!p) return undefined;
    if (p.type === 'bool') return value === true || value === 'true' || value === 1;
    if (p.type === 'enum') return p.options.includes(value) ? value : p.def;
    let v = Number(value);
    if (!Number.isFinite(v)) return p.def;
    v = Math.min(p.max, Math.max(p.min, v));
    if (p.step) v = Math.round(v / p.step) * p.step;
    return v;
  }

  // Knob position (0..1) <-> value.
  function toNorm(p, value) {
    const v = Math.min(p.max, Math.max(p.min, value));
    if (p.curve === 'log') return Math.log(v / p.min) / Math.log(p.max / p.min);
    const lin = (v - p.min) / (p.max - p.min);
    if (p.curve === 'pow') return Math.cbrt(lin);
    return lin;
  }

  function fromNorm(p, norm) {
    const n = Math.min(1, Math.max(0, norm));
    let v;
    if (p.curve === 'log') v = p.min * Math.pow(p.max / p.min, n);
    else if (p.curve === 'pow') v = p.min + (p.max - p.min) * n * n * n;
    else v = p.min + (p.max - p.min) * n;
    if (p.step) v = Math.round(v / p.step) * p.step;
    return Math.min(p.max, Math.max(p.min, v));
  }

  const FEET = ["32'", "16'", "8'", "4'", "2'"];

  function signed(v, digits) {
    const s = Math.abs(v).toFixed(digits);
    if (Number(s) === 0) return (0).toFixed(digits);
    return (v < 0 ? '−' : '+') + s;
  }

  function formatParam(id, value) {
    const p = BY_ID[id];
    if (!p) return String(value);
    if (p.type === 'bool') return value ? 'On' : 'Off';
    if (p.type === 'enum') return OPTION_LABELS[value] || value;
    switch (p.unit) {
      case 'feet':
        return FEET[Math.round(value) + 2];
      case 's':
        if (value === 0) return 'Off';
        return value < 1 ? Math.round(value * 1000) + ' ms' : value.toFixed(2) + ' s';
      case 'hz':
        if (value >= 1000) return (value / 1000).toFixed(2) + ' kHz';
        return value < 10 ? value.toFixed(2) + ' Hz' : Math.round(value) + ' Hz';
      case 'st':
        return '±' + Math.round(value) + ' st';
      case 'semi':
        return p.bipolar ? signed(value, 2) + ' st' : value.toFixed(2) + ' st';
      case 'oct':
        return p.bipolar ? signed(value, 1) + ' oct' : value.toFixed(1) + ' oct';
      case 'ct':
        return Math.round(value) + ' ct';
      case 'bpm':
        return Math.round(value) + ' bpm';
      case 'x':
        return Math.round(value) + (Math.round(value) === 1 ? ' oct' : ' octs');
      case 'level':
        return (value * 10).toFixed(1);
      default:
        return Math.round(value * 100) + '%';
    }
  }

  // Fills gaps with defaults and pulls every value back into range, so a
  // hand-edited or older patch can never put the engine in a bad state.
  function sanitizePatch(params) {
    const out = defaults();
    if (params && typeof params === 'object') {
      // Patches saved before the tempo moved out of the arpeggiator.
      if ('arpBpm' in params && !('bpm' in params)) out.bpm = clampParam('bpm', params.arpBpm);
      for (const id of Object.keys(params)) {
        if (BY_ID[id]) out[id] = clampParam(id, params[id]);
      }
    }
    return out;
  }

  function toBase64(text) {
    const bytes = new TextEncoder().encode(text);
    let bin = '';
    for (const b of bytes) bin += String.fromCharCode(b);
    return btoa(bin);
  }

  function fromBase64(b64) {
    const bin = atob(b64);
    const bytes = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
    return new TextDecoder().decode(bytes);
  }

  const CODE_PREFIX = 'L24:';

  // Only the settings that differ from a fresh panel go in the code, so codes stay short.
  function encodePatch(name, params) {
    const d = defaults();
    const diff = {};
    for (const p of PARAMS) {
      const v = params[p.id];
      if (v !== d[p.id]) diff[p.id] = typeof v === 'number' ? Number(v.toFixed(4)) : v;
    }
    return CODE_PREFIX + toBase64(JSON.stringify({ n: String(name || 'Untitled'), p: diff }));
  }

  function decodePatch(code) {
    const text = String(code || '').trim();
    if (!text.startsWith(CODE_PREFIX)) {
      throw new Error('That is not a Ladder 24 patch code. Codes start with "L24:".');
    }
    let data;
    try {
      data = JSON.parse(fromBase64(text.slice(CODE_PREFIX.length)));
    } catch (e) {
      throw new Error('The patch code is incomplete or damaged. Copy the whole code and try again.');
    }
    if (!data || typeof data !== 'object' || typeof data.p !== 'object') {
      throw new Error('The patch code has no settings in it.');
    }
    return { name: String(data.n || 'Imported patch').slice(0, 40), params: sanitizePatch(data.p) };
  }

  // Two rows of a computer keyboard laid out like a piano, from C.
  const KEYMAP = {
    a: 0, w: 1, s: 2, e: 3, d: 4, f: 5, t: 6, g: 7, y: 8, h: 9, u: 10, j: 11,
    k: 12, o: 13, l: 14, p: 15, ';': 16, "'": 17,
  };

  const NOTE_NAMES = ['C', 'C♯', 'D', 'D♯', 'E', 'F', 'F♯', 'G', 'G♯', 'A', 'A♯', 'B'];

  function noteName(n) {
    return NOTE_NAMES[((n % 12) + 12) % 12] + (Math.floor(n / 12) - 1);
  }

  function isBlack(n) {
    return [1, 3, 6, 8, 10].includes(((n % 12) + 12) % 12);
  }

  // A factory patch. Strict on purpose: a misspelt setting or choice would
  // otherwise be dropped without a word, so it stops the page loading instead.
  function checkSettings(name, changes) {
    for (const id of Object.keys(changes)) {
      const p = BY_ID[id];
      if (!p) throw new Error(name + ': no such setting "' + id + '"');
      if (p.type === 'enum' && !p.options.includes(changes[id])) throw new Error(name + ': ' + id + ' cannot be ' + changes[id]);
      if (p.type === 'knob' && (changes[id] < p.min || changes[id] > p.max)) throw new Error(name + ': ' + id + ' out of range');
    }
  }

  function patch(name, group, changes) {
    checkSettings(name, changes);
    return { name, group, params: sanitizePatch(changes) };
  }

  // Grouped in the order the patch menu shows them.
  const PRESETS = [
    patch('Init', 'Basics', {}),

    // ---- Bass
    patch('Fat Bass', 'Bass', {
      master: 0.6,
      voiceMode: 'legato', glide: 0.04, o1Range: -1, o2Range: -1, o2Tune: 0.1, o3Range: -2, o3Wave: 'saw',
      m1: 0.9, m2: 0.8, m3: 0.6, cutoff: 260, res: 0.3, fEnvAmt: 3.6, fA: 0.002, fD: 0.28, fS: 0.05, fR: 0.12,
      aA: 0.002, aD: 1, aS: 1, aR: 0.08, drive: 0.45, delayMix: 0, reverbMix: 0.05,
    }),
    patch('Sub Bass', 'Bass', {
      master: 0.75,
      voiceMode: 'mono', o1Range: -1, o1Wave: 'sine', o2Range: -1, o2Wave: 'tri', m1: 1, m2: 0.5, cutoff: 420,
      res: 0, fEnvAmt: 0.6, fD: 0.3, fS: 0.4, aA: 0.003, aS: 1, aR: 0.1, drive: 0.15, delayMix: 0, reverbMix: 0,
    }),
    patch('Pedal Drop', 'Bass', {
      master: 0.5,
      voiceMode: 'mono', o1Range: -2, o1Wave: 'saw', o2Range: -2, o2Wave: 'pulse', o2Tune: -0.1, o3Range: -1,
      o3Wave: 'tri', m3: 0.5, pw: 0.22, cutoff: 120, res: 0.5, fEnvAmt: 4.2, fA: 0.001, fD: 0.55, fS: 0.12,
      aA: 0.002, aD: 2, aS: 0.9, aR: 0.4, drive: 0.7, delayMix: 0, reverbMix: 0.08,
    }),
    patch('Rubber Pluck', 'Bass', {
      master: 1,
      voiceMode: 'mono', o1Range: -1, o1Wave: 'square', o2Range: -1, o2Wave: 'saw', o2Tune: 0.04, cutoff: 180,
      res: 0.68, fEnvAmt: 5, fA: 0.001, fD: 0.16, fS: 0, fR: 0.1, aA: 0.001, aD: 0.35, aS: 0, aR: 0.1,
      velFilter: 0.7, delayMix: 0.08,
    }),
    patch('Acid Squelch', 'Bass', {
      master: 0.85,
      voiceMode: 'legato', glide: 0.06, o1Range: -1, o1Wave: 'saw', m2: 0, cutoff: 220, res: 0.86, fEnvAmt: 4.5,
      fA: 0.001, fD: 0.2, fS: 0, fR: 0.1, aD: 0.5, aS: 0.7, aR: 0.06, drive: 0.5, velFilter: 0.8, keyTrack: 0.3,
      delayMix: 0.12, reverbMix: 0.05,
    }),
    patch('Funk Bass', 'Bass', {
      master: 0.95,
      voiceMode: 'mono', o1Range: -1, o1Wave: 'square', o2Range: -1, o2Wave: 'saw', o2Tune: 0.05, m2: 0.7,
      cutoff: 300, res: 0.4, fEnvAmt: 3, fA: 0.003, fD: 0.12, fS: 0.15, fR: 0.08, aD: 0.25, aS: 0.5, aR: 0.06,
      velFilter: 0.6, delayMix: 0, reverbMix: 0.05,
    }),
    patch('Growl Bass', 'Bass', {
      master: 0.47,
      voiceMode: 'mono', o1Range: -1, o2Range: -1, o2Tune: -0.15, o3Range: -2, o3Wave: 'square', m3: 0.7,
      cutoff: 180, res: 0.55, fEnvAmt: 2, fD: 0.4, fS: 0.3, aS: 1, aR: 0.1, drive: 0.9, lfoRate: 6,
      modDepth: 0.3, modPitch: 0, modFilter: 1, delayMix: 0, reverbMix: 0.05,
    }),
    patch('Reese Bass', 'Bass', {
      master: 0.55,
      voiceMode: 'unison', unisonDetune: 25, spread: 0.6, o1Range: -1, o2Range: -1, o2Tune: 0.3, cutoff: 480,
      res: 0.2, fEnvAmt: 1, fD: 0.6, fS: 0.6, aS: 1, aR: 0.15, drive: 0.4, delayMix: 0, reverbMix: 0.08,
    }),
    patch('Synth-Pop Bass', 'Bass', {
      master: 0.85,
      voiceMode: 'mono', o1Range: -1, o1Wave: 'pulse', pw: 0.35, o2Range: 0, o2Wave: 'saw', m2: 0.4,
      cutoff: 600, res: 0.3, fEnvAmt: 2.5, fD: 0.18, fS: 0.2, fR: 0.08, aD: 0.3, aS: 0.6, aR: 0.08,
      delayMix: 0.05, reverbMix: 0.08,
    }),
    patch('Round Bass', 'Bass', {
      master: 0.7, voiceMode: 'mono', o1Wave: 'tri', o1Range: -1, o2Wave: 'sine', o2Range: -1, m2: 0.6, cutoff: 700, res: 0.1, fEnvAmt: 1, fD: 0.3, fS: 0.5, aS: 1, aR: 0.1, delayMix: 0, reverbMix: 0.04,
    }),
    patch('Punch Bass', 'Bass', {
      master: 1, voiceMode: 'mono', o1Range: -1, o2Wave: 'square', o2Range: -1, o2Tune: 0.05, m2: 0.5, pitchEnv: 2, cutoff: 250, res: 0.35, fEnvAmt: 4, fD: 0.1, fS: 0.2, aD: 0.4, aS: 0.6, aR: 0.08, delayMix: 0, reverbMix: 0.04,
    }),
    patch('Pluck Bass', 'Bass', {
      master: 0.9, voiceMode: 'mono', o1Range: -1, o2Wave: 'square', o2Range: -1, m2: 0.5, drive: 0.4, cutoff: 300, res: 0.5, fEnvAmt: 4.5, fD: 0.15, fS: 0, aD: 0.45, aS: 0, aR: 0.1, delayMix: 0, reverbMix: 0.05,
    }),
    patch('Soul Bass', 'Bass', {
      master: 1, voiceMode: 'mono', o1Wave: 'tri', o1Range: -1, o2Range: -1, m2: 0.25, cutoff: 450, res: 0.1, fEnvAmt: 1.5, fD: 0.2, fS: 0.3, aD: 0.8, aS: 0.3, aR: 0.1, velFilter: 0.5, delayMix: 0, reverbMix: 0.05,
    }),
    patch('Wide Unison Bass', 'Bass', {
      master: 0.72, voiceMode: 'unison', unisonDetune: 15, spread: 0.7, o1Range: -1, o2Wave: 'square', o2Range: -2, m2: 0.5, cutoff: 380, res: 0.3, fEnvAmt: 2.5, fD: 0.3, fS: 0.3, aS: 1, aR: 0.12, delayMix: 0, reverbMix: 0.05,
    }),
    patch('Detroit Bass', 'Bass', {
      master: 0.54, voiceMode: 'mono', o1Wave: 'square', o1Range: -1, o2Wave: 'square', o2Range: -1, o2Tune: 0.12, cutoff: 220, res: 0.55, fEnvAmt: 3, fD: 0.25, fS: 0.1, aS: 0.8, aR: 0.1, drive: 0.5, delayMix: 0.1, reverbMix: 0.05,
    }),
    patch('Electro Bass', 'Bass', {
      master: 0.76, voiceMode: 'mono', o1Range: -1, o2Range: 0, o2Tune: 0.1, m2: 0.4, pitchEnv: 5, cutoff: 400, res: 0.45, fEnvAmt: 3, fD: 0.12, fS: 0, aD: 0.3, aS: 0.4, aR: 0.06, drive: 0.6, delayMix: 0, reverbMix: 0.05,
    }),
    patch('808 Glide Bass', 'Bass', {
      master: 0.63, voiceMode: 'legato', glide: 0.08, o1Wave: 'sine', o1Range: -1, m2: 0, drive: 0.5, cutoff: 900, res: 0, fEnvAmt: 0, pitchEnv: 3, fD: 0.06, fS: 0, aD: 1.5, aS: 0.6, aR: 0.3, delayMix: 0, reverbMix: 0.03,
    }),
    patch('Hoover Bass', 'Bass', {
      master: 0.69, voiceMode: 'unison', unisonDetune: 30, spread: 0.9, glide: 0.15, o1Range: -1, o2Wave: 'pulse', o2Range: -1, pw: 0.15, o2Tune: 0.3, cutoff: 900, res: 0.25, fEnvAmt: 1, aS: 1, aR: 0.2, modDepth: 0.4, modPitch: 0.2, lfoRate: 5, reverbMix: 0.1,
    }),
    patch('Dub Bass', 'Bass', {
      master: 0.74, voiceMode: 'mono', o1Wave: 'sine', o1Range: -1, o2Wave: 'tri', o2Range: -1, o2Tune: 0.03, m2: 0.5, cutoff: 300, res: 0.2, fEnvAmt: 1, fD: 0.5, fS: 0.4, aS: 1, aR: 0.15, delayMix: 0.25, delayDiv: '1/4.', reverbMix: 0.08,
    }),
    patch('Squelchy Saw', 'Bass', {
      master: 0.65, voiceMode: 'legato', glide: 0.05, o1Range: -1, o2Range: -1, o2Tune: -0.08, cutoff: 160, res: 0.78, fEnvAmt: 4, fD: 0.25, fS: 0.15, drive: 0.7, aS: 0.9, aR: 0.08, delayMix: 0.05, reverbMix: 0.04,
    }),
    patch('Fingered Bass', 'Bass', {
      master: 0.92, voiceMode: 'mono', o1Wave: 'tri', o1Range: -1, o2Wave: 'square', o2Range: -1, m2: 0.2, cutoff: 600, res: 0.15, fEnvAmt: 2, fD: 0.08, fS: 0.3, aD: 0.6, aS: 0.4, aR: 0.12, velAmp: 0.7, velFilter: 0.6, delayMix: 0, reverbMix: 0.05,
    }),
    patch('Synced Wobble', 'Bass', {
      master: 0.54, voiceMode: 'mono', o1Range: -1, o2Wave: 'square', o2Range: -1, o2Tune: 0.1, cutoff: 150, res: 0.6, fEnvAmt: 0.5, aS: 1, aR: 0.1, drive: 0.6, lfoSync: true, lfoDiv: '1/8', modDepth: 1, modPitch: 0, modFilter: 3, delayMix: 0, reverbMix: 0.06,
    }),
    patch('Triplet Wobble', 'Bass', {
      master: 0.56, voiceMode: 'mono', o1Range: -1, o2Range: -1, o2Tune: -0.1, cutoff: 180, res: 0.55, fEnvAmt: 0.5, aS: 1, aR: 0.1, drive: 0.7, lfoSync: true, lfoDiv: '1/8T', lfoShape: 'saw', modDepth: 1, modPitch: 0, modFilter: 3, delayMix: 0, reverbMix: 0.06,
    }),
    patch('Gritty Bass', 'Bass', {
      master: 0.62, voiceMode: 'mono', o1Wave: 'pulse', o1Range: -1, pw: 0.1, o2Range: -1, o2Tune: -0.05, noise: 0.1, drive: 1, cutoff: 300, res: 0.4, fEnvAmt: 2.5, fD: 0.2, fS: 0.3, aS: 0.9, aR: 0.1, delayMix: 0, reverbMix: 0.04,
    }),
    patch('Velvet Sub', 'Bass', {
      master: 0.8, voiceMode: 'legato', glide: 0.03, o1Wave: 'sine', o1Range: -1, o2Wave: 'sine', o2Range: 0, m2: 0.2, cutoff: 300, res: 0, fEnvAmt: 0, aA: 0.01, aS: 1, aR: 0.2, delayMix: 0, reverbMix: 0.03,
    }),
    patch('Hollow Bass', 'Bass', {
      master: 0.77, voiceMode: 'mono', o1Wave: 'square', o1Range: -1, m2: 0, cutoff: 500, res: 0.2, fEnvAmt: 1, fD: 0.3, fS: 0.4, aS: 1, aR: 0.1, delayMix: 0, reverbMix: 0.05,
    }),
    patch('Snappy Bass', 'Bass', {
      master: 0.94, voiceMode: 'mono', o1Range: -1, o2Wave: 'pulse', o2Range: -1, pw: 0.3, pitchEnv: 1, cutoff: 250, res: 0.45, fEnvAmt: 4, fD: 0.09, fS: 0, aD: 0.35, aS: 0.2, aR: 0.08, delayMix: 0, reverbMix: 0.05,
    }),
    patch('Rumble Bass', 'Bass', {
      master: 0.59, voiceMode: 'mono', o1Range: -1, o2Range: -2, m2: 0.7, noise: 0.05, cutoff: 140, res: 0.3, fEnvAmt: 1, drive: 0.8, aA: 0.05, aS: 1, aR: 0.4, delayMix: 0, reverbMix: 0.1,
    }),
    patch('Fifth Stab Bass', 'Bass', {
      master: 0.79, o1Range: -1, o2Range: 0, o2Tune: 7, m1: 1, m2: 0.5, drive: 0.5, cutoff: 500, res: 0.3, fEnvAmt: 2.5, fD: 0.15, fS: 0, aD: 0.3, aS: 0, aR: 0.1, delayMix: 0.05, reverbMix: 0.08,
    }),
    patch('Talking Bass', 'Bass', {
      master: 0.61, voiceMode: 'mono', o1Range: -1, o2Wave: 'square', o2Range: -1, cutoff: 400, res: 0.7, fEnvAmt: 0.5, aS: 1, aR: 0.1, drive: 0.4, lfoSync: true, lfoDiv: '1/4', lfoShape: 'sh', modDepth: 1, modPitch: 0, modFilter: 2, delayMix: 0, reverbMix: 0.06,
    }),

    // ---- Lead
    patch('Glide Lead', 'Lead', {
      master: 0.84,
      voiceMode: 'legato', glide: 0.12, o1Wave: 'saw', o2Wave: 'square', o2Tune: 7, m2: 0.5, o3Range: 1,
      o3Wave: 'saw', o3Tune: 0.1, m3: 0.4, cutoff: 1700, res: 0.45, fEnvAmt: 2.2, fD: 0.5, fS: 0.45,
      aS: 1, aR: 0.3, modDepth: 0.18, modPitch: 0.35, lfoRate: 5.6, delayMix: 0.26, delayTime: 0.42,
      delayFb: 0.4, reverbMix: 0.22,
    }),
    patch('Prog Solo', 'Lead', {
      master: 0.76,
      voiceMode: 'legato', glide: 0.09, o2Tune: 0.08, o3Range: 1, o3Wave: 'saw', o3Tune: -0.07, m3: 0.5,
      cutoff: 2000, res: 0.3, fEnvAmt: 2, fA: 0.01, fD: 0.8, fS: 0.6, aS: 1, aR: 0.25, modDepth: 0.1,
      modPitch: 0.25, lfoRate: 5.2, delayMix: 0.3, delayTime: 0.38, reverbMix: 0.25,
    }),
    patch('Screaming Emphasis', 'Lead', {
      master: 0.57,
      voiceMode: 'mono', glide: 0.06, o1Wave: 'saw', o2Wave: 'saw', o2Tune: 0.15, cutoff: 900, res: 0.82,
      fEnvAmt: 3, fA: 0.25, fD: 0.8, fS: 0.6, drive: 0.75, aS: 1, modDepth: 0.1, modPitch: 0.25,
      delayMix: 0.22, reverbMix: 0.2,
    }),
    patch('Square Lead', 'Lead', {
      master: 0.62,
      voiceMode: 'mono', o1Wave: 'square', o2Wave: 'square', o2Tune: 0.1, m2: 0.5, cutoff: 2500, res: 0.2,
      fEnvAmt: 1, fD: 0.4, fS: 0.7, aS: 1, aR: 0.15, modDepth: 0.12, modPitch: 0.2, lfoRate: 5.5,
      delayMix: 0.2, reverbMix: 0.15,
    }),
    patch('Fifths Lead', 'Lead', {
      master: 0.47,
      voiceMode: 'unison', unisonDetune: 10, spread: 0.5, o1Wave: 'saw', o2Wave: 'square', o2Tune: 7, m2: 0.6,
      cutoff: 2200, res: 0.4, fEnvAmt: 1.5, fD: 0.5, fS: 0.6, drive: 0.6, aS: 1, aR: 0.2, delayMix: 0.2,
      reverbMix: 0.2,
    }),
    patch('Theremin', 'Lead', {
      master: 0.79,
      voiceMode: 'legato', glide: 0.35, o1Wave: 'sine', m2: 0, cutoff: 4000, res: 0, fEnvAmt: 0, aA: 0.08,
      aS: 1, aR: 0.4, modDepth: 0.35, modPitch: 0.35, lfoRate: 6, bendRange: 12, delayMix: 0.2,
      reverbMix: 0.35, reverbSize: 3,
    }),
    patch('Chiptune Lead', 'Lead', {
      master: 0.91,
      voiceMode: 'mono', o1Wave: 'pulse', pw: 0.125, m2: 0, cutoff: 18000, res: 0, fEnvAmt: 0, aA: 0.001,
      aD: 0.2, aS: 0.7, aR: 0.05, velAmp: 0, modDepth: 0.15, modPitch: 0.3, lfoRate: 7, delayMix: 0.1,
      reverbMix: 0.05,
    }),
    patch('Bright Saw Lead', 'Lead', {
      master: 0.82, voiceMode: 'legato', glide: 0.05, o2Tune: 0.1, cutoff: 3500, res: 0.3, fEnvAmt: 1, fS: 0.7, aS: 1, aR: 0.2, delayMix: 0.2,
    }),
    patch('Soft Flute Lead', 'Lead', {
      master: 0.94, voiceMode: 'mono', o1Wave: 'tri', m2: 0, noise: 0.05, cutoff: 1800, res: 0.1, fEnvAmt: 0, aA: 0.06, aS: 1, aR: 0.2, modDepth: 0.25, modPitch: 0.2, lfoRate: 5, reverbMix: 0.3,
    }),
    patch('Whistle Lead', 'Lead', {
      master: 0.79, voiceMode: 'legato', glide: 0.1, o1Wave: 'sine', o1Range: 1, m2: 0, cutoff: 6000, res: 0, fEnvAmt: 0, aA: 0.03, aS: 1, aR: 0.2, modDepth: 0.3, modPitch: 0.3, lfoRate: 6, reverbMix: 0.3,
    }),
    patch('Pan Pipe', 'Lead', {
      master: 1, voiceMode: 'mono', o1Wave: 'sine', m2: 0, noise: 0.2, cutoff: 2500, res: 0.3, keyTrack: 1, fEnvAmt: 0, aA: 0.04, aD: 0.3, aS: 0.7, aR: 0.2, reverbMix: 0.35,
    }),
    patch('Brass Lead', 'Lead', {
      master: 0.8, voiceMode: 'mono', o2Tune: 0.1, cutoff: 600, res: 0.2, fEnvAmt: 2.5, fA: 0.08, fD: 0.4, fS: 0.6, aA: 0.03, aS: 1, aR: 0.2, modDepth: 0.1, modPitch: 0.2,
    }),
    patch('G-Funk Lead', 'Lead', {
      master: 0.78, voiceMode: 'legato', glide: 0.15, o1Wave: 'sine', o2Wave: 'tri', o2Range: 1, o2Tune: 0, m2: 0.3, cutoff: 3000, res: 0, fEnvAmt: 0, aS: 1, aR: 0.3, modDepth: 0.3, modPitch: 0.4, lfoRate: 5.5, delayMix: 0.2,
    }),
    patch('Acid Lead', 'Lead', {
      master: 0.59, voiceMode: 'mono', glide: 0.04, o1Wave: 'square', m2: 0, cutoff: 600, res: 0.85, fEnvAmt: 3.5, fD: 0.18, fS: 0.1, drive: 0.6, aS: 1, aR: 0.1, delayMix: 0.2,
    }),
    patch('Nasal Lead', 'Lead', {
      master: 0.87, voiceMode: 'mono', o1Wave: 'pulse', pw: 0.1, o2Wave: 'pulse', o2Tune: 0.08, m2: 0.5, cutoff: 1500, res: 0.5, fEnvAmt: 1, aS: 1, aR: 0.15, modDepth: 0.1, modPitch: 0.2,
    }),
    patch('Detuned Lead', 'Lead', {
      master: 0.75, voiceMode: 'unison', unisonDetune: 18, o2Range: 1, o2Tune: 0, m2: 0.5, cutoff: 2500, res: 0.3, fEnvAmt: 1, aS: 1, aR: 0.2, delayMix: 0.2, reverbMix: 0.2,
    }),
    patch('Fat Mono Lead', 'Lead', {
      master: 0.59, voiceMode: 'mono', o2Tune: 0.1, o3Wave: 'saw', o3Range: -1, o3Tune: 0, m3: 0.6, cutoff: 1300, res: 0.35, fEnvAmt: 1.5, fD: 0.5, fS: 0.6, drive: 0.5, aS: 1, aR: 0.15,
    }),
    patch('Portamento Lead', 'Lead', {
      master: 0.98, voiceMode: 'legato', glide: 0.3, o2Wave: 'square', o2Range: 1, o2Tune: 0, m2: 0.3, cutoff: 1800, res: 0.4, fEnvAmt: 1, aS: 1, aR: 0.25, modDepth: 0.12, modPitch: 0.25, delayMix: 0.2,
    }),
    patch('Handheld Game Lead', 'Lead', {
      master: 0.74, voiceMode: 'mono', o1Wave: 'pulse', pw: 0.25, m2: 0, cutoff: 18000, res: 0, fEnvAmt: 0, aD: 0.3, aS: 0.8, aR: 0.05, velAmp: 0, modDepth: 0.2, modPitch: 0.4, lfoRate: 8, delayMix: 0, reverbMix: 0.05,
    }),
    patch('Laser Lead', 'Lead', {
      master: 1, voiceMode: 'mono', m2: 0, cutoff: 900, res: 0.7, fEnvAmt: 3, fD: 0.3, fS: 0.4, pitchEnv: 3, aS: 1, aR: 0.2, delayMix: 0.3,
    }),
    patch('Smooth Sine Lead', 'Lead', {
      master: 0.77, voiceMode: 'legato', glide: 0.06, o1Wave: 'sine', o2Wave: 'sine', o2Range: 1, o2Tune: 0.05, m2: 0.3, cutoff: 5000, res: 0, fEnvAmt: 0, aS: 1, aR: 0.3, modDepth: 0.15, modPitch: 0.2, reverbMix: 0.3,
    }),
    patch('Overdrive Lead', 'Lead', {
      master: 0.44, voiceMode: 'mono', o1Wave: 'square', o2Tune: 0.05, drive: 1, cutoff: 2000, res: 0.3, fEnvAmt: 1, aS: 1, aR: 0.15, delayMix: 0.2,
    }),
    patch('Hollow Square Lead', 'Lead', {
      master: 0.81, voiceMode: 'legato', glide: 0.04, o1Wave: 'square', o2Wave: 'square', o2Range: 1, o2Tune: 0, m2: 0.3, cutoff: 1200, res: 0.55, fEnvAmt: 1, aS: 1, aR: 0.2,
    }),
    patch('Octave Lead', 'Lead', {
      master: 0.85, voiceMode: 'legato', o2Range: 1, o2Tune: 0.05, m2: 0.6, o3Wave: 'saw', o3Range: -1, o3Tune: 0, m3: 0.4, cutoff: 2200, res: 0.25, fEnvAmt: 1, aS: 1, aR: 0.2,
    }),
    patch('Weeping Lead', 'Lead', {
      master: 0.93, voiceMode: 'legato', glide: 0.2, o1Wave: 'tri', o2Tune: 0.12, m2: 0.4, cutoff: 1400, res: 0.45, fEnvAmt: 1, fA: 0.4, fS: 0.6, aA: 0.1, aS: 1, aR: 0.4, modDepth: 0.3, modPitch: 0.35, lfoRate: 5, reverbMix: 0.35, delayMix: 0.3,
    }),
    patch('Oboe Lead', 'Lead', {
      master: 0.95, voiceMode: 'mono', o1Wave: 'pulse', pw: 0.15, m2: 0, cutoff: 1600, res: 0.45, keyTrack: 1, fEnvAmt: 0.5, aA: 0.05, aS: 1, aR: 0.15, modDepth: 0.2, modPitch: 0.15, lfoRate: 5, reverbMix: 0.25,
    }),
    patch('Clarinet Lead', 'Lead', {
      master: 0.74, voiceMode: 'mono', o1Wave: 'square', m2: 0, cutoff: 1200, res: 0.1, keyTrack: 0.8, fEnvAmt: 0.8, fA: 0.05, fS: 0.8, aA: 0.04, aS: 1, aR: 0.15, modDepth: 0.15, modPitch: 0.1, reverbMix: 0.25,
    }),
    patch('Trumpet Lead', 'Lead', {
      master: 0.79, voiceMode: 'legato', glide: 0.02, o2Tune: 0.05, cutoff: 900, res: 0.2, fEnvAmt: 2, fA: 0.05, fD: 0.3, fS: 0.6, aA: 0.03, aS: 1, aR: 0.15, modDepth: 0.2, modPitch: 0.15, lfoRate: 5.5, reverbMix: 0.25,
    }),
    patch('Wah Lead', 'Lead', {
      master: 0.71, voiceMode: 'mono', o2Wave: 'square', o2Tune: 0.1, cutoff: 500, res: 0.7, fEnvAmt: 0.5, aS: 1, aR: 0.15, lfoSync: true, lfoDiv: '1/4', modDepth: 1, modPitch: 0, modFilter: 2.5, delayMix: 0.15,
    }),
    patch('Siren Lead', 'Lead', {
      master: 0.7, voiceMode: 'legato', glide: 0.5, o1Wave: 'square', o2Tune: 0.05, cutoff: 2500, res: 0.3, fEnvAmt: 0.5, aS: 1, aR: 0.3, bendRange: 12, modDepth: 0.4, modPitch: 0.5, lfoRate: 7, delayMix: 0.2,
    }),

    // ---- Keys
    patch('Electric Piano', 'Keys', {
      master: 0.6,
      o1Wave: 'sine', o2Wave: 'tri', o2Range: 1, o2Tune: 0.05, m2: 0.35, cutoff: 2500, res: 0, fEnvAmt: 1.5,
      fD: 0.6, fS: 0, aA: 0.002, aD: 2.2, aS: 0, aR: 0.5, velAmp: 0.8, velFilter: 0.6, spread: 0.7,
      reverbMix: 0.25, delayMix: 0.05,
    }),
    patch('Clav', 'Keys', {
      master: 0.81,
      o1Wave: 'pulse', pw: 0.15, o2Wave: 'square', o2Range: 1, m2: 0.3, cutoff: 900, res: 0.5, fEnvAmt: 3.5,
      fD: 0.12, fS: 0, fR: 0.06, aD: 0.4, aS: 0, aR: 0.08, velFilter: 0.7, delayMix: 0, reverbMix: 0.1,
    }),
    patch('Drawbar Organ', 'Keys', {
      master: 0.49,
      o1Wave: 'sine', o2Wave: 'sine', o2Range: 1, o2Tune: 0, o3Wave: 'sine', o3Range: -1, o3Tune: 0, m1: 0.8,
      m2: 0.6, m3: 0.6, cutoff: 6000, res: 0, fEnvAmt: 0, aA: 0.003, aD: 0.1, aS: 1, aR: 0.05, velAmp: 0,
      modDepth: 0.3, modPitch: 0.08, lfoRate: 6.5, spread: 0.8, reverbMix: 0.2,
    }),
    patch('Poly Stab', 'Keys', {
      master: 0.8,
      o2Tune: 0.1, cutoff: 1200, res: 0.35, fEnvAmt: 3, fD: 0.25, fS: 0, aD: 0.5, aS: 0, aR: 0.2,
      spread: 0.8, delayMix: 0.2, reverbMix: 0.3,
    }),

    patch('Bell Piano', 'Keys', {
      master: 0.62, o1Wave: 'sine', o2Wave: 'sine', o2Range: 2, o2Tune: 0, m2: 0.3, o3Wave: 'tri', o3Range: 1, o3Tune: 7, m3: 0.15, cutoff: 5000, res: 0, fEnvAmt: 0, aA: 0.002, aD: 2.5, aS: 0, aR: 0.8, velAmp: 0.8, spread: 0.7, reverbMix: 0.3,
    }),
    patch('Reed Piano', 'Keys', {
      master: 0.8, o1Wave: 'tri', o2Wave: 'square', o2Range: 1, o2Tune: 0, m2: 0.15, cutoff: 1500, res: 0.2, fEnvAmt: 1.5, fD: 0.5, fS: 0.2, aA: 0.002, aD: 1.5, aS: 0.2, aR: 0.3, velAmp: 0.7, velFilter: 0.7, spread: 0.6, reverbMix: 0.2,
    }),
    patch('Lo-Fi Piano', 'Keys', {
      master: 0.57, o1Wave: 'tri', o2Wave: 'saw', o2Tune: 0.15, m2: 0.2, noise: 0.03, cutoff: 1400, res: 0, fEnvAmt: 1.5, fD: 0.8, fS: 0, aA: 0.002, aD: 2, aS: 0, aR: 0.5, drive: 0.4, modDepth: 0.3, modPitch: 0.1, lfoRate: 0.6, velAmp: 0.7, reverbMix: 0.25,
    }),
    patch('Toy Piano', 'Keys', {
      master: 0.73, o1Wave: 'tri', o1Range: 1, o2Wave: 'sine', o2Range: 2, o2Tune: 0.3, m2: 0.4, cutoff: 4000, res: 0.1, fEnvAmt: 0, aA: 0.001, aD: 0.7, aS: 0, aR: 0.3, velAmp: 0.7, reverbMix: 0.25,
    }),
    patch('Harpsichord', 'Keys', {
      master: 0.74, o1Wave: 'pulse', pw: 0.12, o2Wave: 'saw', o2Range: 1, o2Tune: 0, m2: 0.5, cutoff: 2200, res: 0.3, fEnvAmt: 2, fD: 0.4, fS: 0.1, aA: 0.001, aD: 1.2, aS: 0, aR: 0.15, velAmp: 0.2, reverbMix: 0.25,
    }),
    patch('Celesta', 'Keys', {
      master: 0.64, o1Wave: 'sine', o1Range: 1, o2Wave: 'tri', o2Range: 2, o2Tune: 0, m2: 0.2, cutoff: 6000, res: 0, fEnvAmt: 0, aA: 0.001, aD: 1.2, aS: 0, aR: 1, velAmp: 0.8, reverbMix: 0.4, reverbSize: 3,
    }),
    patch('Mallet Keys', 'Keys', {
      master: 0.63, o1Wave: 'sine', o2Wave: 'sine', o2Range: 2, o2Tune: -0.2, m2: 0.25, cutoff: 4000, res: 0, fEnvAmt: 0, aA: 0.001, aD: 1.8, aS: 0, aR: 1.2, velAmp: 0.7, modDepth: 0.5, modPitch: 0.05, lfoRate: 5.5, spread: 0.8, reverbMix: 0.35,
    }),
    patch('Muted Keys', 'Keys', {
      master: 0.72, o1Wave: 'square', m2: 0, cutoff: 500, res: 0.3, fEnvAmt: 2, fD: 0.15, fS: 0, aA: 0.001, aD: 0.25, aS: 0, aR: 0.1, velFilter: 0.6, delayMix: 0.15, reverbMix: 0.15,
    }),
    patch('Synth Keys', 'Keys', {
      master: 0.65, o1Wave: 'saw', o2Wave: 'pulse', pw: 0.3, o2Tune: 0.07, cutoff: 1500, res: 0.25, fEnvAmt: 2.5, fD: 0.7, fS: 0.3, aD: 1, aS: 0.5, aR: 0.4, spread: 0.8, reverbMix: 0.25, delayMix: 0.1,
    }),
    patch('House Piano', 'Keys', {
      master: 0.74, o1Wave: 'saw', o2Wave: 'square', o2Range: 1, o2Tune: 0.1, m2: 0.4, o3Wave: 'tri', o3Range: -1, o3Tune: 0, m3: 0.4, cutoff: 1800, res: 0.2, fEnvAmt: 2, fD: 0.3, fS: 0.1, aA: 0.001, aD: 0.7, aS: 0.1, aR: 0.25, spread: 0.7, delayMix: 0.2, reverbMix: 0.3,
    }),
    patch('Combo Organ', 'Keys', {
      master: 0.48, o1Wave: 'square', o2Wave: 'square', o2Range: 1, o2Tune: 0, m2: 0.5, o3Wave: 'pulse', o3Range: -1, o3Tune: 0, pw: 0.3, m3: 0.3, cutoff: 2800, res: 0.1, fEnvAmt: 0, aA: 0.003, aS: 1, aR: 0.04, velAmp: 0, modDepth: 0.4, modPitch: 0.12, lfoRate: 6, reverbMix: 0.2,
    }),
    patch('Church Organ', 'Keys', {
      master: 0.49, o1Wave: 'sine', o2Wave: 'sine', o2Range: 1, o2Tune: 0, o3Wave: 'square', o3Range: -1, o3Tune: 0, m1: 0.8, m2: 0.6, m3: 0.35, cutoff: 3000, res: 0, fEnvAmt: 0, aA: 0.08, aS: 1, aR: 0.6, velAmp: 0, spread: 1, reverbMix: 0.55, reverbSize: 5,
    }),
    patch('Gospel Organ', 'Keys', {
      master: 0.51, o1Wave: 'sine', o2Wave: 'sine', o2Range: 1, o2Tune: 7, m2: 0.5, o3Wave: 'sine', o3Range: 2, o3Tune: 0, m3: 0.5, noise: 0.05, cutoff: 5000, res: 0, fEnvAmt: 1, fD: 0.05, fS: 0, aA: 0.002, aS: 1, aR: 0.08, velAmp: 0, modDepth: 0.35, modPitch: 0.1, lfoRate: 6.8, spread: 0.8, reverbMix: 0.25,
    }),
    patch('Bright Piano', 'Keys', {
      master: 0.7, o2Wave: 'tri', o2Range: 1, o2Tune: 0, m2: 0.4, cutoff: 2500, res: 0, fEnvAmt: 2, fD: 0.5, fS: 0, aA: 0.002, aD: 2, aS: 0, aR: 0.4, velAmp: 0.8, velFilter: 0.6, spread: 0.7, reverbMix: 0.25,
    }),
    patch('Upright Piano', 'Keys', {
      master: 0.66, o1Wave: 'tri', o2Tune: 0.04, m2: 0.25, cutoff: 1800, res: 0, fEnvAmt: 1.5, fD: 1, fS: 0, aA: 0.002, aD: 2.5, aS: 0, aR: 0.5, velAmp: 0.8, velFilter: 0.7, spread: 0.7, reverbMix: 0.2,
    }),
    patch('Glass Keys', 'Keys', {
      master: 0.62, o1Wave: 'sine', o2Wave: 'sine', o2Range: 2, o2Tune: 0.1, m2: 0.3, o3Wave: 'sine', o3Range: 1, o3Tune: 7, m3: 0.15, cutoff: 6000, res: 0, fEnvAmt: 0, aA: 0.002, aD: 1.5, aS: 0, aR: 0.6, velAmp: 0.7, reverbMix: 0.35, delayMix: 0.15,
    }),
    patch('Tine Piano', 'Keys', {
      master: 0.63, o1Wave: 'sine', o2Wave: 'sine', o2Range: 2, o2Tune: 0, m2: 0.2, cutoff: 3500, res: 0, fEnvAmt: 1, fD: 0.1, fS: 0, aA: 0.002, aD: 2.2, aS: 0.05, aR: 0.5, velAmp: 0.8, spread: 0.8, reverbMix: 0.25,
    }),
    patch('Bright Tines', 'Keys', {
      master: 0.69, o1Wave: 'sine', o2Wave: 'tri', o2Range: 1, o2Tune: 0, m2: 0.45, noise: 0.02, cutoff: 4000, res: 0.2, fEnvAmt: 1.5, fD: 0.2, fS: 0.2, aA: 0.002, aD: 1.8, aS: 0.1, aR: 0.5, velAmp: 0.8, velFilter: 0.5, spread: 0.8, reverbMix: 0.25,
    }),
    patch('Accordion', 'Keys', {
      master: 0.49, o1Wave: 'pulse', pw: 0.3, o2Wave: 'pulse', o2Tune: 0.1, m2: 0.6, o3Wave: 'saw', o3Range: 1, o3Tune: 0, m3: 0.3, cutoff: 2200, res: 0.15, fEnvAmt: 0, aA: 0.04, aS: 1, aR: 0.1, velAmp: 0, modDepth: 0.2, modPitch: 0.05, lfoRate: 6, reverbMix: 0.2,
    }),
    patch('Harmonium', 'Keys', {
      master: 0.62, o2Wave: 'square', o2Range: 1, o2Tune: 0, m2: 0.4, cutoff: 1500, res: 0.1, fEnvAmt: 0, aA: 0.1, aS: 1, aR: 0.2, velAmp: 0, reverbMix: 0.3,
    }),
    patch('Music Box', 'Keys', {
      master: 0.66, o1Wave: 'sine', o1Range: 2, o2Wave: 'tri', o2Range: 2, o2Tune: 0.2, m2: 0.2, cutoff: 8000, res: 0, fEnvAmt: 0, aA: 0.001, aD: 0.8, aS: 0, aR: 0.6, velAmp: 0.6, reverbMix: 0.4,
    }),
    patch('Clavinet Wah', 'Keys', {
      master: 0.81, o1Wave: 'pulse', pw: 0.15, m2: 0, cutoff: 700, res: 0.6, fEnvAmt: 2, fD: 0.15, fS: 0.1, aD: 0.5, aS: 0, aR: 0.1, lfoSync: true, lfoDiv: '1/8', modDepth: 1, modPitch: 0, modFilter: 1.5, delayMix: 0.05, reverbMix: 0.15,
    }),
    patch('Mellow Keys', 'Keys', {
      master: 0.63, o1Wave: 'sine', o2Wave: 'tri', o2Range: 1, o2Tune: 0, m2: 0.2, cutoff: 1500, res: 0, fEnvAmt: 0, aA: 0.005, aD: 3, aS: 0.1, aR: 0.6, velAmp: 0.6, modDepth: 0.4, modPitch: 0.03, lfoRate: 4, spread: 0.8, reverbMix: 0.3,
    }),
    patch('Stab Organ', 'Keys', {
      master: 0.7, o1Wave: 'square', o2Wave: 'square', o2Range: 1, o2Tune: 0, m2: 0.5, cutoff: 2500, res: 0.2, fEnvAmt: 0, aA: 0.002, aD: 0.25, aS: 0, aR: 0.1, velAmp: 0.3, delayMix: 0.2, reverbMix: 0.3,
    }),
    patch('Rock Organ', 'Keys', {
      master: 0.42, o1Wave: 'sine', o2Wave: 'sine', o2Range: 1, o2Tune: 0, m2: 0.7, o3Wave: 'square', o3Range: -1, o3Tune: 0, m3: 0.3, drive: 0.8, cutoff: 3500, res: 0, fEnvAmt: 0, aA: 0.002, aS: 1, aR: 0.05, velAmp: 0, modDepth: 0.45, modPitch: 0.1, lfoRate: 6.5, reverbMix: 0.2,
    }),
    patch('Bark Piano', 'Keys', {
      master: 0.5, o1Wave: 'tri', o2Wave: 'square', o2Range: 1, o2Tune: 0, m2: 0.25, drive: 0.7, cutoff: 1700, res: 0.2, fEnvAmt: 2, fD: 0.3, fS: 0.2, aA: 0.002, aD: 1.4, aS: 0.2, aR: 0.3, velFilter: 0.8, reverbMix: 0.2,
    }),

    // ---- Pads
    patch('Warm Pad', 'Pads', {
      master: 0.64,
      o2Tune: 0.1, o3Wave: 'tri', o3Range: -1, m3: 0.4, cutoff: 900, res: 0.15, fEnvAmt: 1.2, fA: 1.2,
      fD: 2, fS: 0.5, fR: 2, aA: 1.5, aD: 1, aS: 0.9, aR: 2.5, modDepth: 0.3, modPitch: 0.05, lfoRate: 0.3,
      spread: 1, reverbMix: 0.45, reverbSize: 4,
    }),
    patch('Glass Pad', 'Pads', {
      master: 0.68,
      o1Wave: 'tri', o2Wave: 'sine', o2Range: 1, o2Tune: 0.05, o3Wave: 'pulse', o3Range: 1, o3Tune: 0,
      pw: 0.1, m3: 0.25, cutoff: 3500, res: 0.45, fEnvAmt: 0, aA: 0.8, aD: 1, aS: 0.9, aR: 3, modDepth: 1,
      modPitch: 0, modPw: 0.3, lfoRate: 0.2, spread: 1, reverbMix: 0.5, reverbSize: 5, delayMix: 0.25,
    }),
    patch('Slow Sweep', 'Pads', {
      master: 0.69,
      o2Tune: 0.12, cutoff: 300, res: 0.55, fEnvAmt: 4, fA: 4, fD: 4, fS: 0.3, fR: 3, aA: 1, aS: 1, aR: 3,
      modDepth: 1, modPitch: 0, modFilter: 0.8, lfoRate: 0.1, spread: 1, reverbMix: 0.45, reverbSize: 4,
    }),
    patch('Supersaw Unison', 'Pads', {
      master: 0.75,
      voiceMode: 'unison', unisonDetune: 22, spread: 0.95, o2Tune: 7, m2: 0.5, cutoff: 3200, res: 0.2,
      fEnvAmt: 1.5, fD: 0.6, fS: 0.5, aA: 0.01, aS: 1, aR: 0.5, reverbMix: 0.3, delayMix: 0.2,
    }),
    patch('Dark Drone', 'Pads', {
      master: 0.52,
      voiceMode: 'unison', unisonDetune: 15, spread: 1, o1Range: -1, o2Range: -1, o2Wave: 'square',
      o2Tune: -0.2, noise: 0.1, cutoff: 320, res: 0.45, drive: 0.5, fEnvAmt: 0.5, fA: 2, fD: 3, fS: 0.5, aA: 2, aS: 1,
      aR: 4, modDepth: 1, modPitch: 0, modFilter: 1.5, lfoRate: 0.08, reverbMix: 0.5, reverbSize: 6,
    }),

    patch('Choir Pad', 'Pads', {
      master: 0.63, o2Tune: 0.1, o3Wave: 'pulse', o3Range: 1, o3Tune: 0, pw: 0.2, m3: 0.3, cutoff: 1100, res: 0.5, keyTrack: 0.3, fEnvAmt: 0.3, fA: 0.9, fS: 1, aA: 0.9, aS: 1, aR: 1.8, modDepth: 0.25, modPitch: 0.08, lfoRate: 4.5, spread: 1, reverbMix: 0.5, reverbSize: 4,
    }),
    patch('Shimmer Pad', 'Pads', {
      master: 0.78, voiceMode: 'unison', unisonDetune: 18, spread: 1, o2Range: 1, o2Tune: 7, m2: 0.4, cutoff: 4000, res: 0.2, fEnvAmt: 0, aA: 1.5, aS: 1, aR: 3, reverbMix: 0.6, reverbSize: 6, delayMix: 0.3, delayDiv: '1/4', delayFb: 0.55,
    }),
    patch('Space Pad', 'Pads', {
      master: 0.65, o1Wave: 'tri', o2Wave: 'pulse', o2Range: 1, o2Tune: 0.05, noise: 0.05, cutoff: 1800, res: 0.6, fEnvAmt: 0, aA: 2, aS: 1, aR: 4, lfoRate: 0.15, modDepth: 1, modPitch: 0, modFilter: 1.5, spread: 1, reverbMix: 0.6, reverbSize: 6, delayMix: 0.35, delayFb: 0.5,
    }),
    patch('PWM Pad', 'Pads', {
      master: 0.47, o1Wave: 'pulse', o2Wave: 'pulse', o2Tune: 0.08, pw: 0.3, cutoff: 2000, res: 0.1, fEnvAmt: 0.5, fA: 0.5, fS: 0.8, aA: 0.5, aS: 1, aR: 1.5, lfoRate: 0.5, modDepth: 1, modPitch: 0, modPw: 0.3, spread: 1, reverbMix: 0.4,
    }),
    patch('Breathing Pad', 'Pads', {
      master: 0.67, o2Tune: 0.1, cutoff: 400, res: 0.4, fEnvAmt: 0, aA: 0.5, aS: 1, aR: 2, lfoSync: true, lfoDiv: '1 bar', lfoShape: 'tri', modDepth: 1, modPitch: 0, modFilter: 2, spread: 1, reverbMix: 0.45,
    }),
    patch('Rhythm Pad', 'Pads', {
      master: 0.57, o2Tune: 0.1, o2Wave: 'square', cutoff: 300, res: 0.45, fEnvAmt: 0, aA: 0.1, aS: 1, aR: 1, lfoSync: true, lfoDiv: '1/8', lfoShape: 'square', modDepth: 1, modPitch: 0, modFilter: 3, spread: 1, delayMix: 0.25, reverbMix: 0.35,
    }),
    patch('Ambient Wash', 'Pads', {
      master: 0.65, o1Wave: 'sine', o2Wave: 'tri', o2Range: 1, o2Tune: 0.06, noise: 0.3, cutoff: 2500, res: 0.3, fEnvAmt: 0, aA: 2, aS: 1, aR: 5, lfoRate: 0.1, modDepth: 1, modPitch: 0, modFilter: 0.8, spread: 1, reverbMix: 0.7, reverbSize: 6, delayMix: 0.3, delayFb: 0.6,
    }),
    patch('Cinematic Swell', 'Pads', {
      master: 0.72, voiceMode: 'unison', unisonDetune: 12, spread: 1, o2Wave: 'square', o2Range: -1, o2Tune: 0, m2: 0.5, cutoff: 200, res: 0.3, fEnvAmt: 3.5, fA: 2.5, fD: 3, fS: 0.8, aA: 2, aS: 1, aR: 3, reverbMix: 0.5, reverbSize: 5,
    }),
    patch('Soft Sine Pad', 'Pads', {
      master: 0.55, o1Wave: 'sine', o2Wave: 'sine', o2Range: 1, o2Tune: 0.08, m2: 0.4, cutoff: 3000, res: 0, fEnvAmt: 0, aA: 1, aS: 1, aR: 2.5, modDepth: 0.4, modPitch: 0.06, lfoRate: 4.8, spread: 1, reverbMix: 0.45,
    }),
    patch('80s Poly Pad', 'Pads', {
      master: 0.58, o2Wave: 'pulse', pw: 0.3, o2Tune: 0.1, cutoff: 1300, res: 0.3, fEnvAmt: 1.5, fA: 0.4, fD: 1.5, fS: 0.4, aA: 0.3, aS: 1, aR: 1.2, modDepth: 0.2, modPitch: 0.06, lfoRate: 5, spread: 1, reverbMix: 0.4, delayMix: 0.15,
    }),
    patch('Vapor Pad', 'Pads', {
      master: 0.53, o2Tune: 0.25, noise: 0.06, cutoff: 900, res: 0.15, fEnvAmt: 0, aA: 0.8, aS: 1, aR: 2, modDepth: 0.5, modPitch: 0.15, lfoRate: 0.4, drive: 0.4, spread: 1, reverbMix: 0.5,
    }),
    patch('Glacier', 'Pads', {
      master: 0.78, o1Wave: 'tri', o2Wave: 'sine', o2Range: 2, o2Tune: 0, m2: 0.4, cutoff: 2200, res: 0.5, fEnvAmt: 0, aA: 2.5, aS: 1, aR: 4, lfoRate: 0.15, modDepth: 1, modPitch: 0, modFilter: 0.6, spread: 1, reverbMix: 0.65, reverbSize: 6,
    }),
    patch('Hollow Pad', 'Pads', {
      master: 0.54, o1Wave: 'square', o2Wave: 'square', o2Range: 1, o2Tune: 0.07, m2: 0.5, cutoff: 1500, res: 0.35, fEnvAmt: 0.5, fA: 0.7, fS: 0.7, aA: 0.7, aS: 1, aR: 2, spread: 1, reverbMix: 0.45,
    }),
    patch('String Pad', 'Pads', {
      master: 0.59, o2Tune: 0.15, o3Wave: 'saw', o3Range: 1, o3Tune: -0.1, m3: 0.3, cutoff: 1800, res: 0.1, fEnvAmt: 0, aA: 0.8, aS: 1, aR: 1.5, modDepth: 0.3, modPitch: 0.06, lfoRate: 5, spread: 1, reverbMix: 0.45,
    }),
    patch('Dream Pad', 'Pads', {
      master: 0.74, o1Wave: 'tri', o2Range: 1, o2Tune: 0.1, m2: 0.3, cutoff: 1500, res: 0.35, fEnvAmt: 1, fA: 1.5, fS: 0.5, aA: 1.2, aS: 1, aR: 3, spread: 1, reverbMix: 0.55, delayMix: 0.25,
    }),
    patch('Night Pad', 'Pads', {
      master: 0.53, o1Wave: 'square', o2Tune: -0.1, cutoff: 600, res: 0.3, fEnvAmt: 0, aA: 1, aS: 1, aR: 2.5, lfoRate: 0.2, modDepth: 1, modPitch: 0, modFilter: 0.8, spread: 1, reverbMix: 0.5,
    }),
    patch('Sunrise Pad', 'Pads', {
      master: 0.68, o2Range: 1, o2Tune: 0.1, cutoff: 200, res: 0.3, fEnvAmt: 4, fA: 2.5, fD: 3, fS: 0.7, aA: 1.5, aS: 1, aR: 2.5, spread: 1, reverbMix: 0.5,
    }),
    patch('Organ Pad', 'Pads', {
      master: 0.52, o1Wave: 'sine', o2Wave: 'sine', o2Range: 1, o2Tune: 0, o3Wave: 'sine', o3Range: -1, o3Tune: 0, m3: 0.5, cutoff: 4000, res: 0, fEnvAmt: 0, aA: 0.6, aS: 1, aR: 1.5, modDepth: 0.3, modPitch: 0.06, spread: 1, reverbMix: 0.5,
    }),
    patch('Two-Bar Sweep', 'Pads', {
      master: 0.7, voiceMode: 'unison', unisonDetune: 25, spread: 1, cutoff: 400, res: 0.5, fEnvAmt: 0, aA: 0.8, aS: 1, aR: 2, lfoSync: true, lfoDiv: '2 bars', modDepth: 1, modPitch: 0, modFilter: 2, reverbMix: 0.45,
    }),
    patch('Bell Pad', 'Pads', {
      master: 0.61, o1Wave: 'sine', o2Wave: 'tri', o2Range: 2, o2Tune: 7, m2: 0.3, cutoff: 3000, res: 0, fEnvAmt: 0, aA: 0.01, aD: 2, aS: 0.6, aR: 2.5, spread: 1, reverbMix: 0.5,
    }),
    patch('Frozen Pad', 'Pads', {
      master: 0.83, o1Wave: 'tri', o2Wave: 'pulse', o2Range: 1, pw: 0.1, m2: 0.3, noise: 0.05, cutoff: 3000, res: 0.55, fEnvAmt: 0, aA: 2, aS: 1, aR: 4, lfoRate: 0.1, modDepth: 1, modPitch: 0, modPw: 0.3, spread: 1, reverbMix: 0.65, reverbSize: 6,
    }),
    patch('Sixteenth Gate Pad', 'Pads', {
      master: 0.67, o2Tune: 0.1, cutoff: 500, res: 0.4, fEnvAmt: 0, aA: 0.1, aS: 1, aR: 1, lfoSync: true, lfoDiv: '1/16', lfoShape: 'square', modDepth: 1, modPitch: 0, modFilter: 2.5, spread: 1, delayMix: 0.2, reverbMix: 0.35,
    }),
    patch('Low Drone Pad', 'Pads', {
      master: 0.59, o1Range: -1, o2Wave: 'square', o2Range: -1, o2Tune: 0.1, cutoff: 350, res: 0.4, fEnvAmt: 0, aA: 1.5, aS: 1, aR: 3, spread: 1, reverbMix: 0.5,
    }),
    patch('Airy Pad', 'Pads', {
      master: 0.89, o1Wave: 'sine', m1: 0.5, m2: 0, noise: 0.5, cutoff: 4000, res: 0.4, keyTrack: 1, fEnvAmt: 0, aA: 1.5, aS: 1, aR: 3, spread: 1, reverbMix: 0.6,
    }),
    patch('Analog Brass Pad', 'Pads', {
      master: 0.61, o2Tune: 0.1, cutoff: 800, res: 0.1, fEnvAmt: 1.5, fA: 0.6, fD: 2, fS: 0.6, aA: 0.6, aS: 1, aR: 1.5, spread: 1, reverbMix: 0.4,
    }),

    // ---- Brass & strings
    patch('Brass Section', 'Brass & strings', {
      master: 0.59,
      o1Wave: 'saw', o2Wave: 'saw', o2Tune: 0.12, o3Range: 0, o3Wave: 'saw', o3Tune: -0.1, m3: 0.5, cutoff: 650,
      res: 0.15, fEnvAmt: 2.6, fA: 0.09, fD: 0.6, fS: 0.5, fR: 0.4, aA: 0.05, aD: 0.4, aS: 0.85, aR: 0.35,
      spread: 0.7, reverbMix: 0.25,
    }),
    patch('Brass Stab', 'Brass & strings', {
      master: 0.78,
      o2Tune: 0.12, o3Range: 0, o3Wave: 'saw', o3Tune: -0.1, m3: 0.5, cutoff: 700, res: 0.2, fEnvAmt: 3,
      fA: 0.02, fD: 0.3, fS: 0.2, aA: 0.01, aD: 0.4, aS: 0.3, aR: 0.15, spread: 0.7, reverbMix: 0.25,
      delayMix: 0.1,
    }),
    patch('Velvet Strings', 'Brass & strings', {
      master: 0.52,
      o1Wave: 'pulse', pw: 0.32, o2Wave: 'saw', o2Tune: 0.14, o3Range: 1, o3Wave: 'saw', o3Tune: -0.12, m3: 0.3,
      cutoff: 2600, res: 0.1, fEnvAmt: 0.8, fA: 0.8, fD: 1.5, fS: 0.6, fR: 1.4, aA: 0.7, aD: 1, aS: 0.9,
      aR: 1.6, lfoRate: 0.35, modDepth: 1, modPitch: 0, modPw: 0.22, spread: 1, reverbMix: 0.4,
      reverbSize: 3.5, delayMix: 0.12,
    }),
    patch('Solo Cello', 'Brass & strings', {
      master: 0.77,
      voiceMode: 'legato', glide: 0.05, o2Tune: 0.06, cutoff: 1100, res: 0.1, fEnvAmt: 0.8, fA: 0.2,
      fD: 1, fS: 0.7, aA: 0.15, aS: 1, aR: 0.4, modDepth: 0.25, modPitch: 0.15, lfoRate: 5, reverbMix: 0.3,
    }),
    patch('Synth Brass', 'Brass & strings', {
      master: 0.69, o2Tune: 0.1, cutoff: 500, res: 0.2, fEnvAmt: 3, fA: 0.06, fD: 0.5, fS: 0.4, aA: 0.02, aD: 0.5, aS: 0.8, aR: 0.3, spread: 0.7, reverbMix: 0.25,
    }),
    patch('Soft Brass', 'Brass & strings', {
      master: 0.59, o2Wave: 'pulse', pw: 0.3, o2Tune: 0.08, cutoff: 400, res: 0.1, fEnvAmt: 2, fA: 0.25, fD: 1, fS: 0.5, aA: 0.15, aS: 1, aR: 0.5, spread: 0.7, reverbMix: 0.3,
    }),
    patch('Horn Section', 'Brass & strings', {
      master: 0.64, o2Tune: -0.08, o3Wave: 'saw', o3Range: -1, o3Tune: 0, m3: 0.4, cutoff: 700, res: 0.15, fEnvAmt: 2.5, fA: 0.1, fD: 0.6, fS: 0.5, aA: 0.07, aS: 0.9, aR: 0.3, spread: 0.7, reverbMix: 0.3,
    }),
    patch('Trombone', 'Brass & strings', {
      master: 0.75, voiceMode: 'legato', glide: 0.08, o1Range: -1, o2Range: -1, o2Tune: 0.05, cutoff: 450, res: 0.1, fEnvAmt: 2.5, fA: 0.08, fD: 0.4, fS: 0.5, aA: 0.05, aS: 1, aR: 0.2, modDepth: 0.1, modPitch: 0.15, reverbMix: 0.25,
    }),
    patch('French Horn', 'Brass & strings', {
      master: 0.61, o2Wave: 'tri', o2Tune: 0.05, cutoff: 500, res: 0.05, fEnvAmt: 1.5, fA: 0.15, fS: 0.6, aA: 0.12, aS: 1, aR: 0.4, reverbMix: 0.4,
    }),
    patch('Tuba', 'Brass & strings', {
      master: 0.71, voiceMode: 'mono', o1Range: -1, o2Wave: 'square', o2Range: -1, m2: 0.5, cutoff: 300, res: 0.1, fEnvAmt: 2, fA: 0.06, fD: 0.3, fS: 0.5, aA: 0.05, aS: 1, aR: 0.2, reverbMix: 0.2,
    }),
    patch('Brass Swell', 'Brass & strings', {
      master: 0.59, o2Tune: 0.1, cutoff: 300, res: 0.15, fEnvAmt: 3, fA: 1.2, fD: 1, fS: 0.8, aA: 0.8, aS: 1, aR: 0.6, spread: 0.8, reverbMix: 0.35,
    }),
    patch('Fanfare', 'Brass & strings', {
      master: 0.77, voiceMode: 'unison', unisonDetune: 12, o2Range: 1, o2Tune: 0, m2: 0.4, cutoff: 900, res: 0.2, fEnvAmt: 3, fA: 0.03, fD: 0.4, fS: 0.5, aA: 0.02, aS: 0.9, aR: 0.3, reverbMix: 0.35,
    }),
    patch('Funk Horns', 'Brass & strings', {
      master: 0.77, o2Tune: 0.1, cutoff: 800, res: 0.15, fEnvAmt: 3, fA: 0.01, fD: 0.2, fS: 0, aD: 0.35, aS: 0, aR: 0.15, spread: 0.7, reverbMix: 0.2,
    }),
    patch('Saxophone', 'Brass & strings', {
      master: 0.76, voiceMode: 'mono', o1Wave: 'pulse', pw: 0.3, o2Tune: 0.05, m2: 0.4, noise: 0.05, cutoff: 1000, res: 0.35, keyTrack: 0.6, fEnvAmt: 1.5, fA: 0.05, fS: 0.6, aA: 0.04, aS: 1, aR: 0.15, modDepth: 0.2, modPitch: 0.15, lfoRate: 5, reverbMix: 0.3,
    }),
    patch('String Ensemble', 'Brass & strings', {
      master: 0.59, o2Tune: 0.15, o3Wave: 'saw', o3Range: 1, o3Tune: -0.1, m3: 0.4, cutoff: 2400, res: 0.05, fEnvAmt: 0, aA: 0.4, aS: 1, aR: 1, modDepth: 0.4, modPitch: 0.08, lfoRate: 5.5, spread: 1, reverbMix: 0.4,
    }),
    patch('Violin', 'Brass & strings', {
      master: 0.79, voiceMode: 'legato', glide: 0.04, o2Tune: 0.05, cutoff: 2500, res: 0.2, keyTrack: 0.5, fEnvAmt: 0, aA: 0.12, aS: 1, aR: 0.3, modDepth: 0.3, modPitch: 0.2, lfoRate: 5.8, reverbMix: 0.35,
    }),
    patch('Viola', 'Brass & strings', {
      master: 0.69, voiceMode: 'legato', glide: 0.04, o2Wave: 'pulse', pw: 0.4, o2Tune: 0.06, cutoff: 1800, res: 0.15, fEnvAmt: 0, aA: 0.15, aS: 1, aR: 0.35, modDepth: 0.3, modPitch: 0.18, lfoRate: 5.5, reverbMix: 0.35,
    }),
    patch('Double Bass', 'Brass & strings', {
      master: 0.78, voiceMode: 'mono', o1Range: -1, o2Range: -1, o2Tune: 0.05, cutoff: 700, res: 0.1, fEnvAmt: 0, aA: 0.15, aS: 1, aR: 0.3, modDepth: 0.25, modPitch: 0.15, lfoRate: 5, reverbMix: 0.3,
    }),
    patch('Pizzicato', 'Brass & strings', {
      master: 0.91, o2Wave: 'tri', o2Range: 1, o2Tune: 0, m2: 0.3, cutoff: 1200, res: 0.2, fEnvAmt: 2, fD: 0.1, fS: 0, aA: 0.002, aD: 0.3, aS: 0, aR: 0.2, spread: 0.8, reverbMix: 0.35,
    }),
    patch('Tremolo Strings', 'Brass & strings', {
      master: 0.61, o2Tune: 0.15, cutoff: 1500, res: 0.1, fEnvAmt: 0, aA: 0.3, aS: 1, aR: 0.8, lfoSync: true, lfoDiv: '1/16', lfoShape: 'square', modDepth: 1, modPitch: 0, modFilter: 1.2, spread: 1, reverbMix: 0.4,
    }),
    patch('Slow Strings', 'Brass & strings', {
      master: 0.6, o2Tune: 0.12, cutoff: 1500, res: 0.05, fEnvAmt: 0, aA: 1.5, aS: 1, aR: 2, modDepth: 0.3, modPitch: 0.06, lfoRate: 5, spread: 1, reverbMix: 0.5,
    }),
    patch('Chamber Strings', 'Brass & strings', {
      master: 0.52, o2Wave: 'pulse', pw: 0.3, o2Tune: 0.1, cutoff: 3000, res: 0.05, fEnvAmt: 0, aA: 0.25, aS: 1, aR: 0.8, lfoRate: 4, modDepth: 1, modPitch: 0.08, modPw: 0.15, spread: 1, reverbMix: 0.4,
    }),
    patch('Cello Section', 'Brass & strings', {
      master: 0.59, o1Range: -1, o2Range: -1, o2Tune: 0.08, cutoff: 1000, res: 0.1, fEnvAmt: 0, aA: 0.25, aS: 1, aR: 0.6, modDepth: 0.3, modPitch: 0.12, lfoRate: 5, spread: 0.8, reverbMix: 0.4,
    }),
    patch('Orchestra Hit', 'Brass & strings', {
      master: 0.71, o2Range: 1, o2Tune: 0.1, o3Wave: 'saw', o3Range: -1, o3Tune: 0, m3: 0.6, noise: 0.1, cutoff: 2500, res: 0.1, fEnvAmt: 2, fD: 0.2, fS: 0, aA: 0.002, aD: 0.5, aS: 0, aR: 0.3, spread: 1, reverbMix: 0.5,
    }),
    patch('Bright Strings', 'Brass & strings', {
      master: 0.7, voiceMode: 'unison', unisonDetune: 15, spread: 1, o2Range: 1, o2Tune: 0, m2: 0.4, cutoff: 4500, res: 0.1, fEnvAmt: 0, aA: 0.3, aS: 1, aR: 1, reverbMix: 0.4,
    }),
    patch('Dark Strings', 'Brass & strings', {
      master: 0.57, o2Wave: 'square', o2Tune: 0.1, cutoff: 700, res: 0.1, fEnvAmt: 0, aA: 0.6, aS: 1, aR: 1.5, modDepth: 0.3, modPitch: 0.06, spread: 1, reverbMix: 0.45,
    }),
    patch('Fiddle', 'Brass & strings', {
      master: 0.79, voiceMode: 'legato', glide: 0.03, o2Wave: 'pulse', pw: 0.2, o2Tune: 0.07, cutoff: 3000, res: 0.3, fEnvAmt: 0, aA: 0.05, aS: 1, aR: 0.2, modDepth: 0.25, modPitch: 0.25, lfoRate: 6.5, reverbMix: 0.25,
    }),
    patch('Analog String Machine', 'Brass & strings', {
      master: 0.45, o1Wave: 'pulse', pw: 0.4, o2Wave: 'pulse', o2Tune: 0.1, cutoff: 2000, res: 0.05, fEnvAmt: 0, aA: 0.5, aS: 1, aR: 1.2, lfoRate: 0.6, modDepth: 1, modPitch: 0.04, modPw: 0.2, spread: 1, reverbMix: 0.4,
    }),
    patch('Staccato Strings', 'Brass & strings', {
      master: 0.79, o2Tune: 0.12, cutoff: 2200, res: 0.1, fEnvAmt: 0.5, fD: 0.2, fS: 0, aA: 0.02, aD: 0.2, aS: 0, aR: 0.15, spread: 1, reverbMix: 0.35,
    }),
    patch('Crescendo Strings', 'Brass & strings', {
      master: 0.7, o2Tune: 0.12, o3Wave: 'saw', o3Range: 1, o3Tune: -0.1, m3: 0.3, cutoff: 600, res: 0.1, fEnvAmt: 2, fA: 2.5, fD: 2, fS: 1, aA: 2.5, aS: 1, aR: 1.5, spread: 1, reverbMix: 0.45,
    }),

    // ---- Plucks
    patch('Bright Pluck', 'Plucks', {
      master: 0.78,
      o2Wave: 'square', o2Range: 1, m2: 0.4, cutoff: 400, res: 0.4, fEnvAmt: 5, fD: 0.18, fS: 0, aD: 0.5,
      aS: 0, aR: 0.3, delayMix: 0.3, delayTime: 0.375, delayFb: 0.4, reverbMix: 0.3,
    }),
    patch('Nylon Pluck', 'Plucks', {
      master: 0.76,
      o1Wave: 'tri', o2Wave: 'saw', m2: 0.3, cutoff: 700, res: 0.2, fEnvAmt: 3, fD: 0.25, fS: 0, keyTrack: 1,
      aD: 1, aS: 0, aR: 0.8, velFilter: 0.6, reverbMix: 0.25,
    }),
    patch('Kalimba', 'Plucks', {
      master: 0.65,
      o1Wave: 'sine', o2Wave: 'sine', o2Range: 2, o2Tune: 0, m2: 0.2, cutoff: 3000, res: 0, fEnvAmt: 0,
      aA: 0.001, aD: 0.6, aS: 0, aR: 0.4, velAmp: 0.7, reverbMix: 0.3, delayMix: 0.1,
    }),
    patch('Marimba', 'Plucks', {
      master: 0.83,
      o1Wave: 'sine', o2Wave: 'tri', o2Range: 1, o2Tune: 7, m2: 0.3, cutoff: 2000, res: 0.3, fEnvAmt: 1,
      fD: 0.1, fS: 0, aA: 0.001, aD: 0.35, aS: 0, aR: 0.3, velAmp: 0.7, reverbMix: 0.25,
    }),
    patch('Harp', 'Plucks', {
      master: 0.7, o1Wave: 'tri', o2Wave: 'sine', o2Range: 1, o2Tune: 0, m2: 0.3, cutoff: 1800, res: 0, fEnvAmt: 1.5, fD: 0.2, fS: 0, aA: 0.002, aD: 1.5, aS: 0, aR: 1, spread: 0.8, reverbMix: 0.4,
    }),
    patch('Koto', 'Plucks', {
      master: 0.78, o2Wave: 'pulse', pw: 0.2, o2Range: 1, o2Tune: 0, m2: 0.3, pitchEnv: 0.5, cutoff: 900, res: 0.4, fEnvAmt: 3, fD: 0.15, fS: 0, aA: 0.001, aD: 1, aS: 0, aR: 0.4, reverbMix: 0.3,
    }),
    patch('Banjo', 'Plucks', {
      master: 0.81, o1Wave: 'pulse', pw: 0.15, o2Range: 1, o2Tune: 0, m2: 0.4, cutoff: 2000, res: 0.3, fEnvAmt: 2, fD: 0.08, fS: 0, aA: 0.001, aD: 0.5, aS: 0, aR: 0.2, reverbMix: 0.15,
    }),
    patch('Guitar Pluck', 'Plucks', {
      master: 0.74, o2Wave: 'tri', o2Tune: 0.05, m2: 0.5, cutoff: 800, res: 0.2, keyTrack: 0.8, fEnvAmt: 3, fD: 0.3, fS: 0, aA: 0.001, aD: 1.5, aS: 0, aR: 0.4, velFilter: 0.6, reverbMix: 0.2,
    }),
    patch('Muted Guitar', 'Plucks', {
      master: 1, m2: 0, cutoff: 400, res: 0.2, fEnvAmt: 2, fD: 0.06, fS: 0, aA: 0.001, aD: 0.15, aS: 0, aR: 0.08, reverbMix: 0.1,
    }),
    patch('Twangy Pluck', 'Plucks', {
      master: 0.79, o2Tune: 0.2, cutoff: 700, res: 0.7, fEnvAmt: 2.5, fD: 0.4, fS: 0.1, aA: 0.001, aD: 1.5, aS: 0, aR: 0.4, reverbMix: 0.25,
    }),
    patch('Bell Pluck', 'Plucks', {
      master: 0.62, o1Wave: 'sine', o2Wave: 'sine', o2Range: 2, o2Tune: 7, m2: 0.3, cutoff: 5000, res: 0, fEnvAmt: 0, aA: 0.001, aD: 1, aS: 0, aR: 0.8, reverbMix: 0.35,
    }),
    patch('Glass Pluck', 'Plucks', {
      master: 0.88, o1Wave: 'tri', o1Range: 1, o2Wave: 'sine', o2Range: 2, o2Tune: 0, m2: 0.3, cutoff: 3000, res: 0.5, fEnvAmt: 1, fD: 0.1, fS: 0, aA: 0.001, aD: 0.7, aS: 0, aR: 0.5, reverbMix: 0.4, delayMix: 0.25,
    }),
    patch('Synth Pluck', 'Plucks', {
      master: 0.79, o2Tune: 0.1, cutoff: 500, res: 0.3, fEnvAmt: 4, fD: 0.2, fS: 0, aA: 0.001, aD: 0.4, aS: 0, aR: 0.3, delayMix: 0.25, reverbMix: 0.3,
    }),
    patch('Trance Pluck', 'Plucks', {
      master: 1, voiceMode: 'unison', unisonDetune: 15, spread: 0.9, cutoff: 600, res: 0.3, fEnvAmt: 4, fD: 0.15, fS: 0, aA: 0.001, aD: 0.35, aS: 0, aR: 0.3, delayMix: 0.3, reverbMix: 0.35,
    }),
    patch('Xylophone', 'Plucks', {
      master: 0.74, o1Wave: 'sine', o1Range: 1, o2Wave: 'tri', o2Range: 2, o2Tune: 0, m2: 0.25, cutoff: 5000, res: 0, fEnvAmt: 0, aA: 0.001, aD: 0.25, aS: 0, aR: 0.2, velAmp: 0.7, reverbMix: 0.25,
    }),
    patch('Vibraphone', 'Plucks', {
      master: 0.61, o1Wave: 'sine', o2Wave: 'sine', o2Range: 2, o2Tune: 0, m2: 0.15, cutoff: 4000, res: 0, fEnvAmt: 0, aA: 0.001, aD: 2, aS: 0, aR: 1.5, modDepth: 0.6, modPitch: 0.03, lfoRate: 5, velAmp: 0.7, spread: 0.8, reverbMix: 0.35,
    }),
    patch('Steel Drum', 'Plucks', {
      master: 0.63, o1Wave: 'sine', o2Wave: 'sine', o2Range: 1, o2Tune: 7, m2: 0.35, o3Wave: 'tri', o3Range: 2, o3Tune: 0, m3: 0.1, pitchEnv: 0.4, cutoff: 3000, res: 0, fEnvAmt: 0, fD: 0.05, fS: 0, aA: 0.001, aD: 0.8, aS: 0, aR: 0.5, reverbMix: 0.3,
    }),
    patch('Water Drop', 'Plucks', {
      master: 0.81, o1Wave: 'sine', o1Range: 1, m2: 0, pitchEnv: 12, cutoff: 5000, res: 0, fEnvAmt: 0, fD: 0.05, fS: 0, aA: 0.001, aD: 0.15, aS: 0, aR: 0.1, reverbMix: 0.4, delayMix: 0.3,
    }),
    patch('Rubber Band', 'Plucks', {
      master: 0.72, o1Wave: 'square', m2: 0, pitchEnv: -5, cutoff: 1200, res: 0.5, fEnvAmt: 0, fD: 0.15, fS: 0, aA: 0.001, aD: 0.4, aS: 0, aR: 0.2, reverbMix: 0.2,
    }),
    patch('Short Pluck', 'Plucks', {
      master: 0.87, o1Wave: 'pulse', pw: 0.3, m2: 0, cutoff: 1500, res: 0.2, fEnvAmt: 2, fD: 0.05, fS: 0, aA: 0.001, aD: 0.12, aS: 0, aR: 0.1, delayMix: 0.3, reverbMix: 0.25,
    }),
    patch('Mandolin', 'Plucks', {
      master: 0.79, o2Range: 1, o2Tune: 0.1, cutoff: 2000, res: 0.2, fEnvAmt: 2, fD: 0.1, fS: 0, aA: 0.001, aD: 0.6, aS: 0, aR: 0.3, reverbMix: 0.25,
    }),
    patch('Mbira', 'Plucks', {
      master: 0.7, o1Wave: 'tri', o1Range: 1, o2Wave: 'sine', o2Range: 2, o2Tune: 0.4, m2: 0.3, noise: 0.02, cutoff: 4000, res: 0, fEnvAmt: 0, aA: 0.001, aD: 0.9, aS: 0, aR: 0.5, reverbMix: 0.3,
    }),
    patch('Lute', 'Plucks', {
      master: 0.71, o1Wave: 'tri', o2Wave: 'pulse', pw: 0.2, m2: 0.3, cutoff: 1300, res: 0.1, fEnvAmt: 1.5, fD: 0.3, fS: 0, aA: 0.001, aD: 1.4, aS: 0, aR: 0.5, reverbMix: 0.3,
    }),
    patch('Clean Guitar', 'Plucks', {
      master: 0.74, o2Wave: 'square', o2Range: 1, o2Tune: 0, m2: 0.2, cutoff: 1500, res: 0.1, fEnvAmt: 1, fD: 0.5, fS: 0.2, aA: 0.001, aD: 2, aS: 0, aR: 0.4, reverbMix: 0.25, delayMix: 0.15,
    }),
    patch('Electric Guitar', 'Plucks', {
      master: 0.42, o2Tune: 0.1, drive: 1, cutoff: 2500, res: 0.2, fEnvAmt: 0.5, aA: 0.001, aD: 2, aS: 0.3, aR: 0.3, velAmp: 0.3, reverbMix: 0.2, delayMix: 0.15,
    }),
    patch('Funk Guitar', 'Plucks', {
      master: 0.85, o1Wave: 'pulse', pw: 0.2, m2: 0, cutoff: 800, res: 0.55, fEnvAmt: 3, fD: 0.1, fS: 0, aA: 0.001, aD: 0.2, aS: 0, aR: 0.08, reverbMix: 0.15,
    }),
    patch('Chime', 'Plucks', {
      master: 0.6, o1Wave: 'sine', o1Range: 2, o2Wave: 'sine', o2Range: 2, o2Tune: 3.9, m2: 0.4, cutoff: 8000, res: 0, fEnvAmt: 0, aA: 0.001, aD: 2, aS: 0, aR: 2, reverbMix: 0.5,
    }),
    patch('Blip', 'Plucks', {
      master: 0.82, o1Wave: 'square', o1Range: 1, m2: 0, pitchEnv: 7, cutoff: 4000, res: 0, fEnvAmt: 0, fD: 0.02, fS: 0, aA: 0.001, aD: 0.08, aS: 0, aR: 0.05, delayMix: 0.3, reverbMix: 0.2,
    }),
    patch('Dulcimer', 'Plucks', {
      master: 0.72, o2Range: 1, o2Tune: 0.15, o3Wave: 'tri', o3Range: 2, o3Tune: 0, m3: 0.2, cutoff: 2500, res: 0.2, fEnvAmt: 1.5, fD: 0.15, fS: 0, aA: 0.001, aD: 1.2, aS: 0, aR: 0.6, reverbMix: 0.3,
    }),
    patch('Echo Pluck', 'Plucks', {
      master: 0.79, o1Wave: 'tri', o2Wave: 'square', o2Range: 1, o2Tune: 0, cutoff: 1200, res: 0.2, fEnvAmt: 2, fD: 0.1, fS: 0, aA: 0.001, aD: 0.2, aS: 0, aR: 0.15, delayMix: 0.5, delayFb: 0.6, reverbMix: 0.3,
    }),

    // ---- Drums: one hit per key. The pitch follows the key, so play kicks and
    // toms low (around C2) and cymbals anywhere.
    patch('Analog Kick', 'Drums', {
      master: 0.69, voiceMode: 'mono', o1Wave: 'sine', m1: 1, m2: 0, pitchEnv: 24, cutoff: 900, res: 0.1, keyTrack: 0, fEnvAmt: 0, fA: 0.001, fD: 0.05, fS: 0, fR: 0.05, aA: 0.001, aD: 0.45, aS: 0, aR: 0.3, velAmp: 0.6, drive: 0.45, delayMix: 0, reverbMix: 0.04,
    }),
    patch('808 Boom', 'Drums', {
      master: 0.67, voiceMode: 'mono', o1Wave: 'sine', m1: 1, m2: 0, pitchEnv: 12, cutoff: 600, res: 0, keyTrack: 0, fEnvAmt: 0, fA: 0.001, fD: 0.08, fS: 0, fR: 0.08, aA: 0.001, aD: 1.8, aS: 0, aR: 1.2, velAmp: 0.5, drive: 0.35, delayMix: 0, reverbMix: 0.03,
    }),
    patch('Hard Kick', 'Drums', {
      master: 0.62, voiceMode: 'mono', o1Wave: 'sine', o2Wave: 'tri', m1: 1, m2: 0.4, o2Tune: 0, pitchEnv: 24, noise: 0.15, cutoff: 1500, res: 0.2, keyTrack: 0, fEnvAmt: 0, fA: 0.001, fD: 0.03, fS: 0, fR: 0.03, aA: 0.001, aD: 0.32, aS: 0, aR: 0.2, velAmp: 0.4, drive: 1, delayMix: 0, reverbMix: 0.04,
    }),
    patch('Analog Tom', 'Drums', {
      master: 0.81, o1Wave: 'sine', m1: 1, m2: 0, pitchEnv: 7, cutoff: 1200, res: 0.2, keyTrack: 0.5, fEnvAmt: 0, fA: 0.001, fD: 0.12, fS: 0, aA: 0.001, aD: 0.5, aS: 0, aR: 0.35, velAmp: 0.6, drive: 0.3, delayMix: 0, reverbMix: 0.15,
    }),
    patch('Syn Tom', 'Drums', {
      master: 1, o1Wave: 'tri', m1: 1, m2: 0, noise: 0.12, pitchEnv: 12, cutoff: 900, res: 0.45, keyTrack: 1, fEnvAmt: 2, fA: 0.001, fD: 0.3, fS: 0, aA: 0.001, aD: 0.6, aS: 0, aR: 0.4, velAmp: 0.6, delayMix: 0.12, reverbMix: 0.2,
    }),
    patch('Conga', 'Drums', {
      master: 1, o1Wave: 'sine', o1Range: 1, m1: 1, m2: 0, pitchEnv: 5, cutoff: 1800, res: 0.3, keyTrack: 1, fEnvAmt: 1, fA: 0.001, fD: 0.03, fS: 0, aA: 0.001, aD: 0.28, aS: 0, aR: 0.2, velAmp: 0.7, delayMix: 0, reverbMix: 0.15,
    }),
    patch('Snare', 'Drums', {
      master: 0.97, o1Wave: 'tri', m1: 0.7, m2: 0, noise: 1, pitchEnv: 7, cutoff: 3000, res: 0.2, keyTrack: 0.5, fEnvAmt: 2, fA: 0.001, fD: 0.08, fS: 0, aA: 0.001, aD: 0.18, aS: 0, aR: 0.15, velAmp: 0.7, drive: 0.9, delayMix: 0, reverbMix: 0.15,
    }),
    patch('Electro Snare', 'Drums', {
      master: 0.82, o1Wave: 'sine', o1Range: 1, m1: 0.8, m2: 0, noise: 0.8, pitchEnv: 24, cutoff: 5000, res: 0.35, keyTrack: 0.3, fEnvAmt: 1, fA: 0.001, fD: 0.03, fS: 0, aA: 0.001, aD: 0.22, aS: 0, aR: 0.15, velAmp: 0.6, drive: 0.7, delayMix: 0.1, reverbMix: 0.2,
    }),
    patch('Rimshot', 'Drums', {
      master: 1, o1Wave: 'tri', o1Range: 1, o2Wave: 'square', o2Range: 1, o2Tune: 5, m1: 0.8, m2: 0.4, noise: 0.3, pitchEnv: 5, cutoff: 2200, res: 0.6, keyTrack: 0.5, fEnvAmt: 1, fA: 0.001, fD: 0.02, fS: 0, aA: 0.001, aD: 0.05, aS: 0, aR: 0.04, velAmp: 0.6, drive: 0.6, delayMix: 0, reverbMix: 0.15,
    }),
    patch('Hand Clap', 'Drums', {
      master: 1, m1: 0, m2: 0, noise: 1, cutoff: 1900, res: 0.4, keyTrack: 0.3, fEnvAmt: 1.5, fA: 0.001, fD: 0.12, fS: 0, aA: 0.004, aD: 0.16, aS: 0, aR: 0.2, velAmp: 0.6, drive: 1, delayMix: 0, reverbMix: 0.3, reverbSize: 1.2,
    }),
    patch('Closed Hat', 'Drums', {
      master: 1, m1: 0, m2: 0, noise: 1, cutoff: 9000, res: 0.3, keyTrack: 0, fEnvAmt: 1, fD: 0.05, fS: 0, aA: 0.001, aD: 0.06, aS: 0, aR: 0.05, velAmp: 0.7, drive: 1, delayMix: 0, reverbMix: 0.05,
    }),
    patch('Open Hat', 'Drums', {
      master: 0.78, o1Wave: 'square', o1Range: 2, o2Wave: 'square', o2Range: 2, o2Tune: 6.3, m1: 0.2, m2: 0.2, noise: 1, cutoff: 11000, res: 0.3, keyTrack: 0, fEnvAmt: 0, aA: 0.001, aD: 0.45, aS: 0, aR: 0.35, velAmp: 0.6, drive: 0.9, delayMix: 0, reverbMix: 0.1,
    }),
    patch('Shaker', 'Drums', {
      master: 1, m1: 0, m2: 0, noise: 1, cutoff: 7000, res: 0.15, keyTrack: 0, fEnvAmt: 0, aA: 0.025, aD: 0.09, aS: 0, aR: 0.08, velAmp: 0.7, drive: 0.8, delayMix: 0, reverbMix: 0.1,
    }),
    patch('Crash', 'Drums', {
      master: 0.67, o1Wave: 'square', o1Range: 2, o2Wave: 'square', o2Range: 2, o2Tune: 6.3, o3Wave: 'square', o3Range: 2, o3Tune: 4.1, m1: 0.3, m2: 0.3, m3: 0.3, noise: 1, cutoff: 12000, res: 0.2, keyTrack: 0, fEnvAmt: 0, aA: 0.001, aD: 2, aS: 0, aR: 1.5, velAmp: 0.6, drive: 0.7, spread: 1, delayMix: 0, reverbMix: 0.3,
    }),
    patch('Ride', 'Drums', {
      master: 0.66, o1Wave: 'square', o1Range: 2, o2Wave: 'square', o2Range: 2, o2Tune: 6.3, o3Wave: 'pulse', o3Range: 2, o3Tune: 4.1, pw: 0.3, m1: 0.6, m2: 0.6, m3: 0.4, noise: 0.35, cutoff: 8000, res: 0.35, keyTrack: 0, fEnvAmt: 0, aA: 0.001, aD: 1.2, aS: 0, aR: 0.9, velAmp: 0.6, drive: 0.4, spread: 0.8, delayMix: 0, reverbMix: 0.25,
    }),
    patch('Cowbell', 'Drums', {
      master: 0.91, o1Wave: 'square', o1Range: 1, o2Wave: 'square', o2Range: 1, o2Tune: 6.8, m1: 0.7, m2: 0.7, cutoff: 2500, res: 0.4, keyTrack: 0.5, fEnvAmt: 1, fD: 0.05, fS: 0, aA: 0.001, aD: 0.3, aS: 0, aR: 0.2, velAmp: 0.6, delayMix: 0, reverbMix: 0.15,
    }),
    patch('Woodblock', 'Drums', {
      master: 1, o1Wave: 'sine', o1Range: 2, m1: 1, m2: 0, pitchEnv: 3, cutoff: 3000, res: 0.5, keyTrack: 0.5, fEnvAmt: 1, fA: 0.001, fD: 0.02, fS: 0, aA: 0.001, aD: 0.06, aS: 0, aR: 0.05, velAmp: 0.6, delayMix: 0, reverbMix: 0.2,
    }),
    patch('Rock Kick', 'Drums', {
      master: 0.67, voiceMode: 'mono', o1Wave: 'sine', m1: 1, m2: 0, noise: 0.2, pitchEnv: 18, cutoff: 1200, res: 0.2, keyTrack: 0, fEnvAmt: 0, fA: 0.001, fD: 0.04, fS: 0, fR: 0.04, aA: 0.001, aD: 0.35, aS: 0, aR: 0.2, velAmp: 0.6, drive: 0.6, delayMix: 0, reverbMix: 0.06,
    }),
    patch('Soft Kick', 'Drums', {
      master: 1, voiceMode: 'mono', o1Wave: 'sine', m1: 1, m2: 0, pitchEnv: 12, cutoff: 400, res: 0, keyTrack: 0, fEnvAmt: 0, fA: 0.001, fD: 0.06, fS: 0, fR: 0.06, aA: 0.001, aD: 0.3, aS: 0, aR: 0.2, velAmp: 0.6, drive: 0.1, delayMix: 0, reverbMix: 0.04,
    }),
    patch('Brush Snare', 'Drums', {
      master: 1, m1: 0, m2: 0, noise: 1, cutoff: 4000, res: 0.2, keyTrack: 0, fEnvAmt: 0, aA: 0.01, aD: 0.25, aS: 0, aR: 0.2, velAmp: 0.8, drive: 1, delayMix: 0, reverbMix: 0.15,
    }),
    patch('Piccolo Snare', 'Drums', {
      master: 1, o1Wave: 'tri', o1Range: 1, m1: 0.6, m2: 0, noise: 1, pitchEnv: 5, cutoff: 6000, res: 0.2, keyTrack: 0.3, fEnvAmt: 1, fA: 0.001, fD: 0.05, fS: 0, aA: 0.001, aD: 0.12, aS: 0, aR: 0.1, velAmp: 0.6, drive: 0.9, delayMix: 0, reverbMix: 0.15,
    }),
    patch('Big Room Clap', 'Drums', {
      master: 1, m1: 0, m2: 0, noise: 1, cutoff: 2600, res: 0.5, keyTrack: 0.3, fEnvAmt: 1.5, fA: 0.001, fD: 0.12, fS: 0, aA: 0.004, aD: 0.16, aS: 0, aR: 0.2, velAmp: 0.6, drive: 1, delayMix: 0, reverbMix: 0.5, reverbSize: 2,
    }),
    patch('Finger Snap', 'Drums', {
      master: 0.85, o1Wave: 'sine', o1Range: 2, m1: 1, m2: 0, noise: 0.8, pitchEnv: 12, cutoff: 3500, res: 0.5, keyTrack: 0, fEnvAmt: 0, fA: 0.001, fD: 0.02, fS: 0, aA: 0.001, aD: 0.08, aS: 0, aR: 0.04, velAmp: 0.6, drive: 1, delayMix: 0, reverbMix: 0.2,
    }),
    patch('Floor Tom', 'Drums', {
      master: 0.76, o1Wave: 'sine', m1: 1, m2: 0, pitchEnv: 5, cutoff: 1000, res: 0.15, keyTrack: 0.5, fEnvAmt: 0, fA: 0.001, fD: 0.15, fS: 0, aA: 0.001, aD: 0.8, aS: 0, aR: 0.5, velAmp: 0.6, drive: 0.3, delayMix: 0, reverbMix: 0.15,
    }),
    patch('Timbale', 'Drums', {
      master: 0.79, o1Wave: 'tri', o1Range: 1, m1: 1, m2: 0, noise: 0.2, pitchEnv: 3, cutoff: 3000, res: 0.4, keyTrack: 0.5, fEnvAmt: 0, fA: 0.001, fD: 0.05, fS: 0, aA: 0.001, aD: 0.3, aS: 0, aR: 0.2, velAmp: 0.6, drive: 0.6, delayMix: 0, reverbMix: 0.2,
    }),
    patch('Bongo', 'Drums', {
      master: 1, o1Wave: 'sine', o1Range: 1, m1: 1, m2: 0, pitchEnv: 4, cutoff: 3000, res: 0.2, keyTrack: 1, fEnvAmt: 0, fA: 0.001, fD: 0.02, fS: 0, aA: 0.001, aD: 0.15, aS: 0, aR: 0.1, velAmp: 0.7, delayMix: 0, reverbMix: 0.15,
    }),
    patch('Splash Cymbal', 'Drums', {
      master: 0.71, o1Wave: 'square', o1Range: 2, o2Wave: 'square', o2Range: 2, o2Tune: 6.3, o3Wave: 'square', o3Range: 2, o3Tune: 4.1, m1: 0.3, m2: 0.3, m3: 0.3, noise: 1, cutoff: 12000, res: 0.2, keyTrack: 0, fEnvAmt: 0, aA: 0.001, aD: 0.7, aS: 0, aR: 0.5, velAmp: 0.6, drive: 0.7, spread: 1, delayMix: 0, reverbMix: 0.25,
    }),
    patch('China Cymbal', 'Drums', {
      master: 0.55, o1Wave: 'square', o1Range: 2, o2Wave: 'square', o2Range: 2, o2Tune: 6.3, o3Wave: 'square', o3Range: 2, o3Tune: 4.1, m1: 0.5, m2: 0.5, m3: 0.5, noise: 0.8, cutoff: 7000, res: 0.5, keyTrack: 0, fEnvAmt: 0, aA: 0.001, aD: 1.2, aS: 0, aR: 0.8, velAmp: 0.6, drive: 1, spread: 1, delayMix: 0, reverbMix: 0.25,
    }),
    patch('Triangle', 'Drums', {
      master: 0.75, o1Wave: 'sine', o1Range: 2, o2Wave: 'square', o2Range: 2, o2Tune: 6.1, m1: 1, m2: 0.1, cutoff: 8000, res: 0, keyTrack: 0, fEnvAmt: 0, aA: 0.001, aD: 1.5, aS: 0, aR: 1, velAmp: 0.6, delayMix: 0, reverbMix: 0.3,
    }),
    patch('Clave', 'Drums', {
      master: 1, o1Wave: 'sine', o1Range: 2, m1: 1, m2: 0, cutoff: 18000, res: 0, keyTrack: 0, fEnvAmt: 0, aA: 0.001, aD: 0.07, aS: 0, aR: 0.03, velAmp: 0.6, drive: 0.8, delayMix: 0, reverbMix: 0.2,
    }),

    // ---- Drum Kits: a different drum on every key
    ...KITS.map((k) =>
      patch(k.name, 'Drum Kits', Object.assign({ kit: true, kitId: k.name, delayMix: 0, reverbMix: k.room === undefined ? KIT_BASE.room : k.room, reverbSize: 1.6 }, k.patch || {}))
    ),

    // ---- Effects
    patch('Laser Zap', 'Effects', {
      master: 0.88,
      voiceMode: 'mono', m1: 1, m2: 0, cutoff: 200, res: 0.95, fEnvAmt: 6, fA: 0.001, fD: 0.25, fS: 0,
      aD: 0.3, aS: 0, aR: 0.2, drive: 0.5, delayMix: 0.25, reverbMix: 0.2,
    }),
    patch('Siren', 'Effects', {
      master: 0.77,
      voiceMode: 'mono', o1Wave: 'square', m2: 0, cutoff: 3000, res: 0.2, fEnvAmt: 0, aS: 1, aR: 0.3,
      modDepth: 1, modPitch: 7, lfoRate: 0.5, lfoShape: 'tri', reverbMix: 0.3,
    }),
    patch('UFO', 'Effects', {
      master: 0.95,
      voiceMode: 'mono', o1Wave: 'sine', m2: 0, o3KeyOff: true, o3Range: 1, modSource: 'osc3', modDepth: 1,
      modPitch: 3, cutoff: 3000, res: 0.3, fEnvAmt: 0, aA: 0.2, aS: 1, aR: 1, delayMix: 0.4, delayFb: 0.55,
      reverbMix: 0.4,
    }),
    patch('Wind', 'Effects', {
      master: 0.78,
      m1: 0, m2: 0, noise: 1, cutoff: 600, res: 0.7, keyTrack: 0.6, fEnvAmt: 0, aA: 1, aS: 1, aR: 2,
      modSource: 'noise', modDepth: 1, modPitch: 0, modFilter: 2, reverbMix: 0.5, reverbSize: 4,
    }),
    patch('Ocean Waves', 'Effects', {
      master: 1,
      m1: 0, m2: 0, noise: 1, cutoff: 900, res: 0.2, keyTrack: 0, fEnvAmt: 0, aA: 1.5, aS: 1, aR: 3,
      lfoShape: 'tri', lfoRate: 0.15, modDepth: 1, modPitch: 0, modFilter: 2.2, drive: 0.6, spread: 1, reverbMix: 0.5,
      reverbSize: 5,
    }),
    patch('Thunder', 'Effects', {
      master: 1, m1: 0, m2: 0, noise: 1, cutoff: 250, res: 0.4, keyTrack: 0, fEnvAmt: 2, fA: 0.001, fD: 0.6, fS: 0, drive: 1, aA: 0.05, aD: 3, aS: 0, aR: 2, reverbMix: 0.5, reverbSize: 5, delayMix: 0,
    }),
    patch('Rain', 'Effects', {
      master: 0.69, m1: 0, m2: 0, noise: 1, cutoff: 6000, res: 0.1, keyTrack: 0, fEnvAmt: 0, aA: 0.5, aS: 1, aR: 1.5, modSource: 'noise', modDepth: 1, modPitch: 0, modFilter: 1.5, spread: 1, reverbMix: 0.4, delayMix: 0,
    }),
    patch('Heartbeat', 'Effects', {
      master: 0.89, arpOn: true, arpMode: 'up', arpDiv: '1/8', arpOct: 1, arpGate: 0.3, bpm: 70, voiceMode: 'mono', o1Wave: 'sine', o1Range: -1, m2: 0, pitchEnv: 12, cutoff: 500, res: 0, fEnvAmt: 0, fA: 0.001, fD: 0.05, fS: 0, aA: 0.001, aD: 0.2, aS: 0, aR: 0.1, drive: 0.4, delayMix: 0, reverbMix: 0.15,
    }),
    patch('Bubbles', 'Effects', {
      master: 0.59, o1Wave: 'sine', o1Range: 1, m2: 0, cutoff: 5000, res: 0, fEnvAmt: 0, aS: 1, aR: 0.3, lfoShape: 'sh', lfoRate: 10, modDepth: 1, modPitch: 7, delayMix: 0.3, reverbMix: 0.3,
    }),
    patch('Alarm', 'Effects', {
      master: 0.57, o1Wave: 'square', m2: 0, cutoff: 3000, res: 0.2, fEnvAmt: 0, aS: 1, aR: 0.1, lfoShape: 'square', lfoRate: 3, modDepth: 1, modPitch: 5, reverbMix: 0.2,
    }),
    patch('Radio Static', 'Effects', {
      master: 0.74, m1: 0, m2: 0, noise: 1, cutoff: 3000, res: 0.6, keyTrack: 0, fEnvAmt: 0, aS: 1, aR: 0.2, modSource: 'noise', modDepth: 1, modPitch: 0, modFilter: 3, reverbMix: 0.1, delayMix: 0,
    }),
    patch('Robot Voice', 'Effects', {
      master: 0.62, o1Wave: 'pulse', pw: 0.1, o2Tune: 0, cutoff: 800, res: 0.8, fEnvAmt: 0, aS: 1, aR: 0.2, lfoShape: 'sh', lfoRate: 12, modDepth: 1, modPitch: 0, modFilter: 1.5, reverbMix: 0.2,
    }),
    patch('Engine Rumble', 'Effects', {
      master: 0.38, o1Range: -2, o2Wave: 'square', o2Range: -2, o2Tune: 0.3, noise: 0.3, cutoff: 200, res: 0.4, fEnvAmt: 0, drive: 0.8, aS: 1, aR: 0.4, lfoRate: 8, modDepth: 1, modPitch: 0, modFilter: 0.5, reverbMix: 0.1, delayMix: 0,
    }),
    patch('Spaceship Hum', 'Effects', {
      master: 0.65, voiceMode: 'unison', unisonDetune: 30, spread: 1, o1Range: -1, o2Range: -1, o2Tune: 0.07, cutoff: 300, res: 0.7, fEnvAmt: 0, aA: 1, aS: 1, aR: 1.5, lfoRate: 0.2, modDepth: 1, modPitch: 0, modFilter: 1, reverbMix: 0.5,
    }),
    patch('Teleport', 'Effects', {
      master: 0.7, o1Wave: 'sine', o1Range: 1, m2: 0, pitchEnv: 24, cutoff: 600, res: 0.6, fEnvAmt: 3, fA: 0.5, fD: 0.2, fS: 0, aA: 0.3, aD: 0.6, aS: 0, aR: 0.4, reverbMix: 0.5, delayMix: 0.3,
    }),
    patch('Power Down', 'Effects', {
      master: 0.6, o1Wave: 'square', m2: 0, pitchEnv: -24, cutoff: 3000, res: 0.2, fEnvAmt: 0, fA: 1.5, fS: 1, aA: 0.001, aD: 2, aS: 0, aR: 0.3, reverbMix: 0.2,
    }),
    patch('Power Up', 'Effects', {
      master: 0.59, o1Wave: 'square', m2: 0, pitchEnv: 24, cutoff: 3000, res: 0.2, fEnvAmt: 0, fA: 1.5, fS: 1, aA: 0.001, aS: 1, aR: 0.3, reverbMix: 0.2,
    }),
    patch('Game Coin', 'Effects', {
      master: 0.57, o1Wave: 'square', o1Range: 1, m2: 0, pitchEnv: -5, cutoff: 6000, res: 0, fEnvAmt: 0, fA: 0.001, fD: 0.06, fS: 0, aA: 0.001, aD: 0.4, aS: 0, aR: 0.2, velAmp: 0, reverbMix: 0.1, delayMix: 0,
    }),
    patch('Game Jump', 'Effects', {
      master: 0.62, o1Wave: 'square', m2: 0, pitchEnv: -12, cutoff: 6000, res: 0, fEnvAmt: 0, fA: 0.001, fD: 0.15, fS: 0, aA: 0.001, aD: 0.25, aS: 0, aR: 0.1, velAmp: 0, reverbMix: 0.1, delayMix: 0,
    }),
    patch('Explosion', 'Effects', {
      master: 0.9, m1: 0, m2: 0, noise: 1, drive: 1, cutoff: 100, res: 0.2, keyTrack: 0, fEnvAmt: 5, fA: 0.001, fD: 0.8, fS: 0, aA: 0.001, aD: 2, aS: 0, aR: 1.5, reverbMix: 0.4, delayMix: 0,
    }),
    patch('Wind Chimes', 'Effects', {
      master: 0.63, arpOn: true, arpMode: 'random', arpDiv: '1/16', arpOct: 2, arpGate: 0.2, o1Wave: 'sine', o1Range: 2, o2Wave: 'sine', o2Range: 2, o2Tune: 3.9, m2: 0.4, cutoff: 8000, res: 0, fEnvAmt: 0, aA: 0.001, aD: 1.5, aS: 0, aR: 1.5, spread: 1, reverbMix: 0.55,
    }),
    patch('Birds', 'Effects', {
      master: 0.6, o1Wave: 'sine', o1Range: 2, m2: 0, cutoff: 8000, res: 0, fEnvAmt: 0, aS: 1, aR: 0.3, lfoShape: 'sh', lfoRate: 12, modDepth: 1, modPitch: 5, reverbMix: 0.4, delayMix: 0.2,
    }),
    patch('Crickets', 'Effects', {
      master: 0.54, o1Wave: 'sine', o1Range: 2, o2Wave: 'sine', o2Range: 2, o2Tune: 0.3, cutoff: 100, res: 0, keyTrack: 0, fEnvAmt: 0, aS: 1, aR: 0.3, lfoShape: 'square', lfoRate: 25, modDepth: 1, modPitch: 0, modFilter: 4, reverbMix: 0.3,
    }),
    patch('Underwater', 'Effects', {
      master: 0.62, o1Wave: 'tri', o1Range: -1, noise: 0.3, cutoff: 300, res: 0.5, fEnvAmt: 0, aA: 1, aS: 1, aR: 1.5, modSource: 'noise', modDepth: 1, modPitch: 0, modFilter: 2, reverbMix: 0.5,
    }),
    patch('Ghost', 'Effects', {
      master: 0.65, voiceMode: 'legato', glide: 0.5, o1Wave: 'sine', o2Wave: 'sine', o2Tune: 0.3, cutoff: 3000, res: 0, fEnvAmt: 0, aA: 0.8, aS: 1, aR: 1.5, modDepth: 0.6, modPitch: 0.8, lfoRate: 4, reverbMix: 0.6, delayMix: 0.3,
    }),
    patch('Laser Gun', 'Effects', {
      master: 1, m2: 0, pitchEnv: 24, cutoff: 2000, res: 0.5, fEnvAmt: 0, fA: 0.001, fD: 0.12, fS: 0, aA: 0.001, aD: 0.2, aS: 0, aR: 0.1, delayMix: 0.2, reverbMix: 0.2,
    }),
    patch('Phone Tone', 'Effects', {
      master: 0.48, o1Wave: 'sine', o2Wave: 'sine', o2Tune: -4, m2: 0.8, cutoff: 4000, res: 0, fEnvAmt: 0, aS: 1, aR: 0.05, velAmp: 0, reverbMix: 0, delayMix: 0,
    }),
    patch('Static Swell', 'Effects', {
      master: 0.87, m1: 0, m2: 0, noise: 1, cutoff: 300, res: 0.3, keyTrack: 0, fEnvAmt: 5, fA: 2, fS: 1, aA: 2, aS: 1, aR: 1, reverbMix: 0.4,
    }),
    patch('Glitch', 'Effects', {
      master: 0.55, o1Wave: 'square', cutoff: 2000, res: 0.4, fEnvAmt: 0, aS: 1, aR: 0.1, lfoShape: 'sh', lfoRate: 20, modDepth: 1, modPitch: 12, modFilter: 2, delayMix: 0.3, delayDiv: '1/16',
    }),
    patch('Train Horn', 'Effects', {
      master: 0.83, m1: 0.5, m2: 0.4, o2Tune: 3, o3Wave: 'saw', o3Range: 0, o3Tune: 7, m3: 0.4, cutoff: 1200, res: 0.2, fEnvAmt: 0, drive: 0.15, aA: 0.05, aS: 1, aR: 0.4, reverbMix: 0.3,
    }),

    // ---- Experimental
    patch('Filter Whistle', 'Experimental', {
      master: 1,
      voiceMode: 'legato', glide: 0.18, m1: 0, m2: 0, m3: 0, noise: 0.02, cutoff: 261.6, res: 1, keyTrack: 1,
      fEnvAmt: 0, aA: 0.04, aS: 1, aR: 0.5, modDepth: 0.15, modPitch: 0, modFilter: 0.08, lfoRate: 5.5,
      reverbMix: 0.3, delayMix: 0.2,
    }),
    patch('Wobble', 'Experimental', {
      master: 0.54,
      voiceMode: 'mono', o1Range: -1, o2Range: -1, o2Tune: 0.2, o2Wave: 'square', cutoff: 170, res: 0.62,
      fEnvAmt: 0.5, aS: 1, drive: 0.6, lfoRate: 3, lfoShape: 'tri', modDepth: 1, modPitch: 0, modFilter: 3,
      reverbMix: 0.1,
    }),
    patch('Sample & Hold Computer', 'Experimental', {
      master: 0.62,
      o1Wave: 'pulse', pw: 0.2, o2Wave: 'saw', o2Tune: 7, cutoff: 650, res: 0.72, fEnvAmt: 1, fD: 0.15,
      fS: 0.2, aS: 1, aR: 0.6, lfoRate: 8, lfoShape: 'sh', modDepth: 1, modPitch: 0, modFilter: 2.4,
      delayMix: 0.3, delayDiv: '1/8', delayFb: 0.5, reverbMix: 0.25,
    }),
    patch('Metal Bells', 'Experimental', {
      master: 0.81,
      o1Wave: 'sine', o2Wave: 'tri', o2Tune: 6.9, o3Range: 1, o3Wave: 'square', o3Tune: 3.2,
      modSource: 'osc3', modDepth: 1, modPitch: 0, modFilter: 1.6, m2: 0.5, cutoff: 1400, res: 0.5,
      fEnvAmt: 2.5, fD: 0.9, fS: 0, aA: 0.002, aD: 1.8, aS: 0, aR: 1.5, reverbMix: 0.4, delayMix: 0.18,
    }),
    patch('Resonance Sweep', 'Experimental', {
      master: 0.7, cutoff: 500, res: 0.9, fEnvAmt: 0, aS: 1, aR: 0.3, lfoRate: 0.25, modDepth: 1, modPitch: 0, modFilter: 3, reverbMix: 0.3,
    }),
    patch('Audio-Rate Filter', 'Experimental', {
      master: 0.66, o3Range: 0, o3Tune: 7, m3: 0, modSource: 'osc3', modDepth: 1, modPitch: 0, modFilter: 2.5, cutoff: 500, res: 0.6, fEnvAmt: 0, aS: 1, aR: 0.3, reverbMix: 0.2,
    }),
    patch('Breath Whistle', 'Experimental', {
      master: 1, m1: 0, m2: 0, noise: 0.8, cutoff: 261.6, res: 0.97, keyTrack: 1, fEnvAmt: 0, aA: 0.1, aS: 1, aR: 0.4, reverbMix: 0.35,
    }),
    patch('Ringing Bass', 'Experimental', {
      master: 0.8, voiceMode: 'legato', glide: 0.1, o1Wave: 'sine', o1Range: -1, m1: 0.25, m2: 0, noise: 0.06, cutoff: 130.8, res: 1, keyTrack: 1, fEnvAmt: 0, aA: 0.02, aS: 1, aR: 0.3, reverbMix: 0.1,
    }),
    patch('Chaos Engine', 'Experimental', {
      master: 0.56, o1Wave: 'pulse', cutoff: 800, res: 0.7, fEnvAmt: 0, aS: 1, aR: 0.3, lfoShape: 'sh', lfoRate: 6, modDepth: 1, modPitch: 2, modFilter: 3, modPw: 0.4, delayMix: 0.3,
    }),
    patch('Drive Feedback', 'Experimental', {
      master: 0.4, o2Tune: 0.3, drive: 1, cutoff: 600, res: 0.95, fEnvAmt: 1, aS: 1, aR: 0.3,
    }),
    patch('Reverse Contour', 'Experimental', {
      master: 0.66, cutoff: 4000, res: 0.5, fEnvAmt: -4, fA: 0.5, fD: 1, fS: 0.5, aS: 1, aR: 0.3, reverbMix: 0.25,
    }),
    patch('Backwards Pad', 'Experimental', {
      master: 0.69, o2Tune: 0.1, cutoff: 400, res: 0.4, fEnvAmt: 3, fA: 1.2, fD: 0.05, fS: 0, aA: 1.2, aD: 0.05, aS: 0, aR: 0.05, reverbMix: 0.4,
    }),
    patch('Microtonal Drift', 'Experimental', {
      master: 0.57, o2Tune: 0.45, o3Wave: 'saw', o3Range: 0, o3Tune: -0.4, m3: 0.6, cutoff: 1500, res: 0.2, fEnvAmt: 0, aS: 1, aR: 0.4, modSource: 'noise', modDepth: 1, modPitch: 0.5, reverbMix: 0.3,
    }),
    patch('Detune Cloud', 'Experimental', {
      master: 0.76, voiceMode: 'unison', unisonDetune: 50, spread: 1, o2Range: 1, o2Tune: 7, m2: 0.4, cutoff: 2500, res: 0.2, fEnvAmt: 0, aA: 0.3, aS: 1, aR: 1, reverbMix: 0.5,
    }),
    patch('Tritone Machine', 'Experimental', {
      master: 0.67, o2Tune: 6, o3Wave: 'saw', o3Range: 1, o3Tune: -6, m3: 0.5, cutoff: 1200, res: 0.5, fEnvAmt: 1, aS: 1, aR: 0.3, delayMix: 0.2,
    }),
    patch('Spectral Sines', 'Experimental', {
      master: 0.53, o1Wave: 'sine', o2Wave: 'sine', o2Range: 1, o2Tune: 7, o3Wave: 'sine', o3Range: 2, o3Tune: 4, m3: 0.5, cutoff: 6000, res: 0, fEnvAmt: 0, aA: 0.2, aS: 1, aR: 1, spread: 1, reverbMix: 0.5,
    }),
    patch('Gated Noise', 'Experimental', {
      master: 0.51, m1: 0, m2: 0, noise: 1, cutoff: 2000, res: 0.5, keyTrack: 0.5, fEnvAmt: 0, aS: 1, aR: 0.2, lfoSync: true, lfoDiv: '1/16', lfoShape: 'square', modDepth: 1, modPitch: 0, modFilter: 4,
    }),
    patch('Audio PWM', 'Experimental', {
      master: 0.6, o1Wave: 'pulse', o2Wave: 'pulse', o2Tune: 7, m2: 0.5, o3Range: 1, o3Tune: 0.1, m3: 0, modSource: 'osc3', modDepth: 1, modPitch: 0, modPw: 0.4, cutoff: 3000, res: 0.2, fEnvAmt: 0, aS: 1, aR: 0.3,
    }),
    patch('Slow Morph', 'Experimental', {
      master: 0.67, o1Wave: 'pulse', m2: 0, cutoff: 1500, res: 0.4, fEnvAmt: 0, aS: 1, aR: 0.5, lfoRate: 0.1, modDepth: 1, modPitch: 0, modPw: 0.44, modFilter: 1.5, reverbMix: 0.3,
    }),
    patch('Sub Harmonic', 'Experimental', {
      master: 0.57, o2Wave: 'square', o2Range: -2, o2Tune: 0, m2: 0.6, o3Wave: 'sine', o3Range: -1, o3Tune: 0, m3: 0.6, cutoff: 900, res: 0.3, fEnvAmt: 0.5, aS: 1, aR: 0.3,
    }),
    patch('Stepped Filter', 'Experimental', {
      master: 0.57, cutoff: 400, res: 0.75, fEnvAmt: 0, aS: 1, aR: 0.3, lfoSync: true, lfoDiv: '1/16', lfoShape: 'sh', modDepth: 1, modPitch: 0, modFilter: 2.5, delayMix: 0.3,
    }),
    patch('Random Melody', 'Experimental', {
      master: 0.71, o1Wave: 'tri', o2Wave: 'sine', o2Range: 1, o2Tune: 0, m2: 0.3, cutoff: 3000, res: 0.2, fEnvAmt: 0, aS: 1, aR: 0.3, lfoSync: true, lfoDiv: '1/8', lfoShape: 'sh', modDepth: 1, modPitch: 12, delayMix: 0.3, reverbMix: 0.3,
    }),
    patch('Tape Wobble', 'Experimental', {
      master: 0.56, o1Wave: 'tri', m2: 0.4, drive: 0.3, cutoff: 1800, res: 0.1, fEnvAmt: 0, aS: 1, aR: 0.4, modSource: 'noise', modDepth: 1, modPitch: 0.4, reverbMix: 0.25,
    }),
    patch('Talking Filter', 'Experimental', {
      master: 0.61, m2: 0.4, cutoff: 800, res: 0.85, keyTrack: 0, fEnvAmt: 0, aS: 1, aR: 0.3, lfoRate: 0.5, modDepth: 1, modPitch: 0, modFilter: 1,
    }),
    patch('Bent Bell', 'Experimental', {
      master: 0.59, o1Wave: 'sine', o2Wave: 'sine', o2Range: 1, o2Tune: 6.5, m2: 0.5, pitchEnv: -2, cutoff: 5000, res: 0, fEnvAmt: 0, fD: 0.4, fS: 0, aA: 0.001, aD: 2, aS: 0, aR: 1.5, reverbMix: 0.4,
    }),
    patch('Feedback Scream', 'Experimental', {
      master: 0.67, m1: 0.3, m2: 0, drive: 1, cutoff: 1500, res: 1, keyTrack: 1, fEnvAmt: 0, aS: 1, aR: 0.3, delayMix: 0.2,
    }),
    patch('Noise Bell', 'Experimental', {
      master: 1, m1: 0, m2: 0, noise: 0.8, cutoff: 523.3, res: 0.97, keyTrack: 1, fEnvAmt: 0, aA: 0.001, aD: 1.5, aS: 0, aR: 1, reverbMix: 0.4,
    }),
    patch('Octave Jumper', 'Experimental', {
      master: 0.74, m2: 0, cutoff: 2000, res: 0.3, fEnvAmt: 0, aS: 1, aR: 0.2, lfoSync: true, lfoDiv: '1/8', lfoShape: 'square', modDepth: 1, modPitch: 12, delayMix: 0.2,
    }),
    patch('Saw Swarm', 'Experimental', {
      master: 0.69, voiceMode: 'unison', unisonDetune: 40, spread: 1, o2Wave: 'ramp', o2Tune: 0.3, cutoff: 2500, res: 0.2, fEnvAmt: 0, aS: 1, aR: 0.4, lfoRate: 7, modDepth: 1, modPitch: 0.3, reverbMix: 0.3,
    }),
    patch('Granular Fizz', 'Experimental', {
      master: 0.64, o1Wave: 'pulse', pw: 0.05, m2: 0, noise: 0.4, cutoff: 2000, res: 0.5, fEnvAmt: 0, aS: 1, aR: 0.3, lfoShape: 'sh', lfoRate: 30, modDepth: 1, modPitch: 0, modPw: 0.4, modFilter: 2, reverbMix: 0.3,
    }),

    // ---- Sequenced: hold a chord and the arpeggiator plays it
    patch('Pulse Arp', 'Sequenced', {
      master: 1,
      arpOn: true, arpMode: 'updown', arpDiv: '1/16', arpOct: 2, arpGate: 0.45, bpm: 118, o1Wave: 'pulse',
      pw: 0.25, o2Wave: 'saw', o2Tune: 7, m2: 0.35, cutoff: 480, res: 0.55, fEnvAmt: 3.4,
      fD: 0.16, fS: 0, fR: 0.12, aD: 0.22, aS: 0.2, aR: 0.12, delayTime: 0.381, delayFb: 0.45, delayMix: 0.3,
      reverbMix: 0.22,
    }),
    patch('Latched Sequence', 'Sequenced', {
      master: 0.65,
      arpOn: true, latch: true, arpMode: 'order', arpDiv: '1/8', arpOct: 1, arpGate: 0.7, bpm: 100,
      voiceMode: 'mono', glide: 0.03, o1Range: -1, o2Range: -1, o2Tune: 0.1, cutoff: 380, res: 0.6,
      fEnvAmt: 3, fD: 0.22, fS: 0.1, aS: 1, aR: 0.1, drive: 0.5, delayMix: 0.25, delayTime: 0.45,
    }),
    patch('Berlin School', 'Sequenced', {
      master: 0.75,
      arpOn: true, latch: true, arpMode: 'order', arpDiv: '1/16', arpOct: 2, arpGate: 0.5, bpm: 112,
      voiceMode: 'mono', o1Range: -1, o2Range: -1, o2Wave: 'square', o2Tune: 0.08, cutoff: 350, res: 0.65,
      fEnvAmt: 3.2, fD: 0.14, fS: 0, aS: 1, aR: 0.08, modDepth: 1, modPitch: 0, modFilter: 0.7, lfoRate: 0.07,
      delayMix: 0.35, delayTime: 0.4, delayFb: 0.5, reverbMix: 0.3,
    }),
    patch('Chip Arp', 'Sequenced', {
      master: 1,
      arpOn: true, arpMode: 'up', arpDiv: '1/32', arpOct: 2, arpGate: 0.8, bpm: 130, voiceMode: 'mono',
      o1Wave: 'pulse', pw: 0.125, m2: 0, cutoff: 18000, res: 0, fEnvAmt: 0, aD: 0.1, aS: 0.5, aR: 0.03,
      velAmp: 0, delayMix: 0, reverbMix: 0.08,
    }),
    patch('Trance Gate', 'Sequenced', {
      master: 0.83,
      arpOn: true, arpMode: 'updown', arpDiv: '1/16', arpOct: 1, arpGate: 0.3, bpm: 138,
      voiceMode: 'unison', unisonDetune: 20, spread: 0.9, o2Tune: 7, m2: 0.4, cutoff: 2600, res: 0.25,
      fEnvAmt: 1, fD: 0.2, fS: 0.5, aS: 1, aR: 0.05, delayMix: 0.3, delayTime: 0.326, delayFb: 0.4,
      reverbMix: 0.3,
    }),
    patch('Random Bleeps', 'Sequenced', {
      master: 1,
      arpOn: true, arpMode: 'random', arpDiv: '1/16', arpOct: 3, arpGate: 0.35, bpm: 120, o1Wave: 'square',
      m2: 0, cutoff: 900, res: 0.7, fEnvAmt: 2, fD: 0.1, fS: 0, aD: 0.15, aS: 0, aR: 0.1, lfoShape: 'sh',
      lfoRate: 8, modDepth: 1, modPitch: 0, modFilter: 1.5, delayMix: 0.35, delayDiv: '1/8', delayFb: 0.5,
      reverbMix: 0.3,
    }),
    patch('Octave Bass Arp', 'Sequenced', {
      master: 0.93, arpOn: true, arpMode: 'up', arpDiv: '1/16', arpOct: 2, arpGate: 0.4, bpm: 124, voiceMode: 'mono', o1Range: -1, m2: 0.4, o2Range: -1, cutoff: 300, res: 0.5, fEnvAmt: 3, fD: 0.12, fS: 0, aS: 1, aR: 0.05, delayMix: 0.1,
    }),
    patch('Disco Octaves', 'Sequenced', {
      master: 0.87, arpOn: true, arpMode: 'up', arpDiv: '1/8', arpOct: 2, arpGate: 0.5, bpm: 118, o2Wave: 'square', o2Tune: 0.05, cutoff: 600, res: 0.3, fEnvAmt: 2.5, fD: 0.1, fS: 0.1, aD: 0.2, aS: 0.3, aR: 0.08, delayMix: 0.15,
    }),
    patch('Synthwave Arp', 'Sequenced', {
      master: 0.8, arpOn: true, arpMode: 'updown', arpDiv: '1/16', arpOct: 2, arpGate: 0.6, bpm: 100, o2Wave: 'pulse', o2Tune: 0.08, cutoff: 1200, res: 0.3, fEnvAmt: 2, fD: 0.2, fS: 0.2, aS: 0.6, aR: 0.1, delayMix: 0.35, delayFb: 0.5, reverbMix: 0.35,
    }),
    patch('Ambient Arp', 'Sequenced', {
      master: 0.88, arpOn: true, arpMode: 'random', arpDiv: '1/8', arpOct: 3, arpGate: 0.9, bpm: 80, o1Wave: 'tri', o2Wave: 'sine', o2Range: 1, o2Tune: 0, m2: 0.3, cutoff: 3000, res: 0, fEnvAmt: 0, aA: 0.01, aD: 1, aS: 0, aR: 1, spread: 1, reverbMix: 0.6, delayMix: 0.4,
    }),
    patch('Techno Stab Seq', 'Sequenced', {
      master: 1, arpOn: true, arpMode: 'order', arpDiv: '1/8', arpOct: 1, arpGate: 0.3, bpm: 130, o2Tune: 7, o3Wave: 'saw', o3Range: 0, o3Tune: 3, m3: 0.5, cutoff: 800, res: 0.4, fEnvAmt: 3, fD: 0.12, fS: 0, aD: 0.2, aS: 0, aR: 0.1, delayMix: 0.3, reverbMix: 0.3,
    }),
    patch('Acid Sequence', 'Sequenced', {
      master: 0.78, arpOn: true, arpMode: 'random', arpDiv: '1/16', arpOct: 2, arpGate: 0.5, bpm: 128, voiceMode: 'legato', glide: 0.04, o1Range: -1, m2: 0, cutoff: 250, res: 0.85, fEnvAmt: 4, fD: 0.15, fS: 0, drive: 0.5, aS: 1, aR: 0.05, delayMix: 0.15,
    }),
    patch('Bell Arp', 'Sequenced', {
      master: 0.76, arpOn: true, arpMode: 'up', arpDiv: '1/16', arpOct: 3, arpGate: 0.3, bpm: 110, o1Wave: 'sine', o2Wave: 'sine', o2Range: 2, o2Tune: 0, m2: 0.3, cutoff: 5000, res: 0, fEnvAmt: 0, aA: 0.001, aD: 0.8, aS: 0, aR: 0.6, delayMix: 0.3, reverbMix: 0.4,
    }),
    patch('Chip Melody', 'Sequenced', {
      master: 0.75, arpOn: true, arpMode: 'updown', arpDiv: '1/16', arpOct: 2, arpGate: 0.7, bpm: 150, voiceMode: 'mono', o1Wave: 'pulse', pw: 0.25, m2: 0, cutoff: 18000, res: 0, fEnvAmt: 0, aS: 0.7, aR: 0.03, velAmp: 0, delayMix: 0, reverbMix: 0.05,
    }),
    patch('Pluck Cascade', 'Sequenced', {
      master: 0.97, arpOn: true, arpMode: 'down', arpDiv: '1/16', arpOct: 3, arpGate: 0.4, bpm: 120, o2Wave: 'tri', o2Range: 1, o2Tune: 0, cutoff: 700, res: 0.2, fEnvAmt: 3, fD: 0.15, fS: 0, aA: 0.001, aD: 0.4, aS: 0, aR: 0.3, delayMix: 0.3, reverbMix: 0.35,
    }),
    patch('Triplet Run', 'Sequenced', {
      master: 0.82, arpOn: true, arpMode: 'up', arpDiv: '1/8T', arpOct: 2, arpGate: 0.5, bpm: 110, o1Wave: 'pulse', pw: 0.3, o2Tune: 0.06, cutoff: 900, res: 0.35, fEnvAmt: 2.5, fD: 0.12, fS: 0.1, aS: 0.5, aR: 0.1, delayMix: 0.25, delayDiv: '1/8T',
    }),
    patch('Half-Time Arp', 'Sequenced', {
      master: 0.91, arpOn: true, arpMode: 'up', arpDiv: '1/8', arpOct: 1, arpGate: 0.8, bpm: 70, o2Tune: 0.1, cutoff: 1200, res: 0.2, fEnvAmt: 1, aA: 0.05, aD: 0.6, aS: 0.3, aR: 0.5, spread: 1, reverbMix: 0.45,
    }),
    patch('Driving Bass', 'Sequenced', {
      master: 0.72, arpOn: true, arpMode: 'order', arpDiv: '1/16', arpOct: 1, arpGate: 0.35, bpm: 126, voiceMode: 'mono', o1Wave: 'square', o1Range: -1, o2Range: -1, o2Tune: 0.1, cutoff: 350, res: 0.35, fEnvAmt: 2.5, fD: 0.1, fS: 0, aS: 1, aR: 0.05, delayMix: 0,
    }),
    patch('Thirty-Second Ripple', 'Sequenced', {
      master: 1, arpOn: true, arpMode: 'up', arpDiv: '1/32', arpOct: 3, arpGate: 0.5, bpm: 100, o1Wave: 'tri', o2Wave: 'sine', o2Range: 1, o2Tune: 0, m2: 0.3, cutoff: 4000, res: 0, fEnvAmt: 0, aA: 0.001, aD: 0.2, aS: 0, aR: 0.15, delayMix: 0.2, reverbMix: 0.3,
    }),
    patch('Sequencer Lead', 'Sequenced', {
      master: 0.85, arpOn: true, latch: true, arpMode: 'order', arpDiv: '1/8', arpOct: 1, arpGate: 0.6, bpm: 115, voiceMode: 'legato', glide: 0.05, o2Tune: 0.08, cutoff: 1500, res: 0.4, fEnvAmt: 1.5, fD: 0.2, fS: 0.4, aS: 1, aR: 0.1, delayMix: 0.25,
    }),
    patch('Flowing Arp', 'Sequenced', {
      master: 0.79, arpOn: true, latch: true, arpMode: 'updown', arpDiv: '1/8', arpOct: 2, arpGate: 1, bpm: 90, o1Wave: 'tri', o2Tune: 0.1, m2: 0.4, cutoff: 1800, res: 0.1, fEnvAmt: 0, aA: 0.2, aS: 1, aR: 1, spread: 1, reverbMix: 0.5,
    }),
    patch('Random Sparkle', 'Sequenced', {
      master: 1, arpOn: true, arpMode: 'random', arpDiv: '1/32', arpOct: 4, arpGate: 0.2, bpm: 120, o1Wave: 'sine', o1Range: 1, m2: 0, cutoff: 6000, res: 0, fEnvAmt: 0, aA: 0.001, aD: 0.15, aS: 0, aR: 0.1, spread: 1, delayMix: 0.4, reverbMix: 0.5,
    }),
    patch('Dub Techno Chord', 'Sequenced', {
      master: 1, arpOn: true, arpMode: 'order', arpDiv: '1/4', arpOct: 1, arpGate: 0.3, bpm: 118, o2Tune: 7, o3Wave: 'saw', o3Range: 0, o3Tune: 3, m3: 0.5, cutoff: 600, res: 0.3, fEnvAmt: 1.5, fD: 0.2, fS: 0, aD: 0.3, aS: 0, aR: 0.2, delayMix: 0.5, delayFb: 0.7, reverbMix: 0.5,
    }),
    patch('Electro Bounce', 'Sequenced', {
      master: 0.83, arpOn: true, arpMode: 'updown', arpDiv: '1/16', arpOct: 2, arpGate: 0.5, bpm: 122, voiceMode: 'mono', o1Wave: 'square', o1Range: -1, m2: 0, pitchEnv: 3, cutoff: 400, res: 0.5, fEnvAmt: 3, fD: 0.1, fS: 0, aS: 1, aR: 0.05, delayMix: 0.1,
    }),
    patch('Hypnotic Loop', 'Sequenced', {
      master: 0.81, arpOn: true, latch: true, arpMode: 'order', arpDiv: '1/16', arpOct: 1, arpGate: 0.5, bpm: 120, cutoff: 400, res: 0.6, fEnvAmt: 1.5, fD: 0.1, fS: 0, aS: 1, aR: 0.05, lfoSync: true, lfoDiv: '2 bars', modDepth: 1, modPitch: 0, modFilter: 1.5, delayMix: 0.3,
    }),
    patch('Harp Glissando', 'Sequenced', {
      master: 0.72, arpOn: true, arpMode: 'up', arpDiv: '1/32', arpOct: 4, arpGate: 0.5, bpm: 90, o1Wave: 'tri', o2Wave: 'sine', o2Range: 1, o2Tune: 0, m2: 0.3, cutoff: 1800, res: 0, fEnvAmt: 1.5, fD: 0.2, fS: 0, aA: 0.002, aD: 1.5, aS: 0, aR: 1, spread: 0.8, reverbMix: 0.45,
    }),
    patch('Marimba Pattern', 'Sequenced', {
      master: 1, arpOn: true, arpMode: 'updown', arpDiv: '1/8', arpOct: 2, arpGate: 0.5, bpm: 105, o1Wave: 'sine', o2Wave: 'tri', o2Range: 1, o2Tune: 7, m2: 0.3, cutoff: 2000, res: 0.3, fEnvAmt: 1, fD: 0.1, fS: 0, aA: 0.001, aD: 0.35, aS: 0, aR: 0.3, reverbMix: 0.3,
    }),
    patch('Trance Lead Arp', 'Sequenced', {
      master: 0.75, arpOn: true, arpMode: 'up', arpDiv: '1/16', arpOct: 3, arpGate: 0.5, bpm: 138, voiceMode: 'unison', unisonDetune: 15, spread: 0.9, cutoff: 2000, res: 0.3, fEnvAmt: 1, fD: 0.2, fS: 0.4, aS: 1, aR: 0.1, delayMix: 0.3, reverbMix: 0.3,
    }),
    patch('Slow Motion', 'Sequenced', {
      master: 0.82, arpOn: true, arpMode: 'down', arpDiv: '1/4', arpOct: 2, arpGate: 0.8, bpm: 60, o2Tune: 0.1, cutoff: 1000, res: 0.2, fEnvAmt: 0.5, aA: 0.3, aS: 1, aR: 1.5, spread: 1, reverbMix: 0.6,
    }),
    patch('Drum Kit Arp', 'Sequenced', {
      master: 0.86, kit: true, kitId: 'Techno 909', arpOn: true, latch: true, arpMode: 'order', arpDiv: '1/16', arpOct: 1, arpGate: 0.5, bpm: 100, reverbMix: 0.12, reverbSize: 1.4, delayMix: 0,
    }),
  ];

  // ---------------------------------------------------------------- drum recipes

  const noteOfHz = (f) => 69 + 12 * Math.log2(f / 440);

  // A plain one-shot drum voice every recipe starts from.
  const DRUM = {
    voiceMode: 'poly', o1Wave: 'sine', o1Range: 0, o2Wave: 'sine', o2Range: 0, o2Tune: 0, o3Wave: 'square',
    o3Range: 0, o3Tune: 0, m1: 1, m2: 0, m3: 0, noise: 0, pitchEnv: 0, filterMode: 'lp24', cutoff: 18000,
    res: 0, keyTrack: 0, fEnvAmt: 0, fA: 0.001, fD: 0.05, fS: 0, fR: 0.05, aA: 0.001, aD: 0.3, aS: 0, aR: 0.2,
    velAmp: 0.6, velFilter: 0, drive: 0.3, modDepth: 0, glide: 0, o3KeyOff: false,
  };

  // Detuned square waves a little out of tune with each other: the metal in
  // an analog hi-hat or cymbal.
  const METAL = { o1Wave: 'square', o1Range: 1, o2Wave: 'square', o2Range: 1, o2Tune: 6.3, o3Wave: 'square', o3Range: 1, o3Tune: 4.1 };

  function drum(play, settings) {
    return { play, settings: Object.assign({}, DRUM, settings) };
  }

  const kickOf = (k) =>
    drum(noteOfHz(k.hz), {
      o1Wave: k.wave, noise: k.click, cutoff: 5000, pitchEnv: k.sweep, fD: k.sweepTime,
      aD: k.decay, aR: Math.min(0.3, k.decay), drive: k.drive,
    });

  const snareOf = (s, alt) =>
    drum(noteOfHz(s.hz), {
      // Two tuned drum-head tones, a little under an octave and a half apart, plus the snares (noise).
      m1: alt ? s.body * 0.4 : s.body, o2Range: 1, o2Tune: -1.5, m2: (alt ? s.body * 0.4 : s.body) * 0.6,
      noise: s.noise, pitchEnv: s.sweep, fD: 0.02, filterMode: alt ? 'hp12' : s.mode,
      cutoff: alt ? Math.min(18000, s.cutoff * 0.3) : s.cutoff, aA: s.attack, aD: alt ? s.decay * 0.8 : s.decay,
      aR: 0.15, drive: s.drive,
    });

  const tomOf = (t, step) =>
    drum(noteOfHz(t.hz * step), {
      o1Wave: t.wave, noise: t.noise, cutoff: 3000, pitchEnv: t.sweep, fD: 0.08, aD: t.decay, aR: t.decay * 0.6,
    });

  const congaOf = (c, step) =>
    drum(noteOfHz(c.hz * step), { o1Wave: c.wave, cutoff: 4000, pitchEnv: c.sweep, fD: 0.02, aD: c.decay, aR: c.decay * 0.6 });

  const hatOf = (h, decay) =>
    drum(noteOfHz(h.hz), Object.assign({}, METAL, {
      m1: h.metal, m2: h.metal, m3: h.metal, noise: h.noise, filterMode: h.mode, cutoff: h.cutoff, res: h.res,
      aD: decay, aR: 0.03, drive: h.drive,
    }));

  const cymbalOf = (c, decay, china) =>
    drum(noteOfHz(420), Object.assign({}, METAL, {
      m1: 0.5, m2: 0.5, m3: 0.5, noise: 0.8, filterMode: china ? 'bp' : 'hp12', cutoff: china ? c.cutoff * 1.3 : c.cutoff,
      res: china ? 0.5 : 0.2, aD: decay, aR: decay * 0.7, drive: china ? 1 : 0.6,
    }));

  const FX = {
    sub: () => drum(noteOfHz(45), { pitchEnv: 3, fD: 0.06, aD: 2.5, aR: 1, drive: 0.3 }),
    zap: () => drum(60, { o1Wave: 'saw', m1: 0.8, pitchEnv: 24, fD: 0.12, cutoff: 3000, res: 0.5, aD: 0.25 }),
    laser: () => drum(72, { pitchEnv: 24, fD: 0.2, aD: 0.3 }),
    blip: () => drum(72, { o1Wave: 'square', pitchEnv: 12, fD: 0.02, aD: 0.08, aR: 0.05 }),
    reverse: () => drum(60, { m1: 0, noise: 1, filterMode: 'hp12', cutoff: 2000, aA: 0.4, aD: 0.05, aR: 0.05, drive: 0.6 }),
    gong: () =>
      drum(48, Object.assign({}, METAL, { o1Range: -1, o2Range: -1, o3Range: -1, m2: 0.8, m3: 0.8, filterMode: 'bp', cutoff: 600, res: 0.5, aD: 2.5, aR: 1.5 })),
    vox: () => drum(48, { o1Wave: 'saw', cutoff: 500, res: 0.9, fEnvAmt: 2, fD: 0.3, aD: 0.4 }),
    thunder: () => drum(48, { m1: 0, noise: 1, cutoff: 450, res: 0.4, drive: 1, aD: 1.5, aR: 1 }),
    snap: () => drum(84, { m1: 0.5, noise: 0.8, pitchEnv: 12, fD: 0.02, filterMode: 'bp', cutoff: 2500, res: 0.5, aD: 0.07, aR: 0.05, drive: 0.8 }),
    timpani: () => drum(noteOfHz(90), { noise: 0.05, pitchEnv: 2, fD: 0.1, cutoff: 2000, aD: 2, aR: 1.2 }),
  };

  const ROLE = {
    kick: (k) => kickOf(k.kick),
    kick2: (k) => kickOf(Object.assign({}, k.kick, { hz: k.kick.hz * 0.9, decay: k.kick.decay * 2.5 + 0.3 })),
    kick3: (k) => kickOf(Object.assign({}, k.kick, { click: 0.35, drive: 1, decay: k.kick.decay * 0.6, sweep: Math.min(24, k.kick.sweep + 12) })),
    snare: (k) => snareOf(k.snare, false),
    snare2: (k) => snareOf(k.snare, true),
    clap: (k) =>
      drum(60, {
        m1: 0, noise: 1, filterMode: k.clap.mode, cutoff: k.clap.cutoff, res: k.clap.res, fEnvAmt: 1, fD: 0.1,
        aA: 0.003, aD: k.clap.decay, aR: 0.2, drive: 0.9,
      }),
    rim: (k) =>
      drum(noteOfHz(k.rim.hz), {
        o1Wave: 'tri', o2Wave: 'square', o2Range: 2, o2Tune: -1.5, m2: 0.5, noise: 0.2, filterMode: 'bp',
        cutoff: k.rim.cutoff, res: 0.3, aD: k.rim.decay, aR: 0.03, drive: 0.6,
      }),
    tomL: (k) => tomOf(k.tom, k.tom.steps[0]),
    tomM: (k) => tomOf(k.tom, k.tom.steps[1]),
    tomH: (k) => tomOf(k.tom, k.tom.steps[2]),
    chh: (k) => hatOf(k.hat, k.hat.closed),
    phh: (k) => hatOf(k.hat, k.hat.pedal),
    ohh: (k) => hatOf(k.hat, k.hat.open),
    crash: (k) => cymbalOf(k.cym, k.cym.crash, k.cym.china),
    ride: (k) => Object.assign(cymbalOf(k.cym, k.cym.ride, false), {}),
    splash: (k) => cymbalOf(k.cym, k.cym.splash, k.cym.china),
    congaL: (k) => congaOf(k.conga, k.conga.steps[0]),
    congaM: (k) => congaOf(k.conga, k.conga.steps[1]),
    congaH: (k) => congaOf(k.conga, k.conga.steps[2]),
    clave: (k) => drum(noteOfHz(k.clave.hz), { cutoff: 18000, aD: k.clave.decay, aR: 0.02 }),
    cowbell: (k) =>
      drum(noteOfHz(k.cowbell.hz), {
        // Two square waves a sixth apart, through a band-pass: 540 and 800 Hz on the classic machine.
        o1Wave: 'square', o2Wave: 'square', o2Tune: 6.8, m1: 0.7, m2: 0.7, filterMode: 'bp', cutoff: 1200, res: 0.25,
        aD: k.cowbell.decay, aR: 0.1,
      }),
    shaker: (k) =>
      drum(60, { m1: 0, noise: 1, filterMode: 'hp12', cutoff: k.shaker.cutoff, aA: 0.01, aD: k.shaker.decay, aR: 0.04, drive: 0.6 }),
    tamb: (k) =>
      drum(60, Object.assign({}, METAL, {
        o1Range: 2, o2Range: 2, m1: 0.35, m2: 0.35, noise: 1, filterMode: 'hp12', cutoff: 8000, aA: 0.003,
        aD: k.tamb.decay, aR: 0.1,
      })),
    fx: (k) => (FX[k.fx] || FX.zap)(),
  };

  function kitSpec(name) {
    const kit = KITS.find((k) => k.name === name) || KITS[0];
    const out = {};
    for (const key of Object.keys(KIT_BASE)) {
      const base = KIT_BASE[key];
      out[key] = base && typeof base === 'object' ? Object.assign({}, base, kit[key] || {}) : key in kit ? kit[key] : base;
    }
    out.name = kit.name;
    out.names = kit.names || {};
    out.patch = kit.patch || {};
    return out;
  }

  // The names on the keys for a kit, cheap enough to call on every redraw.
  function kitLayout(name) {
    const k = kitSpec(name);
    return KIT_SLOTS.map(([role, label, short]) => {
      const own = k.names[role];
      return { name: own ? own[0] : label, short: own ? own[1] : short };
    });
  }

  // Full settings for each drum of a kit, ready for the engine: { slot: { params, play, pan, choke, gain } }.
  function kitDrums(name) {
    const k = kitSpec(name);
    const levels = KIT_LEVELS[k.name] || [];
    const out = {};
    KIT_SLOTS.forEach(([role, , , choke, pan], slot) => {
      const d = ROLE[role](k);
      d.settings.cutoff = Math.min(d.settings.cutoff, k.maxCutoff);
      checkSettings(k.name + ' ' + role, d.settings);
      out[slot] = {
        params: engineParams(sanitizePatch(d.settings)),
        play: d.play,
        pan,
        choke,
        gain: levels[slot] || 1,
      };
    });
    return out;
  }

  // Each kit drum's level, measured so a kit plays evenly (see the tests).
  const KIT_LEVELS = {
    "Classic 808": [1.4, 2.08, 1.56, 1.43, 3.03, 1.11, 5.02, 1.11, 3.64, 1.12, 2.14, 1.12, 1.29, 3.5, 0.72, 0.91, 1.35, 1.14, 2.32, 1.35, 2.45, 1.37, 0.93, 1.21],
    "Trap 808": [0.75, 1.89, 1.97, 1.33, 1.61, 1.08, 4.81, 1.09, 3.43, 1.09, 1.84, 1.09, 0.71, 3.24, 0.66, 0.87, 1.29, 1.11, 2.14, 1.3, 2.45, 1.31, 1.22, 1.19],
    "Techno 909": [0.84, 1.91, 1.55, 1.24, 1.31, 1.18, 2.73, 1.18, 2.24, 1.19, 1.29, 1.07, 0.71, 3.24, 0.85, 0.87, 1.29, 1.09, 2.15, 1.3, 2.45, 1.31, 2.73, 1.18],
    "House": [0.99, 1.91, 1.43, 1.23, 1.53, 1.09, 3.65, 1.09, 2.65, 1.09, 1.7, 1.08, 0.84, 3.24, 0.88, 0.87, 1.29, 1.1, 2.12, 1.3, 2.47, 1.31, 2.03, 1.17],
    "Minimal Techno": [1.57, 2.43, 3.25, 1.34, 2.54, 1.09, 3.79, 1.09, 2.79, 1.09, 1.51, 1.07, 1.23, 3.24, 1.04, 0.88, 1.56, 1.12, 2.14, 1.57, 2.5, 1.58, 1.57, 1.19],
    "Rock": [0.9, 1.9, 1.35, 1.36, 1.03, 1.03, 2.52, 1.03, 2.35, 1.03, 1.32, 1.07, 0.74, 3.24, 0.92, 0.87, 1.29, 1.08, 2.1, 1.3, 2.43, 1.31, 0.8, 1.19],
    "Jazz Brushes": [2.48, 1.9, 4.24, 1.33, 1.95, 1.05, 3.43, 1.06, 3.16, 1.06, 1.68, 1.05, 2.03, 3.24, 0.92, 0.87, 1.29, 1.07, 2.13, 1.3, 2.48, 1.31, 1.55, 1.17],
    "Funk": [1.09, 1.92, 1.64, 1.33, 1.62, 1.08, 4.19, 1.09, 2.68, 1.1, 1.77, 1.09, 0.86, 3.24, 0.97, 0.88, 1.3, 1.1, 2.14, 1.3, 2.45, 1.31, 1.48, 1.18],
    "Boom Bap": [0.77, 1.91, 1.42, 1.35, 1.08, 1.09, 3.38, 1.09, 2.5, 1.1, 1.54, 1.07, 0.67, 3.24, 0.82, 0.87, 1.29, 1.12, 2.11, 1.3, 2.44, 1.31, 2.02, 1.18],
    "Lo-Fi": [0.85, 1.9, 1.47, 1.32, 1.59, 1.09, 2.46, 1.09, 1.76, 1.09, 1.17, 1, 0.72, 5.39, 0.88, 0.87, 1.29, 1.01, 2.03, 1.3, 2.15, 1.31, 0.84, 1.08],
    "Electro": [0.92, 1.93, 1.86, 1.19, 1.62, 1.45, 4.79, 1.46, 2.74, 1.47, 1.93, 1.09, 0.83, 3.24, 0.78, 0.87, 1.29, 1.11, 2.16, 1.3, 2.46, 1.31, 2.74, 1.18],
    "Synthwave 80s": [0.97, 1.94, 1.22, 1.16, 1, 1.25, 3.8, 1.25, 2.6, 1.26, 1.56, 1.09, 0.84, 3.24, 0.84, 0.88, 1.29, 1.11, 2.12, 1.3, 2.47, 1.31, 1.22, 1.16],
    "Disco": [1.17, 1.9, 1.39, 1.35, 1.49, 1.09, 3.7, 1.09, 2.83, 1.1, 1.71, 1.08, 0.97, 3.24, 0.92, 0.87, 1.29, 1.11, 2.12, 1.3, 2.47, 1.31, 1.22, 1.16],
    "Drum & Bass": [0.92, 1.89, 1.49, 1.32, 1.3, 1.09, 3.34, 1.09, 1.95, 1.09, 1.36, 1.09, 0.74, 3.24, 0.97, 0.87, 1.29, 1.11, 2.11, 1.3, 2.46, 1.31, 0.84, 1.18],
    "Dubstep": [0.75, 1.91, 1.16, 1.23, 0.98, 1.09, 3.65, 1.09, 2.6, 1.1, 1.57, 1.09, 0.65, 3.24, 0.82, 0.87, 1.29, 1.12, 2.11, 1.3, 2.46, 1.31, 2.04, 1.18],
    "Industrial": [0.82, 1.9, 1.77, 1.37, 1.02, 1.09, 1.47, 1.09, 1.08, 1.1, 0.64, 0.61, 0.68, 3.24, 0.88, 0.87, 1.29, 1.12, 2.13, 1.3, 2.48, 1.31, 5.82, 0.66],
    "Latin Percussion": [1.22, 1.92, 1.45, 1.32, 1.53, 1.4, 3.82, 1.41, 2.62, 1.43, 1.54, 1.09, 0.98, 2.84, 0.97, 0.79, 1.23, 1.11, 1.82, 1.23, 2.47, 1.25, 1.51, 1.17],
    "Afrobeat": [1.19, 1.89, 1.65, 1.32, 1.71, 1.13, 3.71, 1.14, 2.61, 1.15, 1.56, 1.08, 0.97, 3.24, 0.92, 0.84, 1.18, 1.12, 1.94, 1.18, 2.45, 1.19, 1.44, 1.17],
    "Reggae Dub": [1.04, 1.67, 1.36, 1.32, 3.17, 1.08, 3.66, 1.09, 2.63, 1.1, 1.56, 1.09, 0.94, 3.24, 0.78, 0.88, 1.3, 1.12, 2.13, 1.3, 2.41, 1.31, 1.22, 1.17],
    "Big Room EDM": [0.74, 1.92, 1.61, 1.13, 1.35, 1.08, 3.97, 1.09, 2.84, 1.09, 1.72, 1.08, 0.65, 3.24, 0.82, 0.87, 1.29, 1.11, 2.12, 1.3, 2.43, 1.31, 0.83, 1.18],
    "Chiptune": [1.06, 1.91, 2.04, 1.35, 2, 1.08, 4.54, 1.09, 3.89, 1.1, 2.31, 1.09, 0.77, 3.24, 0.98, 0.87, 1.32, 1.11, 2.1, 1.34, 2.5, 1.37, 1.57, 1.17],
    "Vintage Rhythm Box": [1.29, 1.91, 1.88, 1.3, 2.14, 1.23, 2.94, 1.23, 1.68, 1.23, 1.27, 1.16, 1, 3.23, 1.04, 0.87, 1.3, 1.2, 2.09, 1.31, 2.45, 1.33, 1.57, 1.3],
    "UK Garage": [1.23, 1.9, 1.59, 1.24, 1.79, 1.09, 4.27, 1.09, 2.64, 1.09, 1.8, 1.09, 0.98, 3.24, 0.97, 0.87, 1.29, 1.12, 2.17, 1.3, 2.48, 1.31, 2.05, 1.17],
    "Ambient": [1.72, 1.89, 3.85, 1.3, 4.65, 1.09, 2.73, 1.09, 2.51, 1.1, 1.3, 1.03, 1.58, 3.24, 0.75, 0.87, 1.29, 1.06, 2.15, 1.3, 2.5, 1.31, 0.78, 1.17],
    "Metal": [1.07, 1.95, 1.36, 1.35, 1.12, 1.06, 3.54, 1.06, 2.72, 1.06, 1.55, 0.59, 0.77, 3.24, 1.15, 0.87, 1.3, 1.11, 2.12, 1.3, 2.48, 1.31, 0.77, 0.66],
    "Glitch Beats": [1.58, 1.91, 3.84, 1.91, 2.59, 1.23, 5.48, 1.25, 4.35, 1.28, 2.89, 1.07, 1.05, 3.24, 1.31, 0.87, 1.29, 1.12, 2.07, 1.3, 2.45, 1.31, 1.57, 1.17],
    "Cinematic": [0.81, 1.89, 1.8, 1.35, 0.98, 0.96, 3.62, 0.96, 2.55, 0.96, 1.57, 1.03, 0.79, 3.24, 0.65, 0.87, 1.29, 1.1, 2.14, 1.3, 2.47, 1.31, 0.94, 1.17],
    "Tribal": [1.01, 1.91, 1.05, 1.37, 3.11, 1, 3.65, 0.99, 2.64, 1, 1.56, 1.08, 0.93, 3.24, 0.73, 0.88, 1.11, 1.1, 1.63, 1.11, 2.47, 1.12, 0.93, 1.16],
    "Electronic Toms 80s": [1.09, 1.91, 1.76, 1.32, 1.16, 1.22, 3.55, 1.23, 2.59, 1.24, 1.57, 1.08, 0.95, 3.24, 0.82, 0.87, 1.35, 1.12, 2.16, 1.36, 2.44, 1.38, 1.22, 1.17],
    "Hand Percussion": [1.21, 1.9, 1.99, 1.49, 2.68, 1.46, 3.72, 1.47, 2.6, 1.49, 1.56, 1.08, 0.98, 3.24, 0.97, 0.8, 1.22, 1.12, 1.71, 1.23, 2.16, 1.24, 1.47, 1.15],
  };

  const api = {
    WAVES,
    PARAMS,
    BY_ID,
    OPTION_LABELS,
    PRESETS,
    KITS,
    KIT_NAMES,
    KIT_SLOTS,
    kitLayout,
    kitDrums,
    KEYMAP,
    defaults,
    engineParams,
    clampParam,
    toNorm,
    fromNorm,
    formatParam,
    sanitizePatch,
    encodePatch,
    decodePatch,
    noteName,
    isBlack,
  };
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.LadderLogic = api;
})(typeof globalThis !== 'undefined' ? globalThis : this);
