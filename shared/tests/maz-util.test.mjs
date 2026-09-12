#!/usr/bin/env node
/*
 * Tests for shared/maz-util.js.
 *
 * These four helpers are now loaded by five projects, so a mistake here breaks
 * SCRIPT FORGE, SONG FORGE, CODA PICS and ZOMBOID at once. That is the trade
 * made by sharing them, and this file is the other half of it.
 *
 *   node shared/tests/maz-util.test.mjs
 */

import { createRequire } from 'node:module';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import assert from 'node:assert';

const require = createRequire(import.meta.url);
const HERE = dirname(fileURLToPath(import.meta.url));
const U = require(join(HERE, '..', 'maz-util.js'));

let passed = 0;
let failed = 0;

function test(name, fn) {
  try {
    fn();
    passed += 1;
    console.log(`  ok  ${name}`);
  } catch (err) {
    failed += 1;
    console.error(`  FAIL ${name}\n       ${err.message}`);
  }
}

console.log('shared/maz-util');

test('clamp holds a value inside the range', () => {
  assert.strictEqual(U.clamp(5, 0, 3), 3);
  assert.strictEqual(U.clamp(-5, 0, 3), 0);
  assert.strictEqual(U.clamp(2, 0, 3), 2);
});

test('clamp returns the bounds themselves unchanged', () => {
  assert.strictEqual(U.clamp(0, 0, 3), 0);
  assert.strictEqual(U.clamp(3, 0, 3), 3);
});

test('lerp travels from a to b', () => {
  assert.strictEqual(U.lerp(0, 10, 0), 0);
  assert.strictEqual(U.lerp(0, 10, 1), 10);
  assert.strictEqual(U.lerp(0, 10, 0.5), 5);
  assert.strictEqual(U.lerp(10, 0, 0.25), 7.5);
});

test('lerp extrapolates outside [0,1] rather than clamping', () => {
  // ZOMBOID's camera and CODA PICS' colour ramps both rely on this: clamping
  // here would silently flatten an overshoot instead of overshooting.
  assert.strictEqual(U.lerp(0, 10, 2), 20);
  assert.strictEqual(U.lerp(0, 10, -1), -10);
});

test('escapeHtml escapes all five characters that can break out of markup', () => {
  assert.strictEqual(
    U.escapeHtml(`<img src="x" onerror='y'>&`),
    '&lt;img src=&quot;x&quot; onerror=&#39;y&#39;&gt;&amp;'
  );
});

test('escapeHtml leaves ordinary text alone and accepts non-strings', () => {
  assert.strictEqual(U.escapeHtml('a plain title'), 'a plain title');
  assert.strictEqual(U.escapeHtml(42), '42');
  assert.strictEqual(U.escapeHtml(null), 'null');
});

test('pick maps the rng across the whole list', () => {
  const list = ['a', 'b', 'c', 'd'];
  assert.strictEqual(U.pick(list, () => 0), 'a');
  assert.strictEqual(U.pick(list, () => 0.26), 'b');
  assert.strictEqual(U.pick(list, () => 0.99), 'd');
});

test('pick survives an rng that returns exactly 1', () => {
  // Math.floor(1 * 4) is 4, one past the end. The modulo is what stops that
  // being undefined, and a generator that can return 1 is allowed to exist.
  assert.strictEqual(U.pick(['a', 'b', 'c', 'd'], () => 1), 'a');
});

test('pick is deterministic for a deterministic rng', () => {
  // Every project here rebuilds the same film, song or picture from the same
  // words. That only holds if pick asks the seed and nothing else.
  const seeded = () => { let s = 1; return () => (s = (s * 48271) % 2147483647) / 2147483647; };
  const a = seeded();
  const b = seeded();
  const list = ['one', 'two', 'three', 'four', 'five'];
  const runA = Array.from({ length: 20 }, () => U.pick(list, a));
  const runB = Array.from({ length: 20 }, () => U.pick(list, b));
  assert.deepStrictEqual(runA, runB);
});

test('the module loads the same way in a browser and in Node', () => {
  // Both halves of the UMD wrapper have to work: a page gets globalThis.MazUtil,
  // a test gets module.exports, and they must be the same object.
  assert.strictEqual(globalThis.MazUtil, U);
  for (const name of ['clamp', 'lerp', 'escapeHtml', 'pick']) {
    assert.strictEqual(typeof U[name], 'function', `${name} is missing from the API`);
  }
});

console.log(`\n${passed} passed, ${failed} failed`);
process.exitCode = failed ? 1 : 0;
