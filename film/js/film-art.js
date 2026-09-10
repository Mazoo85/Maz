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

  /* ------------------------------------------------------------------ sets
   * Each set draws into a 1000 x 420 "world" box; the camera transform above
   * decides how much of it you see. Back to front, always.
   */
  var SETS = {};

  function ground(ctx, p, y) {
    ctx.fillStyle = rgb(p.deep);
    ctx.fillRect(0, y, 1000, 420 - y);
  }

  SETS.lighthouse = function (ctx, p, n) {
    // Inside the lamp room: the lens, the glazing, the sea a long way down.
    ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 0, 1000, 420);
    // sea and horizon through the glass
    ctx.fillStyle = rgb(p.sky); ctx.fillRect(60, 60, 880, 150);
    ctx.fillStyle = rgb(mix(p.deep, p.key, 0.12)); ctx.fillRect(60, 176, 880, 120);
    for (var i = 0; i < 22; i++) {
      var s = n[i];
      ctx.fillStyle = rgb(p.key, 0.08 + s[2] * 0.12);
      ctx.fillRect(60 + s[0] * 860, 182 + s[1] * 108, 40 + s[2] * 70, 2);
    }
    // glazing bars
    ctx.fillStyle = rgb(p.ink);
    ctx.fillRect(0, 0, 1000, 62); ctx.fillRect(0, 292, 1000, 30);
    for (var g = 0; g < 5; g++) ctx.fillRect(60 + g * 220, 60, 16, 236);
    // the lens, lit, throwing a beam out across the water
    var beam = ctx.createLinearGradient(300, 190, 60, 120);
    beam.addColorStop(0, rgb(p.key, 0.45)); beam.addColorStop(1, rgb(p.key, 0));
    ctx.fillStyle = beam;
    ctx.beginPath(); ctx.moveTo(300, 150); ctx.lineTo(60, 92); ctx.lineTo(60, 210); ctx.lineTo(300, 220);
    ctx.closePath(); ctx.fill();
    ctx.fillStyle = rgb(p.ink); ctx.fillRect(268, 120, 120, 180);
    ctx.fillStyle = rgb(p.key, 0.92);
    ctx.beginPath(); ctx.ellipse(328, 190, 44, 64, 0, 0, Math.PI * 2); ctx.fill();
    ctx.fillStyle = rgb(p.deep, 0.55);
    for (var r = 0; r < 5; r++) ctx.fillRect(284, 136 + r * 26, 88, 6);
    // floor
    ctx.fillStyle = rgb(p.deep); ctx.fillRect(0, 322, 1000, 98);
    ctx.fillStyle = rgb(p.key, 0.06); ctx.fillRect(0, 322, 1000, 4);
  };

  SETS.kitchen = function (ctx, p, n) {
    ctx.fillStyle = rgb(mix(p.deep, p.sky, 0.25));
    ctx.fillRect(0, 0, 1000, 420);
    // window with light
    var g = ctx.createLinearGradient(120, 40, 420, 300);
    g.addColorStop(0, rgb(p.key, 0.55));
    g.addColorStop(1, rgb(p.key, 0));
    ctx.fillStyle = rgb(p.key, 0.75); ctx.fillRect(140, 50, 220, 170);
    ctx.fillStyle = rgb(p.ink); ctx.fillRect(244, 50, 10, 170); ctx.fillRect(140, 128, 220, 10);
    ctx.fillStyle = g; ctx.fillRect(120, 40, 400, 300);
    // counter + cupboards
    ctx.fillStyle = rgb(p.ink);
    ctx.fillRect(0, 300, 1000, 22);
    ctx.fillRect(600, 60, 340, 120);
    ctx.fillStyle = rgb(p.deep); ctx.fillRect(770, 60, 8, 120);
    ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 322, 1000, 98);
    // kettle + two cups
    ctx.fillStyle = rgb(p.ink);
    ctx.fillRect(660, 268, 44, 32);
    ctx.fillRect(740, 282, 24, 18); ctx.fillRect(784, 282, 24, 18);
  };

  SETS.room = function (ctx, p, n) {
    ctx.fillStyle = rgb(mix(p.deep, p.sky, 0.18));
    ctx.fillRect(0, 0, 1000, 420);
    // doorway, lit from beyond
    ctx.fillStyle = rgb(p.shadow); ctx.fillRect(620, 40, 180, 300);
    ctx.fillStyle = rgb(p.key, 0.5); ctx.fillRect(632, 52, 156, 276);
    ctx.fillStyle = rgb(p.deep); ctx.fillRect(648, 68, 124, 244);
    // lamp pool on the floor
    var g = ctx.createRadialGradient(250, 300, 10, 250, 300, 260);
    g.addColorStop(0, rgb(p.key, 0.32)); g.addColorStop(1, rgb(p.key, 0));
    ctx.fillStyle = g; ctx.fillRect(0, 140, 560, 280);
    ctx.fillStyle = rgb(p.ink);
    ctx.fillRect(200, 240, 100, 12); ctx.fillRect(244, 252, 12, 60); // table
    ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 330, 1000, 90);
  };

  SETS.corridor = function (ctx, p, n) {
    ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 0, 1000, 420);
    // one-point perspective: receding doorframes
    for (var i = 6; i >= 1; i--) {
      var w = 120 + i * 130, h = 90 + i * 46;
      var x = 500 - w / 2, y = 210 - h / 2;
      ctx.fillStyle = rgb(mix(p.shadow, p.deep, i / 7));
      ctx.fillRect(x, y, w, h);
      ctx.fillStyle = rgb(p.key, 0.06 + i * 0.02);
      ctx.fillRect(x, y, w, 6);
    }
    var g = ctx.createRadialGradient(500, 210, 8, 500, 210, 190);
    g.addColorStop(0, rgb(p.key, 0.5)); g.addColorStop(1, rgb(p.key, 0));
    ctx.fillStyle = g; ctx.fillRect(300, 60, 400, 320);
    ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 340, 1000, 80);
  };

  SETS.woods = function (ctx, p, n) {
    var g = ctx.createLinearGradient(0, 0, 0, 340);
    g.addColorStop(0, rgb(p.sky)); g.addColorStop(1, rgb(p.deep));
    ctx.fillStyle = g; ctx.fillRect(0, 0, 1000, 340);
    ctx.fillStyle = rgb(p.key, 0.16);
    ctx.beginPath(); ctx.arc(300, 96, 44, 0, Math.PI * 2); ctx.fill();
    // canopy closing over the top of the frame
    ctx.fillStyle = rgb(p.ink, 0.85);
    ctx.beginPath();
    ctx.moveTo(0, 0); ctx.lineTo(1000, 0); ctx.lineTo(1000, 70);
    for (var c = 10; c >= 0; c--) {
      var q = n[c + 30];
      ctx.quadraticCurveTo(c * 100 + 50, 40 + q[0] * 90, c * 100, 60 + q[1] * 40);
    }
    ctx.closePath(); ctx.fill();
    // trunks: tapered, in three depths, with a low branch or two
    for (var layer = 0; layer < 3; layer++) {
      var alpha = 0.40 + layer * 0.28;
      var count = 7 - layer;
      for (var i = 0; i < count; i++) {
        var s = n[layer * 9 + i];
        var x = 40 + ((i + layer * 0.4) / count) * 980 + s[0] * 60;
        var wBase = 14 + s[1] * 26 + layer * 12;
        var top = 30 + layer * 26;
        ctx.fillStyle = rgb(p.ink, alpha);
        ctx.beginPath();
        ctx.moveTo(x - wBase / 2, 360);
        ctx.lineTo(x - wBase * 0.28, top);
        ctx.lineTo(x + wBase * 0.28, top);
        ctx.lineTo(x + wBase / 2, 360);
        ctx.closePath(); ctx.fill();
        if (s[2] > 0.5) {
          ctx.save();
          ctx.strokeStyle = rgb(p.ink, alpha);
          ctx.lineWidth = 4 + layer * 2;
          ctx.beginPath();
          ctx.moveTo(x, 120 + s[1] * 70);
          ctx.lineTo(x + (s[0] > 0.5 ? 1 : -1) * (50 + s[2] * 60), 80 + s[0] * 60);
          ctx.stroke();
          ctx.restore();
        }
      }
    }
    ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 336, 1000, 84);
    ctx.fillStyle = rgb(p.key, 0.05); ctx.fillRect(0, 336, 1000, 3);
  };

  SETS.street = function (ctx, p, n) {
    var g = ctx.createLinearGradient(0, 0, 0, 300);
    g.addColorStop(0, rgb(p.sky)); g.addColorStop(1, rgb(mix(p.sky, p.key, 0.25)));
    ctx.fillStyle = g; ctx.fillRect(0, 0, 1000, 300);
    // skyline
    for (var i = 0; i < 16; i++) {
      var s = n[i];
      var w = 50 + s[1] * 90, h = 80 + s[2] * 190;
      ctx.fillStyle = rgb(p.ink, 0.92);
      ctx.fillRect(s[0] * 1000 - w / 2, 300 - h, w, h);
      for (var wnd = 0; wnd < 5; wnd++) {
        var q = n[(i * 5 + wnd + 20) % n.length];
        if (q[2] > 0.55) {
          ctx.fillStyle = rgb(p.key, 0.28 + q[0] * 0.3);
          ctx.fillRect(s[0] * 1000 - w / 2 + 10 + q[0] * (w - 24), 300 - h + 14 + q[1] * (h - 30), 8, 10);
        }
      }
    }
    ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 300, 1000, 120);
    // streetlight pool
    var pool = ctx.createRadialGradient(760, 300, 6, 760, 300, 230);
    pool.addColorStop(0, rgb(p.key, 0.36)); pool.addColorStop(1, rgb(p.key, 0));
    ctx.fillStyle = pool; ctx.fillRect(520, 160, 480, 260);
    ctx.fillStyle = rgb(p.ink); ctx.fillRect(756, 90, 8, 210);
    ctx.fillStyle = rgb(p.key, 0.85); ctx.fillRect(736, 82, 48, 12);
  };

  SETS.field = function (ctx, p, n) {
    var g = ctx.createLinearGradient(0, 0, 0, 300);
    g.addColorStop(0, rgb(p.sky)); g.addColorStop(1, rgb(mix(p.sky, p.key, 0.4)));
    ctx.fillStyle = g; ctx.fillRect(0, 0, 1000, 300);
    ctx.fillStyle = rgb(p.key, 0.22);
    ctx.beginPath(); ctx.arc(720, 250, 60, 0, Math.PI * 2); ctx.fill();
    ctx.fillStyle = rgb(p.ink, 0.7);
    ctx.beginPath(); ctx.moveTo(0, 300); ctx.lineTo(260, 236); ctx.lineTo(520, 300); ctx.closePath(); ctx.fill();
    ctx.fillStyle = rgb(p.deep); ctx.fillRect(0, 296, 1000, 124);
    // fence posts running to the horizon
    for (var i = 0; i < 10; i++) {
      ctx.fillStyle = rgb(p.ink, 0.85);
      ctx.fillRect(60 + i * 96, 292 - i * 2, 7, 30 + i * 4);
    }
  };

  SETS.vehicle = function (ctx, p, n) {
    ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 0, 1000, 420);
    // windscreen: road running away
    var g = ctx.createLinearGradient(0, 40, 0, 260);
    g.addColorStop(0, rgb(p.sky)); g.addColorStop(1, rgb(p.deep));
    ctx.fillStyle = g; ctx.fillRect(90, 40, 820, 220);
    ctx.fillStyle = rgb(p.ink, 0.9);
    ctx.beginPath(); ctx.moveTo(300, 260); ctx.lineTo(470, 150); ctx.lineTo(530, 150); ctx.lineTo(700, 260);
    ctx.closePath(); ctx.fill();
    ctx.fillStyle = rgb(p.key, 0.5);
    for (var i = 0; i < 4; i++) ctx.fillRect(494, 168 + i * 26, 12, 14 - i * 2);
    // dashboard
    ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 250, 1000, 170);
    ctx.fillStyle = rgb(p.ink); ctx.fillRect(60, 260, 880, 24);
    ctx.fillStyle = rgb(p.accent, 0.8); ctx.beginPath(); ctx.arc(220, 300, 18, 0, Math.PI * 2); ctx.fill();
    ctx.strokeStyle = rgb(p.ink); ctx.lineWidth = 14;
    ctx.beginPath(); ctx.arc(700, 360, 80, Math.PI, Math.PI * 2); ctx.stroke();
  };

  SETS.industrial = function (ctx, p, n) {
    ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 0, 1000, 420);
    // rafters + hanging lamps
    ctx.fillStyle = rgb(p.ink);
    for (var i = 0; i < 5; i++) ctx.fillRect(i * 220 + 40, 0, 18, 340);
    ctx.fillRect(0, 44, 1000, 12);
    for (var l = 0; l < 3; l++) {
      var x = 180 + l * 320;
      ctx.fillStyle = rgb(p.ink); ctx.fillRect(x - 2, 56, 4, 60);
      ctx.fillStyle = rgb(p.key, 0.9); ctx.fillRect(x - 22, 116, 44, 12);
      var g = ctx.createRadialGradient(x, 128, 6, x, 128, 220);
      g.addColorStop(0, rgb(p.key, 0.30)); g.addColorStop(1, rgb(p.key, 0));
      ctx.fillStyle = g; ctx.fillRect(x - 230, 100, 460, 320);
    }
    // crates
    ctx.fillStyle = rgb(p.ink, 0.95);
    ctx.fillRect(60, 250, 130, 90); ctx.fillRect(200, 286, 90, 54); ctx.fillRect(820, 260, 120, 80);
    ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 336, 1000, 84);
  };

  SETS.office = function (ctx, p, n) {
    ctx.fillStyle = rgb(mix(p.deep, p.sky, 0.15)); ctx.fillRect(0, 0, 1000, 420);
    // blinds
    for (var i = 0; i < 14; i++) {
      ctx.fillStyle = rgb(p.key, 0.30 - i * 0.008);
      ctx.fillRect(80, 40 + i * 18, 380, 9);
    }
    ctx.fillStyle = rgb(p.ink); ctx.fillRect(70, 30, 12, 280);
    // desk + lamp + shelves
    ctx.fillStyle = rgb(p.ink); ctx.fillRect(520, 60, 420, 200);
    ctx.fillStyle = rgb(p.deep);
    for (var s = 0; s < 3; s++) ctx.fillRect(530, 74 + s * 62, 400, 10);
    ctx.fillStyle = rgb(p.ink); ctx.fillRect(120, 286, 420, 18); ctx.fillRect(140, 304, 16, 60);
    var g = ctx.createRadialGradient(330, 280, 8, 330, 280, 200);
    g.addColorStop(0, rgb(p.key, 0.34)); g.addColorStop(1, rgb(p.key, 0));
    ctx.fillStyle = g; ctx.fillRect(120, 140, 420, 280);
    ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 340, 1000, 80);
  };

  SETS.bar = function (ctx, p, n) {
    ctx.fillStyle = rgb(mix(p.deep, p.shadow, 0.4)); ctx.fillRect(0, 0, 1000, 420);
    // booth windows with night outside
    ctx.fillStyle = rgb(p.sky, 0.8); ctx.fillRect(60, 60, 260, 150);
    ctx.fillStyle = rgb(p.ink); ctx.fillRect(180, 60, 10, 150);
    // back bar bottles
    ctx.fillStyle = rgb(p.ink); ctx.fillRect(560, 70, 380, 150);
    for (var i = 0; i < 12; i++) {
      var s = n[i];
      ctx.fillStyle = rgb(p.accent, 0.35 + s[2] * 0.4);
      ctx.fillRect(580 + i * 30, 120 + s[0] * 20, 12, 74 - s[1] * 20);
    }
    // counter
    ctx.fillStyle = rgb(p.ink); ctx.fillRect(0, 290, 1000, 26);
    ctx.fillStyle = rgb(p.key, 0.22); ctx.fillRect(0, 290, 1000, 4);
    ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 316, 1000, 104);
    // two stools
    ctx.fillStyle = rgb(p.ink);
    ctx.fillRect(300, 330, 12, 70); ctx.fillRect(276, 322, 60, 10);
    ctx.fillRect(660, 330, 12, 70); ctx.fillRect(636, 322, 60, 10);
  };

  SETS.ship = function (ctx, p, n) {
    ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 0, 1000, 420);
    // viewport onto stars
    ctx.save();
    ctx.beginPath(); ctx.ellipse(500, 190, 330, 150, 0, 0, Math.PI * 2); ctx.clip();
    ctx.fillStyle = '#04060f'; ctx.fillRect(170, 40, 660, 300);
    for (var i = 0; i < 60; i++) {
      var s = n[i % n.length];
      ctx.fillStyle = rgb(p.key, 0.25 + s[2] * 0.7);
      ctx.fillRect(180 + s[0] * 640, 50 + s[1] * 280, 2 + s[2] * 2, 2 + s[2] * 2);
    }
    ctx.fillStyle = rgb(p.accent, 0.25);
    ctx.beginPath(); ctx.arc(700, 260, 90, 0, Math.PI * 2); ctx.fill();
    ctx.restore();
    ctx.strokeStyle = rgb(p.ink); ctx.lineWidth = 26;
    ctx.beginPath(); ctx.ellipse(500, 190, 330, 150, 0, 0, Math.PI * 2); ctx.stroke();
    // console lights
    ctx.fillStyle = rgb(p.ink); ctx.fillRect(0, 330, 1000, 90);
    for (var c = 0; c < 10; c++) {
      var q = n[(c + 12) % n.length];
      ctx.fillStyle = rgb(q[2] > 0.6 ? p.accent : p.key, 0.5 + q[0] * 0.5);
      ctx.fillRect(90 + c * 88, 348, 26, 8);
    }
  };

  SETS.water = function (ctx, p, n) {
    var g = ctx.createLinearGradient(0, 0, 0, 280);
    g.addColorStop(0, rgb(p.sky)); g.addColorStop(1, rgb(mix(p.sky, p.key, 0.35)));
    ctx.fillStyle = g; ctx.fillRect(0, 0, 1000, 280);
    ctx.fillStyle = rgb(mix(p.deep, p.key, 0.16)); ctx.fillRect(0, 280, 1000, 140);
    for (var i = 0; i < 30; i++) {
      var s = n[i];
      ctx.fillStyle = rgb(p.key, 0.08 + s[2] * 0.16);
      ctx.fillRect(s[0] * 1000, 286 + s[1] * 120, 60 + s[2] * 90, 3);
    }
    // jetty
    ctx.fillStyle = rgb(p.ink);
    ctx.fillRect(0, 300, 460, 16);
    for (var j = 0; j < 5; j++) ctx.fillRect(40 + j * 100, 316, 12, 70);
    ctx.fillStyle = rgb(p.ink, 0.8); ctx.fillRect(760, 250, 90, 34); // far boat
  };

  SETS.ward = function (ctx, p, n) {
    ctx.fillStyle = rgb(mix(p.deep, [255, 255, 255], 0.10 + p.lift * 0.2));
    ctx.fillRect(0, 0, 1000, 420);
    ctx.fillStyle = rgb(p.key, 0.5); ctx.fillRect(120, 24, 300, 14); // strip light
    var g = ctx.createLinearGradient(0, 24, 0, 300);
    g.addColorStop(0, rgb(p.key, 0.24)); g.addColorStop(1, rgb(p.key, 0));
    ctx.fillStyle = g; ctx.fillRect(60, 24, 420, 300);
    // curtain rail + curtain
    ctx.fillStyle = rgb(p.ink); ctx.fillRect(560, 40, 380, 8);
    for (var i = 0; i < 12; i++) {
      ctx.fillStyle = rgb(p.deep, 0.55 + (i % 2) * 0.2);
      ctx.fillRect(566 + i * 31, 48, 26, 250);
    }
    // bed
    ctx.fillStyle = rgb(p.ink); ctx.fillRect(140, 280, 360, 20); ctx.fillRect(150, 300, 14, 70);
    ctx.fillRect(476, 300, 14, 70); ctx.fillRect(120, 210, 20, 90);
    ctx.fillStyle = rgb(p.accent, 0.8); ctx.fillRect(540, 250, 40, 10); // monitor blip
    ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 370, 1000, 50);
  };

  SETS.chapel = function (ctx, p, n) {
    ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 0, 1000, 420);
    // arched window
    ctx.save();
    ctx.beginPath();
    ctx.moveTo(400, 320); ctx.lineTo(400, 130);
    ctx.arc(500, 130, 100, Math.PI, 0); ctx.lineTo(600, 320); ctx.closePath();
    ctx.clip();
    ctx.fillStyle = rgb(p.key, 0.75); ctx.fillRect(400, 30, 200, 300);
    ctx.fillStyle = rgb(p.accent, 0.45); ctx.fillRect(400, 150, 200, 60);
    ctx.restore();
    var g = ctx.createLinearGradient(500, 130, 500, 420);
    g.addColorStop(0, rgb(p.key, 0.35)); g.addColorStop(1, rgb(p.key, 0));
    ctx.fillStyle = g; ctx.fillRect(300, 130, 400, 290);
    // pews
    ctx.fillStyle = rgb(p.ink);
    for (var i = 0; i < 3; i++) ctx.fillRect(120, 280 + i * 40, 760, 14);
    ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 400, 1000, 20);
  };

  /* ------------------------------------------------------------- figures
   * A silhouette, not a blob: head, neck, shoulders, a tapering torso, arms and
   * legs. It is filled near-black and *offset-stroked* in the key colour, which
   * is the cheap old trick for a rim light — the light-coloured copy peeks out
   * on the side the key comes from and the black copy covers the rest.
   */
  function bodyPath(ctx, w, h) {
    var headR = h * 0.085;
    ctx.beginPath();
    // head
    ctx.moveTo(0, -h);
    ctx.arc(0, -h + headR, headR, -Math.PI / 2, Math.PI * 1.5);
    // neck + shoulders + torso, down one side
    ctx.moveTo(-w * 0.10, -h + headR * 1.7);
    ctx.lineTo(-w * 0.16, -h + headR * 2.1);
    ctx.quadraticCurveTo(-w * 0.52, -h + headR * 2.5, -w * 0.50, -h * 0.60);
    ctx.lineTo(-w * 0.44, -h * 0.30);          // waist
    ctx.lineTo(-w * 0.46, 0);                  // leg
    ctx.lineTo(-w * 0.14, 0);
    ctx.lineTo(-w * 0.10, -h * 0.30);
    ctx.lineTo(w * 0.10, -h * 0.30);
    ctx.lineTo(w * 0.14, 0);
    ctx.lineTo(w * 0.46, 0);
    ctx.lineTo(w * 0.44, -h * 0.30);
    ctx.lineTo(w * 0.50, -h * 0.60);
    ctx.quadraticCurveTo(w * 0.52, -h + headR * 2.5, w * 0.16, -h + headR * 2.1);
    ctx.lineTo(w * 0.10, -h + headR * 1.7);
    ctx.closePath();
  }

  function armsPath(ctx, w, h, swing) {
    // Arms hang slightly away from the body, and the speaking one lifts a touch.
    ctx.beginPath();
    ctx.moveTo(-w * 0.44, -h * 0.62);
    ctx.quadraticCurveTo(-w * 0.66, -h * 0.44, -w * 0.56 - swing * w * 0.1, -h * 0.24);
    ctx.lineTo(-w * 0.40, -h * 0.26);
    ctx.quadraticCurveTo(-w * 0.46, -h * 0.46, -w * 0.34, -h * 0.60);
    ctx.closePath();
    ctx.moveTo(w * 0.44, -h * 0.62);
    ctx.quadraticCurveTo(w * 0.66, -h * 0.44, w * 0.56 + swing * w * 0.1, -h * 0.24);
    ctx.lineTo(w * 0.40, -h * 0.26);
    ctx.quadraticCurveTo(w * 0.46, -h * 0.46, w * 0.34, -h * 0.60);
    ctx.closePath();
  }

  function drawFigure(ctx, p, x, groundY, height, tint, speaking, wobble) {
    var w = height * 0.34;
    var rim = speaking ? 0.75 : 0.42;
    var lean = wobble * 0.004;

    ctx.save();
    ctx.translate(x, groundY);

    // cast shadow on the floor
    ctx.fillStyle = 'rgba(0,0,0,0.45)';
    ctx.beginPath();
    ctx.ellipse(0, 2, w * 0.75, height * 0.035, 0, 0, Math.PI * 2);
    ctx.fill();

    // a breath of separation from the background
    var halo = ctx.createRadialGradient(0, -height * 0.55, height * 0.05, 0, -height * 0.55, height * 0.75);
    halo.addColorStop(0, rgb(p.key, 0.13));
    halo.addColorStop(1, rgb(p.key, 0));
    ctx.fillStyle = halo;
    ctx.fillRect(-w * 1.6, -height * 1.25, w * 3.2, height * 1.4);

    ctx.rotate(lean);

    // rim light: the same body, offset up-left, in the key colour
    ctx.save();
    ctx.translate(-w * 0.055, -height * 0.012);
    ctx.fillStyle = rgb(p.key, rim);
    bodyPath(ctx, w, height); ctx.fill();
    armsPath(ctx, w, height, speaking ? 1 : 0); ctx.fill();
    ctx.restore();

    // the figure itself
    ctx.fillStyle = 'rgba(6,6,10,0.97)';
    bodyPath(ctx, w, height); ctx.fill();
    armsPath(ctx, w, height, speaking ? 1 : 0); ctx.fill();

    // a trace of the character\'s own colour, so two silhouettes read apart
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    ctx.fillStyle = 'hsla(' + tint + ',65%,58%,' + (speaking ? 0.16 : 0.08) + ')';
    bodyPath(ctx, w, height); ctx.fill();
    ctx.restore();

    ctx.restore();
  }

  /* --------------------------------------------------------------- objects
   * Insert shots: the thing the story turns on, lit on a dark surface. */
  var GLYPHS = {
    radio: function (ctx, p) {
      ctx.fillStyle = 'rgba(0,0,0,0.95)'; ctx.fillRect(-140, -80, 280, 160);
      ctx.fillStyle = rgb(p.key, 0.85); ctx.fillRect(-118, -58, 150, 60);
      ctx.fillStyle = rgb(p.accent, 0.9); ctx.fillRect(-110, 16, 190, 8);
      ctx.fillStyle = rgb(p.key); ctx.beginPath(); ctx.arc(90, -30, 22, 0, Math.PI * 2); ctx.fill();
      ctx.fillStyle = 'rgba(0,0,0,0.9)'; ctx.fillRect(86, -52, 8, 24);
    },
    phone: function (ctx, p) {
      ctx.fillStyle = 'rgba(0,0,0,0.95)'; ctx.fillRect(-70, -120, 140, 240);
      ctx.fillStyle = rgb(p.key, 0.8); ctx.fillRect(-56, -104, 112, 190);
      ctx.fillStyle = rgb(p.accent, 0.9); ctx.fillRect(-36, -60, 72, 10);
      ctx.fillRect(-36, -34, 52, 10);
    },
    letter: function (ctx, p) {
      ctx.fillStyle = rgb(p.key, 0.9); ctx.fillRect(-150, -100, 300, 200);
      ctx.strokeStyle = 'rgba(0,0,0,0.85)'; ctx.lineWidth = 8;
      ctx.beginPath(); ctx.moveTo(-150, -100); ctx.lineTo(0, 10); ctx.lineTo(150, -100); ctx.stroke();
      ctx.fillStyle = 'rgba(0,0,0,0.25)';
      for (var i = 0; i < 4; i++) ctx.fillRect(-110, 30 + i * 18, 220 - i * 40, 6);
    },
    photograph: function (ctx, p) {
      ctx.fillStyle = rgb(p.key, 0.92); ctx.fillRect(-150, -110, 300, 220);
      ctx.fillStyle = 'rgba(0,0,0,0.8)'; ctx.fillRect(-130, -90, 260, 150);
      ctx.fillStyle = rgb(p.accent, 0.5);
      ctx.beginPath(); ctx.arc(-40, -20, 34, 0, Math.PI * 2); ctx.fill();
      ctx.beginPath(); ctx.arc(50, -14, 30, 0, Math.PI * 2); ctx.fill();
    },
    key: function (ctx, p) {
      ctx.strokeStyle = rgb(p.key, 0.95); ctx.lineWidth = 18;
      ctx.beginPath(); ctx.arc(-70, 0, 50, 0, Math.PI * 2); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(-20, 0); ctx.lineTo(140, 0); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(110, 0); ctx.lineTo(110, 44); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(140, 0); ctx.lineTo(140, 34); ctx.stroke();
    },
    gun: function (ctx, p) {
      ctx.fillStyle = 'rgba(0,0,0,0.95)';
      ctx.fillRect(-150, -30, 240, 34);
      ctx.beginPath(); ctx.moveTo(-40, 4); ctx.lineTo(30, 4); ctx.lineTo(-10, 96); ctx.lineTo(-70, 96);
      ctx.closePath(); ctx.fill();
      ctx.fillStyle = rgb(p.key, 0.5); ctx.fillRect(-150, -30, 240, 6);
    },
    book: function (ctx, p) {
      ctx.fillStyle = 'rgba(0,0,0,0.95)'; ctx.fillRect(-160, -110, 320, 220);
      ctx.fillStyle = rgb(p.key, 0.85); ctx.fillRect(-140, -90, 280, 180);
      ctx.fillStyle = 'rgba(0,0,0,0.9)'; ctx.fillRect(-8, -90, 16, 180);
      ctx.fillStyle = rgb(p.accent, 0.6);
      for (var i = 0; i < 5; i++) ctx.fillRect(-120, -60 + i * 26, 90, 6);
    },
    bottle: function (ctx, p) {
      ctx.fillStyle = rgb(p.accent, 0.75);
      ctx.beginPath();
      ctx.moveTo(-30, -140); ctx.lineTo(30, -140); ctx.lineTo(30, -60);
      ctx.quadraticCurveTo(70, -20, 70, 40); ctx.lineTo(70, 120); ctx.lineTo(-70, 120);
      ctx.lineTo(-70, 40); ctx.quadraticCurveTo(-70, -20, -30, -60);
      ctx.closePath(); ctx.fill();
      ctx.fillStyle = rgb(p.key, 0.4); ctx.fillRect(-50, 20, 14, 80);
    },
    box: function (ctx, p) {
      ctx.fillStyle = 'rgba(0,0,0,0.95)'; ctx.fillRect(-150, -70, 300, 170);
      ctx.fillStyle = rgb(p.key, 0.7); ctx.fillRect(-150, -100, 300, 34);
      ctx.fillStyle = rgb(p.accent, 0.85); ctx.fillRect(-20, -100, 40, 200);
    },
    tape: function (ctx, p) {
      ctx.fillStyle = 'rgba(0,0,0,0.95)'; ctx.fillRect(-160, -100, 320, 200);
      ctx.fillStyle = rgb(p.key, 0.85); ctx.fillRect(-130, -70, 260, 90);
      ctx.fillStyle = 'rgba(0,0,0,0.9)';
      ctx.beginPath(); ctx.arc(-60, -26, 34, 0, Math.PI * 2); ctx.fill();
      ctx.beginPath(); ctx.arc(60, -26, 34, 0, Math.PI * 2); ctx.fill();
      ctx.fillStyle = rgb(p.accent, 0.8); ctx.fillRect(-120, 40, 240, 12);
    }
  };

  var GLYPH_FOR = [
    [/radio|transmitter|walkie|signal|recording|answering/, 'radio'],
    [/phone|laptop|computer|drive|monitor|screen/, 'phone'],
    [/letter|note|telegram|postcard|essay|manuscript|papers|contract|deed|will|receipt|chart|blueprint|map|file/, 'letter'],
    [/photo|photograph|painting|canvas|sonogram|picture/, 'photograph'],
    [/key|badge|star|ring|locket|necklace|coin|watch/, 'key'],
    [/gun|rifle|knife|blade|sword|weapon|crowbar|wrench/, 'gun'],
    [/book|diary|journal|notebook|bible/, 'book'],
    [/bottle|jar|canteen|cup|urn|flask/, 'bottle'],
    [/tape|cassette|video|reel|film/, 'tape']
  ];

  function glyphFor(object) {
    var name = String(object || '').toLowerCase();
    for (var i = 0; i < GLYPH_FOR.length; i++) {
      if (GLYPH_FOR[i][0].test(name)) return GLYPHS[GLYPH_FOR[i][1]];
    }
    return GLYPHS.box;
  }

  var API = {
    palette: palette,
    SETS: SETS,
    GLYPHS: GLYPHS,
    glyphFor: glyphFor,
    drawFigure: drawFigure,
    noise: noise,
    rgb: rgb,
    mix: mix
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmArt = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
