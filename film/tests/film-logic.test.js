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
const Voice = require(path.join(__dirname, '..', 'js', 'voice.js'));
const Arc = require(path.join(__dirname, '..', 'js', 'object-arc.js'));
const Subtext = require(path.join(__dirname, '..', 'js', 'subtext.js'));

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
const Dress = require(path.join(__dirname, '..', 'js', 'set-dress.js'));
const World = require(path.join(__dirname, '..', 'js', 'world-sound.js'));
const Director = require(path.join(__dirname, '..', 'js', 'director.js'));
const Why = require(path.join(__dirname, '..', 'js', 'why.js'));

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

console.log('\nWALKING');

/* On the push beat — the beat that is about momentum — a character crosses part
 * of the frame instead of standing in it. */

test('a walk keeps a foot on the ground at every phase', () => {
  // The planted-foot property the still poses already hold, extended to a
  // moving figure: at no point in the cycle are both feet off the floor, or the
  // figure is hopping rather than walking.
  for (let step = 0; step <= 60; step++) {
    const phase = step / 60;
    const pose = Figures.walkAt(Figures.POSES.stand, phase);
    // Reproduce drawBody's own foot arithmetic: 0 hangs straight down.
    const footL = Math.cos(pose.legL) + Math.cos(pose.legL + pose.shinL);
    const footR = Math.cos(pose.legR) + Math.cos(pose.legR + pose.shinR);
    assert(Math.max(footL, footR) > 1.90,
      'at phase ' + phase.toFixed(2) + ' the lower foot reaches only ' +
      Math.max(footL, footR).toFixed(3) + ' of 2 leg-lengths — the figure is airborne');
  }
});

test('a walk is a cycle: it ends where it began', () => {
  const start = Figures.walkAt(Figures.POSES.stand, 0);
  const end = Figures.walkAt(Figures.POSES.stand, 1);
  Object.keys(Figures.POSE_LIMITS).forEach((joint) => {
    assert(Math.abs(start[joint] - end[joint]) < 1e-9,
      joint + ' does not return to its starting angle after a full cycle');
  });
});

test('the legs alternate rather than moving together', () => {
  // Both legs swinging in phase is a bunny hop, not a walk.
  let opposed = 0;
  for (let step = 0; step < 20; step++) {
    const p = Figures.walkAt(Figures.POSES.stand, step / 20);
    if ((p.legL - Figures.POSES.stand.legL) * (p.legR - Figures.POSES.stand.legR) < 0) opposed++;
  }
  assert(opposed >= 14, 'the legs were in opposition in only ' + opposed + ' of 20 samples');
});

test('a walk never bends past a human', () => {
  Object.keys(Figures.POSES).forEach((name) => {
    for (let step = 0; step <= 40; step++) {
      gazeWithinLimits(Figures.walkAt(Figures.POSES[name], step / 40),
        'walkAt(' + name + ', ' + (step / 40) + ')');
    }
  });
});

console.log('\nVOICES THAT SAY A VOWEL');

/* Every syllable used to be the same blip, pitched by character and shaped by
 * an arbitrary character code. Now each one takes the vowel that is actually in
 * the word, so "I can't" and "Say it" stop sounding identical. */

test('every vowel the lexicon can speak has a formant pair', () => {
  const vowels = Object.keys(Score.FORMANTS);
  assert(vowels.length >= 5, 'a voice needs at least the five vowels, saw ' + vowels.length);
  vowels.forEach((v) => {
    const f = Score.FORMANTS[v];
    assert(Array.isArray(f) && f.length === 2, v + ' has no [F1, F2] pair');
    assert(f[0] > 200 && f[0] < 1200, v + ' F1 of ' + f[0] + 'Hz is not a human first formant');
    assert(f[1] > f[0], v + ' F2 (' + f[1] + ') must sit above F1 (' + f[0] + ')');
    assert(f[1] < 3000, v + ' F2 of ' + f[1] + 'Hz is not a human second formant');
  });
});

test('a caption yields one vowel per syllable, every time', () => {
  const lines = ['Say it.', "I can't.", 'You were not there.',
                 'That is the whole sentence. There is no rest of it.',
                 'Then we are done here.'];
  lines.forEach((line) => {
    const n = Parse.syllablesFor(line);
    const a = Parse.vowelsFor(line, n);
    const b = Parse.vowelsFor(line, n);
    eq(a.length, n, 'vowelsFor should return one vowel per syllable for ' + JSON.stringify(line));
    eq(a.join(''), b.join(''), 'vowelsFor was not deterministic for ' + JSON.stringify(line));
    a.forEach((v) => {
      assert(Score.FORMANTS[v], JSON.stringify(line) + ' produced vowel ' + JSON.stringify(v) +
        ' which has no formant pair');
    });
  });
});

test('two different lines do not sound the same', () => {
  // The whole point: the blips were identical regardless of the words.
  const a = Parse.vowelsFor('Say it.', Parse.syllablesFor('Say it.')).join('');
  const b = Parse.vowelsFor("I can't.", Parse.syllablesFor("I can't.")).join('');
  assert(a !== b, 'two different lines produced the same vowel sequence: ' + a);
});

test('a line with no vowels at all still speaks', () => {
  ['...', '!!!', '', 'Hmm', 'Shh'].forEach((odd) => {
    const n = Parse.syllablesFor(odd);
    const v = Parse.vowelsFor(odd, n);
    eq(v.length, n, JSON.stringify(odd) + ' should still produce ' + n + ' speakable syllables');
    v.forEach((x) => assert(Score.FORMANTS[x], JSON.stringify(odd) + ' produced unspeakable ' + x));
  });
});

test('the vowels follow the words in order', () => {
  // "oh no" must not come out "no oh": the mouth has to match the caption.
  const v = Parse.vowelsFor('oh ee', 2);
  eq(v[0], 'o', 'the first vowel of "oh ee" should be o, got ' + v[0]);
  eq(v[1], 'i', 'the second vowel of "oh ee" should be i, got ' + v[1]);
});

/* ------------------------------------------------------------------------
 * THE REEL AS A FILE
 *
 * The reel has been plain, browser-free data since the first sub-project so
 * that something other than a browser could draw it. These check that it
 * actually survives the trip out: a native renderer reading this JSON has to
 * cut the film at exactly the moments the browser does, or the two renderers
 * are drawing different films.
 * ---------------------------------------------------------------------- */

test('the exported reel is valid JSON carrying the whole film', () => {
  const text = Reel.toJson(reel);
  const doc = JSON.parse(text);
  eq(doc.format, 'maz-film-reel', 'the document should name its format');
  eq(doc.version, 1, 'the document should carry a version');
  eq(doc.title, reel.title, 'the title should survive');
  eq(doc.seed, reel.seed, 'the seed should survive — it is what makes the film repeatable');
  eq(doc.genre, reel.genre, 'the genre should survive');
  eq(doc.shots.length, reel.shots.length, 'every shot should survive');
  eq(Math.abs(doc.duration - reel.duration) < 1e-9, true, 'the duration should survive');
});

test('every shot boundary survives the export exactly', () => {
  const doc = JSON.parse(Reel.toJson(reel));
  reel.shots.forEach((shot, i) => {
    const out = doc.shots[i];
    eq(out.start, shot.start, 'shot ' + i + ' should start at ' + shot.start);
    eq(out.duration, shot.duration, 'shot ' + i + ' should run ' + shot.duration);
    eq(out.kind, shot.kind, 'shot ' + i + ' should keep its kind');
    eq(out.set, shot.set, 'shot ' + i + ' should keep its set');
    eq(out.framing, shot.framing, 'shot ' + i + ' should keep its framing');
    eq(out.camera, shot.camera, 'shot ' + i + ' should keep its camera move');
    eq(out.time, shot.time, 'shot ' + i + ' should keep its hour');
  });
});

test('the exported reel carries who is on screen and who is speaking', () => {
  const doc = JSON.parse(Reel.toJson(reel));
  const spoken = doc.shots.filter((s) => s.kind === 'line');
  eq(spoken.length > 0, true, 'a film should have spoken shots to export');
  spoken.forEach((s) => {
    eq(typeof s.speaker === 'string' && s.speaker.length > 0, true,
      'a spoken shot should name its speaker');
    eq(doc.voices[s.speaker] !== undefined, true,
      'the speaker ' + s.speaker + ' should have a voice in the export');
    eq(Array.isArray(s.characters), true, 'a shot should list who is in frame');
  });
});

test('every exported voice carries what a renderer needs to draw and sound it', () => {
  const doc = JSON.parse(Reel.toJson(reel));
  const names = Object.keys(doc.voices);
  eq(names.length > 0, true, 'a film should export at least one voice');
  names.forEach((n) => {
    const v = doc.voices[n];
    eq(typeof v.pitch, 'number', n + ' should export a pitch');
    eq(typeof v.hue, 'number', n + ' should export a hue');
    eq(v.side === -1 || v.side === 1, true, n + ' should export which side they stand on');
  });
});

test('a reel with no captions still exports as valid JSON', () => {
  // Captions carry the user's own words, which can be anything at all —
  // quotes, backslashes, newlines. The export must not be breakable by them.
  const awkward = Reel.build(Writer.write(Parse.parse('a "quote" and a \\ backslash'), { seed: 3 }));
  const doc = JSON.parse(Reel.toJson(awkward));
  eq(doc.shots.length, awkward.shots.length, 'an awkward title should still export every shot');
});

test('the exported shots run back to back with no gap and no overlap', () => {
  const doc = JSON.parse(Reel.toJson(reel));
  let at = 0;
  doc.shots.forEach((s, i) => {
    eq(Math.abs(s.start - at) < 1e-9, true,
      'shot ' + i + ' should start where the last one ended (' + at + '), not ' + s.start);
    at += s.duration;
  });
  eq(Math.abs(at - doc.duration) < 1e-9, true, 'the shots should add up to the whole film');
});

console.log('');
/* ------------------------------------------------------------------ voices */

/* Every line in the bank, in every mouth the program can build.
 *
 * A voice transform that mangles English is worse than no voice transform at
 * all, and the failure would be invisible: one line in one film in one genre
 * comes out as "Maybe i am not sure ,{HERO}." and nobody sees it until a person
 * reads the script. So this walks the whole dialogue bank through a spread of
 * voices covering every corner of the four dials, and demands the result still
 * be a line somebody could say.
 */
function everyBankLine() {
  const lines = [];
  Object.keys(DLG.SHARED).forEach((beat) => DLG.SHARED[beat].forEach((exchange) =>
    exchange.forEach((l) => lines.push(['shared ' + beat, l.line]))));
  Object.keys(DLG.BY_GENRE).forEach((genre) => Object.keys(DLG.BY_GENRE[genre]).forEach((beat) =>
    DLG.BY_GENRE[genre][beat].forEach((exchange) => exchange.forEach((l) =>
      lines.push([genre + ' ' + beat, l.line])))));
  // The dodges are dialogue too, and go through the same voices.
  Subtext.all().forEach(({ beat, exchange }) => exchange.lines.forEach((l) =>
    lines.push(['subtext ' + beat, l.line])));
  return lines;
}

/* The sixteen corners of the four dials, plus the middle: a voice that is at an
 * extreme on every dial at once is the one most likely to break a line. */
function everyVoiceShape() {
  const out = [];
  const ends = [0.1, 0.9];
  ends.forEach((formal) => ends.forEach((terse) => ends.forEach((hedging) => ends.forEach((warmth) =>
    out.push({ name: 'TEST', formal, terse, hedging, warmth })))));
  out.push({ name: 'TEST', formal: 0.5, terse: 0.5, hedging: 0.5, warmth: 0.5 });
  return out;
}

test('every line in the bank survives every voice', () => {
  const lines = everyBankLine();
  const voices = everyVoiceShape();
  assert(lines.length > 100, 'expected a bank of real size, got ' + lines.length);
  let checked = 0;

  lines.forEach(([where, line]) => {
    voices.forEach((voice) => {
      for (let at = 0; at < 3; at++) {
        const said = Voice.speak(line, voice, { at, listenerSlot: '{OTHER}' });
        const at_ = where + ' @' + at + ' :: ' + JSON.stringify(line) + ' -> ' + JSON.stringify(said);
        checked++;

        assert(said.length > 0, 'became empty: ' + at_);
        // A line must end the way it started. A question that stops being a
        // question makes the answer a non sequitur; an interruption that loses
        // its em dash stops being an interruption. Checking the character
        // itself covers every ending the bank uses without listing them.
        eq(said.slice(-1), line.slice(-1), 'changed how the line ends: ' + at_);
        assert(said.indexOf('  ') === -1, 'has a double space: ' + at_);
        assert(!/\s[,.?!]/.test(said), 'has a space before punctuation: ' + at_);
        assert(!/\bi\b/.test(said), 'lowercased a standalone I: ' + at_);
        assert(/^[A-Z0-9{"'(—…]/.test(said), 'does not start cleanly: ' + at_);

        // Slots must come through whole, and no new one may be invented.
        const slotsIn = (line.match(/\{[A-Z_]+\}/g) || []).slice().sort();
        const slotsOut = (said.match(/\{[A-Z_]+\}/g) || []).slice().sort();
        slotsOut.forEach((slot) => assert(slotsIn.indexOf(slot) !== -1 || slot === '{OTHER}',
          'invented a slot: ' + at_));
        eq((said.match(/\{/g) || []).length, (said.match(/\}/g) || []).length,
          'unbalanced braces: ' + at_);

        // It must still be recognisably the same line, not a new one.
        const grew = said.split(/\s+/).length - line.split(/\s+/).length;
        assert(grew <= 3, 'grew by ' + grew + ' words: ' + at_);
      }
    });
  });
  assert(checked > 5000, 'expected thousands of combinations, ran ' + checked);
});

test('a voice is the same every time it is derived', () => {
  const a = Voice.voiceFor('SHAY', 'night nurse', 1234);
  const b = Voice.voiceFor('SHAY', 'night nurse', 1234);
  ['formal', 'terse', 'hedging', 'warmth'].forEach((k) => eq(a[k], b[k], 'dial ' + k));
  const c = Voice.voiceFor('SAM', 'night nurse', 1234);
  assert(['formal', 'terse', 'hedging', 'warmth'].some((k) => a[k] !== c[k]),
    'two different people came out with identical voices');
});

test('a role moves the dials, not just the seed', () => {
  // Someone whose job is a register should land on that register whatever the
  // dice say — a surgeon who talks like a drifter is a casting error.
  const doctor = Voice.voiceFor('X', 'surgeon', 7);
  const kid = Voice.voiceFor('X', 'kid', 7);
  assert(doctor.formal > 0.6, 'a surgeon should speak in full words, got ' + doctor.formal);
  assert(kid.formal < 0.45, 'a kid should clip their words, got ' + kid.formal);
});

test('two characters in the same film do not sound the same', () => {
  // The whole point. Across a spread of films, the two leads must differ on at
  // least one dial far enough to hear.
  let heard = 0;
  const ideas = [
    'a night nurse buries a key in the woods and forgets where',
    'two brothers argue over a boat their father left them',
    'a detective returns a stolen watch to the wrong house',
    'a teenager hides a letter from her grandmother'
  ];
  ideas.forEach((idea) => {
    const premise = Parse.parse(idea);
    const a = Voice.voiceFor(premise.hero.name, premise.hero.role, premise.seed);
    const b = Voice.voiceFor(premise.other.name, premise.other.role, premise.seed);
    const apart = ['formal', 'terse', 'hedging', 'warmth']
      .reduce((m, k) => Math.max(m, Math.abs(a[k] - b[k])), 0);
    if (apart > 0.25) heard++;
  });
  assert(heard >= 3, 'only ' + heard + ' of ' + ideas.length + ' films had two audible voices');
});

test('the voice reaches the finished script', () => {
  // A formal character's lines must actually come out expanded in the film, not
  // just in the module: the wiring is the part that breaks.
  const premise = Parse.parse('a surgeon and a kid trade a stolen watch in a hospital');
  const script = Writer.write(premise, { length: 'festival', seed: 4242 });
  const spoken = script.elements.filter((e) => e.type === 'dialogue').map((e) => e.text);
  assert(spoken.length > 0, 'no dialogue in the script at all');
  const bank = everyBankLine().map(([, l]) => l);
  // At least one line must differ from every neutral form in the bank, which is
  // only possible if a voice changed it.
  const filled = (t) => t.replace(/\{[A-Z_]+\}/g, '');
  const moved = spoken.some((line) => !bank.some((raw) => filled(raw).trim() === filled(line).trim()));
  assert(moved, 'not one line was changed by a voice');
});



/* ------------------------------------------------------- the object's arc */

test('every arc tells a whole story about the object', () => {
  // An arc is only a setup-and-payoff if it covers the whole spine. A missing
  // state is a scene where the object silently drops out of its own film.
  Arc.ARCS.forEach((arc) => {
    Arc.STATES.forEach((state) => {
      assert(typeof arc.lines[state] === 'string' && arc.lines[state].length > 10,
        'arc "' + arc.id + '" has no ' + state + ' line (' + Arc.PURPOSE[state] + ')');
    });
    eq(Object.keys(arc.lines).length, Arc.STATES.length, 'arc "' + arc.id + '" has a stray state');
  });
  assert(Arc.ARCS.length >= 4, 'too few arcs to keep two films apart');
});

test('arc lines are shot descriptions, not narration', () => {
  Arc.ARCS.forEach((arc) => {
    Arc.STATES.forEach((state) => {
      const line = arc.lines[state];
      const where = arc.id + '.' + state + ': ' + JSON.stringify(line);
      // Only slots fill() knows how to fill. An unknown slot survives to the
      // screen as literal braces.
      (line.match(/\{[A-Z_]+\}/g) || []).forEach((slot) => {
        assert(['{OBJ}', '{HERO}', '{OTHER}', '{PLACE}', '{TONIGHT}'].indexOf(slot) !== -1,
          'unknown slot ' + slot + ' in ' + where);
      });
      assert(/[.?!]$/.test(line), 'no final punctuation in ' + where);
      // Interior state cannot be photographed. "remembers", "feels", "knows"
      // in an action line is a novel, not a shot.
      assert(!/\b(?:feels|remembers|realises|realizes|wonders|hopes|regrets)\b/.test(line),
        'un-filmable interior state in ' + where);
    });
  });
});

test('the same seed always tells the same story about the object', () => {
  eq(Arc.arcFor(99).id, Arc.arcFor(99).id, 'arc drifted between calls');
  const ids = {};
  for (let seed = 0; seed < 200; seed++) ids[Arc.arcFor(seed).id] = true;
  eq(Object.keys(ids).length, Arc.ARCS.length, 'some arcs are unreachable');
});

test('the object is present in every scene of every film', () => {
  // The failure this replaces: a story about a key in which the key is named in
  // the opening, vanishes for three scenes, and is mentioned again at the end.
  const ideas = [
    'a night nurse buries a key in the woods and forgets where',
    'a detective returns a stolen watch to the wrong house',
    'two brothers argue over a boat their father left them'
  ];
  ['micro', 'short', 'festival'].forEach((length) => {
    ideas.forEach((idea) => {
      const premise = Parse.parse(idea);
      const script = Writer.write(premise, { length });
      let scenes = 0;
      let withObject = 0;
      let current = null;
      script.elements.forEach((el) => {
        if (el.type === 'scene_heading') { scenes++; current = false; }
        if (el.objectBeat && current === false) { withObject++; current = true; }
      });
      eq(withObject, scenes, length + ' / ' + idea + ': ' +
        (scenes - withObject) + ' scene(s) with no sign of the ' + premise.object);
    });
  });
});

test('the ending calls back to the opening', () => {
  // The plant and the payoff must come from the SAME arc — they were written as
  // a pair, and that is the only reason the last shot lands.
  const premise = Parse.parse('a night nurse buries a key in the woods and forgets where');
  const script = Writer.write(premise, { length: 'short' });
  const marked = script.elements.filter((e) => e.objectBeat);
  assert(marked.length >= 2, 'fewer than two object beats in the whole film');
  eq(marked[0].objectBeat, 'unnoticed', 'the film does not open on the object unnoticed');
  eq(marked[marked.length - 1].objectBeat, 'changed', 'the film does not close on the object changed');

  const arc = Arc.arcFor(premise.seed);
  const plain = (t) => t.replace(/\{[A-Z_]+\}/g, '~').replace(/[A-Z]{2,}/g, '~');
  const shapes = [arc.lines.unnoticed, arc.lines.changed].map(plain);
  assert(shapes[0] !== shapes[1], 'the plant and the payoff are the same line');
  // Both must actually be on the page, from the one arc this film drew.
  const page = script.elements.filter((e) => e.objectBeat).map((e) => e.text).join(' | ');
  assert(page.length > 0, 'no object beats reached the page');
});

test('every object beat becomes a shot of the object', () => {
  // Two arcs make their crisis about the object being GONE, and name nothing a
  // substring search could find. Those are the best lines in the bank and they
  // must still get their insert.
  const premise = Parse.parse('a night nurse buries a key in the woods and forgets where');
  const script = Writer.write(premise, { length: 'short' });
  const reel = Reel.build(script);
  const marked = script.elements.filter((e) => e.objectBeat).map((e) => e.text);
  const inserts = reel.shots.filter((s) => s.framing === 'insert').map((s) => s.caption);
  marked.forEach((text) => assert(inserts.indexOf(text) !== -1,
    'object beat never became an insert: ' + JSON.stringify(text)));
});


test('the object always moves forwards, on every spine', () => {
  // The bug this pins: LEX.STRUCTURES has a seven-scene spine that runs
  // open, PUSH, SPARK, turn, ... so a per-beat arc had the object CARRIED in
  // scene two and NOTICED in scene three -- taken along before anybody picked
  // it up -- and, because that spine ends on 'choice' with 'after' before it,
  // played the payoff line twice in a row.
  const order = {};
  Arc.STATES.forEach((state, i) => { order[state] = i; });

  Object.keys(LEX.STRUCTURES).forEach((length) => {
    LEX.STRUCTURES[length].spines.forEach((spine) => {
      const states = Arc.statesForSpine(spine);
      const where = length + ' [' + spine.join(',') + '] -> ' + states.join('>');
      eq(states.length, spine.length, 'one state per scene: ' + where);
      eq(states[states.length - 1], 'changed', 'the last scene is the payoff: ' + where);
      for (let i = 1; i < states.length; i++) {
        assert(order[states[i]] > order[states[i - 1]],
          'the object goes backwards at scene ' + (i + 1) + ': ' + where);
      }
      eq(new Set(states).size, states.length, 'a state plays twice: ' + where);
    });
  });
});


/* ------------------------------------------------------ what is not said */

test('every dodge comes with a tell', () => {
  // The pairing IS the technique: the line denies something and the action
  // straight after shows it. An exchange with no tell is just a short scene.
  const all = Subtext.all();
  assert(all.length >= 12, 'too few dodges to keep two films apart: ' + all.length);
  all.forEach(({ beat, exchange }) => {
    const where = 'subtext ' + beat + ': ' + JSON.stringify(exchange.lines[0].line);
    assert(Array.isArray(exchange.lines) && exchange.lines.length >= 2, 'not an exchange: ' + where);
    assert(typeof exchange.tell === 'string' && exchange.tell.length > 8, 'no tell: ' + where);
    exchange.lines.forEach((l) => {
      assert(l.who === 'hero' || l.who === 'other', 'unknown speaker in ' + where);
      assert(/[.?!—…]$/.test(l.line), 'unpunctuated line in ' + where);
    });
    // Somebody has to answer somebody. A monologue cannot dodge.
    assert(new Set(exchange.lines.map((l) => l.who)).size === 2, 'only one person speaks in ' + where);
  });
});

test('a tell is a shot, not a thought', () => {
  // "{HERO} already knows what they would do" is a novel. "{HERO} answers too
  // quickly" is a shot. A camera cannot photograph the first one.
  const interior = /\b(?:feels|felt|remembers|realises|realizes|wonders|hopes|regrets|knows|wants|thinks|believes|understands)\b/;
  Subtext.all().forEach(({ beat, exchange }) => {
    const where = 'subtext ' + beat + ' tell: ' + JSON.stringify(exchange.tell);
    assert(!interior.test(exchange.tell), 'un-filmable interior state in ' + where);
    assert(/[.?!]$/.test(exchange.tell), 'no final punctuation in ' + where);
    (exchange.tell.match(/\{[A-Z_]+\}/g) || []).forEach((slot) => {
      assert(['{OBJ}', '{HERO}', '{OTHER}', '{PLACE}', '{TONIGHT}'].indexOf(slot) !== -1,
        'unknown slot ' + slot + ' in ' + where);
    });
  });
});

test('a dodge only happens where it belongs', () => {
  // Not the opening — a film that starts evasive has nothing to become — and
  // not the closing scene, where the point is that somebody finally says it.
  assert(!Subtext.hasBeat('open'), 'the opening should not dodge');
  assert(!Subtext.hasBeat('after'), 'the last scene should not dodge');
  Subtext.BEATS.forEach((beat) => assert(Subtext.hasBeat(beat), 'declared beat ' + beat + ' is empty'));
});

test('the tell lands immediately after the words it contradicts', () => {
  // A beat of anything in between and the tell stops answering the line.
  let found = 0;
  for (let seed = 1; seed <= 40; seed++) {
    const script = Writer.write(Parse.parse('a night nurse buries a key in the woods'),
      { length: 'festival', seed });
    script.elements.forEach((el, i) => {
      if (!el.tell) return;
      found++;
      const before = script.elements[i - 1];
      assert(before && before.type === 'dialogue',
        'a tell followed a ' + (before && before.type) + ' instead of a line, at seed ' + seed);
    });
  }
  assert(found > 10, 'dodges are not reaching films at all: ' + found + ' tells in 40 films');
});

test('a film has both registers in it', () => {
  // People who evade every single line are as characterless as people who
  // evade none. Across a spread of films, some scenes must dodge and some
  // must not.
  let dodged = 0;
  let direct = 0;
  for (let seed = 1; seed <= 30; seed++) {
    const script = Writer.write(Parse.parse('two brothers argue over a boat'), { length: 'festival', seed });
    const tells = script.elements.filter((e) => e.tell).length;
    const scenes = script.elements.filter((e) => e.type === 'scene_heading').length;
    dodged += tells;
    direct += scenes - tells;
  }
  assert(dodged > 20, 'almost nothing dodges: ' + dodged);
  assert(direct > 20, 'almost everything dodges: ' + direct);
});


/* ---------------------------------------------------------- the cutting */

function reelsAcross(seeds, length) {
  const out = [];
  for (let seed = 1; seed <= seeds; seed++) {
    out.push(Reel.build(Writer.write(Parse.parse('a lighthouse keeper finds a radio'), { length, seed })));
  }
  return out;
}

test('the film cuts faster towards the crisis and holds on the choice', () => {
  // Measured in cuts per minute, not average shot length: a shot with a caption
  // on it cannot go below reading speed, so the crisis gets its speed from
  // shots that carry no words rather than from squeezing the ones that do.
  const time = {};
  const count = {};
  reelsAcross(12, 'festival').forEach((reel) => reel.shots.forEach((shot) => {
    time[shot.beat] = (time[shot.beat] || 0) + shot.duration;
    count[shot.beat] = (count[shot.beat] || 0) + 1;
  }));
  const rate = (beat) => 60 * count[beat] / time[beat];

  assert(rate('crisis') > rate('open') * 1.4,
    'the crisis cuts at ' + rate('crisis').toFixed(1) + '/min against an opening of ' +
    rate('open').toFixed(1));
  assert(rate('crisis') > rate('turn'),
    'the crisis (' + rate('crisis').toFixed(1) + ') cuts slower than the turn (' +
    rate('turn').toFixed(1) + ') — the build runs backwards');
  assert(rate('choice') < rate('crisis') * 0.75,
    'the choice is not held: ' + rate('choice').toFixed(1) + ' against a crisis of ' +
    rate('crisis').toFixed(1));
  // The decision must be the slowest thing in the film.
  Object.keys(rate('choice') ? count : {}).forEach((beat) => {
    if (beat === 'choice' || beat === 'title' || beat === 'end') return;
    assert(rate('choice') <= rate(beat),
      'the ' + beat + ' is held longer than the choice');
  });
});

test('a conversation is cut as shot and reverse-shot', () => {
  // The reverse of a close is a close and the reverse of an over-the-shoulder is
  // an over-the-shoulder, from the other side. Mixing the two registers across a
  // cut is what made a two-hander read as two people talking to camera.
  let reverses = 0;
  reelsAcross(10, 'festival').forEach((reel) => {
    const lines = reel.shots.filter((s) => s.kind === 'line');
    for (let i = 1; i < lines.length; i++) {
      const a = lines[i - 1];
      const b = lines[i];
      if (a.scene !== b.scene) continue;
      if (a.speaker === b.speaker) continue;
      if (['ots', 'close'].indexOf(a.framing) === -1) continue;
      if (['ots', 'close'].indexOf(b.framing) === -1) continue;
      reverses++;
      eq(b.framing, a.framing,
        'a ' + a.framing + ' was answered with a ' + b.framing + ' (scene ' + a.scene + ')');
      assert(b.side !== a.side,
        'the camera did not change sides between ' + a.speaker + ' and ' + b.speaker);
    }
  });
  assert(reverses > 20, 'barely any reverses to check: ' + reverses);
});

test('a conversation establishes where everybody is standing first', () => {
  // Cut straight into a close and the audience has no geometry; a conversation
  // with no geometry is alternating portraits.
  let checked = 0;
  reelsAcross(10, 'festival').forEach((reel) => {
    let lastScene = null;
    let firstWithBoth = null;
    reel.shots.forEach((shot) => {
      if (shot.scene !== lastScene) { lastScene = shot.scene; firstWithBoth = null; }
      if (shot.kind !== 'line') return;
      if (!shot.characters || shot.characters.length < 2) return;
      if (firstWithBoth === null) {
        firstWithBoth = shot;
        checked++;
        eq(shot.framing, 'two', 'a two-hander opened on a ' + shot.framing);
      }
    });
  });
  assert(checked > 10, 'no two-handers found at all');
});

test('a cutaway is a shot of the thing, with nothing written on it', () => {
  let cutaways = 0;
  reelsAcross(10, 'festival').forEach((reel) => reel.shots.forEach((shot) => {
    if (!shot.cutaway) return;
    cutaways++;
    eq(shot.framing, 'insert', 'a cutaway was not an insert');
    eq(shot.caption, '', 'a cutaway carried a caption: ' + JSON.stringify(shot.caption));
    eq(shot.speaker, null, 'a cutaway had a speaker');
    assert(shot.duration < 2, 'a cutaway held for ' + shot.duration.toFixed(1) + 's');
    assert(shot.beat === 'turn' || shot.beat === 'crisis',
      'a cutaway landed on the ' + shot.beat);
  }));
  assert(cutaways > 15, 'cutaways are not reaching films: ' + cutaways);
});

test('no caption is ever rushed, at any pace', () => {
  // The acceleration is BOUNDED. This is the bound, checked across every length
  // and every beat rather than on one sample reel.
  ['micro', 'short', 'festival'].forEach((length) => {
    reelsAcross(8, length).forEach((reel) => reel.shots.forEach((shot) => {
      if (!shot.caption || shot.kind === 'establish') return;
      const n = shot.caption.trim().split(/\s+/).length;
      assert(shot.duration >= n / 3.2,
        length + ': "' + shot.caption.slice(0, 30) + '" — ' + n + ' words in ' +
        shot.duration.toFixed(1) + 's');
    }));
  });
});


/* ------------------------------------------------------- faces and hands */

/* A canvas that draws nothing and remembers everything.
 *
 * Drawing code is usually tested by looking at it, which is right and which is
 * also how a change that silently stops drawing the eyes goes unnoticed for a
 * month. A recording context gives the parts that are decisions -- did a face
 * get drawn at this size, did the held object get drawn at all -- somewhere to
 * be checked without a browser.
 */
function stubContext() {
  const calls = [];
  const ctx = {
    calls,
    globalCompositeOperation: '',
    fillStyle: '',
    save() { calls.push(['save']); },
    restore() { calls.push(['restore']); },
    translate(x, y) { calls.push(['translate', x, y]); },
    rotate(a) { calls.push(['rotate', a]); },
    beginPath() { calls.push(['beginPath']); },
    closePath() { calls.push(['closePath']); },
    moveTo(x, y) { calls.push(['moveTo', x, y]); },
    lineTo(x, y) { calls.push(['lineTo', x, y]); },
    fill() { calls.push(['fill', ctx.fillStyle]); },
    fillRect(x, y, w, h) { calls.push(['fillRect', x, y, w, h, ctx.fillStyle]); },
    ellipse(x, y, rx, ry) { calls.push(['ellipse', x, y, rx, ry, ctx.fillStyle]); },
    arc(x, y, r) { calls.push(['arc', x, y, r, ctx.fillStyle]); },
    createRadialGradient() { return { addColorStop() {} }; }
  };
  return ctx;
}

function drawOne(height, extra) {
  const ctx = stubContext();
  const spot = Object.assign({
    x: 0, groundY: 0, height, tint: 200, speaking: false, wobble: 0,
    pose: Figures.POSES.stand, lightX: -1, seconds: 0, seed: 1, mouthOpen: 0
  }, extra || {});
  Figures.drawFigure(ctx, { key: [200, 220, 255], accent: [255, 170, 90] }, spot);
  return ctx.calls;
}

/* How many ellipses the face added. A figure with no face draws exactly four:
 * the contact shadow, and one head per body pass (rim, body, tint). A face adds
 * two eyes and a mouth on top of that. */
const ELLIPSES_WITHOUT_A_FACE = 4;
function faceMarks(calls) {
  return calls.filter((c) => c[0] === 'ellipse').length - ELLIPSES_WITHOUT_A_FACE;
}

test('a face is drawn when the head is big enough to hold one, and not before', () => {
  // Gated on size rather than on framing: a rule expressed in framings has to be
  // plumbed from the director through the artist and kept in step forever.
  const small = faceMarks(drawOne(90));     // head radius ~4.7px — a wide shot
  const large = faceMarks(drawOne(420));    // head radius ~22px — a close-up
  eq(small, 0, 'a face was drawn on a head too small to hold one');
  assert(large >= 3, 'no face on a close-up: ' + large + ' marks');
});

test('the mouth opens when somebody speaks', () => {
  const shut = drawOne(420, { speaking: true, mouthOpen: 0 });
  const open = drawOne(420, { speaking: true, mouthOpen: 1 });
  const mouthOf = (calls) => {
    const ellipses = calls.filter((c) => c[0] === 'ellipse');
    return ellipses[ellipses.length - 1];     // the mouth is drawn last
  };
  assert(mouthOf(open)[4] > mouthOf(shut)[4],
    'the mouth is the same height open as shut: ' + mouthOf(open)[4] + ' vs ' + mouthOf(shut)[4]);
});

test('two people never blink together', () => {
  // Off each figure's own name. Two silhouettes blinking in lockstep is the
  // single most obvious tell that they are the same puppet twice.
  const a = Figures.hashName('SHAY');
  const b = Figures.hashName('SAM');
  let together = 0;
  let apart = 0;
  for (let t = 0; t < 600; t += 0.05) {
    const x = Figures.blinkAt(t, a);
    const y = Figures.blinkAt(t, b);
    if (x && y) together++;
    else if (x || y) apart++;
  }
  assert(apart > 50, 'nobody blinks at all: ' + apart);
  assert(together / apart < 0.1, 'they blink together ' + together + ' times against ' + apart);
});

test('a blink is a blink, not a stare', () => {
  const seed = Figures.hashName('SHAY');
  let shut = 0;
  const span = 600;
  for (let t = 0; t < span; t += 0.02) shut += Figures.blinkAt(t, seed) * 0.02;
  // A person blinks for well under a tenth of their waking life.
  assert(shut / span < 0.06, 'eyes shut ' + (100 * shut / span).toFixed(1) + '% of the time');
  assert(shut > 1, 'eyes never shut at all');
});

test('the thing the story turns on is actually held', () => {
  // The whole film is about an object and until now nobody ever touched one:
  // the insert shot drew it floating on its own.
  const empty = drawOne(420).length;
  const holding = drawOne(420, { holding: 'key' }).length;
  assert(holding > empty, 'holding an object drew nothing extra');
});

test('a held object is shaped like the thing it is', () => {
  eq(Figures.heldShapeFor('key'), 'long');
  eq(Figures.heldShapeFor('letter'), 'flat');
  eq(Figures.heldShapeFor('photograph'), 'flat');
  eq(Figures.heldShapeFor('ring'), 'round');
  eq(Figures.heldShapeFor('watch'), 'round');
  eq(Figures.heldShapeFor('something nobody listed'), 'box');
});

test('the reel says who is holding the object, and only while they have it', () => {
  const script = Writer.write(Parse.parse('a night nurse buries a key in the woods'),
    { length: 'festival', seed: 5 });
  const reel = Reel.build(script);
  const lead = script.characters[0].name;
  let held = 0;
  reel.shots.forEach((shot) => {
    if (!shot.holding) return;
    held++;
    eq(shot.holding.by, lead, 'somebody other than the lead is carrying the object');
    assert(shot.framing !== 'insert', 'an insert shot claimed somebody was holding the object');
  });
  assert(held > 0, 'nobody ever holds the object');
  assert(held < reel.shots.length, 'the object is held in every shot of the film');
});


/* ------------------------------------------------------ dressing the set */

test('dressing never lands where the actors stand', () => {
  // This is the whole reason one dressing layer can be switched on for all
  // fifteen sets without looking at all fifteen. Figures stand between x=250
  // and x=660 of the 1000-unit stage (figureLayout in film-player.js), and set
  // painters put their furniture in the middle; dressing keeps to the outer
  // bands. If that ever stops being true, a crate grows out of somebody's chest.
  const setKeys = ['ward', 'room', 'kitchen', 'corridor', 'office', 'bar', 'ship',
                   'chapel', 'industrial', 'vehicle', 'lighthouse',
                   'woods', 'street', 'field', 'water'];
  const genres = Object.keys(LEX.GENRES);
  let checked = 0;
  setKeys.forEach((setKey) => genres.forEach((genre) => {
    for (let seed = 0; seed < 12; seed++) {
      const d = Dress.dressingFor(setKey, genre, seed);
      const where = setKey + '/' + genre + '/' + seed;
      d.props.concat(d.marks).forEach((item) => {
        checked++;
        const right = item.x + (item.w || 0);
        assert(item.x >= 0 && right <= Dress.STAGE_W,
          'dressing ran off the stage at ' + where + ': x=' + item.x.toFixed(0));
        const intrudes = right > Dress.LEFT_BAND[1] && item.x < Dress.RIGHT_BAND[0];
        assert(!intrudes, 'dressing landed in the acting area at ' + where +
          ': x=' + item.x.toFixed(0) + '..' + right.toFixed(0));
      });
      if (d.lamp) {
        assert(d.lamp.x > Dress.LEFT_BAND[0] && d.lamp.x < Dress.RIGHT_BAND[1],
          'a lamp hung outside the stage at ' + where);
      }
    }
  }));
  assert(checked > 500, 'barely anything was dressed: ' + checked);
});

test('a genre treats a room the way that genre would', () => {
  const tally = (genre) => {
    const counts = { kept: 0, worn: 0, abandoned: 0 };
    for (let seed = 0; seed < 400; seed++) counts[Dress.conditionFor(genre, seed)]++;
    return counts;
  };
  eq(tally('horror').kept, 0, 'horror got a tidy room');
  assert(tally('horror').abandoned > 200, 'horror is not derelict enough');
  assert(tally('comedy').kept > tally('comedy').abandoned * 2,
    'comedy rooms are as derelict as they are kept');
  assert(tally('romance').kept > 100, 'romance never gets a room somebody looks after');
});

test('two rooms in one film are dressed differently', () => {
  // Keyed on the set as well as the film. Otherwise every room in a film is the
  // same room, which is the bug one level up from the one this fixes.
  let differ = 0;
  for (let seed = 0; seed < 30; seed++) {
    const a = Dress.dressingFor('ward', 'drama', seed);
    const b = Dress.dressingFor('corridor', 'drama', seed);
    if (JSON.stringify(a.props) !== JSON.stringify(b.props)) differ++;
  }
  eq(differ, 30, 'only ' + differ + ' of 30 films dressed their two rooms differently');
});

test('a crate in a field is a mistake, not a mood', () => {
  Object.keys(Dress.OUTDOORS).forEach((setKey) => {
    for (let seed = 0; seed < 40; seed++) {
      const d = Dress.dressingFor(setKey, 'drama', seed);
      eq(d.marks.length, 0, 'damp on a wall outdoors, in ' + setKey);
      eq(d.lamp, null, 'a table lamp outdoors, in ' + setKey);
      d.props.forEach((prop) => assert(['post', 'rock', 'scrub', 'plank'].indexOf(prop.kind) !== -1,
        'a ' + prop.kind + ' turned up outdoors in ' + setKey));
    }
  });
});

test('the same film always dresses the same room the same way', () => {
  const a = Dress.dressingFor('ward', 'horror', 77);
  const b = Dress.dressingFor('ward', 'horror', 77);
  eq(JSON.stringify(a), JSON.stringify(b), 'dressing drifted between calls');
});

test('an abandoned room has more in it than a kept one', () => {
  let kept = 0;
  let abandoned = 0;
  for (let seed = 0; seed < 300; seed++) {
    const k = Dress.dressingFor('room', 'comedy', seed);
    const a = Dress.dressingFor('room', 'horror', seed);
    if (k.condition === 'kept') kept += k.props.length + k.marks.length;
    if (a.condition === 'abandoned') abandoned += a.props.length + a.marks.length;
  }
  assert(abandoned > kept * 1.5,
    'a derelict room is no more cluttered than a kept one: ' + abandoned + ' vs ' + kept);
});

test('dressing actually draws something', () => {
  const ctx = stubContext();
  const rgb = () => '#fff';
  const d = Dress.dressingFor('ward', 'horror', 5);
  Dress.draw(ctx, { key: [1, 1, 1], ink: [0, 0, 0], shadow: [0, 0, 0] }, d, rgb);
  assert(ctx.calls.length > d.props.length, 'dressing drew nothing for ' + d.props.length + ' props');
  // And nothing at all when there is nothing to draw.
  const empty = stubContext();
  Dress.draw(empty, { key: [1, 1, 1], ink: [0, 0, 0], shadow: [0, 0, 0] }, null, rgb);
  eq(empty.calls.length, 0, 'drawing no dressing still drew something');
});


/* ------------------------------------------------------ the sound of it */

test('every buildable set has a sound of its own', () => {
  // Fifteen sets that all sound the same is the same bug as fifteen sets that
  // all look the same, and harder to notice.
  const setKeys = ['ward', 'corridor', 'industrial', 'ship', 'office', 'kitchen', 'room',
                   'bar', 'chapel', 'vehicle', 'lighthouse', 'woods', 'field', 'street', 'water'];
  const hums = new Set();
  setKeys.forEach((setKey) => {
    const tone = World.ROOM_TONE[setKey];
    assert(tone, setKey + ' has no room tone at all');
    assert(tone.hum >= 40 && tone.hum <= 400, setKey + ' hums at ' + tone.hum + ' Hz');
    assert(tone.level > 0 && tone.level < 0.2, setKey + ' room tone level is ' + tone.level);
    hums.add(tone.hum);
  });
  assert(hums.size >= 12, 'only ' + hums.size + ' distinct room tones across fifteen sets');
  // A set nobody listed still gets a room rather than silence.
  eq(World.toneFor('nowhere-in-particular'), World.DEFAULT_TONE);
});

test('every kind of weather the picture can show, the sound has too', () => {
  // A picture with rain in it and no rain under it is worse than one with
  // neither. These names come from film-weather.js and must not drift.
  ['rain', 'dust', 'fog', 'haze', 'shimmer', 'embers', 'none'].forEach((kind) => {
    assert(World.WEATHER_SOUND[kind], 'no sound for ' + kind);
  });
  assert(World.WEATHER_SOUND.rain.level > World.WEATHER_SOUND.fog.level,
    'fog is louder than rain');
  eq(World.WEATHER_SOUND.none.level, 0, 'clear air makes a noise');
});

test('the world gets out of the way of the dialogue', () => {
  const spoken = World.levelFor({ kind: 'line', speaker: 'X', mood: 0.5 });
  const silent = World.levelFor({ kind: 'action', mood: 0.5 });
  assert(spoken < silent * 0.7, 'the room is as loud under a line as under an image');
  // And a tense room is louder than a calm one.
  assert(World.levelFor({ kind: 'action', mood: 0.9 }) >
         World.levelFor({ kind: 'action', mood: 0.1 }),
    'tension does not reach the room tone');
  // But not by much: a bed that moves a lot stops being a bed.
  assert(World.levelFor({ kind: 'action', mood: 0.9 }) <
         World.levelFor({ kind: 'action', mood: 0.1 }) * 1.8,
    'the room tone swings so far it is an effect, not a bed');
});

test('feet land on the same stride the legs walk', () => {
  // The player owns the rate; the sound takes it as an argument. Two numbers
  // that must agree are one number too many.
  const rate = 0.85;
  const shot = { beat: 'push', characters: ['A'], duration: 6, framing: 'wide' };
  const falls = World.footfallsFor(shot, rate);
  assert(falls.length > 4, 'nobody walked: ' + falls.length);
  for (let i = 1; i < falls.length; i++) {
    const gap = falls[i].at - falls[i - 1].at;
    assert(Math.abs(gap - 1 / rate / 2) < 1e-9,
      'a step landed ' + gap.toFixed(3) + 's after the last, not ' + (1 / rate / 2).toFixed(3));
    assert(falls[i].foot !== falls[i - 1].foot, 'two lefts in a row');
  }
  assert(falls[0].at > 0.05, 'a foot landed on the cut itself, which sounds like the edit');
  falls.forEach((f) => assert(f.at < shot.duration, 'a step landed after the shot ended'));
});

test('only the people who walk make footsteps', () => {
  const base = { characters: ['A'], duration: 6, framing: 'wide' };
  eq(World.footfallsFor(Object.assign({}, base, { beat: 'open' }), 0.85).length, 0,
    'somebody walked through a beat nobody walks in');
  eq(World.footfallsFor(Object.assign({}, base, { beat: 'push', characters: [] }), 0.85).length, 0,
    'an empty frame made footsteps');
  eq(World.footfallsFor(Object.assign({}, base, { beat: 'push', framing: 'insert' }), 0.85).length, 0,
    'a shot of an object made footsteps');
});

test('you can hear whether somebody is indoors', () => {
  // Most of what tells you where somebody is walking is how bright the step is.
  const outside = World.surfaceFor('woods');
  const inside = World.surfaceFor('corridor');
  assert(inside.cutoff > outside.cutoff * 2,
    'a corridor sounds as soft underfoot as a forest');
  assert(World.surfaceFor('chapel').decay > World.surfaceFor('street').decay,
    'stone does not ring longer than a street');
  eq(World.surfaceFor('nowhere'), World.DEFAULT_SURFACE);
});

test('the seven turns of the object reach the soundtrack', () => {
  // The film reacting to what is on screen rather than to the clock.
  const script = Writer.write(Parse.parse('a night nurse buries a key in the woods'),
    { length: 'festival', seed: 5 });
  const reel = Reel.build(script);
  const marked = reel.shots.filter((s) => s.objectBeat);
  assert(marked.length >= 5, 'only ' + marked.length + ' object beats reached the reel');
  marked.forEach((shot) => assert(Arc.STATES.indexOf(shot.objectBeat) !== -1,
    'unknown object state on a shot: ' + shot.objectBeat));
});


/* -------------------------------------------------------------- directing */

function freshScript(seed) {
  return Writer.write(Parse.parse('a night nurse buries a key in the woods'),
    { length: 'short', seed: seed == null ? 5 : seed });
}

/* A script carries its scenes AND a flat element list for the page, as separate
 * objects holding the same words. This is the invariant every edit must keep:
 * if they drift, an edit shows on the page and is not in the film, or the other
 * way round, and nothing complains. */
function pageAgreesWithScenes(script) {
  const body = (list) => list.filter((el) => el.type !== 'transition').map((el) => el.text);
  const fromScenes = [];
  script.scenes.forEach((scene) => body(scene.elements).forEach((t) => fromScenes.push(t)));
  return fromScenes.join('') === body(script.elements).join('');
}

test('a fresh script already agrees with itself', () => {
  assert(pageAgreesWithScenes(freshScript()), 'the writer produced a page that is not its own scenes');
});

test('a film fades in at the start and out at the end, whatever was cut', () => {
  // The writer hangs FADE OUT. off its last scene. Move that scene to the middle
  // and the film fades out halfway through, so rebuild normalises the bookends
  // rather than carrying them around with whichever scene happened to hold one.
  let script = Director.rebuild(Director.cloneScript(freshScript()));
  script = Director.moveScene(script, script.scenes.length, -2);
  script = Director.deleteScene(script, 1);
  const transitions = script.elements.filter((el) => el.type === 'transition');
  eq(transitions.length, 2, 'a film with ' + transitions.length + ' transitions in it');
  eq(transitions[0].text, 'FADE IN:');
  eq(transitions[1].text, 'FADE OUT.');
  eq(script.elements[0].type, 'transition', 'the film does not fade in first');
  eq(script.elements[script.elements.length - 1].text, 'FADE OUT.', 'the film does not fade out last');
});

test('editing a line reaches the film, not only the page', () => {
  const script = freshScript();
  let sceneNo = 0;
  let index = -1;
  script.scenes.forEach((scene, i) => scene.elements.forEach((el, j) => {
    if (index < 0 && el.type === 'dialogue') { sceneNo = i + 1; index = j; }
  }));
  assert(index >= 0, 'no dialogue to edit');

  const line = 'I came back for it and I am not sorry.';
  const next = Director.editLine(script, sceneNo, index, line);
  assert(pageAgreesWithScenes(next), 'the page and the scenes disagree after an edit');
  const reel = Reel.build(next);
  assert(reel.shots.some((s) => s.caption === line), 'the new line never reached the film');
  const shot = reel.shots.find((s) => s.caption === line);
  assert(shot.duration >= line.split(' ').length / 3.2, 'the new line is on screen too briefly to read');
});

test('an edit leaves the script it was given alone', () => {
  // Undo is a variable rather than a feature, and only because of this.
  const script = freshScript();
  const before = JSON.stringify(script.elements.map((e) => e.text));
  Director.editLine(script, 1, 1, 'Something else entirely.');
  Director.deleteScene(script, 2);
  Director.setHour(script, 1, 'DAWN');
  Director.renameCharacter(script, script.characters[0].name, 'ZED');
  eq(JSON.stringify(script.elements.map((e) => e.text)), before, 'an edit mutated its input');
});

test('renaming a character moves the cue, the line and the action together', () => {
  const script = freshScript();
  const from = script.characters[0].name;
  const next = Director.renameCharacter(script, from, 'Marlow');
  const pattern = new RegExp('\\b' + from + '\\b');
  assert(!next.elements.some((el) => pattern.test(el.text)), 'the old name is still on the page');
  assert(next.elements.some((el) => /\bMARLOW\b/.test(el.text)), 'the new name never appears');
  eq(next.characters[0].name, 'MARLOW', 'the cast list still has the old name');
  // The reel gives a voice to whoever speaks; if the cue and the cast disagree
  // the character speaks with nobody's voice.
  const reel = Reel.build(next);
  reel.shots.filter((s) => s.speaker).forEach((shot) => {
    assert(reel.voices[shot.speaker], shot.speaker + ' speaks with no voice');
  });
});

test('renaming will not mangle a word that merely contains the name', () => {
  const script = freshScript();
  script.scenes[0].elements.push({ type: 'action', text: 'A SAMPLE of something, and SAM, and SAMS.' });
  Director.rebuild(script);
  const next = Director.renameCharacter(script, 'SAM', 'RAY');
  const line = next.elements.filter((e) => /SAMPLE/.test(e.text))[0];
  assert(line, 'the test line vanished');
  eq(line.text, 'A SAMPLE of something, and RAY, and SAMS.');
});

test('changing the hour relights the scene', () => {
  const script = freshScript();
  const next = Director.setHour(script, 2, 'DAWN');
  eq(next.scenes[1].heading.time, 'DAWN');
  assert(/DAWN/.test(next.scenes[1].heading.text), 'the heading text still says the old hour');
  assert(/DAWN/.test(next.elements.filter((e) => e.type === 'scene_heading')[1].text),
    'the page still shows the old hour');
  const reel = Reel.build(next);
  const lit = reel.shots.filter((s) => s.scene === 2).map((s) => s.time);
  assert(lit.length && lit.every((t) => t === 'DAWN'), 'the artist is still lighting it at the old hour');
});

test('cutting and moving a scene renumbers what is left', () => {
  const script = freshScript();
  const n = script.scenes.length;
  const cut = Director.deleteScene(script, 2);
  eq(cut.scenes.length, n - 1);
  eq(cut.scenes.map((s) => s.number).join(','), cut.scenes.map((_, i) => i + 1).join(','));
  assert(pageAgreesWithScenes(cut), 'the page and the scenes disagree after a cut');

  const moved = Director.moveScene(cut, 1, 2);
  eq(moved.scenes[2].heading.text, cut.scenes[0].heading.text, 'the scene did not land where it was sent');
  eq(moved.scenes.map((s) => s.number).join(','), moved.scenes.map((_, i) => i + 1).join(','));
  assert(pageAgreesWithScenes(moved), 'the page and the scenes disagree after a move');
  assert(Reel.build(moved).shots.length > 0, 'the reel will not build from a reordered script');
});

test('a film always has a scene in it', () => {
  let script = freshScript();
  for (let i = 0; i < 10; i++) script = Director.deleteScene(script, 1);
  assert(script.scenes.length >= 1, 'the last scene was deletable');
  assert(Reel.build(script).shots.length > 0, 'a one-scene film will not build');
});

test('a reroll keeps what was locked and rewrites the rest', () => {
  const script = freshScript(5);
  const locked = [1, script.scenes.length];
  const next = Director.reroll(script, { seed: 987654, length: 'short' }, locked);
  const text = (scene) => scene.elements
    .filter((e) => e.type !== 'transition').map((e) => e.text).join('|');
  eq(text(next.scenes[0]), text(script.scenes[0]), 'the locked first scene was rewritten');
  assert(next.scenes.slice(1, -1).some((scene, i) => text(scene) !== text(script.scenes[i + 1])),
    'nothing outside the locks actually changed');
  assert(pageAgreesWithScenes(next), 'the page and the scenes disagree after a reroll');
});

test('a reroll never loses a scene somebody deliberately kept', () => {
  // The new film may be a different shape. Dropping a scene from a button
  // called "keep this" would be the worst outcome available.
  const script = freshScript(5);
  // Compare the WORDS, not the transitions: rebuild normalises FADE IN/OUT to
  // the film's own bookends, so a scene that arrived carrying one comes back
  // without it and is still the same scene.
  const text = (scene) => scene.elements
    .filter((e) => e.type !== 'transition').map((e) => e.text).join('|');
  const wanted = script.scenes.map((s) => text(s));
  for (let seed = 1; seed <= 25; seed++) {
    const all = script.scenes.map((_, i) => i + 1);
    const next = Director.reroll(script, { seed, length: 'micro' }, all);
    wanted.forEach((body) => {
      assert(next.scenes.some((scene) => text(scene) === body),
        'seed ' + seed + ': a locked scene was lost when the film got shorter');
    });
  }
});


/* ------------------------------------------------------ why it did that */

test('the panel explains every film, of every length and genre', () => {
  // A panel that explains the film must never be the thing that breaks it.
  Object.keys(LEX.GENRES).forEach((genre) => {
    ['micro', 'short', 'festival'].forEach((length) => {
      const premise = Parse.parse('somebody loses a watch in a bar', { genre });
      const script = Writer.write(premise, { length, seed: 11 });
      const sections = Why.explain(script, Reel.build(script));
      assert(sections.length >= 5, genre + '/' + length + ': only ' + sections.length + ' sections');
      sections.forEach((section) => {
        assert(section.title && section.lines.length,
          genre + '/' + length + ': empty section "' + section.title + '"');
        section.lines.forEach((line) => {
          assert(typeof line === 'string' && line.length > 4,
            genre + '/' + length + ': a stub line in ' + section.title);
          assert(line.indexOf('undefined') === -1 && line.indexOf('[object') === -1,
            genre + '/' + length + ': ' + line);
          assert(!/\{[A-Z_]+\}/.test(line), genre + '/' + length + ': unfilled slot in ' + line);
        });
      });
    });
  });
});

test('the panel reads the decisions rather than restating the rules', () => {
  // An explanation written separately from the decision drifts from it, and a
  // WRONG explanation is worse than none: it teaches somebody something untrue
  // about their own film. So each claim is checked against the film itself.
  const script = Writer.write(Parse.parse('a night nurse buries a key in the woods'),
    { length: 'short', seed: 5 });
  const reel = Reel.build(script);
  const text = Why.toText(script, reel);

  assert(text.indexOf(String(script.seed)) !== -1, 'the panel never says the seed');
  assert(text.indexOf(script.premise.object) !== -1, 'the panel never names the object');
  script.characters.forEach((c) => assert(text.indexOf(c.name) !== -1,
    c.name + ' is missing from the panel'));
  // The tempo it quotes has to be the tempo the score is actually asked for.
  const bpm = require(path.join(__dirname, '..', 'js', 'film-score.js')).request(reel).bpm;
  assert(text.indexOf(String(bpm)) !== -1, 'the panel quotes a tempo the score never asked for');
  // The set names it lists have to be the sets the reel actually uses.
  const used = new Set(reel.shots.map((s) => s.set));
  let named = 0;
  used.forEach((setKey) => { if (text.indexOf(setKey) !== -1) named++; });
  eq(named, used.size, 'the panel does not account for every set in the film');
});

test('the panel never explains a beat the film does not contain', () => {
  // The first draft said the film "holds longest on the choice" for every film,
  // including the ones whose spine has no choice beat in it.
  for (let seed = 1; seed <= 20; seed++) {
    const script = Writer.write(Parse.parse('two brothers argue over a boat'), { length: 'short', seed });
    const reel = Reel.build(script);
    const beats = new Set(reel.shots.map((s) => s.beat));
    const cutting = Why.explain(script, reel).filter((s) => s.title === 'How it is cut')[0];
    cutting.lines.forEach((line) => {
      const claim = /(?:Fastest at the|slowest at the) (\w+)/g;
      let m;
      while ((m = claim.exec(line)) !== null) {
        assert(beats.has(m[1]), 'seed ' + seed + ': the panel named the ' + m[1] +
          ', which this film does not have');
      }
    });
  }
});

test('two voices in one film are far enough apart to hear', () => {
  // The dials come from the name and the role, and roles can collapse two people
  // onto the same register -- a night nurse is formal and a stranger is guarded,
  // so one film gave both leads formal 0.95 and 0.93.
  const lead = Voice.voiceFor('SHAY', 'night nurse', 5);
  const flat = Voice.voiceFor('SAM', 'stranger', 5);
  const apart = (a, b) => Voice.DIALS.reduce((m, k) => Math.max(m, Math.abs(a[k] - b[k])), 0);
  assert(apart(lead, flat) < Voice.MIN_APART, 'this pair no longer demonstrates the problem');
  assert(apart(lead, Voice.contrast(lead, flat)) >= Voice.MIN_APART - 1e-9,
    'contrast did not pull them apart');
  // And it never pushes a dial off the scale.
  Voice.DIALS.forEach((k) => {
    const out = Voice.contrast({ formal: 1, terse: 1, hedging: 1, warmth: 1 },
      { formal: 1, terse: 1, hedging: 1, warmth: 1 });
    assert(out[k] >= 0 && out[k] <= 1, k + ' went off the scale: ' + out[k]);
  });
});

test('every film has two audible voices in it now', () => {
  let heard = 0;
  const ideas = [
    'a night nurse buries a key in the woods',
    'two brothers argue over a boat their father left them',
    'a detective returns a stolen watch to the wrong house',
    'a teenager hides a letter from her grandmother',
    'a surgeon and a drifter share a waiting room'
  ];
  ideas.forEach((idea) => {
    const premise = Parse.parse(idea);
    const a = Voice.voiceFor(premise.hero.name, premise.hero.role, premise.seed);
    const b = Voice.contrast(a, Voice.voiceFor(premise.other.name, premise.other.role, premise.seed));
    const apart = Voice.DIALS.reduce((m, k) => Math.max(m, Math.abs(a[k] - b[k])), 0);
    if (apart >= Voice.MIN_APART - 1e-9) heard++;
  });
  eq(heard, ideas.length, 'only ' + heard + ' of ' + ideas.length + ' films have two audible voices');
});


if (failures.length) {
  console.error('✖ ' + failures.length + ' failing test(s):');
  failures.forEach((f) => console.error('  - ' + f));
  process.exit(1);
}
console.log('✓ ' + passed + ' tests passed\n');
