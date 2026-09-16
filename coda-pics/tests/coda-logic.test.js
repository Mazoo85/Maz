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
  check(mountains.marks > plains.marks * 3,
    'a mountain now takes several times the drawing a field does (' +
    mountains.marks + ' vs ' + plains.marks + ')');
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

console.log('\n' + (failures ? 'FAILED ' + failures + ' of ' + checks + ' checks'
  : 'All ' + checks + ' checks passed'));
process.exit(failures ? 1 : 0);
