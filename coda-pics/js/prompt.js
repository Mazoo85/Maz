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
   * painting independent — the clouds re-rolling must not move the mountains.
   *
   * `spec.locked` is how a part is held still while the rest re-rolls: it maps
   * a salt to the seed that part should keep using, so "another take" can
   * change the sky and leave the subject exactly where it was. */
  function rng(spec, salt) {
    var seed = spec.seed;
    if (spec.locked && Object.prototype.hasOwnProperty.call(spec.locked, salt)) {
      seed = spec.locked[salt];
    }
    return rngFrom(hash(String(spec.prompt) + '|' + seed + '|' + (salt || '')));
  }

  /* The salts each lock covers. Named for what a person would call the part,
   * not for the functions that happen to use them. */
  var LOCKS = {
    subject: ['subject'],
    sky: ['sky', 'light', 'cloud', 'weather'],
    land: ['scene', 'ground', 'fore']
  };

  /* Build the `locked` map for a new take: every salt covered by a held lock
   * keeps the seed it had. */
  function holdLocks(previousSeed, locks) {
    var out = {};
    Object.keys(LOCKS).forEach(function (name) {
      if (!locks || !locks[name]) return;
      LOCKS[name].forEach(function (salt) { out[salt] = previousSeed; });
    });
    return out;
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

  /* How many single-character edits turn one word into another. Bounded: it
   * stops as soon as the answer is past the limit, because a prompt is short
   * and the vocabulary is not. */
  function editDistance(a, b, limit) {
    if (Math.abs(a.length - b.length) > limit) return limit + 1;
    var prev = [], cur = [], i, j;
    for (j = 0; j <= b.length; j++) prev[j] = j;
    for (i = 1; i <= a.length; i++) {
      cur[0] = i;
      var best = cur[0];
      for (j = 1; j <= b.length; j++) {
        cur[j] = Math.min(
          prev[j] + 1,
          cur[j - 1] + 1,
          prev[j - 1] + (a.charAt(i - 1) === b.charAt(j - 1) ? 0 : 1)
        );
        if (cur[j] < best) best = cur[j];
      }
      if (best > limit) return limit + 1;
      prev = cur.slice();
    }
    return prev[b.length];
  }

  /* A near miss on a word nobody typed on purpose. Only for words long enough
   * that a near match means something — "cat" and "bat" are two animals, not a
   * typo, so short words are never corrected. */
  function findNear(table, tokenList) {
    var best = null;
    table.forEach(function (entry) {
      (entry.words || []).forEach(function (w) {
        if (w.length < 5 || w.indexOf(' ') >= 0) return;
        tokenList.forEach(function (t, at) {
          if (t.length < 5) return;
          var limit = t.length > 7 ? 2 : 1;
          var d = editDistance(t, w, limit);
          if (d <= limit && (!best || d < best.distance)) {
            best = { entry: entry, word: t, meant: w, at: at, distance: d, specific: 0 };
          }
        });
      });
    });
    return best;
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
    var locked = options.locked || null;
    var base = rngFrom(hash(flat + '|' + seed));
    var read = [];

    var used = {};                    // which tokens were understood
    function note(category, label, word, meant) {
      /* A phrase match ("hot air balloon") has to mark every word in it, or
       * the words inside it get reported as ones nobody understood. */
      if (word) String(word).split(' ').forEach(function (w) { used[w] = true; });
      read.push({
        category: category, label: label, word: word || null,
        meant: meant || null           // set when a word was read as a near miss
      });
    }

    /* --- subject (and an optional second one) --- */
    var subjectHits = findAll(LEX.SUBJECTS, flat, toks);
    var sceneHit = findOne(LEX.SCENES, flat, toks);

    var corrected = null;
    if (!subjectHits.length) {
      var near = findNear(LEX.SUBJECTS, toks);
      if (near) { subjectHits = [near]; corrected = near; }
    }

    var subject = null;
    var companion = null;
    if (subjectHits.length) {
      subject = buildSubject(subjectHits[0], flat, toks, base);
      note('subject', subjectHits[0].entry.label, subjectHits[0].word,
        corrected ? corrected.meant : null);
      if (subjectHits.length > 1) {
        companion = buildSubject(subjectHits[1], flat, toks, base);
        companion.scale *= 0.62;
        note('subject', 'and ' + subjectHits[1].entry.label, subjectHits[1].word);
      }
    }

    /* Size and count words are read inside buildSubject, which has no way to
     * report back — so they are accounted for here, or a prompt that was fully
     * understood claims it was not. */
    if (subject) {
      LEX.SCALE_WORDS.forEach(function (entry) {
        entry.words.forEach(function (w) {
          if (toks.indexOf(w) >= 0 && subject.scale === entry.factor) {
            note('size', w, w);
          }
        });
      });
      LEX.COUNT_WORDS.forEach(function (entry) {
        entry.words.forEach(function (w) {
          if (w !== 'a' && toks.indexOf(w) >= 0 && subject.count === entry.count) {
            note('how many', w, w);
          }
        });
      });
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
    var extraStyle = null;
    var forced = options.style && options.style !== 'auto' ? options.style : null;
    if (forced && byId(LEX.STYLES, forced)) {
      style = forced;
      note('style', byId(LEX.STYLES, style).label + ' (you chose it)', null);
    } else {
      var styleHits = findAll(LEX.STYLES, flat, toks);
      if (styleHits.length) {
        style = styleHits[0].entry.id;
        note('style', styleHits[0].entry.label, styleHits[0].word);
        /* Two style words are not a contradiction to resolve — they are a
         * request for both, and the passes run in the order they were asked
         * for. "Watercolour pixel art" is a real thing to want. */
        if (styleHits.length > 1) {
          extraStyle = styleHits[1].entry.id;
          note('style', 'and ' + styleHits[1].entry.label, styleHits[1].word);
        }
      } else {
        var candidates = DEFAULT_STYLE_BY_SCENE[scene.id] || ['poster', 'oil', 'watercolour'];
        style = pick(candidates, base);
        note('style', byId(LEX.STYLES, style).label + ' (my choice)', null);
      }
    }

    /* --- how the two things are arranged ---
     * Only meaningful with two subjects, and only when the word sits between
     * them: "a cat under a tree" places the cat, "under a tree a cat sits"
     * says the same thing and lands the same way. */
    var relation = null;
    if (subject && companion && subjectHits.length > 1) {
      var a = Math.min(subjectHits[0].at, subjectHits[1].at);
      var b = Math.max(subjectHits[0].at, subjectHits[1].at);
      var relHits = findAll(LEX.RELATIONS, flat, toks);
      for (var ri = 0; ri < relHits.length; ri++) {
        if (relHits[ri].at > a && relHits[ri].at < b) {
          relation = { id: relHits[ri].entry.id, of: 'companion' };
          /* The word order says which one is placed: the first-named subject
           * is the one the preposition is about. */
          if (subjectHits[0].at > subjectHits[1].at) relation.of = 'subject';
          note('placing', subject.label + ' ' + relHits[ri].entry.label + ' ' + companion.label,
            relHits[ri].word);
          break;
        }
      }
    }

    /* --- what it could not use ---
     * Silence here is the worst answer: somebody types "a griffin" and gets a
     * fox with no idea why. */
    var filler = {};
    LEX.FILLER.forEach(function (f) { filler[f] = true; });
    /* A relation word is understood English even when there is only one thing
     * in the picture for it to be about. */
    LEX.RELATIONS.forEach(function (rel) {
      rel.words.forEach(function (w) {
        w.split(' ').forEach(function (part) { filler[part] = true; });
      });
    });
    var unknown = [];
    toks.forEach(function (t) {
      if (t.length < 3 || used[t] || filler[t] || /^[0-9]+$/.test(t)) return;
      if (unknown.indexOf(t) < 0) unknown.push(t);
    });

    return {
      prompt: prompt,
      seed: seed,
      locked: locked,
      subject: subject,
      companion: companion,
      relation: relation,
      scene: { id: scene.id, label: scene.label, prep: scene.prep || 'in', horizon: scene.horizon },
      time: time,
      weather: weather,
      style: style,
      styles: extraStyle ? [style, extraStyle] : [style],
      palette: palette,
      mood: Math.max(0, Math.min(1, mood)),
      unknown: unknown,
      read: read,
      // How much of the picture came from the words, and how much CODA PICS
      // chose. `read` has recorded this all along — an entry with a null
      // `word` is something invented — but nothing summarised it, so there was
      // no way to tell "a red dragon over snowy mountains" (four of five parts
      // read from the prompt) from "a stairwell" (none of three, and the result
      // is a comet over a swamp). Standalone, inventing is right:
      // a blank page is never a blank page. Lent to another project, it is the
      // difference between a picture of the thing asked for and a picture of
      // something else entirely, so the caller is told which it got.
      grounded: groundedIn(read)
    };
  }

  /*
   * A summary of what the prompt actually said, derived from the read trail.
   *   parts     how many decisions were made in all
   *   fromWords how many of them came from the prompt rather than the seed
   *   ratio     fromWords / parts, 0..1
   *   invented  the categories chosen rather than read
   *   subject   true when the thing in the picture was named, which is the one
   *             that decides whether the picture is of what was asked for
   */
  function groundedIn(read) {
    var parts = read.length;
    var fromWords = 0;
    var invented = [];
    var subject = false;
    read.forEach(function (r) {
      if (r.word) {
        fromWords += 1;
        if (r.category === 'subject') subject = true;
      } else {
        invented.push(r.category);
      }
    });
    return {
      parts: parts,
      fromWords: fromWords,
      ratio: parts ? fromWords / parts : 0,
      invented: invented,
      subject: subject
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

  /* "an orange fish", not "a orange fish". The adjective list above is written
   * with its article already attached ('an ancient', 'a giant'), but a palette
   * name is picked at random and a tenth of them begin with a vowel. */
  function article(word) {
    return /^[aeiou]/i.test(String(word)) ? 'an' : 'a';
  }

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
      .replace('{colour}', article(colour.words[0]) + ' ' + colour.words[0])
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
    LOCKS: LOCKS,
    holdLocks: holdLocks,
    rngFrom: rngFrom,
    hash: hash,
    normalise: normalise,
    editDistance: editDistance,
    findNear: findNear
  };

  root.CodaPrompt = API;
  if (typeof module !== 'undefined' && module.exports) module.exports = API;
})(typeof window !== 'undefined' ? window : this);
