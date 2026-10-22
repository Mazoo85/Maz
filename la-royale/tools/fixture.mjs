#!/usr/bin/env node
/*
 * A MADE-UP test neighbourhood, for checking the true-scale engine without
 * the real OpenStreetMap download:
 *
 *   node la-royale/tools/fixture.mjs        # writes data/fixture-raw/, then builds data/fixture/
 *
 * then open index.html?map=real&data=fixture. The streets, buildings, shops
 * and trees are invented (a rotated grid round Downtown, in OSM's format);
 * only the ground is real, from the elevation tiles in data/raw/ if they're
 * there. It's for development only: none of it is committed or published.
 */

import { mkdirSync, writeFileSync, rmSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { execFileSync } from 'node:child_process';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const RAW = join(ROOT, 'data', 'fixture-raw');
const OUT = join(ROOT, 'data', 'fixture');
rmSync(RAW, { recursive: true, force: true });
rmSync(OUT, { recursive: true, force: true });
mkdirSync(RAW, { recursive: true });

let a = 7;
const rnd = () => { a = (a + 0x6d2b79f5) >>> 0; let t = a; t = Math.imul(t ^ (t >>> 15), t | 1); t ^= t + Math.imul(t ^ (t >>> 7), t | 61); return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };

// a local frame round (lat0, lon0), rotated like the Downtown grid
const lat0 = 34.0505, lon0 = -118.2535, ROT = (-36 * Math.PI) / 180;
const kx = 111320 * Math.cos((lat0 * Math.PI) / 180), kz = 110574;
const ll = (u, v) => { const x = u * Math.cos(ROT) - v * Math.sin(ROT), z = u * Math.sin(ROT) + v * Math.cos(ROT); return { lat: lat0 - z / kz, lon: lon0 + x / kx }; };

const els = [];
let id = 1;
const way = (tags, pts) => els.push({ type: 'way', id: id++, tags, geometry: pts.map(([u, v]) => ll(u, v)) });
const node = (tags, u, v) => els.push({ type: 'node', id: id++, ...ll(u, v), tags });
const ring = (u0, v0, u1, v1) => [[u0, v0], [u1, v0], [u1, v1], [u0, v1], [u0, v0]];

const B = 120, ST = 24, N = 9;                  // block, street, blocks each way from the middle
const names = ['Main', 'Spring', 'Broadway', 'Hill', 'Olive', 'Grand', 'Hope', 'Flower', 'Figueroa', 'Francisco', 'Bixel', 'Lucas', 'Union', 'Bonnie Brae', 'Alvarado', 'Hoover', 'Vermont', 'Normandie', 'Western'];
const cross = ['1st', '2nd', '3rd', '4th', '5th', '6th', '7th', 'Wilshire', '8th', '9th', 'Olympic', '11th', '12th', 'Pico', '14th', '15th', '16th', 'Venice', '18th'];
const P = B + ST, E = N * P;
for (let k = -N; k <= N; k++) {
  const big = k % 3 === 0;
  const tags = { highway: big ? 'secondary' : 'residential', name: `${names[k + N]} Fixture St`, ...(big ? { lanes: '4' } : {}) };
  way(tags, [[k * P, -E], [k * P, E]]);
  way({ highway: big ? 'primary' : 'tertiary', name: `${cross[k + N]} Fixture Ave`, lanes: big ? '4' : '2' }, [[-E, k * P], [E, k * P]]);
}
// a freeway along one side, two one-way carriageways, and a railway
way({ highway: 'motorway', oneway: 'yes', lanes: '5', name: 'Fixture Freeway', ref: 'I-110' }, [[E + 40, -E], [E + 40, E]]);
way({ highway: 'motorway', oneway: 'yes', lanes: '5', name: 'Fixture Freeway', ref: 'I-110' }, [[E + 64, E], [E + 64, -E]]);
way({ railway: 'light_rail', name: 'Fixture Line' }, [[-E, -E - 30], [E, -E - 30]]);

const kinds = ['shop', 'amenity', 'amenity', 'shop', 'office'];
const what = { shop: ['convenience', 'clothes', 'florist', 'bakery', 'hairdresser'], amenity: ['restaurant', 'cafe', 'bar', 'fast_food', 'bank'], office: ['company'] };
let shops = 0;
for (let i = -N; i < N; i++) {
  for (let j = -N; j < N; j++) {
    const u0 = i * P + ST / 2, v0 = j * P + ST / 2, u1 = u0 + B, v1 = v0 + B;
    const d = Math.hypot(i + 0.5, j + 0.5);
    const r = rnd();
    if (i === 0 && j === 0) {               // a park with a lake in the middle
      way({ leisure: 'park', name: 'Fixture Square' }, ring(u0, v0, u1, v1));
      way({ natural: 'water', name: 'Fixture Pond' }, [[u0 + 30, v0 + 40], [u0 + 70, v0 + 30], [u0 + 90, v0 + 60], [u0 + 60, v0 + 90], [u0 + 35, v0 + 80], [u0 + 30, v0 + 40]]);
      for (let k = 0; k < 12; k++) node({ natural: 'tree', genus: k % 3 ? 'Ficus' : 'Washingtonia' }, u0 + 10 + rnd() * 100, v0 + 5 + rnd() * 20);
      continue;
    }
    if (r < 0.08) { way({ amenity: 'parking' }, ring(u0 + 2, v0 + 2, u1 - 2, v1 - 2)); continue; }
    if (d < 3) {
      // towers: a few big footprints, some with a podium and a tower part
      const h = 60 + rnd() * 240;
      way({ building: 'office', height: String(Math.round(h)), 'building:material': rnd() < 0.6 ? 'glass' : 'concrete', name: `Fixture Tower ${i},${j}` }, ring(u0 + 8, v0 + 8, u1 - 8, v1 - 8));
      for (let s = 0; s < 3; s++) { node({ [kinds[s]]: what[kinds[s]][s], name: `Fixture ${['Cafe', 'Deli', 'Bank'][s]} ${i}${j}` }, u0 + 20 + s * 30, v0 + 4); shops++; }
      continue;
    }
    if (d < 6) {
      // mid-rise and storefront frontage round the block
      for (let s = 0; s < 4; s++) {
        const w = B / 4;
        const h = 6 + rnd() * (d < 4.5 ? 40 : 14);
        const type = h > 20 ? 'apartments' : rnd() < 0.5 ? 'retail' : 'commercial';
        way({ building: type, height: String(Math.round(h)), ...(rnd() < 0.3 ? { 'building:material': 'brick' } : {}) }, ring(u0 + s * w + 1, v0 + 1, u0 + (s + 1) * w - 1, v0 + 30));
        way({ building: 'yes', 'building:levels': String(1 + Math.floor(rnd() * 4)) }, ring(u0 + s * w + 1, v1 - 30, u0 + (s + 1) * w - 1, v1 - 1));
        const k = kinds[Math.floor(rnd() * kinds.length)];
        node({ [k]: what[k][Math.floor(rnd() * what[k].length)], name: `Fixture ${['Market', 'Tacos', 'Books', 'Records', 'Donuts', 'Laundry', 'Pho', 'Pizza'][Math.floor(rnd() * 8)]} ${shops++}` }, u0 + s * w + w / 2, v0 - 1);
      }
      continue;
    }
    // houses: lots along both sides, a pool in some back yards
    for (const side of [0, 1]) {
      for (let s = 0; s < 6; s++) {
        const w = B / 6, lu = u0 + s * w, vf = side ? v1 - 8 : v0 + 8, vb = side ? v1 - 22 : v0 + 22;
        way({ building: rnd() < 0.7 ? 'house' : 'yes' }, ring(lu + 3, Math.min(vf, vb), lu + w - 3, Math.max(vf, vb)));
        if (rnd() < 0.3) { const pv = side ? v1 - 32 : v0 + 28; way({ leisure: 'swimming_pool' }, ring(lu + 6, pv, lu + 12, pv + 4)); }
      }
    }
  }
}
// a hillside park of scrub and woods to the north
way({ natural: 'scrub' }, ring(-E, -E - 400, E, -E - 60));
way({ natural: 'wood' }, ring(-E / 2, -E - 360, 0, -E - 160));
writeFileSync(join(RAW, 'osm_fixture.json'), JSON.stringify({ note: 'MADE-UP TEST DATA, not real Los Angeles', elements: els }));
console.log(`fixture: ${els.length} made-up elements`);

execFileSync('node', [join(ROOT, 'tools', 'build-tiles.mjs')], { stdio: 'inherit', env: { ...process.env, LA_RAW: RAW, LA_DEM: join(ROOT, 'data', 'raw'), LA_OUT: OUT, LA_NAME: 'MADE-UP TEST DATA' } });
