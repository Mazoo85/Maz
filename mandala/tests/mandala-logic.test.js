/*
 * MANDALA MAKER — logic tests.
 *
 *   node mandala/tests/mandala-logic.test.js
 *
 * No dependencies and no browser: the engine is a plain module. What these
 * prove is the promise the app makes — one line really is repeated evenly
 * round the centre, a seed always gives the same mandala, and a saved drawing
 * comes back exactly as it went in.
 */
'use strict';

const path = require('path');
const M = require(path.join(__dirname, '..', 'js', 'mandala.js'));

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

function assert(cond, message) {
  if (!cond) throw new Error(message || 'assertion failed');
}

function eq(actual, expected, message) {
  if (actual !== expected) {
    throw new Error((message || 'values differ') + ': got ' + JSON.stringify(actual) +
      ', expected ' + JSON.stringify(expected));
  }
}

function near(a, b, message) {
  if (Math.abs(a - b) > 1e-9) throw new Error((message || 'not close') + ': ' + a + ' vs ' + b);
}

console.log('\nMANDALA MAKER — logic');

test('one point becomes one copy per repeat', () => {
  for (const n of [2, 3, 8, 12, 32]) {
    eq(M.symmetryPoints(0.5, 0.1, n, false).length, n, n + ' repeats');
  }
});

test('mirror doubles the copies', () => {
  eq(M.symmetryPoints(0.5, 0.1, 8, true).length, 16);
});

test('the first copy is the point itself', () => {
  const p = M.symmetryPoints(0.3, -0.4, 6, true)[0];
  near(p[0], 0.3);
  near(p[1], -0.4);
});

test('every copy sits the same distance from the centre', () => {
  const r = Math.hypot(0.6, 0.2);
  for (const p of M.symmetryPoints(0.6, 0.2, 12, true)) near(Math.hypot(p[0], p[1]), r);
});

test('copies are spaced evenly round the circle', () => {
  const n = 7;
  const pts = M.symmetryPoints(1, 0, n, false);
  for (let k = 0; k < n; k++) {
    near(pts[k][0], Math.cos((2 * Math.PI * k) / n));
    near(pts[k][1], Math.sin((2 * Math.PI * k) / n));
  }
});

test('a mirrored copy is the reflection across the slice edge', () => {
  const pts = M.symmetryPoints(0.5, 0.2, 4, true);
  // Copy 1 is the mirror of the original across the x axis.
  near(pts[1][0], 0.5);
  near(pts[1][1], -0.2);
});

test('repeats are clamped to the slider range', () => {
  eq(M.clampSlices(0), M.MIN_SLICES);
  eq(M.clampSlices(500), M.MAX_SLICES);
  eq(M.clampSlices('9'), 9);
  eq(M.clampSlices(undefined), M.MIN_SLICES);
});

test('a stroke becomes one path per copy, each as long as the stroke', () => {
  const paths = M.symmetryPaths([[0.1, 0], [0.2, 0.1], [0.3, 0.1]], 5, true);
  eq(paths.length, 10);
  for (const p of paths) eq(p.length, 3);
});

test('unit space round-trips through canvas pixels', () => {
  const u = M.toUnit(700, 250, 500, 500, 480);
  const px = M.fromUnit(u[0], u[1], 500, 500, 480);
  near(px[0], 700);
  near(px[1], 250);
});

test('a rainbow stroke changes colour along its length, a plain one does not', () => {
  const rainbow = M.makeStroke({ color: 'rainbow', hue: 10 });
  assert(M.colorAt(rainbow, 0) !== M.colorAt(rainbow, 5), 'rainbow should change');
  const plain = M.makeStroke({ color: '#ff0000' });
  eq(M.colorAt(plain, 0), '#ff0000');
  eq(M.colorAt(plain, 9), '#ff0000');
});

test('the same seed always makes the same mandala', () => {
  eq(JSON.stringify(M.generate(12345)), JSON.stringify(M.generate(12345)));
});

test('different seeds make different mandalas', () => {
  const seen = new Set();
  for (let s = 1; s <= 30; s++) seen.add(JSON.stringify(M.generate(s).strokes));
  assert(seen.size >= 28, 'only ' + seen.size + ' distinct designs from 30 seeds');
});

test('a generated mandala stays inside the circle, over many seeds', () => {
  for (let s = 0; s < 300; s++) {
    const m = M.generate(s * 7919);
    for (const st of m.strokes) {
      for (const p of st.points) {
        assert(Number.isFinite(p[0]) && Number.isFinite(p[1]), 'seed ' + s + ' has a bad point');
        assert(Math.hypot(p[0], p[1]) <= 1, 'seed ' + s + ' leaves the circle');
      }
    }
  }
});

test('the generator follows the chosen repeats, palette and mirror', () => {
  const m = M.generate(42, { slices: 10, palette: 'ocean', mirror: false });
  eq(m.slices, 10);
  eq(m.palette, 'ocean');
  eq(m.background, M.paletteById('ocean').bg);
  for (const st of m.strokes) {
    eq(st.slices, 10);
    eq(st.mirror, false);
  }
});

test('every generated mandala has a centre and several rings', () => {
  for (let s = 0; s < 50; s++) {
    const m = M.generate(s);
    assert(m.strokes.length >= 6, 'seed ' + s + ' made only ' + m.strokes.length + ' strokes');
    eq(JSON.stringify(m.strokes[0].points), '[[0,0]]', 'seed ' + s + ' centre');
  }
});

test('every palette has a background and six colours', () => {
  const ids = new Set();
  for (const p of M.PALETTES) {
    assert(/^#[0-9a-f]{6}$/i.test(p.bg), p.id + ' background');
    eq(p.colors.length, 6, p.id + ' colours');
    ids.add(p.id);
  }
  eq(ids.size, M.PALETTES.length, 'palette ids are unique');
});

test('a saved drawing loads back the same', () => {
  const m = M.generate(777);
  const back = M.deserialize(M.serialize(m));
  eq(back.background, m.background);
  eq(back.strokes.length, m.strokes.length);
  for (let i = 0; i < m.strokes.length; i++) {
    const a = m.strokes[i];
    const b = back.strokes[i];
    eq(b.color, a.color);
    eq(b.slices, a.slices);
    eq(b.mirror, a.mirror);
    eq(b.points.length, a.points.length);
    for (let j = 0; j < a.points.length; j++) {
      assert(Math.abs(a.points[j][0] - b.points[j][0]) <= 0.0005, 'x drifted');
      assert(Math.abs(a.points[j][1] - b.points[j][1]) <= 0.0005, 'y drifted');
    }
  }
});

test('a broken save gives null, not a crash', () => {
  eq(M.deserialize('not json'), null);
  eq(M.deserialize('{}'), null);
  eq(M.deserialize(JSON.stringify({ format: 'something-else', strokes: [] })), null);
});

test('junk strokes inside a save are skipped', () => {
  const text = JSON.stringify({
    format: 'maz-mandala/1',
    background: '#000000',
    strokes: [null, { points: 'nope' }, { points: [] }, { color: '#fff', points: [[0.1, 0.2], ['x', 1]] }]
  });
  const back = M.deserialize(text);
  eq(back.strokes.length, 1);
  eq(back.strokes[0].points.length, 1);
});

console.log('\n' + passed + ' passed, ' + failures.length + ' failed');
if (failures.length) {
  for (const f of failures) console.log('  - ' + f);
  process.exit(1);
}
