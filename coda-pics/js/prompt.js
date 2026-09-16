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
  /*
   * The horizon belongs to both halves, so it is held by either.
   *
   * It used to sit only under `land`, which made "keep the sky" a promise the
   * app could not keep: a new landscape sets the horizon somewhere new, the sky
   * is painted to that horizon, and so the sky everybody asked to keep was
   * repainted. Measured, it moved more than the land did. Holding it from
   * either side is also what a person means — asked for different ground they
   * want different ground, not a different skyline to the world.
   */
  var LOCKS = {
    subject: ['subject'],
    sky: ['sky', 'light', 'cloud', 'weather', 'scene'],
    land: ['scene', 'ground', 'fore']
  };

  /* Build the `locked` map for a new take: every salt covered by a held lock
   * keeps the seed it had. */
  /*
   * Put the sun at a given angle and keep the rest of the picture honest about
   * it.
   *
   * The hour is an angle, but a few things still ask which band of the day it
   * is — stars come out at night, windows light up when it is not daylight —
   * so moving the sun by hand has to move the band with it, or a picture ends
   * up at midnight with the sun overhead.
   */
  function atSun(spec, sun) {
    spec.sun = Math.max(-60, Math.min(90, Math.round(sun * 10) / 10));
    if (spec.sun < -10) spec.time = 'night';
    else if (spec.sun < 14) spec.time = spec.rising ? 'dawn' : 'dusk';
    else spec.time = 'day';
    return spec;
  }

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

  /*
   * Words it was never taught.
   *
   * The vocabulary is a list, and anything off the list was thrown away and
   * reported back as "I did not know that word". But most words nobody taught
   * it are made out of words it does know — "snowy" is snow, "wolflike" is a
   * wolf, "seabird" is a sea and a bird — and working that out is the
   * difference between a fixed list and a language.
   *
   * Two rules, both plain English rather than clever: take an ending off, and
   * split a long word into two words. Neither invents a meaning; both can only
   * ever arrive at words already in the tables, so a derived word paints the
   * same thing the word it came from would.
   */
  var ENDINGS = ['ed', 'y', 'ish', 'like', 'en', 'ing', 'ly', 'ful', 'less'];

  var EVERY_WORD = null;
  function everyWord() {
    if (EVERY_WORD) return EVERY_WORD;
    EVERY_WORD = {};
    Object.keys(LEX).forEach(function (name) {
      var table = LEX[name];
      if (!table || !table.length || typeof table.forEach !== 'function') return;
      table.forEach(function (entry) {
        if (!entry || !entry.words) return;
        entry.words.forEach(function (w) {
          if (w.indexOf(' ') < 0) EVERY_WORD[w] = true;
        });
      });
    });
    return EVERY_WORD;
  }

  function derive(word) {
    var known = everyWord();
    if (known[word]) return null;
    var out = [];

    /* An ending taken off. "Snowy" is snow; "wolflike" is a wolf. A doubled
     * consonant goes with it ("foggy" is fog, not fogg). */
    for (var e = 0; e < ENDINGS.length; e++) {
      var end = ENDINGS[e];
      if (word.length < end.length + 3) continue;
      if (word.slice(-end.length) !== end) continue;
      var stem = word.slice(0, -end.length);
      var tries = [stem, stem + 'e'];
      if (/([bdfglmnprt])\1$/.test(stem)) tries.push(stem.slice(0, -1));
      for (var t = 0; t < tries.length; t++) {
        if (known[tries[t]] && out.indexOf(tries[t]) < 0) out.push(tries[t]);
      }
      if (out.length) return out;
    }

    /* Or two words run together. Both halves have to be words, and both have
     * to be long enough that the split means something — "seabird" is a sea
     * and a bird, but almost any word can be cut into two scraps. */
    if (word.length >= 7) {
      for (var i = 3; i <= word.length - 3; i++) {
        var left = word.slice(0, i), right = word.slice(i);
        if (known[left] && known[right]) return [left, right];
      }
    }
    return out.length ? out : null;
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

  /*
   * How much of it.
   *
   * Every word in every table was an on-off switch: a picture was foggy or it
   * was not. English does not work that way — "slightly misty", "quite
   * weathered" and "impossibly huge" are three amounts of one thing, and it is
   * amounts rather than more words that make a vocabulary feel endless.
   *
   * A degree word modifies whatever follows it, so this looks back from the
   * word that was matched. Two words back as well as one, because half of them
   * are two words long ("a little", "a bit").
   */
  function degreeAt(index, toks) {
    for (var back = 1; back <= 2; back++) {
      var at = index - back;
      if (at < 0) break;
      var one = toks[at];
      var two = at > 0 ? toks[at - 1] + ' ' + one : null;
      for (var d = 0; d < LEX.DEGREES.length; d++) {
        var words = LEX.DEGREES[d].words;
        if (words.indexOf(one) >= 0 || (two && words.indexOf(two) >= 0)) {
          return LEX.DEGREES[d].factor;
        }
      }
    }
    return 1;
  }

  /* Where in the list of words a hit started. A single word knows its own
   * place; a phrase knows where it starts in the flattened sentence, which is
   * the same thing once the words in front of it are counted. */
  function wordIndex(hit, flat) {
    if (!hit) return -1;
    if (hit.specific !== 2) return hit.at;
    var before = flat.slice(0, hit.at).split(' ').filter(function (w) { return w.length; });
    return before.length;
  }

  function degreeOf(hit, flat, toks) {
    var at = wordIndex(hit, flat);
    return at < 0 ? 1 : degreeAt(at, toks);
  }

  function findOne(table, flat, tokenList) {
    var hits = findAll(table, flat, tokenList);
    return hits.length ? hits[0] : null;
  }

  function byId(table, id) {
    for (var i = 0; i < table.length; i++) if (table[i].id === id) return table[i];
    return null;
  }

  function pick(list, r) {
    return list[Math.floor(r() * list.length) % list.length];
  }

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
    /* Words nobody taught it, worked out from words it knows. The word it was
     * derived from stays in the list in front of its meaning, so a degree word
     * still applies to the right thing and the prompt still reads in order. */
    var derived = {};
    var grown = [];
    toks.forEach(function (t) {
      grown.push(t);
      var from = derive(t);
      if (!from) return;
      from.forEach(function (w) {
        grown.push(w);
        derived[w] = t;
      });
    });
    toks = grown;
    var seed = options.seed == null ? 1 : (options.seed | 0);
    var locked = options.locked || null;
    var base = rngFrom(hash(flat + '|' + seed));
    var read = [];

    var used = {};                    // which tokens were understood
    function note(category, label, word, meant) {
      /* A phrase match ("hot air balloon") has to mark every word in it, or
       * the words inside it get reported as ones nobody understood. */
      if (word) {
        String(word).split(' ').forEach(function (w) {
          used[w] = true;
          /* "Snowy" was understood the moment "snow" was, so it is not a word
           * nobody knew. */
          if (derived[w]) used[derived[w]] = true;
        });
        /* And say so: the person typed "snowy", so the readout should show
         * their word and what it was taken to mean, not a word they never
         * used. */
        if (derived[word] && !meant) {
          meant = word;
          word = derived[word];
        }
      }
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
    var time, entry, sun, rising;
    if (timeHit) {
      entry = timeHit.entry;
      time = entry.id;
      note('time', entry.label, timeHit.word);
      /* The word decides the angle, not merely the band: "first light" and
       * "mid-morning" are both dawn and are nothing like each other. */
      sun = (entry.at && entry.at[timeHit.word] != null) ? entry.at[timeHit.word] : entry.sun;
      rising = LEX.RISING[time];
      if (LEX.FALLING_WORDS.indexOf(timeHit.word) >= 0) rising = false;
    } else {
      if (NIGHT_SCENES[scene.id]) time = 'night';
      else time = ['dawn', 'day', 'dusk', 'night'][Math.floor(base() * 4) % 4];
      entry = byId(LEX.TIMES, time);
      /* Nobody said an hour, so the sun may stand anywhere in the band. Two
       * pictures of the same words are lit differently because the light
       * really is different, not because a different label was drawn. */
      sun = entry.band[0] + base() * (entry.band[1] - entry.band[0]);
      rising = LEX.RISING[time];
      if (time === 'day' && base() < 0.5) rising = false;
    }
    sun = Math.round(sun * 10) / 10;

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

    /* How much weather. "A light drizzle" and "a downpour" were the same
     * picture; now the first has a third of the rain in it. */
    var weatherStrength = weatherHit ? degreeOf(weatherHit, flat, toks) : 1;
    if (!weatherHit) weatherStrength = 0.75 + base() * 0.5;

    /* --- mood --- */
    var moodHit = findOne(LEX.MOODS, flat, toks);
    var mood = moodHit ? moodHit.entry.mood * degreeOf(moodHit, flat, toks)
      : 0.35 + base() * 0.3;
    if (moodHit) note('mood', moodHit.entry.id, moodHit.word);
    if (time === 'night') mood = Math.min(1, mood + 0.08);

    /* --- what it is made of ---
     * Noted further down: a material word can turn out to belong to a style
     * instead ("in stained glass"), and that is only knowable once the style
     * words have been read. */
    var materialHit = findOne(LEX.MATERIALS, flat, toks);

    /* --- palette --- */
    var paletteHit = findOne(LEX.PALETTES, flat, toks);
    /* "A bronze dragon" is one word, and it is the material: letting the
     * colour table have it as well would tint the whole world brass. */
    if (paletteHit && materialHit && paletteHit.word === materialHit.word) paletteHit = null;
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
      /* "A glass dragon" is a dragon made of glass; "in stained glass" is the
       * style. One word cannot be both, and the thing the picture is *of*
       * wins — the style still has "stained glass", "mosaic" and "tiffany" of
       * its own to be asked for by. */
      /* "A glass dragon" is a dragon made of glass; "a dragon in stained
       * glass" is the style. The same word cannot be both, and which one it is
       * depends on what it is part of: a longer style phrase that swallows the
       * material word is the style, and the bare word on its own is the
       * material. The style still has "mosaic" and "tiffany" of its own. */
      if (materialHit) {
        var swallowed = styleHits.some(function (hit) {
          return hit.word !== materialHit.word && hit.word.indexOf(materialHit.word) >= 0;
        });
        if (swallowed) materialHit = null;
        else styleHits = styleHits.filter(function (h) { return h.word !== materialHit.word; });
      }
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

    /* --- where the picture is taken from ---
     * Said outright when the words say it; otherwise rolled, because every
     * picture framed identically is its own kind of sameness. The roll is
     * weighted towards the ordinary shot — a gallery where every third picture
     * is an extreme close-up is no better than one where none is. */
    var shotHit = findOne(LEX.SHOTS, flat, toks);
    var shot;
    if (shotHit) {
      shot = shotHit.entry;
      note('shot', shot.label, shotHit.word);
    } else {
      var sr = rng({ prompt: prompt, seed: seed, locked: locked }, 'shot');
      var roll = sr();
      shot = roll < 0.58 ? null
        : roll < 0.74 ? byId(LEX.SHOTS, 'near')
        : roll < 0.88 ? byId(LEX.SHOTS, 'wide')
        : roll < 0.95 ? byId(LEX.SHOTS, 'low')
        : byId(LEX.SHOTS, 'closeup');
    }

    /* --- what it could not use ---
     * Silence here is the worst answer: somebody types "a griffin" and gets a
     * fox with no idea why. */
    var filler = {};
    var material = materialHit ? materialHit.entry : null;
    if (material) note('material', material.label, materialHit.word);

    /* --- what it is doing ---
     * Said or rolled. Mostly standing or alert, because that is mostly what an
     * animal caught in a photograph is doing. */
    var poseHit = findOne(LEX.POSES, flat, toks);
    var pose;
    if (poseHit) {
      pose = poseHit.entry.id;
      note('doing', poseHit.entry.label, poseHit.word);
    } else {
      var roll = base();
      pose = roll < 0.40 ? 'standing' : roll < 0.68 ? 'alert'
        : roll < 0.82 ? 'grazing' : roll < 0.93 ? 'walking'
        : roll < 0.98 ? 'running' : 'resting';
    }

    /* --- what it has got ---
     * Parts, not creatures: any of them goes on any subject, and more than one
     * can be asked for at once. */
    var partHits = findAll(LEX.PARTS, flat, toks);
    var parts = [];
    partHits.forEach(function (hit) {
      if (parts.indexOf(hit.entry.id) >= 0) return;
      parts.push(hit.entry.id);
      note('part', hit.entry.label, hit.word);
    });

    /* --- how long it has been standing there ---
     * Some of these words name a place as well ("abandoned", "overgrown"), and
     * they are allowed to do both: an ancient tower stands in ancient ruins. */
    var ageHit = findOne(LEX.AGES, flat, toks);
    var age = ageHit ? ageHit.entry.age * degreeOf(ageHit, flat, toks) : 0;
    age = Math.max(0, Math.min(1, age));
    if (ageHit) note('wear', ageHit.entry.label, ageHit.word);

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
      /* A word the painter knows but did not use is not a word nobody knew.
       * "A snowy mountain" is one setting or the other, and whichever loses is
       * still English. */
      if (everyWord()[t]) return;
      var stems = derive(t);
      if (stems && stems.some(function (w) { return everyWord()[w]; })) return;
      if (unknown.indexOf(t) < 0) unknown.push(t);
    });

    return {
      prompt: prompt,
      seed: seed,
      locked: locked,
      shot: shot,
      subject: subject,
      companion: companion,
      relation: relation,
      scene: { id: scene.id, label: scene.label, prep: scene.prep || 'in', horizon: scene.horizon },
      material: material,
      parts: parts,
      pose: pose,
      age: age,
      time: time,
      /* How high the sun (or, below the horizon, the moon) stands, in degrees,
       * and which way it is going. The hour is an angle; `time` is only the
       * band it falls in. */
      sun: sun,
      rising: !!rising,
      weather: weather,
      weatherStrength: Math.max(0.2, Math.min(2, weatherStrength)),
      style: style,
      styles: extraStyle ? [style, extraStyle] : [style],
      palette: palette,
      mood: Math.max(0, Math.min(1, mood)),
      unknown: unknown,
      read: read
    };
  }

  function buildSubject(hit, flat, toks, base) {
    var entry = hit.entry;
    var scale = 1;
    LEX.SCALE_WORDS.forEach(function (s) {
      s.words.forEach(function (w) {
        var at = toks.indexOf(w);
        if (at < 0) return;
        /* "Very tiny" is smaller than tiny and "quite big" is less than big,
         * so the degree pushes the factor further from 1 rather than scaling
         * it — multiplying 0.72 by 1.6 would make a very tiny dragon large. */
        var step = (s.factor - 1) * degreeAt(at, toks);
        scale = Math.max(0.15, 1 + step);
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
  /*
   * What it is made of belongs inside the name of the thing, after its article
   * and not in front of it: "a bronze dragon", never "bronze a dragon". And
   * the article has to agree with the new word — "an iron gate", "a bronze
   * one" — because it is now the material that follows it.
   */
  function madeOf(label, material, dropArticle) {
    if (!material) return dropArticle ? label.replace(/^(an?|the) /, '') : label;
    var m = /^(an?|the) (.*)$/.exec(label);
    var rest = m ? m[2] : label;
    var made = material.label + ' ' + rest;
    if (dropArticle || !m) return made;
    if (m[1] === 'the') return 'the ' + made;
    return (/^[aeiou]/i.test(material.label) ? 'an ' : 'a ') + made;
  }

  function describe(spec) {
    var parts = [];
    if (spec.subject) {
      parts.push(spec.subject.count > 1
        ? spec.subject.count + ' × ' + madeOf(spec.subject.label, spec.material, true)
        : madeOf(spec.subject.label, spec.material, false));
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
    LOCKS: LOCKS,
    holdLocks: holdLocks,
    atSun: atSun,
    rngFrom: rngFrom,
    hash: hash,
    normalise: normalise,
    editDistance: editDistance,
    findNear: findNear
  };

  root.CodaPrompt = API;
  if (typeof module !== 'undefined' && module.exports) module.exports = API;
})(typeof window !== 'undefined' ? window : this);
