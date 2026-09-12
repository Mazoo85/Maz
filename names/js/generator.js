/*
 * NAME FORGE — Engine
 * -------------------
 * Pure logic (no DOM) so it runs in the browser AND in a headless test.
 * Reads window.NAME_WORDS and rolls names out of it.
 *
 * The words are random; the shape of the name is not. A name is always an
 * adjective and then a noun — "Crimson Falcon" — because that is what reads
 * like a name. Only the two words are rolled.
 *
 * The order can be changed per roll if you ever want it: pass
 * order: 'noun-adjective' for "Falcon Crimson", or order: 'random' to let the
 * dice decide each time. Left alone, it is always adjective first.
 *
 * A name is reproducible: the same seed always yields the same name, so a
 * favourite can be written down and rolled again later.
 *
 * Exposed as: window.NameForge (also module.exports for testing).
 */
(function (root) {
  'use strict';

  var WORDS = root.NAME_WORDS ||
    (typeof require !== 'undefined' ? require('./words.js') : { adjectives: [], nouns: [] });
  // The seeded RNG and the list pick are shared/maz-util.js's — the same code was
  // written out here and in MADLIBS, so one copy serves both.
  var UTIL = root.MazUtil ||
    (typeof require !== 'undefined' ? require('../../shared/maz-util.js') : {});

  // Which word lands first. Adjective first is the default and the point;
  // the others are there for anyone who wants them.
  var ORDERS = [
    { id: 'adjective-noun', label: 'Adjective first', example: 'Crimson Falcon' },
    { id: 'noun-adjective', label: 'Noun first', example: 'Falcon Crimson' }
  ];

  // How the two words are joined and capitalised.
  var STYLES = [
    { id: 'title', label: 'Two Words', example: 'Crimson Falcon' },
    { id: 'fused', label: 'OneWord', example: 'CrimsonFalcon' },
    { id: 'hyphen', label: 'hyphen-case', example: 'crimson-falcon' },
    { id: 'snake', label: 'snake_case', example: 'crimson_falcon' },
    { id: 'lower', label: 'lower case', example: 'crimson falcon' },
    { id: 'shout', label: 'SHOUT CASE', example: 'CRIMSON FALCON' }
  ];

  var makeRng = UTIL.makeRng;
  var randomSeed = UTIL.randomSeed;
  var pickFrom = UTIL.pick;

  function capitalise(word) {
    return word.charAt(0).toUpperCase() + word.slice(1);
  }

  function has(list, id) {
    for (var i = 0; i < list.length; i++) {
      if (list[i].id === id) return true;
    }
    return false;
  }

  function format(words, styleId) {
    var a = words[0];
    var b = words[1];
    switch (styleId) {
      case 'fused': return capitalise(a) + capitalise(b);
      case 'hyphen': return a + '-' + b;
      case 'snake': return a + '_' + b;
      case 'lower': return a + ' ' + b;
      case 'shout': return (a + ' ' + b).toUpperCase();
      default: return capitalise(a) + ' ' + capitalise(b);
    }
  }

  /*
   * Roll one name.
   *
   * opts:
   *   seed   number   - reproduces an earlier name; omitted means a fresh roll
   *   order  string   - 'adjective-noun' (default), 'noun-adjective', 'random'
   *   style  string   - 'random', or any STYLES id (default 'title')
   *
   * Returns { seed, adjective, noun, order, style, words, text }.
   */
  function generate(opts) {
    opts = opts || {};
    var seed = typeof opts.seed === 'number' ? opts.seed >>> 0 : randomSeed();
    var rng = makeRng(seed);

    var adjective = pickFrom(WORDS.adjectives, rng);
    var noun = pickFrom(WORDS.nouns, rng);
    // A handful of words are on both lists (rose, jade, iron...). Never pair
    // one with itself — "Rose Rose" is not a name, it is a stutter.
    var guard = 0;
    while (noun === adjective && guard++ < 20) {
      noun = pickFrom(WORDS.nouns, rng);
    }

    // Adjective first unless explicitly asked otherwise. 'random' hands this
    // one decision back to the dice; anything unrecognised falls back to the
    // default rather than to a surprise.
    var order = 'adjective-noun';
    if (opts.order === 'random') {
      order = rng() < 0.5 ? 'adjective-noun' : 'noun-adjective';
    } else if (opts.order && has(ORDERS, opts.order)) {
      order = opts.order;
    }

    var style = opts.style && opts.style !== 'random' ? opts.style : null;
    if (!style || !has(STYLES, style)) {
      style = opts.style === 'random' ? pickFrom(STYLES, rng).id : 'title';
    }

    var words = order === 'noun-adjective' ? [noun, adjective] : [adjective, noun];

    return {
      seed: seed,
      adjective: adjective,
      noun: noun,
      order: order,
      style: style,
      words: words,
      text: format(words, style)
    };
  }

  /*
   * Roll `count` names, all different from each other. Each one gets its own
   * seed, so a batch is a batch of independent rolls.
   */
  function generateMany(count, opts) {
    opts = opts || {};
    var out = [];
    var seen = {};
    var attempts = 0;
    var cap = Math.max(count * 40, 200);
    var rng = typeof opts.seed === 'number' ? makeRng(opts.seed) : null;

    while (out.length < count && attempts++ < cap) {
      var one = generate({
        seed: rng ? Math.floor(rng() * 4294967296) >>> 0 : undefined,
        order: opts.order,
        style: opts.style
      });
      var key = one.text.toLowerCase();
      if (seen[key]) continue;
      seen[key] = true;
      out.push(one);
    }
    return out;
  }

  /*
   * How many different names this can make: every adjective against every
   * noun. Only 'random' doubles it, because there "Falcon Crimson" is a
   * second name the app can land on.
   */
  function combinations(order) {
    var pairs = WORDS.adjectives.length * WORDS.nouns.length;
    return order === 'random' ? pairs * 2 : pairs;
  }

  function toText(names) {
    return names.map(function (n) { return n.text; }).join('\n') + '\n';
  }

  function toCsv(names) {
    var rows = ['name,first word,second word,adjective,noun,order,style,seed'];
    names.forEach(function (n) {
      rows.push([
        '"' + n.text.replace(/"/g, '""') + '"',
        n.words[0], n.words[1], n.adjective, n.noun, n.order, n.style, n.seed
      ].join(','));
    });
    return rows.join('\n') + '\n';
  }

  var API = {
    makeRng: makeRng,
    randomSeed: randomSeed,
    capitalise: capitalise,
    format: format,
    generate: generate,
    generateMany: generateMany,
    combinations: combinations,
    toText: toText,
    toCsv: toCsv,
    ORDERS: ORDERS,
    STYLES: STYLES,
    get words() { return WORDS; }
  };

  root.NameForge = API;
  if (typeof module !== 'undefined' && module.exports) {
    module.exports = API;
  }
})(typeof window !== 'undefined' ? window : this);
