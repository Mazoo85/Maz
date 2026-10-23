import { test, assert, eq, done } from './harness.js';
import { seedRng, seedStream, nextU32, nextFloat, nextInt, pick, pickWeighted } from '../js/sim/rng.js';

test('same seed gives the same sequence', () => {
  const a = seedRng('war room'), b = seedRng('war room');
  for (let i = 0; i < 100; i++) eq(nextU32(a), nextU32(b));
});

test('different seeds differ', () => {
  const a = seedRng('a'), b = seedRng('b');
  let same = 0;
  for (let i = 0; i < 20; i++) if (nextU32(a) === nextU32(b)) same++;
  assert(same < 2, 'sequences should not match');
});

test('named streams differ from each other and from the base seed', () => {
  const base = seedRng(7), w = seedStream(7, 'weather'), e = seedStream(7, 'events');
  assert(nextU32(base) !== nextU32(w));
  assert(nextU32(w) !== nextU32(e));
});

test('nextFloat stays in [0, 1)', () => {
  const s = seedRng(1);
  for (let i = 0; i < 10000; i++) { const f = nextFloat(s); assert(f >= 0 && f < 1, 'out of range ' + f); }
});

test('nextInt(s, 6) is roughly uniform', () => {
  const s = seedRng('dice');
  const counts = [0, 0, 0, 0, 0, 0];
  for (let i = 0; i < 6000; i++) counts[nextInt(s, 6)]++;
  for (const c of counts) assert(c > 850 && c < 1150, 'face count ' + c);
});

test('pick returns members and pickWeighted respects weights', () => {
  const s = seedRng('pick');
  const arr = ['x', 'y', 'z'];
  for (let i = 0; i < 50; i++) assert(arr.includes(pick(s, arr)));
  let heavy = 0;
  for (let i = 0; i < 1000; i++) if (pickWeighted(s, [1, 9]) === 1) heavy++;
  assert(heavy > 850 && heavy < 950, 'weighted count ' + heavy);
  eq(pickWeighted(s, [0, 0, 1]), 2);
});

test('the rng state is plain JSON', () => {
  const s = seedRng('json');
  nextU32(s);
  const copy = JSON.parse(JSON.stringify(s));
  eq(nextU32(copy), nextU32(s));
});

done('rng');
