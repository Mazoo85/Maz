/*
 * Tests for the true-scale map: turning OpenStreetMap into tiles (osm.js)
 * and the real world's ground, shapes and collision (realcity.js).
 * Plain Node, no data download needed.
 *
 *   node la-royale/tests/real.test.js
 */

import assert from 'node:assert/strict';
import {
  REAL, projector, toBuilding, toRoad, roadWidth, areaKind, toPoi, treeKind, encodeTile, decodeTile,
  BTYPE, ROOF, MATERIAL, RCLASS, AREA, ringArea, cleanRing
} from '../js/osm.js';
import { makeRealWorld, minRect, styleOf, pip, roofTriangles, LANDMARKS, NEIGHBOURHOODS } from '../js/realcity.js';
import { collideBody } from '../js/world.js';
import { TILE_LAYERS, TILE_GLSL } from '../js/tiles.js';
import { readFileSync, existsSync } from 'node:fs';

let passed = 0, failed = 0;
function test(name, fn) {
  try { fn(); passed++; console.log(`  ✓ ${name}`); } catch (e) { failed++; console.log(`  ✗ ${name}\n    ${e.message}`); }
}
const near = (a, b, tol, what) => assert.ok(Math.abs(a - b) <= tol, `${what}: ${a} is not within ${tol} of ${b}`);

console.log('projection');
const P = projector();
test('the centre is the origin, east is +x and north is −z', () => {
  const [x, z] = P.toXZ(REAL.lat0, REAL.lon0);
  near(x, 0, 1e-9, 'x'); near(z, 0, 1e-9, 'z');
  const [ex] = P.toXZ(REAL.lat0, REAL.lon0 + 0.01), [, nz] = P.toXZ(REAL.lat0 + 0.01, REAL.lon0);
  assert.ok(ex > 0 && nz < 0);
});
test('it is true scale: City Hall to the Capitol Records Building is about 9.3 km', () => {
  const [ax, az] = P.toXZ(34.0537, -118.2427), [bx, bz] = P.toXZ(34.1032, -118.3262);
  near(Math.hypot(bx - ax, bz - az), 9330, 120, 'distance');
});
test('toLatLon undoes toXZ', () => {
  const [lat, lon] = P.toLatLon(...P.toXZ(34.1184, -118.3004));
  near(lat, 34.1184, 1e-9, 'lat'); near(lon, -118.3004, 1e-9, 'lon');
});

console.log('buildings from OpenStreetMap');
const sq = (s, x = 0, z = 0) => [[x, z], [x + s, z], [x + s, z + s], [x, z + s], [x, z]];
test('a measured height is kept; levels become storeys', () => {
  assert.equal(toBuilding({ building: 'office', height: '72.4' }, sq(30)).h, 72.4);
  assert.equal(toBuilding({ building: 'apartments', 'building:levels': '4' }, sq(30)).h, 14.4);
  near(toBuilding({ building: 'yes', height: "40'" }, sq(30)).h, 12.19, 0.01, 'feet');
});
test('untagged heights get the usual size for the kind of building', () => {
  assert.equal(toBuilding({ building: 'house' }, sq(12)).h, 5.2);
  assert.equal(toBuilding({ building: 'garage' }, sq(6)).h, 3);
});
test('rings come out the same way round, closing point dropped', () => {
  const b = toBuilding({ building: 'yes' }, sq(10).reverse());
  assert.equal(b.pts.length, 4);
  assert.ok(ringArea(b.pts) > 0);
});
test('slivers are dropped and collinear points removed', () => {
  assert.equal(toBuilding({ building: 'yes' }, sq(1.5)), null);
  assert.equal(cleanRing([[0, 0], [5, 0], [10, 0], [10, 10], [0, 10]]).length, 4);
});
test('types, roofs and materials', () => {
  assert.equal(toBuilding({ building: 'detached' }, sq(12)).type, BTYPE.house);
  assert.equal(toBuilding({ building: 'house' }, sq(12)).roof, ROOF.gabled);
  assert.equal(toBuilding({ building: 'house', 'roof:shape': 'flat' }, sq(12)).roof, ROOF.flat);
  assert.equal(toBuilding({ building: 'office', height: '200' }, sq(40)).mat, MATERIAL.glass);
  assert.equal(toBuilding({ building: 'yes', 'building:material': 'brick' }, sq(20)).mat, MATERIAL.brick);
  assert.equal(toBuilding({ building: 'yes', 'building:colour': '#ff0000' }, sq(20)).col, 0xff0000);
});

console.log('streets, areas and shops');
test('road classes and widths', () => {
  assert.equal(toRoad({ highway: 'motorway' }, [[0, 0], [9, 0]]).cls, RCLASS.motorway);
  assert.equal(toRoad({ highway: 'living_street' }, [[0, 0], [9, 0]]).cls, RCLASS.residential);
  assert.equal(toRoad({ highway: 'primary', tunnel: 'yes' }, [[0, 0], [9, 0]]), null);
  assert.equal(roadWidth({ lanes: '4' }, RCLASS.secondary), 4 * 3.6 + 4.8);
  assert.equal(roadWidth({ width: '12' }, RCLASS.residential), 12);
  assert.equal(toRoad({ railway: 'subway' }, [[0, 0], [9, 0]]), null);
});
test('areas, shops and trees', () => {
  assert.equal(areaKind({ leisure: 'swimming_pool' }), AREA.water);
  assert.equal(areaKind({ amenity: 'parking' }), AREA.parking);
  assert.equal(areaKind({ amenity: 'parking', parking: 'underground' }), null);
  assert.deepEqual(toPoi({ amenity: 'cafe', name: 'Intelligentsia' }), { name: 'Intelligentsia', kind: 'amenity', what: 'cafe' });
  assert.equal(toPoi({ shop: 'bakery' }), null);
  assert.equal(treeKind({ genus: 'Washingtonia' }), 0);
  assert.equal(treeKind({ species: 'Jacaranda mimosifolia' }), 2);
});
test('a tile survives encoding to within 5 cm', () => {
  const tile = {
    ox: 1000, oz: -2000,
    buildings: [toBuilding({ building: 'retail', name: 'Amoeba Music' }, sq(20, 1003.33, -1990.71))],
    roads: [toRoad({ highway: 'primary', name: 'Sunset Boulevard' }, [[1000, -2000], [1400.04, -1999.96]])],
    areas: [{ kind: AREA.park, pts: sq(50, 1100, -1900) }], trees: [{ x: 1010.12, z: -1995.55, kind: 0 }],
    pois: [{ x: 1013, z: -1980, name: 'Amoeba Music', what: 'music' }]
  };
  const back = decodeTile(JSON.parse(JSON.stringify(encodeTile(tile))));
  assert.equal(back.buildings[0].name, 'Amoeba Music');
  near(back.buildings[0].pts[0][0], 1003.33, 0.05, 'x'); near(back.roads[0].pts[1][0], 1400.04, 0.05, 'road x');
  assert.equal(back.roads[0].name, 'Sunset Boulevard');
  near(back.trees[0].z, -1995.55, 0.05, 'tree z');
  assert.equal(back.pois[0].what, 'music');
});

console.log('the real world');
// a small made-up area: a hill rising 1 m every 10 m to the east
const M = { lat0: REAL.lat0, lon0: REAL.lon0, x0: -500, z0: -500, width: 1000, depth: 800, tile: 1000, tiles: ['0_0'],
  height: { nx: 101, nz: 81, step: 10, scale: 0.1 }, counts: { buildings: 1 } };
const hts = new Int16Array(101 * 81);
for (let j = 0; j < 81; j++) for (let i = 0; i < 101; i++) hts[j * 101 + i] = i * 10 + 500;     // 50 m + 1 m per 10 m, in decimetres
const RW = makeRealWorld(M, hts);
test('it is centred, square and big enough to hold the area', () => {
  assert.equal(RW.size, 1000); assert.equal(RW.half, 500);
  const [x, z] = RW.toWorld(REAL.lat0, REAL.lon0);
  near(x, 0, 1e-6, 'x'); near(z, 0, 1e-6, 'z');
});
test('the ground follows the elevation data', () => {
  near(RW.groundHeight(-500, 0), 50, 0.01, 'west edge');
  near(RW.groundHeight(0, 0), 100, 0.01, 'middle');
  near(RW.groundHeight(123, -77), 112.3, 0.01, 'between grid points');
  near(RW.groundHeight(9999, 0), 150, 0.01, 'clamped past the edge');
});
test('landmarks and neighbourhoods are at their real spots', () => {
  const big = makeRealWorld({ ...M, x0: -6000, z0: -6000, width: 12000, depth: 12000 }, hts);
  const obs = big.pois.find((p) => p.name === 'Griffith Observatory');
  const [x, z] = P.toXZ(34.11824, -118.30037);
  near(obs.x, x - big.OX, 1e-6, 'x'); near(obs.z, z - big.OZ, 1e-6, 'z');
  assert.equal(big.districtAt(...big.toWorld(34.0488, -118.2518)).name, 'Downtown');
  assert.equal(big.districtAt(...big.toWorld(34.1016, -118.3267)).name, 'Hollywood');
  assert.ok(LANDMARKS.length > 60 && NEIGHBOURHOODS.length > 25);
  assert.equal(new Set(LANDMARKS.map((l) => l[0])).size, LANDMARKS.length, 'a landmark is listed twice');
});
test('you walk into a real building outline and are stopped by it', () => {
  const pts = [[0, 0], [20, 0], [26, 14], [8, 22], [-4, 10]];      // an odd five-sided footprint
  RW.colliders.push({ k: 2, x: 10, z: 9, r: 20, y0: 100, y1: 112, pts }); RW._dirty = true;
  const g = RW.groundHeight(-8, 10);
  const body = { x: -8, z: 10, y: g, vx: 3, vy: 0, vz: 0, r: 0.42, h: 1.8, step: 0.55 };
  for (let k = 0; k < 120; k++) { body.x += body.vx / 30; body.y = RW.groundHeight(body.x, body.z); collideBody(RW, body); }
  assert.ok(!pip(body.x, body.z, pts), `ended up inside at ${body.x.toFixed(2)}, ${body.z.toFixed(2)}`);
  assert.ok(body.x > -5.2 && body.x < -4, `stopped at the wall, not ${body.x.toFixed(2)}`);
  RW.colliders.length = 0; RW._dirty = true;
});
test('the collider grid works far from the middle of a big map', () => {
  const big = makeRealWorld({ ...M, x0: -6000, z0: -6000, width: 12000, depth: 12000 }, hts);
  big.colliders.push({ k: 2, x: 5800, z: -5800, r: 10, y0: 0, y1: 500, pts: sq(10, 5795, -5805).slice(0, 4) }); big._dirty = true;
  const body = { x: 5800, z: -5800, y: 100, vx: 0, vy: 0, vz: 0, r: 0.42, h: 1.8, step: 0.55 };
  collideBody(big, body);
  assert.ok(!pip(body.x, body.z, big.colliders[0].pts), 'still inside the building');
});

console.log('shapes and looks');
test('minRect finds a rotated rectangle and its long side', () => {
  const a = 0.6, c = Math.cos(a), s = Math.sin(a);
  const pts = [[-10, -4], [10, -4], [10, 4], [-10, 4]].map(([x, z]) => [100 + x * c - z * s, 50 + x * s + z * c]);
  const R = minRect(pts);
  near(R.L, 10, 1e-6, 'half length'); near(R.Wd, 4, 1e-6, 'half width');
  near(R.cx, 100, 1e-6, 'cx'); near(R.cz, 50, 1e-6, 'cz');
  near(Math.abs(R.ux * c + R.uz * s), 1, 1e-6, 'long axis');
});
test('a small untagged footprint is a house with a pitched roof; a tower is glass', () => {
  const house = toBuilding({ building: 'yes' }, sq(12));
  const st = styleOf(house, 144);
  assert.ok(st.house);
  const tower = toBuilding({ building: 'yes', height: '210' }, sq(50));
  assert.equal(styleOf(tower, 2500).win, 3);
  assert.equal(styleOf(toBuilding({ building: 'warehouse' }, sq(60)), 3600).win, 4);
});
test('roofs mostly get pitched on houses, and LA stucco colours by default', () => {
  let pitched = 0, n = 0;
  for (let k = 0; k < 200; k++) {
    const b = toBuilding({ building: 'house', 'roof:shape': 'flat' }, sq(13, k * 31.7, k * 17.3));
    b.roof = ROOF.flat;
    const st = styleOf({ ...b, type: BTYPE.other }, 169);
    if (st.roof !== ROOF.flat) pitched++;
    n++;
    assert.match(st.wall, /^#[0-9a-f]{6}$/);
  }
  assert.ok(pitched > n * 0.7, `${pitched} of ${n}`);
});

test('every footprint gets a whole roof, even one whose outline crosses itself', () => {
  const area = (tris) => tris.reduce((s, [a, b, c]) => s + Math.abs((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])) / 2, 0);
  near(area(roofTriangles([[0, 0], [10, 0], [10, 4], [4, 4], [4, 10], [0, 10]])), 64, 1e-6, 'L-shaped roof');
  const bow = roofTriangles([[0, 0], [10, 10], [10, 0], [0, 10]]);
  assert.ok(area(bow) >= 49.9, `a crossed outline is still roofed (${area(bow)})`);
});

console.log('surface tiles');
test('the baked Blender tiles are the ones the shader expects, in the same order and size', () => {
  const dir = new URL('../textures/', import.meta.url);
  const baked = JSON.parse(readFileSync(new URL('tiles.json', dir), 'utf8')).tiles;
  assert.deepEqual(baked.map((t) => [t.name, t.metres]), TILE_LAYERS);
  for (const [name] of TILE_LAYERS) for (const s of ['c', 'n']) assert.ok(existsSync(new URL(`${name}_${s}.jpg`, dir)), `${name}_${s}.jpg`);
  assert.match(TILE_GLSL, /bool tileOf\(int kind/);
});

console.log(`\n${passed} passed, ${failed} failed`);
if (failed) process.exit(1);
