import { test, assert, eq, done } from './harness.js';
import { fixtureData } from './fixture.js';
import { newGame } from '../js/sim/state.js';
import { advance } from '../js/sim/tick.js';
import { transitionWeights, weatherAt, WEATHER_TYPES } from '../js/sim/weather.js';

const data = fixtureData();

function histogram(seed, years, regionId) {
  const s = newGame(data, { seed });
  const counts = {};
  for (let d = 0; d < 365 * years; d++) {
    advance(s, data, 24);
    const w = s.regions[regionId].weather;
    counts[w] = (counts[w] || 0) + 1;
  }
  return counts;
}

test('every weather table entry names a known type', () => {
  for (const [climate, c] of Object.entries(data.weather.climates)) {
    for (const [from, row] of Object.entries(c.from)) {
      assert(WEATHER_TYPES.includes(from), `${climate}.from.${from}`);
      for (const to of Object.keys(row)) assert(WEATHER_TYPES.includes(to), `${climate}.from.${from}.${to}`);
    }
    for (const t of Object.keys(c.season)) assert(WEATHER_TYPES.includes(t), `${climate}.season.${t}`);
  }
});

test('a continental region sees snow in a year and a tropical one never does', () => {
  const north = histogram('snowy', 1, 1), south = histogram('snowy', 1, 2);
  assert((north.snow || 0) > 5, 'continental snow days ' + north.snow);
  eq(south.snow || 0, 0);
  assert((south.rain || 0) > 60, 'tropical rain days ' + south.rain);
});

test('transition weights drop to zero out of season', () => {
  const july = transitionWeights(data.weather, 'continental', 'clear', 7);
  assert(!july.some(([t]) => t === 'snow'), 'no snow in July');
  const jan = transitionWeights(data.weather, 'continental', 'clear', 1);
  assert(jan.some(([t]) => t === 'snow'), 'snow possible in January');
});

test('weatherAt reads through the province to its region', () => {
  const s = newGame(data, { seed: 'at' });
  s.regions[2].weather = 'storm';
  eq(weatherAt(s, data, 6), 'storm');
  eq(weatherAt(s, data, 1), 'clear');
  eq(weatherAt(s, data, 999), 'clear');
});

test('weather over ten years is a sensible mix', () => {
  const h = histogram('decade', 10, 1);
  const total = Object.values(h).reduce((a, b) => a + b, 0);
  assert(h.clear / total > 0.3 && h.clear / total < 0.75, 'clear share ' + (h.clear / total));
  assert((h.snow + h.blizzard + h.frost) / total > 0.1, 'winter share');
});

done('weather');
