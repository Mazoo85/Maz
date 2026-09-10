/*
 * SCRIPT FORGE — logic tests.
 *
 *   node film/tests/film-logic.test.js
 *
 * No dependencies and no browser: the reader, the writer and the exporters are
 * all plain modules, so they can be checked directly. The point of these is
 * that a script is never *broken* — no unfilled placeholders, every scene has a
 * heading and something in it, every spoken line belongs to a character the
 * film introduced, and the same idea always rebuilds the same film.
 */
'use strict';

const path = require('path');
const LEX = require(path.join(__dirname, '..', 'js', 'lexicon.js'));
require(path.join(__dirname, '..', 'js', 'dialogue.js'));
const Parse = require(path.join(__dirname, '..', 'js', 'parse.js'));
const Writer = require(path.join(__dirname, '..', 'js', 'screenplay.js'));
const Format = require(path.join(__dirname, '..', 'js', 'format.js'));

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

/* ------------------------------------------------------------------ reading */
console.log('\nREADING THE IDEA');

test('a named character becomes the lead', () => {
  const p = Parse.parse('A woman named Ada visits her father in the hospital.');
  eq(p.hero.name, 'ADA', 'hero name');
  eq(p.other.role, 'father', 'a possessive role belongs to the other character');
});

test('a place name is not mistaken for a person', () => {
  const p = Parse.parse('Two sisters rob a bank in Anchorage at midnight.');
  assert(p.hero.name !== 'ANCHORAGE', 'Anchorage was read as a character');
  eq(p.time, 'NIGHT', 'midnight is night');
});

test('genre is read from the words', () => {
  eq(Parse.parse('a haunted basement, a ritual, and something with teeth').genre, 'horror');
  eq(Parse.parse('a crew, a vault, and a safe full of diamonds').genre, 'heist');
  eq(Parse.parse('an android on a ship learns it has a memory it was not given').genre, 'scifi');
});

test('the genre a sentence lands on', () => {
  // A word only one genre claims outweighs a word four of them share; a word
  // that is also a *place* outweighs both, because it is the world the film is
  // shot in; and a word that is also a job or relationship counts for less,
  // because it has already told us who is in the film rather than what kind of
  // film it is.
  const cases = [
    ['A kid hears his missing brother on a walkie-talkie in the attic at midnight.', 'horror'],
    ['a ghost in the basement of a haunted house', 'horror'],
    ['two sisters rob a bank vault with a crew', 'heist'],
    ['an android on a ship learns it has a memory it was not given', 'scifi'],
    ['a detective works a cold case with no alibi', 'mystery'],
    ['my boss makes me babysit his dog before the wedding', 'comedy'],
    ['a woman visits her father in the hospital and cannot say goodbye', 'drama'],
    ['a stalker follows her home and the phone is dead', 'thriller'],
    ['a witch in the forest trades a locket for a memory', 'fantasy'],
    ['a sheriff rides into a saloon in the desert', 'western'],
    ['two exes meet again at a wedding and dance', 'romance']
  ];
  cases.forEach(([idea, want]) => eq(Parse.parse(idea).genre, want, '"' + idea.slice(0, 34) + '…"'));
});

test('the logline is written in English', () => {
  // "in a attic", "A android" and "a wire cutters" were all real output once.
  eq(Parse.withArticle('attic'), 'an attic');
  eq(Parse.withArticle('android'), 'an android');
  eq(Parse.withArticle('radio'), 'a radio');
  eq(Parse.withArticle('wire cutters'), 'wire cutters', 'a plural takes no article');
  eq(Parse.withArticle('glass'), 'a glass', 'a word merely ending in s is not plural');

  [ 'a kid in the attic', 'an android on a ship', 'a thief with wire cutters',
    'a lighthouse keeper finds a radio', ''
  ].forEach((idea) => {
    const line = Parse.parse(idea).logline;
    assert(!/\b(?:a) [aeiou]/i.test(line), 'wrong article in: ' + line);
    assert(!/\ban [^aeiou]/i.test(line), 'wrong article in: ' + line);
    assert(/^[A-Z]/.test(line), 'logline does not start with a capital: ' + line);
    assert(/\.$/.test(line), 'logline does not end in a full stop: ' + line);
  });
});

test('a chosen genre overrides the guess', () => {
  const p = Parse.parse('a haunted basement', { genre: 'comedy' });
  eq(p.genre, 'comedy');
  eq(p.genreAuto, false);
});

test('the job in the idea sets the location', () => {
  const p = Parse.parse('a lighthouse keeper who has stopped writing the log');
  eq(p.hero.role, 'lighthouse keeper');
  eq(p.places[0].slug, 'LIGHTHOUSE — LAMP ROOM');
});

test('the object is taken from the sentence', () => {
  eq(Parse.parse('a courier finds a package addressed to herself').object, 'package');
  eq(Parse.parse('a kid hears his brother on a walkie-talkie').object, 'walkie-talkie');
});

test('a location is never used as the object', () => {
  const p = Parse.parse('a thief opens a warehouse on the docks');
  assert(!LEX.PLACES[p.object], 'object came back as a place: ' + p.object);
});

test('empty input still produces a complete premise', () => {
  const p = Parse.parse('');
  assert(p.hero.name && p.other.name && p.object && p.places.length >= 2 && p.title, 'premise incomplete');
  eq(p.empty, true);
});

/* ------------------------------------------------------------------ writing */
console.log('\nWRITING THE SCRIPT');

const sample = Writer.write(Parse.parse("A lonely lighthouse keeper finds a radio that plays tomorrow's news."), { length: 'short' });

test('length picks the number of scenes', () => {
  eq(Writer.write(Parse.parse('a robot'), { length: 'micro' }).scenes.length, 3);
  eq(Writer.write(Parse.parse('a robot'), { length: 'short' }).scenes.length, 5);
  eq(Writer.write(Parse.parse('a robot'), { length: 'festival' }).scenes.length, 7);
});

test('every scene has a slug line and something in it', () => {
  sample.scenes.forEach((scene) => {
    assert(/^(INT\.|EXT\.) .+ — .+$/.test(scene.heading.text), 'bad slug: ' + scene.heading.text);
    assert(scene.elements.filter((e) => e.type === 'action').length >= 2, 'scene ' + scene.number + ' has no action');
    assert(scene.shots.length >= 3, 'scene ' + scene.number + ' has no shots');
  });
});

test('the film opens and closes in the same place', () => {
  const first = sample.scenes[0].heading.place.slug;
  const last = sample.scenes[sample.scenes.length - 1].heading.place.slug;
  eq(last, first, 'closing location');
});

test('it opens on FADE IN: and ends on FADE OUT.', () => {
  eq(sample.elements[0].text, 'FADE IN:');
  eq(sample.elements[sample.elements.length - 1].text, 'FADE OUT.');
});

test('every spoken line belongs to a character in the cast', () => {
  const cast = sample.characters.map((c) => c.name);
  sample.elements.forEach((e) => {
    if (e.type === 'character') assert(cast.indexOf(e.text) !== -1, 'unknown character: ' + e.text);
  });
});

test('dialogue always follows a character cue', () => {
  sample.elements.forEach((e, i) => {
    if (e.type !== 'dialogue') return;
    const before = sample.elements[i - 1];
    assert(before && (before.type === 'character' || before.type === 'parenthetical'),
      'orphan dialogue at ' + i);
  });
});

test('characters are the age of the part they are playing', () => {
  // "ALEX (40s), a kid" was real output. So was a grandmother in her 30s.
  const ageOf = (idea, role) => {
    for (let seed = 0; seed < 25; seed++) {
      const script = Writer.write(Parse.parse(idea, { seed }), { length: 'short', seed });
      const intro = script.elements.find((e) => e.type === 'action' && e.text.indexOf(role) !== -1
        && /\((?:[0-9]+|[a-z ]*[0-9]+s)\)/.test(e.text));
      if (!intro) continue;
      const age = intro.text.match(/\(([^)]+)\)/)[1];
      const years = parseInt(age.replace(/[^0-9]/g, ''), 10);
      assert(!isNaN(years), 'unreadable age "' + age + '" in: ' + intro.text);
      return { age, years, intro: intro.text };
    }
    return null;
  };

  const kid = ageOf('a kid alone in the attic at midnight', 'kid');
  assert(kid, 'no kid was introduced');
  assert(kid.years <= 17, 'a kid is ' + kid.age + ': ' + kid.intro);

  const gran = ageOf('a grandmother finds a recipe card', 'grandmother');
  assert(gran, 'no grandmother was introduced');
  assert(gran.years >= 60, 'a grandmother is ' + gran.age + ': ' + gran.intro);
});

test('an introduction reads as English', () => {
  const script = Writer.write(Parse.parse('an android on a ship finds a recording'), { length: 'short' });
  script.elements.filter((e) => e.type === 'action').forEach((e) => {
    assert(!/\ba [aeiou]/i.test(e.text), 'wrong article: ' + e.text);
    assert(!/\ban [^aeiou]/i.test(e.text), 'wrong article: ' + e.text);
  });
});

test('a film does not use the same sound cue twice running', () => {
  ['micro', 'short', 'festival'].forEach((length) => {
    for (let seed = 0; seed < 8; seed++) {
      const script = Writer.write(Parse.parse('a ghost in the attic', { seed }), { length, seed });
      const cues = (script.elements.map((e) => e.text).join(' ')
        .match(/From somewhere close: ([^.]+)\./g) || []);
      for (let i = 1; i < cues.length; i++) {
        assert(cues[i] !== cues[i - 1], 'the same sound twice running: ' + cues[i]);
      }
    }
  });
});

test('the same idea rebuilds the same script', () => {
  const a = Writer.write(Parse.parse('a heist at a bank'), { length: 'short' });
  const b = Writer.write(Parse.parse('a heist at a bank'), { length: 'short' });
  eq(JSON.stringify(b.elements), JSON.stringify(a.elements));
});

test('a different seed gives a different take on the same idea', () => {
  const a = Writer.write(Parse.parse('a heist at a bank', { seed: 1 }), { length: 'short', seed: 1 });
  const b = Writer.write(Parse.parse('a heist at a bank', { seed: 2 }), { length: 'short', seed: 2 });
  assert(JSON.stringify(a.elements) !== JSON.stringify(b.elements), 'seeds produced identical scripts');
});

test('a title you supply is the title you get', () => {
  const p = Parse.parse('a robot', { title: 'THE LONG WAY DOWN' });
  eq(Writer.write(p, { length: 'micro' }).title, 'THE LONG WAY DOWN');
});

/* --------------------------------------------------------------- exporting */
console.log('\nEXPORTING');

test('fountain carries a title page and the scenes', () => {
  const f = Format.toFountain(sample);
  assert(f.indexOf('Title: **' + sample.title + '**') === 0, 'no title page');
  assert(f.indexOf('Author: SCRIPT FORGE') !== -1, 'no author line');
  sample.scenes.forEach((s) => assert(f.indexOf(s.heading.text.toUpperCase()) !== -1, 'missing ' + s.heading.text));
  assert(/\n> FADE OUT\.\n/.test(f), 'transition is not forced');
});

test('plain text stays inside the page width', () => {
  Format.toText(sample).split('\n').forEach((line) => {
    assert(line.length <= 78, 'line too wide (' + line.length + '): ' + line);
  });
});

test('final draft xml is balanced', () => {
  const x = Format.toFdx(sample);
  const open = (x.match(/<Paragraph /g) || []).length;
  const close = (x.match(/<\/Paragraph>/g) || []).length;
  eq(open, close, 'paragraph tags');
  eq(open, sample.elements.length, 'one paragraph per element');
  assert(x.indexOf('<FinalDraft') !== -1 && x.indexOf('</FinalDraft>') !== -1, 'no document element');
});

test('the shot list covers every scene', () => {
  const md = Format.toShotList(sample);
  sample.scenes.forEach((s) => assert(md.indexOf('### ' + s.number + '. ' + s.heading.text) !== -1,
    'scene ' + s.number + ' missing from the shot list'));
});

test('filenames are safe', () => {
  eq(Format.slugify('WHAT THE RADIO KNEW'), 'what-the-radio-knew');
  eq(Format.slugify('!!!'), 'script');
});

/* -------------------------------------------------------------- robustness */
console.log('\nROBUSTNESS');

test('no placeholder ever reaches the page', () => {
  const ideas = [
    '', 'robot', 'a', 'the', '???', 'A LOUD ALL CAPS IDEA ABOUT A DOG',
    'a lighthouse keeper', 'my sister and her boss and a bank vault',
    'a ghost, a kid, a walkie-talkie, an attic, midnight, snow',
    'Ada and Bishop steal a train in the desert',
    'a very long idea '.repeat(40)
  ];
  const lengths = ['micro', 'short', 'festival'];
  ideas.forEach((idea) => {
    lengths.forEach((length) => {
      for (let seed = 0; seed < 12; seed++) {
        const script = Writer.write(Parse.parse(idea, { seed }), { length, seed });
        const all = script.elements.map((e) => e.text).join('\n') + '\n' +
          script.scenes.map((s) => s.shots.join('\n')).join('\n') + '\n' +
          script.title + '\n' + script.logline;
        assert(!/\{[A-Z_]+\}/.test(all), 'unfilled slot for "' + idea.slice(0, 24) + '": ' +
          (all.match(/\{[A-Z_]+\}/) || [''])[0]);
        assert(!/\bundefined\b|\bnull\b|NaN/.test(all), 'undefined leaked for "' + idea.slice(0, 24) + '"');
        Format.toFountain(script);
        Format.toFdx(script);
        Format.toShotList(script);
        Format.toText(script);
      }
    });
  });
});

test('runtime and page count are sane', () => {
  ['micro', 'short', 'festival'].forEach((length) => {
    const s = Writer.write(Parse.parse('a kid and a walkie-talkie'), { length });
    assert(s.pages > 0.5 && s.pages < 20, 'page count off: ' + s.pages);
    assert(/^≈ \d+ min$/.test(s.runtime), 'runtime looks wrong: ' + s.runtime);
  });
});


/* ====================================================================== film
 * The reel is the edit — every shot, its length and what is heard over it —
 * and it is pure data, so the whole cut of a film can be checked here without
 * a browser. The container patcher is checked against a synthetic WebM.
 */
const Reel = require(path.join(__dirname, '..', 'js', 'film-reel.js'));
const Art = require(path.join(__dirname, '..', 'js', 'film-art.js'));
const Score = require(path.join(__dirname, '..', 'js', 'film-audio.js'));
const PlayerLib = require(path.join(__dirname, '..', 'js', 'film-player.js'));
const Webm = require(path.join(__dirname, '..', 'js', 'film-webm.js'));

console.log('\nCUTTING THE FILM');

const reel = Reel.build(sample);

test('the reel opens on a title card and closes on THE END', () => {
  eq(reel.shots[0].kind, 'title');
  eq(reel.shots[0].caption, sample.title);
  eq(reel.shots[reel.shots.length - 1].kind, 'end');
  eq(reel.shots[reel.shots.length - 1].caption, 'THE END');
});

test('shots run back to back with no gaps or overlaps', () => {
  let cursor = 0;
  reel.shots.forEach((shot, i) => {
    assert(Math.abs(shot.start - cursor) < 1e-9, `shot ${i} starts at ${shot.start}, expected ${cursor}`);
    assert(shot.duration > 0.5, `shot ${i} is ${shot.duration}s — too short to read`);
    cursor += shot.duration;
  });
  assert(Math.abs(reel.duration - cursor) < 1e-9, 'reel duration does not match its shots');
});

test('every line of dialogue makes it into the film', () => {
  const spoken = sample.elements.filter((e) => e.type === 'dialogue').map((e) => e.text);
  const onScreen = reel.shots.filter((s) => s.kind === 'line').map((s) => s.caption);
  eq(onScreen.length, spoken.length, 'line count');
  spoken.forEach((line, i) => eq(onScreen[i], line, 'line ' + i));
});

test('a line is always attributed to the character who said it', () => {
  const cast = sample.characters.map((c) => c.name);
  reel.shots.filter((s) => s.kind === 'line').forEach((shot) => {
    assert(cast.indexOf(shot.speaker) !== -1, 'unknown speaker: ' + shot.speaker);
    assert(shot.characters.indexOf(shot.speaker) !== -1, 'the speaker is not in frame');
  });
});

test('captions are on screen long enough to read', () => {
  reel.shots.forEach((shot) => {
    if (!shot.caption || shot.kind === 'establish') return;
    const words = shot.caption.trim().split(/\s+/).length;
    // Comfortable reading is about four words a second; we allow three.
    assert(shot.duration >= words / 3.2, 
      `"${shot.caption.slice(0, 30)}…" — ${words} words in ${shot.duration.toFixed(1)}s`);
  });
});

test('the film runs about as long as the script says it does', () => {
  const claimed = parseInt(sample.runtime.replace(/[^0-9]/g, ''), 10) * 60;
  assert(Math.abs(reel.duration - claimed) < claimed * 0.6 + 40,
    `reel is ${reel.duration.toFixed(0)}s but the script claims ${claimed}s`);
});

test('CONTINUOUS is turned into a real time of day for the lighting', () => {
  const times = new Set(reel.shots.map((s) => s.time));
  ['CONTINUOUS', 'LATER'].forEach((word) => assert(!times.has(word), word + ' reached the artist'));
  times.forEach((t) => assert(['DAY', 'NIGHT', 'DUSK', 'DAWN'].indexOf(t) !== -1, 'odd time: ' + t));
});

test('every location maps to a set that can be drawn', () => {
  Object.keys(Reel.SET_BY_PLACE).forEach((place) => {
    const set = Reel.SET_BY_PLACE[place];
    assert(typeof Art.SETS[set] === 'function', place + ' maps to "' + set + '", which nothing draws');
  });
  // And every place in the lexicon resolves, mapped or not.
  Object.keys(LEX.PLACES).forEach((key) => {
    const set = Reel.setFor({ key, int: LEX.PLACES[key].int });
    assert(typeof Art.SETS[set] === 'function', key + ' resolves to an undrawable set');
  });
});

test('the two characters get voices that tell them apart', () => {
  const voices = Object.keys(reel.voices).map((k) => reel.voices[k]);
  eq(voices.length, 2);
  assert(Math.abs(voices[0].pitch - voices[1].pitch) > 20, 'the two voices are too close to tell apart');
  voices.forEach((v) => {
    assert(v.pitch > 80 && v.pitch < 260, 'voice pitch out of range: ' + v.pitch);
    assert(v.hue >= 0 && v.hue < 360, 'bad hue: ' + v.hue);
  });
});

test('every genre has a palette and a piece of music', () => {
  Object.keys(LEX.GENRES).forEach((genre) => {
    const pal = Art.palette(genre, 'NIGHT', 0.5);
    ['key', 'sky', 'deep', 'ink', 'shadow'].forEach((tone) => {
      assert(Array.isArray(pal[tone]) && pal[tone].length === 3, genre + ' has no ' + tone);
    });
    // A night frame has to separate its tones or it is just a black rectangle.
    const lum = (c) => c[0] * 0.3 + c[1] * 0.6 + c[2] * 0.1;
    assert(lum(pal.key) - lum(pal.shadow) > 60, genre + ' has no contrast at night');
    assert(lum(pal.deep) > lum(pal.shadow), genre + ' ground is darker than its shadows');
    assert(Score.MUSIC[genre], genre + ' has no music');
  });
});

test('the camera never leaves the frame empty', () => {
  reel.shots.forEach((shot) => {
    const f = PlayerLib.framingFor(shot, 0.5);
    assert(f.zoom >= 0.9 && f.zoom <= 3, 'odd zoom on shot ' + shot.index + ': ' + f.zoom);
    assert(Math.abs(f.panX) < 0.35 && Math.abs(f.panY) < 0.35, 'camera panned off the set');
    if (shot.kind === 'line') {
      const layout = PlayerLib.figureLayout(shot);
      assert(layout.length >= 1, 'nobody in frame for a spoken line');
    }
  });
});

test('the film fades up from black and out to it', () => {
  assert(PlayerLib.fadeAmount(reel, reel.shots[0], 0) > 0.9, 'no fade in');
  assert(PlayerLib.fadeAmount(reel, reel.shots[reel.shots.length - 1], reel.duration - 0.01) > 0.9, 'no fade out');
  const middle = reel.shots[Math.floor(reel.shots.length / 2)];
  eq(PlayerLib.fadeAmount(reel, middle, middle.start + middle.duration / 2), 0, 'the middle of a shot is not black');
});

test('longer films are longer', () => {
  const lengths = ['micro', 'short', 'festival'].map((length) =>
    Reel.build(Writer.write(Parse.parse('a heist at a bank'), { length })).duration);
  assert(lengths[0] < lengths[1] && lengths[1] < lengths[2], 'lengths did not increase: ' + lengths);
});

test('any idea, any length, makes a playable reel', () => {
  ['', 'robot', 'a ghost in the attic', 'two sisters rob a bank at midnight',
   'a very long idea '.repeat(30)].forEach((idea) => {
    ['micro', 'short', 'festival'].forEach((length) => {
      const r = Reel.build(Writer.write(Parse.parse(idea, { seed: 5 }), { length, seed: 5 }));
      assert(r.duration > 20, 'reel too short for "' + idea.slice(0, 20) + '"');
      assert(r.shots.every((s) => s.set && s.time && s.framing && s.camera), 'a shot is missing its setup');
      assert(!/\{[A-Z_]+\}/.test(r.shots.map((s) => s.caption).join(' ')), 'unfilled slot in a caption');
    });
  });
});

console.log('\nWRITING THE VIDEO FILE');

test('a duration is spliced into a file that has none', () => {
  const before = Webm.fixture(false);
  eq(Webm.readDuration(before), null, 'the fixture should start with no duration');
  const after = Webm.patch(before, 81.4);
  assert(Math.abs(Webm.readDuration(after) - 81.4) < 0.01, 'duration not written');
  eq(after.length - before.length, 11, 'a spliced duration is 11 bytes');
});

test('an existing duration is overwritten in place', () => {
  const before = Webm.fixture(true);
  const after = Webm.patch(before, 42.5);
  assert(Math.abs(Webm.readDuration(after) - 42.5) < 0.01, 'duration not replaced');
  eq(after.length, before.length, 'the file should not grow');
});

test('everything after the patch is left byte-for-byte alone', () => {
  const before = Webm.fixture(false);
  const after = Webm.patch(before, 12);
  // The cluster is the last 15 bytes of the fixture: the picture data must survive.
  const tailBefore = Array.from(before.slice(-15)).join(',');
  const tailAfter = Array.from(after.slice(-15)).join(',');
  eq(tailAfter, tailBefore, 'the recorded data was disturbed');
});

test('a file it does not understand is handed back untouched', () => {
  const junk = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]);
  assert(Webm.patch(junk, 10) === junk, 'junk was modified');
  const empty = new Uint8Array(0);
  assert(Webm.patch(empty, 10) === empty, 'an empty file was modified');
  const good = Webm.fixture(false);
  assert(Webm.patch(good, 0) === good, 'a zero duration was written');
  assert(Webm.patch(good, -5) === good, 'a negative duration was written');
});

test('variable-length integers round-trip', () => {
  [0, 1, 42, 126, 127, 128, 16000, 2097150, 5000000].forEach((n) => {
    const encoded = Webm.writeVint(n);
    assert(encoded, 'could not encode ' + n);
    const decoded = Webm.readVint(encoded, 0, false);
    eq(decoded.value, n, 'round trip for ' + n);
    eq(decoded.length, encoded.length, 'length for ' + n);
  });
});

/* ------------------------------------------------------------------ report */
console.log('');
if (failures.length) {
  console.error('✖ ' + failures.length + ' failing test(s):');
  failures.forEach((f) => console.error('  - ' + f));
  process.exit(1);
}
console.log('✓ ' + passed + ' tests passed\n');
