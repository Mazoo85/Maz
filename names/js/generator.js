/*
 * NAME FORGE — Engine
 * -------------------
 * Pure logic (no DOM) so it runs in the browser AND in a headless test.
 * Reads window.NAME_WORDS and rolls names out of it.
 *
 * The whole point of this app is that *nothing* about a name is fixed. It
 * draws one adjective and one noun, and then — crucially — rolls again for
 * which of the two goes first. "Crimson Falcon" and "Falcon Crimson" are both
 * on the table, every single roll. The order can be pinned if you want it
 * (order: 'adjective-noun' or 'noun-adjective'), but the default is 'random'.
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

  // Which word lands first. 'random' (the default) rolls this per name.
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

  // ---- Seeded RNG (mulberry32) — the same seed always rebuilds the name.
  function makeRng(seed) {
    var a = seed >>> 0;
    return function () {
      a |= 0;
      a = (a + 0x6D2B79F5) | 0;
      var t = Math.imul(a ^ (a >>> 15), 1 | a);
      t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }

  function randomSeed() {
    return Math.floor(Math.random() * 4294967296) >>> 0;
  }

  function pickFrom(list, rng) {
    return list[Math.floor(rng() * list.length)];
  }

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
   *   order  string   - 'random' (default), 'adjective-noun', 'noun-adjective'
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

    // The roll that matters: which word goes first.
    var order = opts.order && opts.order !== 'random' ? opts.order : null;
    if (!order || !has(ORDERS, order)) {
      order = rng() < 0.5 ? 'adjective-noun' : 'noun-adjective';
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
   * Roll `count` names, all different from each other. Each one still gets its
   * own order and its own seed, so a batch is a batch of independent rolls.
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
   * How many different names this can make. Both orders count, because
   * "Falcon Crimson" is a different name from "Crimson Falcon".
   */
  function combinations(order) {
    var pairs = WORDS.adjectives.length * WORDS.nouns.length;
    return order && order !== 'random' ? pairs : pairs * 2;
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
