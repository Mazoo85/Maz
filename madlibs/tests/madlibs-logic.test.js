/*
 * MADLIBS STORY FORGE — logic tests.
 *
 *   node madlibs/tests/madlibs-logic.test.js
 *
 * No dependencies and no browser: the dictionary, the templates and the
 * generator are plain modules, so they can be checked directly.
 *
 * What these are for. MADLIBS publishes `madlibs/storyideas` and SCRIPT FORGE
 * consumes it — an empty idea box there borrows one of these stories — so a
 * break here is a break in two projects. The invariants worth holding are the
 * ones a reader would notice immediately: a story never shows its own
 * scaffolding ({noun} on the page), the same seed always rebuilds the same
 * story (that is what makes one shareable), and a word tagged as the hero is
 * the same person in every beat.
 */
'use strict';

const path = require('path');
const DICT = require(path.join(__dirname, '..', 'js', 'dictionary.js'));
const TEMPLATES = require(path.join(__dirname, '..', 'js', 'templates.js'));
const Gen = require(path.join(__dirname, '..', 'js', 'generator.js'));

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

/* An unresolved placeholder, in any of the forms templates.js can write. */
const LEFTOVER = /\{[A-Za-z][A-Za-z0-9#-]*\}/;

/* ------------------------------------------------------------- the corpus */
console.log('\nTHE DICTIONARY AND THE TEMPLATES');

test('every dictionary category has words in it', () => {
  const empty = Object.keys(DICT).filter((k) => !Array.isArray(DICT[k]) || DICT[k].length === 0);
  eq(empty.join(', '), '', 'these categories are empty');
});

test('no dictionary category repeats a word', () => {
  // A duplicate does not break anything, it just quietly makes that word twice
  // as likely as its neighbours — which is the kind of bias nobody chooses.
  const dupes = [];
  for (const [category, words] of Object.entries(DICT)) {
    const seen = new Set();
    for (const w of words) {
      if (seen.has(w)) dupes.push(category + ':' + w);
      seen.add(w);
    }
  }
  eq(dupes.join(', '), '', 'repeated words');
});

test('every template is complete', () => {
  assert(TEMPLATES.length > 0, 'there are no templates at all');
  TEMPLATES.forEach((t, i) => {
    assert(t.id, `template ${i} has no id`);
    assert(t.title, `${t.id} has no title`);
    assert(t.genre, `${t.id} has no genre`);
    assert(Array.isArray(t.beats) && t.beats.length > 0, `${t.id} has no beats`);
    t.beats.forEach((b, j) => {
      assert(b.label, `${t.id} beat ${j} has no label`);
      assert(b.text && b.text.trim(), `${t.id} beat ${j} has no text`);
    });
  });
});

test('no two templates share an id', () => {
  const seen = new Set();
  const dupes = TEMPLATES.map((t) => t.id).filter((id) => (seen.has(id) ? true : (seen.add(id), false)));
  eq(dupes.join(', '), '', 'duplicate template ids');
});

test('every placeholder names a real dictionary category', () => {
  // This is the check that catches a typo the moment it is written. Without it
  // a mistyped {charcter} survives all the way to the page, where it renders as
  // the literal text "{charcter}".
  const RE = /\{([A-Za-z][A-Za-z]*)(?:#[A-Za-z0-9]+)?\}/g;
  const unknown = new Set();
  for (const t of TEMPLATES) {
    for (const b of t.beats) {
      let m;
      RE.lastIndex = 0;
      while ((m = RE.exec(b.text)) !== null) {
        const category = m[1];
        if (category === 'a') continue;           // the article helper, not a lookup
        if (!DICT[category]) unknown.add(`${t.id}:{${category}}`);
      }
    }
  }
  eq([...unknown].join(', '), '', 'placeholders with no category behind them');
});

/* ---------------------------------------------------------- filling in */
console.log('\nFILLING A STORY IN');

test('a generated story shows none of its own scaffolding', () => {
  for (let seed = 0; seed < 300; seed++) {
    const story = Gen.generate({ seed });
    for (const beat of story.beats) {
      assert(!LEFTOVER.test(beat.text),
        `seed ${seed} (${story.id}) left a placeholder: ${beat.text}`);
    }
  }
});

test('every beat has a label and words under it', () => {
  const story = Gen.generate({ seed: 7 });
  assert(story.beats.length > 0, 'no beats');
  story.beats.forEach((b) => {
    assert(b.label && b.label.trim(), 'a beat lost its label');
    assert(b.text && b.text.trim().length > 10, 'a beat is empty or nearly so: ' + b.text);
  });
});

test('every beat starts with a capital letter', () => {
  for (let seed = 0; seed < 60; seed++) {
    for (const b of Gen.generate({ seed }).beats) {
      const first = b.text.trim()[0];
      assert(first === first.toUpperCase(), `seed ${seed}: "${b.text.slice(0, 40)}"`);
    }
  }
});

/* -------------------------------------------------------- reproducibility */
console.log('\nTHE SAME SEED REBUILDS THE SAME STORY');

test('one seed always gives the identical story', () => {
  // This is what makes a story shareable: the app stores the seed, not the
  // text, and SCRIPT FORGE borrows a story the same way.
  for (const seed of [0, 1, 42, 1234567, 4294967295]) {
    const a = Gen.generate({ seed });
    const b = Gen.generate({ seed });
    eq(JSON.stringify(a), JSON.stringify(b), `seed ${seed} drifted`);
  }
});

test('the seed the story reports is the seed that reproduces it', () => {
  const first = Gen.generate({});
  const again = Gen.generate({ seed: first.seed });
  eq(JSON.stringify(again), JSON.stringify(first), 'a story could not rebuild itself');
});

test('different seeds give different stories', () => {
  const seen = new Set();
  for (let seed = 0; seed < 200; seed++) seen.add(Gen.generate({ seed }).signature);
  assert(seen.size > 190, `200 seeds produced only ${seen.size} distinct stories`);
});

test('makeRng is deterministic and stays inside [0, 1)', () => {
  const a = Gen.makeRng(99);
  const b = Gen.makeRng(99);
  for (let i = 0; i < 500; i++) {
    const v = a();
    eq(v, b(), 'two generators on the same seed diverged');
    assert(v >= 0 && v < 1, 'out of range: ' + v);
  }
});

/* ------------------------------------------------------------ tagged words */
console.log('\nTAGGED WORDS STAY THE SAME PERSON');

test('a tagged placeholder resolves to one word across every beat', () => {
  // {name#hero} in beat one and beat four must be the same character, or the
  // story stops being about anyone.
  const tagged = TEMPLATES.find((t) =>
    t.beats.filter((b) => /\{[A-Za-z]+#hero\}/.test(b.text)).length > 1);
  assert(tagged, 'no template reuses a #hero tag — this test needs one to check');

  const beatsWithTag = tagged.beats
    .map((b, i) => ({ i, text: b.text }))
    .filter((b) => /\{[A-Za-z]+#hero\}/.test(b.text));
  const category = beatsWithTag[0].text.match(/\{([A-Za-z]+)#hero\}/)[1];

  for (let seed = 0; seed < 50; seed++) {
    const story = Gen.fillTemplate(tagged, { seed });
    // Whichever word was chosen, it must appear in every beat that asked for it.
    const candidates = DICT[category].filter((w) => story.beats[beatsWithTag[0].i].text.includes(w));
    assert(candidates.length > 0, `seed ${seed}: the tagged word is not in the beat at all`);
    for (const b of beatsWithTag.slice(1)) {
      assert(candidates.some((w) => story.beats[b.i].text.includes(w)),
        `seed ${seed}: beat ${b.i} names a different ${category} than beat ${beatsWithTag[0].i}`);
    }
  }
});

test('fillTemplate on the same seed and template repeats exactly', () => {
  const t = TEMPLATES[0];
  eq(JSON.stringify(Gen.fillTemplate(t, { seed: 5 })),
     JSON.stringify(Gen.fillTemplate(t, { seed: 5 })));
});

/* ---------------------------------------------------------------- articles */
console.log('\nARTICLES AND GRAMMAR');

test('{a} becomes "an" before a vowel and "a" before a consonant', () => {
  const vowelWord = DICT.noun.find((w) => 'aeiou'.includes(w[0].toLowerCase()));
  const consonantWord = DICT.noun.find((w) => !'aeiou'.includes(w[0].toLowerCase()));
  assert(vowelWord && consonantWord, 'the noun list needs both to check this');

  const made = Gen.fillTemplate(
    { id: 'x', title: 'x', genre: 'x', beats: [{ label: 'B', text: 'there was {a} {noun} here.' }] },
    { seed: 1 }
  );
  const text = made.beats[0].text;
  const article = text.match(/^There was (an?) (\S+)/);
  assert(article, 'the article was not rendered at all: ' + text);
  const expected = 'aeiou'.includes(article[2][0].toLowerCase()) ? 'an' : 'a';
  eq(article[1], expected, `"${article[1]} ${article[2]}" reads wrong`);
});

test('{a-cap} capitalises the article', () => {
  const made = Gen.fillTemplate(
    { id: 'x', title: 'x', genre: 'x', beats: [{ label: 'B', text: '{a-cap} {noun} waits.' }] },
    { seed: 3 }
  );
  assert(/^An? /.test(made.beats[0].text) === false || /^(A|An) /.test(made.beats[0].text),
    'the article was not capitalised: ' + made.beats[0].text);
  assert(/^(A|An) /.test(made.beats[0].text), made.beats[0].text);
});

/* ------------------------------------------------------------- the catalogue */
console.log('\nGENRES, BATCHES AND EXPORT');

test('asking for a genre gives that genre', () => {
  const genres = [...new Set(TEMPLATES.map((t) => t.genre))];
  assert(genres.length > 1, 'there is only one genre to choose between');
  for (const genre of genres) {
    for (let seed = 0; seed < 20; seed++) {
      eq(Gen.generate({ genre, seed }).genre, genre, `seed ${seed} escaped its genre`);
    }
  }
});

test('an unknown genre falls back rather than failing', () => {
  const story = Gen.generate({ genre: 'no-such-genre', seed: 1 });
  assert(story && story.beats.length > 0, 'nothing came back');
  assert(!LEFTOVER.test(story.beats[0].text), 'the fallback story is unfilled');
});

test('generateMany returns the number asked for, all different', () => {
  const many = Gen.generateMany(120);
  eq(many.length, 120, 'wrong number of stories');
  eq(new Set(many.map((s) => s.signature)).size, 120, 'the batch repeats itself');
});

test('generateMany stops instead of spinning when a genre is nearly exhausted', () => {
  // The cap is what stops "give me 5000 stories" from hanging on a genre that
  // cannot produce them. It must return short, not loop.
  const genre = TEMPLATES[0].genre;
  const many = Gen.generateMany(50, { genre, attemptsCap: 60 });
  assert(many.length <= 50, 'returned more than asked for');
  assert(many.length > 0, 'the cap starved it completely');
});

test('toMarkdown carries the title, the genre, the seed and every beat', () => {
  const story = Gen.generate({ seed: 11 });
  const md = Gen.toMarkdown(story);
  assert(md.startsWith('# ' + story.title), 'no title heading');
  assert(md.includes(story.genre), 'the genre is missing');
  assert(md.includes(String(story.seed)), 'the seed is missing, so this cannot be rebuilt');
  for (const b of story.beats) {
    assert(md.includes(b.label), 'a beat label is missing: ' + b.label);
    assert(md.includes(b.text), 'a beat is missing from the export');
  }
});

test('estimateCombinations reports a real, enormous number', () => {
  const est = Gen.estimateCombinations();
  assert(Number.isFinite(est.log10), 'not a finite log: ' + est.log10);
  assert(est.log10 > 6, 'suspiciously few combinations: 10^' + est.log10);
  assert(/^\d\.\d × 10\^\d+$/.test(est.pretty), 'unreadable: ' + est.pretty);
});

test('pick on an unknown category returns the placeholder rather than crashing', () => {
  eq(Gen.pick('no-such-category', Gen.makeRng(1)), '{no-such-category}');
});

/* -------------------------------------------------- lending the filler out */
console.log('\nFILLING SOMEBODY ELSE\'S TEMPLATE');

test('fillTemplate can be given another project\'s dictionary', () => {
  // The machinery is worth more than the vocabulary it grew up with. Welded to
  // MADLIBS_DICT it could only ever write MADLIBS stories; taking a dictionary,
  // it can fill anyone's templates — which is the whole of madlibs/templates.
  const dict = { colour: ['orange', 'black'], thing: ['fish', 'tower'] };
  const tpl = { id: 't', title: 'T', genre: 'g', beats: [{ label: 'B', text: '{colour} {thing}.' }] };
  const seen = new Set();
  for (let seed = 0; seed < 40; seed++) {
    const text = Gen.fillTemplate(tpl, { seed, dict }).beats[0].text;
    assert(!LEFTOVER.test(text), 'a borrowed dictionary left a placeholder: ' + text);
    seen.add(text);
    const words = text.replace('.', '').toLowerCase().split(' ');
    assert(dict.colour.includes(words[0]), 'word came from somewhere other than the given dictionary: ' + text);
    assert(dict.thing.includes(words[1]), 'word came from somewhere other than the given dictionary: ' + text);
  }
  assert(seen.size > 1, 'a borrowed dictionary produced only one result');
});

test('a borrowed dictionary gets the articles right too', () => {
  // This is the reason to lend it rather than let each project write its own.
  // CODA PICS' four-line copy always wrote "a", so a palette name beginning
  // with a vowel came out as "a orange fish".
  const dict = { colour: ['orange', 'azure', 'black'], thing: ['fish'] };
  const tpl = { id: 't', title: 'T', genre: 'g', beats: [{ label: 'B', text: '{a} {colour} {thing}.' }] };
  for (let seed = 0; seed < 60; seed++) {
    const text = Gen.fillTemplate(tpl, { seed, dict }).beats[0].text;
    assert(!/\ba [aeiou]/i.test(text), 'wrong article: ' + text);
    assert(/^(A|An) /.test(text), 'no article at all: ' + text);
  }
});

test('a borrowed dictionary keeps tagged words consistent', () => {
  const dict = { name: ['Ada', 'Bo', 'Cass'], place: ['Rome', 'Oslo'] };
  const tpl = { id: 't', title: 'T', genre: 'g', beats: [
    { label: 'One', text: '{name#hero} arrives in {place#home}.' },
    { label: 'Two', text: '{name#hero} leaves {place#home} behind.' }
  ] };
  for (let seed = 0; seed < 30; seed++) {
    const b = Gen.fillTemplate(tpl, { seed, dict }).beats;
    const hero = b[0].text.split(' ')[0];
    assert(b[1].text.startsWith(hero), 'the hero changed between beats: ' + b[0].text + ' / ' + b[1].text);
  }
});

test('omitting the dictionary still uses MADLIBS\' own', () => {
  const story = Gen.fillTemplate(TEMPLATES[0], { seed: 5 });
  assert(!LEFTOVER.test(story.beats[0].text), 'the default dictionary stopped working');
  const same = Gen.fillTemplate(TEMPLATES[0], { seed: 5, dict: null });
  eq(same.beats[0].text, story.beats[0].text, 'an explicit null dictionary should mean "use mine"');
});

test('pick takes a dictionary too, and still falls back without one', () => {
  eq(Gen.pick('fruit', Gen.makeRng(1), { fruit: ['plum'] }), 'plum');
  eq(Gen.pick('fruit', Gen.makeRng(1)), '{fruit}', 'MADLIBS has no "fruit", so the placeholder comes back');
});

/* ------------------------------------------------------------------ result */
console.log('');
if (failures.length) {
  console.log('✗ ' + failures.length + ' failed, ' + passed + ' passed\n');
  failures.forEach((f) => console.log('  - ' + f));
  process.exit(1);
}
console.log('✓ ' + passed + ' tests passed\n');
