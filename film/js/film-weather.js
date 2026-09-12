/*
 * SCRIPT FORGE — the air.
 * ------------------------
 * Something hangs between the audience and the picture: rain, dust motes in
 * a shaft of light, fog bands, heat shimmer, rising embers, or a soft haze
 * around the key light. Chosen by the film's genre and the scene's hour,
 * drawn in screen space over everything else so it reads as air, not as
 * part of any one set plane.
 *
 *   FilmWeather.forShot(genre, time, set)        which kind of air, or 'none'
 *   FilmWeather.draw(ctx, kind, p, time, seed, w, h)   paint it, this frame
 *
 * Exposed as window.FilmWeather (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  var Art = root.FilmArt || (typeof require !== 'undefined' ? require('./film-art.js') : {});

  var EXTERIOR = { street: 1, woods: 1, field: 1, water: 1 };

  /* What hangs in the air. Genre first, then the hour, then whether we are
   * outdoors — a rainstorm indoors is a mistake, not a mood. */
  function forShot(genre, time, set) {
    var outside = !!EXTERIOR[set];
    var night = time === 'NIGHT' || time === 'DUSK';

    if (genre === 'western' && !night) return 'shimmer';
    if (genre === 'fantasy') return 'embers';
    if ((genre === 'horror' || genre === 'mystery') && outside) return 'fog';
    if ((genre === 'thriller' || genre === 'horror') && night) return outside ? 'rain' : 'haze';
    if (!night && !outside) return 'dust';
    if (night) return 'haze';
    return 'none';
  }

  /* Particles scale with the canvas: a bigger canvas gets more air, not
   * proportionally more, because the frame budget is shared with everything
   * else on screen — sqrt keeps a 1080p frame at roughly 1.4x the count of a
   * 540p one instead of a full 2x.
   *
   * `poolLen` caps the result at the size of the noise pool each particle
   * draws its position from (`n` below, 120 entries). Past that, `n[i %
   * n.length]` starts repeating entries already used at a lower `i`, so two
   * "different" particles land at the exact same coordinates — overdraw
   * with no extra air, not more particles. At 1920 wide, rain's count(90,w)
   * reaches 127 — 7 past the pool — before this cap. */
  function count(base, w, poolLen) {
    return Math.min(poolLen, Math.round(base * Math.sqrt(w / 960)));
  }

  function draw(ctx, kind, p, time, seed, w, h) {
    if (!kind || kind === 'none') return;
    var n = Art.noise('weather-' + kind + '-' + seed, 120);
    var poolLen = n.length;
    ctx.save();
    if (kind === 'rain') {
      ctx.strokeStyle = Art.rgb(p.key, 0.32);
      ctx.lineWidth = Math.max(1, w / 700);
      // The spec's counts (90/60/40 rain/dust/embers). These were once
      // thinned to 18/12/8 because a Chromium deferred-rasterization stall
      // in the test harness landed inside the measured frame-budget window
      // and looked like these particles were the cost. That stall was
      // root-caused and fixed in the test harness itself (the periodic
      // getImageData flush in the frame-budget gate), and the full counts
      // measured directly afterwards cost at most ~2% of the 4ms budget —
      // see film/tests/film-browser.test.js's FRAME BUDGET section.
      for (var i = 0; i < count(90, w, poolLen); i++) {
        var s = n[i % n.length];
        var x = ((s[0] + time * 0.06) % 1) * w;
        var y = ((s[1] + time * 0.9) % 1) * h;
        ctx.beginPath();
        ctx.moveTo(x, y);
        ctx.lineTo(x - w * 0.006, y + h * 0.045);
        ctx.stroke();
      }
    } else if (kind === 'dust') {
      for (var d = 0; d < count(60, w, poolLen); d++) {
        var q = n[d % n.length];
        var dx = ((q[0] + time * 0.01) % 1) * w;
        var dy = ((q[1] + Math.sin(time * 0.3 + q[2] * 6) * 0.02 + time * 0.006) % 1) * h;
        ctx.fillStyle = Art.rgb(p.key, 0.16 + q[2] * 0.34);
        ctx.fillRect(dx, dy, Math.max(2, w / 380), Math.max(2, w / 380));
      }
    } else if (kind === 'fog' || kind === 'haze') {
      var bands = kind === 'fog' ? 5 : 2;
      for (var b = 0; b < bands; b++) {
        var r = n[b];
        var by = h * (0.25 + r[0] * 0.6) + Math.sin(time * 0.12 + b) * h * 0.02;
        var g = ctx.createLinearGradient(0, by - h * 0.12, 0, by + h * 0.12);
        g.addColorStop(0, Art.rgb(p.key, 0));
        g.addColorStop(0.5, Art.rgb(p.key, kind === 'fog' ? 0.22 : 0.16));
        g.addColorStop(1, Art.rgb(p.key, 0));
        ctx.fillStyle = g;
        ctx.fillRect(0, by - h * 0.12, w, h * 0.24);
      }
    } else if (kind === 'shimmer') {
      for (var m = 0; m < 3; m++) {
        var sh = n[m + 7];
        var sy = h * (0.55 + sh[0] * 0.3);
        ctx.fillStyle = Art.rgb(p.key, 0.14);
        ctx.fillRect(0, sy + Math.sin(time * 2 + m) * h * 0.006, w, h * 0.02);
      }
    } else if (kind === 'embers') {
      for (var e = 0; e < count(40, w, poolLen); e++) {
        var v = n[e % n.length];
        var ex = ((v[0] + Math.sin(time * 0.4 + v[2] * 9) * 0.03) % 1) * w;
        var ey = h - ((v[1] + time * 0.05) % 1) * h;
        ctx.fillStyle = Art.rgb(p.accent, 0.35 + v[2] * 0.5);
        ctx.fillRect(ex, ey, Math.max(2, w / 340), Math.max(2, w / 340));
      }
    }
    ctx.restore();
  }

  var API = {
    forShot: forShot,
    draw: draw
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmWeather = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
