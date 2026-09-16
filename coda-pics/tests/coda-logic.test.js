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

/* ------------------------------------------------------ changing one part
 * "A different sky, everything else as it was" is a promise about seeds, and
 * seeds are exactly comparable — so it is tested here rather than by looking at
 * pixels. On a canvas the finishing passes read the whole picture, so a change
 * anywhere bleeds a little everywhere and no band is ever perfectly still; that
 * makes pixels the wrong place to ask whether a part was held.
 */
(function onePart() {
  console.log('\nChanging one part');

  var words = 'a castle in the mountains at dusk';
  var was = PROMPT.parse(words, { seed: 111 });

  function rollOnly(part) {
    var hold = {};
    ['sky', 'land', 'subject'].forEach(function (k) { if (k !== part) hold[k] = true; });
    return PROMPT.parse(words, { seed: 999, locked: PROMPT.holdLocks(was.seed, hold) });
  }
  function draws(spec, salt) {
    var r = PROMPT.rng(spec, salt);
    return [r(), r(), r(), r()].join(',');
  }
  var SALTS = ['sky', 'light', 'cloud', 'weather', 'scene', 'ground', 'fore', 'subject'];
  function changed(spec) {
    return SALTS.filter(function (s) { return draws(spec, s) !== draws(was, s); });
  }

  check(changed(rollOnly('sky')).sort().join(',') === 'cloud,light,sky,weather',
    'asking for a different sky rolls the sky, its light, its cloud and its weather — ' +
    'and those only (' + changed(rollOnly('sky')).join(', ') + ')');
  check(changed(rollOnly('land')).sort().join(',') === 'fore,ground',
    'asking for different land rolls the ground and what is in front of it, and nothing else ' +
    '(' + changed(rollOnly('land')).join(', ') + ')');
  check(changed(rollOnly('subject')).join(',') === 'subject',
    'asking for a different subject rolls the subject alone ' +
    '(' + changed(rollOnly('subject')).join(', ') + ')');

  /* The horizon is the one both halves share. It used to sit under the land
   * alone, which made "keep the sky" a promise the app could not keep: new
   * ground puts the horizon somewhere new, and the sky is painted to the
   * horizon, so the kept sky was repainted anyway. */
  check(changed(rollOnly('land')).indexOf('scene') < 0,
    'the horizon holds when only the land is asked to change');
  check(changed(rollOnly('sky')).indexOf('scene') < 0,
    'and when only the sky is asked to change');

  var everything = PROMPT.parse(words, { seed: 999 });
  check(changed(everything).length === SALTS.length,
    'while a plain re-roll moves every part of the picture (' + changed(everything).length +
    ' of ' + SALTS.length + ')');
  pass('each part can be rolled on its own, and holds when it is not asked for');
})();

/* --------------------------------------------------------- every beast its own
 * Fifteen four-legged animals share one routine, which is right — they share a
 * skeleton. What was wrong is that four of them shared another animal's
 * *proportions*: a sheep was drawn as a rabbit, a cow and an elephant as a
 * bear, a camel as a horse, with only the ears and tail to tell them apart.
 * This is the check that stops that happening again by accident, and it
 * compares what is actually drawn rather than what the table says.
 */
(function everyBeast() {
  console.log('\nEvery animal its own');

  var LEX = require(path.join(__dirname, '..', 'js', 'lexicon.js'));
  var beasts = LEX.SUBJECTS.filter(function (s) { return s.draw === 'quadruped'; });

  var byForm = {};
  beasts.forEach(function (s) { byForm[s.form] = (byForm[s.form] || 0).valueOf() + 1; });
  var shared = Object.keys(byForm).filter(function (k) { return byForm[k] > 1; });
  check(shared.length === 0,
    beasts.length + ' four-legged animals, ' + Object.keys(byForm).length +
    ' bodies, none borrowed' + (shared.length ? ' — but ' + shared.join(', ') + ' is shared' : ''));

  /* Drawn, not declared: two forms could differ in the table and still put the
   * same marks on the canvas. */
  /* FakeContext counts how many times each operation was used, which cannot
   * tell two animals apart when both are made of the same kinds of stroke. So
   * this records the calls *and their numbers* — where every line went, at what
   * size — which is the drawing itself. */
  function tracer(w, h) {
    var inner = new FakeContext(w, h);
    var trace = [];
    var proxy = { trace: trace };
    for (var key in inner) {
      (function (k) {
        var v = inner[k];
        if (typeof v === 'function') {
          proxy[k] = function () {
            var args = Array.prototype.slice.call(arguments).map(function (a) {
              return typeof a === 'number' ? Math.round(a * 100) / 100 : String(a);
            });
            trace.push(k + '(' + args.join(',') + ')');
            return v.apply(inner, arguments);
          };
        } else {
          Object.defineProperty(proxy, k, {
            get: function () { return inner[k]; },
            set: function (val) { trace.push(k + '=' + String(val)); inner[k] = val; },
            enumerable: true, configurable: true
          });
        }
      })(key);
    }
    return proxy;
  }

  var drawn = {};
  beasts.forEach(function (s) {
    var ctx = tracer(400, 300);
    var spec = PROMPT.parse(s.words[0] + ' in a meadow', { seed: 7 });
    var P = PAINT.makePalette(spec);
    SUBJECTS.draw(ctx, { draw: 'quadruped', form: s.form },
      { x: 60, y: 60, w: 260, h: 180, depth: 0, anchor: 'ground' },
      P, PROMPT.rng(spec, 'subject'), spec);
    drawn[s.form] = ctx.trace.join('|');
  });

  var same = [];
  var forms = Object.keys(drawn);
  for (var i = 0; i < forms.length; i++) {
    for (var j = i + 1; j < forms.length; j++) {
      if (drawn[forms[i]] === drawn[forms[j]]) same.push(forms[i] + '/' + forms[j]);
    }
  }
  check(same.length === 0,
    'and no two of them put the same marks on the canvas' +
    (same.length ? ' — ' + same.join(', ') + ' draw identically' : ''));

  /* The features that make an animal that animal, rather than a shape with
   * different ears: a trunk, horns, a fleece, a second hump. */
  function marks(form) { return drawn[form] || ''; }
  check(marks('elephant').length > marks('bear').length,
    'an elephant is drawn with more than a bear is — it has a trunk and tusks to draw');
  check(marks('sheep') !== marks('rabbit'),
    'a sheep is no longer a rabbit');
  check(marks('cow') !== marks('bear'), 'a cow is no longer a bear');
  check(marks('camel') !== marks('horse'), 'a camel is no longer a horse');
  pass(beasts.length + ' four-legged animals, every one of them drawn its own way');
})();

/* ------------------------------------------------------------- the camera
 * Every picture used to be taken from the same spot with the same lens:
 * horizon a little past halfway, subject somewhere near the middle, always
 * about the same distance off. These are the words people already use for
 * wanting otherwise.
 */
(function camera() {
  console.log('\nWhere the picture is taken from');

  var said = {
    'a close up of a wolf': 'closeup',
    'a portrait of a wolf': 'closeup',
    'a wolf from below': 'low',
    'a towering castle': 'low',
    'a tiny cabin in a vast desert': 'wide',
    'a distant lighthouse': 'wide',
    'an aerial view of a city': 'aerial',
    'a bird eye view of a forest': 'aerial'
  };
  var wrong = [];
  Object.keys(said).forEach(function (text) {
    var got = PROMPT.parse(text, { seed: 5 }).shot;
    if (!got || got.id !== said[text]) wrong.push(text + ' -> ' + (got ? got.id : 'none'));
  });
  check(wrong.length === 0,
    Object.keys(said).length + ' ways of saying where to stand, all understood' +
    (wrong.length ? ' — except ' + wrong.join('; ') : ''));

  /* Saying it has to beat the dice, or asking for a close-up gets you one
   * about two times in three, which is worse than not offering it. */
  var always = [];
  for (var s2 = 1; s2 < 40; s2++) {
    var got2 = PROMPT.parse('a close up of a wolf', { seed: s2 }).shot;
    if (!got2 || got2.id !== 'closeup') always.push(s2);
  }
  check(always.length === 0,
    'and asking for one gets one every time, not most of the time (' +
    (39 - always.length) + ' of 39 seeds)');

  /* Unsaid, it is rolled — but weighted, because a gallery where every third
   * picture is an extreme close-up is its own kind of sameness. */
  var rolled = {};
  for (var s3 = 1; s3 < 300; s3++) {
    var g = PROMPT.parse('a wolf in a forest', { seed: s3 }).shot;
    var id = g ? g.id : 'ordinary';
    rolled[id] = (rolled[id] || 0) + 1;
  }
  check(rolled.ordinary > 130 && rolled.ordinary < 220,
    'left unsaid, most pictures are still framed the ordinary way (' +
    rolled.ordinary + ' of 299)');
  check(Object.keys(rolled).length >= 4,
    'but the rest are spread across the other framings (' +
    Object.keys(rolled).filter(function (k) { return k !== 'ordinary'; }).join(', ') + ')');

  /* The word is accounted for, or somebody typing "a close up of a wolf" is
   * told the app did not know what "close" meant. */
  check((PROMPT.parse('a close up of a wolf', { seed: 5 }).unknown || []).length === 0,
    'and the words that said it are not reported back as ones nobody knew');
  pass('five ways of framing a picture, said or rolled');
})();

/* ------------------------------------------------------ what a ridge is made of
 * A mountain was an outline: no snow where it is cold, no trees where they
 * stop, no rock showing through. This checks the difference is really being
 * drawn rather than merely being called for.
 */
(function ridgeDress() {
  console.log('\nWhat a ridge is made of');

  function marksFor(text) {
    var ctx = new FakeContext(320, 240);
    PAINT.render(ctx, 320, 240, PROMPT.parse(text, { seed: 3 }));
    var total = 0;
    Object.keys(ctx.ops).forEach(function (k) { total += ctx.ops[k]; });
    return { clips: ctx.ops.clip || 0, marks: total };
  }

  /* Snow, trees and rock each clip themselves to the ridge already drawn, so
   * the number of clips counts how many of them ran. Counting marks alone
   * would not: a mountain drew more than a field long before any of this. */
  var mountains = marksFor('mountains at noon');
  var snowy = marksFor('snowy mountains at noon');
  var plains = marksFor('open plains at noon');

  check(mountains.clips >= 8,
    'a mountain is dressed layer by layer — snow, trees, rock (' +
    mountains.clips + ' clipped passes)');
  check(snowy.clips >= 4,
    'and so is a snowfield (' + snowy.clips + ' clipped passes)');
  /* Flat ground is dressed too now — it gets grain, because a field is the
   * largest flat fill in most pictures — but it has no slope to put snow, trees
   * or strata on, so it takes far fewer passes than a mountain. */
  check(plains.clips > 0 && plains.clips < mountains.clips / 2,
    'flat ground gets its grain but none of the slope work (' +
    plains.clips + ' against a mountain\'s ' + mountains.clips + ')');
  /* Counted on the ground pass alone. A whole picture includes its sky, and a
   * sky full of clouds is a great deal of drawing that has nothing to do with
   * what the ground is made of — measuring the lot would have this check
   * quietly reporting on the weather. */
  function groundMarks(text) {
    var spec = PROMPT.parse(text, { seed: 3 });
    var ctx = new FakeContext(320, 240);
    var draw = PAINT.GROUND[spec.scene.id] || PAINT.GROUND.plains;
    draw(ctx, 320, 240, 150, PAINT.makePalette(spec), spec,
      PROMPT.rng(spec, 'ground'), { x: 100, y: 40 });
    var total = 0;
    Object.keys(ctx.ops).forEach(function (k) { total += ctx.ops[k]; });
    return total;
  }
  var ridgeWork = groundMarks('mountains at noon');
  var fieldWork = groundMarks('open plains at noon');
  check(ridgeWork > fieldWork * 3,
    'a mountain now takes several times the drawing a field does (' +
    ridgeWork + ' vs ' + fieldWork + ')');
  pass('ridges are made of something now');
})();

/* ------------------------------------------------------------------ the lens
 * One distance sharp and everything else soft is the strongest single cue that
 * a picture is a photograph. Checked on an image built here, so "sharp" and
 * "soft" can be measured rather than judged by eye: a hard edge stays a hard
 * edge where the lens is focused, and stops being one where it is not.
 */
(function theLens() {
  console.log('\nThe lens');

  function striped(w, h) {
    var d = new Uint8ClampedArray(w * h * 4);
    for (var y = 0; y < h; y++) {
      for (var x = 0; x < w; x++) {
        var i = (y * w + x) * 4;
        /* Stripes two pixels wide, because the blur radius is a share of the
         * picture's size and on an image this small that is a single pixel —
         * wider stripes would keep their extremes and the test would report a
         * working blur as doing nothing. */
        var on = (x % 4) < 2 ? 255 : 0;      // hard vertical edges everywhere
        d[i] = d[i + 1] = d[i + 2] = on;
        d[i + 3] = 255;
      }
    }
    return { data: d, width: w, height: h };
  }
  /* How hard the edges are in one row: a sharp row swings all the way between
   * black and white, a blurred one does not. */
  function contrastAt(img, w, y) {
    var d = img.data, lo = 255, hi = 0;
    for (var x = 0; x < w; x++) {
      var v = d[(y * w + x) * 4];
      if (v < lo) lo = v;
      if (v > hi) hi = v;
    }
    return hi - lo;
  }

  /* A picture the size of a real one, because the blur radius is a share of
   * the picture's size: on a postage stamp it rounds down to a single pixel and
   * a working lens measures as a broken one. */
  var W = 400, H = 600;
  var img = striped(W, H);
  FINISH.helpers.focusPass(img, W, H, { y: 0.5, reach: 0.16, strength: 0.9 });

  var atFocus = contrastAt(img, W, Math.round(H * 0.5));
  var farOff = contrastAt(img, W, 4);
  check(atFocus > 200, 'what is focused on stays sharp (' + atFocus + ' of 255)');
  check(farOff < atFocus * 0.75,
    'and what is not goes soft (' + farOff + ' against ' + atFocus + ')');
  check(farOff > 10, 'soft, not erased — a background is out of focus, not gone (' + farOff + ')');

  /* The sharp band has to be at least as deep as the thing being focused on,
   * or the lens focuses on the middle of an animal and blurs its own head. */
  var deep = striped(W, H);
  FINISH.helpers.focusPass(deep, W, H, { y: 0.5, reach: 0.8, strength: 0.9 });
  check(contrastAt(deep, W, Math.round(H * 0.25)) > contrastAt(img, W, Math.round(H * 0.25)),
    'a deeper subject keeps more of itself sharp');

  /* A landscape is sharp front to back; a close-up is not. The framing decides,
   * which is what a real lens does. */
  function focusOf(text) {
    var ctx = new FakeContext(200, 150);
    return PAINT.render(ctx, 200, 150, PROMPT.parse(text, { seed: 11 })).focus;
  }
  var close = focusOf('a close up of a stag in a forest');
  var wide = focusOf('a stag in a vast forest, wide');
  check(close && wide && close.strength > wide.strength * 2.5,
    'a close-up is shallow and a landscape is not (' +
    close.strength + ' against ' + wide.strength + ')');
  check(close.y >= 0 && close.y <= 1 && close.reach > 0,
    'and the lens is focused somewhere inside the picture');
  pass('focus follows the framing, and keeps the subject sharp');
})();

/* ------------------------------------------------- colour that shifts with light
 * Things do not simply go darker in shadow and lighter in sun; they go bluer and
 * warmer. Painting shade as grey is one of the loudest tells that a picture was
 * drawn rather than taken.
 */
(function daylightColour() {
  console.log('\nColour that shifts with light');

  function greys(n) {
    var d = new Uint8ClampedArray(n * 4);
    for (var i = 0; i < n; i++) {
      var v = Math.round((i / (n - 1)) * 255);
      d[i * 4] = d[i * 4 + 1] = d[i * 4 + 2] = v;
      d[i * 4 + 3] = 255;
    }
    return { data: d, width: n, height: 1 };
  }
  var n = 32;
  var img = greys(n);
  var P = PAINT.makePalette(PROMPT.parse('a wolf in snow at noon', { seed: 2 }));
  FINISH.helpers.daylight(img, P, 0.5);

  var d = img.data;
  var darkBlueness = d[2] - d[0];                     // blue minus red, in shadow
  var lightWarmth = d[(n - 1) * 4] - d[(n - 1) * 4 + 2];   // red minus blue, in light
  check(darkBlueness > 2,
    'shadow takes the colour of the sky rather than going grey (blue over red by ' +
    darkBlueness + ')');
  check(lightWarmth > 2,
    'and light takes the colour of the sun (red over blue by ' + lightWarmth + ')');

  /* A flat grey ramp has to stay a ramp — this tints, it does not crush. */
  var rising = true;
  for (var i = 1; i < n; i++) {
    if (d[i * 4] + d[i * 4 + 1] + d[i * 4 + 2] <= d[(i - 1) * 4] + d[(i - 1) * 4 + 1] + d[(i - 1) * 4 + 2]) rising = false;
  }
  check(rising, 'and dark is still darker than light afterwards');
  pass('shade goes blue, sun goes warm, nothing is crushed');
})();

/* ------------------------------------------------------------ grain and coat
 * Nothing in the world is one smooth colour, and nothing with fur has a clean
 * outline. Both were true of every picture this engine made.
 */
(function grainAndCoat() {
  console.log('\nGrain and coat');

  function marks(text, seed) {
    var ctx = new FakeContext(640, 480);
    PAINT.render(ctx, 640, 480, PROMPT.parse(text, { seed: seed == null ? 4 : seed }));
    return ctx.ops;
  }

  /* Grain is drawn as many small marks clipped to the band they belong to.
   * Sparse grain is invisible — the first attempt was one mark per thirty-by-
   * thirty patch and could not be seen at any size — so the count matters, not
   * merely that some exist. */
  var meadow = marks('a wolf in a meadow at noon');
  var hills = marks('mountains at noon');
  check((meadow.fillRect || 0) > 400,
    'a field is made of hundreds of marks, not one flat fill (' +
    (meadow.fillRect || 0) + ')');
  check((hills.fillRect || 0) > 800,
    'and a mountain of more, because it has more surfaces (' +
    (hills.fillRect || 0) + ')');

  /* Underground there is no sky, so no haze — but there is still rock, so
   * there should still be grain. */
  var cave = marks('a crystal in a cave');
  check((cave.fillRect || 0) > 40, 'a cave is grained too (' + (cave.fillRect || 0) + ')');

  /* The coat. Only things that have one. */
  check(PAINT.COATED.quadruped && PAINT.COATED.bird && PAINT.COATED.humanoid,
    'animals, birds and people have a coat');
  check(!PAINT.COATED.tower && !PAINT.COATED.castle && !PAINT.COATED.cabin &&
        !PAINT.COATED.crystal,
    'and towers, castles, cabins and crystals do not — a ragged castle is a mistake, not fur');

  /* Drawn, not merely declared — and measured on the same animal with its coat
   * taken away rather than against some other subject, because a wolf and a
   * tower differ for a dozen reasons and comparing them proves nothing. */
  var withCoat = marks('a wolf in a meadow at noon', 9);
  var wasCoated = PAINT.COATED.quadruped;
  PAINT.COATED.quadruped = false;
  var without = marks('a wolf in a meadow at noon', 9);
  PAINT.COATED.quadruped = wasCoated;

  var extra = (withCoat.fill || 0) - (without.fill || 0);
  check(extra >= 20,
    'the same wolf puts its outline down ' + extra + ' more times for its coat');
  check((without.fill || 0) > 0, 'and is still drawn without one');
  pass('surfaces have grain, and things with fur have a fringe');
})();

/* --------------------------------------------------- shadow, bounce, detail
 * Three things every real photograph has and no painted one here had.
 *
 * FakeContext counts operations but throws their numbers away, and its
 * gradients forget their colours — so none of the three can be seen through
 * it. This records both: every call with its arguments, and every colour stop
 * of every gradient. It is the drawing itself, not a tally of it.
 */
function recorder(w, h) {
  var inner = new FakeContext(w, h);
  var log = [];  /* not "calls" — FakeContext owns that name, and it is a number */
  var gradients = [];
  var proxy = { log: log, gradients: gradients, inner: inner };
  for (var key in inner) {
    (function (k) {
      if (typeof inner[k] !== 'function') {
        Object.defineProperty(proxy, k, {
          get: function () { return inner[k]; },
          set: function (v) { log.push({ op: 'set', args: [k, v] }); inner[k] = v; },
          enumerable: true, configurable: true
        });
        return;
      }
      proxy[k] = function () {
        log.push({ op: k, args: Array.prototype.slice.call(arguments) });
        return inner[k].apply(inner, arguments);
      };
    })(key);
  }
  /* The style properties are never set on FakeContext itself, so the loop
   * above never sees them — and the colour of a mark is half of what it is. */
  ['fillStyle', 'strokeStyle', 'globalAlpha', 'lineWidth', 'globalCompositeOperation',
   'filter', 'shadowBlur', 'shadowColor', 'lineCap', 'lineJoin', 'font'].forEach(function (k) {
    var held;
    Object.defineProperty(proxy, k, {
      get: function () { return held; },
      set: function (v) { log.push({ op: 'set', args: [k, v] }); held = v; },
      enumerable: true, configurable: true
    });
  });

  function grad() {
    var g = { stops: [], addColorStop: function (o, c) { this.stops.push({ at: o, colour: c }); } };
    gradients.push(g);
    /* The arguments matter as much as the stops: they are where the gradient
     * starts and where it dies out, which is the shape of the light. */
    log.push({ op: 'gradient', args: Array.prototype.slice.call(arguments), gradient: g });
    return g;
  }
  proxy.createLinearGradient = grad;
  proxy.createRadialGradient = grad;
  proxy.of = function (op) {
    return log.filter(function (c) { return c.op === op; });
  };
  return proxy;
}

/*
 * Every closed shape a recording drew: the colour it was filled or stroked in,
 * how opaque it was, where its middle ended up and how wide it was. A picture
 * is shapes; this is how a test sees the ones that were actually put down.
 *
 * The coordinates are carried through the canvas transform, because some of
 * these routines translate before they draw — an ellipse is drawn round the
 * origin after moving there — and without that the numbers a test reads are in
 * whatever space that routine happened to be working in. That is not a small
 * error: it puts marks at the top-left corner of the picture and a test asking
 * "is anything above the horizon" believes it.
 */
function shapesOf(ctx) {
  var shapes = [], colour = null, stroke = null, alpha = 1, pts = [];
  /* [a, b, c, d, e, f], the same six numbers the canvas keeps. */
  var m = [1, 0, 0, 1, 0, 0], stack = [];

  function mul(n) {
    m = [
      m[0] * n[0] + m[2] * n[1], m[1] * n[0] + m[3] * n[1],
      m[0] * n[2] + m[2] * n[3], m[1] * n[2] + m[3] * n[3],
      m[0] * n[4] + m[2] * n[5] + m[4], m[1] * n[4] + m[3] * n[5] + m[5]
    ];
  }
  function place(pt) {
    return [m[0] * pt[0] + m[2] * pt[1] + m[4], m[1] * pt[0] + m[3] * pt[1] + m[5]];
  }

  ctx.log.forEach(function (c) {
    var a = c.args;
    if (c.op === 'save') stack.push(m.slice());
    else if (c.op === 'restore') m = stack.length ? stack.pop() : [1, 0, 0, 1, 0, 0];
    else if (c.op === 'translate') mul([1, 0, 0, 1, a[0], a[1]]);
    else if (c.op === 'scale') mul([a[0], 0, 0, a[1], 0, 0]);
    else if (c.op === 'rotate') {
      mul([Math.cos(a[0]), Math.sin(a[0]), -Math.sin(a[0]), Math.cos(a[0]), 0, 0]);
    } else if (c.op === 'transform') mul(a.slice(0, 6));
    else if (c.op === 'setTransform') m = a.slice(0, 6);
    else if (c.op === 'set' && a[0] === 'fillStyle') colour = a[1];
    else if (c.op === 'set' && a[0] === 'strokeStyle') stroke = a[1];
    else if (c.op === 'set' && a[0] === 'globalAlpha') alpha = a[1];
    else if (c.op === 'beginPath') pts = [];
    else if (c.op === 'moveTo' || c.op === 'lineTo' || c.op === 'quadraticCurveTo') {
      /* A curve's last two numbers are where it ends up; the control point in
       * front of them is not on the line. */
      pts.push(place(a.slice(-2)));
    } else if ((c.op === 'fill' || c.op === 'stroke') && pts.length) {
      var sx = 0, sy = 0, lo = pts[0][0], hi = pts[0][0], top = pts[0][1], low = pts[0][1];
      pts.forEach(function (pt) {
        sx += pt[0]; sy += pt[1];
        lo = Math.min(lo, pt[0]); hi = Math.max(hi, pt[0]);
        top = Math.min(top, pt[1]); low = Math.max(low, pt[1]);
      });
      shapes.push({
        colour: c.op === 'fill' ? colour : stroke, filled: c.op === 'fill', alpha: alpha,
        x: sx / pts.length, y: sy / pts.length, w: hi - lo, h: low - top,
        top: top, bottom: low
      });
      if (c.op === 'fill') pts = [];
    }
  });
  return shapes;
}

/*
 * Was this gradient ever actually put on the canvas?
 *
 * A gradient that is built and then not used is not a pass — and a test that
 * counts the gradients a render made will happily pass against code that has
 * stopped drawing with them. This looks for the moment it became the fill and
 * something was filled with it.
 */
function laidDown(ctx, gradient) {
  var chosen = false;
  for (var i = 0; i < ctx.log.length; i++) {
    var c = ctx.log[i];
    if (c.op === 'set' && c.args[0] === 'fillStyle') chosen = c.args[1] === gradient;
    else if (chosen && (c.op === 'fill' || c.op === 'fillRect')) return true;
  }
  return false;
}

/* ------------------------------------------------------------- the shadow
 * A shadow is not a smudge under a thing. It is dark and sharp where the thing
 * meets the ground and it opens out, softens and fades as it runs away from
 * the light — and when the sun is low it runs a long way. The old one was a
 * single ellipse, which is why everything looked pasted on.
 */
(function shadowsSoften() {
  console.log('\nShadows that soften as they run');

  var box = { x: 100, y: 100, w: 120, h: 160 };
  var cx = box.x + box.w / 2;

  function cast(text, lightX) {
    var spec = PROMPT.parse(text, { seed: 5 });
    var ctx = recorder(400, 400);
    PAINT.groundShadow(ctx, box, PAINT.makePalette(spec),
      { x: lightX == null ? cx - 300 : lightX, y: 20 }, spec);
    /* Each ellipse is one translate → scale → arc, in that order. */
    var out = [];
    for (var i = 0; i < ctx.log.length; i++) {
      if (ctx.log[i].op !== 'arc') continue;
      var arc = ctx.log[i];
      var scale = null, move = null, g = null;
      for (var j = i - 1; j >= 0 && (!scale || !move || !g); j--) {
        if (!scale && ctx.log[j].op === 'scale') scale = ctx.log[j];
        else if (!move && ctx.log[j].op === 'translate') move = ctx.log[j];
        else if (!g && ctx.log[j].op === 'gradient') g = ctx.log[j].gradient;
      }
      out.push({ rx: arc.args[2], squash: scale.args[1],
                 x: move.args[0], stops: g.stops });
    }
    return out;
  }

  var noon = cast('a wolf in a meadow at noon');
  check(noon.length >= 4,
    'a shadow is built from ' + noon.length + ' shapes, not one — one ellipse is a smudge');

  var widens = true, flattens = true;
  for (var i = 1; i < noon.length; i++) {
    if (noon[i].rx <= noon[i - 1].rx) widens = false;
    if (noon[i].squash >= noon[i - 1].squash) flattens = false;
  }
  check(widens, 'each one is wider than the last, so the edge opens out');
  check(flattens, 'and flatter, so it lies on the ground rather than standing up');

  /* It has to run away from the light, and it has to change sides when the
   * light does — otherwise it is only leaning in a direction that happens to
   * look right in one picture. */
  function drift(shapes) { return shapes[shapes.length - 1].x - shapes[0].x; }
  var fromLeft = cast('a wolf in a meadow at noon', cx - 300);
  var fromRight = cast('a wolf in a meadow at noon', cx + 300);
  check(drift(fromLeft) > 0 && drift(fromRight) < 0,
    'and it runs away from the light, whichever side the light is on (' +
    Math.round(drift(fromLeft)) + 'px right of a left-hand sun, ' +
    Math.round(drift(fromRight)) + 'px of a right-hand one)');

  /* Softening is in the alpha of the near stop: the far end of the shadow is
   * fainter than the end touching the thing's feet. */
  function alphaOf(stop) {
    var m = /,\s*([0-9.]+)\s*\)$/.exec(stop.colour);
    return m ? parseFloat(m[1]) : NaN;
  }
  var firstDark = alphaOf(noon[0].stops[0]);
  var lastDark = alphaOf(noon[noon.length - 1].stops[0]);
  check(firstDark > lastDark,
    'the contact end is the darkest (' + firstDark.toFixed(2) + ' against ' +
    lastDark.toFixed(2) + ' at the far end)');

  /* A low sun throws a long shadow. This is the same animal, the same light
   * position, the same seed — only the hour differs. */
  function reach(shapes) {
    var far = 0;
    shapes.forEach(function (s) { far = Math.max(far, Math.abs(s.x - cx)); });
    return far;
  }
  var dusk = cast('a wolf in a meadow at dusk');
  check(reach(dusk) > reach(noon) * 1.5,
    'and at dusk it stretches ' + Math.round(reach(dusk)) + 'px against ' +
    Math.round(reach(noon)) + 'px at noon');
  pass('a shadow opens, softens, fades and lengthens with the hour');
})();

/* ------------------------------------------------------- light that bounces
 * Light does not stop when it hits the ground. Grass throws green up under a
 * horse's belly; sea throws blue-green up a boat's hull. Without it the
 * underside of everything goes to flat black, which is the single clearest
 * tell of a drawing.
 */
(function lightBounces() {
  console.log('\nLight that bounces off the ground');

  /* The subject is painted on its own, into a box of a size chosen here, so
   * that how far the bounce climbs can be measured against something known. */
  function bounce(text, box) {
    var spec = PROMPT.parse(text, { seed: 3 });
    var P = PAINT.makePalette(spec);
    var PS = PAINT.makePalette(spec, { tintStrength: 0.85 });
    var ground = box.anchor === 'water' ? (P.scene.sea || P.scene.far) : P.scene.land;
    var want = P.css(ground, 0.62);
    var ctx = recorder(480, 360);
    PAINT.paintSubject(ctx, spec.subject, box, P, PS,
      PROMPT.rng(spec, 'subject'), spec, { x: 60, y: 30 }, 260, 360);

    for (var i = 0; i < ctx.log.length; i++) {
      var c = ctx.log[i];
      if (c.op !== 'gradient' || !c.gradient.stops.length) continue;
      if (c.gradient.stops[0].colour !== want) continue;
      /* Built is not drawn: a pass that makes a gradient and then never fills
       * anything with it is not a pass at all. */
      if (!laidDown(ctx, c.gradient)) continue;
      var found = { colour: want, stops: c.gradient.stops, alpha: 0, climb: 0 };
      /* The gradient's own geometry: y0 at the feet, y1 where it dies out. */
      found.climb = Math.abs(c.args[3] - c.args[1]);
      /* And the strength it is laid on at — the next alpha the stencil sets. */
      for (var j = i + 1; j < ctx.log.length; j++) {
        if (ctx.log[j].op === 'set' && ctx.log[j].args[0] === 'globalAlpha') {
          found.alpha = ctx.log[j].args[1];
          break;
        }
      }
      return found;
    }
    return null;
  }

  var stands = { x: 160, y: 140, w: 120, h: 160, depth: 0, anchor: 'ground' };
  var floats = { x: 160, y: 140, w: 120, h: 160, depth: 0, anchor: 'water' };

  var grass = bounce('a horse in a meadow at noon', stands);
  check(!!grass, 'a horse standing on grass is lit from below by the grass');

  var water = bounce('a boat on the sea at noon', floats);
  check(!!water, 'and a boat on water by the water');
  check(grass && water && grass.colour !== water.colour,
    'and the two are different colours, so it really is the ground it stands on');

  /* It fades out going up: the belly is lit, the back is not. */
  check(grass && /,\s*0\)$/.test(grass.stops[grass.stops.length - 1].colour),
    'it has died away entirely by the top, so only the underside catches it');

  /* Bounced light is reflected sunlight, so there has to be sunlight. The
   * first attempt washed the same green up a castle wall at midnight, which is
   * a picture admitting it was drawn. */
  var noon = bounce('a horse in a meadow at noon', stands);
  var night = bounce('a horse in a meadow at night', stands);
  check(noon && night && noon.alpha > night.alpha * 3,
    'a field hands back far more at noon than at midnight (' +
    (noon ? noon.alpha.toFixed(3) : '-') + ' against ' +
    (night ? night.alpha.toFixed(3) : '-') + ')');

  /* How high it climbs is set by how wide the thing is, not how tall. Bounce
   * falls away with distance from the ground: it licks a horse's belly and the
   * foot of a castle wall, and a wash halfway up a keep is a mistake. */
  var wide = bounce('a castle in a meadow at noon',
    { x: 100, y: 120, w: 220, h: 180, depth: 0, anchor: 'ground' });
  var narrow = bounce('a tower in a meadow at noon',
    { x: 200, y: 40, w: 60, h: 280, depth: 0, anchor: 'ground' });
  check(wide && narrow && narrow.climb < wide.climb,
    'a tower two hundred and eighty high takes less of it than a castle a ' +
    'hundred and eighty high, because the tower is the narrower (' +
    (narrow ? Math.round(narrow.climb) : '-') + 'px against ' +
    (wide ? Math.round(wide.climb) : '-') + 'px)');
  check(narrow && narrow.climb < 280 * 0.35,
    'and it stays down at the foot of it (' +
    (narrow ? Math.round(narrow.climb) : '-') + 'px of 280)');
  pass('the ground throws its own colour back up into what stands on it');
})();

/* --------------------------------------------- detail that comes with closeness
 * A mountain a mile off is a silhouette. The same mountain from its foot is
 * rock faces and boulders and single trees. The engine drew both the same.
 */
(function detailWithCloseness() {
  console.log('\nDetail that arrives as you walk closer');

  var SHOTS = {};
  LEX.SHOTS.forEach(function (s) { SHOTS[s.id] = s; });

  /* One scene, one seed. Only where the camera stands changes — otherwise a
   * different scene is being measured, not a different distance. */
  function paint(shotId, seed) {
    var spec = PROMPT.parse('mountains at noon', { seed: seed });
    spec.shot = shotId ? SHOTS[shotId] : null;
    var ctx = recorder(640, 480);
    PAINT.render(ctx, 640, 480, spec);
    return ctx.log;
  }

  var order = ['wide', null, 'near', 'closeup'];
  var names = ['a wide shot', 'an ordinary shot', 'a near shot', 'a close-up'];
  var rising = true, counts = [];
  for (var seed = 1; seed <= 3; seed++) {
    var row = order.map(function (s) {
      return paint(s, seed).filter(function (c) { return c.op === 'fillRect'; }).length;
    });
    for (var i = 1; i < row.length; i++) if (row[i] <= row[i - 1]) rising = false;
    if (seed === 1) counts = row;
  }
  check(rising,
    'the same mountain is made of more marks the closer you stand — ' +
    names.map(function (n, i) { return n + ' ' + counts[i]; }).join(', ') +
    ', and the same order on three seeds');

  /* More marks alone would just be a denser mess. They also have to be
   * smaller, or a close-up is a distant view with the speckle turned up. */
  function median(list) {
    var s = list.slice().sort(function (a, b) { return a - b; });
    return s[Math.floor(s.length / 2)];
  }
  /* Only the grain's own marks, found by the colour set immediately before
   * them: a whisper of black or white. Every fillRect in the picture would
   * measure the mix of everything drawn, which shifts with the framing anyway
   * and so would report a change even if no mark had altered its size. */
  function markSize(shotId) {
    var log = paint(shotId, 1);
    var sizes = [];
    for (var i = 1; i < log.length; i++) {
      if (log[i].op !== 'fillRect') continue;
      var before = log[i - 1];
      if (before.op !== 'set' || before.args[0] !== 'fillStyle') continue;
      if (!/^rgba\((0,0,0|255,255,255),0\.0/.test(String(before.args[1]))) continue;
      sizes.push(Math.abs(log[i].args[3]));
    }
    return { n: sizes.length, size: median(sizes) };
  }
  var closeGrain = markSize('closeup'), wideGrain = markSize('wide');
  check(closeGrain.n > 200 && wideGrain.n > 100,
    'the grain is what is being measured (' + closeGrain.n + ' marks close up, ' +
    wideGrain.n + ' wide)');
  var closeMark = closeGrain.size, wideMark = wideGrain.size;
  check(closeMark < wideMark,
    'and each mark is finer close up (' + closeMark.toFixed(2) + 'px against ' +
    wideMark.toFixed(2) + 'px wide), so it reads as texture and not as noise');
  pass('walking closer brings more detail, and finer detail');
})();

/* ------------------------------------------------------------- the hour
 * There were four hours — dawn, day, dusk, night — and every picture borrowed
 * one of the four skies. What actually makes a sky is how high the sun is
 * standing, which is a number, so there are as many skies as there are angles.
 */
(function theHour() {
  console.log('\nThe hour of the day');

  function sunOf(text) { return PROMPT.parse(text, { seed: 11 }).sun; }

  /* Words inside one band are not the same hour. "First light" and
   * "mid-morning" were both `dawn` and were painted identically. */
  var early = sunOf('a stag in a meadow at first light');
  var mid = sunOf('a stag in a meadow at mid-morning');
  var high = sunOf('a stag in a meadow at high noon');
  var evening = sunOf('a stag in a meadow at blue hour');
  check(early < mid && mid < high,
    'first light, mid-morning and high noon are three heights of sun (' +
    early + '°, ' + mid + '°, ' + high + '°)');
  check(early < 0 && high > 80,
    'first light is below the horizon and high noon nearly overhead');
  check(evening < 0, 'and the blue hour is after the sun has gone (' + evening + '°)');

  /* Which way it is going, because a sunrise is not a sunset run backwards. */
  check(PROMPT.parse('a stag at dawn', { seed: 2 }).rising === true &&
        PROMPT.parse('a stag at sunset', { seed: 2 }).rising === false,
    'dawn is on the way up and sunset on the way down');
  check(PROMPT.parse('a stag in the afternoon', { seed: 2 }).rising === false,
    'and the afternoon is daylight on the way down, not on the way up');

  /* Said nothing about the hour: the sun stands somewhere inside the band it
   * rolled, anywhere at all, so two pictures of the same words are lit
   * differently rather than picking from four. */
  var rolled = {}, spread = [];
  for (var seed = 0; seed < 40; seed++) {
    var spec = PROMPT.parse('a stag in a meadow', { seed: seed });
    rolled[spec.time] = true;
    spread.push(spec.sun);
  }
  var distinct = {};
  spread.forEach(function (v) { distinct[v] = true; });
  check(Object.keys(distinct).length > 20,
    'with no hour given, forty pictures stand at ' + Object.keys(distinct).length +
    ' different sun heights, not at four');
  var bands = LEX.TIMES;
  var outside = 0;
  for (seed = 0; seed < 40; seed++) {
    var sp = PROMPT.parse('a stag in a meadow', { seed: seed });
    var band = bands.filter(function (b) { return b.id === sp.time; })[0];
    if (sp.sun < band.band[0] || sp.sun > band.band[1]) outside++;
  }
  check(outside === 0, 'and every one of them inside the band it says it is in');

  /* The sky is mixed from the two hours it falls between. Nothing about that
   * should jump: a degree of sun is not a different sky. */
  function bright(sky) { return sky.top[2] + sky.mid[2] + sky.low[2]; }
  var jumpiest = 0, jumpAt = 0;
  for (var e = -59; e <= 89; e += 0.5) {
    var step = Math.abs(bright(PAINT.skyAt(e, false)) - bright(PAINT.skyAt(e - 0.5, false)));
    if (step > jumpiest) { jumpiest = step; jumpAt = e; }
  }
  check(jumpiest < 8,
    'half a degree of sun never moves the sky by more than a little (worst ' +
    jumpiest.toFixed(1) + ' at ' + jumpAt + '°)');
  check(bright(PAINT.skyAt(70, false)) > bright(PAINT.skyAt(10, false)) &&
        bright(PAINT.skyAt(10, false)) > bright(PAINT.skyAt(-8, false)) &&
        bright(PAINT.skyAt(-8, false)) > bright(PAINT.skyAt(-40, false)),
    'and the higher the sun, the brighter the sky, all the way down to midnight');

  /* Mixing two skies by rotating the hue takes the short way round the wheel,
   * and the short way from a sunset orange to a daylight blue goes through
   * green: the sky turned bilious at mid-morning. It is mixed in red, green
   * and blue instead, so it washes out rather than turning. */
  var greenest = 0, greenAt = null;
  for (e = -60; e <= 90; e += 0.25) {
    [true, false].forEach(function (rising) {
      var sky = PAINT.skyAt(e, rising);
      ['top', 'mid', 'low'].forEach(function (band) {
        var hue = sky[band][0], sat = sky[band][1];
        if (hue > 80 && hue < 160 && sat > greenest) { greenest = sat; greenAt = e; }
      });
    });
  }
  check(greenest < 25,
    'and no hour of the day has a green sky (the greenest is ' +
    greenest.toFixed(0) + '% saturated, at ' + greenAt + '°)');

  /* The ground is only as bright as the light falling on it. A meadow was the
   * same green at midnight as at noon, with a dark sky hung above it. */
  function landL(text) {
    var P = PAINT.makePalette(PROMPT.parse(text, { seed: 4 }));
    return P.scene.land[2];
  }
  var noonLand = landL('a stag in a meadow at high noon');
  var nightLand = landL('a stag in a meadow at midnight');
  var duskLand = landL('a stag in a meadow at sunset');
  check(nightLand < noonLand * 0.5,
    'a field at midnight is less than half as light as the same field at noon (' +
    nightLand.toFixed(0) + '% against ' + noonLand.toFixed(0) + '%)');
  check(duskLand < noonLand && duskLand > nightLand,
    'and at sunset it is somewhere between the two (' + duskLand.toFixed(0) + '%)');

  /* How far a shadow runs is the cotangent of the sun's angle — a puddle at
   * noon, half the afternoon long near the horizon. */
  check(PAINT.shadowReach(80) < PAINT.shadowReach(30) &&
        PAINT.shadowReach(30) < PAINT.shadowReach(5),
    'a shadow lengthens as the sun drops (' + PAINT.shadowReach(80).toFixed(2) +
    ' at 80°, ' + PAINT.shadowReach(30).toFixed(2) + ' at 30°, ' +
    PAINT.shadowReach(5).toFixed(2) + ' at 5°)');
  check(PAINT.groundLit(75) > PAINT.groundLit(20) &&
        PAINT.groundLit(20) > PAINT.groundLit(-30),
    'and the ground hands back less light the lower the sun gets');
  pass('the hour is an angle, and everything follows it');
})();

/* --------------------------------------------------------- sun through cloud
 * An overcast picture was a sunny picture with grey clouds pasted over the
 * top: the sun still hung there, and everything still threw a hard black
 * shadow from it. Cloud turns the sun into the whole sky.
 */
(function sunThroughCloud() {
  console.log('\nThe sun behind the weather');

  var lift = PAINT.softness;
  check(lift({ weather: 'clear' }) === 0, 'a clear sky softens nothing');
  check(lift({ weather: 'clouds' }) > 0 &&
        lift({ weather: 'rain' }) > lift({ weather: 'clouds' }) &&
        lift({ weather: 'storm' }) > lift({ weather: 'rain' }) &&
        lift({ weather: 'fog' }) > lift({ weather: 'storm' }),
    'and cloud, rain, storm and fog soften more and more (' +
    [lift({ weather: 'clouds' }), lift({ weather: 'rain' }),
     lift({ weather: 'storm' }), lift({ weather: 'fog' })].join(', ') + ')');

  /* One picture, one seed, one scene — only the weather is changed, so what is
   * measured is the weather and not a different roll. */
  function underWeather(weather) {
    var spec = PROMPT.parse('a stag in a meadow at noon', { seed: 6 });
    spec.weather = weather;
    /* How much weather there is rolls when nobody says, and this is a test
     * about which weather rather than how much of it. */
    spec.weatherStrength = 1;
    return spec;
  }

  function shadow(weather) {
    var spec = underWeather(weather);
    var box = { x: 140, y: 130, w: 120, h: 150 };
    var ctx = recorder(400, 400);
    PAINT.groundShadow(ctx, box, PAINT.makePalette(spec), { x: 40, y: 20 }, spec);
    var first = null, widest = 0;
    for (var i = 0; i < ctx.log.length; i++) {
      if (ctx.log[i].op !== 'arc') continue;
      widest = Math.max(widest, ctx.log[i].args[2]);
      if (first != null) continue;
      for (var j = i - 1; j >= 0; j--) {
        if (ctx.log[j].op === 'gradient') {
          var m = /,\s*([0-9.]+)\s*\)$/.exec(ctx.log[j].gradient.stops[0].colour);
          first = m ? parseFloat(m[1]) : 0;
          break;
        }
      }
    }
    return { dark: first, wide: widest };
  }

  var sunny = shadow('clear'), grey = shadow('clouds'), murk = shadow('fog');
  check(grey.dark < sunny.dark * 0.6,
    'an overcast shadow is a fraction as dark as a sunlit one (' +
    grey.dark.toFixed(2) + ' against ' + sunny.dark.toFixed(2) + ')');
  check(murk.dark < grey.dark, 'and fog takes even that away (' + murk.dark.toFixed(2) + ')');
  check(grey.wide > sunny.wide * 1.2,
    'and what is left of it has spread out (' + Math.round(grey.wide) +
    'px against ' + Math.round(sunny.wide) + 'px)');

  /* The disc itself goes behind the weather, and its glow spreads across the
   * whole sky rather than sitting in one place. */
  function disc(weather) {
    var spec = underWeather(weather);
    var P = PAINT.makePalette(spec);
    var ctx = recorder(480, 360);
    PAINT.render(ctx, 480, 360, spec);
    for (var i = 0; i < ctx.log.length; i++) {
      var c = ctx.log[i];
      if (c.op !== 'gradient' || c.args.length !== 6) continue;
      if (!c.gradient.stops.length) continue;
      /* The glow: a round gradient of the light's own colour that fades to
       * nothing, painted before the disc is. */
      if (c.gradient.stops[0].colour.indexOf('hsla') !== 0) continue;
      if (!/,\s*0\)$/.test(c.gradient.stops[c.gradient.stops.length - 1].colour)) continue;
      if (!laidDown(ctx, c.gradient)) continue;
      /* The disc: the first round thing drawn after the glow. Its colour is
       * set between the arc and the fill, not before the arc. */
      var alpha = null;
      for (var j = i + 1; j < ctx.log.length && alpha == null; j++) {
        if (ctx.log[j].op !== 'arc' || ctx.log[j].args[2] <= 5) continue;
        for (var k = j + 1; k < ctx.log.length; k++) {
          if (ctx.log[k].op === 'fill') break;
          if (ctx.log[k].op === 'set' && ctx.log[k].args[0] === 'fillStyle') {
            var m = /,\s*([0-9.]+)\s*\)$/.exec(String(ctx.log[k].args[1]));
            alpha = m ? parseFloat(m[1]) : 1;
            break;
          }
        }
      }
      return { spread: c.args[5], alpha: alpha, P: P };
    }
    return null;
  }

  var open = disc('clear'), hidden = disc('fog');
  check(open && hidden && hidden.spread > open.spread * 1.5,
    'the glow spreads much wider in fog (' + (hidden ? Math.round(hidden.spread) : '-') +
    'px against ' + (open ? Math.round(open.spread) : '-') + 'px)');
  function shown(bag) { return bag && bag.alpha != null ? bag.alpha.toFixed(2) : '-'; }
  check(open && hidden && open.alpha != null && hidden.alpha != null &&
        hidden.alpha < open.alpha * 0.5,
    'and the sun itself is barely there behind it (' + shown(hidden) +
    ' against ' + shown(open) + ')');
  pass('cloud turns the sun into the whole sky');
})();

/* ------------------------------------------------- things that are a light
 * A campfire was an orange shape on a dark field. It lit nothing — not the
 * ground it burned on, not the person standing by it — and so every night
 * picture had exactly one light in it, the moon, whatever else was in the
 * scene.
 */
(function thingsThatGlow() {
  console.log('\nThings that are a light themselves');

  function lightsFor(text) {
    var spec = PROMPT.parse(text, { seed: 8 });
    var box = { x: 180, y: 180, w: 90, h: 80, depth: 0, anchor: 'ground' };
    return {
      spec: spec,
      lights: PAINT.extraLights(spec, [{ s: spec.subject, box: box }], 480, 360)
    };
  }

  var fire = lightsFor('a campfire in a forest at night');
  check(fire.lights.length === 1, 'a campfire at night is a light in the picture');
  var day = lightsFor('a campfire in a forest at high noon');
  check(day.lights.length === 0,
    'and at noon it is not — a fire only shows against the dark');

  var stone = lightsFor('a castle in a forest at night');
  check(stone.lights.length === 0, 'a castle is not a light, whatever the hour');

  /* Reach is in widths of the thing, but a close-up of a fire is a bigger
   * fire, not a fire that lights the county. */
  var spec = PROMPT.parse('a campfire in a forest at night', { seed: 8 });
  var huge = PAINT.extraLights(spec,
    [{ s: spec.subject, box: { x: 0, y: 40, w: 900, h: 300, anchor: 'ground' } }], 480, 360);
  check(huge.length === 1 && huge[0].reach <= 480 * 0.42 + 0.001,
    'and however big the thing is, its light stops (' +
    (huge.length ? Math.round(huge[0].reach) + 'px' : 'no light at all') +
    ' in a 480px picture)');

  /* The pool it lays down is added to what is there rather than painted over
   * it, which is what light does and why a fire warms a clearing without
   * hiding it. */
  /* The pool is a round gradient in the fire's own colour, laid down with the
   * picture's light added to rather than painted over. Looking merely for an
   * additive pass would not do: beams of light through mist are additive too,
   * and so is light on water. */
  function pools(text) {
    var ctx = recorder(480, 360);
    PAINT.render(ctx, 480, 360, PROMPT.parse(text, { seed: 8 }));
    var hue = PAINT.GLOWING.campfire.hue[0].toFixed(1);
    var found = 0, additive = false;
    ctx.log.forEach(function (c) {
      if (c.op === 'set' && c.args[0] === 'globalCompositeOperation') {
        additive = c.args[1] === 'lighter';
      } else if (c.op === 'gradient' && c.args.length === 6 && additive &&
                 c.gradient.stops.length &&
                 c.gradient.stops[0].colour.indexOf('hsla(' + hue + ',') === 0 &&
                 laidDown(ctx, c.gradient)) {
        found++;
      }
    });
    return found;
  }
  check(pools('a campfire in a forest at night') > 0,
    'its pool is added to the picture, not painted over it');
  check(pools('a castle in a forest at night') === 0,
    'and a picture with nothing glowing in it lays down no pool at all');

  /* And it lights what is standing near it. */
  function litSide(text, box, lightBox) {
    var sp = PROMPT.parse(text, { seed: 8 });
    var P = PAINT.makePalette(sp);
    var PS = PAINT.makePalette(sp, { tintStrength: 0.85 });
    var fireHue = PAINT.GLOWING.campfire.hue;
    var extras = [{
      x: lightBox.x + lightBox.w * 0.5, y: lightBox.y + lightBox.h * 0.78,
      hue: fireHue, reach: lightBox.w * 3.6, strength: 0.9, box: lightBox
    }];
    var c = recorder(480, 360);
    PAINT.paintSubject(c, sp.subject, box, P, PS, PROMPT.rng(sp, 'subject'),
      sp, { x: 60, y: 30 }, 200, 360, extras);
    var want = P.css(fireHue, 0.85);
    var found = null;
    c.gradients.forEach(function (g) {
      g.stops.forEach(function (st) { if (st.colour === want) found = g; });
    });
    return found;
  }

  var near = { x: 210, y: 150, w: 70, h: 120, depth: 0, anchor: 'ground' };
  var fireBox = { x: 130, y: 210, w: 60, h: 50, depth: 0, anchor: 'ground' };
  check(!!litSide('a knight by a campfire at night', near, fireBox),
    'someone standing by the fire is lit by the fire');

  var away = { x: 430, y: 150, w: 70, h: 120, depth: 0, anchor: 'ground' };
  check(!litSide('a knight by a campfire at night', away, fireBox),
    'and someone across the field is not — it falls off with distance');

  check(!litSide('a campfire in a forest at night', fireBox, fireBox),
    'and a lamp does not light itself');
  pass('a fire lights the clearing, and whoever is standing in it');
})();

/* --------------------------------------------------------- wet ground
 * Rain was drawn as streaks in the air and nothing else: it fell in front of a
 * perfectly dry field.
 */
(function wetGround() {
  console.log('\nGround that has been rained on');

  var wet = PAINT.wetness;
  check(wet({ weather: 'clear' }) === 0 && wet({ weather: 'clouds' }) === 0,
    'a clear or merely cloudy sky leaves the ground dry');
  check(wet({ weather: 'rain' }) > wet({ weather: 'fog' }) &&
        wet({ weather: 'fog' }) > wet({ weather: 'snowfall' }) &&
        wet({ weather: 'snowfall' }) > 0,
    'rain wets it most, fog a little, falling snow less still');

  /* One scene, one seed, only the weather changed. */
  function ground(weather) {
    var spec = PROMPT.parse('a wolf on a road at dusk', { seed: 5 });
    spec.weather = weather;
    spec.weatherStrength = 1;
    return PAINT.makePalette(spec).scene.land;
  }
  var dry = ground('clear'), rained = ground('rain');
  check(rained[2] < dry[2] * 0.85,
    'wet ground is darker than dry (' + rained[2].toFixed(0) + '% against ' +
    dry[2].toFixed(0) + '%)');
  check(rained[1] > dry[1] * 1.1,
    'and more saturated, because the water fills the pores and what comes back ' +
    'is the colour rather than the scatter (' + rained[1].toFixed(0) + '% against ' +
    dry[1].toFixed(0) + '%)');

  /* And every light is smeared down it. */
  function streaks(weather) {
    var spec = PROMPT.parse('a wolf on a road at dusk', { seed: 5 });
    spec.weather = weather;
    spec.weatherStrength = 1;
    var P = PAINT.makePalette(spec);
    var ctx = recorder(480, 360);
    PAINT.render(ctx, 480, 360, spec);
    var found = 0;
    ctx.log.forEach(function (c) {
      if (c.op !== 'gradient' || c.args.length !== 4) return;
      if (c.args[0] !== c.args[2]) return;              // upright, not across
      /* Running downwards, and all the way to the bottom edge of the frame.
       * That is what a reflection down the ground is, and it is what the light
       * bouncing up off the ground into a subject is not — without this the
       * check was counting the bounce, and passing whether or not a streak had
       * ever been drawn. */
      if (c.args[3] <= c.args[1] || Math.abs(c.args[3] - 360) > 2) return;
      if (!c.gradient.stops.length) return;
      var first = c.gradient.stops[0].colour;
      var last = c.gradient.stops[c.gradient.stops.length - 1].colour;
      if (first.indexOf('hsla') !== 0 || !/,\s*0\)$/.test(last)) return;
      if (!laidDown(ctx, c.gradient)) return;
      found++;
    });
    return found;
  }
  check(streaks('rain') > streaks('clear'),
    'a light leaves a streak down wet ground and none down dry (' +
    streaks('rain') + ' against ' + streaks('clear') + ')');

  var additive = function (weather) {
    var spec = PROMPT.parse('a wolf on a road at dusk', { seed: 5 });
    spec.weather = weather;
    spec.weatherStrength = 1;
    var ctx = recorder(480, 360);
    PAINT.render(ctx, 480, 360, spec);
    return ctx.log.filter(function (c) {
      return c.op === 'set' && c.args[0] === 'globalCompositeOperation' &&
        c.args[1] === 'lighter';
    }).length;
  };
  check(additive('rain') > 0 && additive('clear') === 0,
    'and the streak is light added to the ground, not paint laid over it');
  pass('rain lands on the ground as well as falling in front of it');
})();

/* ------------------------------------------------------ what it is made of
 * A colour word says a dragon is red. It does not say that a stone dragon is
 * chalky, a bronze one dark with a hard bright edge, and a glass one has the
 * sky showing through it.
 */
(function materials() {
  console.log('\nWhat a thing is made of');

  var mats = LEX.MATERIALS;
  var seen = {}, bad = [];
  mats.forEach(function (m) {
    if (seen[m.id]) bad.push('two ' + m.id + 's');
    seen[m.id] = true;
    if (!m.colour || m.colour.length !== 3) bad.push(m.id + ' has no colour');
    ['rough', 'metal', 'clear'].forEach(function (k) {
      if (typeof m[k] !== 'number' || m[k] < 0 || m[k] > 1) bad.push(m.id + ' has a silly ' + k);
    });
    if (!m.words || !m.words.length) bad.push(m.id + ' cannot be asked for');
  });
  check(bad.length === 0,
    mats.length + ' materials, each with a colour and a roughness, a metal and ' +
    'a clearness between nothing and everything' + (bad.length ? ' — ' + bad.join(', ') : ''));

  check(PROMPT.parse('a bronze dragon over the sea', { seed: 1 }).material.id === 'bronze',
    'a bronze dragon is made of bronze');
  check(PROMPT.parse('a stone tower', { seed: 1 }).material.id === 'stone',
    'and a stone tower of stone');
  check(PROMPT.parse('a wolf in a meadow', { seed: 1 }).material === null,
    'and a wolf of nothing in particular');

  /* One word cannot be both the material and the style. */
  var glassy = PROMPT.parse('a glass dragon over the sea', { seed: 1 });
  var stained = PROMPT.parse('a dragon in stained glass', { seed: 1 });
  check(glassy.material && glassy.material.id === 'glass' && glassy.style !== 'glass',
    'a glass dragon is made of glass and not painted as a window');
  check(stained.material === null && stained.style === 'glass',
    'and a dragon in stained glass is painted as a window and made of nothing');

  /* Nor the material and the colour of the whole world. "Silver" is in both
   * tables; it means the dragon is silver, not that the sea is. */
  var silver = PROMPT.parse('a silver dragon over the sea', { seed: 1 });
  check(silver.material && silver.material.id === 'silver' && silver.palette === null,
    'a silver dragon is made of silver, and does not turn the sea silver too');

  /* Drawn, not declared. Same subject, same box, same seed — only the
   * material, so what differs is the material. */
  function marks(materialId) {
    var spec = PROMPT.parse('a tower on a hill at noon', { seed: 12 });
    spec.material = materialId
      ? mats.filter(function (m) { return m.id === materialId; })[0] : null;
    var P = PAINT.makePalette(spec);
    var PS = PAINT.makePalette(spec, { tintStrength: 0.85 });
    var ctx = recorder(420, 300);
    PAINT.paintSubject(ctx, spec.subject,
      { x: 150, y: 90, w: 110, h: 170, depth: 0, anchor: 'ground' },
      P, PS, PROMPT.rng(spec, 'subject'), spec, { x: 70, y: 30 }, 190, 300);
    var colours = ctx.log.filter(function (c) {
      return c.op === 'set' && c.args[0] === 'fillStyle' && typeof c.args[1] === 'string';
    }).map(function (c) { return c.args[1]; });
    return { colours: colours, gradients: ctx.gradients, P: P };
  }

  var plain = marks(null), stone = marks('stone'), iron = marks('iron');
  check(plain.colours.join('|') !== stone.colours.join('|'),
    'the same tower is painted in different colours once it is made of stone');
  check(stone.colours.join('|') !== iron.colours.join('|'),
    'and different ones again in iron');

  function carries(bag, colour) {
    return bag.gradients.some(function (g) {
      return g.stops.some(function (st) { return st.colour === colour; });
    });
  }

  /* Metal is mostly a reflection of where it is: sky above, ground below. */
  check(carries(iron, iron.P.css(iron.P.sky.mid, 0.85)),
    'iron has the sky in it, which is what makes metal look like metal');
  check(!carries(stone, stone.P.css(stone.P.sky.mid, 0.85)),
    'and stone does not — a chalky thing reflects nothing');

  /* A smooth thing has a tight bright highlight; a chalky one has none. */
  var hot = function (bag) { return bag.P.css(bag.P.sky.light, 0.95); };
  check(carries(iron, hot(iron)), 'iron catches the sun in a hard bright edge');
  var chalk = marks('concrete');
  check(!carries(chalk, hot(chalk)), 'concrete catches nothing at all');

  /* And something you can see through lights up at its edges. */
  var glass = marks('glass'), wood = marks('wood');
  /* Counted, not merely looked for: this tower has lit windows of its own in
   * the same colour, so the question is how many times it is laid down, not
   * whether it appears at all. */
  function times(bag, colour) {
    return bag.colours.filter(function (c) { return c === colour; }).length;
  }
  var glassEdges = times(glass, glass.P.css(glass.P.sky.light, 0.8));
  var woodEdges = times(wood, wood.P.css(wood.P.sky.light, 0.8));
  check(glassEdges >= woodEdges + 6,
    'glass glows all the way round its edge, because light that goes in comes ' +
    'out somewhere (' + glassEdges + ' passes against wood\'s ' + woodEdges + ')');
  /* And it is said back to the person in their own order — inside the name of
   * the thing, after the article and agreeing with it. */
  check(PROMPT.describe(PROMPT.parse('a bronze dragon over the sea', { seed: 1 }))
          .indexOf('a bronze dragon') === 0,
    'the picture calls itself a bronze dragon, not "bronze a dragon"');
  check(PROMPT.describe(PROMPT.parse('an iron tower', { seed: 1 })).indexOf('an iron tower') === 0,
    'and an iron tower, because the article agrees with the material now');
  pass('a thing is made of something, and the light knows it');
})();

/* ------------------------------------------------------- clouds with insides
 * A cloud was two flat blobs, one pale and one paler, and it read as a sticker
 * on the sky — which matters, because a cloud is in most of these pictures and
 * the largest thing in a good many of them.
 */
(function cloudsWithInsides() {
  console.log('\nClouds with an inside');

  /* Every closed shape drawn, with the colour it was filled in and where its
   * middle ended up. A cloud is made of shapes; this is what was drawn. */
  function sky(weather, light) {
    var spec = PROMPT.parse('open plains at noon', { seed: 9 });
    spec.weather = weather;
    spec.weatherStrength = 1;
    var P = PAINT.makePalette(spec);
    var ctx = recorder(480, 360);
    PAINT.clouds(ctx, 480, 360, 200, P, spec, PROMPT.rng(spec, 'cloud'), light || { x: 60, y: 20 });

    return { shapes: shapesOf(ctx), P: P, ctx: ctx };
  }

  var fair = sky('clear'), heavy = sky('storm');
  check(fair.shapes.length >= 30,
    'a fair sky is built from ' + fair.shapes.length + ' shapes — lumps, not one blob each');
  check(heavy.shapes.length > fair.shapes.length,
    'and a storm from more still (' + heavy.shapes.length + ')');

  /* Three passes per lump: the shadowed underside, the body, and the top that
   * can see the sun. Three distinct colours, and the lit one is the light's. */
  var lit = fair.P.light(0.85);
  var litShapes = fair.shapes.filter(function (sh) { return sh.colour === lit; });
  check(litShapes.length > 8,
    'and every lump has a top that catches the sun (' + litShapes.length + ' of them)');
  /* Drawn where they can be seen. A pass laid down at no opacity at all is a
   * pass that is not there, and counting the calls would never know. */
  var faintest = litShapes.reduce(function (a, sh) { return Math.min(a, sh.alpha); }, 1);
  check(faintest > 0.05,
    'and they are laid down thickly enough to see (the faintest at ' +
    faintest.toFixed(2) + ')');
  var colours = {};
  fair.shapes.forEach(function (sh) { colours[sh.colour] = true; });
  check(Object.keys(colours).length >= 3,
    'drawn in ' + Object.keys(colours).length + ' colours: the dark underside, ' +
    'the body, and the lit top');

  /* And the lit side is the side the light is on. This is the one thing that
   * tells you where the sun is when the sun is behind the cloud. */
  function lean(at) {
    var s = sky('clear', at);
    var l = s.P.light(0.85);
    var caps = 0, sum = 0;
    s.shapes.forEach(function (sh) {
      if (sh.colour !== l) return;
      caps++; sum += sh.x;
    });
    return caps ? sum / caps : 0;
  }
  var leftSun = lean({ x: 20, y: 20 }), rightSun = lean({ x: 460, y: 20 });
  check(rightSun > leftSun,
    'the lit tops move to whichever side the sun is on (' + Math.round(leftSun) +
    'px against ' + Math.round(rightSun) + 'px)');

  /* A cloud low in the frame is not a low cloud, it is the same cloud further
   * off — so it is smaller. */
  var near = [], far = [];
  fair.shapes.forEach(function (sh) { (sh.y < 90 ? near : far).push(sh.w); });
  function mean(list) {
    return list.length ? list.reduce(function (a, b) { return a + b; }, 0) / list.length : 0;
  }
  check(near.length && far.length && mean(near) > mean(far) * 1.5,
    'clouds near the horizon are smaller than the ones overhead (' +
    Math.round(mean(far)) + 'px against ' + Math.round(mean(near)) + 'px)');

  /* Overcast is not a lot of clouds, it is the lid coming down. */
  function hasDeck(weather) {
    var s = sky(weather);
    return s.ctx.gradients.some(function (g) {
      return g.stops.length === 3 && /,\s*0\)$/.test(g.stops[2].colour) &&
        g.stops[0].colour.indexOf('hsla') === 0 && laidDown(s.ctx, g);
    });
  }
  check(hasDeck('rain') && !hasDeck('clear'),
    'a wet sky has a lid over it and a clear one does not');
  pass('a cloud has a lit top, a dark underside and lumps in between');
})();

/* ------------------------------------------------- the small things lying about
 * Ground was a clean sheet of colour with a texture over it. Real ground is
 * covered in things — stones, tufts, sticks, shells — and none of them is
 * interesting on its own, which is exactly why they matter: a surface with
 * nothing on it reads as a painted backdrop however well it is shaded.
 */
(function smallThings() {
  console.log('\nThe small things lying about');

  function litter(scene, closeness) {
    var spec = PROMPT.parse('a wolf in a ' + scene + ' at noon', { seed: 7 });
    var P = PAINT.makePalette(spec);
    P.detail = closeness || 1;
    var ctx = recorder(480, 360);
    PAINT.scatter(ctx, 480, 360, 200, P, spec, PROMPT.rng(spec, 'scatter'));
    var shapes = shapesOf(ctx);
    /* A flower head is the only round thing out here; everything else is a
     * lump or a line. Counting the colour would catch a stone's lit top too. */
    shapes.flowers = ctx.log.filter(function (c) { return c.op === 'arc'; }).length;
    return shapes;
  }

  var meadow = litter('meadow');
  check(meadow.length > 30,
    'a field has ' + meadow.length + ' small things lying on it');

  /* Never in the sky. Everything lies on the ground, which starts at the
   * skyline — anything above it is litter floating in mid-air. */
  var above = meadow.filter(function (sh) { return sh.bottom < 200 - 1; });
  check(above.length === 0,
    'and every one of them is on the ground rather than in the sky' +
    (above.length ? ' — ' + above.length + ' are floating' : ''));

  /* Perspective: what is near the bottom of the frame is near the camera, so
   * it is bigger. Sprinkling them evenly is what makes scattered detail read
   * as noise on a photograph rather than as things on the ground. */
  function mean(list) {
    return list.length ? list.reduce(function (a, b) { return a + b; }, 0) / list.length : 0;
  }
  var close = [], distant = [];
  meadow.forEach(function (sh) {
    (sh.y > 200 + (360 - 200) * 0.55 ? close : distant).push(Math.max(sh.w, sh.h));
  });
  check(close.length && distant.length && mean(close) > mean(distant) * 1.8,
    'the ones at your feet are bigger than the ones by the skyline (' +
    mean(close).toFixed(1) + 'px against ' + mean(distant).toFixed(1) + 'px)');

  /* And they crowd towards the skyline, because a receding plane makes
   * everything crowd towards it. Sprinkled evenly, half of them would sit
   * below the halfway line; crowded, the middle one sits well above it. */
  var depths = meadow.map(function (sh) { return (sh.y - 200) / (360 - 200); })
    .sort(function (a, b) { return a - b; });
  var middle = depths[Math.floor(depths.length / 2)];
  check(middle < 0.36,
    'and they crowd towards it — the middle one sits ' +
    Math.round(middle * 100) + '% of the way down the ground, not halfway');

  /* Walk closer and there is more to see, the same as every other surface. */
  var far = litter('meadow', 0.55).length;
  var near = litter('meadow', 2.1).length;
  check(near > far * 1.4,
    'a close-up finds more of them than a wide shot does (' + near +
    ' against ' + far + ')');

  /* Different ground is covered in different things. A meadow has flowers in
   * it; a mountainside has stones. */
  var hill = litter('mountains');
  check(meadow.flowers > 0, 'a field has flowers in it (' + meadow.flowers + ')');
  check(hill.length > 20 && hill.flowers === 0,
    'and a mountainside has stones instead (' + hill.length + ' things, no flowers)');

  /* Open water has nothing lying on it. */
  var sea = litter('ocean');
  check(sea.length === 0, 'and nothing at all is lying about on the open sea');
  pass('the ground has things on it');
})();

/* ------------------------------------------------------- trees that grew
 * A tree was a trunk, two stubby branches and a ring of seven blobs, and every
 * oak in every picture was that same tree. A tree is not a shape: it is what
 * is left after a rule has been applied to itself a few times.
 */
(function treesThatGrew() {
  console.log('\nTrees that grew rather than were drawn');

  /* One spec throughout, so the colours are identical and the only thing that
   * can differ is the tree. Feeding it a different seed would change the hour
   * and the weather too, and two traces would differ because the sky did. */
  var one = PROMPT.parse('a tree in a meadow at noon', { seed: 4 });
  var pal = PAINT.makePalette(one);

  function tree(form, salt) {
    var spec = one;
    var ctx = recorder(400, 320);
    SUBJECTS.draw(ctx, { draw: 'tree', form: form },
      { x: 120, y: 40, w: 160, h: 250, depth: 0, anchor: 'ground' },
      pal, PROMPT.rng(spec, salt || 'subject'), spec);
    var strokes = [], widths = [];
    var wide = 0;
    ctx.log.forEach(function (c) {
      if (c.op === 'set' && c.args[0] === 'lineWidth') wide = c.args[1];
      else if (c.op === 'stroke') { strokes.push(wide); widths.push(wide); }
    });
    return { ctx: ctx, strokes: strokes, shapes: shapesOf(ctx),
      trace: ctx.log.map(function (c) {
        return c.op + '(' + c.args.map(function (a) {
          return typeof a === 'number' ? Math.round(a * 10) / 10 : String(a);
        }).join(',') + ')';
      }).join('|') };
  }

  var oak = tree('oak');
  check(oak.strokes.length > 40,
    'an oak is grown out of ' + oak.strokes.length + ' branches, not three');

  /* Each generation is thinner than the one it came off. */
  var thickest = Math.max.apply(null, oak.strokes);
  var thinnest = Math.min.apply(null, oak.strokes);
  check(thickest > thinnest * 4,
    'and they thin out as they go — the trunk is ' + thickest.toFixed(1) +
    'px and the twigs ' + thinnest.toFixed(1) + 'px');

  /* No two of them alike. The splits never fall in the same place twice, which
   * is the whole point of growing one rather than drawing one. */
  var again = tree('oak'), other = tree('oak', 'another');
  check(oak.trace === again.trace,
    'the same tree from the same words comes out the same tree');
  /* Compared by where its branches ended up, not by the whole trace: two trees
   * whose twigs wobble differently but which fork in exactly the same places
   * are the same tree with a shake, and that is what this is meant to catch. */
  function tips(bag) {
    /* The branches, not the leaves: a leaf blob is jittered where it sits, so
     * comparing those would report a difference in the shake of the foliage
     * over a tree that forks in exactly the same places. */
    return bag.shapes.filter(function (sh) { return !sh.filled; })
      .map(function (sh) { return Math.round(sh.x) + ',' + Math.round(sh.y); })
      .sort().join(' ');
  }
  check(tips(oak) !== tips(other) && tips(oak).length > 40,
    'and the next one along in the same picture forks in different places');

  /* Four broadleaf shapes, each with its own habit. */
  var birch = tree('birch'), willow = tree('willow'), baobab = tree('baobab');
  var kinds = [oak.trace, birch.trace, willow.trace, baobab.trace];
  var same = 0;
  for (var i = 0; i < kinds.length; i++) {
    for (var j = i + 1; j < kinds.length; j++) if (kinds[i] === kinds[j]) same++;
  }
  check(same === 0, 'an oak, a birch, a willow and a baobab all grow differently');

  /* A branch that forked sideways turns back towards the sky as it grows,
   * because that is where the light is. A birch does it hard and a willow
   * hardly at all, so the willow ends up the wider of the two. */
  function spread(bag) {
    var lo = 1e9, hi = -1e9, top = 1e9, low = -1e9;
    bag.shapes.forEach(function (sh) {
      lo = Math.min(lo, sh.x - sh.w / 2); hi = Math.max(hi, sh.x + sh.w / 2);
      top = Math.min(top, sh.top); low = Math.max(low, sh.bottom);
    });
    return (hi - lo) / Math.max(low - top, 1);
  }
  check(spread(willow) > spread(birch),
    'a willow spreads wider for its height than a birch does (' +
    spread(willow).toFixed(2) + ' against ' + spread(birch).toFixed(2) + ')');

  /* A fir is not grown — it is a straight bole with skirts of needles — but it
   * is not the same fir every time either. */
  var pine = tree('pine'), pine2 = tree('pine', 'another');
  check(pine.trace !== pine2.trace, 'and two firs are not the same fir');
  pass('trees are grown, and no two of them come out alike');
})();

/* --------------------------------------------------------- wear, dirt and age
 * Everything was brand new: no streak of dirt down it, no moss at its foot,
 * nothing that had been rained on for a hundred years. New is the one thing
 * almost nothing in the world actually is.
 */
(function wearAndAge() {
  console.log('\nWhat time has done to it');

  var ages = LEX.AGES, bad = [];
  var seen = {};
  ages.forEach(function (a) {
    if (seen[a.id]) bad.push('two ' + a.id + 's');
    seen[a.id] = true;
    if (typeof a.age !== 'number' || a.age < 0 || a.age > 1) bad.push(a.id + ' has a silly age');
    if (!a.words || !a.words.length) bad.push(a.id + ' cannot be asked for');
  });
  check(bad.length === 0,
    ages.length + ' degrees of wear, from brand new to abandoned' +
    (bad.length ? ' — ' + bad.join(', ') : ''));

  check(PROMPT.parse('a tower on a hill', { seed: 2 }).age === 0,
    'a tower nobody said anything about is new');
  var worn = PROMPT.parse('a weathered tower on a hill', { seed: 2 }).age;
  var ancient = PROMPT.parse('an ancient tower', { seed: 2 }).age;
  check(worn > 0 && ancient > worn,
    'a weathered one has some wear and an ancient one more (' + worn +
    ' against ' + ancient + ')');
  check(PROMPT.parse('a pristine tower on a hill', { seed: 2 }).age === 0,
    'and a pristine one is new however long it has been there');

  /* Some of these words name a place as well, and are allowed to do both. */
  check(PROMPT.parse('an ancient tower', { seed: 2 }).scene.id === 'ruins',
    'an ancient tower still stands in ancient ruins');

  /* Drawn, not declared. Same tower, same seed, same everything but the age. */
  function marked(age, materialId) {
    var spec = PROMPT.parse('a tower on a hill at noon', { seed: 12 });
    spec.age = age;
    spec.material = materialId
      ? LEX.MATERIALS.filter(function (m) { return m.id === materialId; })[0] : null;
    var P = PAINT.makePalette(spec);
    var PS = PAINT.makePalette(spec, { tintStrength: 0.85 });
    var ctx = recorder(400, 300);
    PAINT.paintSubject(ctx, spec.subject,
      { x: 150, y: 80, w: 70, h: 190, depth: 0, anchor: 'ground' },
      P, PS, PROMPT.rng(spec, 'subject'), spec, { x: 80, y: 30 }, 230, 300);
    return { ctx: ctx, P: P };
  }

  /* Dirt runs downwards, in streaks, because that is the way rain runs — which
   * in a canvas gradient is a lot of stops laid across the shape, alternating
   * between grime and nothing at widths that never repeat. */
  function streaks(bag) {
    return bag.ctx.gradients.filter(function (g) {
      return g.stops.length > 8 && laidDown(bag.ctx, g);
    }).length;
  }
  var neglected = marked(0.88), fresh = marked(0);
  check(streaks(neglected) > 0 && streaks(fresh) === 0,
    'an old tower has dirt running down it and a new one has none');

  /* And growth at its foot, where the damp is — which is green, and which the
   * ground it is standing on is not. */
  function mossHue(bag) {
    var found = null;
    bag.ctx.gradients.forEach(function (g) {
      if (g.stops.length !== 3 || !laidDown(bag.ctx, g)) return;
      var m = /^hsla\(([0-9.]+),([0-9.]+)%,([0-9.]+)%,0\.85\)$/.exec(g.stops[0].colour);
      if (m) found = parseFloat(m[1]);
    });
    return found;
  }
  var hue = mossHue(neglected);
  check(hue !== null && hue > 70 && hue < 150,
    'and moss at its foot, which is green (hue ' + (hue === null ? 'none' : Math.round(hue)) + ')');
  check(mossHue(fresh) === null, 'and a new one has nothing growing on it');

  /* A hundred years of weather takes the shine off anything. */
  function shine(bag) {
    var want = bag.P.css(bag.P.sky.light, 0.95);
    for (var i = 0; i < bag.ctx.log.length; i++) {
      var c = bag.ctx.log[i];
      if (c.op !== 'gradient' || !c.gradient.stops.length) continue;
      if (c.gradient.stops[0].colour !== want) continue;
      for (var j = i + 1; j < bag.ctx.log.length; j++) {
        if (bag.ctx.log[j].op === 'set' && bag.ctx.log[j].args[0] === 'globalAlpha') {
          return bag.ctx.log[j].args[1];
        }
      }
    }
    return 0;
  }
  var polished = shine(marked(0, 'iron')), corroded = shine(marked(0.88, 'iron'));
  check(polished > 0 && corroded < polished * 0.75,
    'polished iron catches the sun much harder than iron left out for a century (' +
    polished.toFixed(2) + ' against ' + corroded.toFixed(2) + ')');
  pass('things have been standing there a while');
})();

/* ------------------------------------------------------------- how much
 * Every word in every table was an on-off switch: a picture was foggy or it
 * was not, a tower was weathered or it was not. English does not work that
 * way, and it is amounts rather than more words that make a vocabulary feel
 * endless.
 */
(function howMuch() {
  console.log('\nHow much of it');

  var degrees = LEX.DEGREES;
  var rising = true;
  for (var i = 1; i < degrees.length; i++) {
    if (degrees[i].factor <= degrees[i - 1].factor) rising = false;
  }
  check(rising && degrees[0].factor < 1 && degrees[degrees.length - 1].factor > 1,
    degrees.length + ' degrees, from barely to impossibly, either side of "just so"');

  function wear(text) { return PROMPT.parse(text, { seed: 2 }).age; }
  var plain = wear('a weathered tower on a hill');
  check(wear('a slightly weathered tower on a hill') < plain,
    'slightly weathered is less weathered than weathered (' +
    wear('a slightly weathered tower on a hill').toFixed(2) + ' against ' +
    plain.toFixed(2) + ')');
  check(wear('a very weathered tower on a hill') > plain,
    'and very weathered is more of it (' +
    wear('a very weathered tower on a hill').toFixed(2) + ')');
  check(wear('an extremely ancient tower') <= 1,
    'and no amount of saying so takes it past completely ruined');

  /* Two-word degrees, because half of them are ("a little", "a bit"). */
  check(wear('a little weathered tower on a hill') < plain,
    '"a little weathered" is read as a degree and not as a small tower');

  /* How much weather. */
  function much(text) { return PROMPT.parse(text, { seed: 2 }).weatherStrength; }
  check(much('a barely foggy meadow') < much('a foggy meadow') &&
        much('a foggy meadow') < much('a very foggy meadow'),
    'barely foggy, foggy and very foggy are three amounts of fog (' +
    [much('a barely foggy meadow'), much('a foggy meadow'), much('a very foggy meadow')]
      .map(function (n) { return n.toFixed(2); }).join(', ') + ')');

  /* And it reaches the picture: more fog is a softer light and a wetter road. */
  function spec(text) { return PROMPT.parse(text, { seed: 2 }); }
  check(PAINT.softness(spec('a very foggy meadow')) >
        PAINT.softness(spec('a barely foggy meadow')),
    'more fog softens the light more');
  check(PAINT.wetness(spec('a heavy downpour over a meadow')) >
        PAINT.wetness(spec('a slightly rainy meadow')),
    'and a downpour leaves the ground wetter than a spot of rain');
  var many = 0, few = 0;
  [['a very rainy meadow at noon', 'many'], ['a barely rainy meadow at noon', 'few']]
    .forEach(function (pair) {
      var sp = PROMPT.parse(pair[0], { seed: 2 });
      var ctx = recorder(300, 220);
      PAINT.render(ctx, 300, 220, sp);
      var n = ctx.log.filter(function (c) { return c.op === 'stroke'; }).length;
      if (pair[1] === 'many') many = n; else few = n;
    });
  check(many > few * 1.5,
    'and there is visibly more rain falling in it (' + many + ' strokes against ' + few + ')');

  /* Size takes a degree too, and it has to push away from ordinary rather than
   * multiply: "very tiny" is smaller than tiny, and multiplying 0.72 by 1.6
   * would make a very tiny dragon a large one. */
  function size(text) { return PROMPT.parse(text, { seed: 2 }).subject.scale; }
  check(size('a very tiny dragon') < size('a tiny dragon'),
    'a very tiny dragon is smaller than a tiny one (' +
    size('a very tiny dragon').toFixed(2) + ' against ' + size('a tiny dragon').toFixed(2) + ')');
  check(size('an impossibly huge dragon') > size('a huge dragon'),
    'and an impossibly huge one is bigger than a huge one (' +
    size('an impossibly huge dragon').toFixed(2) + ' against ' +
    size('a huge dragon').toFixed(2) + ')');
  pass('words have amounts, not just meanings');
})();

/* --------------------------------------------------- things made of parts
 * Everything the painter could draw was something it had a routine for, so the
 * vocabulary was exactly as long as the list of routines and "a winged wolf"
 * was a wolf.
 */
(function madeOfParts() {
  console.log('\nThings a thing can have');

  var list = LEX.PARTS, bad = [];
  var seen = {};
  list.forEach(function (p) {
    if (seen[p.id]) bad.push('two ' + p.id + 's');
    seen[p.id] = true;
    if (!SUBJECTS.PART_DRAW[p.id]) bad.push(p.id + ' has no way of being drawn');
    if (!SUBJECTS.PART_LAYER[p.id]) bad.push(p.id + ' does not know which side of the body it is on');
    if (!p.words || !p.words.length) bad.push(p.id + ' cannot be asked for');
  });
  check(bad.length === 0,
    list.length + ' parts, each drawable and each knowing whether it goes in ' +
    'front of the body or behind it' + (bad.length ? ' — ' + bad.join(', ') : ''));

  check(PROMPT.parse('a wolf in a meadow', { seed: 3 }).parts.length === 0,
    'a wolf has none of them');
  var winged = PROMPT.parse('a winged wolf in a meadow', { seed: 3 });
  check(winged.parts.length === 1 && winged.parts[0] === 'wings' &&
        winged.subject.id === 'wolf',
    'a winged wolf is a wolf with wings, not a new animal');
  var both = PROMPT.parse('an armoured horned bear', { seed: 3 });
  check(both.parts.length === 2 && both.parts.indexOf('armour') >= 0 &&
        both.parts.indexOf('horns') >= 0,
    'and they combine — an armoured horned bear has both');

  /* Drawn, not declared. The same wolf, the same box, the same seed. */
  function drawn(partIds, draw, form) {
    var spec = PROMPT.parse('a wolf in a meadow at noon', { seed: 3 });
    spec.parts = partIds;
    var ctx = recorder(400, 300);
    SUBJECTS.draw(ctx, { draw: draw || 'quadruped', form: form || 'wolf' },
      { x: 120, y: 90, w: 170, h: 130, depth: 0, anchor: 'ground' },
      PAINT.makePalette(spec), PROMPT.rng(spec, 'subject'), spec);
    return { ctx: ctx, shapes: shapesOf(ctx) };
  }

  var bare = drawn([]), flying = drawn(['wings']);
  check(flying.shapes.length > bare.shapes.length,
    'a winged wolf takes more drawing than a wolf (' + flying.shapes.length +
    ' shapes against ' + bare.shapes.length + ')');

  /* Wings come out of the back and go up, so they reach above everything the
   * wolf alone reaches. */
  function above(bag) {
    /* How much reaches above the top of the wolf's own box. Counted rather
     * than measured as a highest point, which one stray mark would decide. */
    return bag.shapes.filter(function (sh) { return sh.top < 90; }).length;
  }
  check(above(flying) > above(bare) + 1,
    'and they reach up above it (' + above(flying) + ' marks over its back ' +
    'against ' + above(bare) + ')');

  /* Which side of the body. A wing comes out of the far shoulder as often as
   * the near one, so it goes behind; a horn never does. */
  function firstExtra(partId) {
    var bag = drawn([partId]);
    var base = drawn([]);
    /* The body's marks are the same in both, so the first place the two traces
     * differ is where the extra part was drawn. */
    var a = bag.shapes, b = base.shapes;
    for (var i = 0; i < Math.min(a.length, b.length); i++) {
      if (a[i].x !== b[i].x || a[i].y !== b[i].y) return i;
    }
    return Math.min(a.length, b.length);
  }
  check(firstExtra('wings') === 0,
    'wings are laid down before the body, so the body sits in front of them');
  check(firstExtra('horns') > 0,
    'and horns after it, so they sit in front of the head');

  /* Any part on any subject: they are parts, not creatures. */
  var wingedTower = drawn(['wings'], 'tower');
  var plainTower = drawn([], 'tower');
  check(wingedTower.shapes.length > plainTower.shapes.length,
    'a tower can have wings too, because nothing here is a special case');

  /* Where a part goes is read off the body it is going on: a wolf's head is
   * not where a person's is. */
  function headAt(draw) {
    var bag = drawn(['horns'], draw, draw === 'quadruped' ? 'wolf' : undefined);
    var base = drawn([], draw, draw === 'quadruped' ? 'wolf' : undefined);
    var extra = bag.shapes.slice(base.shapes.length);
    if (!extra.length) return null;
    return extra.reduce(function (a, sh) { return a + sh.x; }, 0) / extra.length;
  }
  var onBeast = headAt('quadruped'), onPerson = headAt('humanoid');
  check(onBeast !== null && onPerson !== null && Math.abs(onBeast - onPerson) > 20,
    'horns land on a wolf\'s head and on a person\'s, which are not the same ' +
    'place (' + Math.round(onBeast) + ' against ' + Math.round(onPerson) + ')');

  /* A halo is light rather than shape, so it sits out the passes that redraw
   * the subject flat — otherwise the shadow of a haloed stag has a ring in it. */
  var spec = PROMPT.parse('a haloed stag in snow at dusk', { seed: 3 });
  spec.parts = ['halo'];
  function haloMarks(stencilOn) {
    var ctx = recorder(400, 300);
    SUBJECTS.setStencil(stencilOn);
    SUBJECTS.draw(ctx, { draw: 'quadruped', form: 'deer' },
      { x: 120, y: 90, w: 170, h: 130, depth: 0, anchor: 'ground' },
      PAINT.makePalette(spec), PROMPT.rng(spec, 'subject'), spec);
    SUBJECTS.setStencil(false);
    return shapesOf(ctx).length;
  }
  check(haloMarks(false) > haloMarks(true),
    'a halo is left out of the flat passes, so the stag\'s shadow has no ring in it');
  /* Parts belong to the thing the sentence is about. "A haloed knight by a
   * campfire" is a knight with a halo standing beside an ordinary fire. */
  var pair = PROMPT.parse('a haloed knight by a campfire at night', { seed: 3 });
  check(!!pair.companion && pair.parts.indexOf('halo') >= 0,
    'a haloed knight by a campfire has both a knight and a fire in it');
  /* Counted by the ring itself: a flat ellipse stroked in the light's own
   * colour, which nothing else in the picture draws. */
  function haloes(spec) {
    var ctx = recorder(480, 360);
    PAINT.render(ctx, 480, 360, spec);
    var ring = PAINT.makePalette(spec).light(0.85);
    return shapesOf(ctx).filter(function (sh) {
      return !sh.filled && sh.colour === ring && sh.w > 8 && sh.h < sh.w * 0.6;
    }).length;
  }
  var withFire = haloes(pair);
  var alone = {};
  for (var k in pair) alone[k] = pair[k];
  alone.companion = null;
  check(withFire > 0 && withFire === haloes(alone),
    'and only the knight is wearing it — taking the fire out of the picture ' +
    'changes nothing about the halo (' + withFire + ' rings either way)');
  pass('parts go on anything, and combine');
})();

/* ------------------------------------------- words it was never taught
 * The vocabulary is a list, and anything off the list was thrown away and
 * reported back as a word nobody knew. But most words nobody taught it are
 * made out of words it does know, and working that out is the difference
 * between a fixed list and a language.
 */
(function wordsNeverTaught() {
  console.log('\nWords it was never taught');

  /* Every single word in every table, so the examples used here can be shown
   * to be ones it really was never taught — "snowy" and "foggy" look like good
   * tests and are both in the tables already. */
  var taught = {};
  Object.keys(LEX).forEach(function (name) {
    var table = LEX[name];
    if (!table || !table.forEach) return;
    table.forEach(function (entry) {
      ((entry && entry.words) || []).forEach(function (w) {
        if (w.indexOf(' ') < 0) taught[w] = true;
      });
    });
  });
  var examples = ['stony', 'dragonlike', 'wolfish', 'snowbird'];
  var slipped = examples.filter(function (w) { return taught[w]; });
  check(slipped.length === 0,
    'the words tested here are ones nobody taught it' +
    (slipped.length ? ' — but ' + slipped.join(', ') + ' already is' : ''));

  function read(text) { return PROMPT.parse(text, { seed: 3 }); }
  function idOf(thing) { return thing ? thing.id : null; }

  /* An ending taken off. */
  check(idOf(read('a stony tower').material) === 'stone',
    '"stony" is stone');
  check(idOf(read('a dragonlike beast in a meadow').subject) === 'dragon',
    'and "dragonlike" is a dragon');
  check(idOf(read('a wolfish shape in snow').subject) === 'wolf',
    'and "wolfish" is a wolf');

  /* Or two words run together. */
  var snowbird = read('a snowbird over the sea');
  check(idOf(snowbird.subject) === 'bird' && snowbird.scene.id === 'snow',
    '"snowbird" is snow and a bird, which nobody taught it either');

  /* It cannot invent a meaning: everything it arrives at is a word already in
   * the tables, so a derived word paints what the word it came from paints. */
  check(read('a florble in a meadow').unknown.indexOf('florble') >= 0,
    'and a word that is not made of anything is still reported as unknown');
  check(read('a gzhqwy thing in a meadow').unknown.length > 0,
    'including one that merely ends like a word it knows');

  /* A word it knows but did not use is not a word nobody knew: "a snowy
   * mountain" is one setting or the other, and whichever loses is still
   * English. */
  check(read('a snowy mountain').unknown.length === 0,
    'a snowy mountain leaves nothing unaccounted for, whichever setting wins');

  /* The readout shows the person their own word and what it was taken to
   * mean, rather than a word they never typed. */
  var said = read('a wolfish shape in snow').read.filter(function (item) {
    return item.word === 'wolfish';
  })[0];
  check(said && said.meant === 'wolf',
    'and it says so: "wolfish", read as wolf');

  /* And a derived word sits where its own word sat, so a degree word in front
   * of it still applies to it. */
  check(PROMPT.parse('a very stony tower', { seed: 3 }).age ===
        PROMPT.parse('a stony tower', { seed: 3 }).age,
    'a degree word in front of a derived word is not read as belonging to ' +
    'something else in the sentence');
  pass('it works out words nobody taught it, out of words it knows');
})();

/* --------------------------------------------------- air with something in it
 * Light is invisible. You only ever see it where it hits something, and in
 * clear air between you and the sun there is nothing for it to hit — which is
 * why a shaft of light is a sign that the air is full of water or dust.
 */
(function beams() {
  console.log('\nBeams through air that has something in it');

  function beamsFor(weather, text) {
    var spec = PROMPT.parse(text || 'a forest at mid-morning', { seed: 7 });
    if (weather) spec.weather = weather;
    spec.weatherStrength = 1;
    var P = PAINT.makePalette(spec);
    var ctx = recorder(480, 360);
    PAINT.shafts(ctx, 480, 360, 200, P, spec, PROMPT.rng(spec, 'shafts'), { x: 120, y: 40 });
    var shapes = shapesOf(ctx);
    return { shapes: shapes, ctx: ctx, P: P };
  }

  var misty = beamsFor('fog'), clean = beamsFor('clear');
  check(misty.shapes.length >= 7,
    'mist puts ' + misty.shapes.length + ' beams in the air');
  check(clean.shapes.length === 0,
    'and clear air puts none there, because there is nothing for the light to hit');

  /* Every one of them starts at the light. A beam that starts anywhere else is
   * a stripe. */
  var away = misty.shapes.filter(function (sh) {
    return Math.abs(sh.x - 120) > 400 && Math.abs(sh.y - 40) > 400;
  });
  check(away.length === 0, 'and every one of them comes out of the sun');

  /* Light adds to what is behind it. */
  var additive = misty.ctx.log.filter(function (c) {
    return c.op === 'set' && c.args[0] === 'globalCompositeOperation' && c.args[1] === 'lighter';
  });
  check(additive.length > 0, 'and it is light, added to the picture rather than painted over it');

  /* Nothing to shine through once the sun has gone, and nothing underground. */
  function strength(sun) {
    var spec = PROMPT.parse('a forest at mid-morning', { seed: 7 });
    spec.weather = 'fog';
    spec.weatherStrength = 1;
    spec.sun = sun;
    var ctx = recorder(480, 360);
    PAINT.shafts(ctx, 480, 360, 200, PAINT.makePalette(spec), spec,
      PROMPT.rng(spec, 'shafts'), { x: 120, y: 40 });
    /* How bright the beams were laid down, read off the first stop of each. */
    var most = 0;
    ctx.gradients.forEach(function (g) {
      if (!g.stops.length) return;
      var m = /,\s*([0-9.]+)\s*\)$/.exec(g.stops[0].colour);
      if (m) most = Math.max(most, parseFloat(m[1]));
    });
    return most;
  }
  check(strength(10) > strength(-45),
    'a low sun through mist makes beams and a sun that has set does not (' +
    strength(10).toFixed(3) + ' against ' + strength(-45).toFixed(3) + ')');

  var cave = beamsFor('fog', 'a crystal in a cave');
  check(cave.shapes.length === 0, 'and there is no sun underground');
  pass('the air has something in it, so the light shows');
})();

/* ----------------------------------------------------------- water as a mirror
 * Water was a wash of sea colour, which is the wrong way round twice over:
 * water far off is seen at a grazing angle and is almost all reflected sky,
 * and water near you is looked into rather than off, so it is dark.
 */
(function waterMirror() {
  console.log('\nWater, which is mostly a mirror');

  /* At sunset, because that is when the difference shows: the sky's low band
   * goes orange while the sea's own colour stays blue, so a water that merely
   * hazes its own colour into the distance comes out blue under an orange sky
   * and one that reflects comes out orange. At noon both are blue and the
   * question cannot be asked. */
  var spec = PROMPT.parse('the open sea at sunset', { seed: 4 });
  var P = PAINT.makePalette(spec);
  var ctx = recorder(480, 360);
  PAINT.GROUND.ocean(ctx, 480, 360, 180, P, spec, PROMPT.rng(spec, 'ground'),
    { x: 200, y: 40, r: 20 });

  /* The far band is mixed from the sky; the near band is the water itself. */
  var wash = null;
  ctx.log.forEach(function (c) {
    if (c.op === 'gradient' && c.args.length === 4 && c.gradient.stops.length >= 3 &&
        laidDown(ctx, c.gradient) && !wash) {
      wash = c.gradient.stops;
    }
  });
  function hueOf(colour) {
    var m = /^hsla?\(([0-9.]+)/.exec(colour);
    return m ? parseFloat(m[1]) : null;
  }
  function apart(a, b) { return Math.abs(((a - b) % 360 + 540) % 360 - 180); }
  var far = wash ? hueOf(wash[0].colour) : null;
  var near = wash ? hueOf(wash[wash.length - 1].colour) : null;
  var skyHue = P.sky.low[0], seaHue = (P.scene.sea || P.scene.far)[0];
  check(far !== null && apart(far, skyHue) < 20,
    'the far water is the colour of the sky, because that is what is coming ' +
    'back off it (water ' + (far === null ? '-' : far.toFixed(0)) + ', sky ' +
    skyHue.toFixed(0) + ')');
  check(near !== null && apart(near, seaHue) < apart(far, seaHue),
    'and the near water is more its own colour, because you are looking into ' +
    'it rather than off it (near ' + (near === null ? '-' : near.toFixed(0)) +
    ', sea ' + seaHue.toFixed(0) + ')');

  /* And the reflection is broken, more so as the water comes towards you. */
  var skyColour = P.css(P.sky.low, 1);
  var pieces = [], alphas = [];
  var held = 1, fill = null;
  ctx.log.forEach(function (c) {
    if (c.op === 'set' && c.args[0] === 'globalAlpha') held = c.args[1];
    else if (c.op === 'set' && c.args[0] === 'fillStyle') fill = c.args[1];
    else if (c.op === 'fillRect' && fill === skyColour) {
      pieces.push(c.args[1]);
      alphas.push({ y: c.args[1], a: held });
    }
  });
  check(pieces.length > 10,
    'the reflection comes back in ' + pieces.length + ' pieces, not one sheet');
  var far = alphas.filter(function (p) { return p.y < 180 + (360 - 180) * 0.35; });
  var near = alphas.filter(function (p) { return p.y > 180 + (360 - 180) * 0.65; });
  function mean(list) {
    return list.length ? list.reduce(function (a, p) { return a + p.a; }, 0) / list.length : 0;
  }
  check(far.length && near.length && mean(far) > mean(near) * 2,
    'and it fades as the water comes towards you and the angle steepens (' +
    mean(far).toFixed(3) + ' against ' + mean(near).toFixed(3) + ')');

  /* A pebble lying on the open sea is not a small thing lying about, it is a
   * mistake. On a shore the sand starts well down the frame. */
  var shore = PROMPT.parse('a lighthouse on the shore at noon', { seed: 4 });
  var sctx = recorder(480, 360);
  PAINT.scatter(sctx, 480, 360, 180, PAINT.makePalette(shore), shore,
    PROMPT.rng(shore, 'scatter'));
  var sand = 180 + (360 - 180) * 0.62;
  var afloat = shapesOf(sctx).filter(function (sh) { return sh.bottom < sand - 2; });
  check(shapesOf(sctx).length > 10 && afloat.length === 0,
    'and nothing is lying about on the water in front of a shore' +
    (afloat.length ? ' — ' + afloat.length + ' pieces are afloat' : ''));
  pass('water gives the sky back, in pieces');
})();

/* -------------------------------------------- exposure behaving like exposure
 * Paint stops at white and a camera does not. Film and sensors both roll off:
 * as a highlight gets brighter the response flattens, so the bright end of a
 * photograph crowds together instead of arriving at pure white all at once and
 * going flat.
 */
(function exposure() {
  console.log('\nExposure behaving like exposure');

  function ramp(n) {
    var d = new Uint8ClampedArray(n * 4);
    for (var i = 0; i < n; i++) {
      var v = Math.round((i / (n - 1)) * 255);
      d[i * 4] = d[i * 4 + 1] = d[i * 4 + 2] = v;
      d[i * 4 + 3] = 255;
    }
    return { data: d, width: n, height: 1 };
  }

  var n = 64;
  function exposed(text) {
    var img = ramp(n);
    var spec = PROMPT.parse(text, { seed: 2 });
    FINISH.helpers.expose(img, n, 1, spec, PAINT.makePalette(spec));
    var out = [];
    for (var i = 0; i < n; i++) out.push(img.data[i * 4]);
    return out;
  }

  var noon = exposed('a wolf in snow at high noon');
  /* The top end crowds together: the last few steps of the ramp are closer to
   * each other than the middle ones, which is the roll-off. */
  /* Measured over eight steps rather than two: a ramp of whole numbers rounds,
   * so two steps can differ by one for no reason at all and a check reading
   * them would pass on a picture with no roll-off in it. */
  var topStep = noon[n - 1] - noon[n - 9];
  var midStep = noon[Math.floor(n / 2)] - noon[Math.floor(n / 2) - 8];
  check(topStep < midStep * 0.7,
    'the bright end crowds together rather than climbing straight to white (' +
    topStep + ' across the top eight steps against ' + midStep + ' in the middle)');
  check(noon[n - 1] === 255,
    'and white is still white — a roll-off that never reaches it is a picture ' +
    'with the whites turned down (' + noon[n - 1] + ')');

  /* Nothing may go backwards: this is exposure, not a filter. */
  var falls = 0;
  for (var i = 1; i < n; i++) if (noon[i] < noon[i - 1]) falls++;
  check(falls === 0, 'and brighter is still brighter all the way up the ramp');

  /* A night picture is a long exposure: the shadows come up while the
   * highlights stay where they are. */
  var night = exposed('a wolf in snow at midnight');
  check(night[4] > noon[4] + 4,
    'a long exposure lifts the shadows (' + night[4] + ' against ' + noon[4] + ')');
  /* Read below the very top, where both are pinned to white anyway and any
   * lift at all would be hidden by the ceiling. */
  check(Math.abs(night[n - 8] - noon[n - 8]) < 8,
    'and leaves the highlights roughly where they were (' + night[n - 8] +
    ' against ' + noon[n - 8] + ')');

  /* A lens leaks: light from a bright patch scatters in the glass and spills
   * into whatever is beside it, which is why the sun in a photograph has a
   * size and the sun in a drawing does not. */
  var w = 33, h = 33;
  var spot = { data: new Uint8ClampedArray(w * h * 4), width: w, height: h };
  for (var q = 0; q < w * h; q++) {
    spot.data[q * 4 + 3] = 255;
    var x = q % w, y = Math.floor(q / w);
    var mid = Math.abs(x - 16) < 2 && Math.abs(y - 16) < 2;
    spot.data[q * 4] = spot.data[q * 4 + 1] = spot.data[q * 4 + 2] = mid ? 255 : 20;
  }
  var edge = (16 * w + 22) * 4;
  var was = spot.data[edge];
  FINISH.helpers.bloom(spot, w, h, 205, 5, 0.6);
  check(spot.data[edge] > was + 2,
    'a bright patch spills into what is next to it (' + was + ' to ' +
    spot.data[edge] + ' six pixels away)');
  /* Black-and-white film has no colour in it, and the camera comes before the
   * film: the daylight pass tints shadows blue and highlights warm, and the
   * lens then spreads those warm highlights over everything beside them, so a
   * noir picture came out of the camera with a faint cast on it. */
  function colourLeft(style) {
    var ctx = new FakeContext(120, 90);
    var spec = PROMPT.parse('a castle in the mountains at dusk', { seed: 4 });
    spec.style = style;
    spec.styles = [style];
    var P = PAINT.makePalette(spec);
    PAINT.render(ctx, 120, 90, spec);
    FINISH.apply(ctx, 120, 90, spec, P);
    var d = ctx.getImageData(0, 0, 120, 90).data;
    var coloured = 0, n = 0;
    for (var i = 0; i < d.length; i += 4) {
      if (Math.abs(d[i] - d[i + 1]) > 8 || Math.abs(d[i + 1] - d[i + 2]) > 8) coloured++;
      n++;
    }
    return coloured / n;
  }
  check(colourLeft('noir') < 0.01,
    'a film-noir picture comes out with no colour in it at all (' +
    (colourLeft('noir') * 100).toFixed(1) + '% left)');
  check(colourLeft('oil') > 0.05,
    'and an ordinary one still has its colour (' +
    (colourLeft('oil') * 100).toFixed(1) + '%)');
  pass('the bright end rolls off, the shadows come up at night, and the lens leaks');
})();

/* ------------------------------------------------ weather landing on things
 * Weather was painted in front of everything and then stopped. It never
 * landed: a thing standing out in the rain was perfectly dry, and a thing
 * standing in falling snow had nothing on it.
 */
(function weatherLands() {
  console.log('\nWeather landing on the things in the picture');

  function standing(weather) {
    var spec = PROMPT.parse('a knight on the plains at noon', { seed: 5 });
    spec.weather = weather;
    spec.weatherStrength = 1;
    var P = PAINT.makePalette(spec);
    var PS = PAINT.makePalette(spec, { tintStrength: 0.85 });
    var ctx = recorder(400, 300);
    PAINT.paintSubject(ctx, spec.subject,
      { x: 160, y: 90, w: 80, h: 170, depth: 0, anchor: 'ground' },
      P, PS, PROMPT.rng(spec, 'subject'), spec, { x: 60, y: 30 }, 200, 300);
    return { ctx: ctx, P: P, shapes: shapesOf(ctx) };
  }

  /* Wet is darker and deeper, and it runs down — so the bottom of a thing
   * standing in the rain is wetter than its top. */
  function soaked(bag) {
    var want = bag.P.css(bag.P.scene.ink, 0.62);
    return bag.ctx.gradients.some(function (g) {
      return g.stops.length && g.stops[0].colour === want && laidDown(bag.ctx, g);
    });
  }
  var rained = standing('rain'), dry = standing('clear');
  check(soaked(rained), 'a knight standing in the rain is wet');
  check(!soaked(dry), 'and one standing in the sun is not');

  /* And a wet surface is a mirror, so the sky comes off it. */
  function slick(bag) {
    var want = bag.P.css(bag.P.sky.low, 0.55);
    return bag.ctx.gradients.some(function (g) {
      return g.stops.length && g.stops[0].colour === want && laidDown(bag.ctx, g);
    });
  }
  check(slick(rained) && !slick(dry), 'and the sky comes off him, because wet is a mirror');

  /* Snow gathers on whatever faces upwards. One clean line of it reads as a
   * hat, so it goes down at three heights. */
  /* Counted by how many different heights it was laid at, not by how many
   * marks are white: one pass redraws the whole knight in white and puts down
   * a dozen marks, so counting marks cannot tell one pass from three. */
  function capped(bag) {
    var white = bag.P.css([205, 18, 97], 1);
    var heights = {}, lastShift = null;
    bag.ctx.log.forEach(function (c) {
      if (c.op === 'translate') lastShift = Math.round(c.args[1] * 10) / 10;
      else if (c.op === 'set' && c.args[0] === 'fillStyle' && c.args[1] === white &&
               lastShift !== null && lastShift < 0) {
        heights[lastShift] = true;
      }
    });
    return Object.keys(heights).length;
  }
  var snowed = standing('snowfall');
  check(capped(snowed) >= 3,
    'snow settles on top of him at ' + capped(snowed) + ' different heights, ' +
    'because one clean line of it reads as a hat');
  check(capped(dry) === 0, 'and none settles on him in the sun');

  /* It is the amount of weather, not merely the name of it. */
  function wetAlpha(weather, strength) {
    var spec = PROMPT.parse('a knight on the plains at noon', { seed: 5 });
    spec.weather = weather;
    spec.weatherStrength = strength;
    var P = PAINT.makePalette(spec);
    var ctx = recorder(400, 300);
    PAINT.paintSubject(ctx, spec.subject,
      { x: 160, y: 90, w: 80, h: 170, depth: 0, anchor: 'ground' },
      P, PAINT.makePalette(spec, { tintStrength: 0.85 }),
      PROMPT.rng(spec, 'subject'), spec, { x: 60, y: 30 }, 200, 300);
    var want = P.css(P.scene.ink, 0.62);
    for (var i = 0; i < ctx.log.length; i++) {
      var c = ctx.log[i];
      if (c.op !== 'gradient' || !c.gradient.stops.length) continue;
      if (c.gradient.stops[0].colour !== want) continue;
      for (var j = i + 1; j < ctx.log.length; j++) {
        if (ctx.log[j].op === 'set' && ctx.log[j].args[0] === 'globalAlpha') {
          return ctx.log[j].args[1];
        }
      }
    }
    return 0;
  }
  check(wetAlpha('rain', 1.4) > wetAlpha('rain', 0.6),
    'a downpour soaks him more than a drizzle does (' +
    wetAlpha('rain', 1.4).toFixed(3) + ' against ' + wetAlpha('rain', 0.6).toFixed(3) + ')');
  pass('the weather happens to the picture rather than in front of it');
})();

/* ---------------------------------------------------- composition on purpose
 * Dead centre is the one place a photographer never puts the skyline, and a
 * head that lands exactly on it reads as stuck to it. Both were happening by
 * accident, because nothing was deciding either.
 */
(function composed() {
  console.log('\nA picture framed on purpose');

  /* Asked of the painter itself rather than of a copy of its rule kept here —
   * a test that reimplements the thing it is testing passes whatever the
   * painter does. The wobble is handed in so the boundaries can be asked
   * about directly instead of waiting for a seed to land on one. */
  function where(horizon) {
    return PAINT.skylineAt({ scene: { horizon: horizon } }, null, 0);
  }

  check(Math.abs(where(0.50) - 0.5) > 0.08,
    'a skyline that lands dead centre is moved off it (' + where(0.50).toFixed(2) + ')');
  check(where(0.46) < 0.42 && where(0.54) > 0.58,
    'and pushed to whichever third it was already nearer (' +
    where(0.46).toFixed(2) + ' and ' + where(0.54).toFixed(2) + ')');
  check(where(0.30) === 0.30 && where(0.72) === 0.72,
    'while one that was already well off centre is left where the setting put it');

  /* Across every setting the painter knows, with the wobble at both ends of
   * its range, nothing may cut the frame in half. */
  var middling = [];
  LEX.SCENES.forEach(function (scene) {
    [-0.5, 0, 0.5].forEach(function (wobble) {
      var at = PAINT.skylineAt({ scene: scene }, null, wobble);
      if (at > 0.45 && at < 0.55) middling.push(scene.id + ' ' + at.toFixed(2));
    });
  });
  check(middling.length === 0,
    'and no setting at all puts it across the middle of the frame' +
    (middling.length ? ' — ' + middling.join(', ') : ''));

  /*
   * And nothing may end on the skyline.
   *
   * Swept across where the skyline can actually fall rather than at one
   * height: at a single height a tangent almost never comes up by chance, so a
   * check standing there passes whether the rule exists or not. Over the whole
   * range there are plenty of them to avoid.
   */
  var tangents = 0, checked = 0;
  for (var s2 = 0; s2 < 60; s2++) {
    var sp = PROMPT.parse('a wolf in a meadow at noon', { seed: s2 });
    for (var line = 0.34; line <= 0.78; line += 0.02) {
      var hz = 300 * line;
      var box = PAINT.placeBox(400, 300, hz, sp, sp.subject, 0, 1,
        PROMPT.rng(sp, 'subject'));
      if (box.anchor !== 'ground') continue;
      checked++;
      if (Math.abs(box.y - hz) < 300 * 0.03) tangents++;
    }
  }
  check(checked > 500 && tangents === 0,
    'and across ' + checked + ' placements no subject ends on the skyline' +
    (tangents ? ' — ' + tangents + ' do' : ''));
  pass('the skyline is put somewhere on purpose, and nothing is stuck to it');
})();

console.log('\n' + (failures ? 'FAILED ' + failures + ' of ' + checks + ' checks'
  : 'All ' + checks + ' checks passed'));
process.exit(failures ? 1 : 0);
