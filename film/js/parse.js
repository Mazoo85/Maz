/*
 * SCRIPT FORGE — the reader.
 * --------------------------
 * Turns the sentence you type into a premise the writer can work from: who is
 * in it, where it happens, what it turns on, what genre it wants to be, and
 * what the hero is actually after.
 *
 * It is deliberately forgiving. One word, one line or a paragraph all produce
 * a complete premise — anything it cannot find in your text, it chooses for
 * you from the lexicon, seeded so the same idea gives the same film.
 *
 * Exposed as window.FilmParse (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  var LEX = root.FILM_LEXICON ||
    (typeof require !== 'undefined' ? require('./lexicon.js') : {});

  /* ------------------------------------------------------------ seeded RNG */
  // mulberry32 — same generator the other Maz apps use. Deterministic, so a
  // seed always reproduces the same script.
  function makeRng(seed) {
    var a = seed >>> 0;
    return function () {
      a |= 0;
      a = (a + 0x6d2b79f5) | 0;
      var t = Math.imul(a ^ (a >>> 15), 1 | a);
      t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }

  // FNV-1a over the text, so the same idea lands on the same seed.
  function hashText(text) {
    var h = 0x811c9dc5;
    for (var i = 0; i < text.length; i++) {
      h ^= text.charCodeAt(i);
      h = Math.imul(h, 0x01000193);
    }
    return h >>> 0;
  }

  function pick(list, rng) {
    return list[Math.floor(rng() * list.length) % list.length];
  }

  /* ------------------------------------------------------------ text tools */
  function tokens(text) {
    return (text.toLowerCase().match(/[a-z][a-z'-]*/g) || []);
  }

  // A padded token string ("  the  radio  ") makes whole-word and whole-phrase
  // matching a plain indexOf, with no regex escaping to get wrong.
  function haystack(words) {
    var singular = words.map(function (w) {
      return w.length > 3 && w.slice(-1) === 's' && w.slice(-2) !== 'ss' ? w.slice(0, -1) : w;
    });
    return ' ' + words.join(' ') + ' | ' + singular.join(' ') + ' ';
  }

  function has(hay, phrase) {
    return hay.indexOf(' ' + phrase + ' ') !== -1;
  }

  // Longest keys first, so "lighthouse keeper" beats "keeper" and
  // "parking garage" beats "garage".
  function byLengthDesc(keys) {
    return keys.slice().sort(function (a, b) { return b.length - a.length; });
  }

  function titleCase(s) {
    return s.replace(/\b[a-z]/g, function (c) { return c.toUpperCase(); });
  }

  function article(word) {
    return /^[aeiou]/i.test(word) ? 'an' : 'a';
  }

  /* Words that start sentences or are simply common, so a capital letter on
   * them says nothing about a character being named. */
  var NOT_A_NAME = {};
  ('a an the and but or if when while after before because so then that this these those ' +
   'i you he she they we it his her their our my your one two three four five six seven ' +
   'eight nine ten in on at to from with without for of by as is are was were be been being ' +
   'do does did done have has had will would can could should must may might there here ' +
   'what who whom whose which where why how not no yes every each all some any every ' +
   'monday tuesday wednesday thursday friday saturday sunday january february march april ' +
   'may june july august september october november december mr mrs ms dr int ext ' +
   'god day night today tomorrow yesterday').split(/\s+/).forEach(function (w) { NOT_A_NAME[w] = true; });

  /* --------------------------------------------------------------- finders */

  /* A capital after one of these is usually a place, not a person:
   * "in Anchorage", "to Rome". Names introduced with "named"/"called" are
   * exempt, because those are unambiguous. */
  var PLACE_PREPS = {};
  ('in at from to near outside inside across around through over under into on ' +
   'toward towards past along down up beyond').split(' ').forEach(function (w) { PLACE_PREPS[w] = true; });

  function findNames(text) {
    var out = [];
    var seen = {};

    function add(word) {
      var key = word.toLowerCase();
      if (NOT_A_NAME[key] || seen[key]) return;
      if (LEX.PLACES[key] || LEX.ROLES[key]) return;
      if (LEX.OBJECTS.indexOf(key) !== -1) return;
      seen[key] = true;
      out.push(word.toUpperCase());
    }

    // "a man named Tobias", "a dog called Bishop" — explicit, so it goes first.
    var named = /\b(?:named|called)\s+([A-Z][a-zA-Z'-]+)/g;
    var m;
    while ((m = named.exec(text)) !== null) add(m[1]);

    // Otherwise walk the words: a capitalised one that neither opens a
    // sentence nor follows a place preposition is taken to be a character.
    var re = /[A-Za-z][A-Za-z'-]*|[.!?;:]/g;
    var prev = null;
    var sentenceStart = true;
    while ((m = re.exec(text)) !== null) {
      var tok = m[0];
      if (tok.length === 1 && '.!?;:'.indexOf(tok) !== -1) {
        sentenceStart = true;
        prev = null;
        continue;
      }
      if (/^[A-Z][a-z'-]+$/.test(tok) && !sentenceStart && !(prev && PLACE_PREPS[prev])) add(tok);
      prev = tok.toLowerCase();
      sentenceStart = false;
    }
    return out.slice(0, 4);
  }

  function findAll(hay, keys) {
    var found = [];
    byLengthDesc(keys).forEach(function (k) {
      if (has(hay, k) && found.indexOf(k) === -1) {
        // Skip a key already covered by a longer one already found
        for (var i = 0; i < found.length; i++) {
          if (found[i].indexOf(k) !== -1) return;
        }
        found.push(k);
      }
    });
    return found;
  }

  /* "her father", "his boss", "my sister" — a role introduced by a possessive
   * belongs to the *other* character, not the one the film is about. */
  function findRoles(text, hay) {
    var keys = findAll(hay, Object.keys(LEX.ROLES));
    var relational = {};
    keys.forEach(function (k) {
      var esc = k.replace(/[-\/\\^$*+?.()|[\]{}]/g, '\\$&');
      if (new RegExp("\\b(?:her|his|their|my|our|your)\\s+(?:[a-z]+\\s+)?" + esc + "\\b", 'i').test(text)) {
        relational[k] = true;
      }
    });
    var heroRole = null;
    var otherRole = null;
    keys.forEach(function (k) { if (!relational[k] && !heroRole) heroRole = k; });
    keys.forEach(function (k) { if (relational[k] && !otherRole && k !== heroRole) otherRole = k; });
    if (!heroRole) heroRole = keys[0] || null;
    if (!otherRole) {
      otherRole = keys.filter(function (k) { return k !== heroRole; })[0] || null;
    }
    return { heroRole: heroRole, otherRole: otherRole };
  }

  /* How many genres claim each keyword. "attic" belongs to horror alone;
   * "brother" is in half the lists. The rare one should decide the film. */
  var KEYWORD_OWNERS = null;
  function keywordOwners() {
    if (KEYWORD_OWNERS) return KEYWORD_OWNERS;
    KEYWORD_OWNERS = {};
    Object.keys(LEX.GENRES).forEach(function (g) {
      LEX.GENRES[g].keywords.forEach(function (k) {
        KEYWORD_OWNERS[k] = (KEYWORD_OWNERS[k] || 0) + 1;
      });
    });
    return KEYWORD_OWNERS;
  }

  function scoreGenres(hay) {
    var owners = keywordOwners();
    var scores = {};
    var hits = {};
    var best = 'drama';
    var bestScore = 0;
    Object.keys(LEX.GENRES).forEach(function (g) {
      var score = 0;
      var found = 0;
      LEX.GENRES[g].keywords.forEach(function (k) {
        if (!has(hay, k)) return;
        found++;
        // A word nobody else uses is worth half a hit more than a shared one.
        var weight = owners[k] <= 1 ? 1.5 : 1 / owners[k] + 0.5;
        // A word that is also a job or a relationship has already told us who
        // is in the film, not what kind of film it is.
        if (LEX.ROLES[k]) weight *= 0.4;
        // A word that is also a place is the strongest signal there is: it is
        // the world the film gets shot in. An attic is a horror film; a vault
        // is a heist; a saloon is a western.
        if (LEX.PLACES[k]) weight *= 1.3;
        score += weight;
      });
      scores[g] = score;
      hits[g] = found;
      if (score > bestScore) { bestScore = score; best = g; }
    });
    return { genre: best, score: hits[best], weight: bestScore, scores: scores };
  }

  var TIME_WORDS = [
    ['midnight', 'NIGHT'], ['tonight', 'NIGHT'], ['night', 'NIGHT'],
    ['evening', 'NIGHT'], ['dusk', 'DUSK'], ['sunset', 'DUSK'],
    ['dawn', 'DAWN'], ['sunrise', 'DAWN'], ['morning', 'DAY'],
    ['afternoon', 'DAY'], ['noon', 'DAY'], ['daylight', 'DAY'], ['day', 'DAY']
  ];

  var OBJECT_TRIGGER =
    /\b(?:finds?|found|discovers?|discovered|receives?|received|inherits?|inherited|steals?|stole|buys?|bought|opens?|opened|loses?|lost|keeps?|kept|carries|carrying|holding|hides?|hid)\s+(?:a|an|the|their|his|her|its|one|some)?\s*([a-z][a-z'-]*(?:\s+[a-z][a-z'-]*)?)/i;

  var OBJECT_TAIL = {
    that: 1, which: 1, who: 1, in: 1, on: 1, at: 1, to: 1, from: 1, with: 1,
    of: 1, and: 1, but: 1, for: 1, into: 1, under: 1, behind: 1, is: 1, was: 1,
    has: 1, had: 1, will: 1, can: 1, it: 1, he: 1, she: 1, they: 1
  };

  function findObject(text, hay) {
    var m = OBJECT_TRIGGER.exec(text);
    if (m) {
      var phrase = m[1].trim().split(/\s+/);
      // Drop a trailing connective or verb: "radio that" and "package
      // addressed" are both really just the noun.
      while (phrase.length > 1 &&
        (OBJECT_TAIL[phrase[phrase.length - 1]] || /(?:ed|ing)$/.test(phrase[phrase.length - 1]))) {
        phrase.pop();
      }
      var joined = phrase.join(' ');
      if (phrase.length && !OBJECT_TAIL[phrase[0]] && !isPlaceWord(joined)) return joined;
    }
    var known = findAll(hay, LEX.OBJECTS);
    return known.length ? known[0] : null;
  }

  // "rob a bank" must not make the bank the object — it is the set.
  function isPlaceWord(phrase) {
    return !!LEX.PLACES[String(phrase).toLowerCase()];
  }

  function findWant(hay) {
    for (var i = 0; i < LEX.WANTS.length; i++) {
      for (var j = 0; j < LEX.WANTS[i].keys.length; j++) {
        if (has(hay, LEX.WANTS[i].keys[j])) return LEX.WANTS[i].want;
      }
    }
    return null;
  }

  /* ---------------------------------------------------------------- titles */
  var TITLE_TEMPLATES = [
    'THE {OBJ}',
    'WHAT THE {OBJ} KNEW',
    'THE LAST {OBJ}',
    '{PLACEWORD}',
    '{PLACEWORD} AT {TIME}',
    '{HERO} AND THE {OBJ}',
    'SOMETHING ABOUT THE {OBJ}',
    'AFTER THE {OBJ}',
    '{PLACEWORD}, {TIME}'
  ];

  function makeTitle(p, rng) {
    var pool = TITLE_TEMPLATES.slice();
    // A one-word object makes the sparest titles work; a long phrase does not.
    if (p.object.indexOf(' ') !== -1) {
      pool = ['{PLACEWORD}', '{PLACEWORD} AT {TIME}', 'THE {OBJ}', 'WHAT THE {OBJ} KNEW'];
    }
    // "LIGHTHOUSE AT DAY" is not a title; the same line at night is.
    if (p.time === 'DAY') pool = pool.filter(function (x) { return x.indexOf('AT {TIME}') === -1; });
    var t = pick(pool, rng);
    return t
      .replace('{OBJ}', p.object.toUpperCase())
      .replace('{PLACEWORD}', p.places[0].word.toUpperCase())
      .replace('{TIME}', p.time)
      .replace('{HERO}', p.hero.name);
  }

  /* ----------------------------------------------------------------- parse */
  function parse(text, opts) {
    opts = opts || {};
    var raw = String(text == null ? '' : text).trim();
    var words = tokens(raw);
    var hay = haystack(words);
    var seed = typeof opts.seed === 'number' ? opts.seed >>> 0 : hashText(raw || 'blank');
    var rng = makeRng(seed);

    /* genre — yours if you chose one, otherwise whichever the words vote for */
    var scored = scoreGenres(hay);
    var genreKey = opts.genre && opts.genre !== 'auto' && LEX.GENRES[opts.genre]
      ? opts.genre
      : scored.genre;
    var genre = LEX.GENRES[genreKey];

    /* who */
    var roles = findRoles(raw, hay);
    var names = findNames(raw);
    var usedNames = {};
    function nameFor(index) {
      if (names[index]) { usedNames[names[index]] = true; return names[index]; }
      var n;
      var guard = 0;
      do { n = pick(LEX.NAMES, rng); guard++; } while (usedNames[n] && guard < 40);
      usedNames[n] = true;
      return n;
    }

    var heroRole = roles.heroRole;
    var otherRole = roles.otherRole;
    var hero = { name: nameFor(0), role: heroRole || 'person who has been holding it together' };
    var foil = otherRole ? { role: otherRole } : pick(LEX.FOILS, rng);
    var other = { name: nameFor(1), role: foil.role };

    /* where — up to two locations, so the film can cut between them */
    var placeKeys = findAll(hay, Object.keys(LEX.PLACES));
    if (!placeKeys.length && heroRole && LEX.ROLES[heroRole].place) placeKeys = [LEX.ROLES[heroRole].place];
    // A second location the film can cut to. Drawn from places that plausibly
    // adjoin anywhere — a corridor, a car, the street outside — so a lighthouse
    // story does not cut to a hospital for no reason.
    var CONNECTORS = ['car', 'street', 'porch', 'hallway', 'parking lot', 'stairwell', 'alley', 'kitchen'];
    var guardPlaces = 0;
    while (placeKeys.length < 2 && guardPlaces++ < 40) {
      var candidate = pick(CONNECTORS, rng);
      if (placeKeys.indexOf(candidate) === -1) placeKeys.push(candidate);
    }
    var places = placeKeys.slice(0, 3).map(function (k) {
      var entry = LEX.PLACES[k] || { slug: k.toUpperCase(), int: 'INT.' };
      return { key: k, slug: entry.slug, int: entry.int, word: entry.slug.split(' — ')[0] };
    });

    /* what it turns on */
    var object = findObject(raw, hay);
    // A genre the words clearly voted for outranks a job's default prop: a bank
    // robbery gets a duffel bag, not the teller's order pad.
    if (!object && scored.score >= 2 && genreKey === scored.genre) object = pick(genre.objects, rng);
    if (!object && heroRole && LEX.ROLES[heroRole].object) object = LEX.ROLES[heroRole].object;
    if (!object) object = pick(genre.objects, rng);

    /* when */
    var time = null;
    for (var i = 0; i < TIME_WORDS.length && !time; i++) {
      if (has(hay, TIME_WORDS[i][0])) time = TIME_WORDS[i][1];
    }
    if (!time) time = genre.time;

    /* what the hero is after */
    var want = findWant(hay) || 'get to the other side of this';

    var premise = {
      raw: raw,
      seed: seed,
      empty: raw === '',
      genre: genreKey,
      genreLabel: genre.label,
      genreAuto: !(opts.genre && opts.genre !== 'auto'),
      genreConfidence: scored.score,
      hero: hero,
      heroNamed: !!names[0],
      other: other,
      places: places,
      object: object,
      time: time,
      want: want,
      detail: pick(genre.details, rng),
      sound: pick(genre.sounds, rng)
    };

    premise.title = opts.title || makeTitle(premise, rng);
    premise.logline = makeLogline(premise);
    return premise;
  }

  // "a wire cutters" is not English: a plural object takes no article at all.
  function withArticle(noun) {
    var word = String(noun);
    var plural = /s$/.test(word) && !/ss$/.test(word) && !/^(?:gas|bus|glass|dress)$/.test(word);
    return plural ? word : article(word) + ' ' + word;
  }

  function makeLogline(p) {
    var role = p.hero.role;
    var who = p.heroNamed
      ? titleCase(p.hero.name.toLowerCase()) + ', ' + withArticle(role) + ','
      : withArticle(role).charAt(0).toUpperCase() + withArticle(role).slice(1);
    var where = p.places[0].word.toLowerCase();
    return who + ' finds ' + withArticle(p.object) + ' in ' + withArticle(where) +
      ', and has one ' + p.time.toLowerCase() + ' to ' + p.want + '.';
  }

  var API = {
    parse: parse,
    makeRng: makeRng,
    hashText: hashText,
    article: article,
    withArticle: withArticle,
    titleCase: titleCase
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmParse = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
