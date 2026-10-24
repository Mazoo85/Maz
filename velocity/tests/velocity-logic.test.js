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

const Physics = js('physics.js');

console.log('Physics');

// A straight, wide road with the same shape as a real track, for tests that
// should not be about cornering.
function straightRoad(width) {
  const w = width || 14;
  return {
    length: 1e6, ramps: [], RAMP_HEIGHT: 2.5,
    nearest: (x, z) => ({ i: 0, s: x, lateral: z, dx: 1, dz: 0, w, px: x, pz: 0, tunnel: false, curvature: 0, rampHeight: 0 }),
    at: (s) => ({ x: s, z: 0, w, dx: 1, dz: 0, curvature: 0, i: 0, tunnel: false }),
    isOffRoad: (x, z) => Math.abs(z) > w / 2,
  };
}
function drive(car, input, seconds, road, dt) {
  dt = dt || 1 / 60;
  for (let t = 0; t < seconds; t += dt) Physics.step(car, input, road || track, dt);
  return car;
}
function carAt(key, s, lateral) {
  const p = track.at(s);
  return Physics.createCar(key, { x: p.x - p.dz * (lateral || 0), z: p.z + p.dx * (lateral || 0), heading: Math.atan2(p.dz, p.dx), s, hint: p.i });
}
function carOnStraight(key, x) { return Physics.createCar(key, { x: x || 0, z: 0, heading: 0, s: x || 0, hint: 0 }); }
// Steer towards the centreline a little way ahead, like a sensible driver.
function aim(car) {
  const p = track.at(car.s + 15);
  const d = Math.atan2(p.z - car.z, p.x - car.x);
  return Math.max(-1, Math.min(1, Track.wrapAngle(d - car.heading) * 2));
}

test('a car accelerates on its own and settles at its top speed', () => {
  const car = drive(carOnStraight('comet'), { steer: 0 }, 60, straightRoad());
  const v = Physics.forwardSpeed(car);
  assert(v > Physics.CARS.comet.topSpeed * 0.97, 'reached top speed: ' + v);
  assert(v <= Physics.CARS.comet.topSpeed + 0.01, 'did not exceed it: ' + v);
});

test('steering turns the heading', () => {
  const car = drive(carOnStraight('comet'), { steer: 0 }, 3, straightRoad(40));
  const before = car.heading;
  drive(car, { steer: 1 }, 0.5, straightRoad(40));
  assert(Track.wrapAngle(car.heading - before) > 0.2, 'turned right');
});

test('the Dart out-accelerates the Brawler off the line, the Brawler has the higher top speed', () => {
  const dart = drive(carOnStraight('dart'), { steer: 0 }, 2, straightRoad());
  const brawler = drive(carOnStraight('brawler'), { steer: 0 }, 2, straightRoad());
  assert(Physics.forwardSpeed(dart) > Physics.forwardSpeed(brawler), 'dart quicker at 2 s');
  assert(Physics.CARS.brawler.topSpeed > Physics.CARS.dart.topSpeed);
});

test('drifting keeps more speed than braking and fills the nitro bar', () => {
  const wide = straightRoad(60);
  const start = () => drive(carOnStraight('comet'), { steer: 0 }, 4, wide);
  const drifting = start(); drifting.nitro = 0;
  const braking = start(); braking.nitro = 0;
  drive(drifting, { steer: 1, drift: true }, 0.5, wide);
  drive(braking, { steer: 0, drift: true }, 0.5, wide);
  assert(drifting.drifting, 'was drifting');
  assert(braking.braking, 'was braking');
  assert(Physics.forwardSpeed(drifting) > Physics.forwardSpeed(braking), 'drift kept speed');
  assert(drifting.nitro > 0.03, 'drift filled nitro: ' + drifting.nitro);
  eq(braking.nitro, 0, 'braking does not');
});

test('nitro pushes the car past its top speed and drains, then speed falls back', () => {
  const road = straightRoad();
  const car = drive(carOnStraight('comet'), { steer: 0 }, 30, road);
  car.nitro = 1;
  drive(car, { steer: 0, nitro: true }, 1.4, road);
  const boosted = Physics.forwardSpeed(car);
  assert(boosted > Physics.CARS.comet.topSpeed + 3, 'above top speed: ' + boosted);
  assert(car.nitro < 1 && car.nitro > 0, 'drained some: ' + car.nitro);
  drive(car, { steer: 0 }, 10, road);
  assert(Physics.forwardSpeed(car) <= Physics.CARS.comet.topSpeed + 0.01, 'back to top speed');
});

test('a single tap of nitro burns for a moment and then stops', () => {
  const road = straightRoad();
  const car = drive(carOnStraight('comet'), { steer: 0 }, 5, road);
  car.nitro = 1;
  Physics.step(car, { steer: 0, nitro: true }, road, 1 / 60);
  assert(car.nitroActive, 'active after the tap');
  drive(car, { steer: 0 }, 1, road);
  assert(car.nitroActive, 'still burning a second later');
  drive(car, { steer: 0 }, 1, road);
  assert(!car.nitroActive, 'stopped after the burn');
  assert(car.nitro > 0.3, 'bar not emptied by one tap');
});

test('the ramp launches the car, it cannot steer in the air, then it lands', () => {
  const car = carAt('comet', track.ramps[0] - 150);
  let flew = false, maxY = 0, headingInAir = null, steeredInAir = false;
  for (let t = 0; t < 10; t += 1 / 60) {
    Physics.step(car, { steer: car.airborne ? 1 : aim(car) }, track, 1 / 60);
    if (car.airborne) {
      flew = true; maxY = Math.max(maxY, car.y);
      if (headingInAir === null) headingInAir = car.heading;
      else if (Math.abs(Track.wrapAngle(car.heading - headingInAir)) > 1e-6) steeredInAir = true;
    }
  }
  assert(flew, 'went airborne');
  assert(maxY > 3, 'got some height: ' + maxY);
  assert(!steeredInAir, 'no steering in the air');
  assert(!car.airborne && car.y === 0, 'landed');
  eq(car.wrecks, 0, 'landed cleanly');
});

test('scraping a wall throws sparks and slows the car; a head-on hit wrecks and respawns', () => {
  const road = straightRoad();
  const scrape = drive(carOnStraight('comet'), { steer: 0 }, 8, road);
  const vBefore = Physics.forwardSpeed(scrape);
  drive(scrape, { steer: 0.15 }, 2, road);
  assert(scrape.sparks, 'sparks while scraping');
  assert(!scrape.wrecked, 'a scrape is not a wreck');
  assert(Physics.forwardSpeed(scrape) < vBefore, 'scraping slowed it');
  assert(!road.isOffRoad(scrape.x, scrape.z), 'kept on the road');

  const crash = drive(carOnStraight('comet'), { steer: 0 }, 8, road);
  drive(crash, { steer: 1 }, 0.8, road);
  assert(crash.wrecked, 'head-on into the wall wrecks');
  eq(Physics.forwardSpeed(crash), 0, 'stopped');
  drive(crash, { steer: 0 }, 2.5, road);
  assert(!crash.wrecked, 'respawned');
  assert(!road.isOffRoad(crash.x, crash.z), 'back on the road');
  near(Math.abs(crash.lateral), 0, 1, 'in the middle of the road');
});

test('a car with a bad hint still finds its stretch of road', () => {
  const car = carAt('comet', 1500);
  car.hint = 2;
  Physics.step(car, { steer: 0 }, track, 1 / 60);
  near(car.s, 1500, 3);
});

test('two overlapping cars are pushed apart; a hard ram wrecks the slower one', () => {
  const a = carOnStraight('comet', 100), b = carOnStraight('comet', 100.5);
  assert(Physics.collide(a, b), 'overlapping cars collide');
  assert(Math.hypot(a.x - b.x, a.z - b.z) >= 2.7, 'pushed apart');
  const slow = carOnStraight('comet', 200), fast = carOnStraight('brawler', 198);
  fast.vx = 40;
  assert(Physics.collide(fast, slow), 'ram collides');
  assert(slow.wrecked, 'the slower car is wrecked');
  assert(!fast.wrecked, 'the rammer drives on');
  assert(!Physics.collide(fast, slow), 'wrecked cars are ghosts');
});

test('a near miss is close and fast but not a touch', () => {
  const a = carOnStraight('comet', 300), b = Physics.createCar('comet', { x: 300, z: 3.5, heading: 0, s: 300, hint: 0 });
  a.vx = 40;
  assert(Physics.nearMiss(a, b), 'close pass counts');
  const far = Physics.createCar('comet', { x: 300, z: 7, heading: 0, s: 300, hint: 0 });
  assert(!Physics.nearMiss(a, far), 'too far does not');
  const parked = carOnStraight('comet', 300);
  assert(!Physics.nearMiss(parked, b), 'no speed difference does not');
});

const AI = js('ai.js');

console.log('AI');

function simulate(car, driver, seconds, ctx) {
  const dt = 1 / 60;
  let time = 0, minS = Infinity, maxS = -Infinity, crossings = 0;
  for (let t = 0; t < seconds; t += dt) {
    const inp = AI.input(car, driver, track, Object.assign({ time }, ctx || {}));
    const before = car.s;
    Physics.step(car, inp, track, dt);
    if (before > track.length * 0.8 && car.s < track.length * 0.2) crossings++;
    time += dt;
  }
  return { crossings };
}

test('a skilled driver gets round the whole lap without wrecking', () => {
  const car = carAt('comet', 10);
  const driver = AI.makeDriver({ skill: 0.9, aggression: 0, lane: 0 });
  const r = simulate(car, driver, 110);
  assert(r.crossings >= 1, 'completed a lap: crossings ' + r.crossings);
  eq(car.wrecks, 0, 'never wrecked');
});

test('a weak driver still finishes a lap, a little slower', () => {
  const strong = carAt('comet', 10), weak = carAt('comet', 10);
  const strongR = simulate(strong, AI.makeDriver({ skill: 1, aggression: 0, lane: 0 }), 120);
  const weakR = simulate(weak, AI.makeDriver({ skill: 0.3, aggression: 0, lane: 1.5, phase: 2 }), 120);
  assert(weak.wrecks <= 1, 'weak driver mostly stays clean: ' + weak.wrecks);
  assert(strongR.crossings >= 1 && weakR.crossings >= 1, 'both finished a lap');
  assert(strongR.crossings > weakR.crossings || strong.s > weak.s, 'the strong driver is ahead');
});

test('the driver slows for corners and uses nitro on straights', () => {
  const car = carAt('comet', 10);
  const driver = AI.makeDriver({ skill: 0.8, aggression: 0 });
  assert(AI.cornerSpeed(track, 10, 0.8) > AI.cornerSpeed(track, 1000, 0.8) || AI.cornerSpeed(track, 10, 0.8) < 200, 'corner speed is a number');
  let usedNitro = false, lifted = false;
  const dt = 1 / 60;
  for (let t = 0; t < 60; t += dt) {
    car.nitro = 1;
    const inp = AI.input(car, driver, track, { time: t });
    if (inp.nitro) usedNitro = true;
    if (inp.throttle < 0.5) lifted = true;
    Physics.step(car, inp, track, dt);
  }
  assert(usedNitro, 'used nitro somewhere');
  assert(lifted, 'lifted for a corner somewhere');
});

test('rubber-banding eases off when far ahead of the player and pushes when far behind', () => {
  const car = carAt('comet', 500);
  const driver = AI.makeDriver({ skill: 0.8, aggression: 0 });
  const ahead = AI.input(car, driver, track, { time: 0, progress: 1000, playerProgress: 500 });
  const behind = AI.input(car, driver, track, { time: 0, progress: 500, playerProgress: 1000 });
  const level = AI.input(car, driver, track, { time: 0, progress: 500, playerProgress: 520 });
  assert(ahead.powerScale < 1, 'eases off when far ahead');
  assert(behind.powerScale > 1, 'pushes when far behind');
  eq(level.powerScale, 1, 'level is normal');
});

test('a driver steers round a car directly ahead', () => {
  const car = carAt('comet', 400, 0);
  const blocker = carAt('comet', 410, 0);
  const driver = AI.makeDriver({ skill: 0.8, aggression: 0, lane: 0 });
  car.vx = Math.cos(car.heading) * 30; car.vz = Math.sin(car.heading) * 30;
  const clear = AI.input(car, driver, track, { time: 0, cars: [car] });
  const blocked = AI.input(car, driver, track, { time: 0, cars: [car, blocker] });
  assert(Math.abs(blocked.steer - clear.steer) > 0.05, 'steers away from the blocker');
});

const Race = js('race.js');

console.log('Race');

function fakeStorage(initial) {
  const data = Object.assign({}, initial || {});
  return { get: (k) => (k in data ? data[k] : null), set: (k, v) => { data[k] = v; }, data };
}
function seeded(seed) {
  let s = seed || 1;
  return () => { s = (s * 16807) % 2147483647; return (s - 1) / 2147483646; };
}
// Move a car to a distance along the lap. The next physics step reads the new
// distance from the position and remembers the old one as prevS, so a
// teleport across the line looks like a crossing.
function teleport(car, s) {
  const p = track.at(s);
  car.x = p.x; car.z = p.z; car.hint = p.i; car.heading = Math.atan2(p.dz, p.dx);
}
function runRace(race, playerInput, seconds) {
  const dt = 1 / 60;
  for (let t = 0; t < seconds && race.phase !== 'finished'; t += dt) {
    Race.update(race, dt, typeof playerInput === 'function' ? playerInput(race) : playerInput);
  }
  return race;
}
const aiPlayer = (() => { const d = AI.makeDriver({ skill: 0.95, aggression: 0 }); return (race) => AI.input(Race.playerEntry(race).car, d, race.track, { time: race.time, cars: race.entries.map((e) => e.car) }); })();

test('a race starts with a countdown during which nobody moves', () => {
  const race = Race.create({ track, playerCar: 'comet', random: seeded(1) });
  eq(race.phase, 'countdown');
  eq(race.entries.length, 8);
  assert(Race.playerEntry(race).isPlayer);
  const before = race.entries.map((e) => [e.car.x, e.car.z]);
  runRace(race, { steer: 0 }, 2);
  race.entries.forEach((e, i) => { eq(e.car.x, before[i][0]); eq(e.car.z, before[i][1]); });
  runRace(race, { steer: 0 }, 1.1);
  eq(race.phase, 'racing');
  eq(Race.lapNumber(Race.playerEntry(race), 3), 1);
});

test('the player starts at the back of the grid', () => {
  const race = Race.create({ track, playerCar: 'comet', random: seeded(2) });
  const p = Race.playerEntry(race);
  race.entries.forEach((e) => { if (!e.isPlayer) assert(e.car.s >= p.car.s, 'opponent level or ahead on the grid'); });
  eq(p.place, 8);
});

test('laps count only when the line is crossed forwards', () => {
  const race = Race.create({ track, playerCar: 'comet', random: seeded(3) });
  const e = Race.playerEntry(race);
  race.phase = 'racing';
  teleport(e.car, track.length - 5); e.crossings = 0;
  Race.update(race, 0, { steer: 0 });
  eq(e.crossings, 0, 'not yet');
  // Across the line forwards.
  teleport(e.car, 5); Race.update(race, 0, { steer: 0 });
  eq(e.crossings, 1, 'forwards counts');
  teleport(e.car, track.length - 5); Race.update(race, 0, { steer: 0 });
  eq(e.crossings, 0, 'backwards uncounts');
});

test('positions order by distance covered, and finishers come first', () => {
  const race = Race.create({ track, playerCar: 'comet', random: seeded(4) });
  race.phase = 'racing';
  race.entries.forEach((e, i) => { e.crossings = 1; teleport(e.car, 100 + i * 50); });
  race.entries[2].finished = true; race.entries[2].finishTime = 100;
  Race.update(race, 0, { steer: 0 });
  const order = Race.standings(race);
  assert(order[0] === race.entries[2], 'the finisher leads');
  assert(order[1] === race.entries[7], 'then the car furthest round');
  eq(race.entries[2].place, 1);
});

test('a full race: the player finishes, gets a place and a best time is saved', () => {
  const storage = fakeStorage();
  const race = Race.create({ track, playerCar: 'dart', laps: 1, storage, random: seeded(5) });
  runRace(race, aiPlayer, 240);
  eq(race.phase, 'finished', 'finished within 4 minutes');
  const r = race.results;
  assert(r.place >= 1 && r.place <= 8, 'a place: ' + r.place);
  assert(r.total > 30 && r.total < 240, 'a sensible time: ' + r.total);
  eq(r.lapTimes.length, 1);
  eq(r.newBest, true, 'first time is a record');
  const saved = Race.loadRecords(storage);
  near(saved.bestTime, r.total, 0.001);
  // A slower second race does not beat it.
  const again = Race.create({ track, playerCar: 'dart', laps: 1, storage, random: seeded(6) });
  again.phase = 'racing'; again.time = r.total + 10;
  const e = Race.playerEntry(again); e.crossings = 1; e.lapStart = 0; teleport(e.car, track.length - 2);
  Race.update(again, 0, { steer: 0 });
  teleport(e.car, 1); Race.update(again, 0, { steer: 0 });
  eq(again.phase, 'finished');
  eq(again.results.newBest, false);
  near(Race.loadRecords(storage).bestTime, r.total, 0.001, 'record kept');
});

test('records survive a broken storage', () => {
  const broken = { get: () => { throw new Error('no'); }, set: () => { throw new Error('no'); } };
  eq(Race.loadRecords(broken).bestTime, null);
  Race.saveRecords(broken, { bestTime: 1 });
  eq(Race.loadRecords(null).bestTime, null);
});

test('times format as m:ss.mmm', () => {
  eq(Race.formatTime(0), '0:00.000');
  eq(Race.formatTime(65.4321), '1:05.432');
  eq(Race.formatTime(null), '--:--.---');
});

test('resuming after a pause brings the countdown back without moving the cars', () => {
  const race = Race.create({ track, playerCar: 'comet', random: seeded(7) });
  runRace(race, { steer: 0 }, 5);
  eq(race.phase, 'racing');
  Race.resume(race);
  eq(race.phase, 'countdown');
  const x = Race.playerEntry(race).car.x;
  runRace(race, { steer: 0 }, 1);
  eq(Race.playerEntry(race).car.x, x);
});

console.log('');
if (failures.length) { console.log(failures.length + ' failed, ' + passed + ' passed'); process.exit(1); }
console.log('All ' + passed + ' passed');
