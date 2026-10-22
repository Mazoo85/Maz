/*
 * LA Royale — world logic tests. Plain Node, no dependencies:
 *
 *   node la-royale/tests/world.test.js
 *
 * They check the map is Los Angeles (the ocean is west, the hills are
 * north, the Hollywood Sign is up high), that the city generator keeps
 * buildings out of the sea and off the freeways, and that you can't walk
 * through walls.
 */

import assert from 'node:assert/strict';
import { createWorld, collideBody, locationAt, rayClear, polyDist, queryPoint } from '../js/world.js';
import { project, unproject, ROADS, ROAD_WIDTH, DISTRICTS, HALF } from '../js/geo.js';
import { LANDMARKS } from '../js/landmarks.js';
import { PROP, SIGN_TEXTS } from '../js/street.js';

let passed = 0, failed = 0;
function test(name, fn) {
  try { fn(); passed++; console.log(`  ✓ ${name}`); } catch (e) { failed++; console.log(`  ✗ ${name}\n    ${e.message}`); }
}

const W = createWorld({ seed: 1985 });
const at = (name) => {
  const p = W.pois.find((q) => q.name === name);
  assert.ok(p, `no landmark called ${name}`);
  return p;
};

console.log('geography');
test('projection round-trips', () => {
  const [x, z] = project(34.0522, -118.2437);
  const [lat, lon] = unproject(x, z);
  assert.ok(Math.abs(lat - 34.0522) < 1e-9 && Math.abs(lon + 118.2437) < 1e-9);
});
test('the map is laid out like LA', () => {
  const pier = at('Santa Monica Pier'), cityHall = at('City Hall'), lax = at('LAX'), sign = at('Hollywood Sign'), port = at('Port of Los Angeles');
  assert.ok(pier.x < cityHall.x - 2500, 'Santa Monica is far west of Downtown');
  assert.ok(lax.z > cityHall.z + 1200, 'LAX is south of Downtown');
  assert.ok(lax.x < cityHall.x, 'LAX is west of Downtown');
  assert.ok(sign.z < cityHall.z - 1000, 'the Hollywood Sign is north of Downtown');
  assert.ok(port.z > lax.z && port.x > lax.x, 'the port is south-east');
});
test('the Pacific is west of the beaches', () => {
  const pier = at('Santa Monica Pier');
  assert.ok(W.isSea(pier.x - 600, pier.z + 400), 'out past the pier is ocean');
  assert.ok(W.groundHeight(pier.x - 600, pier.z + 400) < -5, 'and it is deep');
  assert.ok(!W.isSea(at('City Hall').x, at('City Hall').z), 'Downtown is dry');
  assert.equal(locationAt(W, -2800, 1500).district, 'Pacific Ocean');
});
test('the Hollywood Sign sits high in the hills', () => {
  const sign = at('Hollywood Sign');
  const blvd = project(34.1016, -118.330);
  const hSign = W.groundHeight(sign.x, sign.z), hBlvd = W.groundHeight(...blvd);
  assert.ok(hSign > hBlvd + 60, `sign ${hSign.toFixed(0)} m vs boulevard ${hBlvd.toFixed(0)} m`);
  const obs = at('Griffith Observatory');
  assert.ok(W.groundHeight(obs.x, obs.z) > hBlvd + 40, 'Griffith Observatory looks down on Hollywood');
});
test('the LA River runs in a sunken channel', () => {
  const [x, z] = project(34.010, -118.222);
  const bank = W.groundHeight(x + 60, z);
  assert.ok(W.groundHeight(x, z) < bank - 3, 'channel bed is below the banks');
});
test('every lake has water above its bed', () => {
  for (const l of W.lakes) assert.ok(W.groundHeight(l.x, l.z) < l.level, l.name);
});
test('ground height matches the rendered terrain grid at its vertices', () => {
  for (const [i, j] of [[10, 10], [200, 133], [383, 7], [57, 300]]) {
    const x = -HALF + i * W.cell, z = -HALF + j * W.cell;
    assert.ok(Math.abs(W.groundHeight(x, z) - W.heights[j * (W.gridN + 1) + i]) < 1e-4);
  }
});

console.log('city');
test('it is a big city', () => {
  assert.ok(W.buildings.length > 10000, `${W.buildings.length} buildings`);
  assert.ok(W.trees.filter((t) => t.t === 0).length > 2000, 'plenty of palm trees');
  assert.ok(W.pools.length > 1000, 'and swimming pools');
});
test('generation is deterministic', () => {
  const W2 = createWorld({ seed: 1985 });
  assert.equal(W2.buildings.length, W.buildings.length);
  assert.deepEqual(W2.buildings[1234], W.buildings[1234]);
  const W3 = createWorld({ seed: 7 });
  assert.notDeepEqual(W3.buildings.slice(0, 50), W.buildings.slice(0, 50));
});
test('no building stands in the sea', () => {
  const wet = W.buildings.filter((b) => W.isSea(b.x, b.z));
  assert.equal(wet.length, 0, `${wet.length} buildings in the water`);
});
test('no generated building sits on a freeway', () => {
  const freeways = ROADS.filter((r) => r.kind === 'freeway');
  const onRoad = W.buildings.filter((b) => b.win !== 0 && freeways.some((r) => polyDist(r.pts, b.x, b.z) < ROAD_WIDTH.freeway / 2));
  assert.equal(onRoad.length, 0, onRoad.slice(0, 3).map((b) => `${b.x.toFixed(0)},${b.z.toFixed(0)}`).join(' '));
});
test('every neighbourhood with a street grid got built on', () => {
  const empty = DISTRICTS.filter((d) => !['wild', 'park', 'airport', 'oilfield', 'hills'].includes(d.style))
    .filter((d) => !W.blocks.some((b) => W.districtAt(b.x, b.z) === d));
  assert.deepEqual(empty.map((d) => d.name), []);
});
test('the famous places are all on the map', () => {
  for (const name of ['Santa Monica Pier', 'Venice Beach', 'Hollywood Sign', 'Griffith Observatory', 'Dodger Stadium', 'LAX',
    'SoFi Stadium', 'Getty Center', 'Rodeo Drive', 'Walk of Fame', 'Universal Studios', 'Crypto.com Arena', 'Union Station',
    'LA Memorial Coliseum', 'Watts Towers', 'Rose Bowl', 'Queen Mary', 'Port of Los Angeles', "Randy's Donuts", 'Urban Light (LACMA)',
    'El Capitan Theatre', 'Hollywood Roosevelt', 'Paramount Pictures', 'Greek Theatre', 'Angels Flight', 'Bradbury Building', 'The Wiltern',
    'Binoculars Building', "Bob's Big Boy", 'Colorado Street Bridge', 'Pacific Design Center', 'Bronson Caves']) at(name);
  assert.ok(LANDMARKS.length >= 100);
});
test('the drivable cars are parked on dry land, not inside anything', () => {
  assert.ok(W.vehicles.length >= 35, `${W.vehicles.length} cars`);
  for (const v of W.vehicles) {
    assert.ok(!W.isSea(v.x, v.z), `${v.type} at ${v.x.toFixed(0)},${v.z.toFixed(0)} is in the sea`);
    assert.equal(queryPoint(W, v.x, W.groundHeight(v.x, v.z) + 1, v.z, 1.5), null, `${v.type} spawns inside something`);
  }
});

console.log('streets');
test('the streets are furnished', () => {
  const count = (t) => W.props.filter((p) => p.t === t).length;
  assert.ok(count(PROP.STREETLIGHT) > 400, `${count(PROP.STREETLIGHT)} streetlights`);
  assert.ok(count(PROP.POWER_POLE) > 2000, `${count(PROP.POWER_POLE)} power poles`);
  assert.ok(count(PROP.SIGNAL) > 20, 'traffic signals where boulevards cross');
  assert.ok(W.wires.length / 6 > 5000, 'power lines between the poles');
  assert.ok(W.crosswalks.length > 40, 'crosswalks');
});
test('no streetlight, pole or hydrant stands in the roadway', () => {
  const inRoad = W.props.filter((p) => ROADS.some((r) => polyDist(r.pts, p.x, p.z) < ROAD_WIDTH[r.kind] / 2 - 0.2));
  assert.equal(inRoad.length, 0, inRoad.slice(0, 3).map((p) => `${p.t}@${p.x.toFixed(0)},${p.z.toFixed(0)}`).join(' '));
});
test('shops have signs that fit the neighbourhood', () => {
  assert.ok(W.signs.length > 300, `${W.signs.length} signs`);
  const inEastLA = W.signs.filter((s) => W.districtAt(s.x, s.z).name === 'East LA' || W.districtAt(s.x, s.z).name === 'Boyle Heights').map((s) => SIGN_TEXTS[s.cell]);
  assert.ok(inEastLA.some((t) => ['CARNICERÍA', 'PANADERÍA', 'TAQUERÍA', 'MERCADO', 'LAVANDERÍA'].includes(t)), 'East LA has its panaderías and carnicerías');
});
test('freeway signs name LA\'s real control cities', () => {
  const t = W.gantryTexts.map((g) => `${g.num} ${g.dir} ${g.dest}`);
  assert.ok(t.includes('405 NORTH Sacramento'), t.slice(0, 5).join(', '));
  assert.ok(t.some((x) => x.startsWith('10 WEST Santa Monica')));
});

console.log('architecture');
test('LA building types are all there', () => {
  const by = (pred) => W.parts.filter(pred).length;
  assert.ok(by((p) => p.s === 'gable' && p.m === 'tile') > 1000, 'Spanish tile roofs');
  assert.ok(by((p) => p.s === 'gable' && p.m === 'asphalt') > 500, 'Craftsman and cottage roofs');
  assert.ok(by((p) => p.s === 'arch') > 1000, 'arched windows, porches and arcades');
  assert.ok(by((p) => p.m === 'fabric') > 200, 'shop awnings');
  assert.ok(by((p) => p.m === 'foliage') > 100, 'hedges');
  assert.ok(W.parts.some((p) => p.s === 'text' && /^GAS/.test(p.text)), 'gas stations');
  assert.ok(W.parts.some((p) => p.s === 'text' && /^(The |Casa |Sea |Kona |Palm |Villa |Lanai |Royal |Shangri)/.test(p.text)), 'dingbats with their names');
});
test('houses are built to real dimensions', () => {
  // single-storey bungalows and ranches: 2.8-3.6 m walls on 9-20 m wide footprints
  const houses = W.buildings.filter((b) => b.win === 1 && b.noLip && b.h < 5 && b.w > 8);
  assert.ok(houses.length > 3000, `${houses.length} houses`);
  for (const b of houses) assert.ok(b.w <= 20 && b.d <= 17.5, `${b.w.toFixed(1)} × ${b.d.toFixed(1)} m`);
});

console.log('collision');
const tall = W.buildings.find((b) => b.h > 60 && b.win === 3);
const body = (x, z, y) => ({ x, y, z, vx: 0, vy: 0, vz: 0, r: 0.42, h: 1.8, step: 0.55, onGround: false, groundY: 0 });
test('you cannot walk into a building', () => {
  const b = body(tall.x, tall.z, W.groundHeight(tall.x, tall.z));
  collideBody(W, b);
  const c = Math.cos(tall.rot), s = Math.sin(tall.rot);
  const dx = b.x - tall.x, dz = b.z - tall.z;
  const lx = dx * c - dz * s, lz = dx * s + dz * c;
  assert.ok(Math.abs(lx) >= tall.w / 2 - 0.01 || Math.abs(lz) >= tall.d / 2 - 0.01, 'pushed outside the walls');
});
test('you can stand on a roof', () => {
  const top = tall.y + tall.h;
  const b = body(tall.x, tall.z, top + 0.02);
  b.vy = -1;
  collideBody(W, b);
  assert.ok(b.onGround, 'landed');
  assert.ok(Math.abs(b.y - top) < 0.1, `on the roof at ${b.y.toFixed(1)} (roof ${top.toFixed(1)})`);
});
test('you land on the ground, never under it', () => {
  const [x, z] = project(34.0, -118.30);
  const b = body(x, z, W.groundHeight(x, z) - 3);
  collideBody(W, b);
  assert.ok(b.y >= W.groundHeight(x, z) - 0.01);
});
test('the camera cannot see through the hills', () => {
  const sign = at('Hollywood Sign');
  const t = rayClear(W, sign.x, W.groundHeight(sign.x, sign.z) + 2, sign.z - 900, sign.x, W.groundHeight(sign.x, sign.z) + 2, sign.z + 900);
  assert.ok(t < 1, 'a ray through the mountain is blocked');
});
test('"where am I" names the landmark and the neighbourhood', () => {
  const pier = at('Santa Monica Pier');
  const loc = locationAt(W, pier.x, pier.z);
  assert.equal(loc.poi, 'Santa Monica Pier');
  const [x, z] = project(34.05, -118.25);
  assert.equal(locationAt(W, x, z).district, 'Downtown LA');
});

console.log(`\n${passed} passed, ${failed} failed (world generated in ${W.stats.genMs} ms)`);
if (failed) process.exit(1);
