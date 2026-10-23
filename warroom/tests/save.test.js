import { test, assert, eq, deepEq, throws, done } from './harness.js';
import { fixtureData } from './fixture.js';
import { newGame } from '../js/sim/state.js';
import { advance } from '../js/sim/tick.js';
import { toSave, fromSave, SAVE_VERSION } from '../js/sim/save.js';

const data = fixtureData();

test('a save round-trips exactly', () => {
  const s = newGame(data, { seed: 'rt' });
  advance(s, data, 24 * 40);
  const back = fromSave(JSON.parse(JSON.stringify(toSave(s))), data);
  deepEq(back, s);
});

test('a save from a newer game is refused with a clear message', () => {
  const s = newGame(data, {});
  const save = toSave(s);
  save.version = SAVE_VERSION + 1;
  throws(() => fromSave(save, data), /newer/);
});

test('garbage is refused', () => {
  throws(() => fromSave({ hello: 1 }, data), /not a War Room save/);
  throws(() => fromSave(null, data), /not a War Room save/);
});

test('a save that names a state the map no longer has is refused', () => {
  const s = newGame(data, {});
  s.states[999] = { owner: 'RED' };
  throws(() => fromSave(toSave(s), data), /unknown state 999/);
});

done('save');
