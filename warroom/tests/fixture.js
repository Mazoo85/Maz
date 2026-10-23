/*
 * A tiny world for simulation tests: three countries, four states, eight land
 * provinces, two sea provinces, two regions. Shaped exactly like the real data
 * files, so what passes here passes on the real map.
 */
import weather from '../data/common/weather.json' with { type: 'json' };
import { indexData } from '../js/data/load.js';

export function fixtureData() {
  const provinces = [
    null,
    { id: 1, name: 'Aa', type: 'land', terrain: 'plains', state: 1, region: 1, coastal: true, vp: 5, x: 10, y: 10, lat: 50, lon: 10 },
    { id: 2, name: 'Ab', type: 'land', terrain: 'forest', state: 1, region: 1, coastal: false, vp: 0, x: 12, y: 10, lat: 50, lon: 11 },
    { id: 3, name: 'Ba', type: 'land', terrain: 'hills', state: 2, region: 1, coastal: true, vp: 3, x: 14, y: 10, lat: 50, lon: 12 },
    { id: 4, name: 'Bb', type: 'land', terrain: 'plains', state: 2, region: 1, coastal: false, vp: 0, x: 16, y: 10, lat: 50, lon: 13 },
    { id: 5, name: 'Ca', type: 'land', terrain: 'mountain', state: 3, region: 2, coastal: true, vp: 10, x: 10, y: 14, lat: 20, lon: 10 },
    { id: 6, name: 'Cb', type: 'land', terrain: 'jungle', state: 3, region: 2, coastal: false, vp: 0, x: 12, y: 14, lat: 20, lon: 11 },
    { id: 7, name: 'Da', type: 'land', terrain: 'desert', state: 4, region: 2, coastal: true, vp: 1, x: 14, y: 14, lat: 20, lon: 12 },
    { id: 8, name: 'Db', type: 'land', terrain: 'urban', state: 4, region: 2, coastal: false, vp: 0, x: 16, y: 14, lat: 20, lon: 13 },
    { id: 9, name: 'Sea 1', type: 'sea', terrain: 'ocean', state: 0, region: 3, coastal: false, vp: 0, x: 8, y: 10, lat: 50, lon: 8 },
    { id: 10, name: 'Sea 2', type: 'sea', terrain: 'ocean', state: 0, region: 3, coastal: false, vp: 0, x: 8, y: 14, lat: 20, lon: 8 },
  ];
  const adjacency = [[], [2, 9], [1, 4], [4, 9], [3, 2, 8], [6, 10], [5, 8], [8, 10], [7, 6, 4], [1, 3, 10], [5, 7, 9]];
  const rivers = [[1, 2]];
  const states = [
    { id: 1, name: 'Alpha', owner: 'RED', cores: ['RED'], provinces: [1, 2], capital_province: 1, manpower: 100000, infrastructure: 3, resources: {} },
    { id: 2, name: 'Beta', owner: 'RED', cores: ['RED', 'BLU'], provinces: [3, 4], capital_province: 3, manpower: 50000, infrastructure: 2, resources: {} },
    { id: 3, name: 'Gamma', owner: 'BLU', cores: ['BLU'], provinces: [5, 6], capital_province: 5, manpower: 80000, infrastructure: 2, resources: {} },
    { id: 4, name: 'Delta', owner: 'GRN', cores: ['GRN'], provinces: [7, 8], capital_province: 7, manpower: 20000, infrastructure: 1, resources: {} },
  ];
  const regions = [
    { id: 1, name: 'North', type: 'land', climate: 'continental', provinces: [1, 2, 3, 4] },
    { id: 2, name: 'South', type: 'land', climate: 'tropical', provinces: [5, 6, 7, 8] },
    { id: 3, name: 'Western Sea', type: 'sea', climate: 'ocean', provinces: [9, 10] },
  ];
  const countries = [
    { tag: 'RED', name: 'Redland', adjective: 'Red', color: '#cc3333', ideology: 'fascist', leader: 'R. Red', capital_state: 1, capital_province: 1, flag: { kind: 'plain', colors: ['#cc3333'] } },
    { tag: 'BLU', name: 'Blueland', adjective: 'Blue', color: '#3333cc', ideology: 'democratic', leader: 'B. Blue', capital_state: 3, capital_province: 5, flag: { kind: 'plain', colors: ['#3333cc'] } },
    { tag: 'GRN', name: 'Greenland', adjective: 'Green', color: '#33aa33', ideology: 'communist', leader: 'G. Green', capital_state: 4, capital_province: 7, flag: { kind: 'plain', colors: ['#33aa33'] } },
  ];
  const scenarios = { '1936': { start_hour: 0, armies: { RED: [{ state: 1, divisions: 12 }], BLU: [{ state: 3, divisions: 6 }] } } };
  return indexData({ provinces, adjacency, rivers, states, regions, countries, weather, scenarios,
    terrain: { types: ['plains', 'forest', 'hills', 'mountain', 'urban', 'jungle', 'marsh', 'desert', 'wasteland', 'ocean', 'lake'] } });
}
