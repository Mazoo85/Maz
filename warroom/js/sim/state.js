/*
 * The game state: one plain object holding everything, so that
 * JSON.parse(JSON.stringify(state)) is a complete save.
 *
 * Sections that later stages fill (economy, research, politics, ...) exist from
 * the start as empty objects, so a Stage 1 save still loads when they arrive.
 */
import { seedStream } from './rng.js';

export const STATE_VERSION = 1;

export function newGame(data, { seed = 'war room', scenario = '1936', player = null } = {}) {
  const sc = data.scenarios[scenario];
  if (!sc) throw new Error(`unknown scenario "${scenario}"`);

  const countries = {};
  for (const c of data.countries) {
    countries[c.tag] = {
      tag: c.tag,
      name: c.name,
      adjective: c.adjective,
      color: c.color,
      ideology: c.ideology,
      leader: c.leader,
      capital: c.capital_state,
      flag: c.flag,
      states: [],
    };
  }

  const states = {};
  for (const s of data.states) {
    if (!countries[s.owner]) throw new Error(`state ${s.id} "${s.name}" has unknown owner "${s.owner}"`);
    states[s.id] = {
      owner: s.owner,
      controller: s.owner,
      cores: s.cores.slice(),
      infrastructure: s.infrastructure,
      buildings: {},
      manpower: s.manpower,
    };
    countries[s.owner].states.push(s.id);
  }

  const regions = {};
  for (const r of data.regions) {
    regions[r.id] = { weather: 'clear', weatherDays: 0 };
  }

  const armies = {};
  for (const [tag, list] of Object.entries(sc.armies || {})) {
    if (!countries[tag]) throw new Error(`scenario army for unknown country "${tag}"`);
    armies[tag] = list.map((a) => ({ state: a.state, divisions: a.divisions }));
  }

  return {
    version: STATE_VERSION,
    seed: String(seed),
    scenario,
    hour: sc.start_hour || 0,
    speed: 0,
    player,
    rng: {
      weather: seedStream(seed, 'weather'),
      events: seedStream(seed, 'events'),
    },
    countries,
    states,
    regions,
    land: { armies },
    economy: {},
    research: {},
    politics: {},
    diplomacy: {},
    naval: {},
    air: {},
    intel: {},
    dirty: { provinces: false, regions: false },
    log: [],
  };
}

/** Who controls a province right now (via its state). Wasteland and sea return null. */
export function provinceController(state, data, provinceId) {
  const p = data.provinces[provinceId];
  if (!p || !p.state) return null;
  return state.states[p.state].controller;
}
