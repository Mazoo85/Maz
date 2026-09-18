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
    var put = PAINT.clouds(ctx, 480, 360, 200, P, spec,
      PROMPT.rng(spec, 'cloud'), light || { x: 60, y: 20 });

    return { shapes: shapesOf(ctx), P: P, ctx: ctx, put: put };
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

  /*
   * A cloud low in the frame is not a low cloud, it is the same cloud further
   * off — so it is smaller.
   *
   * Measured on the bodies rather than on every shape. A cloud is built as a
   * lump with lumps on it and lumps on those, so most of the shapes in any
   * band are the smallest ones, and those come out about the same size
   * wherever they are — averaging the lot said an overhead cloud and a distant
   * one were both eight pixels across. The widest quarter of each band is the
   * clouds themselves.
   */
  function mean(list) {
    return list.length ? list.reduce(function (a, b) { return a + b; }, 0) / list.length : 0;
  }
  var overcast = sky('clouds');
  var high = overcast.put.filter(function (c) { return c.far < 0.45; });
  var low = overcast.put.filter(function (c) { return c.far > 0.6; });
  check(high.length > 0 && low.length > 0,
    'with clouds both overhead and down by the skyline (' + high.length +
    ' and ' + low.length + ')');
  var overhead = mean(high.map(function (c) { return c.rx; }));
  var distant = mean(low.map(function (c) { return c.rx; }));
  check(overhead > distant * 1.3,
    'clouds near the horizon are smaller than the ones overhead (' +
    Math.round(distant) + 'px against ' + Math.round(overhead) + 'px)');

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

/* ------------------------------------------------- variation within a colour
 * Nothing in the world is one flat colour over any distance: a wall is
 * lighter where the sun has bleached it, a field is a hundred greens. Every
 * large fill here was one exact colour from edge to edge, and that flatness
 * reads as paint however well the shape is drawn.
 */
(function withinAColour() {
  console.log('\nVariation within a colour');

  var w = 120, h = 90;
  function flat(v) {
    var img = { data: new Uint8ClampedArray(w * h * 4), width: w, height: h };
    for (var i = 0; i < w * h; i++) {
      img.data[i * 4] = img.data[i * 4 + 1] = img.data[i * 4 + 2] = v;
      img.data[i * 4 + 3] = 255;
    }
    return img;
  }

  var img = flat(128);
  /* At the amount the finisher actually uses, so that turning it up to a
   * stain is caught here rather than only in front of somebody's eyes. */
  FINISH.helpers.mottle(img, w, h,
    PROMPT.rng(PROMPT.parse('a wall', { seed: 3 }), 'mottle'), FINISH.helpers.DRIFT);

  var lo = 255, hi = 0, sum = 0;
  for (var i = 0; i < w * h; i++) {
    var v = img.data[i * 4];
    lo = Math.min(lo, v); hi = Math.max(hi, v); sum += v;
  }
  check(hi - lo >= 6,
    'a flat grey wall is no longer flat (' + lo + ' to ' + hi + ')');
  check(hi - lo < 40,
    'but it is still the same wall — a drift, not a stain (' + (hi - lo) + ' apart)');
  check(Math.abs(sum / (w * h) - 128) < 6,
    'and it is no lighter or darker overall (' + (sum / (w * h)).toFixed(1) + ')');

  /* Smooth, not speckled. Grain is the small scale and already exists; this is
   * the slow drift, so neighbours must be close even where far-apart pixels
   * are not — and there must be no seam where the cells of it meet. */
  var biggestStep = 0;
  for (var y = 0; y < h; y++) {
    for (var x = 1; x < w; x++) {
      var a = img.data[(y * w + x) * 4], b = img.data[(y * w + x - 1) * 4];
      biggestStep = Math.max(biggestStep, Math.abs(a - b));
    }
  }
  check(biggestStep <= 2,
    'and it drifts rather than speckles — no two neighbours differ by more ' +
    'than ' + biggestStep + ', while the picture spans ' + (hi - lo));
  pass('no large fill is one exact colour any more');
})();

/* ------------------------------------------------------------- the glass
 * Two things every lens does and no painter has to: it darkens the corners,
 * and it brings red and blue to focus at slightly different sizes so the
 * corners pick up a faint coloured fringe. Their absence is part of why a
 * drawn picture looks evenly lit in a way nothing photographed ever is.
 */
(function theGlass() {
  console.log('\nWhat the glass itself does');

  /* A picture the size of a real one. The fringe is a share of the distance
   * from the middle, the way a lens's is, so on a thumbnail it is a fraction
   * of a pixel and there is nothing to measure — a check run on a small canvas
   * would report no fringe from code that has one. */
  var w = 481, h = 321;
  function flat(v) {
    var img = { data: new Uint8ClampedArray(w * h * 4), width: w, height: h };
    for (var i = 0; i < w * h; i++) {
      img.data[i * 4] = img.data[i * 4 + 1] = img.data[i * 4 + 2] = v;
      img.data[i * 4 + 3] = 255;
    }
    return img;
  }
  function at(img, x, y) { return img.data[(y * w + x) * 4 + 1]; }

  var even = flat(180);
  FINISH.helpers.lens(even, w, h, FINISH.helpers.GLASS);
  var middle = at(even, 240, 160), corner = at(even, 1, 1);
  check(corner < middle - 8,
    'the corners of the frame are darker than the middle (' + corner +
    ' against ' + middle + ')');
  check(middle === 180,
    'and the middle is exactly where it was, because that is where the light ' +
    'goes straight through (' + middle + ')');

  /* The fringe. A hard black-and-white edge picks one up out at the corner and
   * none of one in the middle, which is what makes it read as the lens rather
   * than as a filter over the picture. */
  function edged() {
    var img = flat(0);
    for (var y = 0; y < h; y++) {
      for (var x = 0; x < w; x++) {
        var v = (x % 8) < 4 ? 255 : 0;
        var i = (y * w + x) * 4;
        img.data[i] = img.data[i + 1] = img.data[i + 2] = v;
      }
    }
    return img;
  }
  /* How much of a region is fringed, rather than how strong the worst pixel
   * is: a fringe either crosses a stripe edge or it does not, so the strength
   * is always the same and only the amount of it changes. */
  function fringeIn(img, x0, x1, y0, y1) {
    var hit = 0, n = 0;
    for (var y = y0; y < y1; y++) {
      for (var x = x0; x < x1; x++) {
        var i = (y * w + x) * 4;
        if (Math.abs(img.data[i] - img.data[i + 2]) > 30) hit++;
        n++;
      }
    }
    return hit / n;
  }
  var stripes = edged();
  FINISH.helpers.lens(stripes, w, h, FINISH.helpers.GLASS);
  var outer = fringeIn(stripes, 0, 40, 0, 30);
  var halfway = fringeIn(stripes, 140, 200, 90, 130);
  var inner = fringeIn(stripes, 232, 248, 152, 168);
  check(outer > 0.1,
    'a hard edge out at the corner picks up a coloured fringe (' +
    Math.round(outer * 100) + '% of it)');
  check(inner === 0, 'and the same edge in the middle picks up none');
  /* And it grows towards the edges rather than being spread evenly, which is
   * the difference between a lens and a filter laid over the picture. */
  check(halfway < outer * 0.1,
    'and it grows towards the corner rather than sitting evenly over the ' +
    'picture (' + Math.round(halfway * 100) + '% halfway out against ' +
    Math.round(outer * 100) + '% at the corner)');

  /* Enough to be there, not enough to notice. */
  check(FINISH.helpers.GLASS > 0 && FINISH.helpers.GLASS <= 0.6,
    'and the glass is set to show rather than to be looked at (' +
    FINISH.helpers.GLASS + ')');
  pass('every picture is taken through a piece of glass');
})();

/* --------------------------------------------------- distance is one number
 * On a flat piece of ground, how far away a thing is shows twice: once as how
 * far its feet are below the skyline, and once as how big it is. Those cannot
 * disagree — and they did, so the further members of a group were drawn
 * smaller and stood lower in the frame, which is a smaller thing standing
 * nearer the camera, and reads as a toy rather than as distance.
 */
(function oneDistance() {
  console.log('\nHow far away a thing is, said once');

  var hz = 170, w = 400, h = 300;
  var wrong = [], groups = 0;
  ['three wolves in a meadow at noon', 'five pines in the snow at dusk',
   'four ships at sea at noon', 'several deer on the plains at dawn']
    .forEach(function (text) {
      for (var seed = 0; seed < 30; seed++) {
        var spec = PROMPT.parse(text, { seed: seed });
        if (!spec.subject || spec.subject.count < 2) continue;
        var r = PROMPT.rng(spec, 'subject');
        var placed = [];
        for (var i = spec.subject.count - 1; i >= 0; i--) {
          placed.push(PAINT.placeBox(w, h, hz, spec, spec.subject, i,
            spec.subject.count, r));
        }
        if (placed[0].anchor === 'sky') continue;
        groups++;
        /* Sorted by size, the feet must be in the same order: bigger is
         * nearer, and nearer is further down the ground. */
        var bySize = placed.slice().sort(function (a, b) { return a.h - b.h; });
        for (var k = 1; k < bySize.length; k++) {
          var lower = (bySize[k].y + bySize[k].h) - hz;
          var higher = (bySize[k - 1].y + bySize[k - 1].h) - hz;
          if (lower < higher - 0.5) {
            wrong.push(text.split(' ')[1] + ' seed ' + seed);
            break;
          }
        }
      }
    });
  check(groups > 20, groups + ' groups of things standing about to look at');

  check(wrong.length === 0,
    'in every one of them, the bigger a thing is drawn the further down the ' +
    'ground it stands' + (wrong.length ? ' — but not in ' + wrong.slice(0, 4).join(', ') : ''));

  /* Agreeing is not enough on its own: a group that is all one size at all one
   * distance agrees perfectly and has no depth in it at all. They have to
   * spread out — on the water as much as on the ground, since a boat obeys the
   * same rule as a stag. */
  var flat = [];
  ['three wolves in a meadow at noon', 'four ships at sea at noon']
    .forEach(function (text) {
      for (var seed = 0; seed < 30; seed++) {
        var sp = PROMPT.parse(text, { seed: seed });
        if (!sp.subject || sp.subject.count < 2) continue;
        var rr = PROMPT.rng(sp, 'subject');
        var sizes = [], feet = [];
        for (var i = sp.subject.count - 1; i >= 0; i--) {
          var b = PAINT.placeBox(w, h, hz, sp, sp.subject, i, sp.subject.count, rr);
          if (b.anchor === 'sky') continue;
          sizes.push(b.h);
          feet.push((b.y + b.h) - hz);
        }
        if (sizes.length < 2) continue;
        var big = Math.max.apply(null, sizes), small = Math.min.apply(null, sizes);
        var near = Math.max.apply(null, feet), off = Math.min.apply(null, feet);
        if (big < small * 1.25 || near < off * 1.25) {
          flat.push(text.split(' ')[1] + ' seed ' + seed);
        }
      }
    });
  check(flat.length === 0,
    'and each group is spread through the picture rather than standing in a ' +
    'line at one distance' + (flat.length ? ' — ' + flat.slice(0, 4).join(', ') : ''));

  /* And the two move together rather than merely agreeing in order: half the
   * size means half the distance below the skyline. */
  var spec = PROMPT.parse('five pines in the snow at dusk', { seed: 3 });
  var r = PROMPT.rng(spec, 'subject');
  var near = PAINT.placeBox(w, h, hz, spec, spec.subject, 0, 5, r);
  var far = PAINT.placeBox(w, h, hz, spec, spec.subject, 4, 5, r);
  var sizeRatio = far.h / near.h;
  var footRatio = ((far.y + far.h) - hz) / ((near.y + near.h) - hz);
  check(Math.abs(sizeRatio - footRatio) < 0.12,
    'and they shrink by the same amount they retreat (' + sizeRatio.toFixed(2) +
    ' of the size, ' + footRatio.toFixed(2) + ' of the way down)');
  pass('distance is one number, seen twice');
})();

/* ---------------------------------------------------------- what it is doing
 * Every animal in every picture stood in exactly the same way: four legs down,
 * head level, facing right. A herd of them was the same statue three times
 * over — and what an animal is doing is most of what a picture of an animal is
 * about.
 */
(function whatItIsDoing() {
  console.log('\nWhat the animal is doing');

  var missing = LEX.POSES.filter(function (p) { return !SUBJECTS.POSE[p.id]; });
  check(missing.length === 0,
    LEX.POSES.length + ' things an animal can be doing, each with a way of ' +
    'standing' + (missing.length ? ' — but not ' + missing.map(function (p) { return p.id; }).join(', ') : ''));

  check(PROMPT.parse('a stag grazing in a meadow', { seed: 3 }).pose === 'grazing',
    'a grazing stag is grazing');
  check(PROMPT.parse('a wolf running on the plains', { seed: 3 }).pose === 'running',
    'and a running wolf is running');

  /* Said nothing, and they are not all doing the same thing. */
  var doing = {};
  for (var seed = 0; seed < 60; seed++) {
    doing[PROMPT.parse('a wolf in a meadow at noon', { seed: seed }).pose] = true;
  }
  check(Object.keys(doing).length >= 4,
    'and sixty wolves nobody said anything about are doing ' +
    Object.keys(doing).length + ' different things');

  /* Drawn, not declared. One animal, one box, one seed — only the pose. */
  function posed(pose) {
    var spec = PROMPT.parse('a stag in a meadow at noon', { seed: 4 });
    spec.pose = pose;
    var ctx = recorder(400, 300);
    SUBJECTS.draw(ctx, { draw: 'quadruped', form: 'deer' },
      { x: 110, y: 70, w: 190, h: 180, depth: 0, anchor: 'ground' },
      PAINT.makePalette(spec), PROMPT.rng(spec, 'subject'), spec);
    var shapes = shapesOf(ctx);
    /* Facing right, so the muzzle is the rightmost thing drawn. */
    var head = shapes.reduce(function (a, sh) {
      return (!a || sh.x > a.x) ? sh : a;
    }, null);
    var top = shapes.reduce(function (a, sh) { return Math.min(a, sh.top); }, 1e9);
    var spread = shapes.reduce(function (a, sh) { return Math.max(a, sh.w); }, 0);
    return { head: head, top: top, spread: spread, shapes: shapes };
  }

  var grazing = posed('grazing'), alert = posed('alert'), standing = posed('standing');
  check(grazing.head.y > alert.head.y + 30,
    'a grazing stag has its head down where the grass is and an alert one has ' +
    'it up (' + Math.round(grazing.head.y) + ' against ' + Math.round(alert.head.y) + ')');

  var resting = posed('resting');
  check(resting.top > standing.top + 15,
    'a resting one sits lower than a standing one (' + Math.round(resting.top) +
    ' against ' + Math.round(standing.top) + ')');

  /* Legs thrown further when it is running. Measured as how far apart the feet
   * finish up, which is what a stride is. */
  function feet(bag) {
    var low = bag.shapes.filter(function (sh) { return sh.bottom > 240; });
    if (!low.length) return 0;
    var lo = 1e9, hi = -1e9;
    low.forEach(function (sh) { lo = Math.min(lo, sh.x); hi = Math.max(hi, sh.x); });
    return hi - lo;
  }
  var running = posed('running');
  check(feet(running) > feet(standing) + 4,
    'and a running one throws its legs further out than a standing one (' +
    Math.round(feet(running)) + 'px apart against ' + Math.round(feet(standing)) + ')');
  /*
   * A walk is the diagonals out of step and a gallop is not. Measured as which
   * way each leg leans — a leg's foot against its own shoulder — because
   * comparing how far apart the feet end up cannot tell the two apart: a walk
   * with a short stride and a gallop with a long one spread the same way round
   * whatever the diagonals are doing.
   */
  function gait(pose) {
    var spec = PROMPT.parse('a stag in a meadow at noon', { seed: 4 });
    spec.pose = pose;
    var ctx = recorder(400, 300);
    SUBJECTS.draw(ctx, { draw: 'quadruped', form: 'deer' },
      { x: 110, y: 70, w: 190, h: 180, depth: 0, anchor: 'ground' },
      PAINT.makePalette(spec), PROMPT.rng(spec, 'subject'), spec);
    var pts = [], legs = [];
    ctx.log.forEach(function (c) {
      if (c.op === 'beginPath') pts = [];
      else if (c.op === 'moveTo' || c.op === 'lineTo' || c.op === 'quadraticCurveTo') {
        pts.push(c.args.slice(-2));
      } else if (c.op === 'fill' && pts.length) {
        var top = Math.min.apply(null, pts.map(function (q) { return q[1]; }));
        var low = Math.max.apply(null, pts.map(function (q) { return q[1]; }));
        if (low - top > 20) {
          function meanAt(y) {
            var near = pts.filter(function (q) { return Math.abs(q[1] - y) < 1; });
            return near.reduce(function (a, q) { return a + q[0]; }, 0) / (near.length || 1);
          }
          /* Which way the foot is thrown from the shoulder above it. */
          legs.push({ lean: meanAt(low) - meanAt(top), at: meanAt(top), tall: low - top });
        }
        pts = [];
      }
    });
    return legs;
  }

  var walkLegs = gait('walking'), runLegs = gait('running');
  function outOfStep(shapes) {
    /* The legs are the four tallest things drawn — a body and a neck are in
     * there too, and picking them up instead reads a lean off the wrong shape
     * entirely. Of those four the leftmost two are the hind legs, one near and
     * one far, and out of step means they lean opposite ways. */
    var four = shapes.slice().sort(function (a, b) { return b.tall - a.tall; }).slice(0, 4);
    if (four.length < 4) return null;
    four.sort(function (a, b) { return a.at - b.at; });
    return four[0].lean * four[1].lean < 0;
  }
  check(outOfStep(walkLegs) === true,
    'a walking stag has its diagonals out of step');
  check(outOfStep(runLegs) === false,
    'and a running one reaches out with everything at once');
  pass('an animal is doing something, and all fifteen bodies do it');
})();

/* ------------------------------------------------- putting the sun by hand
 * The hour is an angle, but a few things still ask which band of the day it
 * is — stars come out at night, windows light up when it is not daylight — so
 * moving the sun by hand has to move the band with it.
 */
(function sunByHand() {
  console.log('\nPutting the sun where you want it');

  function moved(text, sun) {
    var spec = PROMPT.parse(text, { seed: 2 });
    return PROMPT.atSun(spec, sun);
  }

  check(moved('a wolf in a meadow at noon', -40).time === 'night',
    'pulling the sun down to -40 makes it night, not noon with a dark sky');
  check(moved('a wolf in a meadow at midnight', 70).time === 'day',
    'and pushing it up to 70 makes it day');
  var rising = moved('a wolf in a meadow at dawn', 4);
  var setting = moved('a wolf in a meadow at dusk', 4);
  check(rising.time === 'dawn' && setting.time === 'dusk',
    'and at the horizon it is a sunrise or a sunset depending which way the ' +
    'sun was already going');

  check(moved('a wolf in a meadow', 200).sun <= 90 &&
        moved('a wolf in a meadow', -400).sun >= -60,
    'and the sun stays somewhere a sun can be');

  /* And the picture follows it: stars come out. */
  var day = PROMPT.parse('a wolf in a meadow at noon', { seed: 2 });
  var night = PROMPT.atSun(PROMPT.parse('a wolf in a meadow at noon', { seed: 2 }), -40);
  function stars(spec) {
    var ctx = recorder(320, 240);
    PAINT.render(ctx, 320, 240, spec);
    return ctx.log.filter(function (c) { return c.op === 'arc' && c.args[2] < 2.5; }).length;
  }
  check(stars(night) > stars(day) + 50,
    'and the stars come out with it (' + stars(night) + ' against ' + stars(day) + ')');
  pass('the sun can be put where you want it, and the picture follows');
})();

/* ----------------------------------------------------- what can be said
 * The examples on the page and the "surprise me" button are the only two
 * places most people ever find out what this understands. A vocabulary nobody
 * is shown may as well not be there.
 */
(function whatCanBeSaid() {
  console.log('\nShowing people what can be said');

  /* Every one of its own suggestions has to be a sentence it understands.
   * A button that writes a prompt and then reports back that it did not know
   * one of the words in it is the app arguing with itself. */
  var puzzled = [], empty = 0;
  for (var i = 0; i < 400; i++) {
    var text = PROMPT.surprise(i);
    if (!text || text.indexOf('{') >= 0) { empty++; continue; }
    if (/\s\s/.test(text)) { puzzled.push('double space: ' + text); continue; }
    var spec = PROMPT.parse(text, { seed: 1 });
    if (spec.unknown.length) puzzled.push(text + ' → ' + spec.unknown.join(', '));
  }
  check(empty === 0, 'every surprise comes out as words rather than as a template');
  check(puzzled.length === 0,
    'and all four hundred of them are sentences it understands' +
    (puzzled.length ? ' — ' + puzzled.slice(0, 3).join(' · ') : ''));

  /* And they reach the words that were added later, not only the ones that
   * were there first. */
  var all = [];
  for (var j = 0; j < 400; j++) all.push(PROMPT.surprise(j));
  var joined = all.join(' | ');
  var reaches = {
    'an hour that is not one of four': /first light|mid-morning|golden hour|blue hour|high noon/,
    'what a thing is made of': /bronze|stone|glass|marble|copper|jade|obsidian|iron/,
    'a part it can be given': /winged|horned|antlered|spiked|armoured|haloed|crested/,
    'how worn it is': /weathered|ancient|pristine|worn|crumbling/,
    'how much of something': /slightly|very|barely|extremely|incredibly/
  };
  var unreached = Object.keys(reaches).filter(function (k) { return !reaches[k].test(joined); });
  check(unreached.length === 0,
    'and between them they show off ' + Object.keys(reaches).length + ' kinds of word ' +
    'that were added after the first draft' +
    (unreached.length ? ' — but never ' + unreached.join(', ') : ''));

  /* And they are written in English: "a iron dragon" is the app not reading
   * its own sentence. */
  var articles = all.filter(function (t) {
    return /(^|\s)a\s+[aeiou]/i.test(t) || /(^|\s)an\s+[^aeiou\s]/i.test(t);
  });
  check(articles.length === 0,
    'and each one says "a" or "an" to suit the word after it' +
    (articles.length ? ' — ' + articles.slice(0, 3).join(' · ') : ''));

  /* An animal can be doing something; a portal cannot graze. */
  var nonsense = all.filter(function (t) {
    return /(portal|tower|castle|comet|planet|sword|crystal|ufo|rocket) (grazing|drinking|resting|walking|running)/.test(t);
  });
  check(nonsense.length === 0,
    'and nothing without legs is grazing' +
    (nonsense.length ? ' — ' + nonsense.slice(0, 2).join(' · ') : ''));
  pass('the app shows people what it can be told');
})();

/*
 * The noise floor.
 *
 * Grain used to be added only below `day < 0.55`, so every picture taken in
 * daylight came out mathematically perfect: whole regions where neighbouring
 * pixels were bit-for-bit identical. Nothing photographed is ever like that.
 * Measured across the painter, half of a noon frame had no variation at all,
 * which is most of why the daylight pictures read as posters while the dusk
 * ones read as photographs.
 */
(function theNoiseFloor() {
  console.log('\nThe noise floor');

  var w = 96, h = 72;
  function flat(v) {
    var img = { data: new Uint8ClampedArray(w * h * 4), width: w, height: h };
    for (var i = 0; i < w * h; i++) {
      img.data[i * 4] = img.data[i * 4 + 1] = img.data[i * 4 + 2] = v;
      img.data[i * 4 + 3] = 255;
    }
    return img;
  }
  /* Spread of a single channel, which is what "how grainy" actually means. */
  function spread(img, c) {
    var n = w * h, sum = 0, sum2 = 0;
    for (var i = 0; i < n; i++) { var v = img.data[i * 4 + c]; sum += v; sum2 += v * v; }
    var m = sum / n;
    return Math.sqrt(Math.max(0, sum2 / n - m * m));
  }
  /*
   * A whole picture put through the finisher at a given hour.
   *
   * The recording context does not rasterise, so its buffer arrives full of
   * the pattern it was born with — which is variation, and made the first
   * version of this test pass with the daylight bug still in place. So the
   * buffer is flattened to one exact grey after the painter has run and
   * before the finisher does: then anything but a flat sheet coming out the
   * other end is the finishing passes' own work, which is what is under test.
   */
  function finished(hour) {
    var spec = PROMPT.parse('a stone tower in a meadow ' + hour, { seed: 4, style: 'auto' });
    var ctx = new FakeContext(w, h);
    var P = PAINT.render(ctx, w, h, spec);
    for (var i = 0; i < w * h; i++) {
      ctx._pixels[i * 4] = ctx._pixels[i * 4 + 1] = ctx._pixels[i * 4 + 2] = 128;
      ctx._pixels[i * 4 + 3] = 255;
    }
    FINISH.apply(ctx, w, h, spec, P);
    var got = ctx.getImageData(0, 0, w, h);
    return { data: got.data, width: w, height: h };
  }
  /* How often two side-by-side pixels are bit-for-bit identical. A photograph
   * almost never does this; a flat fill does it everywhere. */
  function twins(img) {
    var same = 0, n = 0;
    for (var y = 0; y < h; y++) {
      for (var x = 1; x < w; x++) {
        var a = (y * w + x) * 4, b = (y * w + x - 1) * 4;
        if (img.data[a] === img.data[b] && img.data[a + 1] === img.data[b + 1] &&
            img.data[a + 2] === img.data[b + 2]) same++;
        n++;
      }
    }
    return same / n;
  }

  var noon = finished('at noon');
  var night = finished('at midnight');
  check(twins(noon) < 0.10,
    'a picture taken at noon has no two identical neighbours to speak of (' +
    (twins(noon) * 100).toFixed(1) + '% of pairs, and it was far more)');
  check(twins(night) < 0.10,
    'and neither does one taken at midnight (' + (twins(night) * 100).toFixed(1) + '%)');

  /*
   * And the amount of it follows the light, the way an ISO dial does. Asked of
   * `sensor` directly this proves nothing — handing it two numbers and finding
   * that the bigger one is noisier is a test of arithmetic. It has to be asked
   * of the finisher, which is the thing that decides which number to use.
   */
  /* Measured between neighbours rather than across the frame: the vignette and
   * the slow drift both spread the picture out too, and a picture that merely
   * has a dark corner is not a grainy one. Noise is what changes from one pixel
   * to the next. */
  function fineness(img) {
    var sum = 0, n = 0;
    for (var y = 0; y < h; y++) {
      for (var x = 1; x < w; x++) {
        sum += Math.abs(img.data[(y * w + x) * 4] - img.data[(y * w + x - 1) * 4]);
        n++;
      }
    }
    return sum / n;
  }
  var byDay = fineness(noon), byNight = fineness(night);
  check(byNight > byDay * 1.6,
    'a picture taken at midnight is grainier than one taken at noon (' +
    byNight.toFixed(2) + ' against ' + byDay.toFixed(2) + ')');
  check(byDay > 1.2,
    'but the noon one is never perfectly clean either (' +
    byDay.toFixed(2) + ' levels between one pixel and the next)');

  /*
   * Two components, because a sensor has two. If the noise were luminance only
   * it would be film grain; a digital frame also has the channels disagreeing
   * with each other, which is what makes a dark patch go faintly purple.
   */
  var one = flat(128);
  FINISH.helpers.sensor(one, 12, 7, PROMPT.rng(PROMPT.parse('a wall', { seed: 2 }), 'n2'));
  var apart = 0;
  for (var i = 0; i < w * h; i++) {
    apart += Math.abs(one.data[i * 4] - one.data[i * 4 + 2]);
  }
  apart /= w * h;
  check(apart > 1.2,
    'the channels do not move together — there is colour in the noise, not ' +
    'just brightness (red and blue differ by ' + apart.toFixed(2) + ' on average)');

  /* And green is the quiet one: a sensor has twice as many green photosites,
   * so it averages twice the light and comes out the least noisy channel. */
  var g = spread(one, 1), rr = spread(one, 0), bb = spread(one, 2);
  check(g < rr && g < bb,
    'and green is the quietest of the three, as it is on real silicon (' +
    'r ' + rr.toFixed(2) + ' · g ' + g.toFixed(2) + ' · b ' + bb.toFixed(2) + ')');

  /* It must not shift the picture. Noise is a wobble around the value, not a
   * change to it. */
  var tone = 0;
  for (var j = 0; j < w * h; j++) tone += one.data[j * 4];
  check(Math.abs(tone / (w * h) - 128) < 1.5,
    'and it leaves the picture exactly as bright as it found it (' +
    (tone / (w * h)).toFixed(2) + ')');
  pass('every picture has noise in it, in daylight as much as after dark');
})();

/*
 * The colour of a shadow.
 *
 * Every shadow in the painter was black. Nothing is. A surface with the sun
 * off it is lit by the sky instead and takes the sky's colour, and that split
 * between a warm lit side and a cool dark one is the strongest cue there is
 * for a photograph rather than a drawing.
 */
(function theColourOfShadow() {
  console.log('\nThe colour of a shadow');

  function paletteFor(text) {
    var spec = PROMPT.parse(text, { seed: 5, style: 'auto' });
    return { P: PAINT.makePalette(spec), spec: spec };
  }
  /* hsla(h,s%,l%,a) back into numbers. */
  function read(css) {
    var m = /hsla?\(([-0-9.]+),\s*([-0-9.]+)%,\s*([-0-9.]+)%/.exec(css);
    return m ? [parseFloat(m[1]), parseFloat(m[2]), parseFloat(m[3])] : null;
  }

  var noon = paletteFor('a stone tower in a meadow at noon');
  var shadow = read(noon.P.shadow(1, 1));
  check(shadow !== null, 'the palette can be asked for the colour of a shadow');
  check(shadow[1] > 4,
    'and a shadow at noon has colour in it — it is not black or grey (' +
    shadow[1].toFixed(1) + '% saturated)');
  check(shadow[2] < 30,
    'while still being dark enough to read as shadow (' + shadow[2].toFixed(1) + '% light)');

  /* It is the sky's colour, because the sky is what is lighting it. The hue
   * has to follow the sky round the day rather than being a fixed blue. */
  var sky = noon.P.sky.mid;
  var off = Math.abs(((shadow[0] - sky[0]) % 360 + 540) % 360 - 180);
  check(off < 1,
    'and it is the sky\'s own colour, because the sky is what is lighting it');

  var dusk = paletteFor('a stone tower in a meadow at sunset');
  var duskShadow = read(dusk.P.shadow(1, 1));
  var moved = Math.abs(((duskShadow[0] - shadow[0]) % 360 + 540) % 360 - 180);
  check(moved > 12,
    'so a shadow at sunset is a different colour from one at noon (' +
    moved.toFixed(0) + ' degrees round the wheel)');

  /*
   * And a shadow lying on the ground is a third thing again: not a dark shape
   * on the grass but the grass with the sun off it. Drawn in the scene's ink,
   * as it was, every shadow came out the colour of the animals standing in it.
   */
  var cast = read(noon.P.cast(0, 1));
  var land = noon.P.bend(noon.P.scene.land);
  check(cast[2] < land[2],
    'a shadow on the ground is darker than the ground (' + cast[2].toFixed(1) +
    '% against ' + land[2].toFixed(1) + '%)');

  /*
   * And it is that ground's colour, not a colour of its own: a shadow on sand
   * and a shadow on grass are different colours because sand and grass are.
   * Comparing it against the scene's ink instead would prove nothing here —
   * in a meadow the ink and the field share a hue — so the test is whether it
   * follows the ground from scene to scene.
   */
  var grounds = ['a meadow', 'the desert', 'a snowy plain', 'a forest',
    'the open sea', 'a cave'].map(function (where) {
    var q = paletteFor('a stone tower in ' + where + ' at noon');
    return {
      where: where, cast: read(q.P.cast(0, 1)),
      ink: read(q.P.ink(0, 1)), land: q.P.bend(q.P.scene.land)
    };
  });
  var strays = grounds.filter(function (g) {
    return Math.abs(((g.cast[0] - g.land[0]) % 360 + 540) % 360 - 180) > 22;
  });
  check(strays.length === 0,
    'and in every scene it is that scene\'s ground, gone dark — never more ' +
    'than 22 degrees off the colour it is lying on' +
    (strays.length ? ' (' + strays[0].where + ' is not)' : ''));

  /*
   * Against the ink it replaced, and by saturation rather than by hue: in most
   * of these scenes the ink and the ground already share a hue, so a hue test
   * here would pass against the old code and prove nothing. What the ink does
   * and a shadow must not is drain the colour out — grass in shade is still
   * as green as grass, only darker.
   */
  var drained = grounds.filter(function (g) { return g.cast[1] <= g.ink[1]; });
  check(drained.length === 0,
    'and it keeps the colour the ink drained away — a shadow on grass is as ' +
    'green as the grass' +
    (drained.length ? ' (' + drained[0].where + ' is not)' : ''));
  /* And the two really do differ, or the check above is measuring nothing. */
  var same = grounds.filter(function (g) { return Math.abs(g.cast[1] - g.ink[1]) < 4; });
  check(same.length === 0,
    'by a margin that is actually there in every scene, not a rounding error');
  pass('shadows are the colour of the sky that fills them, not black');
})();

/*
 * The ground underfoot.
 *
 * Measured row by row down the frame, the engine's pictures sat at one flat
 * level of detail from top to bottom, and that level was exactly the sensor
 * noise laid over them. The ground had no texture of its own: a wash of
 * colour, a slow drift, and thirty-odd pebbles scattered on it.
 *
 * What a photograph of ground has is a texture gradient — the same clods and
 * tufts all the way out, large and sparse at your feet, small and crowded at
 * the horizon. It is the strongest depth cue in any landscape photograph, and
 * an evenly-grained plane says "flat picture" however well it is shaded.
 */
(function theGroundUnderfoot() {
  console.log('\nThe ground underfoot');

  var w = 640, h = 400, hz = h * 0.70;
  /* Every mark the ground pass lays down, with the colour it was laid in. */
  function bedOf(text, light, seed) {
    var spec = PROMPT.parse(text, { seed: seed || 4, style: 'auto' });
    var ctx = recorder(w, h);
    var P = PAINT.makePalette(spec);
    PAINT.bed(ctx, w, h, hz, P, spec, PROMPT.rng(spec, 'bed'), light);
    var out = [], fill = null;
    ctx.log.forEach(function (c) {
      if (c.op === 'set' && c.args[0] === 'fillStyle') fill = c.args[1];
      else if (c.op === 'ellipse') {
        out.push({ x: c.args[0], y: c.args[1], rx: c.args[2], ry: c.args[3], fill: fill });
      }
    });
    return out;
  }
  function mean(list, of) {
    if (!list.length) return 0;
    return list.reduce(function (a, m) { return a + of(m); }, 0) / list.length;
  }

  var left = { x: w * 0.15, y: h * 0.10, r: 20 };
  var field = bedOf('a stone tower in a meadow at noon', left);
  check(field.length > 400,
    'the ground has a texture of its own, not just a colour (' +
    field.length + ' marks)');

  /* None of it in the sky. */
  var floating = field.filter(function (m) { return m.y < hz - 1; });
  check(floating.length === 0,
    'and all of it is on the ground — nothing is lying in the air');

  /*
   * The gradient itself, which is the whole point. A clod at your feet and a
   * clod at the horizon are the same clod; only the distance differs, so the
   * near one must be drawn far larger. Split the band in half by depth and
   * compare.
   */
  var deep = h - hz;
  var near = field.filter(function (m) { return m.y > hz + deep * 0.72; });
  var far = field.filter(function (m) { return m.y < hz + deep * 0.34; });
  var nearWide = mean(near, function (m) { return m.rx; });
  var farWide = mean(far, function (m) { return m.rx; });
  check(near.length > 20 && far.length > 20,
    'with marks at both ends of it to compare (' + near.length + ' near, ' +
    far.length + ' far)');
  check(nearWide > farWide * 2,
    'and the near ones are far bigger than the far ones (' + nearWide.toFixed(2) +
    'px against ' + farWide.toFixed(2) + 'px)');

  /* And crowded the other way: the further ground holds more of them per inch
   * of picture, because more of it fits up there. */
  var nearPer = near.length / (deep * 0.28);
  var farPer = far.length / (deep * 0.34);
  check(farPer > nearPer * 1.5,
    'while the far ones are more crowded, as receding ground is (' +
    farPer.toFixed(1) + ' against ' + nearPer.toFixed(1) + ' marks a pixel of depth)');

  /* Foreshortening: the same stone lies flatter the further off it is, because
   * you are looking across the plane rather than down at it. */
  var nearFlat = mean(near, function (m) { return m.ry / Math.max(m.rx, 0.001); });
  var farFlat = mean(far, function (m) { return m.ry / Math.max(m.rx, 0.001); });
  check(farFlat < nearFlat * 0.85,
    'and it lies flatter the further off it is, the way a plane seen at an ' +
    'angle does (' + farFlat.toFixed(2) + ' against ' + nearFlat.toFixed(2) + ')');

  /*
   * Lumps, not speckles. Each mark is a shadow and a highlight offset from one
   * another, and which way round decides whether the ground reads as bumpy or
   * as dirty. The shadow goes on the side away from the light.
   */
  var pairs = [];
  for (var i = 0; i + 1 < field.length; i += 2) {
    pairs.push({ dim: field[i], lit: field[i + 1] });
  }
  var twoTone = pairs.filter(function (p) { return p.dim.fill !== p.lit.fill; });
  check(twoTone.length === pairs.length,
    'every mark is drawn twice, in two colours — a shadow and a lit top');
  /*
   * Pulled properly apart, not nudged. Checking only which side the shadow
   * falls on lets half the offset be deleted and still pass — the highlight
   * alone keeps the sign — so what is measured is the gap as a share of the
   * lump it belongs to. A shadow a tenth of a stone's width off centre is not
   * a lit stone, it is a smudge on a flat one.
   */
  var lean = mean(pairs, function (p) { return p.dim.x - p.lit.x; });
  var leanBy = mean(pairs, function (p) {
    return (p.dim.x - p.lit.x) / Math.max(p.dim.rx * 2, 0.001);
  });
  var dropBy = mean(pairs, function (p) {
    return (p.dim.y - p.lit.y) / Math.max(p.dim.ry * 2, 0.001);
  });
  check(lean > 0,
    'and with the light off to the left the shadows fall to the right (' +
    lean.toFixed(2) + 'px)');
  check(leanBy > 0.25,
    'by a real part of the lump rather than a nudge (' +
    (leanBy * 100).toFixed(0) + '% of its width)');
  check(dropBy > 0.25,
    'and they sit below the lit top by as much again (' +
    (dropBy * 100).toFixed(0) + '% of its height)');

  /* Move the light and they follow, or they are decoration rather than lumps. */
  var right = bedOf('a stone tower in a meadow at noon', { x: w * 0.85, y: h * 0.10, r: 20 });
  var other = [];
  for (var j = 0; j + 1 < right.length; j += 2) other.push({ dim: right[j], lit: right[j + 1] });
  var otherLean = mean(other, function (p) { return p.dim.x - p.lit.x; });
  check(otherLean < 0,
    'and putting the light on the other side turns every one of them round (' +
    otherLean.toFixed(2) + 'px)');

  /* Ground only. There is no ground in space, and a texture of clods hanging
   * in it would be the picture forgetting where it is. */
  check(bedOf('a comet in space', left).length === 0,
    'and there is none of it in space, where there is no ground to have any');

  /* On a shore the sand starts well down the frame; above it is open water. */
  var beach = bedOf('a lighthouse on the shore at noon', left);
  var wet = beach.filter(function (m) { return m.y < hz + deep * 0.5; });
  check(beach.length > 100 && wet.length === 0,
    'and on a shore it starts where the sand does, not out on the water (' +
    beach.length + ' marks, none above the tideline)');

  /* Finally, that the painter actually runs it. Nothing else in the engine
   * draws ellipses in these numbers, so counting them in a whole picture is
   * enough to catch the pass being written and never called. */
  var whole = recorder(w, h);
  var spec = PROMPT.parse('a stone tower in a meadow at noon', { seed: 4, style: 'auto' });
  PAINT.render(whole, w, h, spec);
  var drawn = whole.log.filter(function (c) { return c.op === 'ellipse'; }).length;
  check(drawn > 400,
    'and the painter lays it down as part of painting a picture (' + drawn +
    ' marks in a whole one)');
  pass('the ground has a texture, and it recedes');
})();

/*
 * What a thing is made of, in marks.
 *
 * The flatness that made the ground read as paint was true of everything
 * standing on it, and worse: every pass over a subject is a smooth gradient,
 * so however carefully a stone tower was lit there was no stone in it. Put a
 * magnifier on one and there was nothing there at all. Real surfaces are the
 * opposite of smooth close up, and which kind of not-smooth is most of how
 * anybody tells stone from bark from beaten iron without being told.
 */
(function madeOfMarks() {
  console.log('\nWhat a thing is made of');

  var w = 640, h = 400;
  /* The painter asks its host for a small canvas to build the tile on. Node
   * has none, so the test lends it one that records instead of drawing — which
   * means what is checked below is the real pass, not a stand-in for it. */
  var asked = [];
  function lend() {
    PAINT.useScratch(function (tw, th) {
      var tc = recorder(tw, th);
      var can = { width: tw, height: th, getContext: function () { return tc; }, ctx: tc };
      asked.push(can);
      return can;
    });
  }
  function giveBack() { PAINT.useScratch(null); asked = []; }

  /* Every mark the tile builder lays down, by kind. */
  function tileOps(kind, px) {
    asked = [];
    lend();
    var r = PROMPT.rng(PROMPT.parse('a wall', { seed: 3 }), 'tile');
    var can = PAINT.surfaceTile(kind, px, 'rgba(0,0,0,0.5)', 'rgba(255,255,255,0.4)', r);
    var ops = can ? can.ctx.log.filter(function (c) {
      return c.op === 'ellipse' || c.op === 'lineTo';
    }) : [];
    giveBack();
    return ops;
  }

  var KINDS = ['pit', 'grain', 'brush', 'weave', 'fur', 'scale', 'facet', 'vein'];
  var empty = KINDS.filter(function (k) { return tileOps(k, 40).length < 20; });
  check(empty.length === 0,
    'all ' + KINDS.length + ' kinds of surface draw something' +
    (empty.length ? ' — but not ' + empty.join(', ') : ''));

  /*
   * Seamless. A repeating tile whose marks stop at its edge shows the edge as
   * a grid across the whole thing, which is worse than the flatness it is
   * fixing. So every mark is drawn nine times, once for each way the tile can
   * meet itself.
   */
  var pits = tileOps('pit', 40);
  var first = pits.slice(0, 9);
  var xs = first.map(function (c) { return Math.round(c.args[0]); });
  var ys = first.map(function (c) { return Math.round(c.args[1]); });
  function spread(list) {
    var seen = list.filter(function (v, i) { return list.indexOf(v) === i; }).sort(function (a, b) { return a - b; });
    return seen;
  }
  var acrossX = spread(xs), acrossY = spread(ys);
  check(acrossX.length === 3 && acrossY.length === 3,
    'and each one is drawn in three places across and three down, so the tile ' +
    'meets itself on every side');
  check(acrossX[1] - acrossX[0] === 40 && acrossX[2] - acrossX[1] === 40,
    'exactly one tile apart, which is what makes the repeat seamless (' +
    acrossX.join(', ') + ')');

  /* Each kind is its own thing: pitted stone is specks, wood grain is lines. */
  check(tileOps('pit', 40).every(function (c) { return c.op === 'ellipse'; }),
    'stone is pitted — hollows, not lines');
  check(tileOps('grain', 40).every(function (c) { return c.op === 'lineTo'; }),
    'and wood runs in a grain — lines, not hollows');

  /*
   * How coarse it is on screen is not how coarse it is in the world. The same
   * brickwork is inches across on a tower filling the frame and invisible on
   * one at the horizon, so the tile is built at a size taken from how big the
   * thing is being drawn.
   */
  function tileFor(text, scale) {
    asked = [];
    lend();
    var spec = PROMPT.parse(text, { seed: 4, style: 'auto' });
    var ctx = recorder(w, h);
    var patterns = 0;
    ctx.createPattern = function () { patterns++; return { pattern: true }; };
    var P = PAINT.makePalette(spec);
    var box = { x: 100, y: 100, w: 240 * scale, h: 300 * scale, depth: 0, anchor: 'ground' };
    PAINT.surfaceOn(ctx, spec.subject, box, P, P,
      PROMPT.rng(spec, 's'), spec, spec.material, 0);
    var size = asked.length ? asked[0].width : 0;
    var alpha = null;
    ctx.log.forEach(function (c) {
      if (c.op === 'set' && c.args[0] === 'globalAlpha') alpha = c.args[1];
    });
    giveBack();
    return { size: size, patterns: patterns, alpha: alpha };
  }

  var big = tileFor('a huge stone tower in a meadow', 1);
  var small = tileFor('a huge stone tower in a meadow', 0.35);
  check(big.patterns === 1, 'a stone tower is given a surface');
  check(big.size > small.size,
    'and the nearer it is drawn the coarser that surface is, because it is ' +
    'the same stone either way (' + big.size + 'px against ' + small.size + 'px)');

  /* And past a certain distance a real surface is a tone rather than a
   * texture, which is why the far half of a landscape photograph is smooth. */
  var tiny = tileFor('a huge stone tower in a meadow', 0.06);
  check(tiny.patterns === 0,
    'and a thing drawn small enough is given none at all, as distance does');

  /* Distance flattens what is left of it. */
  asked = []; lend();
  var spec2 = PROMPT.parse('a huge stone tower in a meadow', { seed: 4, style: 'auto' });
  function strengthAt(far) {
    var ctx = recorder(w, h);
    ctx.createPattern = function () { return { pattern: true }; };
    var P = PAINT.makePalette(spec2);
    PAINT.surfaceOn(ctx, spec2.subject,
      { x: 100, y: 100, w: 240, h: 300, depth: 0, anchor: 'ground' },
      P, P, PROMPT.rng(spec2, 's'), spec2, spec2.material, far);
    var a = 0;
    ctx.log.forEach(function (c) {
      if (c.op === 'set' && c.args[0] === 'globalAlpha') a = c.args[1];
    });
    return a;
  }
  var near = strengthAt(0), away = strengthAt(0.9);
  giveBack();
  check(near > away * 1.5,
    'and it fades with distance, the way everything else does (' +
    near.toFixed(2) + ' near, ' + away.toFixed(2) + ' far)');

  /*
   * Things the sentence never said the material of still have one. A stag has
   * fur whether or not anybody mentioned it, and a cabin is made of wood.
   */
  var natural = [
    ['a stag in a meadow', 'fur'],
    ['a wooden cabin in a forest', 'grain'],
    ['a red dragon over the mountains', 'scale'],
    ['an iron knight in a meadow', 'brush']
  ];
  /* Asked of the painter, not worked out from the same two tables it uses —
   * that would be checking this test's copy of the rule rather than the rule. */
  var missed = natural.filter(function (pair) {
    asked = []; lend();
    var spec3 = PROMPT.parse(pair[0], { seed: 5, style: 'auto' });
    var ctx3 = recorder(w, h);
    ctx3.createPattern = function () { return { pattern: true }; };
    var P3 = PAINT.makePalette(spec3);
    var got = PAINT.surfaceOn(ctx3, spec3.subject,
      { x: 100, y: 100, w: 240, h: 300, depth: 0, anchor: 'ground' },
      P3, P3, PROMPT.rng(spec3, 's'), spec3, spec3.material, 0);
    giveBack();
    return got !== pair[1];
  });
  check(missed.length === 0,
    'and a thing nobody said the material of still has one — fur on a stag, ' +
    'grain on a cabin, scales on a dragon' +
    (missed.length ? ' — but not ' + missed[0][0] : ''));

  /* Finally, that the painter runs it while painting a subject. */
  asked = []; lend();
  var whole = recorder(w, h);
  var patterned = 0;
  whole.createPattern = function () { patterned++; return { pattern: true }; };
  var spec4 = PROMPT.parse('a close-up of a huge stone tower in a meadow at noon',
    { seed: 4, style: 'auto' });
  PAINT.render(whole, w, h, spec4);
  giveBack();
  check(patterned > 0,
    'and the painter lays it on while painting a picture (' + patterned + ' surfaces)');

  /* With no canvas to build a tile on — an old browser, a stripped host — the
   * picture still gets painted, just without this. */
  PAINT.useScratch(function () { return null; });
  var bare = recorder(w, h);
  var threw = false;
  try { PAINT.render(bare, w, h, spec4); } catch (e) { threw = true; }
  PAINT.useScratch(null);
  check(!threw, 'and where there is no canvas to build one on, it simply goes without');
  pass('things are made of something, and it shows');
})();

/*
 * Company.
 *
 * Measured as energy per octave — halve the picture, see how much structure
 * was lost, repeat — a photograph of the natural world comes out close to
 * flat: about as much going on at every scale. These pictures were badly
 * tilted, and the reason was a plain gap rather than anything subtle.
 * Everything the engine drew on the ground was under about five pixels, and
 * the only other thing in the frame was one large subject. Between the two
 * there was nothing: no bushes, no boulders, no fallen logs, no clumps of
 * scrub. A forest floor measured three times more structure at one pixel than
 * at sixteen, where a photograph of one has more at sixteen.
 */
(function theCompanyItKeeps() {
  console.log('\nCompany');

  var w = 640, h = 400, hz = h * 0.70;
  function out(text, light, seed) {
    var spec = PROMPT.parse(text, { seed: seed || 4, style: 'auto' });
    var ctx = recorder(w, h);
    var P = PAINT.makePalette(spec);
    var put = PAINT.company(ctx, w, h, hz, P, spec,
      PROMPT.rng(spec, 'company'), light);
    var marks = [], fill = null;
    ctx.log.forEach(function (c) {
      if (c.op === 'set' && c.args[0] === 'fillStyle') fill = c.args[1];
      else if (c.op === 'ellipse') {
        marks.push({ x: c.args[0], y: c.args[1], rx: c.args[2], ry: c.args[3], fill: fill });
      }
    });
    return { put: put, marks: marks, P: P, spec: spec };
  }
  function mean(list, of) {
    if (!list.length) return 0;
    return list.reduce(function (a, m) { return a + of(m); }, 0) / list.length;
  }

  var left = { x: w * 0.12, y: h * 0.10, r: 20 };
  var field = out('a stag in a meadow at noon', left, 2);
  check(field.put.length > 12,
    'a meadow has things standing about in it (' + field.put.length + ')');

  /* All of it on the ground. */
  var floating = field.put.filter(function (it) { return it.y < hz - 1; });
  check(floating.length === 0, 'and all of it on the ground, none in the sky');

  /*
   * Painted far to near, which is how the engine gets the one thing it has
   * never had: something in front of something else. Nothing in a CODA picture
   * used to overlap anything.
   */
  var backwards = 0;
  for (var i = 1; i < field.put.length; i++) {
    if (field.put[i].y < field.put[i - 1].y - 0.001) backwards++;
  }
  check(backwards === 0,
    'and drawn from the back forwards, so a near one covers part of a far one' +
    (backwards ? ' — ' + backwards + ' out of order' : ''));

  /* Perspective, the same as the ground under it. */
  var deep = h - hz;
  var near = field.put.filter(function (it) { return it.y > hz + deep * 0.7; });
  var far = field.put.filter(function (it) { return it.y < hz + deep * 0.35; });
  check(near.length > 2 && far.length > 2,
    'with things at both ends of the field (' + near.length + ' near, ' +
    far.length + ' far)');
  var nearBig = mean(near, function (it) { return it.size; });
  var farBig = mean(far, function (it) { return it.size; });
  check(nearBig > farBig * 2,
    'and the near ones far bigger than the far ones (' + nearBig.toFixed(1) +
    'px against ' + farBig.toFixed(1) + 'px)');

  /*
   * And a wide spread of sizes, which turned out to matter more than how many
   * there are. A field of same-sized bushes measures *worse* than a few: they
   * merge into an even field, and an even field is the flatness this exists to
   * fix, only greener.
   */
  var sizes = near.map(function (it) { return it.size; }).sort(function (a, b) { return a - b; });
  check(sizes.length > 3 && sizes[sizes.length - 1] > sizes[0] * 2.5,
    'a few big ones and many small, rather than a field of one size (' +
    sizes[0].toFixed(1) + 'px to ' + sizes[sizes.length - 1].toFixed(1) + 'px)');

  /*
   * Each one puts something on the ground. Without that they are stickers on a
   * picture of a field rather than things standing in one.
   */
  check(field.marks.length >= field.put.length,
    'every one of them puts a shadow on the ground');

  /* And they all agree about where the light is. A field where each bush is
   * lit from its own direction is worse than a field of flat ones. */
  var shadowsLeft = mean(field.marks.slice(0, field.put.length),
    function (m, k) { return m.x; });
  var right = out('a stag in a meadow at noon', { x: w * 0.88, y: h * 0.10, r: 20 }, 2);
  var shadowsRight = mean(right.marks.slice(0, right.put.length),
    function (m) { return m.x; });
  check(shadowsRight < shadowsLeft,
    'and they all agree which way the light is coming from — moving it moves ' +
    'every shadow (' + shadowsLeft.toFixed(1) + ' against ' + shadowsRight.toFixed(1) + ')');

  /*
   * Painted in the ground's own colour they cannot be seen, which is how the
   * first attempt went: every shape was there and a field full of bushes came
   * out as a field. What grows on a surface is not the colour of the surface.
   */
  function light3(css) {
    var m = /hsla?\([-0-9.]+,\s*([-0-9.]+)%,\s*([-0-9.]+)%/.exec(css);
    return m ? [parseFloat(m[1]), parseFloat(m[2])] : null;
  }
  /* Asked of the colours the pass actually used, against the ground it used
   * them on — not worked out here from the same table, which would be this
   * test checking its own arithmetic. */
  var greens = field.put.filter(function (it) {
    return it.kind === 'bush' || it.kind === 'tussock';
  });
  var pale = greens.filter(function (it) {
    var a = light3(it.dim), b = light3(it.ground);
    return !a || !b || a[1] > b[1] - 5;
  });
  check(greens.length > 3 && pale.length === 0,
    'what grows on the ground is darker than the ground it grows in — all ' +
    greens.length + ' of them');
  var stones = field.put.filter(function (it) { return it.kind === 'rock'; });
  var bright = stones.filter(function (it) {
    var a = light3(it.dim), b = light3(it.ground);
    return !a || !b || a[0] > b[0] * 0.8;
  });
  check(stones.length > 2 && bright.length === 0,
    'and a stone is greyer than either — all ' + stones.length + ' of them');

  /* Where there is no ground there is nothing standing on it. */
  check(out('a comet in space', left).put.length === 0,
    'there is none of it in space, where there is nothing to stand on');
  check(out('a whale in the open sea', left).put.length === 0,
    'and none on the open sea');

  /* A street is not a meadow. Filling one in would be a different mistake from
   * leaving it empty. */
  var street = out('a robot in the city at night', left, 3);
  check(street.put.length > 0 && street.put.length < field.put.length * 0.7,
    'and a city street has less of it than a meadow does (' +
    street.put.length + ' against ' + field.put.length + ')');

  /* On a shore it starts where the sand does; above that is open water. */
  var beach = out('a lighthouse on the shore at noon', left, 5);
  var afloat = beach.put.filter(function (it) { return it.y < hz + deep * 0.5; });
  check(beach.put.length > 4 && afloat.length === 0,
    'and on a shore nothing is standing out on the water (' +
    beach.put.length + ' on the sand, none above the tideline)');

  /* Finally, that the painter runs it while painting a picture. */
  var whole = recorder(w, h);
  var before = whole.log.length;
  PAINT.render(whole, w, h, PROMPT.parse('a stag in a meadow at noon',
    { seed: 2, style: 'auto' }));
  var bare = recorder(w, h);
  PAINT.render(bare, w, h, PROMPT.parse('a whale in the open sea',
    { seed: 2, style: 'auto' }));
  check(whole.log.length > before, 'and the painter draws a whole picture with it in');
  pass('the landscape has more in it than one thing');
})();

/*
 * Cloud at every size.
 *
 * Measured as energy per octave on the sky alone — which is seventy per cent
 * of most of these pictures — adding cloud to a clear day made it *less*
 * structured at every scale: 9.98 down to 7.51 at thirty-two pixels, 6.95 down
 * to 6.05 at eight. A storm barely beat a clear noon. That is backwards.
 * Cloud is one of the most structured things anybody has ever photographed.
 *
 * Two causes. A cloud was three to six smooth blobs — one scale of detail with
 * nothing above or below it — where a real one has the same shape at every
 * size, towers to bulges to cauliflower to a ragged fringe. And the overcast
 * deck was a single grey gradient across the whole sky at up to seventy-eight
 * per cent, a sheet of paint over everything, which of course measures flatter
 * than no sheet at all.
 */
(function cloudAtEverySize() {
  console.log('\nCloud at every size');

  function lumpsOf(depth, rx, ry) {
    var ctx = recorder(480, 360);
    var r = PROMPT.rng(PROMPT.parse('open plains at noon', { seed: 9 }), 'cloud');
    PAINT.billow(ctx, 240, 120, rx == null ? 90 : rx, ry == null ? 30 : ry,
      depth, r, 'rgba(255,255,255,1)');
    return shapesOf(ctx);
  }

  var one = lumpsOf(0);
  var many = lumpsOf(2);
  check(one.length === 1, 'asked for no detail, a cloud lump is one lump');
  check(many.length > 8,
    'asked for three storeys of it, it is built from ' + many.length +
    ' — a lump, the lumps on it, and the lumps on those');

  /* More than one scale is the whole point: the same shape at every size. */
  var wide = many.map(function (sh) { return sh.w; }).sort(function (a, b) { return b - a; });
  check(wide[0] > wide[wide.length - 1] * 3,
    'and they run from ' + Math.round(wide[0]) + 'px down to ' +
    Math.round(wide[wide.length - 1]) + 'px, which is what makes it a cloud ' +
    'rather than a blob');
  /* And filled in at the sizes between, rather than one big and one tiny. */
  var middling = many.filter(function (sh) {
    return sh.w < wide[0] * 0.7 && sh.w > wide[wide.length - 1] * 1.6;
  });
  check(middling.length > 2,
    'with ' + middling.length + ' at the sizes in between, not just big and small');

  /*
   * A continuum of sizes rather than a few of them. Shrinking every lump by
   * the same fraction gives three sizes exactly, three storeys of identical
   * bobbles, which reads as a pattern rather than as weather. Pooled across
   * several clouds so the gaps mean something: sorted by width, no step
   * between one lump and the next bigger should swallow a large part of the
   * whole range.
   */
  var pooled = [];
  for (var seed = 1; seed <= 6; seed++) {
    var c2 = recorder(480, 360);
    PAINT.billow(c2, 240, 120, 90, 30, 2,
      PROMPT.rng(PROMPT.parse('open plains at noon', { seed: seed }), 'cloud'),
      'rgba(255,255,255,1)');
    shapesOf(c2).forEach(function (sh) { pooled.push(sh.w); });
  }
  pooled.sort(function (a, b) { return a - b; });
  /* Without the single biggest lump of each cloud, which sits well clear of
   * everything below it by construction — there is one of those per cloud and
   * a real gap under it, and counting it would measure the shape of the
   * recursion rather than whether the sizes inside it run continuously. */
  var inner = pooled.slice(0, Math.floor(pooled.length * 0.9));
  var range = inner[inner.length - 1] - inner[0];
  var widest = 0;
  for (var g = 1; g < inner.length; g++) {
    widest = Math.max(widest, inner[g] - inner[g - 1]);
  }
  check(inner.length > 40 && widest < range * 0.22,
    'and every size in between is used, not three sizes repeated — the biggest ' +
    'step between one lump and the next is ' + Math.round(100 * widest / range) +
    '% of the range across ' + inner.length + ' of them');

  /*
   * Piled upward. A cloud grows where it is rising, so the lumps go round the
   * top rather than all the way round — which is what gives one a heaped crown
   * and a flat bottom instead of a ball.
   */
  /*
   * Pooled across several clouds. One cloud is fifteen lumps, and on fifteen
   * the difference between heaping upward and not is inside the noise — the
   * first version of this check passed against lumps placed all the way round.
   * Across eight it is not close: 0.8% of them below the middle against 46%.
   */
  var lift = 0, counted = 0;
  for (var heapSeed = 1; heapSeed <= 8; heapSeed++) {
    var c3 = recorder(480, 360);
    PAINT.billow(c3, 240, 120, 90, 30, 2,
      PROMPT.rng(PROMPT.parse('open plains at noon', { seed: heapSeed }), 'cloud'),
      'rgba(255,255,255,1)');
    var lot = shapesOf(c3);
    var root = lot[0];
    var mid = root.y + root.h / 2;
    lot.slice(1).forEach(function (sh) {
      /* How far above the middle it sits, as a share of the whole lump's
       * height. A count of above-versus-below is a cliff — it read 10.4%
       * against a 10% bar, which is no separation at all — where the average
       * offset separates the two cases by a mile. */
      lift += (mid - (sh.y + sh.h / 2)) / Math.max(root.h, 1);
      counted++;
    });
  }
  var heaped = lift / Math.max(counted, 1);
  /* Measured both ways: 0.67 heaped upward, 0.38 placed all the way round.
   * Both are deterministic, so the bar sits between them rather than near
   * either. */
  check(counted > 60 && heaped > 0.52,
    'and they heap upward rather than all the way round — their middles sit ' +
    heaped.toFixed(2) + ' of a lump above its centre');

  /* It has to stop. A recursion with no floor is a hang, and lumps under a
   * pixel are a cost with nothing to show for it. */
  var tiny = lumpsOf(6, 7, 3);
  check(tiny.length < 40,
    'and it stops when the lumps get small, whatever depth it is asked for (' +
    tiny.length + ' from a lump seven pixels across)');

  /*
   * The overcast deck. Not a lid of paint but the underside of something
   * enormous, which is all texture: a heavier mass here, a thinner patch
   * there.
   */
  function skyOf(weather) {
    var spec = PROMPT.parse('open plains at noon', { seed: 9 });
    spec.weather = weather;
    spec.weatherStrength = 1;
    var P = PAINT.makePalette(spec);
    var ctx = recorder(480, 360);
    PAINT.clouds(ctx, 480, 360, 200, P, spec, PROMPT.rng(spec, 'cloud'), { x: 60, y: 20 });
    return { shapes: shapesOf(ctx), ctx: ctx, P: P };
  }
  var dull = skyOf('clouds'), fine = skyOf('clear');
  /* Big shapes up in the sky: the mass in the deck, which a gradient has none
   * of. Measured against a clear sky, which has no deck to have mass in. */
  function masses(s) {
    return s.shapes.filter(function (sh) { return sh.w > 110 && sh.y < 200; }).length;
  }
  check(masses(dull) > masses(fine) + 3,
    'an overcast sky has mass in it rather than one flat grey lid (' +
    masses(dull) + ' big shapes against a clear sky\'s ' + masses(fine) + ')');

  /* And some of it thinner than the deck, because a break in the cloud is a
   * real thing and a sky that only ever thickens is a ceiling. */
  function lightness(css) {
    var m = /hsla?\([-0-9.]+,\s*[-0-9.]+%,\s*([-0-9.]+)%/.exec(css || '');
    return m ? parseFloat(m[1]) : null;
  }
  var tones = {};
  dull.shapes.forEach(function (sh) {
    if (sh.w > 110 && sh.y < 200) {
      var v = lightness(sh.colour);
      if (v != null) tones[Math.round(v)] = true;
    }
  });
  check(Object.keys(tones).length >= 2,
    'in more than one tone, so the cloud thins somewhere as well as thickening (' +
    Object.keys(tones).join('%, ') + '%)');

  /* Fog is the one weather that should stay featureless — it is the absence of
   * anything to see, and giving it towers would be the picture disagreeing
   * with the word. */
  var mist = skyOf('fog');
  check(masses(mist) < masses(dull),
    'and fog stays the flat thing fog is (' + masses(mist) + ' against ' +
    masses(dull) + ')');
  pass('cloud has structure at every size, and overcast has mass');
})();

/*
 * An edge worth having.
 *
 * Measured with the coastline method — take a silhouette as a mask, count its
 * boundary pixels, halve the resolution, count again — every subject in this
 * engine came out between 1.01 and 1.13, where a smooth mathematical curve is
 * 1.0. A wolf measured 1.032: as smooth as a crystal. That is what being built
 * out of ellipses and swept curves does, and no amount of light, texture or
 * weather fixes it, because the shape is already wrong when they arrive.
 */
(function anEdgeWorthHaving() {
  console.log('\nAn edge worth having');

  var SUBJECTS = require('../js/subjects.js');

  /* Every point a path was built from. */
  function pathOf(fn) {
    var ctx = recorder(400, 400);
    fn(ctx);
    var pts = [];
    ctx.log.forEach(function (c) {
      if (c.op === 'moveTo' || c.op === 'lineTo') pts.push([c.args[0], c.args[1]]);
    });
    return pts;
  }
  function ringOf(amount) {
    return pathOf(function (ctx) {
      SUBJECTS.roughEllipse(ctx, 200, 200, 80, 80, 0, 200, amount);
    });
  }

  var ring = ringOf(1);
  check(ring.length > 30, 'a circle is walked round rather than swept (' + ring.length + ' points)');

  /* It is no longer a circle, which is the point. */
  var offs = ring.map(function (pt) {
    return Math.sqrt((pt[0] - 200) * (pt[0] - 200) + (pt[1] - 200) * (pt[1] - 200)) - 80;
  });
  var worst = Math.max.apply(null, offs.map(Math.abs));
  var swing = Math.max.apply(null, offs) - Math.min.apply(null, offs);
  check(worst > 1.5 && worst < 80 * 0.25,
    'and its edge wanders — up to ' + worst.toFixed(1) + 'px off a true radius ' +
    'of 80, which is an edge rather than a deformity');
  check(swing > 2.5,
    'in and out rather than simply bigger (' + swing.toFixed(1) + 'px between ' +
    'its closest and furthest)');

  /* Smooth, not fizzing: neighbours along the edge agree with each other. */
  var jump = 0;
  for (var i = 1; i < offs.length; i++) jump = Math.max(jump, Math.abs(offs[i] - offs[i - 1]));
  check(jump < swing * 0.6,
    'and it wanders rather than fizzes — no two neighbouring points differ by ' +
    'more than ' + jump.toFixed(1) + 'px while the edge spans ' + swing.toFixed(1));

  /*
   * The same wander every time, because it is a function of where a point is
   * and nothing else. This matters more than it looks: the painter redraws
   * every subject a dozen times over — as its own shadow, its rim light, its
   * coat, its settled snow, its wear — and if the edge were rolled from the
   * random stream each of those passes would wander differently and a wolf's
   * shadow would not be the shape of the wolf.
   */
  var again = ringOf(1);
  var drift = 0;
  for (var j = 0; j < Math.min(ring.length, again.length); j++) {
    drift = Math.max(drift, Math.abs(ring[j][0] - again[j][0]),
      Math.abs(ring[j][1] - again[j][1]));
  }
  check(ring.length === again.length && drift === 0,
    'and the same shape drawn again comes out identical, so a subject and its ' +
    'own shadow have one outline between them');

  /* And it closes. A field of position gives this for nothing: the last point
   * and the first are in the same place, so they agree. */
  var first = ring[0], last = ring[ring.length - 1];
  check(Math.abs(first[0] - last[0]) < 0.001 && Math.abs(first[1] - last[1]) < 0.001,
    'and the loop closes on itself exactly, with no seam where it meets');

  /*
   * A long line and a line chopped into pieces must wander by the same amount.
   * The first attempt measured the wander against each segment's own length,
   * which did nothing at all to a wolf — a wolf is eight hundred and fifteen
   * straight lines, each three pixels long, and five per cent of three pixels
   * is nothing. Size has to come from the subject, not the segment.
   */
  function deviation(pts, x0, y0, x1, y1) {
    var dx = x1 - x0, dy = y1 - y0;
    var len = Math.sqrt(dx * dx + dy * dy) || 1;
    var most = 0;
    pts.forEach(function (pt) {
      most = Math.max(most, Math.abs((pt[0] - x0) * dy - (pt[1] - y0) * dx) / len);
    });
    return most;
  }
  var oneGo = pathOf(function (ctx) {
    var c = SUBJECTS.rough(ctx, 200, 1);
    c.beginPath(); c.moveTo(60, 200); c.lineTo(340, 200);
  });
  var inBits = pathOf(function (ctx) {
    var c = SUBJECTS.rough(ctx, 200, 1);
    c.beginPath(); c.moveTo(60, 200);
    for (var k = 1; k <= 80; k++) c.lineTo(60 + (280 * k) / 80, 200);
  });
  var whole = deviation(oneGo, 60, 200, 340, 200);
  var chopped = deviation(inBits, 60, 200, 340, 200);
  check(whole > 2, 'one long line wanders off true by ' + whole.toFixed(1) + 'px');
  check(chopped > whole * 0.5,
    'and the same line drawn in eighty pieces wanders just as far (' +
    chopped.toFixed(1) + 'px) — the size comes from the subject, not the segment');

  /*
   * How much depends on what the thing is. A stag's outline is fur and should
   * fray; a tower has been rained on but is still a tower, and one that
   * wobbles like a bush reads as a melted one. A crystal should be sharp —
   * that is the whole point of a crystal.
   */
  /*
   * Asked of the painter, not read out of its own table. How much of a path is
   * wander rather than shape is the difference between drawing a subject the
   * way the painter does and drawing the same subject with the wander turned
   * off, so each thing is compared against itself and a tower built of
   * straight lines is not held against a stag built of curves.
   */
  /*
   * How far along that scale each thing is pushed. The wander moves points
   * without changing where they are sampled, so the same subject drawn with
   * the wander off and with it fully on gives two paths of the same length
   * that can be compared point for point. Where the painter's own output sits
   * between those two is the amount it chose — asked of it, rather than read
   * back out of the table it reads.
   */
  function apart(a, b) {
    var n = Math.min(a.length, b.length), sum = 0;
    for (var k = 0; k < n; k++) {
      sum += Math.abs(a[k][0] - b[k][0]) + Math.abs(a[k][1] - b[k][1]);
    }
    return n ? sum / n : 0;
  }
  function chosen(text) {
    var spec = PROMPT.parse(text, { seed: 4, style: 'auto' });
    var P = PAINT.makePalette(spec);
    var box = { x: 60, y: 40, w: 280, h: 320, depth: 0, anchor: 'ground' };
    var span = Math.min(box.w, box.h);
    function run(amount) {
      return pathOf(function (ctx) {
        var c = amount == null
          ? ctx
          : SUBJECTS.rough(ctx, span, amount);
        if (amount == null) {
          SUBJECTS.draw(ctx, spec.subject, box, P, PROMPT.rng(spec, 'subject'), spec);
        } else {
          SUBJECTS.DRAW[spec.subject.draw](c, box, P,
            PROMPT.rng(spec, 'subject'), spec, spec.subject.form);
        }
      });
    }
    var painter = run(null), off = run(0), full = run(1);
    var span2 = apart(full, off);
    return span2 > 0 ? apart(painter, off) / span2 : 0;
  }
  var furry = chosen('a stag'), built = chosen('a stone tower'), cut = chosen('a crystal');
  check(furry > built * 1.8,
    'a stag frays more than a tower does (' + furry.toFixed(2) + ' of the way ' +
    'against ' + built.toFixed(2) + ')');
  check(built > cut,
    'and a tower more than a crystal, which should be sharp (' +
    built.toFixed(2) + ' against ' + cut.toFixed(2) + ')');

  /* A part-arc is a curve somebody meant, not an outline, and is left alone. */
  var pie = pathOf(function (ctx) {
    var c = SUBJECTS.rough(ctx, 200, 1);
    c.beginPath(); c.arc(200, 200, 80, 0, Math.PI * 0.5);
  });
  check(pie.length === 0,
    'a part-arc is passed straight through — it is a curve somebody drew, not an outline');

  /*
   * And it survives a context that will not be written to.
   *
   * `canvas` is read-only on a real browser context and an ordinary property
   * on the recording one this suite uses, so forwarding it as a setter throws
   * in a browser and passes in Node. That is the exact shape of bug a
   * pixels-never-touched test suite cannot see, and it got through: every
   * picture in the browser threw on the first subject drawn.
   */
  var strict = {};
  ['save', 'restore', 'beginPath', 'closePath', 'moveTo', 'lineTo', 'fill',
   'stroke', 'quadraticCurveTo', 'arc', 'ellipse', 'fillRect', 'translate',
   'rotate', 'scale', 'clip', 'rect'].forEach(function (k) { strict[k] = function () {}; });
  /* A styling property that refuses to be written to, which is what the guard
   * is actually for — `canvas` is skipped outright, so leaving it as the only
   * read-only thing here tested nothing. */
  Object.defineProperty(strict, 'fillStyle', {
    get: function () { return '#000'; },
    enumerable: true, configurable: false
  });
  Object.defineProperty(strict, 'canvas', {
    get: function () { return { width: 400, height: 400 }; },
    enumerable: true, configurable: false
  });
  var blewUp = null;
  try {
    var wrapped = SUBJECTS.rough(strict, 200, 1);
    wrapped.fillStyle = '#fff';
    wrapped.beginPath(); wrapped.moveTo(10, 10); wrapped.lineTo(90, 90); wrapped.fill();
  } catch (e) { blewUp = e.message; }
  check(blewUp === null,
    'and a context whose properties refuse to be written to is handled rather ' +
    'than thrown at' + (blewUp ? ' — ' + blewUp : ''));
  pass('outlines wander the way real ones do, and every pass agrees on how');
})();

/*
 * Colour gives out at the top.
 *
 * Measured across the engine, the brightest tenth of a picture was its most
 * saturated part: 60% against 38% in the midtones for a stag at noon, 64%
 * against 21% for a ship at sunset, 76% against 46% for a desert. In a
 * photograph that is the wrong way round, always. A sensor approaching full
 * well runs out of headroom in its brightest channel first, so the channels
 * converge and the colour drains towards white. Nothing drawn does this — a
 * paint bucket does not run out of headroom — and colour that keeps its full
 * strength all the way to white is one of the most reliable marks of a
 * picture that came out of a program.
 */
(function colourGivesOut() {
  console.log('\nColour gives out at the top');

  function hsl(r, g, b) {
    r /= 255; g /= 255; b /= 255;
    var mx = Math.max(r, g, b), mn = Math.min(r, g, b), l = (mx + mn) / 2, h = 0, s = 0;
    if (mx !== mn) {
      var d = mx - mn;
      s = l > 0.5 ? d / (2 - mx - mn) : d / (mx + mn);
      h = mx === r ? (g - b) / d + (g < b ? 6 : 0) : mx === g ? (b - r) / d + 2 : (r - g) / d + 4;
      h *= 60;
    }
    return [h, s * 100, l * 100];
  }
  /* A strip of one hue climbing from dark to white, at full strength all the
   * way — which is what a paint program gives you and a camera never does. */
  function ramp(n) {
    var img = { data: new Uint8ClampedArray(n * 4), width: n, height: 1 };
    for (var i = 0; i < n; i++) {
      var t = i / (n - 1);
      /* An orange held at full saturation as it brightens. */
      var l = t;
      var c = l < 0.5 ? l * 2 : 1;
      var lo = l < 0.5 ? 0 : (l - 0.5) * 2;
      img.data[i * 4] = Math.round(255 * c);
      img.data[i * 4 + 1] = Math.round(255 * (lo + (c - lo) * 0.55));
      img.data[i * 4 + 2] = Math.round(255 * lo);
      img.data[i * 4 + 3] = 255;
    }
    return img;
  }
  function satAt(img, i) {
    return hsl(img.data[i * 4], img.data[i * 4 + 1], img.data[i * 4 + 2])[1];
  }
  function hueAt(img, i) {
    return hsl(img.data[i * 4], img.data[i * 4 + 1], img.data[i * 4 + 2])[0];
  }

  var N = 41;
  var before = ramp(N), after = ramp(N);
  FINISH.helpers.shoulder(after, FINISH.helpers.SHOULDER);

  /* The top of the strip loses its colour. */
  var high = 34;                                  /* about 85% bright */
  check(satAt(after, high) < satAt(before, high) * 0.55,
    'a colour near white comes back with most of its strength gone (' +
    satAt(before, high).toFixed(0) + '% down to ' + satAt(after, high).toFixed(0) + '%)');

  /* And the middle does not. */
  /* Well clear of the top: this strip is fully saturated all the way up, so
   * even at 40% lightness its red channel is at 204 and legitimately inside
   * the shoulder. The midtone sample has to be somewhere with real headroom. */
  var midIdx = 10;                                /* red channel at 128 of 255 */
  check(Math.abs(satAt(after, midIdx) - satAt(before, midIdx)) < 3,
    'while the midtones keep theirs (' + satAt(before, midIdx).toFixed(0) +
    '% to ' + satAt(after, midIdx).toFixed(0) + '%)');

  /* It has to be a slope rather than a step, or it shows as a band across
   * every bright surface. */
  /*
   * Measured across the part of the strip the shoulder actually touches. Both
   * ends of it are artefacts of the strip rather than of anything under test:
   * it starts at pure black and finishes at pure white, and both have no
   * saturation by definition, so the steps in and out of them are a hundred
   * points wide whatever the shoulder does.
   */
  var steps = [];
  for (var i = midIdx + 1; i < N - 1; i++) {
    steps.push(Math.abs(satAt(after, i) - satAt(after, i - 1)));
  }
  var biggest = Math.max.apply(null, steps);
  var span = satAt(after, midIdx) - satAt(after, N - 2);
  check(biggest < Math.abs(span) * 0.45,
    'and it comes on gradually rather than as a line across the picture ' +
    '(worst step ' + biggest.toFixed(1) + '% over a fall of ' + Math.abs(span).toFixed(1) + '%)');

  /* Draining colour, not changing it: an orange goes pale orange, not pink. */
  /* Only where there is enough colour left to have a hue worth measuring: at
   * three per cent the angle is numerical noise. */
  var turned = 0;
  for (var j = 20; j < N - 1; j++) {
    if (satAt(after, j) > 12) {
      turned = Math.max(turned, Math.abs(((hueAt(after, j) - hueAt(before, j)) % 360 + 540) % 360 - 180));
    }
  }
  check(turned < 4,
    'and it drains the colour rather than turning it — the hue moves at most ' +
    turned.toFixed(1) + ' degrees');

  /* And it leaves brightness alone: this is the colour running out, not the
   * exposure. */
  function lum(img, i) {
    return 0.2126 * img.data[i * 4] + 0.7152 * img.data[i * 4 + 1] + 0.0722 * img.data[i * 4 + 2];
  }
  var moved = 0;
  for (var k = 0; k < N; k++) moved = Math.max(moved, Math.abs(lum(after, k) - lum(before, k)));
  check(moved < 1.5,
    'and nothing gets brighter or darker for it (' + moved.toFixed(2) + ' levels at worst)');

  /* A grey has nothing to lose and must come back untouched. */
  var grey = { data: new Uint8ClampedArray(4 * 8), width: 8, height: 1 };
  for (var g = 0; g < 8; g++) {
    var v = 140 + g * 16;
    grey.data[g * 4] = grey.data[g * 4 + 1] = grey.data[g * 4 + 2] = v;
    grey.data[g * 4 + 3] = 255;
  }
  var was = Array.prototype.slice.call(grey.data);
  FINISH.helpers.shoulder(grey, FINISH.helpers.SHOULDER);
  var same = was.every(function (v, i) { return Math.abs(v - grey.data[i]) < 1; });
  check(same, 'and a grey, having no colour to lose, comes back exactly as it went in');

  /*
   * Through the whole finisher, which is where the fault was measured. The
   * buffer is seeded with bright, strongly coloured pixels — the thing a
   * photograph does not contain — and they have to come back weaker.
   */
  var w = 64, h = 48;
  var spec = PROMPT.parse('a stone tower in a meadow at noon', { seed: 4, style: 'auto' });
  var ctx = new FakeContext(w, h);
  var P = PAINT.render(ctx, w, h, spec);
  for (var q = 0; q < w * h; q++) {
    ctx._pixels[q * 4] = 252;
    ctx._pixels[q * 4 + 1] = 176;
    ctx._pixels[q * 4 + 2] = 40;
    ctx._pixels[q * 4 + 3] = 255;
  }
  var started = hsl(252, 176, 40)[1];
  FINISH.apply(ctx, w, h, spec, P);
  var got = ctx.getImageData(0, 0, w, h);
  var ended = 0;
  for (var z = 0; z < w * h; z++) {
    ended += hsl(got.data[z * 4], got.data[z * 4 + 1], got.data[z * 4 + 2])[1];
  }
  ended /= w * h;
  check(ended < started * 0.7,
    'and a picture full of bright strong colour comes out of the finisher ' +
    'with it given up (' + started.toFixed(0) + '% down to ' + ended.toFixed(0) + '%)');
  pass('bright colour gives out the way it does in a camera');
})();

console.log('\n' + (failures ? 'FAILED ' + failures + ' of ' + checks + ' checks'
  : 'All ' + checks + ' checks passed'));
process.exit(failures ? 1 : 0);
