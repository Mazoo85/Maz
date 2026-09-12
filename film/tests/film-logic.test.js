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
const DLG = require(path.join(__dirname, '..', 'js', 'dialogue.js'));
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

test('what the hero wants only ever follows "to"', () => {
  // Every want in the lexicon is a bare verb phrase — "find what went missing",
  // "be forgiven" — so it reads as English after "to" and nowhere else.
  // "finally tells the truth about find what went missing" was real output.
  const sources = [];
  LEX.BEATS.forEach((beat) => beat.action.forEach((line) => sources.push(['beat ' + beat.id, line])));
  Object.keys(DLG.SHARED).forEach((beat) => DLG.SHARED[beat].forEach((exchange) =>
    exchange.forEach((line) => sources.push(['dialogue ' + beat, line.line]))));
  Object.keys(DLG.BY_GENRE).forEach((genre) => Object.keys(DLG.BY_GENRE[genre]).forEach((beat) =>
    DLG.BY_GENRE[genre][beat].forEach((exchange) => exchange.forEach((line) =>
      sources.push([genre + ' ' + beat, line.line])))));

  let checked = 0;
  sources.forEach(([where, line]) => {
    let at = line.indexOf('{WANT}');
    while (at !== -1) {
      checked++;
      assert(line.slice(Math.max(0, at - 3), at) === 'to ',
        where + ': {WANT} must follow "to " — "' + line + '"');
      at = line.indexOf('{WANT}', at + 1);
    }
  });
  assert(checked > 0, 'no template uses {WANT} at all');

  // And prove it end to end: every want, rendered, reads as a sentence.
  LEX.WANTS.forEach((entry) => {
    const idea = entry.keys[0] + ' in a kitchen';
    const script = Writer.write(Parse.parse(idea), { length: 'festival' });
    script.elements.forEach((e) => {
      assert(!/truth about (?:find|get|say|take|prove|be|win|hold|stop|make|tell)\b/.test(e.text),
        'ungrammatical want: ' + e.text);
    });
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
const Sets = require(path.join(__dirname, '..', 'js', 'film-sets.js'));
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
  function drawable(name) {
    const entry = Sets.SETS[name];
    return !!entry && ['back', 'mid', 'fore'].every((layer) => typeof entry[layer] === 'function');
  }
  Object.keys(Reel.SET_BY_PLACE).forEach((place) => {
    const set = Reel.SET_BY_PLACE[place];
    assert(drawable(set), place + ' maps to "' + set + '", which nothing draws');
  });
  // And every place in the lexicon resolves, mapped or not.
  Object.keys(LEX.PLACES).forEach((key) => {
    const set = Reel.setFor({ key, int: LEX.PLACES[key].int });
    assert(drawable(set), key + ' resolves to an undrawable set');
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
  // `plane()` multiplies panX (and panYMove) by up to PARALLAX.fore before it
  // ever reaches the screen, so a raw panX check against the same bound
  // `plane()` itself uses is checking the wrong number — this checks what a
  // viewer actually sees on the widest-swinging plane instead. And a single
  // progress = 0.5 sample missed the camera moves that peak elsewhere in the
  // shot (the whip pan is at 0.043 of its full swing by the midpoint), so
  // every move gets checked across the whole shot, not just its middle.
  const FORE = Sets.PARALLAX.fore;
  reel.shots.forEach((shot) => {
    [0, 0.15, 0.3, 0.5, 0.75, 1].forEach((progress) => {
      const time = shot.start + progress * shot.duration;
      const f = PlayerLib.framingFor(shot, progress, time);
      assert(f.zoom >= 0.9 && f.zoom <= 3,
        'odd zoom on shot ' + shot.index + ' at progress ' + progress + ': ' + f.zoom);
      const screenPanX = Math.abs(f.panX) * FORE;
      const screenPanY = Math.abs(f.panY) + Math.abs(f.panYMove) * FORE;
      assert(screenPanX < 0.5 && screenPanY < 0.5,
        'camera panned off the set on shot ' + shot.index + ' at progress ' + progress +
        ' (fore-plane screen panX ' + screenPanX.toFixed(3) + ', panY ' + screenPanY.toFixed(3) + ')');
    });
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

console.log('\nCHOOSING A VIDEO FORMAT');

test('a bare "video/mp4" claim is never trusted', () => {
  // A browser can answer yes to the bare type and then write VP9 into an MP4
  // wrapper — a .mp4 an iPhone cannot play. Measured, not guessed: headless
  // Chromium does exactly this. Only an explicit H.264 string is a promise.
  const liar = (type) => type === 'video/mp4' || type.indexOf('video/webm') === 0;
  const chosen = PlayerLib.pickMimeType(liar);
  eq(chosen.container, 'webm', 'a bare mp4 claim was believed');
  assert(chosen.type.indexOf('codecs=') !== -1, 'chose a container with no codecs named');
  eq(chosen.playsOnApple, false);
});

test('an MP4 whose audio codec is unnamed is refused', () => {
  // A browser offering H.264 but no explicit AAC gets no MP4 from us.
  const videoOnly = (type) =>
    (type.indexOf('avc1') !== -1 && type.indexOf('mp4a') === -1 && type.indexOf('aac') === -1) ||
    type.indexOf('webm') !== -1;
  eq(PlayerLib.pickMimeType(videoOnly).container, 'webm', 'took an mp4 with unnamed audio');
});

test('H.264 in MP4 is preferred when the browser really has it', () => {
  const realChrome = (type) => type.indexOf('avc1') !== -1 || type.indexOf('webm') !== -1;
  const chosen = PlayerLib.pickMimeType(realChrome);
  eq(chosen.container, 'mp4');
  eq(chosen.extension, '.mp4');
  eq(chosen.playsOnApple, true, 'an H.264 mp4 must be marked as playing on Apple devices');
  assert(chosen.type.indexOf('avc1') !== -1, 'picked an mp4 without naming H.264: ' + chosen.type);
});

test('every candidate names its codecs', () => {
  PlayerLib.MP4_CANDIDATES.concat(PlayerLib.WEBM_CANDIDATES).forEach((type) => {
    if (type === 'video/webm') return; // the last-resort fallback, and honest about it
    assert(type.indexOf('codecs=') !== -1, 'candidate without codecs: ' + type);
  });
  PlayerLib.MP4_CANDIDATES.forEach((type) => {
    assert(/avc1|h264/.test(type), 'an mp4 candidate that is not H.264: ' + type);
    // Naming only the video codec leaves the audio to the browser, and Chrome
    // will put Opus in an MP4 — H.264 an iPhone plays, with sound it does not.
    assert(/mp4a|aac/.test(type), 'an mp4 candidate that does not name its audio codec: ' + type);
  });
  assert(PlayerLib.MP4_CANDIDATES.indexOf('video/mp4') === -1, 'the bare type is a candidate again');
});

test('webm falls back in order, and nothing at all is handled', () => {
  eq(PlayerLib.pickMimeType((t) => t === 'video/webm;codecs=vp8,opus').type, 'video/webm;codecs=vp8,opus');
  eq(PlayerLib.pickMimeType((t) => t === 'video/webm').container, 'webm');
  eq(PlayerLib.pickMimeType(() => false), null, 'a browser with no format at all');
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

/* ==================================================================== score
 * The conductor is pure data in, pure data out, so the entire musical shape of
 * a film is checkable here. SONG FORGE's own modules are browser files, so they
 * load the way SONG FORGE's tests load them: in a vm sandbox with a fake window.
 */
const vm = require('vm');
const fs = require('fs');
const Conductor = require(path.join(__dirname, '..', 'js', 'film-score.js'));

function loadSongForge() {
  const sandbox = { console: console };
  sandbox.window = sandbox;
  vm.createContext(sandbox);
  ['theory.js', 'genres.js', 'composer.js'].forEach((f) => {
    vm.runInContext(
      fs.readFileSync(path.join(__dirname, '..', '..', 'music', 'js', f), 'utf8'),
      sandbox, { filename: f });
  });
  return sandbox;
}

console.log('\nSCORING THE FILM');

test('every film genre maps to music SONG FORGE actually has', () => {
  const forge = loadSongForge();
  const genres = forge.Genres.GENRES;
  const moods = forge.Genres.MOODS;

  Object.keys(LEX.GENRES).forEach((filmGenre) => {
    const pick = Conductor.MUSIC_FOR[filmGenre];
    assert(pick, 'no music for film genre ' + filmGenre);
    assert(genres[pick.genre], filmGenre + ' asks for genre "' + pick.genre + '", which SONG FORGE does not have');
    assert(moods[pick.mood], filmGenre + ' asks for mood "' + pick.mood + '", which SONG FORGE does not have');
  });
});

test('the tempo is chosen to land bars near the cuts', () => {
  const reel = Reel.build(sample);
  const range = [70, 110];
  const bpm = Conductor.chooseBpm(reel, range);
  assert(bpm >= range[0] && bpm <= range[1], 'bpm outside the genre range: ' + bpm);
  assert(bpm === Math.round(bpm), 'bpm is not a whole number: ' + bpm);

  // It must be no worse than the middle of the range, or choosing is pointless.
  const error = (candidate) => Conductor.cutTimes(reel).reduce((total, cut) => {
    const block = Conductor.blockSeconds(candidate);
    return total + Math.abs(cut - Math.round(cut / block) * block);
  }, 0);
  const middle = Math.round((range[0] + range[1]) / 2);
  assert(error(bpm) <= error(middle) + 1e-9,
    `chosen ${bpm} (error ${error(bpm).toFixed(2)}s) is worse than ${middle} (${error(middle).toFixed(2)}s)`);

  eq(Conductor.chooseBpm(reel, range), bpm, 'the same reel must choose the same tempo');
  eq(Conductor.chooseBpm(reel, [96, 96]), 96, 'a single-value range must be honoured');
});

test('cut times are the scene starts after the first', () => {
  const reel = Reel.build(sample);
  const cuts = Conductor.cutTimes(reel);
  eq(cuts.length, sample.scenes.length - 1, 'one cut between each pair of scenes');
  cuts.forEach((t, i) => {
    assert(t > 0 && t < reel.duration, 'cut outside the film: ' + t);
    if (i > 0) assert(t > cuts[i - 1], 'cuts are not in order');
  });
});

test('the section plan covers the whole film', () => {
  ['micro', 'short', 'festival'].forEach((length) => {
    const reel = Reel.build(Writer.write(Parse.parse('a ghost in the attic'), { length }));
    const bpm = Conductor.chooseBpm(reel, [70, 110]);
    const plan = Conductor.sectionPlan(reel, bpm);

    assert(plan.length >= 1, 'no sections for a ' + length + ' film');
    plan.forEach((section) => {
      assert(section.bars >= 4, 'section shorter than four bars: ' + section.bars);
      eq(section.bars % 4, 0, 'section is not a whole number of four-bar blocks');
      assert(section.energy >= 0 && section.energy <= 1, 'energy out of range: ' + section.energy);
      assert(typeof section.type === 'string' && section.type.length > 0, 'section has no type');
    });

    const bars = plan.reduce((total, s) => total + s.bars, 0);
    const seconds = (bars * Conductor.BEATS_PER_BAR * 60) / bpm;
    assert(seconds >= reel.duration,
      `${length}: the music runs ${seconds.toFixed(1)}s but the film runs ${reel.duration.toFixed(1)}s`);
  });
});

test('sections take their type and energy from the beat they cover', () => {
  const reel = Reel.build(sample);
  const plan = Conductor.sectionPlan(reel, Conductor.chooseBpm(reel, [70, 110]));
  eq(plan[0].type, 'intro', 'a film opens on an intro');

  const crisisOrClimax = plan.reduce((best, s) => (s.energy > best.energy ? s : best), plan[0]);
  eq(plan.indexOf(crisisOrClimax) < plan.length - 1, true, 'the highest-energy section is not the last one');
  assert(plan[plan.length - 1].energy <= crisisOrClimax.energy,
    'the film ends on more energy than its peak');
});

test('the band grows with the tension and stands down at the end', () => {
  const quiet = Conductor.partsFor(0.15, false);
  eq(quiet.drums, false, 'drums under the opening');
  eq(quiet.bass, false, 'bass under the opening');
  eq(quiet.pad && quiet.chords, true, 'the opening still needs pad and chords');

  eq(Conductor.partsFor(0.4, false).bass, true, 'bass joins in the middle band');
  eq(Conductor.partsFor(0.4, false).drums, false, 'drums are too early at 0.4');
  eq(Conductor.partsFor(0.6, false).drums, true, 'drums join by 0.6');
  eq(Conductor.partsFor(0.6, false).lead, false, 'the lead is not out yet at 0.6');

  const crisis = Conductor.partsFor(0.88, false);
  eq(crisis.drums && crisis.bass && crisis.lead && crisis.arp, true, 'the crisis gets the full band');

  // Boundary assertions for bass at >= 0.3
  eq(Conductor.partsFor(0.29999, false).bass, false, 'bass must not engage below 0.3');
  eq(Conductor.partsFor(0.3, false).bass, true, 'bass must engage at exactly 0.3');

  // Boundary assertions for drums at >= 0.5
  eq(Conductor.partsFor(0.49999, false).drums, false, 'drums must not engage below 0.5');
  eq(Conductor.partsFor(0.5, false).drums, true, 'drums must engage at exactly 0.5');

  // Boundary assertions for arp and lead at > 0.7
  eq(Conductor.partsFor(0.7, false).arp, false, 'arp must not engage at 0.7');
  eq(Conductor.partsFor(0.7, false).lead, false, 'lead must not engage at 0.7');
  eq(Conductor.partsFor(0.70001, false).arp, true, 'arp must engage above 0.7');
  eq(Conductor.partsFor(0.70001, false).lead, true, 'lead must engage above 0.7');

  // The last scene resolves regardless of its own tension — a micro film ends
  // on the choice at 0.5 and must still land rather than stop.
  const ending = Conductor.partsFor(0.5, true);
  eq(ending.drums, false, 'the closing scene still had drums');
  eq(ending.pad && ending.chords, true, 'the closing scene needs pad and chords');
});

test('every section carries its instruments', () => {
  const reel = Reel.build(sample);
  const plan = Conductor.sectionPlan(reel, Conductor.chooseBpm(reel, [70, 110]));
  plan.forEach((section) => {
    ['drums', 'bass', 'chords', 'arp', 'lead', 'pad'].forEach((part) => {
      eq(typeof section.parts[part], 'boolean', 'section is missing ' + part);
    });
  });
  eq(plan[plan.length - 1].parts.drums, false, 'the film ends on drums');
});

test('a reel becomes a complete score request', () => {
  const reel = Reel.build(sample);
  const forge = loadSongForge();
  const music = Conductor.MUSIC_FOR[reel.genre];
  const req = Conductor.request(reel, { bpmRange: forge.Genres.GENRES[music.genre].bpm });

  eq(req.genre, music.genre);
  eq(req.mood, music.mood);
  eq(req.seconds, reel.duration);
  assert(req.sections.length >= 1, 'a request with no sections');
  assert(req.seed !== reel.seed, 'the score seed must not be the film seed itself');
  assert(typeof req.seed === 'number' && isFinite(req.seed), 'bad seed: ' + req.seed);

  const again = Conductor.request(reel, { bpmRange: forge.Genres.GENRES[music.genre].bpm });
  eq(JSON.stringify(again), JSON.stringify(req), 'the same film must ask for the same score');
});

test('any film, any length, produces a usable request', () => {
  ['', 'robot', 'two sisters rob a bank at midnight', 'a ghost in the attic'].forEach((idea) => {
    ['micro', 'short', 'festival'].forEach((length) => {
      const reel = Reel.build(Writer.write(Parse.parse(idea, { seed: 3 }), { length, seed: 3 }));
      const req = Conductor.request(reel);
      assert(req.bpm > 0 && req.sections.length > 0, 'unusable request for "' + idea.slice(0, 20) + '"');
      const seconds = (req.sections.reduce((b, s) => b + s.bars, 0) * Conductor.BEATS_PER_BAR * 60) / req.bpm;
      assert(seconds >= reel.duration - 1e-6, 'the score is shorter than the film');
    });
  });
});

test('a reel with an unknown genre falls back to drama', () => {
  // A hand-built reel may have a genre the map has never seen (e.g., 'documentary').
  // It should still yield a usable request, scored as drama.
  const unknownReel = {
    genre: 'documentary',
    seed: 0x12345678,
    duration: 32.5,
    shots: [
      { start: 0, duration: 10, scene: 1, beat: 'open', mood: 0.15, kind: 'action', set: 'room', time: 'DAY', framing: 'wide', camera: 'push', caption: 'First scene', speaker: null, characters: [] },
      { start: 10, duration: 12.5, scene: 1, beat: 'spark', mood: 0.38, kind: 'action', set: 'room', time: 'DAY', framing: 'mid', camera: 'push', caption: 'Still scene one', speaker: null, characters: [] },
      { start: 22.5, duration: 10, scene: 2, beat: 'after', mood: 0.18, kind: 'action', set: 'room', time: 'NIGHT', framing: 'close', camera: 'static', caption: 'Final scene', speaker: null, characters: [] }
    ]
  };

  const req = Conductor.request(unknownReel);

  // Should match drama's genre and mood
  const drama = Conductor.MUSIC_FOR.drama;
  eq(req.genre, drama.genre, 'fallback request should have drama genre');
  eq(req.mood, drama.mood, 'fallback request should have drama mood');

  // Should still be usable: has tempo and sections
  assert(req.bpm > 0 && isFinite(req.bpm), 'request has no valid bpm');
  assert(req.sections.length >= 1, 'request has no sections');
  assert(req.sections.every((s) => s.bars >= 4), 'all sections must be at least 4 bars');

  // Should cover the film
  const musicSeconds = (req.sections.reduce((b, s) => b + s.bars, 0) * Conductor.BEATS_PER_BAR * 60) / req.bpm;
  assert(musicSeconds >= unknownReel.duration - 1e-6, 'the score is shorter than the film');
});

test('the music ducks for every line and comes back up', () => {
  const reel = Reel.build(sample);
  const env = Conductor.duckEnvelope(reel);
  const lines = reel.shots.filter((s) => s.kind === 'line');

  eq(env[0].t, 0, 'the envelope must start at the top of the film');
  eq(env[0].gain, 1, 'the film must start at full music');

  for (let i = 1; i < env.length; i++) {
    assert(env[i].t >= env[i - 1].t, 'envelope points are out of order');
    assert(env[i].gain === 1 || env[i].gain === Conductor.DUCK_GAIN,
      'unexpected gain ' + env[i].gain);
    assert(env[i].t >= 0 && env[i].t <= reel.duration + 1, 'envelope point outside the film');
  }

  const ducks = env.filter((p) => p.gain === Conductor.DUCK_GAIN).length;
  assert(ducks >= 1 && ducks <= lines.length,
    `${ducks} ducks for ${lines.length} lines — expected at most one per line`);
  eq(env[env.length - 1].gain, 1, 'the music must come back up before the end');
});

test('lines close together stay ducked rather than pumping', () => {
  const reel = {
    duration: 20, genre: 'drama', seed: 1,
    shots: [
      { kind: 'line', start: 5, duration: 2, scene: 1 },
      { kind: 'line', start: 7.1, duration: 2, scene: 1 }
    ]
  };
  const env = Conductor.duckEnvelope(reel);
  const ducks = env.filter((p) => p.gain === Conductor.DUCK_GAIN).length;
  eq(ducks, 1, 'two lines a fifth of a second apart should be one duck, not two');
});

/* The duck envelope is arithmetic, but *scheduling* it is where it goes wrong:
 * Web Audio ramps from the previous automation event, so a bare list of ramps
 * glides the level continuously instead of holding it. These drive the real
 * `applyDuck` through a fake AudioParam that records every call, then replay
 * the recording to ask what the level actually is at a given moment. */

/* A fake Score: applyDuck only touches these four things. */
function fakeScore(reel, now) {
  const calls = [];
  const gain = {
    calls: calls,
    cancelScheduledValues(t) { calls.push({ op: 'cancel', value: null, at: t }); },
    setValueAtTime(v, t) { calls.push({ op: 'set', value: v, at: t }); },
    linearRampToValueAtTime(v, t) { calls.push({ op: 'ramp', value: v, at: t }); }
  };
  return {
    calls: calls,
    duckPoints: Conductor.duckEnvelope(reel),
    musicBus: { gain: gain },
    ctx: { currentTime: now }
  };
}

/* Replay a recorded automation the way Web Audio would, and report the level
 * at one moment: a `set` pins a value, a `ramp` runs linearly to its value
 * from whatever event came before it. */
function levelAt(calls, t) {
  let prevAt = null, prevValue = null, value = 0;
  for (const call of calls) {
    if (call.op === 'cancel') continue;
    if (call.at <= t) {
      value = call.value;
      prevAt = call.at;
      prevValue = call.value;
      continue;
    }
    if (call.op === 'ramp' && prevValue !== null) {
      const span = call.at - prevAt;
      value = span <= 0 ? call.value
        : prevValue + (call.value - prevValue) * ((t - prevAt) / span);
    }
    break;
  }
  return value;
}

const duckReel = {
  duration: 40, genre: 'drama', seed: 1,
  shots: [
    { kind: 'line', start: 10, duration: 3, scene: 1 },
    { kind: 'line', start: 25, duration: 2, scene: 2 }
  ]
};

test('the duck is scheduled flat, not as one long glide', () => {
  const NOW = 1000;          // a context that has been running a while
  const score = fakeScore(duckReel, NOW);
  Score.Score.prototype.applyDuck.call(score, 0);

  eq(score.calls[0].op, 'cancel', 'the old envelope must be cancelled first');
  const full = score.calls[1].value;
  assert(score.calls[1].op === 'set' && full > 0, 'the envelope must open on a held value');
  const ducked = full * Conductor.DUCK_GAIN;

  const at = (filmSeconds) => levelAt(score.calls, NOW + filmSeconds);
  const near = (actual, expected, where) => assert(Math.abs(actual - expected) < 1e-6,
    `at ${where} the music is at ${actual.toFixed(4)}, expected ${expected.toFixed(4)}`);

  // Flat at full right across the gap before the first line — this is the one
  // that fails if the setValueAtTime anchors go: without them the level is
  // already halfway down by here, sliding since the film began.
  near(at(0), full, '0s, the top of the film');
  near(at(5), full, '5s, the middle of the gap');
  near(at(9.7), full, '9.7s, a breath before the dip starts');

  // A quarter-second dip that lands exactly as the line starts.
  near(at(10 - Conductor.DUCK_LEAD), full, 'the instant the dip begins');
  assert(at(9.9) < full && at(9.9) > ducked, 'the dip is not moving mid-ramp');
  near(at(10), ducked, '10s, the first word');

  // Flat and low through the line, not climbing back while it is spoken.
  near(at(11.5), ducked, '11.5s, mid-line');
  near(at(13), ducked, '13s, the last word');
  near(at(13 + Conductor.DUCK_TAIL), ducked, 'the instant the rise begins');

  // Up again after it, and flat until the next line.
  near(at(13.2 + Conductor.DUCK_TAIL), full, 'the top of the rise');
  near(at(20), full, '20s, between the two lines');

  // And the same shape again for the second line.
  near(at(24.7), full, '24.7s, before the second line');
  near(at(25), ducked, '25s, the second line');
  near(at(26.5), ducked, '26.5s, mid second line');
  near(at(27.4), full, '27.4s, back up after the second line');
  near(at(39), full, '39s, the end of the film');
});

test('every duck transition is anchored before it ramps', () => {
  const NOW = 4;
  const score = fakeScore(duckReel, NOW);
  Score.Score.prototype.applyDuck.call(score, 0);

  const ramps = score.calls.filter((c) => c.op === 'ramp');
  eq(ramps.length, 4, 'two lines make four transitions: down, up, down, up');

  ramps.forEach((ramp) => {
    const anchor = score.calls[score.calls.indexOf(ramp) - 1];
    eq(anchor.op, 'set', 'a ramp with no setValueAtTime before it glides from the last event');
    assert(anchor.value !== ramp.value, 'the anchor holds the old level, not the new one');
    const seconds = ramp.at - anchor.at;
    const expected = ramp.value < anchor.value ? Conductor.DUCK_LEAD : Conductor.DUCK_TAIL;
    assert(Math.abs(seconds - expected) < 1e-6,
      `a transition took ${seconds.toFixed(3)}s, expected ${expected}s`);
  });
});

test('playing from the middle of a line starts already ducked', () => {
  const NOW = 7;
  const score = fakeScore(duckReel, NOW);
  Score.Score.prototype.applyDuck.call(score, 11);   // eleven seconds in, mid-line

  const opening = score.calls[1];
  eq(opening.op, 'set', 'the envelope opens on a held value');
  eq(opening.at, NOW, 'and it is held from this instant, not from film zero');
  assert(Math.abs(opening.value - 0.55 * Conductor.DUCK_GAIN) < 1e-6,
    'starting mid-line must start under the dialogue, at ' + opening.value);

  // Everything still to come is laid relative to where playback starts.
  const first = score.calls.filter((c) => c.op === 'ramp')[0];
  assert(Math.abs(first.at - (NOW + (13.2 - 11) + Conductor.DUCK_TAIL)) < 1e-6,
    'the rise after the current line is at the wrong moment: ' + first.at);
  score.calls.forEach((c) => assert(c.at >= NOW, 'an event was scheduled in the past: ' + c.at));
});

/* ================================================================== figures */
const Figures = require(path.join(__dirname, '..', 'js', 'film-figures.js'));

console.log('\nHOW A CHARACTER STANDS');

test('every pose defines every joint, inside a human range', () => {
  const joints = Figures.JOINTS;
  assert(joints.length >= 10, 'not enough joints to make a body');

  Object.keys(Figures.POSES).forEach((name) => {
    const pose = Figures.POSES[name];
    joints.forEach((joint) => {
      eq(typeof pose[joint], 'number', name + ' does not say what ' + joint + ' does');
      const limit = Figures.POSE_LIMITS[joint];
      assert(limit, 'no limit declared for ' + joint);
      assert(pose[joint] >= limit[0] && pose[joint] <= limit[1],
        name + '.' + joint + ' is ' + pose[joint].toFixed(2) + ', outside ' + JSON.stringify(limit));
    });
  });
});

test('the library covers the bearings the story needs', () => {
  ['stand', 'turn-away', 'reach', 'recoil', 'sit', 'slump',
   'hands-in-pockets', 'point', 'head-in-hands', 'walk'].forEach((name) => {
    assert(Figures.POSES[name], 'no pose called ' + name);
  });
});

test('the beat decides how a character carries themselves', () => {
  const allowed = Figures.POSES_BY_BEAT;
  ['open', 'spark', 'push', 'turn', 'crisis', 'choice', 'after'].forEach((beat) => {
    assert(allowed[beat] && allowed[beat].length, 'no poses for the ' + beat + ' beat');
    allowed[beat].forEach((name) => assert(Figures.POSES[name], beat + ' asks for a pose that does not exist: ' + name));

    for (let seed = 0; seed < 20; seed++) {
      const chosen = Figures.poseFor(beat, 0.5, false, seed);
      assert(allowed[beat].indexOf(chosen) !== -1,
        beat + ' chose ' + chosen + ', which is not one of its poses');
    }
  });
});

test('the crisis breaks a character and the choice straightens them up', () => {
  const crisis = [];
  const choice = [];
  for (let seed = 0; seed < 30; seed++) {
    crisis.push(Figures.poseFor('crisis', 0.88, false, seed));
    choice.push(Figures.poseFor('choice', 0.5, false, seed));
  }
  crisis.forEach((p) => assert(['recoil', 'slump', 'head-in-hands'].indexOf(p) !== -1,
    'the crisis produced ' + p));
  choice.forEach((p) => assert(['stand', 'point', 'reach'].indexOf(p) !== -1,
    'the choice produced ' + p));
});

test('the same film poses the same way twice', () => {
  for (let seed = 0; seed < 10; seed++) {
    eq(Figures.poseFor('push', 0.52, true, seed), Figures.poseFor('push', 0.52, true, seed));
  }
});

test('a speaker is never turned away from the room', () => {
  ['open', 'spark', 'push', 'turn', 'crisis', 'choice', 'after'].forEach((beat) => {
    for (let seed = 0; seed < 20; seed++) {
      assert(Figures.poseFor(beat, 0.5, true, seed) !== 'turn-away',
        'a speaking character turned away during ' + beat);
    }
  });
});

test('high tension biases the pose toward more extreme expressions', () => {
  // The poseFor function uses tension to bias selection toward later (more extreme)
  // entries in a beat's pool. For the crisis beat, 'recoil' is the first/least extreme
  // entry. At low tension it should be chosen more often than at high tension.
  const pool = Figures.POSES_BY_BEAT.crisis;
  const leastExtreme = pool[0];

  let countLow = 0;
  let countHigh = 0;
  const samples = 300;

  for (let seed = 0; seed < samples; seed++) {
    const poseAtLowTension = Figures.poseFor('crisis', 0.1, false, seed);
    const poseAtHighTension = Figures.poseFor('crisis', 0.88, false, seed);

    if (poseAtLowTension === leastExtreme) countLow++;
    if (poseAtHighTension === leastExtreme) countHigh++;
  }

  const shareLow = countLow / samples;
  const shareHigh = countHigh / samples;

  assert(shareLow > 0, 'no samples of least extreme pose at low tension');
  assert(shareHigh > 0, 'no samples of least extreme pose at high tension');
  assert(shareLow >= shareHigh * 2,
    'least extreme pose (' + leastExtreme + ') should appear at least twice as often at low tension: ' +
    'low=' + (shareLow * 100).toFixed(1) + '%, high=' + (shareHigh * 100).toFixed(1) + '%');
});

test('a gesture moves the head and hand and nothing else', () => {
  const rest = Figures.POSES.stand;
  const mid = Figures.gestureAt(rest, 0.5);

  assert(Math.abs(mid.head - rest.head) > 0.001, 'the head did not move on a syllable');
  assert(Math.abs(mid.armL - rest.armL) > 0.001 || Math.abs(mid.armR - rest.armR) > 0.001,
    'neither hand moved on a syllable');
  ['legL', 'legR', 'shinL', 'shinR'].forEach((joint) => {
    eq(mid[joint], rest[joint], 'a syllable moved the ' + joint);
  });

  Figures.JOINTS.forEach((joint) => {
    const limit = Figures.POSE_LIMITS[joint];
    [0, 0.25, 0.5, 0.75, 1].forEach((phase) => {
      const g = Figures.gestureAt(rest, phase);
      assert(g[joint] >= limit[0] && g[joint] <= limit[1],
        'a gesture at phase ' + phase + ' put ' + joint + ' outside its range');
    });
  });
});

test('a gesture starts and finishes at rest', () => {
  const rest = Figures.POSES.stand;
  Figures.JOINTS.forEach((joint) => {
    eq(Figures.gestureAt(rest, 0)[joint].toFixed(4), rest[joint].toFixed(4), joint + ' at phase 0');
    eq(Figures.gestureAt(rest, 1)[joint].toFixed(4), rest[joint].toFixed(4), joint + ' at phase 1');
  });
});

test('the voice and the gesture agree on syllable count for a line with a standalone dash', () => {
  // "Wait — what?" has a standalone em-dash token: raw-word counting sees it
  // as a word, punctuation-stripped counting does not. The two clocks must
  // still land on the same number, because Parse.syllablesFor is the only
  // place either of them is allowed to compute it.
  const caption = 'Wait — what? I said no...';
  const reference = Parse.syllablesFor(caption);

  // The picture: film-player.js's gesture wiring calls Parse.syllablesFor(shot.caption).
  const pictureSyllables = Parse.syllablesFor(caption);

  // The audio: invoke the real Score.prototype.speak (film-audio.js) and count
  // the blips it actually schedules — one per syllable — with a stub `this`
  // so no AudioContext is needed.
  const blips = [];
  Score.Score.prototype.speak.call(
    { ctx: { currentTime: 0 }, blip: function (when, voice, through, charCode) { blips.push(charCode); } },
    caption, { pitch: 220 }, 2
  );
  const audioSyllables = blips.length;

  eq(audioSyllables, reference, 'the audio scheduled ' + audioSyllables +
    ' blips but Parse.syllablesFor(caption) says ' + reference);
  eq(pictureSyllables, reference, 'the gesture clock used ' + pictureSyllables +
    ' syllables but Parse.syllablesFor(caption) says ' + reference);
  assert(audioSyllables === pictureSyllables,
    'the voice and the gesture disagree on syllable count: audio=' + audioSyllables +
    ' picture=' + pictureSyllables);
});

console.log('\nSETS WITH DEPTH');

test('every set is built in three layers', () => {
  const names = Object.keys(Sets.SETS);
  assert(names.length >= 15, 'expected fifteen sets, found ' + names.length);
  names.forEach((name) => {
    ['back', 'mid', 'fore'].forEach((layer) => {
      eq(typeof Sets.SETS[name][layer], 'function', name + ' has no ' + layer + ' layer');
    });
  });
});

test('the layers move at different speeds, in the right order', () => {
  const p = Sets.PARALLAX;
  assert(p.back < p.mid && p.mid < p.fore,
    'parallax is not ordered back < mid < fore: ' + JSON.stringify(p));
  assert(p.back > 0 && p.fore < 4, 'parallax rates are out of a sane range');
});

test('every set a scene can ask for still exists', () => {
  Object.keys(Reel.SET_BY_PLACE).forEach((place) => {
    const set = Reel.SET_BY_PLACE[place];
    assert(Sets.SETS[set], place + ' maps to "' + set + '", which no longer exists');
  });
});

test('light moves within a scene, and stays in a sane range', () => {
  Object.keys(Sets.SETS).forEach((set) => {
    const kind = Sets.LIGHT[set];
    assert(['sweep', 'passing', 'flicker', 'cloud', 'none'].indexOf(kind) !== -1,
      set + ' declares light "' + kind + '", which nothing draws');
  });

  ['sweep', 'passing', 'flicker', 'cloud', 'none'].forEach((kind) => {
    for (let t = 0; t < 40; t++) {
      [0.15, 0.5, 0.88].forEach((tension) => {
        const light = Sets.lightAt(kind, t * 0.37, tension);
        assert(light.brightness > 0.4 && light.brightness < 2.2,
          kind + ' went to brightness ' + light.brightness.toFixed(2));
        assert(light.offset >= -1 && light.offset <= 1,
          kind + ' put its light at ' + light.offset.toFixed(2));
      });
    }
  });
});

test('a bulb flickers harder when the story is tense', () => {
  const spread = (tension) => {
    let lo = 2, hi = 0;
    for (let t = 0; t < 200; t++) {
      const b = Sets.lightAt('flicker', t * 0.11, tension).brightness;
      if (b < lo) lo = b;
      if (b > hi) hi = b;
    }
    return hi - lo;
  };
  assert(spread(0.88) > spread(0.15) * 1.5,
    'the crisis flickers no harder than the opening');
});

const Weather = require(path.join(__dirname, '..', 'js', 'film-weather.js'));

console.log('\nWEATHER');

test('every genre and hour gets air the artist can draw', () => {
  const drawable = ['rain', 'dust', 'fog', 'shimmer', 'embers', 'haze', 'none'];
  Object.keys(LEX.GENRES).forEach((genre) => {
    ['NIGHT', 'DAY', 'DUSK', 'DAWN'].forEach((time) => {
      Object.keys(Sets.SETS).forEach((set) => {
        const kind = Weather.forShot(genre, time, set);
        assert(drawable.indexOf(kind) !== -1,
          genre + '/' + time + '/' + set + ' asked for "' + kind + '", which nothing draws');
      });
    });
  });
});

test('the obvious cases land where they should', () => {
  eq(Weather.forShot('thriller', 'NIGHT', 'street'), 'rain');
  eq(Weather.forShot('western', 'DAY', 'field'), 'shimmer');
  eq(Weather.forShot('fantasy', 'NIGHT', 'woods'), 'embers');
  eq(Weather.forShot('drama', 'DAY', 'kitchen'), 'dust');
  eq(Weather.forShot('horror', 'NIGHT', 'woods'), 'fog');
});

test('the same shot always has the same weather', () => {
  for (let i = 0; i < 5; i++) {
    eq(Weather.forShot('mystery', 'DUSK', 'office'), Weather.forShot('mystery', 'DUSK', 'office'));
  }
});

console.log('\nTHE CAMERA');

test('every camera the reel can ask for is one the player knows', () => {
  const known = ['push', 'push-slow', 'pull', 'pan-l', 'pan-r', 'static',
                 'handheld', 'track-l', 'track-r', 'whip'];
  ['micro', 'short', 'festival'].forEach((length) => {
    for (let seed = 0; seed < 8; seed++) {
      const reel = Reel.build(Writer.write(Parse.parse('a ghost in the attic', { seed }), { length, seed }));
      reel.shots.forEach((shot) => {
        assert(known.indexOf(shot.camera) !== -1, 'unknown camera: ' + shot.camera);
      });
    }
  });
});

test('the camera stays pointed at the set, whatever the move', () => {
  const known = ['push', 'push-slow', 'pull', 'pan-l', 'pan-r', 'static',
                 'handheld', 'track-l', 'track-r', 'whip'];
  known.forEach((camera) => {
    [0, 0.25, 0.5, 0.75, 1].forEach((progress) => {
      const f = PlayerLib.framingFor({ camera, framing: 'mid', mood: 0.88, kind: 'action' }, progress, progress * 3);
      assert(f.zoom > 0.8 && f.zoom < 3.2, camera + ' zoomed to ' + f.zoom.toFixed(2));
      assert(Math.abs(f.panX) < 0.5 && Math.abs(f.panY) < 0.5, camera + ' panned off the set');
      assert(Math.abs(f.roll || 0) < 0.09, camera + ' rolled ' + (f.roll || 0).toFixed(3) + ' radians');
    });
  });
});

test('handheld is steady when the story is calm and unsteady when it is not', () => {
  const wobble = (mood) => {
    let lo = 9, hi = -9;
    for (let i = 0; i < 200; i++) {
      const f = PlayerLib.framingFor({ camera: 'handheld', framing: 'mid', mood, kind: 'action' }, 0.5, i * 0.05);
      if (f.panX < lo) lo = f.panX;
      if (f.panX > hi) hi = f.panX;
    }
    return hi - lo;
  };
  assert(wobble(0.88) > wobble(0.15) * 1.8, 'the crisis is no shakier than the opening');
});

test('only the crisis is allowed to tilt', () => {
  const calm = PlayerLib.framingFor({ camera: 'handheld', framing: 'mid', mood: 0.2, kind: 'action' }, 0.5, 1);
  const crisis = PlayerLib.framingFor({ camera: 'handheld', framing: 'mid', mood: 0.88, kind: 'action' }, 0.5, 1);
  assert(Math.abs(calm.roll || 0) < 0.005, 'a calm shot was tilted');
  assert(Math.abs(crisis.roll || 0) > 0.01, 'the crisis was not tilted');
});

test('the whip pan is actually used somewhere', () => {
  let seen = false;
  for (let seed = 0; seed < 40 && !seen; seed++) {
    const reel = Reel.build(Writer.write(Parse.parse('two thieves argue in a warehouse', { seed }), { length: 'festival', seed }));
    seen = reel.shots.some((s) => s.camera === 'whip');
  }
  assert(seen, 'no film in forty used a whip pan');
});

test('a conversation alternates rather than repeating one framing', () => {
  ['short', 'festival'].forEach((length) => {
    for (let seed = 0; seed < 6; seed++) {
      const reel = Reel.build(Writer.write(Parse.parse('two sisters argue in a kitchen', { seed }), { length, seed }));
      const lines = reel.shots.filter((s) => s.kind === 'line');
      for (let i = 2; i < lines.length; i++) {
        assert(!(lines[i].framing === lines[i - 1].framing && lines[i].framing === lines[i - 2].framing),
          'three spoken shots in a row used ' + lines[i].framing);
      }
    }
  });
});

test('the choice is held longer than the push', () => {
  const reel = Reel.build(Writer.write(Parse.parse('a lighthouse keeper finds a radio'), { length: 'festival' }));
  // Compare seconds per word, not raw duration: a wordier push line would
  // otherwise run longer than a held choice and the test would prove nothing.
  const pace = (beat) => {
    const shots = reel.shots.filter((s) => s.beat === beat && s.kind === 'action');
    assert(shots.length, 'no action shots on the ' + beat + ' beat to measure');
    const secs = shots.reduce((a, s) => a + s.duration, 0);
    const words = shots.reduce((a, s) => a + String(s.caption).trim().split(/\s+/).length, 0);
    return secs / words;
  };
  assert(pace('choice') > pace('push') * 1.2,
    'the choice is cut at the same pace as the push');
});

test('the new framings are understood by the camera', () => {
  ['ots', 'low'].forEach((framing) => {
    const f = PlayerLib.framingFor({ camera: 'static', framing, mood: 0.4, kind: 'line', speaker: 'A' }, 0.5, 1);
    assert(f.zoom > 0.8 && f.zoom < 3.2, framing + ' zoomed to ' + f.zoom.toFixed(2));
    const layout = PlayerLib.figureLayout({ framing, characters: ['A', 'B'], speaker: 'A', kind: 'line' });
    assert(layout.length >= 1, framing + ' put nobody in frame');
  });
});

console.log('\nWHERE A FILM HAPPENS');

test('a premise offers three to five places', () => {
  for (let seed = 0; seed < 60; seed++) {
    const p = Parse.parse('a courier takes a job in a city at night', { seed });
    assert(p.places.length >= 3 && p.places.length <= 5,
      'seed ' + seed + ' offered ' + p.places.length + ' places');
  }
});

test('the places are distinct', () => {
  for (let seed = 0; seed < 60; seed++) {
    const p = Parse.parse('a lighthouse keeper finds a radio', { seed });
    const keys = p.places.map((x) => x.key);
    eq(new Set(keys).size, keys.length, 'seed ' + seed + ' repeated a place: ' + keys.join(','));
  }
});

test('a place named in the idea is still used, and comes first', () => {
  const p = Parse.parse('two sisters argue in a kitchen');
  eq(p.places[0].key, 'kitchen', 'the typed place did not lead');
});

test('the same idea and seed give the same places', () => {
  for (let seed = 0; seed < 20; seed++) {
    const a = Parse.parse('a thief in a warehouse', { seed }).places.map((x) => x.key).join(',');
    const b = Parse.parse('a thief in a warehouse', { seed }).places.map((x) => x.key).join(',');
    eq(a, b, 'seed ' + seed + ' was not deterministic');
  }
});

test('a festival film uses at least three distinct places', () => {
  for (let seed = 0; seed < 40; seed++) {
    const script = Writer.write(Parse.parse('a courier takes a job', { seed }), { length: 'festival', seed });
    const used = new Set(script.scenes.map((s) => s.heading.place.key));
    assert(used.size >= 3, 'seed ' + seed + ' used only ' + used.size + ' places');
  }
});

test('a film ends where it began', () => {
  // By *position*, not by beat id: festival's third shape ends
  // ['... crisis, after, choice'] — 'after' is second-to-last there, not
  // last, so an id-based 'after === open' check is only true by accident of
  // the other two shapes. The real property, true of every shape, is that
  // the first scene and the last scene share a place.
  for (let seed = 0; seed < 40; seed++) {
    const script = Writer.write(Parse.parse('a lighthouse keeper finds a radio', { seed }), { length: 'festival', seed });
    const first = script.scenes[0].heading.place.key;
    const last = script.scenes[script.scenes.length - 1].heading.place.key;
    eq(last, first, 'seed ' + seed + ' did not return to the opening place');
  }
});

test('the crisis happens somewhere the film has not been', () => {
  // The old version of this test only checked byBeat.crisis !== byBeat.open —
  // but placeForBeat put the crisis at the far index (placeCount - 1) and
  // then spread spark/push/turn over [1, placeCount - 1], a range that
  // *includes* the crisis's own index, so a middle beat routinely got there
  // first while open (always index 0) never collided anyway. That made the
  // assertion true by construction: open and crisis literally could not
  // share an index, so it could never fail. The real property is that no
  // beat *before* the crisis in the spine used the crisis's place — checked
  // here directly against placesForSpine, for every shape this app ships, at
  // every place count a premise can actually offer, over many seeds.
  ['micro', 'short', 'festival'].forEach((len) => {
    LEX.STRUCTURES[len].spines.forEach((spine, i) => {
      const ci = spine.indexOf('crisis');
      if (ci === -1) return;
      for (let count = 3; count <= 5; count++) {
        for (let seed = 0; seed < 100; seed++) {
          const result = Writer.placesForSpine(spine, count, seed);
          const crisisPlace = result.places[ci];
          const usedBefore = new Set(result.places.slice(0, ci));
          assert(result.degraded === 'crisis-unused' || !usedBefore.has(crisisPlace),
            len + ' shape ' + i + ' (' + spine.join(' ') + ') at ' + count + ' places, seed ' + seed +
            ': the crisis reused an earlier beat\'s place ' + crisisPlace + ' (degraded=' + result.degraded + ')');
        }
      }
    });
  });

  // And the same property holds end to end, through the real writer, on
  // real generated premises (3-5 places, never fewer).
  let total = 0;
  ['short', 'festival'].forEach((len) => {
    for (let seed = 0; seed < 200; seed++) {
      const script = Writer.write(Parse.parse('a thief in a warehouse', { seed }), { length: len, seed });
      const crisisIndex = script.scenes.findIndex((s) => s.beat.id === 'crisis');
      if (crisisIndex === -1) continue;
      total++;
      const crisisPlace = script.scenes[crisisIndex].heading.place.key;
      const usedBefore = new Set(script.scenes.slice(0, crisisIndex).map((s) => s.heading.place.key));
      assert(!usedBefore.has(crisisPlace),
        len + ' seed ' + seed + ': the crisis landed back in ' + crisisPlace + ', already used');
    }
  });
  assert(total > 0, 'no film reached a crisis');
});

test('placeForBeat stays inside the places it is given', () => {
  ['open', 'spark', 'push', 'turn', 'crisis', 'choice', 'after'].forEach((beat) => {
    for (let count = 1; count <= 5; count++) {
      for (let seed = 0; seed < 20; seed++) {
        const i = Writer.placeForBeat(beat, count, seed);
        assert(Number.isInteger(i) && i >= 0 && i < count,
          beat + ' with ' + count + ' places returned ' + i);
      }
    }
  });
});

console.log('\nTHE SHAPE OF A STORY');

test('every length offers more than one shape', () => {
  Object.keys(LEX.STRUCTURES).forEach((len) => {
    const spines = LEX.STRUCTURES[len].spines;
    assert(Array.isArray(spines) && spines.length >= 2,
      len + ' offers ' + (spines ? spines.length : 0) + ' shapes');
  });
});

test('every shape is made of real beats and has a beginning', () => {
  const known = ['open', 'spark', 'push', 'turn', 'crisis', 'choice', 'after'];
  Object.keys(LEX.STRUCTURES).forEach((len) => {
    LEX.STRUCTURES[len].spines.forEach((spine, i) => {
      eq(spine[0], 'open', len + ' shape ' + i + ' does not open on the open beat');
      eq(new Set(spine).size, spine.length, len + ' shape ' + i + ' repeats a beat');
      spine.forEach((b) => assert(known.indexOf(b) !== -1, len + ' shape ' + i + ' has unknown beat ' + b));
    });
  });
});

test('a short film always reaches a crisis', () => {
  ['short', 'festival'].forEach((len) => {
    LEX.STRUCTURES[len].spines.forEach((spine, i) => {
      assert(spine.indexOf('crisis') !== -1,
        len + ' shape ' + i + ' has no crisis: ' + spine.join(' '));
    });
  });
  // and the shipped default really does produce one
  for (let seed = 0; seed < 30; seed++) {
    const script = Writer.write(Parse.parse('a stranger arrives', { seed }), { length: 'short', seed });
    assert(script.scenes.some((s) => s.beat.id === 'crisis'),
      'a default-length film at seed ' + seed + ' had no crisis');
  }
});

test('two films of the same length can be shaped differently', () => {
  const shapes = new Set();
  for (let seed = 0; seed < 40; seed++) shapes.add(Writer.spineFor('festival', seed).join(' '));
  assert(shapes.size >= 2, 'every festival film had the same shape');
});

test('scene times never go backwards within a film', () => {
  // headingFor used to advance the clock only for the beat id 'after' — a
  // rule written back when 'after' was always the spine's last beat. Once a
  // shape puts 'choice' after it (festival's third shape does:
  // '... crisis, after, choice'), the closing scene reverted to the
  // premise's original time, e.g. DUSK (after) followed by DAY (choice) on
  // the last two cards. It now advances for 'after' and everything at or
  // after it in the spine, so this checks the property directly: once a
  // scene shows the advanced time, nothing later in the same film shows the
  // original time again.
  const NEXT_TIME = { NIGHT: 'DAWN', DAWN: 'DAY', DAY: 'DUSK', DUSK: 'NIGHT' };
  ['micro', 'short', 'festival'].forEach((len) => {
    for (let seed = 0; seed < 150; seed++) {
      const premise = Parse.parse('a lighthouse keeper finds a radio', { seed });
      const script = Writer.write(premise, { length: len, seed });
      const base = premise.time;
      const advanced = NEXT_TIME[base] || base;
      let sawAdvanced = false;
      script.scenes.forEach((s) => {
        const t = s.heading.time;
        if (t === 'CONTINUOUS' || t === 'LATER') return; // reads as the same clock as the scene before it
        if (advanced !== base && t === advanced) sawAdvanced = true;
        else if (t === base) {
          assert(!sawAdvanced, len + ' seed ' + seed + ': the clock ran backwards, back to ' +
            base + ' after already showing ' + advanced + ' (' +
            script.scenes.map((x) => x.beat.id + '=' + x.heading.time).join(', ') + ')');
        }
      });
    }
  });
});

test('the same seed always gives the same shape', () => {
  for (let seed = 0; seed < 20; seed++) {
    eq(Writer.spineFor('short', seed).join(' '), Writer.spineFor('short', seed).join(' '));
  }
});

test('every shape, at every length, ends where it began', () => {
  // The other guard on this ('a film ends where it began') runs only at
  // festival length and compares beat ids rather than positions, so the micro
  // and short spines — two of which end on `choice` with no `after` beat at
  // all — have no cover from it.
  //
  // This must assert against placesForSpine, the mapping write() actually
  // calls. It used to call placeForBeat, which write() no longer uses, so it
  // was pinning dead code: mutating the live path for micro spines left every
  // test passing while micro films stopped ending where they began.
  Object.keys(LEX.STRUCTURES).forEach((len) => {
    LEX.STRUCTURES[len].spines.forEach((spine, i) => {
      for (let count = 3; count <= 5; count++) {
        for (let seed = 0; seed < 8; seed++) {
          const places = Writer.placesForSpine(spine, count, seed).places;
          eq(places[places.length - 1], places[0],
            len + ' shape ' + i + ' (' + spine.join(' ') + ') ends on ' +
            spine[spine.length - 1] + ' but opens on ' + spine[0] +
            ', at ' + count + ' places, seed ' + seed);
        }
      }
    });
  });
});

console.log('\nSTORIES FROM MADLIBS');
const Seed = require(path.join(__dirname, '..', 'js', 'story-seed.js'));
const MADLIBS = require(path.join(__dirname, '..', '..', 'madlibs', 'js', 'generator.js'));
const MAD_TEMPLATES = require(path.join(__dirname, '..', '..', 'madlibs', 'js', 'templates.js'));

test('every MADLIBS genre maps to a genre SCRIPT FORGE actually has', () => {
  const templates = MAD_TEMPLATES.templates || MAD_TEMPLATES;
  const genres = new Set(templates.map((t) => t.genre));
  assert(genres.size >= 5, 'expected several MADLIBS genres, found ' + genres.size);
  genres.forEach((g) => {
    const mapped = Seed.GENRE_FOR[g];
    assert(mapped, 'no mapping for MADLIBS genre "' + g + '"');
    assert(LEX.GENRES[mapped], g + ' maps to "' + mapped + '", which SCRIPT FORGE does not have');
  });
});

test('every MADLIBS story yields an idea a film can be made from', () => {
  for (let seed = 0; seed < 60; seed++) {
    const idea = Seed.idea(seed);
    assert(idea && typeof idea.text === 'string' && idea.text.length > 20,
      'seed ' + seed + ' gave no usable idea');
    assert(LEX.GENRES[idea.genre], 'seed ' + seed + ' gave genre ' + idea.genre);

    const premise = Parse.parse(idea.text, { seed, genre: idea.genre });
    const script = Writer.write(premise, { length: 'short', seed });
    assert(script.scenes.length >= 3, 'seed ' + seed + ' produced ' + script.scenes.length + ' scenes');
    assert(script.title && script.title.length, 'seed ' + seed + ' produced no title');
    const reel = Reel.build(script);
    assert(reel.duration > 30, 'seed ' + seed + ' produced a ' + reel.duration + 's film');
  }
});

test('the same seed always gives the same story', () => {
  for (let seed = 0; seed < 20; seed++) {
    eq(Seed.idea(seed).text, Seed.idea(seed).text, 'seed ' + seed + ' was not deterministic');
  }
});

test('different seeds give different stories', () => {
  const seen = new Set();
  for (let seed = 0; seed < 40; seed++) seen.add(Seed.idea(seed).text);
  assert(seen.size >= 20, 'only ' + seen.size + ' distinct stories in 40 seeds');
});

test('a borrowed story never says "a" before a vowel sound', () => {
  // MADLIBS decides its article before it knows which role fills the slot,
  // so roughly 4-6% of borrowed loglines used to read "a astronaut", "a
  // apothecary", "a archaeologist". Checked across a large sample rather
  // than a handful of fixed strings, since the bug depends on which role
  // MADLIBS happens to roll.
  let offenders = [];
  for (let seed = 0; seed < 4000; seed++) {
    const text = Seed.idea(seed).text;
    const bad = text.match(/(?:^|\s)[Aa] [aeiouAEIOU]\w*/g);
    if (bad) offenders.push(seed + ': ' + JSON.stringify(bad));
  }
  eq(offenders.length, 0, offenders.length + ' of 4000 borrowed loglines say "a" before a vowel sound: ' +
    offenders.slice(0, 5).join(' | '));
});

test('an article is only an article when a space follows it', () => {
  // "the" used to match inside "they": "discovers they are the last heir"
  // yielded the object "y are", and so a film titled "THE Y ARE". Any typed
  // idea containing "they" after a find/discover verb hit this.
  const p = Parse.parse('A brazen pilot named Cordelia discovers they are the last heir to Umberfall.',
    { seed: 7, genre: 'fantasy' });
  assert(!/\b(are|were|was|is|be|to|of|and|they)\b/i.test(p.object),
    'object came back as ' + JSON.stringify(p.object));
  assert(!/\bY ARE\b/.test(p.title), 'title came back as ' + JSON.stringify(p.title));

  // and the objects it is supposed to find are still found
  [['a lighthouse keeper finds a radio that plays tomorrow', 'radio'],
   ['a kid finds a walkie-talkie in an attic', 'walkie-talkie'],
   ['a thief steals the duffel bag', 'duffel bag'],
   ['she discovers letters in the attic', 'letters']].forEach((pair) => {
    eq(Parse.parse(pair[0], { seed: 1 }).object, pair[1], 'object from: ' + pair[0]);
  });
});

/* ------------------------------------------------------------------ report */
console.log('\nUNDER AN OPEN SKY');

/* A film used to be two interiors. It now has three to five places and about a
 * third of its scenes are exteriors, which is how "Rain finds the same crack in
 * the sill it always finds" ended up in a parking lot. LEX.OUTDOORS gives those
 * lines an outdoor twin and the writer swaps them when the scene is EXT. */

const INTERIOR_WORDS = /\b(sill|floor|ceiling|walls?|doorway|hallway|rooms?|corridor|radiator|counter|fridge|floorboard|kettle)\b/i;

// Everything the lexicon can put on a page, as raw strings: a key that matches
// none of these is a typo and would swap nothing, silently.
function everyLexiconLine() {
  const out = new Set();
  Object.keys(LEX.GENRES).forEach((g) => {
    LEX.GENRES[g].details.forEach((x) => out.add(x));
    LEX.GENRES[g].sounds.forEach((x) => out.add(x));
  });
  LEX.BEATS.forEach((b) => {
    ['action', 'actions', 'lines', 'shots', 'openers'].forEach((field) => {
      if (Array.isArray(b[field])) b[field].forEach((x) => out.add(x));
    });
  });
  return out;
}

test('every outdoor swap replaces a line that really exists', () => {
  const lines = everyLexiconLine();
  const orphans = Object.keys(LEX.OUTDOORS).filter((k) => !lines.has(k));
  eq(orphans.length, 0,
    'these OUTDOORS keys match nothing in the lexicon, so they would never fire: ' +
    JSON.stringify(orphans));
});

test('an outdoor twin is never itself swapped again', () => {
  Object.keys(LEX.OUTDOORS).forEach((k) => {
    const twin = LEX.OUTDOORS[k];
    assert(LEX.OUTDOORS[twin] === undefined,
      'the twin of "' + k + '" is itself a key, so the swap would chain');
    assert(twin !== k, 'the twin of "' + k + '" is the same line');
  });
});

test('an outdoor twin mentions nothing that needs a ceiling', () => {
  Object.keys(LEX.OUTDOORS).forEach((k) => {
    const twin = LEX.OUTDOORS[k];
    assert(!INTERIOR_WORDS.test(twin),
      '"' + twin + '" is the outdoor twin of "' + k + '" but still names an interior');
  });
});

test('no exterior scene uses a line that needs a room around it', () => {
  const keys = new Set(Object.keys(LEX.OUTDOORS));
  const ideas = [
    "A lighthouse keeper finds a radio that plays tomorrow's news.",
    'A courier discovers a package that hums.',
    'Two sisters inherit a house that remembers them.',
    'A detective loses the only witness who believed her.',
    'A diver finds a door on the seabed.'
  ];
  const offenders = [];
  for (let seed = 0; seed < 300; seed++) {
    const script = Writer.write(Parse.parse(ideas[seed % ideas.length], { seed }),
      { length: 'short', seed });
    script.scenes.forEach((scene) => {
      if (scene.heading.int !== 'EXT.') return;
      scene.elements.forEach((el) => {
        if (el.type !== 'action') return;
        keys.forEach((k) => {
          // The raw line, and the way it reads once capitalised on the page.
          const shown = k.charAt(0).toUpperCase() + k.slice(1);
          if (el.text.indexOf(shown) !== -1 && offenders.length < 5) {
            offenders.push(scene.heading.text + ' — ' + el.text);
          }
        });
      });
    });
  }
  eq(offenders.length, 0, 'exterior scenes still reading as interiors: ' +
    JSON.stringify(offenders));
});

test('an interior scene keeps the interior line', () => {
  // The swap must be per scene, not global: a lamp room should still have a
  // radiator in it. Proven by finding at least one interior line still in use.
  let found = false;
  const keys = Object.keys(LEX.OUTDOORS);
  for (let seed = 0; seed < 300 && !found; seed++) {
    const script = Writer.write(Parse.parse('A lighthouse keeper finds a radio.', { seed }),
      { length: 'festival', seed });
    script.scenes.forEach((scene) => {
      if (scene.heading.int !== 'INT.') return;
      scene.elements.forEach((el) => {
        if (el.type !== 'action') return;
        keys.forEach((k) => {
          const shown = k.charAt(0).toUpperCase() + k.slice(1);
          if (el.text.indexOf(shown) !== -1) found = true;
        });
      });
    });
  }
  assert(found, 'no interior scene used an interior line — the swap is firing everywhere');
});

console.log('\nWHERE A CHARACTER IS LOOKING');

/* Two figures in a scene used to stare straight ahead regardless of each other,
 * which is why a two-shot read as two portraits rather than a conversation.
 * gazeAt turns the head, and the torso less, toward the other figure. */

function gazeWithinLimits(pose, where) {
  Object.keys(Figures.POSE_LIMITS).forEach((joint) => {
    const lo = Figures.POSE_LIMITS[joint][0];
    const hi = Figures.POSE_LIMITS[joint][1];
    assert(pose[joint] >= lo && pose[joint] <= hi,
      where + ': ' + joint + ' = ' + pose[joint] + ' is outside [' + lo + ', ' + hi + ']');
  });
}

test('a figure turns toward someone standing to their right', () => {
  const rest = Figures.POSES.stand;
  const turned = Figures.gazeAt(rest, 100, 400, 1);
  assert(turned.head > rest.head,
    'head should turn positive (toward +x) for a listener on the right, got ' + turned.head);
  assert(turned.torso > rest.torso, 'the torso should follow the head, got ' + turned.torso);
  assert(Math.abs(turned.torso - rest.torso) < Math.abs(turned.head - rest.head),
    'the torso should turn less than the head');
});

test('a figure turns the other way for someone on their left', () => {
  const rest = Figures.POSES.stand;
  const right = Figures.gazeAt(rest, 100, 400, 1);
  const left = Figures.gazeAt(rest, 400, 100, 1);
  assert(left.head < rest.head, 'head should turn negative for a listener on the left');
  assert(Math.abs(left.head - rest.head) - Math.abs(right.head - rest.head) < 1e-9,
    'the turn should be symmetric either way');
});

test('nobody turns toward themselves', () => {
  const rest = Figures.POSES.stand;
  const same = Figures.gazeAt(rest, 250, 250, 1);
  Object.keys(Figures.POSE_LIMITS).forEach((joint) => {
    eq(same[joint], rest[joint], 'gazing at your own position should change ' + joint);
  });
});

test('gaze never bends a neck further than a neck bends', () => {
  // Absurd distances and amounts must still produce a pose a human could hold.
  const names = Object.keys(Figures.POSES);
  [-1e6, -500, -1, 0, 1, 500, 1e6].forEach((otherX) => {
    [0, 0.5, 1, 4].forEach((amount) => {
      names.forEach((name) => {
        gazeWithinLimits(Figures.gazeAt(Figures.POSES[name], 0, otherX, amount),
          'gazeAt(' + name + ', 0, ' + otherX + ', ' + amount + ')');
      });
    });
  });
});

test('gaze leaves every joint but the head and torso alone', () => {
  const rest = Figures.POSES['hands-in-pockets'];
  const turned = Figures.gazeAt(rest, 0, 900, 1);
  Object.keys(Figures.POSE_LIMITS).forEach((joint) => {
    if (joint === 'head' || joint === 'torso') return;
    eq(turned[joint], rest[joint], joint + ' should not move when someone looks sideways');
  });
});

console.log('\nA STANDING PERSON IS NEVER STILL');

/* A figure held one pose exactly until the next cut, which is most of what made
 * them read as cardboard. aliveAt adds a slow weight shift, a shallow breath and
 * a head settle — small, and driven by the clock and the character's seed so two
 * recordings of one film still match frame for frame. */

test('being alive is deterministic', () => {
  for (let seed = 0; seed < 5; seed++) {
    for (const t of [0, 0.37, 1.5, 9.25, 240]) {
      const a = Figures.aliveAt(Figures.POSES.stand, t, seed);
      const b = Figures.aliveAt(Figures.POSES.stand, t, seed);
      Object.keys(Figures.POSE_LIMITS).forEach((joint) => {
        eq(a[joint], b[joint], 'aliveAt(stand, ' + t + ', ' + seed + ') differed on ' + joint);
      });
    }
  }
});

test('two characters do not breathe in lockstep', () => {
  // Same moment, different seeds: if these matched, a two-shot would look like
  // a chorus line.
  const a = Figures.aliveAt(Figures.POSES.stand, 3.1, 1);
  const b = Figures.aliveAt(Figures.POSES.stand, 3.1, 2);
  const differs = Object.keys(Figures.POSE_LIMITS).some((j) => a[j] !== b[j]);
  assert(differs, 'two seeds produced identical motion at the same instant');
});

test('being alive never leaves a pose a human could hold', () => {
  const names = Object.keys(Figures.POSES);
  for (const name of names) {
    for (let seed = 0; seed < 4; seed++) {
      for (let step = 0; step <= 40; step++) {
        const pose = Figures.aliveAt(Figures.POSES[name], step * 0.31, seed);
        gazeWithinLimits(pose, 'aliveAt(' + name + ', ' + (step * 0.31) + ', ' + seed + ')');
      }
    }
  }
});

test('being alive is a breath, not a dance', () => {
  // Every joint stays close to where the pose put it: this is life, not a new
  // pose. Bounded at a tenth of each joint's own range.
  const names = Object.keys(Figures.POSES);
  let worst = 0, worstAt = '';
  names.forEach((name) => {
    const rest = Figures.POSES[name];
    for (let seed = 0; seed < 4; seed++) {
      for (let step = 0; step <= 40; step++) {
        const pose = Figures.aliveAt(rest, step * 0.29, seed);
        Object.keys(Figures.POSE_LIMITS).forEach((joint) => {
          const range = Figures.POSE_LIMITS[joint][1] - Figures.POSE_LIMITS[joint][0];
          const drift = Math.abs(pose[joint] - rest[joint]) / range;
          if (drift > worst) { worst = drift; worstAt = name + '.' + joint; }
        });
      }
    }
  });
  assert(worst <= 0.1, 'the largest drift was ' + worst.toFixed(3) + ' of range at ' + worstAt +
    ' — that is a new pose, not a breath');
});

test('nobody is frozen', () => {
  // The opposite failure: aliveAt that returns the pose unchanged would pass
  // every test above and do nothing.
  const rest = Figures.POSES.stand;
  let moved = false;
  for (let step = 0; step <= 40 && !moved; step++) {
    const pose = Figures.aliveAt(rest, step * 0.23, 0);
    moved = Object.keys(Figures.POSE_LIMITS).some((j) => Math.abs(pose[j] - rest[j]) > 1e-6);
  }
  assert(moved, 'aliveAt never moved anything across 40 samples');
});

console.log('\nA CUT NO LONGER SNAPS');

/* Poses changed instantly at a cut. blendPoses eases between them. It existed
 * once and was deleted as dead code when nothing called it; this is the caller
 * it was waiting for. */

test('a blend starts and ends exactly where it should', () => {
  const a = Figures.POSES.stand, b = Figures.POSES['hands-in-pockets'];
  const at0 = Figures.blendPoses(a, b, 0);
  const at1 = Figures.blendPoses(a, b, 1);
  Object.keys(Figures.POSE_LIMITS).forEach((joint) => {
    eq(at0[joint], a[joint], 't=0 should be the first pose exactly, at ' + joint);
    eq(at1[joint], b[joint], 't=1 should be the second pose exactly, at ' + joint);
  });
});

test('a blend is monotonic between the two poses', () => {
  const a = Figures.POSES.stand, b = Figures.POSES['turn-away'];
  Object.keys(Figures.POSE_LIMITS).forEach((joint) => {
    const lo = Math.min(a[joint], b[joint]), hi = Math.max(a[joint], b[joint]);
    for (let step = 0; step <= 20; step++) {
      const v = Figures.blendPoses(a, b, step / 20)[joint];
      assert(v >= lo - 1e-9 && v <= hi + 1e-9,
        joint + ' left the span between the two poses at t=' + (step / 20) + ': ' + v);
    }
  });
});

test('no blend of any two poses bends past a human', () => {
  const names = Object.keys(Figures.POSES);
  names.forEach((from) => {
    names.forEach((to) => {
      for (let step = 0; step <= 20; step++) {
        gazeWithinLimits(Figures.blendPoses(Figures.POSES[from], Figures.POSES[to], step / 20),
          'blendPoses(' + from + ', ' + to + ', ' + (step / 20) + ')');
      }
    });
  });
});

test('a blend clamps a t outside 0..1 rather than overshooting', () => {
  const a = Figures.POSES.stand, b = Figures.POSES.recoil;
  const under = Figures.blendPoses(a, b, -3);
  const over = Figures.blendPoses(a, b, 4);
  Object.keys(Figures.POSE_LIMITS).forEach((joint) => {
    eq(under[joint], a[joint], 't below 0 should hold at the first pose, at ' + joint);
    eq(over[joint], b[joint], 't above 1 should hold at the second pose, at ' + joint);
  });
});

console.log('');
if (failures.length) {
  console.error('✖ ' + failures.length + ' failing test(s):');
  failures.forEach((f) => console.error('  - ' + f));
  process.exit(1);
}
console.log('✓ ' + passed + ' tests passed\n');
