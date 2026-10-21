#!/usr/bin/env node
/*
 * CODA PICS — a picture for a headline.
 *
 *   node coda-pics/tests/headline.test.js
 *
 * The painter will make a picture out of any sentence at all, which is exactly
 * wrong beside a news story: a picture next to a headline is a claim about
 * that headline. Handed "Teachers vote to accept pay offer" the painter
 * recognises nothing, rolls the dice and returns a bear in a canyon. The whole
 * point of the adapter under test is knowing when to say no, so most of what
 * follows is about refusing rather than about drawing.
 *
 * No network and no drawing — words in, a prompt out.
 */
'use strict';

var path = require('path');
var H = require(path.join(__dirname, '..', 'js', 'headline.js'));
var PROMPT = require(path.join(__dirname, '..', 'js', 'prompt.js'));

var failures = 0;
var checks = 0;
function check(cond, msg) {
  checks++;
  if (!cond) { failures++; console.log('  FAIL  ' + msg); }
}
function pass(msg) { console.log('  ok    ' + msg); }
function section(name) { console.log('\n' + name); }

/* Real front-page shapes: some depictable, most not. */
var ABSTRACT = [
  'Shares fall as central bank holds rates',
  'Teachers vote to accept pay offer',
  'Inflation eases for a third straight month',
  'Council leader resigns after inquiry',
  'Trade talks resume in the new year',
  'Hospital waiting times improve slightly'
];
var CONCRETE = [
  'Council approves new bridge over the river',
  'Wolves return to the national park after 40 years',
  'Lighthouse restored after a decade of neglect',
  'Whale spotted in the harbour for the first time',
  'Snow closes schools across the north',
  'Cave divers map new passage',
  'Rescue team reaches climbers on the mountain at dawn',
  'Eagle pair nest on the cliffs again',
  'Flooding closes roads in the valley',
  'New telescope captures distant galaxy'
];

/* ------------------------------------------------------- knowing to say no */
section('Knowing when there is no picture');

var wrongly = ABSTRACT.filter(function (h) { return H.pictureFor(h); });
check(wrongly.length === 0,
  'a story with nothing depictable in it gets no picture at all' +
  (wrongly.length ? ' — but ' + wrongly[0] + ' got "' +
    H.pictureFor(wrongly[0]).prompt + '"' : ''));

var missed = CONCRETE.filter(function (h) { return !H.pictureFor(h); });
check(missed.length === 0,
  'while every story that names something gets one' +
  (missed.length ? ' — but not ' + missed[0] : ''));

check(!H.pictureFor('') && !H.pictureFor(null) && !H.pictureFor('   '),
  'and an empty headline is not a picture either');
pass('politics, money and the rest of the front page are left alone');

/* ------------------------------------------------------------- groundedness */
section('Only what the story said');

/*
 * The claim that matters. Everything the prompt names has to be a thing the
 * headline mentioned — checked by asking the painter what it would draw and
 * looking for one of that thing's own words back in the headline.
 */
var LEX = require(path.join(__dirname, '..', 'js', 'lexicon.js'));
/*
 * The spec the painter hands back carries a subject with an id and a label but
 * no vocabulary — the word list lives in the lexicon. Asking the spec's own
 * object which words mean it therefore always answered "none", and the first
 * version of the check below failed every headline it was given, including the
 * ones that were perfectly grounded.
 */
function entryFor(list, id) {
  for (var i = 0; i < list.length; i++) if (list[i].id === id) return list[i];
  return null;
}
function saidIn(entry, headline) {
  if (!entry) return false;
  var line = headline.toLowerCase();
  var said = H.words(headline);
  return (entry.words || []).some(function (w) {
    return w.indexOf(' ') >= 0 ? line.indexOf(w) >= 0 : said.indexOf(w) >= 0;
  });
}
var ungrounded = [];
CONCRETE.forEach(function (h) {
  var got = H.pictureFor(h);
  var back = PROMPT.parse(got.prompt, { seed: got.seed, style: 'auto' });
  if (back.scene && !saidIn(entryFor(LEX.SCENES, back.scene.id), h)) {
    /* A subject brings its own home with it — a wolf implies a forest — so a
     * scene is only wrong if the headline named a different one. */
    if (got.scene) ungrounded.push(h + ' → scene ' + back.scene.id);
  }
  if (back.subject && got.subject &&
      !saidIn(entryFor(LEX.SUBJECTS, back.subject.id), h)) {
    ungrounded.push(h + ' → subject ' + back.subject.id);
  }
});
check(ungrounded.length === 0,
  'nothing in the picture that was not in the headline' +
  (ungrounded.length ? ' — ' + ungrounded[0] : ''));

/* Where the painter fills in a subject of its own, that is said out loud
 * rather than passed off as the story's. */
var storm = H.pictureFor('Storm warning issued for the coast this weekend');
check(storm && storm.invented === true,
  'and where the painter adds a subject of its own, the caller is told' +
  (storm ? ' (it drew a ' + storm.paints.subject + ')' : ''));
var wolf = H.pictureFor('Wolves return to the national park after 40 years');
check(wolf && wolf.invented === false,
  'while a subject the headline named is not flagged as invented');
pass('a picture beside a story claims nothing the story did not');

/* ---------------------------------------------------------- the round trip */
section('The prompt means what the headline meant');

var drifted = CONCRETE.map(function (h) { return H.pictureFor(h); })
  .filter(function (g) { return !g.faithful; });
check(drifted.length === 0,
  'every prompt reads back as the thing it was built from' +
  (drifted.length ? ' — "' + drifted[0].prompt + '"' : ''));

/*
 * The case that forced the check. "deep space" is the lexicon's own label for
 * space and contains "deep", which is one of the ocean's words — so the
 * well-worded prompt came back parsed as the sea, and a story about a
 * telescope was illustrated with water.
 */
var sky = H.pictureFor('New telescope captures distant galaxy');
check(sky && sky.paints.scene === 'space',
  'including the one that reads as the sea if nobody checks (' +
  (sky ? sky.paints.scene : 'nothing') + ')');
pass('good wording first, blunt wording where good wording lies');

/* ------------------------------------------------------- one story, one picture */
section('One story, one picture');

var twice = CONCRETE.map(function (h) { return H.pictureFor(h).seed; });
var again = CONCRETE.map(function (h) { return H.pictureFor(h).seed; });
check(twice.every(function (v, i) { return v === again[i]; }),
  'the same headline always gets the same seed, so a story does not change ' +
  'its picture when the page is reloaded');
var distinct = twice.filter(function (v, i) { return twice.indexOf(v) === i; });
check(distinct.length === twice.length,
  'and different headlines get different ones (' + distinct.length + ' of ' +
  twice.length + ')');
check(H.pictureFor(CONCRETE[0]).seed !== H.pictureFor(CONCRETE[0] + '!').seed,
  'down to a changed character, because an edited headline is a new story');
pass('a picture is pinned to its headline');

/* --------------------------------------------------------- not editorialising */
section('The picture does not editorialise');

var quiet = CONCRETE.filter(function (h) { return !/dawn|dusk|night|noon|morning|evening/i.test(h); })
  .map(function (h) { return H.pictureFor(h); });
var afterDark = quiet.filter(function (g) {
  return /at night|at dusk|at dawn|at sunset/.test(g.prompt);
});
check(afterDark.length === 0,
  'a story that does not mention an hour is never set at night or at dusk' +
  (afterDark.length ? ' — "' + afterDark[0].prompt + '"' : ''));

var dawn = H.pictureFor('Rescue team reaches climbers on the mountain at dawn');
check(dawn && /at dawn/.test(dawn.prompt),
  'while a story that does name one keeps it');

/* `pictureFor` returns no copy of the headline — only `pickable` attaches one
 * — so the first version of this read an undefined field and quietly tested
 * the string "undefined" against the pattern. The headline has to be carried
 * alongside. */
var weathered = CONCRETE
  .map(function (h) { return { h: h, g: H.pictureFor(h) }; })
  .filter(function (r) { return /in a storm|in the rain|in fog|in falling snow/.test(r.g.prompt); })
  .filter(function (r) { return !/storm|rain|fog|snow|flood/i.test(r.h); });
check(weathered.length === 0,
  'and weather is only ever in the picture because the story put it there' +
  (weathered.length ? ' — "' + weathered[0].h + '" got "' + weathered[0].g.prompt + '"' : ''));

var doubled = CONCRETE.map(function (h) { return H.pictureFor(h).prompt; })
  .filter(function (p) { return /\b(in|on|at|under) (in|on|at|under)\b/.test(p); });
check(doubled.length === 0,
  'with no doubled preposition from a label that brought its own' +
  (doubled.length ? ' — "' + doubled[0] + '"' : ''));
pass('the hour and the weather come from the story or not at all');

/* ------------------------------------------------------------ no sky indoors */
section('Nowhere has an hour it cannot have');

var cave = H.pictureFor('Cave divers map new passage');
var space = H.pictureFor('New telescope captures distant galaxy');
check(cave && !/daylight|at /.test(cave.prompt),
  'a cavern is not given an hour (" ' + (cave ? cave.prompt : '') + '")');
check(space && !/daylight|at /.test(space.prompt),
  'and neither is deep space');
var meadow = H.pictureFor('Wolves return to the national park after 40 years');
check(meadow && /daylight/.test(meadow.prompt),
  'while somewhere with a sky over it gets daylight by default');
pass('an hour only where an hour exists');

/* ------------------------------------------------------------------ a list */
section('A page of headlines');

var all = ABSTRACT.concat(CONCRETE);
var picked = H.pickable(all);
check(picked.length === CONCRETE.length,
  'given a page of stories it returns only the ones worth a picture (' +
  picked.length + ' of ' + all.length + ')');
check(picked.every(function (p) { return p.headline && p.prompt && p.seed >= 0; }),
  'each one carrying its headline, its prompt and its seed');
check(H.pickable([]).length === 0 && H.pickable(null).length === 0,
  'and nothing at all is not an error');
pass('a newsroom can hand it the whole page');

console.log('\n' + (failures ? 'FAILED ' + failures + ' of ' + checks + ' checks'
  : 'All ' + checks + ' checks passed'));
process.exit(failures ? 1 : 0);
