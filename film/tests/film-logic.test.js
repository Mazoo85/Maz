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

/* ------------------------------------------------------------------ report */
console.log('');
if (failures.length) {
  console.error('✖ ' + failures.length + ' failing test(s):');
  failures.forEach((f) => console.error('  - ' + f));
  process.exit(1);
}
console.log('✓ ' + passed + ' tests passed\n');
