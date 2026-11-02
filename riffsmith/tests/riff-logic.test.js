/*
 * RIFFSMITH — logic tests.
 *
 *   node riffsmith/tests/riff-logic.test.js
 *
 * No dependencies and no browser: the engine is a plain module. These prove
 * the promises the app makes — the same settings always give the same riff,
 * rolling one step changes only that step, every note lands in the key and
 * the range Fit asks for, each step really does what its controls say, and
 * the MIDI file it saves is a valid file that reads back to the same notes.
 */
'use strict';

const path = require('path');
const R = require(path.join(__dirname, '..', 'js', 'riff.js'));

let passed = 0;
const failures = [];

function test(name, fn) {
  try {
    fn();
    passed++;
    console.log('  ok   ' + name);
  } catch (e) {
    failures.push(name + ' — ' + e.message);
    console.log('  FAIL ' + name + ' — ' + e.message);
  }
}

function assert(cond, message) { if (!cond) throw new Error(message || 'assertion failed'); }
function eq(a, b, message) {
  const sa = JSON.stringify(a), sb = JSON.stringify(b);
  if (sa !== sb) throw new Error((message || 'not equal') + ': ' + sa + ' !== ' + sb);
}

function settings(overrides) {
  const s = R.defaults();
  Object.keys(overrides || {}).forEach((k) => {
    if (overrides[k] && typeof overrides[k] === 'object') Object.assign(s[k], overrides[k]);
    else s[k] = overrides[k];
  });
  return R.normalize(s);
}

function pcInScale(pitch, key, scaleId) {
  const steps = R.SCALES[scaleId].steps;
  return steps.indexOf((((pitch - key) % 12) + 12) % 12) !== -1;
}

// A tiny reader for the MIDI files toMidi writes, to prove they round-trip.
function readMidi(bytes) {
  let i = 0;
  const str = (n) => { let s = ''; for (let k = 0; k < n; k++) s += String.fromCharCode(bytes[i++]); return s; };
  const u32 = () => { const v = (bytes[i] << 24 | bytes[i + 1] << 16 | bytes[i + 2] << 8 | bytes[i + 3]) >>> 0; i += 4; return v; };
  const u16 = () => { const v = bytes[i] << 8 | bytes[i + 1]; i += 2; return v; };
  const vlq = () => { let v = 0, b; do { b = bytes[i++]; v = (v << 7) | (b & 0x7F); } while (b & 0x80); return v; };
  assert(str(4) === 'MThd', 'header chunk');
  assert(u32() === 6, 'header length');
  const format = u16(), tracks = u16(), ppq = u16();
  assert(str(4) === 'MTrk', 'track chunk');
  const len = u32();
  const end = i + len;
  let t = 0, tempo = null, endOfTrack = false;
  const open = {}, notes = [];
  while (i < end) {
    t += vlq();
    const status = bytes[i++];
    if (status === 0xFF) {
      const type = bytes[i++];
      const l = vlq();
      if (type === 0x51) tempo = (bytes[i] << 16) | (bytes[i + 1] << 8) | bytes[i + 2];
      if (type === 0x2F) endOfTrack = true;
      i += l;
    } else if ((status & 0xF0) === 0x90 && bytes[i + 1] > 0) {
      open[bytes[i]] = { t, pitch: bytes[i], vel: bytes[i + 1] };
      i += 2;
    } else if ((status & 0xF0) === 0x80 || (status & 0xF0) === 0x90) {
      const o = open[bytes[i]];
      assert(o, 'note-off without a note-on');
      notes.push({ t: o.t, len: t - o.t, pitch: o.pitch, vel: o.vel });
      delete open[bytes[i]];
      i += 2;
    } else {
      throw new Error('unexpected status byte ' + status);
    }
  }
  assert(i === end, 'track length matches its contents');
  assert(i === bytes.length, 'nothing after the track');
  return { format, tracks, ppq, tempo, endOfTrack, notes, length: t, openNotes: Object.keys(open).length };
}

console.log('Riffsmith engine');

// ------------------------------------------------------------- settings

test('defaults are already normal', () => {
  eq(R.normalize(R.defaults()), R.defaults());
});

test('normalize repairs junk and out-of-range values', () => {
  const s = R.normalize({ key: 99, bpm: 'fast', arp: { rate: 7, gate: 5, flip: 'yes' },
    progression: { mood: 'furious', pattern: '99' }, fit: { low: 80, high: 70 } });
  eq(s.key, 11);
  eq(s.bpm, R.defaults().bpm);
  eq(s.arp.rate, R.defaults().arp.rate);
  eq(s.arp.gate, 1);
  eq(s.arp.flip, R.defaults().arp.flip);
  eq(s.progression.mood, R.defaults().progression.mood);
  eq(s.progression.pattern, 'auto');
  assert(s.fit.high >= s.fit.low + 12, 'range is at least an octave');
  eq(R.normalize(null), R.defaults());
});

test('every preset loads and makes notes', () => {
  R.PRESETS.forEach((p) => {
    const r = R.generate(R.applyPreset(p.id));
    assert(r.notes.length > 0, p.id + ' made no notes');
  });
});

// --------------------------------------------------------- determinism

test('the same settings always give the same riff', () => {
  R.PRESETS.forEach((p) => {
    const s = R.applyPreset(p.id);
    eq(R.generate(s).notes, R.generate(JSON.parse(JSON.stringify(s))).notes, p.id);
  });
});

test('rolling the dice changes the riff', () => {
  const s = settings({ progression: { variation: 0.5 }, arp: { pattern: 'random' } });
  const rng = R.mulberry32(42);
  let changed = 0;
  for (let k = 0; k < 10; k++) {
    if (JSON.stringify(R.generate(R.reroll(s, {}, rng)).notes) !== JSON.stringify(R.generate(s).notes)) changed++;
  }
  assert(changed >= 8, 'only ' + changed + ' of 10 rolls changed anything');
});

test('a kept (locked) step keeps its seed when rolling', () => {
  const s = R.defaults();
  const locked = { progression: true, arp: true };
  const r = R.reroll(s, locked, R.mulberry32(7));
  eq(r.progression.seed, s.progression.seed);
  eq(r.arp.seed, s.arp.seed);
  assert(r.chords.seed !== s.chords.seed, 'unlocked step got a new seed');
});

test('re-rolling one step leaves the steps before it untouched', () => {
  // Levels only changes loudness and pan, so timing and pitch must not move.
  const s = settings({ levels: { velVar: 1, panVar: 1 } });
  const before = R.generate(s).notes;
  const s2 = JSON.parse(JSON.stringify(s));
  s2.levels.seed = 123456;
  const after = R.generate(s2).notes;
  eq(after.map((n) => [n.t, n.pitch, n.len]), before.map((n) => [n.t, n.pitch, n.len]));
  assert(JSON.stringify(after) !== JSON.stringify(before), 'velocities changed');
});

test('randomizeStep only changes that step', () => {
  const s = R.defaults();
  R.STEPS.forEach((step) => {
    const r = R.randomizeStep(s, step.id, R.mulberry32(3));
    R.STEPS.forEach((other) => {
      if (other.id !== step.id) eq(r[other.id], s[other.id], step.id + ' touched ' + other.id);
    });
  });
});

// ----------------------------------------------------------- the steps

test('progression: one long note per chord, chord names follow the key', () => {
  const s = settings({ key: 0, progression: { mood: 'happy', pattern: '0', bars: 4, chordBars: 1 } });
  const r = R.generate(s, 0);
  eq(r.notes.length, 4);
  r.notes.forEach((n) => eq(n.len, R.BAR));
  eq(r.chords.map((c) => c.name), ['C', 'G', 'Am', 'F']);
  eq(r.chords.map((c) => c.roman), ['I', 'V', 'vi', 'IV']);
});

test('progression: bars and bars-per-chord set the chord count', () => {
  eq(R.generate(settings({ progression: { bars: 8, chordBars: 2 } }), 0).notes.length, 4);
  eq(R.generate(settings({ progression: { bars: 2, chordBars: 0.5 } }), 0).notes.length, 4);
  eq(R.generate(settings({ progression: { bars: 16 } })).length, 16 * R.BAR);
});

test('every built-in progression uses only major and minor chords', () => {
  Object.keys(R.MOODS).forEach((mood) => {
    R.MOODS[mood].progressions.forEach((p, i) => {
      const r = R.generate(settings({ progression: { mood, pattern: String(i) } }), 0);
      r.chords.forEach((c) => assert(!/[°+]/.test(c.name), mood + ' #' + i + ' has ' + c.name));
    });
  });
});

test('chords: triads have 3 notes, sevenths 4, ninths 5, bass adds one', () => {
  const count = (type, bass) =>
    R.generate(settings({ chords: { type, bass: !!bass, inversion: 'none' } }), 1).notes
      .filter((n) => n.t === 0).length;
  eq(count('triad'), 3);
  eq(count('seventh'), 4);
  eq(count('ninth'), 5);
  eq(count('power'), 3);
  eq(count('triad', true), 4);
});

test('chords: a C major triad in C is C E G', () => {
  const r = R.generate(settings({ key: 0, progression: { mood: 'happy', pattern: '0' },
    chords: { type: 'triad', inversion: 'none', voicing: 'close' }, fit: { on: false } }), 1);
  eq(r.notes.filter((n) => n.t === 0).map((n) => R.noteName(n.pitch)), ['C4', 'E4', 'G4']);
});

test('chords: smooth inversions move less than root position', () => {
  const spread = (inv) => {
    const r = R.generate(settings({ progression: { mood: 'happy', pattern: '0', bars: 8 },
      chords: { inversion: inv }, fit: { on: false } }), 1);
    const centres = [];
    for (let t = 0; t < r.length; t += R.BAR) {
      const ps = r.notes.filter((n) => n.t === t).map((n) => n.pitch);
      centres.push(ps.reduce((a, b) => a + b, 0) / ps.length);
    }
    let move = 0;
    for (let i = 1; i < centres.length; i++) move += Math.abs(centres[i] - centres[i - 1]);
    return move;
  };
  assert(spread('smooth') < spread('none'), 'smooth ' + spread('smooth') + ' vs none ' + spread('none'));
});

test('arpeggio: speed sets the spacing, gate sets the length', () => {
  const r = R.generate(settings({ arp: { rate: 16, gate: 0.5, pattern: 'up', rests: 0 },
    levels: { on: false }, art: { on: false }, groove: { on: false } }));
  const starts = [...new Set(r.notes.map((n) => n.t))];
  eq(starts.length, 4 * 16, 'a note on every 16th of 4 bars');
  starts.forEach((t, i) => eq(t, i * 24));
  r.notes.forEach((n) => eq(n.len, 12));
});

test('arpeggio: triplets fit 3 notes where 2 would go', () => {
  const r = R.generate(settings({ arp: { rate: 8, triplet: true, rests: 0 },
    art: { on: false }, groove: { on: false } }));
  eq(r.notes.filter((n) => n.t < R.PPQ).length, 3);
});

test('arpeggio: up climbs and down falls within a chord', () => {
  const base = { progression: { mood: 'happy', pattern: '0' }, chords: { inversion: 'none' },
    arp: { range: 1, rate: 16 }, art: { on: false }, groove: { on: false }, fit: { on: false } };
  const first3 = (pattern) => R.generate(settings(Object.assign({}, base,
    { arp: Object.assign({}, base.arp, { pattern }) }))).notes.slice(0, 3).map((n) => n.pitch);
  const up = first3('up'), down = first3('down');
  assert(up[0] < up[1] && up[1] < up[2], 'up: ' + up);
  assert(down[0] > down[1] && down[1] > down[2], 'down: ' + down);
  eq(down, up.slice().reverse());
});

test('arpeggio: flip reverses the pattern', () => {
  const s = { arp: { pattern: 'up', range: 1 }, art: { on: false }, groove: { on: false }, fit: { on: false } };
  const plain = R.generate(settings(s)).notes.slice(0, 3).map((n) => n.pitch);
  const flipped = R.generate(settings(Object.assign({}, s, { arp: { pattern: 'up', range: 1, flip: true } })))
    .notes.slice(0, 3).map((n) => n.pitch);
  eq(flipped, plain.slice().reverse());
});

test('arpeggio: more octaves reach higher', () => {
  const top = (range) => Math.max(...R.generate(settings({ arp: { range }, fit: { on: false } })).notes
    .map((n) => n.pitch));
  assert(top(3) >= top(1) + 12, 'range 3 ' + top(3) + ' vs range 1 ' + top(1));
});

test('arpeggio: rests leave gaps but never on a chord\'s first beat', () => {
  const s = settings({ arp: { rests: 0.6, rate: 16 }, groove: { on: false } });
  const r = R.generate(s);
  assert(r.notes.length < 64, 'some notes were rested');
  for (let bar = 0; bar < 4; bar++) assert(r.notes.some((n) => n.t === bar * R.BAR), 'bar ' + bar + ' starts');
});

test('arpeggio: chord stabs play whole chords together', () => {
  const r = R.generate(settings({ arp: { pattern: 'stab', rate: 8 }, chords: { type: 'triad' },
    groove: { on: false } }));
  eq(r.notes.filter((n) => n.t === 0).length, 3);
  eq(r.notes.filter((n) => n.t === 48).length, 3);
});

test('mirror: back to front reverses the second bar', () => {
  const base = { progression: { pattern: '0', bars: 2, chordBars: 2 }, chords: { inversion: 'none' },
    arp: { pattern: 'up', range: 1, rate: 16, gate: 1 }, art: { on: false }, groove: { on: false },
    levels: { on: false }, fit: { on: false } };
  const plain = R.generate(settings(base)).notes;
  const mir = R.generate(settings(Object.assign({}, base,
    { mirror: { on: true, vertical: false, horizontal: true, where: 'bar' } }))).notes;
  const bar = (notes, b) => notes.filter((n) => n.t >= b * R.BAR && n.t < (b + 1) * R.BAR).map((n) => n.pitch);
  eq(bar(mir, 0), bar(plain, 0), 'first bar unchanged');
  eq(bar(mir, 1), bar(plain, 1).reverse(), 'second bar reversed');
});

test('mirror: upside down turns a climb into a fall', () => {
  const base = { progression: { pattern: '0', bars: 2, chordBars: 2 }, arp: { pattern: 'up', range: 1 },
    art: { on: false }, groove: { on: false }, fit: { on: false } };
  const mir = R.generate(settings(Object.assign({}, base,
    { mirror: { on: true, vertical: true, horizontal: false, where: 'bar' } }))).notes;
  const plain = R.generate(settings(base)).notes;
  // Wherever the plain riff steps up in the second bar, the mirrored one steps
  // down by the same amount, and the other way round.
  const moves = (notes) => {
    const ps = notes.filter((n) => n.t >= R.BAR).map((n) => n.pitch);
    return ps.slice(1).map((p, i) => Math.sign(p - ps[i]));
  };
  eq(moves(mir), moves(plain).map((m) => -m));
  eq(mir.filter((n) => n.t < R.BAR), plain.filter((n) => n.t < R.BAR), 'first bar unchanged');
});

test('levels: velocities stay in range, accents land on the beat', () => {
  const r = R.generate(settings({ levels: { velocity: 0.6, velVar: 0, accent: 1, panVar: 1 },
    groove: { on: false } }));
  r.notes.forEach((n) => {
    assert(n.vel > 0 && n.vel <= 1, 'vel ' + n.vel);
    assert(n.pan >= -1 && n.pan <= 1, 'pan ' + n.pan);
  });
  const onBeat = r.notes.find((n) => n.t === R.PPQ);
  const offBeat = r.notes.find((n) => n.t === R.PPQ + 24);
  assert(onBeat.vel > offBeat.vel, 'accent ' + onBeat.vel + ' > ' + offBeat.vel);
});

test('articulation: length multiplies, legato joins notes up', () => {
  const base = { arp: { gate: 0.5, rate: 16, rests: 0 }, groove: { on: false } };
  // (The very last note is trimmed to the riff's end, so it is left out.)
  const long = R.generate(settings(Object.assign({}, base, { art: { multiply: 2 } })));
  long.notes.slice(0, -1).forEach((n) => eq(n.len, 24));
  const leg = R.generate(settings(Object.assign({}, base, { art: { legato: true } })));
  leg.notes.slice(0, -1).forEach((n) => eq(n.len, 24));
});

test('articulation: ratchets add quick repeats', () => {
  const base = { arp: { rate: 8, gate: 1 }, groove: { on: false } };
  const plain = R.generate(settings(base)).notes.length;
  const rat = R.generate(settings(Object.assign({}, base, { art: { ratchet: 1 } }))).notes.length;
  assert(rat >= plain * 2, rat + ' vs ' + plain);
});

test('groove: swing pushes offbeats late and leaves the beat alone', () => {
  const r = R.generate(settings({ arp: { rate: 8, rests: 0 }, groove: { template: 'swing8', amount: 1, humanize: 0 } }));
  assert(r.notes.some((n) => n.t === 0), 'downbeat stays');
  assert(r.notes.some((n) => n.t === 48 + 16), 'offbeat 8th moved to the triplet');
  assert(!r.notes.some((n) => n.t === 48), 'no straight offbeat left');
});

test('groove: straight with no human feel leaves timing on the grid', () => {
  const r = R.generate(settings({ groove: { template: 'straight', humanize: 0 } }));
  r.notes.forEach((n) => eq(n.t % 24, 0));
});

test('fit: every note stays in range and in key, in every preset and many rolls', () => {
  const rng = R.mulberry32(99);
  R.PRESETS.forEach((p) => {
    let s = R.applyPreset(p.id);
    for (let k = 0; k < 15; k++) {
      R.STEPS.forEach((step) => { s = R.randomizeStep(s, step.id, rng); });
      s.key = Math.floor(rng() * 12);
      s = R.normalize(s);
      const scale = R.MOODS[s.progression.mood].scale;
      R.generate(s).notes.forEach((n) => {
        assert(n.pitch >= s.fit.low && n.pitch <= s.fit.high,
          p.id + ': ' + R.noteName(n.pitch) + ' outside ' + s.fit.low + '..' + s.fit.high);
        assert(pcInScale(n.pitch, s.key, scale), p.id + ': ' + R.noteName(n.pitch) + ' not in ' + scale);
      });
    }
  });
});

test('fit: a different scale snaps every note into it', () => {
  const r = R.generate(settings({ key: 2, fit: { scale: 'pentMinor' } }));
  r.notes.forEach((n) => assert(pcInScale(n.pitch, 2, 'pentMinor'), R.noteName(n.pitch)));
});

test('fit: a tight range squashes, folds or drops notes', () => {
  ['fold', 'clamp', 'drop'].forEach((mode) => {
    const s = settings({ arp: { range: 4 }, fit: { low: 60, high: 72, mode } });
    const r = R.generate(s);
    r.notes.forEach((n) => assert(n.pitch >= 60 && n.pitch <= 72, mode + ' ' + n.pitch));
  });
  const all = R.generate(settings({ arp: { range: 4 }, fit: { low: 60, high: 72, mode: 'fold' } })).notes.length;
  const kept = R.generate(settings({ arp: { range: 4 }, fit: { low: 60, high: 72, mode: 'drop' } })).notes.length;
  assert(kept < all, 'drop removes notes');
});

test('snapToScale picks the nearest scale note', () => {
  eq(R.snapToScale(61, 0, R.SCALES.major.steps), 60); // C# -> C (ties go down)
  eq(R.snapToScale(66, 0, R.SCALES.pentMajor.steps), 67); // F# -> G
  eq(R.snapToScale(66, 0, R.SCALES.chromatic.steps), 66);
});

test('switching a step off skips it', () => {
  const noArp = R.generate(settings({ arp: { on: false }, art: { on: false } }));
  eq(noArp.notes.filter((n) => n.t === 0).length, 3, 'held chords remain');
  noArp.notes.forEach((n) => eq(n.len, R.BAR));
});

test('"only up to this step" stops part-way', () => {
  const s = R.defaults();
  eq(R.generate(s, 0).notes.length, 4, 'progression only');
  eq(R.generate(s, 1).notes.length, 12, 'plus chords');
  assert(R.generate(s, 2).notes.length > 12, 'plus the arpeggio');
});

test('the same pitch never overlaps itself (a MIDI file could not hold it)', () => {
  const rng = R.mulberry32(11);
  let s = settings({ art: { legato: true, multiply: 2, ratchet: 0.5 }, groove: { humanize: 1 } });
  for (let k = 0; k < 40; k++) {
    s = R.reroll(s, {}, rng);
    const last = {};
    R.generate(s).notes.forEach((n) => {
      if (last[n.pitch] !== undefined) assert(last[n.pitch] <= n.t, 'overlap at ' + n.t);
      last[n.pitch] = n.t + n.len;
    });
  }
});

test('notes never run past the end and never double up', () => {
  const rng = R.mulberry32(5);
  let s = R.defaults();
  for (let k = 0; k < 40; k++) {
    R.STEPS.forEach((step) => { s = R.randomizeStep(s, step.id, rng); });
    const r = R.generate(s);
    const seen = new Set();
    r.notes.forEach((n) => {
      assert(n.t >= 0 && n.len >= 1 && n.t + n.len <= r.length, 'note ' + JSON.stringify(n));
      const k2 = n.t + ':' + n.pitch;
      assert(!seen.has(k2), 'duplicate ' + k2);
      seen.add(k2);
    });
  }
});

// ------------------------------------------------------------------ MIDI

test('MIDI: a valid file that reads back to the same notes', () => {
  R.PRESETS.forEach((p) => {
    const s = R.applyPreset(p.id);
    const r = R.generate(s);
    const m = readMidi(R.toMidi(r, { bpm: s.bpm }));
    eq(m.format, 0);
    eq(m.tracks, 1);
    eq(m.ppq, R.PPQ);
    eq(m.tempo, Math.round(60000000 / s.bpm));
    assert(m.endOfTrack, 'end of track');
    eq(m.openNotes, 0, 'every note is closed');
    eq(m.length, r.length, p.id + ': file is as long as the riff');
    const want = r.notes.map((n) => [n.t, n.len, n.pitch, Math.max(1, Math.round(n.vel * 127))])
      .sort((a, b) => a[0] - b[0] || a[2] - b[2]);
    const got = m.notes.map((n) => [n.t, n.len, n.pitch, n.vel]).sort((a, b) => a[0] - b[0] || a[2] - b[2]);
    eq(got, want, p.id);
  });
});

test('MIDI: a repeated note retriggers (note-off comes first)', () => {
  const riff = { ppq: 96, length: 192, notes: [
    { t: 0, len: 96, pitch: 60, vel: 1, pan: 0 }, { t: 96, len: 96, pitch: 60, vel: 1, pan: 0 }] };
  const m = readMidi(R.toMidi(riff, { bpm: 100 }));
  eq(m.notes.map((n) => [n.t, n.len]), [[0, 96], [96, 96]]);
});

console.log('\n' + passed + ' passed, ' + failures.length + ' failed');
if (failures.length) {
  failures.forEach((f) => console.log('  - ' + f));
  process.exit(1);
}
