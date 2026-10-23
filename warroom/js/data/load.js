/*
 * Loads and checks the game's data files. In the browser `read` is fetch; under
 * Node the tests pass a file reader. Every problem is reported with the file
 * and the path inside it.
 */
import { check, SCHEMAS } from './schema.js';

const FILES = {
  provinces: 'map/provinces.json',
  adjacency: 'map/adjacency.json',
  rivers: 'map/rivers.json',
  states: 'map/states.json',
  regions: 'map/regions.json',
  countries: 'common/countries.json',
  terrain: 'common/terrain.json',
  weather: 'common/weather.json',
};
const SCENARIOS = ['1936'];

export async function loadData(baseUrl, read) {
  const raw = {};
  for (const [key, rel] of Object.entries(FILES)) {
    raw[key] = await readJson(read, baseUrl + rel, key);
  }
  raw.scenarios = {};
  for (const s of SCENARIOS) {
    raw.scenarios[s] = await readJson(read, `${baseUrl}scenarios/${s}.json`, 'scenario');
  }
  return indexData(raw);
}

async function readJson(read, url, key) {
  let text;
  try { text = await read(url); } catch (e) { throw new Error(`could not load ${url}: ${e.message}`); }
  let value;
  try { value = JSON.parse(text); } catch (e) { throw new Error(`${url} is not valid JSON: ${e.message}`); }
  check(value, SCHEMAS[key], url);
  return value;
}

/** Cross-check references and build the lookup tables the sim and view use. */
export function indexData(raw) {
  const data = { ...raw };
  // provinces.json is a list indexed by id (index 0 is null)
  if (data.provinces[0] !== null) data.provinces = [null, ...data.provinces];
  data.provinces.forEach((p, i) => { if (p && p.id !== i) throw new Error(`provinces[${i}] has id ${p.id}; ids must be contiguous from 1`); });
  data.provinceCount = data.provinces.length - 1;

  data.stateById = {};
  for (const s of data.states) {
    if (data.stateById[s.id]) throw new Error(`duplicate state id ${s.id}`);
    data.stateById[s.id] = s;
    for (const pid of s.provinces) {
      const p = data.provinces[pid];
      if (!p) throw new Error(`state ${s.id} "${s.name}" lists unknown province ${pid}`);
      if (p.state !== s.id) throw new Error(`province ${pid} says state ${p.state} but state ${s.id} lists it`);
    }
    if (!data.provinces[s.capital_province]) throw new Error(`state ${s.id} "${s.name}" capital province ${s.capital_province} does not exist`);
  }
  for (const p of data.provinces) {
    if (p && p.type === 'land' && !data.stateById[p.state]) throw new Error(`land province ${p.id} "${p.name}" has no state`);
  }

  data.regionById = {};
  for (const r of data.regions) {
    data.regionById[r.id] = r;
    for (const pid of r.provinces) {
      const p = data.provinces[pid];
      if (!p) throw new Error(`region ${r.id} "${r.name}" lists unknown province ${pid}`);
      if (p.region !== r.id) throw new Error(`province ${pid} says region ${p.region} but region ${r.id} lists it`);
    }
    if (!data.weather.climates[r.climate]) throw new Error(`region ${r.id} "${r.name}" has climate "${r.climate}" with no weather table`);
  }

  data.byTag = {};
  for (const c of data.countries) {
    if (data.byTag[c.tag]) throw new Error(`duplicate country tag ${c.tag}`);
    data.byTag[c.tag] = c;
    if (!data.stateById[c.capital_state]) throw new Error(`country ${c.tag} capital state ${c.capital_state} does not exist`);
    if (!data.provinces[c.capital_province]) throw new Error(`country ${c.tag} capital province ${c.capital_province} does not exist`);
  }
  for (const s of data.states) {
    if (!data.byTag[s.owner]) throw new Error(`state ${s.id} "${s.name}" owner "${s.owner}" is not a country`);
    for (const t of s.cores) if (!data.byTag[t]) throw new Error(`state ${s.id} "${s.name}" core "${t}" is not a country`);
  }

  if (data.adjacency.length !== data.provinces.length) {
    throw new Error(`adjacency has ${data.adjacency.length} entries for ${data.provinces.length} provinces`);
  }
  data.riverSet = new Set(data.rivers.map(([a, b]) => (a < b ? `${a}-${b}` : `${b}-${a}`)));

  for (const [name, sc] of Object.entries(data.scenarios)) {
    for (const [tag, armies] of Object.entries(sc.armies)) {
      if (!data.byTag[tag]) throw new Error(`scenario ${name}: armies for unknown country ${tag}`);
      for (const a of armies) if (!data.stateById[a.state]) throw new Error(`scenario ${name}: ${tag} army in unknown state ${a.state}`);
    }
  }
  return data;
}

export function isRiverCrossing(data, a, b) {
  return data.riverSet.has(a < b ? `${a}-${b}` : `${b}-${a}`);
}
