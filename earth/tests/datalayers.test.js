/*
 * EARTH EXPLORER — tests for the live and history layers (js/datalayers.js).
 *
 *   node earth/tests/datalayers.test.js
 *
 * No dependencies and no browser. They prove what the layers show: the aurora sits where NOAA's
 * grid says, raised volcano alerts are found by name, a span of years picks the right battles,
 * countdowns read correctly and old maps get a usable name and tile address.
 */
'use strict';

const path = require('path');
const fs = require('fs');
const D = require(path.join(__dirname, '..', 'js', 'datalayers.js'));

let passed = 0;
const failures = [];
function test(name, fn) {
  try { fn(); passed++; } catch (e) { failures.push(`${name}: ${e.message}`); }
}
function eq(a, b, what) { if (JSON.stringify(a) !== JSON.stringify(b)) throw new Error(`${what}: expected ${JSON.stringify(b)}, got ${JSON.stringify(a)}`); }
function ok(v, what) { if (!v) throw new Error(what); }

test('aurora: grid cells become 1° squares, east longitudes wrap to the west', () => {
  const g = D.auroraGeoJSON([[270, 65, 30], [10, 70, 2], [0, 89, 50]]);
  eq(g.features.length, 1, 'cells kept (2% is skipped, 89° is past the map edge)');
  eq(g.features[0].geometry.coordinates[0][0], [-90.5, 64.5], 'south-west corner');
  eq(g.features[0].properties.v, 30, 'chance');
});

test('aurora: the latest Kp from either table shape', () => {
  eq(D.latestKp([['time_tag', 'Kp'], ['2026-11-19 00:00', '2.67']]), 2.67, 'rows');
  eq(D.latestKp([{ Kp: 5.33 }]), 5.33, 'objects');
  eq(D.latestKp([]), null, 'empty');
  ok(D.kpWords(7.3).startsWith('strong'), 'storm words');
});

test('volcanoes: USGS alert levels are matched by name', () => {
  const g = D.volcanoGeoJSON([{ name: 'Kīlauea', lon: -155.29, lat: 19.42 }, { name: 'Etna', lon: 15, lat: 37.7 }],
    [{ volcano_name: 'Kilauea', alert_level: 'WATCH', color_code: 'ORANGE' }]);
  eq(g.features[0].properties.alert, '', 'diacritics differ, no false match');
  const h = D.volcanoGeoJSON([{ name: 'Great Sitkin', lon: -176, lat: 52 }], [{ volcano_name: 'Great Sitkin', alert_level: 'watch', color_code: 'ORANGE' }]);
  eq(h.features[0].properties.alert, 'WATCH', 'matched and upper-cased');
  eq(h.features[0].properties.rank, 2, 'rank for drawing order');
});

test('battles: the span and the search pick the same battles on the map and in the count', () => {
  const list = [{ id: 'Q1', name: 'Battle of Stalingrad', year: 1942, war: ['World War II'], lon: 44, lat: 48 },
    { id: 'Q2', name: 'Battle of Hastings', year: 1066, war: [], lon: 0.5, lat: 50.9 },
    { id: 'Q3', name: 'Battle of Kursk', year: 1943, war: ['Eastern Front'], lon: 36, lat: 51 }];
  eq(D.countBattles(list, 1939, 1945, ''), 2, 'WW2 span');
  eq(D.countBattles(list, 1939, 1945, 'stalin'), 1, 'search');
  eq(D.countBattles(list, -2000, 2026, 'eastern'), 1, 'search a war');
  eq(D.battleFilter(1939, 1945, 'Kursk')[3], ['in', 'kursk', ['get', 's']], 'map filter lower-cases the search');
  eq(D.battleGeoJSON(list)[ 'features'][0].properties.s, 'battle of stalingrad world war ii', 'search text');
  eq(D.yearText(-490), '490 BCE', 'BCE');
});

test('battles: the bundled data file is complete', () => {
  const list = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'data', 'battles.json'), 'utf8'));
  eq(list.length, 8740, 'battles');
  ok(list.every((d) => Number.isFinite(d.year) && Number.isFinite(d.lon) && Number.isFinite(d.lat) && d.name), 'every battle has a year, place and name');
  const v = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'data', 'volcanoes.json'), 'utf8'));
  eq(v.length, 1214, 'volcanoes');
});

test('launches: countdowns and pads', () => {
  const now = Date.parse('2026-11-19T00:00:00Z');
  eq(D.countdown('2026-11-20T03:05:00Z', now), 'T−1d 03:05', 'before');
  eq(D.countdown('2026-11-18T23:48:00Z', now), 'T+00:12', 'after');
  const g = D.launchGeoJSON([{ name: 'Falcon 9 | Starlink', net: '2026-11-19T02:00:00Z', pad: { latitude: '28.56', longitude: '-80.57', name: 'SLC-40', location: { name: 'Cape Canaveral' } } },
    { name: 'no pad' }], now);
  eq(g.features.length, 1, 'launches without a pad position are left out');
  eq(g.features[0].geometry.coordinates, [-80.57, 28.56], 'pad position');
  eq(g.features[0].properties.label, 'T−02:00', 'label');
  const list = [{ name: 'gone', net: '2026-11-18T20:00:00Z' }, { name: 'later', net: '2026-11-21T00:00:00Z' }, { name: 'soon', net: '2026-11-19T05:00:00Z' }];
  eq(D.nextLaunch(list, now).name, 'soon', 'the next launch still to come');
  eq(D.nextLaunch([{ name: 'gone', net: '2026-11-18T20:00:00Z' }], now), null, 'none left');
});

test('old maps: a name, a holder, a size and the tile address', () => {
  const m = { id: 'https://annotations.allmaps.org/maps/abc123', resource: { id: 'https://iiif.example.org/img/9/', provider: [{ label: { en: ['David Rumsey Map Collection'] } }], partOf: [] }, _allmaps: { area: 4e8 } };
  eq(D.mapLabel(m), 'Map from David Rumsey Map Collection', 'name from the holder');
  m.resource.partOf = [{ label: { en: ['Plan of London, 1799'] } }];
  eq(D.mapLabel(m), 'Plan of London, 1799', 'name from the collection');
  m.resource.partOf = [{ label: { none: ['[1]'] }, partOf: [{ label: { en: ['Ordnance Survey of London'] } }] }];
  eq(D.mapLabel(m), 'Ordnance Survey of London', 'a page number is skipped for the series name');
  m.resource.partOf = [{ label: { none: ['[1]'] } }];
  eq(D.mapLabel(m), 'Map from David Rumsey Map Collection', 'only a page number: the holder');
  eq(D.mapAcross(m), 'about 20 km across', 'size');
  eq(D.mapId(m), 'abc123', 'id');
  eq(D.mapTiles('abc123'), 'https://allmaps.xyz/maps/abc123/{z}/{x}/{y}.png', 'tiles');
  eq(D.mapThumb(m), 'https://iiif.example.org/img/9/full/96,/0/default.jpg', 'thumbnail');
});

console.log(`${passed} passed, ${failures.length} failed`);
for (const f of failures) console.log('FAIL ' + f);
process.exit(failures.length ? 1 : 0);
