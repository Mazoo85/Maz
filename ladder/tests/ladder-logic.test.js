// Ladder 24 tests: plain Node, no dependencies. Run: node ladder/tests/ladder-logic.test.js
'use strict';
const assert = require('assert');
const path = require('path');
const DSP = require(path.join(__dirname, '../js/dsp.js'));
const L = require(path.join(__dirname, '../js/logic.js'));

const SR = 48000;
let passed = 0;
function test(name, fn) {
  try {
    fn();
    passed++;
    console.log('  ok  ' + name);
  } catch (e) {
    console.error('FAIL  ' + name);
    console.error(e);
    process.exitCode = 1;
  }
}

function synth(changes) {
  return new DSP.Synth(SR, L.engineParams(Object.assign(L.defaults(), changes || {})));
}

// Renders seconds of sound in 128-sample blocks, like the browser does.
function render(s, seconds) {
  const n = Math.round(SR * seconds);
  const L_ = new Float32Array(n);
  const R_ = new Float32Array(n);
  for (let i = 0; i < n; i += 128) {
    const m = Math.min(128, n - i);
    s.render(L_.subarray(i, i + m), R_.subarray(i, i + m), m);
  }
  return { L: L_, R: R_ };
}

function rms(a, from, to) {
  let sum = 0;
  from = from || 0;
  to = to || a.length;
  for (let i = from; i < to; i++) sum += a[i] * a[i];
  return Math.sqrt(sum / (to - from));
}

// Frequency from rising zero crossings, after a DC block.
function pitch(a, from) {
  let prev = 0;
  let mean = 0;
  for (let i = from; i < a.length; i++) mean += a[i];
  mean /= a.length - from;
  const cross = [];
  for (let i = from + 1; i < a.length; i++) {
    const x = a[i] - mean;
    if (prev < 0 && x >= 0) cross.push(i - 1 + -prev / (x - prev));
    prev = x;
  }
  return (SR * (cross.length - 1)) / (cross[cross.length - 1] - cross[0]);
}

console.log('Ladder 24');

test('midiToFreq: A4 is 440 Hz and octaves double', () => {
  assert.ok(Math.abs(DSP.midiToFreq(69) - 440) < 1e-9);
  assert.ok(Math.abs(DSP.midiToFreq(81) - 880) < 1e-9);
  assert.ok(Math.abs(DSP.midiToFreq(60) - 261.6256) < 1e-3);
});

test('every waveform stays within ±1.6 and has no DC drift', () => {
  for (const w of L.WAVES) {
    const o = new DSP.Oscillator(0);
    let min = 9;
    let max = -9;
    let sum = 0;
    const n = SR;
    for (let i = 0; i < n; i++) {
      const y = o.next(441, SR, w, 0.2);
      min = Math.min(min, y);
      max = Math.max(max, y);
      sum += y;
    }
    assert.ok(max <= 1.6 && min >= -1.6, w + ' range ' + min + '..' + max);
    assert.ok(Math.abs(sum / n) < 0.02, w + ' DC ' + sum / n);
  }
});

test('a held note sounds at its pitch', () => {
  const s = synth({ cutoff: 18000, res: 0, m2: 0, o1Wave: 'saw', fEnvAmt: 0, delayMix: 0 });
  s.noteOn(57, 1); // A3
  const out = render(s, 0.5);
  const f = pitch(out.L, SR * 0.1);
  assert.ok(Math.abs(f - 220) < 1, 'heard ' + f);
});

test('osc range switch moves an octave per step', () => {
  const s = synth({ cutoff: 18000, res: 0, m2: 0, o1Range: 1, o1Wave: 'tri', fEnvAmt: 0 });
  s.noteOn(57, 1);
  const f = pitch(render(s, 0.5).L, SR * 0.1);
  assert.ok(Math.abs(f - 440) < 2, 'heard ' + f);
});

test('the note dies away after release', () => {
  const s = synth({ aR: 0.05 });
  s.noteOn(60, 1);
  const a = render(s, 0.3);
  s.noteOff(60);
  const b = render(s, 0.5);
  assert.ok(rms(a.L, SR * 0.1) > 0.01, 'loud while held');
  assert.ok(rms(b.L, SR * 0.4) < 1e-4, 'silent after release');
  assert.strictEqual(s.activeMask(), 0);
});

test('the ladder filter cuts high notes far more than low ones (24 dB/oct)', () => {
  const lo = new DSP.LadderFilter();
  const hi = new DSP.LadderFilter();
  let eLo = 0;
  let eHi = 0;
  for (let i = 0; i < SR; i++) {
    const a = lo.process(0.1 * Math.sin((2 * Math.PI * 100 * i) / SR), 500, 0, SR);
    const b = hi.process(0.1 * Math.sin((2 * Math.PI * 4000 * i) / SR), 500, 0, SR);
    if (i > SR / 2) {
      eLo += a * a;
      eHi += b * b;
    }
  }
  const db = 10 * Math.log10(eHi / eLo);
  // Three octaves above the cutoff, a four-pole filter is ~ -72 dB.
  assert.ok(db < -60, 'only ' + db.toFixed(1) + ' dB');
});

test('at full emphasis the filter rings by itself at the cutoff', () => {
  const f = new DSP.LadderFilter();
  const out = new Float32Array(SR);
  for (let i = 0; i < SR; i++) out[i] = f.process(i === 0 ? 0.5 : 0, 1000, 4.15, SR);
  const level = rms(out, SR / 2);
  const freq = pitch(out, SR / 2);
  assert.ok(level > 0.05, 'level ' + level);
  assert.ok(Math.abs(freq - 1000) < 40, 'rings at ' + freq);
});

test('nothing blows up at the extremes of every knob', () => {
  const extreme = {};
  for (const p of L.PARAMS) if (p.type === 'knob') extreme[p.id] = p.max;
  for (const set of [extreme, { cutoff: 20, res: 1, drive: 1, noise: 1 }]) {
    const s = synth(Object.assign({ voiceMode: 'unison' }, set));
    s.noteOn(108, 1);
    s.noteOn(21, 1);
    const out = render(s, 0.3);
    for (const x of out.L) assert.ok(Number.isFinite(x) && Math.abs(x) < 4, 'bad sample ' + x);
  }
});

test('poly plays up to 8 notes and steals the oldest after that', () => {
  const s = synth();
  for (let n = 0; n < 9; n++) s.noteOn(48 + n, 1);
  render(s, 0.05);
  const notes = s.voices.map((v) => v.note).sort((a, b) => a - b);
  assert.deepStrictEqual(notes, [49, 50, 51, 52, 53, 54, 55, 56]);
});

test('mono returns to the still-held note when the top one is let go', () => {
  const s = synth({ voiceMode: 'mono' });
  s.noteOn(60, 1);
  s.noteOn(67, 1);
  render(s, 0.02);
  assert.strictEqual(s.voices[0].target, 67);
  s.noteOff(67);
  assert.strictEqual(s.voices[0].target, 60);
  assert.ok(s.voices[0].gate);
  s.noteOff(60);
  assert.ok(!s.voices[0].gate);
});

test('glide slides the pitch instead of jumping', () => {
  const s = synth({ voiceMode: 'legato', glide: 0.5 });
  s.noteOn(48, 1);
  render(s, 0.05);
  s.noteOn(60, 1);
  render(s, 0.05);
  const mid = s.voices[0].cur;
  assert.ok(mid > 48.5 && mid < 59.5, 'pitch was ' + mid);
  render(s, 3);
  assert.ok(Math.abs(s.voices[0].cur - 60) < 0.01);
});

test('unison stacks five detuned voices on one key', () => {
  const s = synth({ voiceMode: 'unison', unisonDetune: 20 });
  s.noteOn(60, 1);
  render(s, 0.01);
  const on = s.voices.filter((v) => v.gate);
  assert.strictEqual(on.length, DSP.UNISON);
  assert.strictEqual(Math.min(...on.map((v) => v.detune)), -20);
  assert.strictEqual(Math.max(...on.map((v) => v.detune)), 20);
});

test('arpSequence orders', () => {
  assert.deepStrictEqual(DSP.arpSequence([64, 60, 67], 'up', 1), [60, 64, 67]);
  assert.deepStrictEqual(DSP.arpSequence([64, 60, 67], 'down', 1), [67, 64, 60]);
  assert.deepStrictEqual(DSP.arpSequence([64, 60, 67], 'order', 1), [64, 60, 67]);
  assert.deepStrictEqual(DSP.arpSequence([60, 64], 'up', 2), [60, 64, 72, 76]);
  assert.deepStrictEqual(DSP.arpSequence([60, 64, 67], 'updown', 1), [60, 64, 67, 64]);
  assert.deepStrictEqual(DSP.arpSequence([], 'up', 2), []);
});

test('the arpeggiator steps in time with the tempo', () => {
  const s = synth({ arpOn: true, arpBpm: 120, arpDiv: '1/16', arpOct: 1, arpMode: 'up' });
  s.noteOn(60, 1);
  s.noteOn(64, 1);
  s.noteOn(67, 1);
  const heard = [];
  const block = new Float32Array(128);
  for (let i = 0; i < Math.round((SR * 0.5) / 128); i++) {
    s.render(block, block, 128);
    if (s.arpNote >= 0 && heard[heard.length - 1] !== s.arpNote) heard.push(s.arpNote);
  }
  // 120 bpm sixteenths = 8 steps a second, so 4 in half a second.
  assert.deepStrictEqual(heard.slice(0, 4), [60, 64, 67, 60]);
});

test('latch keeps the arpeggio going after the keys are let go', () => {
  const s = synth({ arpOn: true, latch: true });
  s.noteOn(60, 1);
  s.noteOff(60);
  render(s, 0.3);
  assert.deepStrictEqual(s.arpHeld, [60]);
  s.setParams({ latch: false });
  assert.deepStrictEqual(s.arpHeld, []);
});

test('knob curves round-trip', () => {
  for (const p of L.PARAMS) {
    if (p.type !== 'knob') continue;
    for (const n of [0, 0.25, 0.5, 0.75, 1]) {
      const v = L.fromNorm(p, n);
      assert.ok(v >= p.min && v <= p.max, p.id);
      if (!p.step) assert.ok(Math.abs(L.toNorm(p, v) - n) < 1e-9, p.id + ' at ' + n);
    }
    assert.ok(p.def >= p.min && p.def <= p.max, p.id + ' default out of range');
  }
});

test('panel readouts', () => {
  assert.strictEqual(L.formatParam('cutoff', 1200), '1.20 kHz');
  assert.strictEqual(L.formatParam('cutoff', 440), '440 Hz');
  assert.strictEqual(L.formatParam('o1Range', -2), "32'");
  assert.strictEqual(L.formatParam('o1Range', 0), "8'");
  assert.strictEqual(L.formatParam('aA', 0.05), '50 ms');
  assert.strictEqual(L.formatParam('fEnvAmt', -2), '−2.0 oct');
  assert.strictEqual(L.formatParam('glide', 0), 'Off');
  assert.strictEqual(L.formatParam('voiceMode', 'unison'), 'Unison');
});

test('patch codes round-trip and reject junk with a clear message', () => {
  const p = Object.assign(L.defaults(), { cutoff: 333.3, voiceMode: 'mono', arpOn: true });
  const code = L.encodePatch('My Bass ♪', p);
  const back = L.decodePatch(code);
  assert.strictEqual(back.name, 'My Bass ♪');
  assert.strictEqual(back.params.cutoff, 333.3);
  assert.strictEqual(back.params.voiceMode, 'mono');
  assert.strictEqual(back.params.arpOn, true);
  assert.throws(() => L.decodePatch('hello'), /not a Ladder 24 patch/);
  assert.throws(() => L.decodePatch('L24:@@@'), /damaged/);
});

test('sanitizePatch clamps, drops unknowns, and fixes bad choices', () => {
  const p = L.sanitizePatch({ cutoff: 99999, res: -3, o1Wave: 'kazoo', bogus: 1, arpBpm: 'x' });
  assert.strictEqual(p.cutoff, 18000);
  assert.strictEqual(p.res, 0);
  assert.strictEqual(p.o1Wave, 'saw');
  assert.strictEqual(p.arpBpm, 120);
  assert.ok(!('bogus' in p));
});

test('every factory patch is valid and makes sound', () => {
  const names = new Set();
  for (const pr of L.PRESETS) {
    assert.ok(!names.has(pr.name), 'duplicate ' + pr.name);
    names.add(pr.name);
    assert.deepStrictEqual(L.sanitizePatch(pr.params), pr.params, pr.name);
    const s = new DSP.Synth(SR, L.engineParams(pr.params));
    s.noteOn(48, 0.9);
    s.noteOn(55, 0.9);
    const out = render(s, 0.6);
    const level = rms(out.L) + rms(out.R);
    assert.ok(level > 0.003, pr.name + ' is silent (' + level + ')');
    for (const x of out.L) assert.ok(Number.isFinite(x), pr.name + ' made a bad sample');
  }
});

test('page messages drive the engine the same way on either thread', () => {
  const s = synth();
  s.handle({ type: 'on', note: 60, vel: 1 });
  s.handle({ type: 'params', params: { voiceMode: 'mono' } });
  assert.strictEqual(s.p.voiceMode, 'mono');
  s.handle({ type: 'on', note: 64, vel: 1 });
  s.handle({ type: 'bend', value: 5 });
  assert.strictEqual(s.bend, 1);
  s.handle({ type: 'panic' });
  render(s, 0.01);
  assert.strictEqual(s.activeMask(), 0);
});

test('the computer keyboard covers an octave and a half from C', () => {
  assert.strictEqual(L.KEYMAP.a, 0);
  assert.strictEqual(L.KEYMAP.k, 12);
  assert.strictEqual(L.noteName(60), 'C4');
  assert.ok(L.isBlack(61) && !L.isBlack(64));
});

console.log(passed + ' passed');
