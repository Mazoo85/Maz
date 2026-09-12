/*
 * CODA PICS — the reader.
 * -----------------------
 * Turns what a person typed into a scene the painter can draw:
 *
 *   CodaPrompt.parse('a red dragon over snowy mountains at sunset, neon')
 *     -> { subject:{id:'dragon',...}, scene:{id:'snow',...}, time:'dusk',
 *          style:'neon', palette:{id:'crimson',...}, mood:0.5, read:[...] }
 *
 * Two rules hold the whole app together:
 *
 *   1. It always returns a complete scene. There is no "I didn't understand" —
 *      anything the words don't settle is decided from a hash of the prompt
 *      itself, so even nonsense paints something, and paints it consistently.
 *   2. The same prompt and seed always produce the same scene, on any machine.
 *      Every random choice downstream comes from `rng(spec)`, never Math.random.
 *
 * `read` records what each word was understood as, so the app can show the
 * person why they got the picture they got.
 *
 * Exposed as window.CodaPrompt (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  var LEX = root.CodaLexicon ||
    (typeof require !== 'undefined' ? require('./lexicon.js') : null);

  /* ------------------------------------------------------------ randomness
   * A 32-bit string hash and mulberry32, so "same words, same picture" holds
   * across browsers and across Node (the tests rely on it).
   */
  function hash(str) {
    var h = 2166136261 >>> 0;
    for (var i = 0; i < str.length; i++) {
      h ^= str.charCodeAt(i);
      h = Math.imul(h, 16777619) >>> 0;
    }
    return h >>> 0;
  }

  function rngFrom(seed) {
    var a = seed >>> 0;
    return function () {
      a = (a + 0x6D2B79F5) >>> 0;
      var t = a;
      t = Math.imul(t ^ (t >>> 15), t | 1);
      t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }

  /* A fresh generator for one spec. `salt` keeps independent parts of the
   * painting independent — the clouds re-rolling must not move the mountains. */
  function rng(spec, salt) {
    return rngFrom(hash(String(spec.prompt) + '|' + spec.seed + '|' + (salt || '')));
  }

  /* ------------------------------------------------------------- tokenising */
  function normalise(text) {
    return String(text || '')
      .toLowerCase()
      .replace(/[^a-z0-9]+/g, ' ')
      .replace(/\s+/g, ' ')
      .trim();
  }

  function tokens(flat) {
    return flat ? flat.split(' ') : [];
  }

  /* Words are matched exactly, then as a simple plural, so "dragons" and
   * "wolves" find their entry without a stemming library. */
  function variants(word) {
    var out = [word];
    if (word.length > 3 && /s$/.test(word)) out.push(word.slice(0, -1));
    if (word.length > 4 && /es$/.test(word)) out.push(word.slice(0, -2));
    if (word.length > 4 && /ves$/.test(word)) out.push(word.slice(0, -3) + 'f');
    if (word.length > 4 && /ies$/.test(word)) out.push(word.slice(0, -3) + 'y');
    return out;
  }

  /*
   * Find the entries of one table that the prompt mentions, best first.
   * A multi-word phrase ("hot air balloon", "northern lights") is matched
   * against the flattened prompt and beats any single word, because it is the
   * more specific reading.
   */
  function findAll(table, flat, tokenList) {
    var hits = [];
    table.forEach(function (entry) {
      var best = null;
      (entry.words || []).forEach(function (w) {
        if (w.indexOf(' ') >= 0) {
          if (flat.indexOf(w) >= 0) {
            var phrase = { entry: entry, word: w, at: flat.indexOf(w), specific: 2 };
            if (!best || best.specific < 2) best = phrase;
          }
          return;
        }
        for (var i = 0; i < tokenList.length; i++) {
          var vs = variants(tokenList[i]);
          for (var v = 0; v < vs.length; v++) {
            if (vs[v] === w) {
              var single = { entry: entry, word: tokenList[i], at: i, specific: 1 };
              if (!best) best = single;
              return;
            }
          }
        }
      });
      if (best) hits.push(best);
    });
    hits.sort(function (a, b) {
      return b.specific - a.specific || a.at - b.at;
    });
    return hits;
  }

  function findOne(table, flat, tokenList) {
    var hits = findAll(table, flat, tokenList);
    return hits.length ? hits[0] : null;
  }

  function byId(table, id) {
    for (var i = 0; i < table.length; i++) if (table[i].id === id) return table[i];
    return null;
  }

  var UTIL = root.MazUtil ||
    (typeof require !== 'undefined' ? require('../../shared/maz-util.js') : {});
  var pick = UTIL.pick;

  /* ------------------------------------------------------------- defaulting
   * Nothing here is "random" in the everyday sense: an unstated hour, style or
   * setting is derived from the prompt's own hash, so one prompt keeps one
   * identity while two different prompts rarely look alike.
   */
  var DEFAULT_STYLE_BY_SCENE = {
    space: ['neon', 'poster', 'lowpoly'],
    city: ['neon', 'comic', 'retro16'],
    snow: ['watercolour', 'poster', 'storybook'],
    ocean: ['ukiyo', 'watercolour', 'poster'],
    lake: ['ukiyo', 'watercolour', 'oil'],
    forest: ['storybook', 'oil', 'watercolour'],
    jungle: ['storybook', 'oil', 'poster'],
    desert: ['poster', 'oil', 'retro16'],
    volcano: ['comic', 'oil', 'neon'],
    ruins: ['noir', 'oil', 'poster'],
    cave: ['glass', 'lowpoly', 'noir'],
    mountains: ['poster', 'watercolour', 'lowpoly'],
    meadow: ['storybook', 'watercolour', 'oil'],
    plains: ['poster', 'oil', 'photo'],
    canyon: ['poster', 'lowpoly', 'oil'],
    swamp: ['noir', 'oil', 'storybook'],
    shore: ['watercolour', 'ukiyo', 'photo'],
    island: ['poster', 'storybook', 'watercolour'],
    road: ['neon', 'photo', 'noir'],
    sky: ['storybook', 'watercolour', 'poster']
  };

  var NIGHT_SCENES = { space: true, cave: true };

  /* ----------------------------------------------------------------- parse */
  function parse(text, options) {
    options = options || {};
    var prompt = String(text == null ? '' : text).trim();
    var flat = normalise(prompt);
    var toks = tokens(flat);
    var seed = options.seed == null ? 1 : (options.seed | 0);
    var base = rngFrom(hash(flat + '|' + seed));
    var read = [];

    function note(category, label, word) {
      read.push({ category: category, label: label, word: word || null });
    }

    /* --- subject (and an optional second one) --- */
    var subjectHits = findAll(LEX.SUBJECTS, flat, toks);
    var sceneHit = findOne(LEX.SCENES, flat, toks);

    var subject = null;
    var companion = null;
    if (subjectHits.length) {
      subject = buildSubject(subjectHits[0], flat, toks, base);
      note('subject', subjectHits[0].entry.label, subjectHits[0].word);
      if (subjectHits.length > 1) {
        companion = buildSubject(subjectHits[1], flat, toks, base);
        companion.scale *= 0.62;
        note('subject', 'and ' + subjectHits[1].entry.label, subjectHits[1].word);
      }
    }

    /* --- setting --- */
    var scene;
    if (sceneHit) {
      scene = sceneHit.entry;
      note('scene', scene.label, sceneHit.word);
    } else if (subject) {
      scene = byId(LEX.SCENES, subject.scene) || LEX.SCENES[0];
      note('scene', scene.label + ' (where it belongs)', null);
    } else {
      scene = pick(LEX.SCENES, base);
      note('scene', scene.label + ' (my choice)', null);
    }

    /* A prompt with no subject at all is a landscape, and a good one — but a
     * bare "" would be an empty field, so give the hash something to stand
     * in it. */
    if (!subject && !sceneHit) {
      var invented = pick(LEX.SUBJECTS, base);
      subject = buildSubject({ entry: invented, word: null }, flat, toks, base);
      note('subject', invented.label + ' (my choice)', null);
    }

    /* --- hour --- */
    var timeHit = findOne(LEX.TIMES, flat, toks);
    var time;
    if (timeHit) {
      time = timeHit.entry.id;
      note('time', timeHit.entry.label, timeHit.word);
    } else if (NIGHT_SCENES[scene.id]) {
      time = 'night';
    } else {
      time = ['dawn', 'day', 'dusk', 'night'][Math.floor(base() * 4) % 4];
    }

    /* --- weather --- */
    var weatherHit = findOne(LEX.WEATHER, flat, toks);
    var weather;
    if (weatherHit) {
      weather = weatherHit.entry.id;
      note('weather', weatherHit.entry.label, weatherHit.word);
    } else if (scene.id === 'snow' && base() < 0.5) {
      weather = 'snowfall';
    } else if (scene.id === 'space') {
      weather = 'clear';
    } else {
      weather = base() < 0.68 ? 'clear' : 'clouds';
    }

    /* --- mood --- */
    var moodHit = findOne(LEX.MOODS, flat, toks);
    var mood = moodHit ? moodHit.entry.mood : 0.35 + base() * 0.3;
    if (moodHit) note('mood', moodHit.entry.id, moodHit.word);
    if (time === 'night') mood = Math.min(1, mood + 0.08);

    /* --- palette --- */
    var paletteHit = findOne(LEX.PALETTES, flat, toks);
    var palette = paletteHit ? paletteHit.entry : null;
    if (palette) note('colour', palette.label, paletteHit.word);

    /* --- style --- */
    var style;
    var forced = options.style && options.style !== 'auto' ? options.style : null;
    if (forced && byId(LEX.STYLES, forced)) {
      style = forced;
      note('style', byId(LEX.STYLES, style).label + ' (you chose it)', null);
    } else {
      var styleHit = findOne(LEX.STYLES, flat, toks);
      if (styleHit) {
        style = styleHit.entry.id;
        note('style', styleHit.entry.label, styleHit.word);
      } else {
        var candidates = DEFAULT_STYLE_BY_SCENE[scene.id] || ['poster', 'oil', 'watercolour'];
        style = pick(candidates, base);
        note('style', byId(LEX.STYLES, style).label + ' (my choice)', null);
      }
    }

    return {
      prompt: prompt,
      seed: seed,
      subject: subject,
      companion: companion,
      scene: { id: scene.id, label: scene.label, prep: scene.prep || 'in', horizon: scene.horizon },
      time: time,
      weather: weather,
      style: style,
      palette: palette,
      mood: Math.max(0, Math.min(1, mood)),
      read: read
    };
  }

  function buildSubject(hit, flat, toks, base) {
    var entry = hit.entry;
    var scale = 1;
    LEX.SCALE_WORDS.forEach(function (s) {
      s.words.forEach(function (w) {
        if (toks.indexOf(w) >= 0) scale = s.factor;
      });
    });
    var count = 1;
    LEX.COUNT_WORDS.forEach(function (c) {
      c.words.forEach(function (w) {
        if (w !== 'a' && toks.indexOf(w) >= 0) count = c.count;
      });
    });
    /* "wolves", "birds" — a plural asks for more than one, unless a number
     * word already said how many. A word counts as plural when its singular
     * is also a word this entry knows, which catches both the plurals the
     * lexicon lists ("wolves") and the ones it doesn't ("dragons"). */
    if (count === 1 && hit.word && /s$/.test(hit.word)) {
      var singulars = variants(hit.word).slice(1);
      for (var si = 0; si < singulars.length; si++) {
        if (entry.words.indexOf(singulars[si]) >= 0) {
          count = 2 + Math.floor(base() * 2);
          break;
        }
      }
    }
    return {
      id: entry.id,
      label: entry.label,
      draw: entry.draw,
      form: entry.form || entry.id,
      scene: entry.scene,
      scale: scale,
      count: Math.max(1, Math.min(5, count))
    };
  }

  /* --------------------------------------------------------------- describe
   * One plain sentence for the "what I read" line, written the way a person
   * would say it back.
   */
  function describe(spec) {
    var parts = [];
    if (spec.subject) {
      parts.push(spec.subject.count > 1
        ? spec.subject.count + ' × ' + spec.subject.label
        : spec.subject.label);
    }
    if (spec.companion) parts.push('with ' + spec.companion.label);
    parts.push((spec.scene.prep || 'in') + ' ' + spec.scene.label);
    var t = byId(LEX.TIMES, spec.time);
    if (t) parts.push(t.label);
    var w = byId(LEX.WEATHER, spec.weather);
    if (w && spec.weather !== 'clear') parts.push(w.label);
    var s = byId(LEX.STYLES, spec.style);
    if (s) parts.push('· ' + s.label);
    return parts.join(' ').replace(' · ', ' · ');
  }

  /* -------------------------------------------------------------- surprise
   * A random prompt in the app's own vocabulary, for the "Surprise me"
   * button — so a blank page is never a blank page.
   */
  var SURPRISE_SHAPES = [
    '{adj} {subject} {prep} {scene}, {time}, {style}',
    '{subject} {prep} {scene} {time}',
    '{adj} {subject}, {scene}, {style}',
    '{colour} {subject} {prep} {scene}, {style}'
  ];
  var SURPRISE_ADJ = ['a lonely', 'an ancient', 'a giant', 'a tiny', 'a glowing', 'a ruined', 'a peaceful', 'an epic'];
  var SURPRISE_PREP = ['over', 'in', 'above', 'beside', 'deep in', 'at the edge of'];

  function surprise(seed) {
    var r = rngFrom(hash('surprise|' + (seed == null ? Date.now() : seed)));
    var shape = pick(SURPRISE_SHAPES, r);
    var subject = pick(LEX.SUBJECTS, r);
    var scene = pick(LEX.SCENES, r);
    var time = pick(LEX.TIMES, r);
    var style = pick(LEX.STYLES, r);
    var colour = pick(LEX.PALETTES, r);
    return shape
      .replace('{adj}', pick(SURPRISE_ADJ, r))
      .replace('{colour}', 'a ' + colour.words[0])
      .replace('{subject}', subject.words[0])
      .replace('{prep}', pick(SURPRISE_PREP, r))
      .replace('{scene}', scene.words[0])
      .replace('{time}', time.words[0])
      .replace('{style}', style.words[0]);
  }

  var API = {
    parse: parse,
    describe: describe,
    surprise: surprise,
    rng: rng,
    rngFrom: rngFrom,
    hash: hash,
    normalise: normalise
  };

  root.CodaPrompt = API;
  if (typeof module !== 'undefined' && module.exports) module.exports = API;
})(typeof window !== 'undefined' ? window : this);
