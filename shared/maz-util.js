/*
 * The handful of helpers every MAZ ARCADE project needs.
 *
 * Nothing here is clever. Each one was written out independently in two or more
 * projects — identically, which is how `tools/inventory/main.mjs` found them —
 * and lives here now so a fix lands everywhere at once instead of in one copy
 * while the others quietly keep the bug.
 *
 * The bar for adding something: it is already duplicated, it is pure (no DOM,
 * no canvas, no audio, no state), and it means exactly the same thing in every
 * project that uses it. A `noise` belongs in each project, because SCRIPT FORGE
 * means text variation by it and ZOMBOID means a burst of static; a `clamp`
 * belongs here, because there is only one thing it can mean.
 *
 * Loaded by a page as `<script src="../shared/maz-util.js"></script>` before
 * the project's own scripts, and by a Node test as
 * `require('../../shared/maz-util.js')` — same file, both ways, like
 * shared/projects.js.
 */
(function (root) {
  'use strict';

  /** Hold `v` inside [lo, hi]. */
  function clamp(v, lo, hi) { return v < lo ? lo : v > hi ? hi : v; }

  /** Travel from `a` to `b`, `t` of the way. Not clamped: `t` outside [0,1] extrapolates. */
  function lerp(a, b, t) { return a + (b - a) * t; }

  /**
   * Make `s` safe to drop into HTML.
   *
   * Escapes the five characters that can end an attribute or open a tag, which
   * is what matters when a project renders something the person typed — a film
   * idea, a song title, a picture prompt — back onto the page.
   */
  function escapeHtml(s) {
    return String(s).replace(/[&<>"']/g, function (c) {
      return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c];
    });
  }

  /**
   * One item from `list`, chosen by the random function `rng`.
   *
   * `rng` is a seeded generator, not Math.random: every project here rebuilds
   * the same film, song or picture from the same words, and that only holds if
   * every choice comes from the seed. The modulo is belt and braces for an
   * `rng` that returns exactly 1.
   */
  function pick(list, rng) {
    return list[Math.floor(rng() * list.length) % list.length];
  }


  /**
   * A seeded random generator (mulberry32): call the returned function for the
   * next number in [0, 1).
   *
   * The whole arcade is built on reproducibility — the same seed rebuilds the
   * same story, the same name, the same picture — and that only works if the
   * randomness comes from the seed rather than from Math.random. Thirty-two
   * bits of state, four operations, identical results in every browser and in
   * Node, which is what lets a logic test check a specific seed.
   */
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

  /** A fresh 32-bit seed for makeRng, for when nobody asked for a particular one. */
  function randomSeed() {
    return Math.floor(Math.random() * 4294967296) >>> 0;
  }

  var API = {
    clamp: clamp,
    lerp: lerp,
    escapeHtml: escapeHtml,
    pick: pick,
    makeRng: makeRng,
    randomSeed: randomSeed
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.MazUtil = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
