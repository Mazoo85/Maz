/* =============================================================================
 *  NEON CELLS  —  seeded randomness
 *
 *  Every run is generated from one seed, so a run can be replayed, and the
 *  level generator can be tested: the same seed must always build the same
 *  level. Nothing in the game calls Math.random() directly — it all comes
 *  through here, which is what makes that promise keepable.
 * ========================================================================== */
(function (global) {
  'use strict';

  /* FNV-1a, so a string seed ("run-7", a daily date) becomes a usable number. */
  function hashSeed(value) {
    const str = String(value);
    let h = 2166136261 >>> 0;
    for (let i = 0; i < str.length; i++) {
      h ^= str.charCodeAt(i);
      h = Math.imul(h, 16777619);
    }
    return h >>> 0;
  }

  /* mulberry32: tiny, fast, good enough for a game, and fully deterministic. */
  function Rng(seed) {
    let s =
      typeof seed === 'number' && isFinite(seed)
        ? seed >>> 0
        : hashSeed(seed == null ? Date.now() + ':' + Math.random() : seed);
    if (s === 0) s = 0x9e3779b9;

    const api = {
      seed: s,

      next() {
        s = (s + 0x6d2b79f5) >>> 0;
        let t = s;
        t = Math.imul(t ^ (t >>> 15), t | 1);
        t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
      },

      float(a, b) {
        return a + (b - a) * api.next();
      },

      /* Inclusive on both ends — the way level coordinates want it. */
      int(a, b) {
        return Math.floor(api.float(a, b + 1));
      },

      chance(p) {
        return api.next() < p;
      },

      sign() {
        return api.next() < 0.5 ? -1 : 1;
      },

      pick(list) {
        return list[Math.floor(api.next() * list.length)];
      },

      shuffle(list) {
        const out = list.slice();
        for (let i = out.length - 1; i > 0; i--) {
          const j = Math.floor(api.next() * (i + 1));
          const tmp = out[i];
          out[i] = out[j];
          out[j] = tmp;
        }
        return out;
      },

      /* n distinct entries, or the whole list if it is shorter. */
      sample(list, n) {
        return api.shuffle(list).slice(0, Math.max(0, Math.min(n, list.length)));
      },

      /* weightOf(item) -> number. Zero-weight items are never chosen. */
      weighted(list, weightOf) {
        let total = 0;
        for (const item of list) total += Math.max(0, weightOf(item));
        if (total <= 0) return api.pick(list);
        let roll = api.next() * total;
        for (const item of list) {
          roll -= Math.max(0, weightOf(item));
          if (roll <= 0) return item;
        }
        return list[list.length - 1];
      }
    };

    return api;
  }

  const API = { Rng: Rng, hashSeed: hashSeed };

  if (typeof module === 'object' && module.exports) module.exports = API;
  global.CELLS_RNG = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
