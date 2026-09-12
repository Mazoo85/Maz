/*
 * SCRIPT FORGE — the artist.
 * --------------------------
 * Draws a shot. Everything on screen is drawn in code — no images to load, no
 * fonts to fetch — so a film renders identically offline, first time, every
 * time, and can be recorded straight off the canvas.
 *
 * The look: limited-palette graphic-novel silhouettes, a hard key light whose
 * colour comes from the genre and the hour, 2.35:1 letterbox, grain and gate
 * flicker over the top.
 *
 *   FilmArt.drawShot(ctx, width, height, shot, state)
 *
 * `state` carries { progress 0..1, elapsed, reel, speaking } — everything that
 * changes within a shot. The artist keeps no state of its own beyond a cache
 * of per-shot noise, so any frame can be drawn at any time.
 *
 * Exposed as window.FilmArt (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  var PARSE = root.FilmParse || (typeof require !== 'undefined' ? require('./parse.js') : {});

  /* ---------------------------------------------------------------- palette
   * Each genre owns a key light, a shadow and an accent; the hour bends all
   * three. Everything drawn is one of these five colours or black.
   */
  var GENRE_COLOUR = {
    drama:    { key: [255, 214, 160], shadow: [26, 22, 34], accent: [232, 176, 106], sky: [58, 48, 72] },
    thriller: { key: [150, 224, 255], shadow: [10, 14, 26], accent: [255, 96, 84], sky: [22, 32, 54] },
    horror:   { key: [140, 255, 190], shadow: [6, 10, 10], accent: [180, 32, 48], sky: [12, 22, 20] },
    comedy:   { key: [255, 232, 150], shadow: [46, 32, 58], accent: [255, 122, 190], sky: [104, 168, 220] },
    romance:  { key: [255, 190, 200], shadow: [38, 20, 44], accent: [255, 148, 96], sky: [96, 52, 92] },
    scifi:    { key: [150, 240, 255], shadow: [10, 12, 30], accent: [180, 120, 255], sky: [20, 26, 62] },
    mystery:  { key: [200, 226, 235], shadow: [12, 18, 24], accent: [255, 176, 64], sky: [26, 38, 48] },
    fantasy:  { key: [190, 235, 255], shadow: [22, 14, 40], accent: [140, 255, 190], sky: [46, 30, 78] },
    heist:    { key: [180, 214, 255], shadow: [8, 12, 20], accent: [255, 64, 64], sky: [18, 26, 44] },
    western:  { key: [255, 206, 128], shadow: [40, 24, 18], accent: [214, 108, 48], sky: [186, 132, 78] }
  };

  /* The hour multiplies the palette: how bright the sky is, how hard the key
   * light falls, how much colour survives in the shadows. */
  /* `key` is how bright the light is, `lift` is how much of it reaches the
   * shadows — so day is bright *and* low-contrast, night is a hard light in
   * the dark. They are separate knobs on purpose. */
  var HOUR = {
    NIGHT: { sky: 0.30, key: 0.95, lift: 0.06, warm: 0.90 },
    DAWN:  { sky: 0.72, key: 1.00, lift: 0.20, warm: 1.05 },
    DAY:   { sky: 1.15, key: 1.15, lift: 0.34, warm: 1.00 },
    DUSK:  { sky: 0.62, key: 1.05, lift: 0.16, warm: 1.15 }
  };

  function mix(a, b, t) {
    return [
      Math.round(a[0] + (b[0] - a[0]) * t),
      Math.round(a[1] + (b[1] - a[1]) * t),
      Math.round(a[2] + (b[2] - a[2]) * t)
    ];
  }

  function rgb(c, alpha) {
    return 'rgba(' + c[0] + ',' + c[1] + ',' + c[2] + ',' + (alpha == null ? 1 : alpha) + ')';
  }

  function scale(c, f) {
    return [
      Math.max(0, Math.min(255, Math.round(c[0] * f))),
      Math.max(0, Math.min(255, Math.round(c[1] * f))),
      Math.max(0, Math.min(255, Math.round(c[2] * f)))
    ];
  }

  /* Five tones that always separate: a bright key, a mid ground, a darker
   * structure ink, a near-black shadow, and a sky between. A night scene stays
   * dark without collapsing into one flat black. */
  function palette(genre, time, mood) {
    var g = GENRE_COLOUR[genre] || GENRE_COLOUR.drama;
    var h = HOUR[time] || HOUR.NIGHT;
    var tension = Math.max(0, Math.min(1, mood == null ? 0.4 : mood));
    var key = scale(g.key, h.key * h.warm);
    return {
      key: key,
      accent: g.accent,
      sky: mix(scale(g.sky, 0.4 + h.sky * 0.7), key, 0.05 + h.lift * 0.25),
      deep: mix(g.shadow, key, 0.10 + h.lift * 0.35 - tension * 0.04),
      ink: mix(g.shadow, key, 0.03 + h.lift * 0.08),
      shadow: scale(g.shadow, 0.6),
      lift: h.lift,
      tension: tension
    };
  }

  /* Stable per-shot noise: stars, grain seeds, the wobble of a silhouette.
   * Keyed off the shot index so a frame drawn twice is drawn the same. */
  var noiseCache = {};
  function noise(key, count) {
    if (noiseCache[key]) return noiseCache[key];
    var rng = PARSE.makeRng(PARSE.hashText(String(key)));
    var out = [];
    for (var i = 0; i < count; i++) out.push([rng(), rng(), rng()]);
    noiseCache[key] = out;
    return out;
  }

  var API = {
    palette: palette,
    noise: noise,
    rgb: rgb,
    mix: mix
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmArt = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
