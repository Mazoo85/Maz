/*
 * WAR ROOM 1936 — test harness. Plain Node, no dependencies.
 *
 *   node warroom/tests/<name>.test.js
 *
 * Same shape as earth/tests: test() runs a case, done() prints the summary and
 * sets the exit code so CI fails on any failure.
 */
let passed = 0;
const failures = [];

export function test(name, fn) {
  try {
    fn();
    passed++;
    console.log('  ok   ' + name);
  } catch (e) {
    failures.push(name + ' — ' + e.message);
    console.log('  FAIL ' + name + ' — ' + e.message);
  }
}

export async function testAsync(name, fn) {
  try {
    await fn();
    passed++;
    console.log('  ok   ' + name);
  } catch (e) {
    failures.push(name + ' — ' + e.message);
    console.log('  FAIL ' + name + ' — ' + e.message);
  }
}

export function assert(cond, message) {
  if (!cond) throw new Error(message || 'assertion failed');
}

export function eq(actual, expected, message) {
  if (actual !== expected) {
    throw new Error((message || 'values differ') + ': got ' + JSON.stringify(actual) +
      ', expected ' + JSON.stringify(expected));
  }
}

export function deepEq(actual, expected, message) {
  const a = JSON.stringify(actual), b = JSON.stringify(expected);
  if (a !== b) {
    throw new Error((message || 'values differ') + ': got ' + a.slice(0, 200) +
      ', expected ' + b.slice(0, 200));
  }
}

export function near(actual, expected, tolerance, message) {
  if (Math.abs(actual - expected) > tolerance) {
    throw new Error((message || 'values differ') + ': got ' + actual + ', expected ' +
      expected + ' ± ' + tolerance);
  }
}

export function throws(fn, pattern, message) {
  try { fn(); } catch (e) {
    if (pattern && !pattern.test(e.message)) {
      throw new Error((message || 'wrong error') + ': ' + e.message);
    }
    return;
  }
  throw new Error((message || 'expected an error') + ' but none was thrown');
}

export function done(label) {
  console.log('\n' + (label || 'tests') + ': ' + passed + ' passed, ' + failures.length + ' failed');
  if (failures.length) {
    for (const f of failures) console.log('  - ' + f);
    process.exitCode = 1;
  }
}
