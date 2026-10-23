import { test, assert, eq, deepEq, throws, done } from './harness.js';
import { fixtureData } from './fixture.js';
import { newGame, provinceController } from '../js/sim/state.js';

const data = fixtureData();

test('newGame builds every country with its states', () => {
  const s = newGame(data, { seed: 'x' });
  deepEq(Object.keys(s.countries).sort(), ['BLU', 'GRN', 'RED']);
  deepEq(s.countries.RED.states, [1, 2]);
  deepEq(s.countries.BLU.states, [3]);
  eq(s.states[2].controller, 'RED');
  deepEq(s.states[2].cores, ['RED', 'BLU']);
  eq(s.hour, 0); eq(s.speed, 0); eq(s.player, null);
});

test('regions start clear and armies come from the scenario', () => {
  const s = newGame(data, { seed: 'x' });
  eq(s.regions[1].weather, 'clear');
  eq(s.land.armies.RED[0].divisions, 12);
});

test('provinceController follows the state controller; sea has none', () => {
  const s = newGame(data, { seed: 'x' });
  eq(provinceController(s, data, 3), 'RED');
  s.states[2].controller = 'BLU';
  eq(provinceController(s, data, 3), 'BLU');
  eq(provinceController(s, data, 9), null);
});

test('the state is plain JSON', () => {
  const s = newGame(data, { seed: 'x' });
  deepEq(JSON.parse(JSON.stringify(s)), s);
});

test('an unknown scenario is refused', () => {
  throws(() => newGame(data, { scenario: '1066' }), /unknown scenario/);
});

done('state');
