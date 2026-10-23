/*
 * Daily weather per strategic region. Each day every region rolls tomorrow's
 * weather from a transition table for its climate, biased by the month.
 * The result is read by land, air and naval combat and by movement.
 */
import { hourToDate } from './clock.js';
import { nextFloat } from './rng.js';

export const WEATHER_TYPES = ['clear', 'rain', 'storm', 'snow', 'blizzard', 'sandstorm', 'mud', 'frost'];

/** Relative weights for tomorrow's weather given today's, for a climate and month (1..12). */
export function transitionWeights(table, climate, current, month) {
  const c = table.climates[climate];
  if (!c) throw new Error(`no weather table for climate "${climate}"`);
  const row = c.from[current] || c.from.clear;
  const out = [];
  for (const [type, w] of Object.entries(row)) {
    const season = c.season[type];
    const k = season ? season[month - 1] : 1;
    const weight = w * k;
    if (weight > 0) out.push([type, weight]);
  }
  if (out.length === 0) out.push(['clear', 1]);
  return out;
}

export function tickWeather(state, data, hour) {
  const { month } = hourToDate(hour);
  const rng = state.rng.weather;
  for (const r of data.regions) {
    const reg = state.regions[r.id];
    const weights = transitionWeights(data.weather, r.climate, reg.weather, month);
    let total = 0;
    for (const [, w] of weights) total += w;
    let roll = nextFloat(rng) * total;
    let next = weights[weights.length - 1][0];
    for (const [type, w] of weights) {
      roll -= w;
      if (roll < 0) { next = type; break; }
    }
    if (next === reg.weather) reg.weatherDays += 1;
    else { reg.weather = next; reg.weatherDays = 1; }
  }
  state.dirty.regions = true;
}

export function weatherAt(state, data, provinceId) {
  const p = data.provinces[provinceId];
  if (!p) return 'clear';
  const reg = state.regions[p.region];
  return reg ? reg.weather : 'clear';
}
