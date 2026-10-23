import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import { test, testAsync, assert, eq, throws, done } from './harness.js';
import { loadData } from '../js/data/load.js';
import { check } from '../js/data/schema.js';

const base = join(dirname(fileURLToPath(import.meta.url)), '..', 'data') + '/';
const read = async (url) => readFileSync(url, 'utf8');

test('schema errors name the path', () => {
  throws(() => check({ a: [1, 'x'] }, { type: 'object', props: { a: { type: 'array', items: { type: 'integer' } } } }, 'f'), /^f\.a\[1\]: expected number/);
  throws(() => check({}, { type: 'object', required: ['id'] }, 'f'), /^f\.id: missing/);
  throws(() => check(7, { type: 'integer', max: 5 }, 'f'), /above 5/);
});

const data = await (async () => {
  let d;
  await testAsync('every data file loads and cross-checks', async () => { d = await loadData(base, read); });
  return d;
})();

if (data) {
  test('the world is the right size', () => {
    assert(data.states.length >= 900 && data.states.length <= 1400, 'states ' + data.states.length);
    const land = data.provinces.filter((p) => p && p.type === 'land').length;
    const sea = data.provinces.filter((p) => p && p.type === 'sea').length;
    assert(land >= 10000 && land <= 14000, 'land provinces ' + land);
    assert(sea >= 2000 && sea <= 5000, 'sea provinces ' + sea);
    assert(data.countries.length >= 70, 'countries ' + data.countries.length);
  });

  test('every country owns its capital and has at least one state', () => {
    for (const c of data.countries) {
      const st = data.stateById[c.capital_state];
      eq(st.owner, c.tag, c.tag + ' capital owner');
      assert(st.provinces.includes(c.capital_province), c.tag + ' capital province in capital state');
      assert(data.provinces[c.capital_province].vp >= 15, c.tag + ' capital has victory points');
    }
  });

  test('the great powers are where they should be', () => {
    const byName = (n) => data.provinces.filter((p) => p && p.name === n).sort((a, b) => b.vp - a.vp)[0];
    eq(data.stateById[byName('Berlin').state].owner, 'GER');
    eq(data.stateById[byName('Paris').state].owner, 'FRA');
    eq(data.stateById[byName('London').state].owner, 'ENG');
    eq(data.stateById[byName('Moscow').state].owner, 'SOV');
    eq(data.stateById[byName('Tokyo').state].owner, 'JAP');
    eq(data.stateById[byName('Rome').state].owner, 'ITA');
    eq(data.stateById[byName('Washington').state].owner, 'USA');
    eq(data.stateById[byName('Nanjing').state].owner, 'CHI');
  });

  test('1936 borders: East Prussia is German, Lwów is Polish, Manchuria is Manchukuo', () => {
    const owner = (name) => data.stateById[data.provinces.filter((p) => p && p.name === name).sort((a, b) => b.vp - a.vp)[0].state].owner;
    eq(owner('Kaliningrad'), 'GER');
    eq(owner('Lviv'), 'POL');
    eq(owner('Harbin'), 'MAN');
    eq(owner('Vienna'), 'AUS');
    eq(owner('Addis Ababa'), 'ETH');
  });

  test('terrain is varied and sensible', () => {
    const land = data.provinces.filter((p) => p && p.type === 'land');
    const n = (t) => land.filter((p) => p.terrain === t).length / land.length;
    assert(n('plains') > 0.2 && n('plains') < 0.6, 'plains share ' + n('plains'));
    assert(n('mountain') > 0.03 && n('mountain') < 0.2, 'mountain share ' + n('mountain'));
    assert(n('forest') > 0.05, 'forest share ' + n('forest'));
    assert(n('desert') > 0.03, 'desert share ' + n('desert'));
    assert(n('jungle') > 0.02, 'jungle share ' + n('jungle'));
    assert(n('urban') > 0.003, 'urban share ' + n('urban'));
    const t = (name) => data.provinces.filter((p) => p && p.name === name).sort((a, b) => b.vp - a.vp)[0].terrain;
    eq(t('Berlin'), 'urban');
    assert(['desert'].includes(t('Cairo')) || data.provinces.find((p) => p && p.name === 'Cairo').terrain === 'urban');
  });
}

done('data');
