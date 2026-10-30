/*
 * Ladder 24 — the panel's rules, with no sound and no page.
 *
 * Every control (its range, its curve, how its value reads), the factory
 * patches, the computer-keyboard map, and the patch code you can copy and
 * paste. Plain functions, so the tests run them in Node.
 */
(function (root) {
  'use strict';

  const WAVES = ['tri', 'saw', 'ramp', 'square', 'pulse', 'sine'];

  // type: knob | enum | bool. curve: lin | log | pow (cubic, fine control near 0).
  // unit picks how the value reads on the panel.
  const PARAMS = [
    // Controllers
    { id: 'glide', label: 'Glide', type: 'knob', min: 0, max: 2, def: 0, curve: 'pow', unit: 's' },
    { id: 'bendRange', label: 'Bend', type: 'knob', min: 1, max: 12, def: 2, step: 1, unit: 'st' },
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
    { id: 'pw', label: 'Pulse width', type: 'knob', min: 0.05, max: 0.5, def: 0.3, unit: '%' },
    // Mixer
    { id: 'm1', label: 'Osc 1', type: 'knob', min: 0, max: 1, def: 0.8, unit: 'level' },
    { id: 'm2', label: 'Osc 2', type: 'knob', min: 0, max: 1, def: 0.6, unit: 'level' },
    { id: 'm3', label: 'Osc 3', type: 'knob', min: 0, max: 1, def: 0, unit: 'level' },
    { id: 'noise', label: 'Noise', type: 'knob', min: 0, max: 1, def: 0, unit: 'level' },
    { id: 'drive', label: 'Drive', type: 'knob', min: 0, max: 1, def: 0.2, unit: 'level' },
    // Filter
    { id: 'cutoff', label: 'Cutoff', type: 'knob', min: 20, max: 18000, def: 1200, curve: 'log', unit: 'hz' },
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
    { id: 'lfoShape', label: 'Shape', type: 'enum', options: ['tri', 'saw', 'square', 'sh'], def: 'tri' },
    { id: 'modSource', label: 'Source', type: 'enum', options: ['lfo', 'osc3', 'noise'], def: 'lfo' },
    { id: 'modDepth', label: 'Amount', type: 'knob', min: 0, max: 1, def: 0, unit: '%' },
    { id: 'modPitch', label: '→ Pitch', type: 'knob', min: 0, max: 12, def: 0.3, curve: 'pow', unit: 'semi' },
    { id: 'modFilter', label: '→ Filter', type: 'knob', min: 0, max: 4, def: 0, unit: 'oct' },
    { id: 'modPw', label: '→ Width', type: 'knob', min: 0, max: 0.45, def: 0, unit: '%' },
    // Arpeggiator
    { id: 'arpOn', label: 'Arp', type: 'bool', def: false },
    { id: 'latch', label: 'Latch', type: 'bool', def: false },
    { id: 'arpMode', label: 'Order', type: 'enum', options: ['up', 'down', 'updown', 'order', 'random'], def: 'up' },
    { id: 'arpBpm', label: 'Tempo', type: 'knob', min: 40, max: 240, def: 120, step: 1, unit: 'bpm' },
    { id: 'arpDiv', label: 'Step', type: 'enum', options: ['1/4', '1/8', '1/8T', '1/16', '1/32'], def: '1/16' },
    { id: 'arpOct', label: 'Octaves', type: 'knob', min: 1, max: 4, def: 2, step: 1, unit: 'x' },
    { id: 'arpGate', label: 'Gate', type: 'knob', min: 0.05, max: 1, def: 0.5, unit: '%' },
    // Effects and output (these never reach the engine)
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

  function patch(name, group, changes) {
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

    // ---- Drums: one key each, the pitch follows the key you play
    patch('Analog Kick', 'Drums', {
      master: 0.74,
      voiceMode: 'mono', o1Wave: 'sine', o1Range: -1, m1: 1, m2: 0, cutoff: 60, res: 0.8, keyTrack: 0.5,
      fEnvAmt: 5, fA: 0.001, fD: 0.05, fS: 0, fR: 0.05, aA: 0.001, aD: 0.4, aS: 0, aR: 0.25, velAmp: 0.6,
      drive: 0.5, delayMix: 0, reverbMix: 0.05,
    }),
    patch('Analog Tom', 'Drums', {
      master: 0.81,
      o1Wave: 'sine', m1: 1, m2: 0, cutoff: 150, res: 0.75, keyTrack: 1, fEnvAmt: 3.5, fA: 0.001, fD: 0.08,
      fS: 0, aA: 0.001, aD: 0.5, aS: 0, aR: 0.35, velAmp: 0.6, drive: 0.3, delayMix: 0, reverbMix: 0.15,
    }),
    patch('Snare', 'Drums', {
      master: 0.95,
      o1Wave: 'tri', o1Range: -1, m1: 0.7, m2: 0, noise: 1, cutoff: 3000, res: 0.2, keyTrack: 0.5, fEnvAmt: 2,
      fA: 0.001, fD: 0.1, fS: 0, aA: 0.001, aD: 0.18, aS: 0, aR: 0.15, velAmp: 0.7, drive: 0.9, delayMix: 0,
      reverbMix: 0.15,
    }),
    patch('Closed Hat', 'Drums', {
      master: 1,
      m1: 0, m2: 0, noise: 1, cutoff: 9000, res: 0.3, keyTrack: 0, fEnvAmt: 1, fD: 0.05, fS: 0,
      aA: 0.001, aD: 0.06, aS: 0, aR: 0.05, velAmp: 0.7, drive: 1, delayMix: 0, reverbMix: 0.05,
    }),

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
      delayMix: 0.3, delayTime: 0.25, delayFb: 0.5, reverbMix: 0.25,
    }),
    patch('Metal Bells', 'Experimental', {
      master: 0.81,
      o1Wave: 'sine', o2Wave: 'tri', o2Tune: 6.9, o3Range: 1, o3Wave: 'square', o3Tune: 3.2,
      modSource: 'osc3', modDepth: 1, modPitch: 0, modFilter: 1.6, m2: 0.5, cutoff: 1400, res: 0.5,
      fEnvAmt: 2.5, fD: 0.9, fS: 0, aA: 0.002, aD: 1.8, aS: 0, aR: 1.5, reverbMix: 0.4, delayMix: 0.18,
    }),

    // ---- Sequenced: hold a chord and the arpeggiator plays it
    patch('Pulse Arp', 'Sequenced', {
      master: 1,
      arpOn: true, arpMode: 'updown', arpDiv: '1/16', arpOct: 2, arpGate: 0.45, arpBpm: 118, o1Wave: 'pulse',
      pw: 0.25, o2Wave: 'saw', o2Tune: 7, m2: 0.35, cutoff: 480, res: 0.55, fEnvAmt: 3.4,
      fD: 0.16, fS: 0, fR: 0.12, aD: 0.22, aS: 0.2, aR: 0.12, delayTime: 0.381, delayFb: 0.45, delayMix: 0.3,
      reverbMix: 0.22,
    }),
    patch('Latched Sequence', 'Sequenced', {
      master: 0.65,
      arpOn: true, latch: true, arpMode: 'order', arpDiv: '1/8', arpOct: 1, arpGate: 0.7, arpBpm: 100,
      voiceMode: 'mono', glide: 0.03, o1Range: -1, o2Range: -1, o2Tune: 0.1, cutoff: 380, res: 0.6,
      fEnvAmt: 3, fD: 0.22, fS: 0.1, aS: 1, aR: 0.1, drive: 0.5, delayMix: 0.25, delayTime: 0.45,
    }),
    patch('Berlin School', 'Sequenced', {
      master: 0.75,
      arpOn: true, latch: true, arpMode: 'order', arpDiv: '1/16', arpOct: 2, arpGate: 0.5, arpBpm: 112,
      voiceMode: 'mono', o1Range: -1, o2Range: -1, o2Wave: 'square', o2Tune: 0.08, cutoff: 350, res: 0.65,
      fEnvAmt: 3.2, fD: 0.14, fS: 0, aS: 1, aR: 0.08, modDepth: 1, modPitch: 0, modFilter: 0.7, lfoRate: 0.07,
      delayMix: 0.35, delayTime: 0.4, delayFb: 0.5, reverbMix: 0.3,
    }),
    patch('Chip Arp', 'Sequenced', {
      master: 1,
      arpOn: true, arpMode: 'up', arpDiv: '1/32', arpOct: 2, arpGate: 0.8, arpBpm: 130, voiceMode: 'mono',
      o1Wave: 'pulse', pw: 0.125, m2: 0, cutoff: 18000, res: 0, fEnvAmt: 0, aD: 0.1, aS: 0.5, aR: 0.03,
      velAmp: 0, delayMix: 0, reverbMix: 0.08,
    }),
    patch('Trance Gate', 'Sequenced', {
      master: 0.83,
      arpOn: true, arpMode: 'updown', arpDiv: '1/16', arpOct: 1, arpGate: 0.3, arpBpm: 138,
      voiceMode: 'unison', unisonDetune: 20, spread: 0.9, o2Tune: 7, m2: 0.4, cutoff: 2600, res: 0.25,
      fEnvAmt: 1, fD: 0.2, fS: 0.5, aS: 1, aR: 0.05, delayMix: 0.3, delayTime: 0.326, delayFb: 0.4,
      reverbMix: 0.3,
    }),
    patch('Random Bleeps', 'Sequenced', {
      master: 1,
      arpOn: true, arpMode: 'random', arpDiv: '1/16', arpOct: 3, arpGate: 0.35, arpBpm: 120, o1Wave: 'square',
      m2: 0, cutoff: 900, res: 0.7, fEnvAmt: 2, fD: 0.1, fS: 0, aD: 0.15, aS: 0, aR: 0.1, lfoShape: 'sh',
      lfoRate: 8, modDepth: 1, modPitch: 0, modFilter: 1.5, delayMix: 0.35, delayTime: 0.25, delayFb: 0.5,
      reverbMix: 0.3,
    }),
  ];

  const api = {
    WAVES,
    PARAMS,
    BY_ID,
    OPTION_LABELS,
    PRESETS,
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
