/*
 * MadLibs Story Generator — Engine
 * --------------------------------
 * Pure logic (no DOM) so it can run in the browser AND in a headless test.
 * Consumes window.MADLIBS_DICT + window.MADLIBS_TEMPLATES and produces filled,
 * scene-by-scene story ideas.
 *
 * Placeholder grammar (see templates.js):
 *   {category}       -> fresh random word from that dictionary category
 *   {category#tag}   -> random word reused wherever the same #tag appears
 *   {a} / {a-cap}    -> the article "a"/"an" (or "A"/"An"), auto-chosen from the
 *                       word that immediately follows it
 *
 * Exposed as: window.MadlibsGenerator (also module.exports for testing).
 */
(function (root) {
  'use strict';

  var DICT = root.MADLIBS_DICT ||
    (typeof require !== 'undefined' ? require('./dictionary.js') : {});
  var TEMPLATES = root.MADLIBS_TEMPLATES ||
    (typeof require !== 'undefined' ? require('./templates.js') : []);

  // Categories that are reserved grammar helpers, not dictionary lookups.
  var RESERVED = { a: true };

  // ---- Seeded RNG (mulberry32) — deterministic so a seed reproduces a story.
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
    // 32-bit seed. Math.random is fine in the browser and in node.
    return (Math.floor(Math.random() * 4294967296)) >>> 0;
  }

  function pickFrom(list, rng) {
    return list[Math.floor(rng() * list.length)];
  }

  // Public: draw one random word from a dictionary category.
  function pick(category, rng) {
    var list = DICT[category];
    if (!list || !list.length) return '{' + category + '}';
    return pickFrom(list, rng || Math.random);
  }

  var PLACEHOLDER = /\{([A-Za-z][A-Za-z]*)(?:#([A-Za-z0-9]+))?\}/g;

  function isVowel(ch) {
    return 'aeiouAEIOU'.indexOf(ch) !== -1;
  }

  // Replace {a}/{a-cap} with the correct article based on the following word.
  function applyArticles(text) {
    text = text.replace(/\{a\}\s*([A-Za-z])/g, function (_, letter) {
      return (isVowel(letter) ? 'an ' : 'a ') + letter;
    });
    text = text.replace(/\{a-cap\}\s*([A-Za-z])/g, function (_, letter) {
      return (isVowel(letter) ? 'An ' : 'A ') + letter;
    });
    return text;
  }

  // Capitalize the first letter of the text and of every sentence after it.
  function capitalizeSentences(text) {
    return text.replace(/(^|[.!?]\s+)([a-z])/g, function (_, pre, ch) {
      return pre + ch.toUpperCase();
    });
  }

  /*
   * Fill a single template. Returns:
   *   { id, title, genre, seed, beats:[{label,text}], signature }
   * A per-story tag map keeps tagged words (#hero, #realm, ...) consistent
   * across every beat. `picks` records every resolved word so two identical
   * fills produce an identical signature (used for de-duplication).
   */
  function fillTemplate(template, opts) {
    opts = opts || {};
    var seed = (opts.seed != null) ? (opts.seed >>> 0) : randomSeed();
    var rng = makeRng(seed);
    var tagMap = {};
    var picks = [];

    function resolveText(text) {
      var out = text.replace(PLACEHOLDER, function (match, category, tag) {
        if (RESERVED[category]) return match; // {a} handled later
        var word;
        if (tag) {
          var key = category + '#' + tag;
          if (tagMap[key] == null) tagMap[key] = pick(category, rng);
          word = tagMap[key];
        } else {
          word = pick(category, rng);
        }
        picks.push(word);
        return word;
      });
      out = applyArticles(out);
      return capitalizeSentences(out);
    }

    var beats = template.beats.map(function (b) {
      return { label: b.label, text: resolveText(b.text) };
    });

    var signature = template.id + '|' + picks.join('');

    return {
      id: template.id,
      title: template.title,
      genre: template.genre,
      seed: seed,
      beats: beats,
      signature: signature
    };
  }

  function templatesFor(genre) {
    if (!genre || genre === 'all') return TEMPLATES;
    return TEMPLATES.filter(function (t) { return t.genre === genre; });
  }

  /*
   * Generate one story. opts: { genre, seed }.
   * When a seed is supplied the whole result (template choice + words) is
   * reproducible, so saved/shared ideas can be regenerated exactly.
   */
  function generate(opts) {
    opts = opts || {};
    var seed = (opts.seed != null) ? (opts.seed >>> 0) : randomSeed();
    var rng = makeRng(seed);
    var pool = templatesFor(opts.genre);
    if (!pool.length) pool = TEMPLATES;
    var template = pool[Math.floor(rng() * pool.length)];
    // Derive the fill seed from the same stream so `seed` alone reproduces it.
    var fillSeed = Math.floor(rng() * 4294967296) >>> 0;
    var story = fillTemplate(template, { seed: fillSeed });
    story.seed = seed;      // the shareable/reproducible seed
    story.genre = template.genre;
    return story;
  }

  /*
   * Generate `n` de-duplicated stories. Guarantees distinct `signature`s, so
   * "give me 5000 different madlibs" yields 5000 genuinely different stories.
   * `attemptsCap` guards against exhausting a tiny genre pool.
   */
  function generateMany(n, opts) {
    opts = opts || {};
    var seen = Object.create(null);
    var out = [];
    var attempts = 0;
    var cap = opts.attemptsCap || (n * 40 + 1000);
    while (out.length < n && attempts < cap) {
      attempts++;
      var story = generate({ genre: opts.genre });
      if (seen[story.signature]) continue;
      seen[story.signature] = true;
      out.push(story);
    }
    return out;
  }

  /*
   * Estimate how many DISTINCT stories are possible: for each template,
   * multiply the dictionary size of every blank (a reused #tag counts once).
   * Returned as a base-10 logarithm because the true count is astronomically
   * large and overflows a JS Number.
   */
  function estimateCombinations() {
    var totalLog10 = -Infinity; // log10 of a running sum
    function log10SumExp(a, b) {
      // log10(10^a + 10^b) without overflow
      if (a === -Infinity) return b;
      if (b === -Infinity) return a;
      var hi = Math.max(a, b), lo = Math.min(a, b);
      return hi + Math.log10(1 + Math.pow(10, lo - hi));
    }
    TEMPLATES.forEach(function (t) {
      var seenTags = Object.create(null);
      var log = 0;
      t.beats.forEach(function (b) {
        var m;
        PLACEHOLDER.lastIndex = 0;
        while ((m = PLACEHOLDER.exec(b.text)) !== null) {
          var category = m[1], tag = m[2];
          if (RESERVED[category]) continue;
          var list = DICT[category];
          if (!list || !list.length) continue;
          if (tag) {
            var key = category + '#' + tag;
            if (seenTags[key]) continue;
            seenTags[key] = true;
          }
          log += Math.log10(list.length);
        }
      });
      totalLog10 = log10SumExp(totalLog10, log);
    });
    return {
      log10: totalLog10,
      // A friendly "≈ 10^N" plus a leading-digits form.
      pretty: prettyFromLog10(totalLog10)
    };
  }

  function prettyFromLog10(log10) {
    if (log10 === -Infinity) return '0';
    var exp = Math.floor(log10);
    var mantissa = Math.pow(10, log10 - exp);
    var digits = mantissa.toFixed(1);
    return digits + ' × 10^' + exp;
  }

  // Render a story to plain Markdown (used by copy / export).
  function toMarkdown(story) {
    var lines = ['# ' + story.title, '', '_Genre: ' + story.genre +
      ' • Seed: ' + story.seed + '_', ''];
    story.beats.forEach(function (b) {
      lines.push('**' + b.label + '**');
      lines.push('');
      lines.push(b.text);
      lines.push('');
    });
    return lines.join('\n').trim() + '\n';
  }

  /*
   * Build a production brief for a story — the hand-off document the user gives
   * to a writer (Claude) to turn the idea into a 30-minute episode script.
   * Reuses toMarkdown for the idea itself and appends a fixed production spec.
   */
  function toBrief(story) {
    return [
      '# Production Brief — ' + story.title,
      '',
      '> Hand this to Claude to write the episode. Ask: ' +
        '“Write the 30-minute script for this madlib.”',
      '',
      '## The story idea',
      '',
      toMarkdown(story).trim(),
      '',
      '## Production spec',
      '',
      '- **Format:** half-hour television episode (teleplay), ~25–30 pages.',
      '- **Structure:** Cold Open → Act One → Act Two → Act Three (short) → Tag.',
      '- **Deliverable:** 3 distinct takes (differ in tone / POV / structure), ' +
        'so I can choose one.',
      '- **Then:** from the chosen script, produce a visual shot-list storyboard.',
      '- **Keep consistent:** the character names, places, and key object from ' +
        'the beats above.',
      '',
      '_Source: MadLibs Story Forge · id `' + story.id + '` · seed `' +
        story.seed + '` · signature `' + story.signature + '`_',
      ''
    ].join('\n');
  }

  var API = {
    makeRng: makeRng,
    randomSeed: randomSeed,
    pick: pick,
    fillTemplate: fillTemplate,
    generate: generate,
    generateMany: generateMany,
    estimateCombinations: estimateCombinations,
    templatesFor: templatesFor,
    toMarkdown: toMarkdown,
    toBrief: toBrief,
    get templates() { return TEMPLATES; },
    get dict() { return DICT; }
  };

  root.MadlibsGenerator = API;
  if (typeof module !== 'undefined' && module.exports) {
    module.exports = API;
  }
})(typeof window !== 'undefined' ? window : this);
