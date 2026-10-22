/*
 * EARTH EXPLORER — logic tests.
 *
 *   node earth/tests/earth-logic.test.js
 *
 * No dependencies and no browser. What these prove is what the map tells you:
 * a measured distance or area is right, the numbers read the way they should,
 * and a saved-places file comes back exactly as it went out.
 */
'use strict';

const path = require('path');
const G = require(path.join(__dirname, '..', 'js', 'geo.js'));

let passed = 0;
const failures = [];

function test(name, fn) {
  try {
    fn();
    passed++;
    console.log('  ok   ' + name);
  } catch (e) {
    failures.push(name + ' — ' + e.message);
    console.log('  FAIL ' + name + ' — ' + e.message);
  }
}

function assert(cond, message) {
  if (!cond) throw new Error(message || 'assertion failed');
}

function eq(actual, expected, message) {
  if (actual !== expected) {
    throw new Error((message || 'values differ') + ': got ' + JSON.stringify(actual) +
      ', expected ' + JSON.stringify(expected));
  }
}

function near(actual, expected, tolerance, message) {
  if (Math.abs(actual - expected) > tolerance) {
    throw new Error((message || 'not close enough') + ': got ' + actual + ', expected ' +
      expected + ' ± ' + tolerance);
  }
}

const LONDON = [-0.1276, 51.5072];
const PARIS = [2.3522, 48.8566];
const NEW_YORK = [-74.006, 40.7128];

console.log('Distances');

test('London to Paris is about 344 km', () => {
  near(G.distance(LONDON, PARIS), 343900, 1500);
});

test('London to New York is about 5,570 km', () => {
  near(G.distance(LONDON, NEW_YORK), 5570000, 10000);
});

test('distance is the same both ways and zero to itself', () => {
  near(G.distance(PARIS, LONDON), G.distance(LONDON, PARIS), 1e-6);
  eq(G.distance(PARIS, PARIS), 0);
});

test('a quarter of the way round the equator is a quarter of its length', () => {
  near(G.distance([0, 0], [90, 0]), (Math.PI / 2) * G.EARTH_RADIUS, 1);
});

test('crossing the date line takes the short way', () => {
  near(G.distance([179.5, 0], [-179.5, 0]), G.distance([0, 0], [1, 0]), 1);
});

test('a path adds up its legs', () => {
  near(G.pathLength([LONDON, PARIS, LONDON]), 2 * G.distance(LONDON, PARIS), 1e-6);
  eq(G.pathLength([LONDON]), 0);
});

console.log('Areas');

test('a one-degree square at the equator is about 12,364 km²', () => {
  const sq = [[0, 0], [1, 0], [1, 1], [0, 1]];
  near(G.polygonArea(sq) / 1e6, 12364, 15);
});

test('area does not depend on which way round you click', () => {
  const sq = [[0, 0], [1, 0], [1, 1], [0, 1]];
  near(G.polygonArea(sq.slice().reverse()), G.polygonArea(sq), 1e-6);
});

test('a shape across the date line has the same area as one that is not', () => {
  const a = [[179.5, 0], [-179.5, 0], [-179.5, 1], [179.5, 1]];
  const b = [[0, 0], [1, 0], [1, 1], [0, 1]];
  near(G.polygonArea(a), G.polygonArea(b), 1);
});

test('a shape round the North Pole measures the small side, not the rest of the world', () => {
  const capAbove80 = 2 * Math.PI * G.EARTH_RADIUS ** 2 * (1 - Math.sin((80 * Math.PI) / 180));
  const ring = [[0, 80], [90, 80], [180, 80], [-90, 80]];
  near(G.polygonArea(ring) / 1e6, capAbove80 / 1e6, capAbove80 / 1e6 * 0.05);
});

test('fewer than three points has no area', () => {
  eq(G.polygonArea([LONDON, PARIS]), 0);
});

console.log('Lines that follow the curve of the Earth');

test('halfway along the great circle is the same distance from both ends', () => {
  const mid = G.interpolate(LONDON, NEW_YORK, 0.5);
  near(G.distance(LONDON, mid), G.distance(mid, NEW_YORK), 1);
});

test('the flight path from London to New York bends north', () => {
  const mid = G.interpolate(LONDON, NEW_YORK, 0.5);
  assert(mid[1] > 51.5, 'midpoint latitude ' + mid[1] + ' should be north of London');
});

test('densify keeps both ends and the total length', () => {
  const line = G.densify([LONDON, NEW_YORK], 100000);
  assert(line.length > 50, 'expected many points, got ' + line.length);
  near(line[0][0], LONDON[0], 1e-9);
  near(line[line.length - 1][0], NEW_YORK[0], 1e-9);
  near(G.pathLength(line), G.distance(LONDON, NEW_YORK), 50);
});

test('densify keeps a date-line crossing in one piece', () => {
  const line = G.densify([[170, 0], [-170, 0]], 100000);
  for (let i = 1; i < line.length; i++) {
    assert(Math.abs(line[i][0] - line[i - 1][0]) < 5, 'line jumps at point ' + i);
  }
  near(line[line.length - 1][0], 190, 1e-6);
});

console.log('Reading the numbers');

test('distances in metric', () => {
  eq(G.formatDistance(850.4, 'metric'), '850 m');
  eq(G.formatDistance(8523, 'metric'), '8.52 km');
  eq(G.formatDistance(1234567, 'metric'), '1,235 km');
});

test('distances in miles and feet', () => {
  eq(G.formatDistance(100, 'imperial'), '328 ft');
  eq(G.formatDistance(5000, 'imperial'), '3.11 mi');
  eq(G.formatDistance(5570000, 'imperial'), '3,461 mi');
});

test('areas in metric', () => {
  eq(G.formatArea(500, 'metric'), '500 m²');
  eq(G.formatArea(25000, 'metric'), '2.50 ha');
  eq(G.formatArea(12364e6, 'metric'), '12,364 km²');
});

test('areas in acres and square miles', () => {
  eq(G.formatArea(100, 'imperial'), '1,076 sq ft');
  eq(G.formatArea(4046.8564224 * 5, 'imperial'), '5.00 acres');
  eq(G.formatArea(2589988.11 * 100, 'imperial'), '100 sq mi');
});

test('elevation', () => {
  eq(G.formatElevation(8848.86, 'metric'), '8,849 m');
  eq(G.formatElevation(8848.86, 'imperial'), '29,032 ft');
});

test('coordinates read with N/S and E/W', () => {
  eq(G.formatCoord(40.7128, -74.006), '40.7128° N, 74.0060° W');
  eq(G.formatCoord(-33.8688, 151.2093), '33.8688° S, 151.2093° E');
});

test('typed coordinates are understood', () => {
  const a = G.parseCoordinates('48.8584, 2.2945');
  eq(a.lat, 48.8584);
  eq(a.lng, 2.2945);
  const b = G.parseCoordinates('33.86° S 151.21° E');
  eq(b.lat, -33.86);
  eq(b.lng, 151.21);
  const c = G.parseCoordinates('40.7 N, 74.0 W');
  eq(c.lng, -74);
});

test('place names are not mistaken for coordinates', () => {
  eq(G.parseCoordinates('Paris'), null);
  eq(G.parseCoordinates('10 Downing Street'), null);
  eq(G.parseCoordinates('95, 10'), null);
});

console.log('Saved places');

test('a place is cleaned up and kept within limits', () => {
  const p = G.normalizePlace({ name: '  Home  ', note: 'x'.repeat(2000), lat: '51.5', lng: 360 + 10 });
  eq(p.name, 'Home');
  eq(p.note.length, 1000);
  eq(p.lat, 51.5);
  near(p.lng, 10, 1e-9);
  assert(p.id, 'gets an id');
});

test('a place with no name gets one, and broken places are dropped', () => {
  eq(G.normalizePlace({ lat: 0, lng: 0 }).name, 'Untitled place');
  eq(G.normalizePlace({ name: 'x', lat: 95, lng: 0 }), null);
  eq(G.normalizePlace({ name: 'x', lat: 'abc', lng: 0 }), null);
  eq(G.normalizePlace(null), null);
});

test('a saved file comes back exactly as it went out', () => {
  const places = [
    G.normalizePlace({ id: 'a1', name: 'Cabin', note: 'Key under the mat', lat: 46.5, lng: 7.9, created: 1 }),
    G.normalizePlace({ id: 'b2', name: 'Café', lat: -22.95, lng: -43.21, created: 2, zoom: 15 }),
  ];
  const back = G.parsePlaces(G.serializePlaces(places));
  eq(JSON.stringify(back), JSON.stringify(places));
});

test('a bare list of places can be read too', () => {
  eq(G.parsePlaces('[{"name":"A","lat":1,"lng":2}]').length, 1);
});

test('a file that is not a places file says so', () => {
  let message = '';
  try { G.parsePlaces('not json'); } catch (e) { message = e.message; }
  eq(message, 'That file is not a saved-places file.');
  try { G.parsePlaces('{"hello":1}'); } catch (e) { message = e.message; }
  eq(message, 'That file has no places in it.');
});

test('reading a file twice does not save places twice', () => {
  const mine = [G.normalizePlace({ id: 'a', name: 'Home', lat: 10, lng: 10 })];
  const incoming = [
    G.normalizePlace({ id: 'zzz', name: 'Home', lat: 10.000001, lng: 10 }),
    G.normalizePlace({ id: 'b', name: 'Work', lat: 11, lng: 11 }),
  ];
  const once = G.mergePlaces(mine, incoming);
  eq(once.added, 1);
  eq(once.places.length, 2);
  const twice = G.mergePlaces(once.places, incoming);
  eq(twice.added, 0);
});

console.log('');
if (failures.length) {
  console.log(failures.length + ' failed, ' + passed + ' passed');
  process.exit(1);
}
console.log('All ' + passed + ' passed');
