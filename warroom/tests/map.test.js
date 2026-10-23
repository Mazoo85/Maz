import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import { test, testAsync, assert, eq, done } from './harness.js';
import { loadData } from '../js/data/load.js';
import { decodeIdMap } from './png.js';

const base = join(dirname(fileURLToPath(import.meta.url)), '..', 'data') + '/';
const data = await loadData(base, async (url) => readFileSync(url, 'utf8'));

test('adjacency is symmetric and nothing is isolated', () => {
  for (let a = 1; a <= data.provinceCount; a++) {
    const p = data.provinces[a];
    for (const b of data.adjacency[a]) {
      assert(data.adjacency[b].includes(a), `edge ${a}->${b} has no return`);
      assert(b !== a, 'self edge ' + a);
    }
    if (p.type !== 'lake') assert(data.adjacency[a].length > 0, `province ${a} "${p.name}" has no neighbours`);
  }
});

test('river crossings join real neighbours', () => {
  for (const [a, b] of data.rivers) assert(data.adjacency[a].includes(b), `river ${a}-${b}`);
  assert(data.rivers.length > 500, 'rivers ' + data.rivers.length);
});

function reach(from, allowSea) {
  const seen = new Uint8Array(data.provinceCount + 1);
  const stack = [from]; seen[from] = 1;
  while (stack.length) {
    const a = stack.pop();
    for (const b of data.adjacency[a]) {
      if (seen[b]) continue;
      const t = data.provinces[b].type;
      if (t === 'land' && data.provinces[b].terrain === 'wasteland') continue;
      if ((t === 'lake' || t === 'sea') && !allowSea) continue;
      seen[b] = 1; stack.push(b);
    }
  }
  return seen;
}

test('you can march from Berlin to Lisbon, Moscow, Vladivostok, Cape Town and Singapore', () => {
  const id = (n) => data.provinces.filter((p) => p && p.name === n).sort((a, b) => b.vp - a.vp)[0].id;
  const seen = reach(id('Berlin'), false);
  for (const city of ['Lisbon', 'Moscow', 'Vladivostok', 'Cape Town', 'Singapore', 'Hanoi', 'Istanbul', 'Cairo']) {
    assert(seen[id(city)], city + ' not reachable by land');
  }
  assert(!seen[id('Honolulu')], 'Honolulu should need the sea');
  assert(!seen[id('New York')], 'New York should need the sea');
});

test('every land province is reachable by land or sea', () => {
  const id = (n) => data.provinces.filter((p) => p && p.name === n).sort((a, b) => b.vp - a.vp)[0].id;
  const seen = reach(id('Berlin'), true);
  const missing = [];
  for (let a = 1; a <= data.provinceCount; a++) {
    const p = data.provinces[a];
    if (p.type === 'land' && p.terrain !== 'wasteland' && !seen[a]) missing.push(p.name);
  }
  assert(missing.length === 0, missing.length + ' land provinces unreachable even by sea: ' + missing.slice(0, 10).join(', '));
});

test('the id map matches provinces.json', () => {
  const { width, height, ids } = decodeIdMap(readFileSync(base + 'map/provinces.png'));
  eq(width, 8192); eq(height, 4096);
  const counts = new Uint32Array(data.provinceCount + 1);
  let max = 0;
  for (let i = 0; i < ids.length; i++) { const v = ids[i]; if (v > max) max = v; if (v <= data.provinceCount) counts[v]++; }
  eq(max, data.provinceCount, 'highest id in the image');
  eq(counts[0], 0, 'no unassigned pixels');
  for (let a = 1; a <= data.provinceCount; a++) {
    const p = data.provinces[a];
    assert(counts[a] > 0, `province ${a} "${p.name}" has no pixels`);
    eq(ids[p.y * width + p.x], a, `centre pixel of ${a} "${p.name}" belongs to it`);
  }
});

test('states are contiguous ownership units with a capital in them', () => {
  for (const s of data.states) {
    assert(s.provinces.includes(s.capital_province), `state ${s.id} ${s.name} capital`);
    assert(s.manpower > 0, `state ${s.id} ${s.name} manpower`);
  }
});

done('map');
