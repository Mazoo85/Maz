#!/usr/bin/env node
/*
 * CODA PICS — the tests.
 *
 *   node coda-pics/tests/coda-logic.test.js
 *
 * No browser, no dependencies, no network. Three things are checked, in the
 * order they can break:
 *
 *   1. The vocabulary hangs together — every word the lexicon knows leads to a
 *      routine that exists, and every style has a pass.
 *   2. Reading is total and repeatable — any text at all yields a complete
 *      scene, and the same text always yields the same one.
 *   3. Painting actually runs — every subject, every setting and every style
 *      is rendered against a recording canvas, so a typo in a rarely-chosen
 *      branch fails here rather than in front of someone typing "a crab".
 */
'use strict';

var path = require('path');
var LEX = require(path.join(__dirname, '..', 'js', 'lexicon.js'));
var PROMPT = require(path.join(__dirname, '..', 'js', 'prompt.js'));
var SUBJECTS = require(path.join(__dirname, '..', 'js', 'subjects.js'));
var PAINT = require(path.join(__dirname, '..', 'js', 'paint.js'));
var FINISH = require(path.join(__dirname, '..', 'js', 'finish.js'));
var FakeContext = require(path.join(__dirname, 'fake-canvas.js')).FakeContext;

var failures = 0;
var checks = 0;

function check(cond, msg) {
  checks++;
  if (!cond) {
    failures++;
    console.log('  FAIL  ' + msg);
  }
}

function section(name) { console.log('\n' + name); }
function pass(msg) { console.log('  ok    ' + msg); }

/* ------------------------------------------------------- 1. the vocabulary */
section('Vocabulary');

(function () {
  var ids = {};
  LEX.SUBJECTS.forEach(function (s) {
    check(!ids[s.id], 'duplicate subject id: ' + s.id);
    ids[s.id] = true;
    check(!!SUBJECTS.DRAW[s.draw], s.id + ' points at a drawing routine that does not exist: ' + s.draw);
    check(!!SUBJECTS.META[s.draw], s.draw + ' has no META entry, so the painter cannot place it');
    check(s.words.length > 0, s.id + ' has no words, so nobody can ask for it');
    check(!!s.label && !!s.scene, s.id + ' is missing a label or a home setting');
    var home = LEX.SCENES.filter(function (sc) { return sc.id === s.scene; });
    check(home.length === 1, s.id + ' lives in "' + s.scene + '", which is not a setting');
  });
  pass(LEX.SUBJECTS.length + ' subjects, each with a routine, a home and words');

  Object.keys(SUBJECTS.META).forEach(function (k) {
    check(!!SUBJECTS.DRAW[k], 'META describes ' + k + ' but there is no routine for it');
    var m = SUBJECTS.META[k];
    check(['ground', 'sky', 'water'].indexOf(m.anchor) >= 0, k + ' has an unknown anchor: ' + m.anchor);
    check(m.base > 0 && m.base < 1.2, k + ' has an unreasonable base size: ' + m.base);
  });
  Object.keys(SUBJECTS.DRAW).forEach(function (k) {
    check(!!SUBJECTS.META[k], 'routine ' + k + ' has no META, so it can never be placed');
  });
  pass(Object.keys(SUBJECTS.DRAW).length + ' drawing routines, each placeable');

  LEX.SCENES.forEach(function (s) {
    check(!!PAINT.GROUND[s.id], 'setting ' + s.id + ' has nothing to draw it');
    check(!!PAINT.SCENE_COLOUR[s.id], 'setting ' + s.id + ' has no colours');
    check(s.horizon > 0, s.id + ' has no horizon');
  });
  pass(LEX.SCENES.length + ' settings, each with ground and colours');

  LEX.STYLES.forEach(function (s) {
    check(!!FINISH.STYLE[s.id], 'style ' + s.id + ' has no finishing pass');
  });
  Object.keys(FINISH.STYLE).forEach(function (k) {
    check(LEX.STYLES.some(function (s) { return s.id === k; }),
      'finishing pass ' + k + ' is not a style anybody can ask for');
  });
  pass(LEX.STYLES.length + ' styles, each with a pass and a name');

  Object.keys(LEX.TIMES).forEach(function () {});
  LEX.TIMES.forEach(function (t) {
    check(!!PAINT.SKY[t.id], 'hour ' + t.id + ' has no sky');
  });
  pass(LEX.TIMES.length + ' hours, each with a sky');
})();

/* ------------------------------------------------------------- 2. reading */
section('Reading what people type');

(function () {
  var awkward = [
    '', '   ', '?!?!', '12345', 'aaaaaaaaaaaaaaaaaaaaaaaa',
    'a'.repeat(4000), '🐉🌋', 'DRAGON', 'the', 'null', 'undefined',
    'a red dragon over snowy mountains at sunset, neon',
    'lonely lighthouse in a storm, watercolour',
    'wolves howling at the moon'
  ];
  awkward.forEach(function (text) {
    var spec = PROMPT.parse(text, { seed: 1 });
    var name = JSON.stringify(text.slice(0, 28));
    check(!!spec.scene && !!PAINT.GROUND[spec.scene.id], name + ' gave no drawable setting');
    check(!!FINISH.STYLE[spec.style], name + ' gave an unknown style: ' + spec.style);
    check(!!PAINT.SKY[spec.time], name + ' gave an unknown hour: ' + spec.time);
    check(spec.mood >= 0 && spec.mood <= 1, name + ' gave a mood outside 0..1');
    check(!spec.subject || !!SUBJECTS.DRAW[spec.subject.draw], name + ' gave an undrawable subject');
    check(typeof PROMPT.describe(spec) === 'string' && PROMPT.describe(spec).length > 0,
      name + ' could not be described back');
  });
  pass(awkward.length + ' awkward prompts all produced a complete, drawable scene');

  for (var seed = 1; seed <= 5; seed++) {
    var a = PROMPT.parse('a fox in the snow', { seed: seed });
    var b = PROMPT.parse('a fox in the snow', { seed: seed });
    check(JSON.stringify(a) === JSON.stringify(b), 'seed ' + seed + ' was not repeatable');
  }
  pass('the same words and seed always give the same scene');

  var s1 = PROMPT.parse('a fox in the snow', { seed: 1 });
  var s2 = PROMPT.parse('a fox in the snow', { seed: 2 });
  check(JSON.stringify(s1) !== JSON.stringify(s2), 'a new seed changed nothing at all');
  pass('a new seed gives a different take');

  check(PROMPT.parse('a wolf, pixel art', { seed: 1 }).style === 'pixel', 'style word was ignored');
  check(PROMPT.parse('a wolf', { seed: 1, style: 'noir' }).style === 'noir', 'chosen style was ignored');
  check(PROMPT.parse('a red wolf', { seed: 1 }).palette.id === 'crimson', 'colour word was ignored');
  check(PROMPT.parse('a wolf at midnight', { seed: 1 }).time === 'night', 'hour word was ignored');
  check(PROMPT.parse('a whale', { seed: 1 }).scene.id === 'ocean', 'subject did not bring its own setting');
  check(PROMPT.parse('a giant wolf', { seed: 1 }).subject.scale > 1.4, 'a giant wolf was not giant');
  check(PROMPT.parse('a tiny wolf', { seed: 1 }).subject.scale < 0.8, 'a tiny wolf was not tiny');
  check(PROMPT.parse('three wolves', { seed: 1 }).subject.count === 3, 'three wolves were not three');
  check(PROMPT.parse('a wolf and a castle', { seed: 1 }).companion !== null, 'the second subject was dropped');
  pass('every kind of word a person types changes the picture it should');

  for (var i = 0; i < 40; i++) {
    var text = PROMPT.surprise(i);
    check(typeof text === 'string' && text.length > 3, 'surprise ' + i + ' produced nothing');
    var spec = PROMPT.parse(text, { seed: i });
    check(!!spec.subject, 'surprise ' + i + ' ("' + text + '") has nothing in it');
  }
  pass('40 surprise prompts all parse back into a picture');

  /* A palette name is picked at random and a tenth of them start with a vowel,
   * so this wrote "a orange fish" roughly once every forty prompts — often
   * enough that anyone pressing Surprise me a few times would see it. */
  var misarticled = [];
  for (var a = 0; a < 400; a++) {
    var line = PROMPT.surprise(a);
    if (/\ba [aeiou]/i.test(line)) misarticled.push(line);
  }
  check(misarticled.length === 0, 'wrong article in: ' + misarticled.slice(0, 3).join(' | '));
  pass('400 surprise prompts all read as English ("an orange fish", not "a orange fish")');
})();

/* ------------------------------------------------------------- 3. painting */
section('Painting');

function paintOnce(text, opts, w, h) {
  var ctx = new FakeContext(w || 96, h || 72);
  var spec = PROMPT.parse(text, opts || { seed: 1 });
  var P = PAINT.render(ctx, ctx.width, ctx.height, spec);
  FINISH.apply(ctx, ctx.width, ctx.height, spec, P);
  return ctx;
}

(function () {
  LEX.SCENES.forEach(function (scene) {
    var ctx;
    try {
      ctx = paintOnce(scene.words[0] + ' with a castle', { seed: 3 });
    } catch (e) {
      check(false, 'painting ' + scene.id + ' threw: ' + e.message);
      return;
    }
    check(ctx.calls > 20, 'painting ' + scene.id + ' barely drew anything (' + ctx.calls + ' calls)');
  });
  pass(LEX.SCENES.length + ' settings painted without throwing');

  LEX.SUBJECTS.forEach(function (subject) {
    var ctx;
    try {
      ctx = paintOnce(subject.words[0], { seed: 5 });
    } catch (e) {
      check(false, 'painting subject ' + subject.id + ' threw: ' + e.message);
      return;
    }
    check(ctx.calls > 20, 'subject ' + subject.id + ' barely drew anything');
  });
  pass(LEX.SUBJECTS.length + ' subjects painted without throwing');

  LEX.STYLES.forEach(function (style) {
    var ctx;
    try {
      ctx = paintOnce('a dragon over the mountains', { seed: 7, style: style.id });
    } catch (e) {
      check(false, 'style ' + style.id + ' threw: ' + e.message);
      return;
    }
    check(ctx.ops.getImageData > 0 || ctx.ops.fill > 0,
      'style ' + style.id + ' neither read pixels nor drew anything');
  });
  pass(LEX.STYLES.length + ' styles finished without throwing');

  LEX.TIMES.forEach(function (t) {
    LEX.WEATHER.forEach(function (wx) {
      try {
        paintOnce('a lighthouse ' + t.words[0] + ' ' + wx.words[0], { seed: 9 });
      } catch (e) {
        check(false, t.id + ' + ' + wx.id + ' threw: ' + e.message);
      }
    });
  });
  pass(LEX.TIMES.length * LEX.WEATHER.length + ' hour/weather combinations painted');

  [[64, 64], [320, 180], [180, 320], [97, 43]].forEach(function (size) {
    try {
      paintOnce('a whale at sunset', { seed: 11 }, size[0], size[1]);
    } catch (e) {
      check(false, size.join('x') + ' threw: ' + e.message);
    }
  });
  pass('square, wide, tall and odd canvas shapes all painted');

  /* The same words must paint the same picture twice — the promise the app
   * makes when it shows a seed next to the image. */
  var first = paintOnce('an ancient temple in the jungle at dawn', { seed: 4 });
  var again = paintOnce('an ancient temple in the jungle at dawn', { seed: 4 });
  check(first.calls === again.calls, 'the same prompt painted a different number of strokes');
  check(JSON.stringify(first.ops) === JSON.stringify(again.ops), 'the same prompt painted differently');
  pass('the same prompt paints exactly the same picture twice');
})();

console.log('\n' + (failures ? 'FAILED ' + failures + ' of ' + checks + ' checks'
  : 'All ' + checks + ' checks passed'));
process.exit(failures ? 1 : 0);
