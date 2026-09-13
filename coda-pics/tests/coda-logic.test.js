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
var PHOTO = require(path.join(__dirname, '..', 'js', 'photo.js'));
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
})();

/* ------------------------------------------------- 2b. reading a photograph
 * The analysis is pure arithmetic over pixels, so it can be checked against
 * images built right here — no browser, and no real photographs needed to
 * prove that reading one works.
 */
section('Reading a photograph');

function buildImage(w, h, paint) {
  var data = new Uint8ClampedArray(w * h * 4);
  for (var y = 0; y < h; y++) {
    for (var x = 0; x < w; x++) {
      var rgb = paint(x / w, y / h);
      var i = (y * w + x) * 4;
      data[i] = rgb[0]; data[i + 1] = rgb[1]; data[i + 2] = rgb[2]; data[i + 3] = 255;
    }
  }
  return { data: data, width: w, height: h };
}

(function () {
  var W = 240, H = 160;

  /* A landscape: blue sky, a bright sun to the right, dark land below 60%. */
  var landscape = buildImage(W, H, function (u, v) {
    var dx = u - 0.72, dy = v - 0.20;
    if (Math.sqrt(dx * dx + dy * dy) < 0.08) return [255, 246, 210];
    if (v < 0.60) return [60 + v * 60, 120 + v * 80, 200 + v * 30];
    return [28, 34, 26];
  });
  var a = PHOTO.analyse(landscape, W, H);

  check(Math.abs(a.skyline.mean - 0.60) < 0.05,
    'the horizon is found where it is (' + a.skyline.mean.toFixed(2) + ', expected 0.60)');
  check(a.skyline.confidence > 0.6,
    'and a clear horizon is reported as clear (' + a.skyline.confidence.toFixed(2) + ')');
  check(a.skyline.line.length >= 32, 'the horizon is a line, not one number');
  check(Math.abs(a.light.x - 0.72) < 0.12 && Math.abs(a.light.y - 0.20) < 0.14,
    'the light is found where the bright part is (' +
    a.light.x.toFixed(2) + ',' + a.light.y.toFixed(2) + ')');
  check(a.palette.sky.top[0] > 180 && a.palette.sky.top[0] < 250,
    'the sky colour is the sky colour (hue ' + Math.round(a.palette.sky.top[0]) + ', expected blue)');
  check(a.palette.scene.ink[2] < 30,
    'the darkest tenth is dark enough to be a silhouette (l ' +
    Math.round(a.palette.scene.ink[2]) + ')');
  check(a.colours.length >= 2, 'it reports the colours the photo is made of');
  pass('a landscape gives up its horizon, its light and its colours');

  /* A flat wall: no horizon anywhere. Saying so is the point — inventing a
   * ridge from a photograph that has none looks worse than not trying. */
  var flat = buildImage(W, H, function () { return [128, 120, 118]; });
  var f = PHOTO.analyse(flat, W, H);
  check(f.skyline.confidence < 0.35,
    'a photo with no horizon admits it (' + f.skyline.confidence.toFixed(2) + ')');

  /* Noise: also no horizon, and no crash. */
  var noise = buildImage(W, H, function (u, v) {
    var n = ((u * 7919 + v * 104729) * 1000) % 255;
    return [n, (n * 3) % 255, (n * 7) % 255];
  });
  var n2 = PHOTO.analyse(noise, W, H);
  check(n2.skyline.confidence < 0.6, 'and so does noise');
  check(n2.palette && n2.palette.sky && n2.palette.scene, 'noise still yields a usable palette');
  pass('a photo with nothing to read says so instead of inventing one');

  /* The painter accepts the analysis: a scene painted with a photo's colours
   * and horizon must still paint. */
  var spec = PROMPT.parse('a dragon over the mountains', { seed: 4 });
  spec.photo = {
    use: { colours: true, skyline: true, backdrop: false },
    palette: a.palette, skyline: a.skyline, light: a.light
  };
  var ctx = new FakeContext(96, 72);
  try {
    var P = PAINT.render(ctx, 96, 72, spec);
    FINISH.apply(ctx, 96, 72, spec, P);
    check(ctx.calls > 20, 'a photo-coloured scene really paints');
  } catch (e) {
    check(false, 'painting with a photo threw: ' + e.message);
  }

  /* --- mixing several photographs into one palette --- */
  var warm = PHOTO.analyse(buildImage(W, H, function (u, v) {
    return v < 0.6 ? [230, 120, 90] : [60, 30, 30];
  }), W, H);
  var cool = PHOTO.analyse(buildImage(W, H, function (u, v) {
    return v < 0.6 ? [80, 140, 230] : [20, 40, 60];
  }), W, H);
  var mixed = PHOTO.mix([warm, cool]);
  check(!!mixed && !!mixed.sky && !!mixed.scene, 'two photos mix into one palette');
  var warmL = warm.palette.sky.mid[2], coolL = cool.palette.sky.mid[2];
  check(mixed.sky.mid[2] >= Math.min(warmL, coolL) - 1 &&
        mixed.sky.mid[2] <= Math.max(warmL, coolL) + 1,
    'the mixture sits between the photos it came from');
  check(PHOTO.mix([]) === null, 'and mixing nothing gives nothing');

  /* Hue is a circle: 350 and 10 average to red, not to its opposite. */
  var wrapped = PHOTO.averageHsl([[350, 80, 50], [10, 80, 50]])[0] % 360;
  check(wrapped < 12 || wrapped > 348,
    'hues average the short way round the circle (' + wrapped.toFixed(1) + ', not 180)');
  pass('a mixture of photos is a palette of its own');

  var ridge = PAINT.photoRidge(spec, 200, 120, 70, 30);
  check(!!ridge && ridge.length >= 32, 'the photo horizon becomes a ridge the painter can draw');
  var bare = PROMPT.parse('a dragon', { seed: 4 });
  check(PAINT.photoRidge(bare, 200, 120, 70, 30) === null,
    'and no photo means no photo ridge');
  pass('the painter takes what the photograph gave it');
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

/* ------------------------------------------------------------------ the look
 * The claim this makes to a person is strong — "every photo you ever give it
 * makes it better, and none of them is ever dropped" — so it is worth proving
 * rather than asserting. A running fold has to behave like a real average: one
 * colour repeated must come back unchanged, the first photo of a thousand must
 * still be in the answer, and the storage must not grow.
 */
(function theLook() {
  console.log('\nWhat it remembers');

  function pal(h, l) {
    return {
      sky: { top: [h, 50, l], mid: [h, 50, l], low: [h, 50, l],
             light: [h, 40, 80], haze: [h, 30, 60], lightY: 0.3 },
      scene: { ink: [h, 40, 20], land: [h, 45, 40], far: [h, 35, 50], sea: [h, 55, 45] }
    };
  }
  function fold(list) {
    var look = null, n = 0;
    list.forEach(function (p) { look = PHOTO.fold(look, n, p); n++; });
    return { palette: look, count: n };
  }

  var same = fold(new Array(200).join(',').split(',').map(function () { return pal(200, 60); }));
  check(Math.abs(same.palette.sky.mid[0] - 200) < 0.01 &&
        Math.abs(same.palette.sky.mid[2] - 60) < 0.01,
    'one colour folded two hundred times comes back as itself');
  check(same.count === 200, 'and it counted every one of them');

  /* Lightness is a plain mean, so this is exact and can be checked against
   * arithmetic rather than against itself. */
  var lights = [20, 40, 60, 80];
  var mean = fold(lights.map(function (l) { return pal(200, l); }));
  check(Math.abs(mean.palette.sky.mid[2] - 50) < 0.01,
    'four lightnesses fold to their true average, not to the last one');

  /* The point of the whole feature: an early photo must still be pulling at
   * the answer after hundreds more, or "nothing is forgotten" is a lie. */
  var withRed = fold([pal(0, 50)].concat(
    new Array(300).join(',').split(',').map(function () { return pal(200, 50); })));
  var withoutRed = fold(
    new Array(300).join(',').split(',').map(function () { return pal(200, 50); }));
  check(Math.abs(withRed.palette.sky.mid[0] - withoutRed.palette.sky.mid[0]) > 0.05,
    'a photo added first is still in the answer three hundred photos later');

  /* And it must not drift off to the newest thing: 300 blues plus one red is
   * still blue. A memory that the last photo could capture would be no memory. */
  check(Math.abs(withRed.palette.sky.mid[0] - 200) < 12,
    'while one odd photo among hundreds cannot hijack it');

  /* Constant cost is what makes "every photo, forever" safe to promise. Counted
   * as stored numbers rather than as characters: the same twenty numbers
   * printed to different precisions are the same amount remembered. */
  function numbersIn(v) {
    if (typeof v === 'number') return 1;
    if (!v || typeof v !== 'object') return 0;
    return Object.keys(v).reduce(function (t, k) { return t + numbersIn(v[k]); }, 0);
  }
  var few = numbersIn(fold(lights.map(function (l) { return pal(120, l); })).palette);
  var many = numbersIn(fold(
    new Array(500).join(',').split(',').map(function (_, i) { return pal(i % 360, 50); })).palette);
  check(few === many && few > 0,
    'five hundred photos cost the same to remember as four (' + many + ' numbers either way)');

  check(PHOTO.fold(null, 0, pal(10, 50)) !== null, 'the very first photo becomes the look');
  check(PHOTO.fold(pal(10, 50), 3, null) !== null, 'and nothing is lost to a photo that failed to read');
  pass('every photo folds in, none is dropped, and the cost never grows');
})();

console.log('\n' + (failures ? 'FAILED ' + failures + ' of ' + checks + ' checks'
  : 'All ' + checks + ' checks passed'));
process.exit(failures ? 1 : 0);
