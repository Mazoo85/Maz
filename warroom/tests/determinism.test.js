import { test, assert, eq, done } from './harness.js';
import { fixtureData } from './fixture.js';
import { newGame } from '../js/sim/state.js';
import { advance } from '../js/sim/tick.js';
import { applyCommand } from '../js/sim/commands.js';

const data = fixtureData();

test('two games with the same seed and commands end identical', () => {
  const a = newGame(data, { seed: 'same' }), b = newGame(data, { seed: 'same' });
  for (const s of [a, b]) {
    advance(s, data, 24 * 100);
    applyCommand(s, { type: 'set_speed', speed: 2 });
    advance(s, data, 24 * 265);
  }
  eq(JSON.stringify(a), JSON.stringify(b));
  eq(a.hour, 24 * 365);
});

test('a different seed gives different weather', () => {
  const a = newGame(data, { seed: 'one' }), b = newGame(data, { seed: 'two' });
  advance(a, data, 24 * 365); advance(b, data, 24 * 365);
  assert(JSON.stringify(a.regions) !== JSON.stringify(b.regions) || JSON.stringify(a.rng) !== JSON.stringify(b.rng));
});

test('advancing in one go equals advancing in pieces', () => {
  const a = newGame(data, { seed: 'pieces' }), b = newGame(data, { seed: 'pieces' });
  advance(a, data, 24 * 60);
  for (let i = 0; i < 60; i++) advance(b, data, 24);
  eq(JSON.stringify(a), JSON.stringify(b));
});

done('determinism');
