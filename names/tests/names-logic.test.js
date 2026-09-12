/*
 * NAME FORGE — logic tests.
 *
 *   node names/tests/names-logic.test.js
 *
 * No dependencies and no browser: the word bank and the engine are plain
 * modules. What these prove is the promise the app makes on its own front
 * page — 1000 adjectives, 1000 nouns, no duplicates, and a name that always
 * comes out as an adjective and then a noun.
 */
'use strict';

const path = require('path');
const WORDS = require(path.join(__dirname, '..', 'js', 'words.js'));
const Forge = require(path.join(__dirname, '..', 'js', 'generator.js'));

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

/* A fixed stream of seeds, so every run of this file rolls the same names. */
function seeds(count, from) {
  const rng = Forge.makeRng(from || 20260912);
  const out = [];
  for (let i = 0; i < count; i++) out.push(Math.floor(rng() * 4294967296) >>> 0);
  return out;
}

console.log('\nNAME FORGE — word bank');

test('exactly 1000 adjectives and 1000 nouns', () => {
  eq(WORDS.adjectives.length, 1000, 'adjectives');
  eq(WORDS.nouns.length, 1000, 'nouns');
});

test('no duplicates within either list', () => {
  for (const key of ['adjectives', 'nouns']) {
    const seen = new Set();
    const dupes = [];
    for (const word of WORDS[key]) {
      if (seen.has(word)) dupes.push(word);
      seen.add(word);
    }
    assert(dupes.length === 0, key + ' repeat: ' + dupes.join(', '));
  }
});

test('every word is plain lowercase a-z', () => {
  for (const key of ['adjectives', 'nouns']) {
    const bad = WORDS[key].filter((w) => !/^[a-z]{2,}$/.test(w));
    assert(bad.length === 0, key + ' not plain lowercase: ' + bad.join(', '));
  }
});

console.log('\nNAME FORGE — one name');

test('a name is one adjective and one noun, and nothing else', () => {
  const adjectives = new Set(WORDS.adjectives);
  const nouns = new Set(WORDS.nouns);
  for (const seed of seeds(300)) {
    const name = Forge.generate({ seed });
    assert(adjectives.has(name.adjective), 'not an adjective: ' + name.adjective);
    assert(nouns.has(name.noun), 'not a noun: ' + name.noun);
    eq(name.words.length, 2, 'word count for ' + name.text);
    const pair = [name.adjective, name.noun].sort().join(' ');
    eq(name.words.slice().sort().join(' '), pair, 'words of ' + name.text);
  }
});

test('a word is never paired with itself', () => {
  for (const seed of seeds(2000, 7)) {
    const name = Forge.generate({ seed });
    assert(name.adjective !== name.noun, 'paired with itself: ' + name.text);
  }
});

test('the same seed always rebuilds the same name', () => {
  for (const seed of seeds(50)) {
    eq(Forge.generate({ seed }).text, Forge.generate({ seed }).text, 'seed ' + seed);
  }
});

test('different seeds give different names, near enough always', () => {
  const names = seeds(500, 99).map((seed) => Forge.generate({ seed }).text);
  const unique = new Set(names).size;
  assert(unique > 490, 'only ' + unique + ' of 500 rolls were distinct');
});

console.log('\nNAME FORGE — adjective first, then the noun');

test('every roll comes out adjective first', () => {
  for (const seed of seeds(1000, 4242)) {
    const name = Forge.generate({ seed });
    eq(name.order, 'adjective-noun', 'order of ' + name.text);
    eq(name.words[0], name.adjective, 'first word of ' + name.text);
    eq(name.words[1], name.noun, 'second word of ' + name.text);
  }
});

test('a name reads as "Adjective Noun"', () => {
  for (const seed of seeds(100, 808)) {
    const name = Forge.generate({ seed });
    eq(name.text, Forge.capitalise(name.adjective) + ' ' + Forge.capitalise(name.noun),
      'text for seed ' + seed);
  }
});

test('the order always matches the words actually used', () => {
  for (const seed of seeds(200, 5)) {
    for (const order of ['adjective-noun', 'noun-adjective', 'random']) {
      const name = Forge.generate({ seed, order });
      if (name.order === 'adjective-noun') {
        eq(name.words[0], name.adjective, 'first word of ' + name.text);
        eq(name.words[1], name.noun, 'second word of ' + name.text);
      } else {
        eq(name.words[0], name.noun, 'first word of ' + name.text);
        eq(name.words[1], name.adjective, 'second word of ' + name.text);
      }
    }
  }
});

test('the order can still be asked for either way', () => {
  for (const seed of seeds(100, 11)) {
    const adjFirst = Forge.generate({ seed, order: 'adjective-noun' });
    eq(adjFirst.order, 'adjective-noun', 'pinned adjective-first');
    eq(adjFirst.words[0], adjFirst.adjective, 'pinned adjective-first words');

    const nounFirst = Forge.generate({ seed, order: 'noun-adjective' });
    eq(nounFirst.order, 'noun-adjective', 'pinned noun-first');
    eq(nounFirst.words[0], nounFirst.noun, 'pinned noun-first words');

    // Pinning changes the order, never the pair of words.
    eq(adjFirst.adjective, nounFirst.adjective, 'same adjective either way');
    eq(adjFirst.noun, nounFirst.noun, 'same noun either way');
  }
});

test('asking for "random" hands that one decision back to the dice', () => {
  let adjFirst = 0;
  for (const seed of seeds(1000, 31337)) {
    if (Forge.generate({ seed, order: 'random' }).order === 'adjective-noun') adjFirst++;
  }
  assert(adjFirst > 400 && adjFirst < 600,
    'adjective went first ' + adjFirst + ' times in 1000 — that is not a coin flip');
});

test('an unknown order falls back to adjective first, not to a surprise', () => {
  const orders = new Set(seeds(200, 13).map((seed) =>
    Forge.generate({ seed, order: 'sideways' }).order));
  eq(orders.size, 1, 'unknown order should not produce a mixture');
  eq([...orders][0], 'adjective-noun', 'the fallback order');
});

console.log('\nNAME FORGE — styles, batches and exports');

test('every style formats the same pair its own way', () => {
  const name = Forge.generate({ seed: 1, order: 'adjective-noun' });
  const a = name.words[0];
  const b = name.words[1];
  const cap = Forge.capitalise;
  eq(Forge.format([a, b], 'title'), cap(a) + ' ' + cap(b), 'title');
  eq(Forge.format([a, b], 'fused'), cap(a) + cap(b), 'fused');
  eq(Forge.format([a, b], 'hyphen'), a + '-' + b, 'hyphen');
  eq(Forge.format([a, b], 'snake'), a + '_' + b, 'snake');
  eq(Forge.format([a, b], 'lower'), a + ' ' + b, 'lower');
  eq(Forge.format([a, b], 'shout'), (a + ' ' + b).toUpperCase(), 'shout');
});

test('a name carries the style it was asked for', () => {
  for (const style of Forge.STYLES) {
    const name = Forge.generate({ seed: 77, style: style.id });
    eq(name.style, style.id, 'style id');
    eq(name.text, Forge.format(name.words, style.id), 'text for ' + style.id);
  }
});

test('a batch of 200 is 200 different names', () => {
  const batch = Forge.generateMany(200);
  eq(batch.length, 200, 'batch size');
  eq(new Set(batch.map((n) => n.text.toLowerCase())).size, 200, 'distinct names');
});

test('a whole batch comes out adjective first', () => {
  const batch = Forge.generateMany(400);
  const stragglers = batch.filter((n) => n.words[0] !== n.adjective);
  eq(stragglers.length, 0, 'names that did not lead with their adjective');
});

test('a batch passes an order of its own through to every name', () => {
  const nounFirst = Forge.generateMany(50, { order: 'noun-adjective' });
  eq(nounFirst.filter((n) => n.words[0] === n.noun).length, 50, 'all noun-first');

  const rolled = Forge.generateMany(400, { order: 'random' });
  const adjFirst = rolled.filter((n) => n.order === 'adjective-noun').length;
  assert(adjFirst > 150 && adjFirst < 250,
    'adjective led ' + adjFirst + ' times in 400 — "random" should flip in batches too');
});

test('the advertised number of names is the real one', () => {
  eq(Forge.combinations(), 1000000, 'the default, adjective first');
  eq(Forge.combinations('adjective-noun'), 1000000, 'asked for adjective first');
  eq(Forge.combinations('random'), 2000000, 'either way round');
});

test('exports contain every name, one per line', () => {
  const batch = Forge.generateMany(25);
  const lines = Forge.toText(batch).trim().split('\n');
  eq(lines.length, 25, 'text lines');
  eq(lines[0], batch[0].text, 'first text line');

  const csv = Forge.toCsv(batch).trim().split('\n');
  eq(csv.length, 26, 'csv lines (header + 25)');
  assert(csv[0].startsWith('name,'), 'csv header');
  assert(csv[1].indexOf(batch[0].adjective) !== -1, 'csv row keeps its adjective');
});

console.log('\n' + passed + ' passed, ' + failures.length + ' failed');
if (failures.length) {
  console.error('\nFAILURES:\n  ' + failures.join('\n  '));
  process.exit(1);
}
