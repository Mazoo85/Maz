import { test, assert, eq, throws, done } from './harness.js';
import { fixtureData } from './fixture.js';
import { newGame } from '../js/sim/state.js';
import { validateCommand, applyCommand } from '../js/sim/commands.js';

const data = fixtureData();

test('set_speed validates its range', () => {
  const s = newGame(data, {});
  eq(validateCommand(s, { type: 'set_speed', speed: 3 }), null);
  assert(validateCommand(s, { type: 'set_speed', speed: 9 }));
  assert(validateCommand(s, { type: 'set_speed', speed: -1 }));
  assert(validateCommand(s, { type: 'set_speed', speed: 1.5 }));
});

test('set_speed, pause and resume change the speed and remember the last one', () => {
  const s = newGame(data, {});
  applyCommand(s, { type: 'set_speed', speed: 4 });
  eq(s.speed, 4);
  applyCommand(s, { type: 'pause' });
  eq(s.speed, 0);
  applyCommand(s, { type: 'resume' });
  eq(s.speed, 4);
  applyCommand(s, { type: 'resume', speed: 2 });
  eq(s.speed, 2);
});

test('select_country needs a real tag or null', () => {
  const s = newGame(data, {});
  assert(validateCommand(s, { type: 'select_country', tag: 'XXX' }));
  applyCommand(s, { type: 'select_country', tag: 'BLU' });
  eq(s.player, 'BLU');
  applyCommand(s, { type: 'select_country', tag: null });
  eq(s.player, null);
});

test('unknown and malformed commands are refused and apply throws', () => {
  const s = newGame(data, {});
  assert(validateCommand(s, { type: 'launch_nukes' }));
  assert(validateCommand(s, null));
  throws(() => applyCommand(s, { type: 'set_speed', speed: 42 }), /refused/);
});

test('applied commands are logged with the hour', () => {
  const s = newGame(data, {});
  s.hour = 77;
  applyCommand(s, { type: 'set_speed', speed: 1 });
  eq(s.log.length, 1);
  eq(s.log[0].hour, 77);
  eq(s.log[0].cmd.type, 'set_speed');
});

done('commands');
