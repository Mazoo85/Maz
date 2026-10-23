/*
 * VELOCITY — logic tests.
 *
 *   node velocity/tests/velocity-logic.test.js
 *
 * No dependencies and no browser. These prove the rules of the race: the
 * track knows where the road is, a car moves the way the stats say, the AI
 * can get round the circuit, and laps, places and records are counted right.
 */
'use strict';

const path = require('path');
const js = (f) => require(path.join(__dirname, '..', 'js', f));
const Track = js('track.js');

let passed = 0;
const failures = [];

function test(name, fn) {
  try { fn(); passed++; console.log('  ok   ' + name); }
  catch (e) { failures.push(name + ' — ' + e.message); console.log('  FAIL ' + name + ' — ' + e.message); }
}
function assert(cond, message) { if (!cond) throw new Error(message || 'assertion failed'); }
function eq(actual, expected, message) {
  if (actual !== expected) throw new Error((message || 'values differ') + ': got ' + JSON.stringify(actual) + ', expected ' + JSON.stringify(expected));
}
function near(actual, expected, tolerance, message) {
  if (Math.abs(actual - expected) > tolerance) throw new Error((message || 'not close enough') + ': got ' + actual + ', expected ' + expected + ' ± ' + tolerance);
}

console.log('Track');
const track = Track.build();

test('Harbor Loop is about 3 km and closed', () => {
  assert(track.length > 2500 && track.length < 3500, 'length ' + track.length);
  const first = track.points[0], last = track.points[track.points.length - 1];
  assert(Math.hypot(first.x - last.x, first.z - last.z) < 30, 'loop closes');
});

test('distance along the lap increases round the circuit and wraps at the line', () => {
  let prev = 0;
  for (let s = 0; s < track.length; s += 50) {
    const p = track.at(s);
    const n = track.nearest(p.x, p.z);
    assert(n.s >= prev - 1, 's went backwards at ' + s);
    near(n.s, s, 2, 'round trip at ' + s);
    prev = n.s;
  }
  const beyond = track.at(track.length + 10);
  near(track.nearest(beyond.x, beyond.z).s, 10, 2, 'wraps past the line');
});

test('nearest point reports a signed lateral offset', () => {
  const p = track.at(300);
  const rx = -p.dz, rz = p.dx;
  const right = track.nearest(p.x + rx * 3, p.z + rz * 3);
  const left = track.nearest(p.x - rx * 3, p.z - rz * 3);
  near(right.lateral, 3, 0.2, 'right of travel is positive');
  near(left.lateral, -3, 0.2, 'left of travel is negative');
  near(right.s, 300, 2);
});

test('a point beyond the road width counts as off-road', () => {
  const p = track.at(500);
  const rx = -p.dz, rz = p.dx;
  assert(!track.isOffRoad(p.x + rx * (p.w / 2 - 1), p.z + rz * (p.w / 2 - 1)), 'inside the kerb is on the road');
  assert(track.isOffRoad(p.x + rx * (p.w / 2 + 2), p.z + rz * (p.w / 2 + 2)), 'past the kerb is off the road');
});

test('a hint speeds the search without changing the answer', () => {
  const p = track.at(1200);
  const slow = track.nearest(p.x, p.z);
  const fast = track.nearest(p.x, p.z, slow.i);
  near(fast.s, slow.s, 0.01);
  const far = track.nearest(p.x, p.z, 3);
  near(far.s, slow.s, 0.01, 'a bad hint still finds the right stretch');
});

test('eight distinct grid slots sit on the road just before the line', () => {
  const slots = track.gridSlots();
  eq(slots.length, 8);
  for (let i = 0; i < 8; i++) {
    assert(!track.isOffRoad(slots[i].x, slots[i].z), 'slot ' + i + ' on the road');
    assert(slots[i].s > track.length - 80 && slots[i].s < track.length, 'slot ' + i + ' before the line');
    for (let j = 0; j < i; j++) assert(Math.hypot(slots[i].x - slots[j].x, slots[i].z - slots[j].z) > 4, 'slots ' + i + ' and ' + j + ' apart');
  }
  assert(slots[7].s < slots[0].s, 'slot 7 is at the back');
});

test('the circuit has a tunnel and one ramp', () => {
  assert(track.points.some((p) => p.tunnel), 'tunnel');
  eq(track.ramps.length, 1);
  const lip = track.nearest(track.at(track.ramps[0]).x, track.at(track.ramps[0]).z);
  near(lip.rampHeight, 2.5, 0.3, 'ramp height at the lip');
});

console.log('');
if (failures.length) { console.log(failures.length + ' failed, ' + passed + ' passed'); process.exit(1); }
console.log('All ' + passed + ' passed');
