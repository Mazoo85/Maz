/*
 * CODA PICS — the painter surface other projects use.
 * ---------------------------------------------------
 * Published as `coda-pics/painter` in shared/exchange.json.
 *
 * CODA PICS' own page is a studio: type, paint, restyle, reshape, keep, save.
 * Another project wants one call — words in, picture on a canvas — and one
 * thing more that the studio does not need.
 *
 * That extra thing is honesty about the result. CODA PICS is built so that a
 * blank page is never a blank page: given words it does not recognise it
 * invents a subject, a setting and a style from the prompt's own hash and
 * paints those. Standalone that is exactly right — you always get a picture,
 * and pressing again gives you another. Handed to another project it is a
 * trap: "a stairwell" paints a comet over a swamp, and the caller has no way to
 * tell that from a picture of what it asked for.
 *
 * So `paint()` reports how much of the picture came from the words, and
 * `paintIfRecognised()` refuses rather than guessing. A project illustrating
 * something can then choose: show the picture, or show nothing, which is the
 * better of the two when the alternative is a confident picture of the wrong
 * thing.
 *
 * Exposed as window.MazPainter (and module.exports for tests).
 */
(function (root) {
  'use strict';

  var PROMPT = root.CodaPrompt ||
    (typeof require !== 'undefined' ? require('./prompt.js') : null);
  var PAINT = root.CodaPaint ||
    (typeof require !== 'undefined' ? require('./paint.js') : null);
  var FINISH = root.CodaFinish ||
    (typeof require !== 'undefined' ? require('./finish.js') : null);

  /* The default bar for paintIfRecognised: the thing in the picture has to have
   * been named. Everything else — the hour, the weather, the art style — is
   * decoration CODA PICS may reasonably choose. The subject is the picture. */
  var DEFAULT_MIN_RATIO = 0.34;

  /** Every art style a caller may ask for. */
  function styles() {
    return FINISH && FINISH.STYLE ? Object.keys(FINISH.STYLE) : [];
  }

  /**
   * Read a prompt without painting it, to see what CODA PICS makes of it.
   * Returns { grounded, describe } — cheap, so a caller can ask first.
   */
  function read(text, opts) {
    opts = opts || {};
    if (!PROMPT) return null;
    var spec = PROMPT.parse(text, { seed: opts.seed == null ? 1 : opts.seed });
    return {
      grounded: spec.grounded,
      describe: PROMPT.describe(spec),
      spec: spec
    };
  }

  /**
   * Paint `text` onto a 2D context.
   *
   * opts: { seed, style }  — style is one of styles(); omitted, CODA PICS picks
   * one from the prompt as it does on its own page.
   *
   * Returns { painted, grounded, describe, style } — or null if CODA PICS is
   * not fully loaded, so a consumer that half-loaded it finds out at once
   * rather than painting nothing and wondering.
   */
  function paint(ctx, w, h, text, opts) {
    opts = opts || {};
    if (!PROMPT || !PAINT || !ctx || !(w > 0) || !(h > 0)) return null;

    var spec = PROMPT.parse(text, { seed: opts.seed == null ? 1 : opts.seed });
    if (opts.style && FINISH && FINISH.STYLE && FINISH.STYLE[opts.style]) spec.style = opts.style;

    var palette = PAINT.render(ctx, w, h, spec);
    if (FINISH && FINISH.apply) FINISH.apply(ctx, w, h, spec, palette);

    return {
      painted: true,
      grounded: spec.grounded,
      describe: PROMPT.describe(spec),
      style: spec.style
    };
  }

  /**
   * Paint only if CODA PICS actually recognised what was asked for.
   *
   * `minRatio` (default 0.34) is the share of the picture that must come from
   * the words; `requireSubject` (default true) insists the thing in the picture
   * was named rather than invented. Returns the same object as paint() with
   * `painted: false` and nothing drawn when the bar is not met, so the caller
   * can fall back to its own artwork.
   */
  function paintIfRecognised(ctx, w, h, text, opts) {
    opts = opts || {};
    var look = read(text, opts);
    if (!look) return null;

    var minRatio = typeof opts.minRatio === 'number' ? opts.minRatio : DEFAULT_MIN_RATIO;
    var needSubject = opts.requireSubject !== false;
    var g = look.grounded;
    if (g.ratio < minRatio || (needSubject && !g.subject)) {
      return { painted: false, grounded: g, describe: look.describe, style: null };
    }
    return paint(ctx, w, h, text, opts);
  }

  var API = {
    paint: paint,
    paintIfRecognised: paintIfRecognised,
    read: read,
    styles: styles,
    DEFAULT_MIN_RATIO: DEFAULT_MIN_RATIO
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.MazPainter = API;
})(typeof window !== 'undefined' ? window : this);
