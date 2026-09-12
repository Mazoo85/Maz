/*
 * CODA PICS — the painter.
 * ------------------------
 * Draws the scene the reader built. Everything on the canvas is drawn in code:
 * no photographs, no model weights, no network. That is the whole point — the
 * app works on a plane, on a phone, forever, and the same words always give
 * back the same picture.
 *
 *   CodaPaint.render(ctx, width, height, spec)
 *
 * The order below is the order a painter would work in: sky, light, the far
 * distance, the middle, the subject, then the ground at your feet, then the
 * weather over all of it. js/finish.js does the style treatment afterwards.
 *
 * Colour lives in HSL so a prompt's colour word ("a red dragon") can bend the
 * whole picture towards one hue without flattening it — see makePalette.
 *
 * Exposed as window.CodaPaint (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  var PROMPT = root.CodaPrompt ||
    (typeof require !== 'undefined' ? require('./prompt.js') : null);
  var SUBJECTS = root.CodaSubjects ||
    (typeof require !== 'undefined' ? require('./subjects.js') : null);

  function clamp(v, lo, hi) { return v < lo ? lo : v > hi ? hi : v; }
  function lerp(a, b, t) { return a + (b - a) * t; }

  /* ---------------------------------------------------------------- colour */

  /* Sky by hour: three stops from the top of the frame down to the horizon,
   * plus where the sun or moon sits and what haze it throws. */
  var SKY = {
    dawn:  { top: [248, 48, 24], mid: [18, 70, 56],  low: [34, 92, 74], light: [38, 100, 76], lightY: 0.80, haze: [30, 78, 70] },
    day:   { top: [212, 74, 46], mid: [203, 62, 70], low: [196, 58, 86], light: [48, 100, 90], lightY: 0.20, haze: [200, 48, 86] },
    dusk:  { top: [266, 54, 20], mid: [332, 58, 44], low: [22, 92, 62], light: [16, 96, 64],  lightY: 0.84, haze: [20, 76, 60] },
    night: { top: [240, 62, 6],  mid: [232, 56, 13], low: [220, 46, 21], light: [210, 28, 94], lightY: 0.26, haze: [225, 44, 24] }
  };

  /* Each setting pulls the sky and the land its own way. `ink` is what a
   * silhouette is made of, `land` the lit ground, `far` the distance. */
  var SCENE_COLOUR = {
    mountains: { ink: [228, 34, 16], land: [210, 22, 42], far: [216, 30, 54] },
    forest:    { ink: [150, 40, 10], land: [128, 38, 26], far: [160, 26, 44] },
    jungle:    { ink: [140, 46, 9],  land: [120, 44, 24], far: [150, 32, 42] },
    ocean:     { ink: [212, 52, 14], land: [203, 62, 34], far: [205, 44, 52], sea: [203, 64, 30], water: true },
    shore:     { ink: [214, 40, 16], land: [38, 42, 66],  far: [205, 36, 56], sea: [196, 58, 40], water: true },
    lake:      { ink: [218, 40, 14], land: [200, 44, 34], far: [212, 30, 50], sea: [206, 48, 28], water: true },
    desert:    { ink: [26, 40, 20],  land: [34, 62, 62],  far: [24, 40, 52] },
    city:      { ink: [240, 40, 10], land: [235, 18, 20], far: [230, 30, 34] },
    space:     { ink: [262, 50, 8],  land: [258, 40, 18], far: [270, 44, 26] },
    snow:      { ink: [216, 28, 26], land: [208, 24, 88], far: [210, 26, 72] },
    canyon:    { ink: [16, 42, 18],  land: [20, 56, 48],  far: [14, 42, 44] },
    swamp:     { ink: [128, 30, 8],  land: [110, 26, 22], far: [140, 20, 36], sea: [128, 28, 18], water: true },
    meadow:    { ink: [110, 34, 16], land: [96, 44, 44],  far: [120, 28, 52] },
    plains:    { ink: [64, 34, 18],  land: [58, 44, 50],  far: [70, 30, 56] },
    volcano:   { ink: [8, 40, 8],    land: [12, 34, 20],  far: [6, 44, 30] },
    ruins:     { ink: [40, 20, 14],  land: [44, 22, 38],  far: [40, 18, 46] },
    cave:      { ink: [268, 34, 6],  land: [264, 26, 16], far: [270, 30, 22] },
    island:    { ink: [168, 44, 14], land: [46, 52, 70],  far: [180, 36, 50], sea: [186, 62, 40], water: true },
    road:      { ink: [232, 28, 10], land: [228, 10, 24], far: [224, 24, 38] },
    sky:       { ink: [220, 30, 30], land: [210, 40, 80], far: [210, 30, 70] }
  };

  /*
   * The palette a whole picture is mixed from. A colour word rotates every hue
   * towards one target and lifts saturation, which is what makes "a red dragon
   * over the sea" read red without turning the sea into a flat red block.
   */
  function makePalette(spec) {
    var sky = SKY[spec.time] || SKY.dusk;
    var scene = SCENE_COLOUR[spec.scene.id] || SCENE_COLOUR.plains;
    var tint = spec.palette;
    var drama = spec.mood;

    function bend(c) {
      var h = c[0], s = c[1], l = c[2];
      if (tint) {
        var d = ((tint.hue - h) % 360 + 540) % 360 - 180;   // shortest way round
        h = (h + d * 0.55 + 360) % 360;
        s = clamp(s * tint.sat, 0, 100);
        l = clamp(l * (0.94 + (tint.warm - 1) * 0.22), 0, 100);
      }
      return [h, s, l];
    }

    function css(c, a) {
      var b = bend(c);
      return 'hsla(' + b[0].toFixed(1) + ',' + b[1].toFixed(1) + '%,' +
        b[2].toFixed(1) + '%,' + (a == null ? 1 : a) + ')';
    }

    /* Atmospheric perspective: the further away a thing is, the more of the
     * sky is mixed into it. `depth` 0 = at your feet, 1 = on the horizon. */
    function depthMix(c, depth) {
      var air = sky.haze;
      var t = clamp(depth, 0, 1) * (0.62 - drama * 0.22);
      /* Hue moves the short way round the wheel and only a little: distance
       * drains a colour and lifts it towards the sky, it does not turn blue
       * mountains green on the way past. */
      var dh = ((air[0] - c[0]) % 360 + 540) % 360 - 180;
      return [
        (c[0] + dh * t * 0.28 + 360) % 360,
        lerp(c[1], air[1], t * 0.85),
        lerp(c[2], air[2], t)
      ];
    }

    return {
      sky: sky,
      scene: scene,
      drama: drama,
      isWater: !!scene.water,
      css: css,
      bend: bend,
      /* A silhouette at `depth`, optionally lightened towards the land tone. */
      ink: function (depth, a) { return css(depthMix(scene.ink, depth), a); },
      /* What the *subject* is cut from. Darker than the land it stands on, so
       * a fox against a treeline is still a fox and not a smudge — this one
       * difference is most of what makes the pictures read. */
      silhouette: function (depth, a) {
        var c = depthMix(scene.ink, depth);
        return css([c[0], c[1] * 0.92, c[2] * (0.62 - drama * 0.10)], a);
      },
      land: function (depth, a) { return css(depthMix(scene.land, depth), a); },
      far: function (depth, a) { return css(depthMix(scene.far, depth), a); },
      light: function (a) { return css(sky.light, a); },
      /* Open water, which is not the same colour as the land beside it. */
      sea: function (depth, a) { return css(depthMix(scene.sea || scene.far, depth), a); },
      haze: function (a) { return css(sky.haze, a); },
      shade: function (c, dl, a) { return css([c[0], c[1], clamp(c[2] + dl, 0, 100)], a); }
    };
  }

  /* ------------------------------------------------------------- primitives */

  /* A jagged ridge line, built by repeatedly splitting the segment and pushing
   * the midpoint — the cheapest way to get a mountain that never repeats. */
  function ridge(x0, y0, x1, y1, rough, r, depth) {
    var pts = [[x0, y0], [x1, y1]];
    for (var pass = 0; pass < (depth || 5); pass++) {
      var next = [pts[0]];
      var amp = rough / Math.pow(1.55, pass);
      for (var i = 1; i < pts.length; i++) {
        var a = pts[i - 1], b = pts[i];
        next.push([(a[0] + b[0]) / 2, (a[1] + b[1]) / 2 + (r() - 0.5) * amp]);
        next.push(b);
      }
      pts = next;
    }
    return pts;
  }

  function fillPoly(ctx, pts, closeY, style) {
    ctx.beginPath();
    ctx.moveTo(pts[0][0], pts[0][1]);
    for (var i = 1; i < pts.length; i++) ctx.lineTo(pts[i][0], pts[i][1]);
    ctx.lineTo(pts[pts.length - 1][0], closeY);
    ctx.lineTo(pts[0][0], closeY);
    ctx.closePath();
    ctx.fillStyle = style;
    ctx.fill();
  }

  /* Soft rolling ground — hills, dunes, swells — as one wavy band. */
  function hills(ctx, w, baseY, amp, freq, phase, closeY, style) {
    ctx.beginPath();
    ctx.moveTo(0, baseY);
    for (var x = 0; x <= w; x += Math.max(2, w / 220)) {
      var t = x / w * freq * Math.PI * 2 + phase;
      var y = baseY + Math.sin(t) * amp + Math.sin(t * 2.3 + 1.7) * amp * 0.35;
      ctx.lineTo(x, y);
    }
    ctx.lineTo(w, closeY);
    ctx.lineTo(0, closeY);
    ctx.closePath();
    ctx.fillStyle = style;
    ctx.fill();
  }

  function pine(ctx, x, baseY, h, style) {
    var wd = h * 0.36;
    ctx.fillStyle = style;
    ctx.beginPath();
    ctx.moveTo(x, baseY - h);
    ctx.lineTo(x + wd * 0.55, baseY - h * 0.42);
    ctx.lineTo(x + wd * 0.3, baseY - h * 0.44);
    ctx.lineTo(x + wd * 0.8, baseY - h * 0.06);
    ctx.lineTo(x - wd * 0.8, baseY - h * 0.06);
    ctx.lineTo(x - wd * 0.3, baseY - h * 0.44);
    ctx.lineTo(x - wd * 0.55, baseY - h * 0.42);
    ctx.closePath();
    ctx.fill();
    ctx.fillRect(x - h * 0.03, baseY - h * 0.1, h * 0.06, h * 0.12);
  }

  function blob(ctx, x, y, rx, ry, lumps, r, style) {
    ctx.beginPath();
    for (var i = 0; i <= lumps; i++) {
      var a = (i / lumps) * Math.PI * 2;
      var k = 0.78 + r() * 0.44;
      var px = x + Math.cos(a) * rx * k;
      var py = y + Math.sin(a) * ry * k;
      if (i === 0) ctx.moveTo(px, py);
      else ctx.lineTo(px, py);
    }
    ctx.closePath();
    ctx.fillStyle = style;
    ctx.fill();
  }

  /* ------------------------------------------------------------------- sky */

  function paintSky(ctx, w, h, horizon, P, spec, r) {
    var g = ctx.createLinearGradient(0, 0, 0, Math.max(horizon, h * 0.55));
    g.addColorStop(0, P.css(P.sky.top));
    g.addColorStop(0.55, P.css(P.sky.mid));
    g.addColorStop(1, P.css(P.sky.low));
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, w, h);

    if (spec.scene.id === 'space') {
      ctx.fillStyle = P.css([258, 60, 4]);
      ctx.fillRect(0, 0, w, h);
      nebula(ctx, w, h, P, r);
    }
    if (spec.time === 'night' || spec.scene.id === 'space') stars(ctx, w, h, horizon, P, spec, r);
    if (spec.weather === 'aurora') aurora(ctx, w, h, horizon, P, r);
  }

  function stars(ctx, w, h, horizon, P, spec, r) {
    var count = spec.scene.id === 'space' ? 420 : 220;
    var top = spec.scene.id === 'space' ? h : Math.min(horizon, h);
    for (var i = 0; i < count; i++) {
      var x = r() * w, y = r() * top;
      var s = r();
      var rad = s > 0.96 ? 1.9 : s > 0.8 ? 1.2 : 0.7;
      ctx.globalAlpha = 0.25 + r() * 0.75;
      ctx.fillStyle = P.css([lerp(200, 50, r()), 30, 96]);
      ctx.beginPath();
      ctx.arc(x, y, rad, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.globalAlpha = 1;
  }

  function nebula(ctx, w, h, P, r) {
    for (var i = 0; i < 5; i++) {
      var x = r() * w, y = r() * h * 0.9;
      var rad = (0.25 + r() * 0.4) * Math.max(w, h);
      var g = ctx.createRadialGradient(x, y, 0, x, y, rad);
      var hue = [286, 214, 330, 190, 258][i];
      g.addColorStop(0, P.css([hue, 70, 46], 0.30));
      g.addColorStop(0.5, P.css([hue, 64, 32], 0.14));
      g.addColorStop(1, P.css([hue, 60, 20], 0));
      ctx.fillStyle = g;
      ctx.fillRect(0, 0, w, h);
    }
  }

  function aurora(ctx, w, h, horizon, P, r) {
    ctx.globalCompositeOperation = 'lighter';
    for (var b = 0; b < 4; b++) {
      var baseY = horizon * (0.12 + b * 0.11);
      var hue = [150, 172, 196, 128][b];
      ctx.beginPath();
      ctx.moveTo(0, baseY);
      for (var x = 0; x <= w; x += w / 40) {
        ctx.lineTo(x, baseY + Math.sin(x / w * 6 + b * 1.4) * h * 0.05);
      }
      ctx.lineTo(w, baseY + h * 0.22);
      for (var x2 = w; x2 >= 0; x2 -= w / 40) {
        ctx.lineTo(x2, baseY + h * 0.2 + Math.sin(x2 / w * 5 + b) * h * 0.04);
      }
      ctx.closePath();
      var g = ctx.createLinearGradient(0, baseY, 0, baseY + h * 0.24);
      g.addColorStop(0, P.css([hue, 90, 62], 0.42));
      g.addColorStop(1, P.css([hue, 90, 52], 0));
      ctx.fillStyle = g;
      ctx.fill();
    }
    ctx.globalCompositeOperation = 'source-over';
  }

  /* The sun or the moon, with the glow it throws into the sky around it. */
  function paintLight(ctx, w, h, horizon, P, spec, r) {
    if (spec.scene.id === 'cave') return null;
    var x = w * (0.2 + r() * 0.6);
    var y = h * P.sky.lightY * (spec.scene.id === 'space' ? 0.4 : 1);
    var rad = Math.min(w, h) * (spec.time === 'night' ? 0.055 : 0.075);
    if (spec.scene.id === 'space') rad *= 0.7;

    var glow = ctx.createRadialGradient(x, y, 0, x, y, rad * 9);
    glow.addColorStop(0, P.css(P.sky.light, 0.55));
    glow.addColorStop(0.25, P.css(P.sky.light, 0.16));
    glow.addColorStop(1, P.css(P.sky.light, 0));
    ctx.fillStyle = glow;
    ctx.fillRect(0, 0, w, h);

    ctx.beginPath();
    ctx.arc(x, y, rad, 0, Math.PI * 2);
    ctx.fillStyle = P.light(spec.time === 'night' ? 0.92 : 1);
    ctx.fill();

    if (spec.time === 'night') {           // craters, so a moon reads as a moon
      for (var i = 0; i < 6; i++) {
        var a = r() * Math.PI * 2, d = r() * rad * 0.65;
        ctx.globalAlpha = 0.16;
        ctx.beginPath();
        ctx.arc(x + Math.cos(a) * d, y + Math.sin(a) * d, rad * (0.08 + r() * 0.16), 0, Math.PI * 2);
        ctx.fillStyle = P.css([225, 30, 40]);
        ctx.fill();
        ctx.globalAlpha = 1;
      }
    }
    return { x: x, y: y, r: rad };
  }

  function clouds(ctx, w, h, horizon, P, spec, r) {
    if (spec.scene.id === 'space' || spec.scene.id === 'cave') return;
    var heavy = spec.weather === 'storm' || spec.weather === 'clouds' || spec.weather === 'rain';
    var n = heavy ? 9 : spec.weather === 'clear' ? 3 : 5;
    for (var i = 0; i < n; i++) {
      var y = horizon * (0.12 + r() * 0.62);
      var x = r() * w;
      var scale = (0.5 + r() * 0.9) * (0.9 + (1 - y / horizon) * 0.5);
      var rx = w * 0.16 * scale, ry = h * 0.028 * scale;
      var depth = clamp(y / Math.max(horizon, 1), 0, 1);
      var light = heavy ? 0.30 : 0.55;
      ctx.globalAlpha = heavy ? 0.85 : 0.5;
      blob(ctx, x, y, rx, ry * 2.2, 11, r,
        P.css([P.sky.haze[0], P.sky.haze[1] * 0.8, P.sky.haze[2] * (heavy ? 0.55 : 1.05)], light + 0.25));
      ctx.globalAlpha = heavy ? 0.5 : 0.34;
      blob(ctx, x + rx * 0.2, y - ry * 0.9, rx * 0.7, ry * 1.5, 9, r, P.light(heavy ? 0.16 : 0.4));
      ctx.globalAlpha = 1;
      void depth;
    }
  }

  /* ------------------------------------------------------------- the ground
   * One function per setting. Each gets the frame, where the horizon is, and
   * its own generator, and is free to draw as far forward as it likes.
   */
  var GROUND = {};

  /* Lit upper faces for a ridge: a wash that fades out downwards, plus a bright
   * line along the crest. Clipped to the ridge, so it can never sit in the air. */
  function crestLight(ctx, pts, w, h, P, strength, tone) {
    ctx.save();
    ctx.beginPath();
    ctx.moveTo(pts[0][0], pts[0][1]);
    for (var i = 1; i < pts.length; i++) ctx.lineTo(pts[i][0], pts[i][1]);
    ctx.lineTo(pts[pts.length - 1][0], h);
    ctx.lineTo(pts[0][0], h);
    ctx.closePath();
    ctx.clip();
    var top = h, bottom = 0;
    pts.forEach(function (p) { top = Math.min(top, p[1]); bottom = Math.max(bottom, p[1]); });
    var g = ctx.createLinearGradient(0, top, 0, top + (bottom - top) * 0.55);
    g.addColorStop(0, P.css(tone, strength));
    g.addColorStop(0.45, P.css(tone, strength * 0.35));
    g.addColorStop(1, P.css(tone, 0));
    ctx.fillStyle = g;
    ctx.fillRect(0, top, w, h - top);
    ctx.restore();
    ctx.save();                                    // the crest itself, catching light
    ctx.globalAlpha = strength * 0.30;
    ctx.strokeStyle = P.css(tone);
    ctx.lineWidth = Math.max(1, h * 0.002);
    ctx.beginPath();
    ctx.moveTo(pts[0][0], pts[0][1]);
    for (var k = 1; k < pts.length; k++) ctx.lineTo(pts[k][0], pts[k][1]);
    ctx.stroke();
    ctx.restore();
  }

  GROUND.mountains = function (ctx, w, h, hz, P, spec, r) {
    for (var layer = 0; layer < 3; layer++) {
      var depth = 1 - layer * 0.42;
      var baseY = hz + (h - hz) * layer * 0.30;
      var peak = (h * 0.44) * (1 - layer * 0.24);
      var pts = ridge(-w * 0.05, baseY - peak * (0.4 + r() * 0.5), w * 1.05,
        baseY - peak * (0.4 + r() * 0.5), peak * 1.1, r, 7);
      fillPoly(ctx, pts, h, P.ink(depth));
      crestLight(ctx, pts, w, h, P, 0.26 - layer * 0.07, [P.sky.haze[0], 22, 90]);
    }
    hills(ctx, w, h * 0.93, h * 0.02, 1.4, r() * 6, h, P.land(0));
  };

  GROUND.snow = function (ctx, w, h, hz, P, spec, r) {
    /* Dark rock first, pale snow last: without that ramp a white landscape
     * dissolves into a white sky. */
    for (var layer = 0; layer < 3; layer++) {
      var baseY = hz + (h - hz) * layer * 0.26;
      var peak = h * 0.40 * (1 - layer * 0.20);
      var pts = ridge(-w * 0.05, baseY - peak * 0.6, w * 1.05, baseY - peak * 0.85, peak * 0.95, r, 7);
      fillPoly(ctx, pts, h, P.ink(0.85 - layer * 0.38));
      crestLight(ctx, pts, w, h, P, 0.40 - layer * 0.10, [P.sky.haze[0], 16, 94]);
    }
    hills(ctx, w, hz + (h - hz) * 0.62, h * 0.025, 1.1, r() * 6, h, P.css([208, 22, 86]));
    hills(ctx, w, hz + (h - hz) * 0.86, h * 0.02, 1.7, r() * 6, h, P.css([205, 16, 96]));
  };

  GROUND.forest = function (ctx, w, h, hz, P, spec, r) {
    hills(ctx, w, hz, h * 0.03, 1.2, r() * 6, h, P.far(1));
    for (var layer = 0; layer < 3; layer++) {
      var y = hz + (h - hz) * (0.12 + layer * 0.26);
      var depth = 0.8 - layer * 0.35;
      var th = h * (0.10 + layer * 0.06);
      ctx.fillStyle = P.ink(depth);
      ctx.fillRect(0, y, w, h - y);
      for (var x = -w * 0.02; x < w * 1.02; x += th * 0.28) {
        pine(ctx, x + (r() - 0.5) * th * 0.2, y + th * 0.1, th * (0.75 + r() * 0.5), P.ink(depth));
      }
    }
  };

  GROUND.jungle = function (ctx, w, h, hz, P, spec, r) {
    hills(ctx, w, hz, h * 0.04, 1.1, r() * 6, h, P.far(1));
    for (var layer = 0; layer < 2; layer++) {
      var y = hz + (h - hz) * (0.2 + layer * 0.3);
      ctx.fillStyle = P.ink(0.7 - layer * 0.4);
      ctx.fillRect(0, y, w, h - y);
      for (var i = 0; i < 26; i++) {
        blob(ctx, r() * w, y + (r() - 0.4) * h * 0.06, w * 0.05, h * 0.035, 9, r, P.ink(0.7 - layer * 0.4));
      }
    }
    for (var v = 0; v < 14; v++) {           // vines down from the top of frame
      var vx = r() * w, len = h * (0.08 + r() * 0.26);
      ctx.strokeStyle = P.ink(0.15, 0.9);
      ctx.lineWidth = Math.max(1, w * 0.004);
      ctx.beginPath();
      ctx.moveTo(vx, 0);
      ctx.quadraticCurveTo(vx + (r() - 0.5) * w * 0.05, len * 0.6, vx + (r() - 0.5) * w * 0.03, len);
      ctx.stroke();
    }
  };

  function water(ctx, w, h, top, P, spec, r, light) {
    var g = ctx.createLinearGradient(0, top, 0, h);
    g.addColorStop(0, P.sea(0.9));
    g.addColorStop(1, P.sea(0));
    ctx.fillStyle = g;
    ctx.fillRect(0, top, w, h - top);
    if (light) {                              // the light's glitter on the water
      ctx.save();
      ctx.globalCompositeOperation = 'lighter';   // light on water adds, never greys
      var bands = 26;
      for (var g2 = 0; g2 < bands; g2++) {
        var t2 = g2 / bands;
        var y2 = top + (h - top) * Math.pow(t2, 1.5);
        var half = light.r * 0.5 + (w * 0.07 - light.r * 0.5) * t2;
        var jitter = (r() - 0.5) * half * 0.8;
        ctx.globalAlpha = (0.30 - t2 * 0.22) * (0.5 + r() * 0.7);
        ctx.fillStyle = P.light(1);
        ctx.fillRect(light.x - half + jitter, y2, half * 2 * (0.4 + r() * 0.6),
          Math.max(1, (h - top) * 0.012));
      }
      ctx.restore();
      ctx.globalAlpha = 1;
    }
    for (var i = 0; i < 24; i++) {            // wave dashes, wider as they near
      var t = Math.pow(r(), 0.6);
      var y = top + (h - top) * t;
      var len = w * (0.02 + t * 0.08) * (0.5 + r());
      ctx.globalAlpha = 0.08 + t * 0.22;
      ctx.strokeStyle = P.css([P.sky.haze[0], 30, 92]);
      ctx.lineWidth = Math.max(1, (h - top) * 0.006 * (0.4 + t));
      ctx.beginPath();
      ctx.moveTo(r() * w, y);
      ctx.lineTo(r() * w + len, y);
      ctx.stroke();
    }
    ctx.globalAlpha = 1;
  }

  GROUND.ocean = function (ctx, w, h, hz, P, spec, r, light) {
    water(ctx, w, h, hz, P, spec, r, light);
  };

  GROUND.lake = function (ctx, w, h, hz, P, spec, r, light) {
    var pts = ridge(-w * 0.05, hz - h * 0.18, w * 1.05, hz - h * 0.12, h * 0.3, r, 5);
    fillPoly(ctx, pts, hz, P.ink(0.85));
    water(ctx, w, h, hz, P, spec, r, light);
    ctx.save();                                // the hills again, upside down
    ctx.globalAlpha = 0.3;
    ctx.translate(0, hz * 2);
    ctx.scale(1, -1);
    fillPoly(ctx, pts, hz, P.ink(0.4));
    ctx.restore();
    ctx.globalAlpha = 1;
  };

  GROUND.shore = function (ctx, w, h, hz, P, spec, r, light) {
    water(ctx, w, h, hz, P, spec, r, light);
    var sandTop = hz + (h - hz) * 0.62;
    ctx.beginPath();
    ctx.moveTo(0, sandTop + h * 0.05);
    for (var x = 0; x <= w; x += w / 30) {
      ctx.lineTo(x, sandTop + Math.sin(x / w * 5 + r() * 0.2) * h * 0.02);
    }
    ctx.lineTo(w, h);
    ctx.lineTo(0, h);
    ctx.closePath();
    ctx.fillStyle = P.land(0);
    ctx.fill();
    for (var i = 0; i < 3; i++) {             // a few rocks, on the sand only
      var rx = r() * w;
      var ry = sandTop + h * 0.04 + r() * (h - sandTop) * 0.5;
      blob(ctx, rx, ry, w * 0.032, h * 0.020, 9, r, P.silhouette(0.1));
    }
  };

  GROUND.island = function (ctx, w, h, hz, P, spec, r, light) {
    water(ctx, w, h, hz, P, spec, r, light);
    var cx = w * (0.3 + r() * 0.4), iw = w * 0.30, iy = hz + (h - hz) * 0.22;
    ctx.beginPath();
    ctx.moveTo(cx - iw, iy);
    ctx.quadraticCurveTo(cx, iy - h * 0.10, cx + iw, iy);
    ctx.quadraticCurveTo(cx, iy + h * 0.05, cx - iw, iy);
    ctx.closePath();
    ctx.fillStyle = P.land(0.4);
    ctx.fill();
  };

  GROUND.desert = function (ctx, w, h, hz, P, spec, r) {
    var mesaY = hz - h * 0.02;               // a flat-topped butte in the haze
    for (var m = 0; m < 3; m++) {
      var mx = r() * w, mw = w * (0.08 + r() * 0.12), mh = h * (0.05 + r() * 0.08);
      ctx.fillStyle = P.far(0.9);
      ctx.beginPath();
      ctx.moveTo(mx - mw, mesaY);
      ctx.lineTo(mx - mw * 0.8, mesaY - mh);
      ctx.lineTo(mx + mw * 0.75, mesaY - mh);
      ctx.lineTo(mx + mw, mesaY);
      ctx.closePath();
      ctx.fill();
    }
    for (var d = 0; d < 4; d++) {
      var duneY = hz + (h - hz) * (0.08 + d * 0.24);
      var amp = h * (0.012 + d * 0.012), freq = 0.7 + d * 0.3, phase = r() * 6;
      hills(ctx, w, duneY, amp, freq, phase, h, P.land(0.7 - d * 0.22));
      ctx.globalAlpha = 0.20 - d * 0.04;          // sun catching the crest
      ctx.strokeStyle = P.light(0.9);
      ctx.lineWidth = Math.max(1, h * 0.004);
      ctx.beginPath();
      for (var x = 0; x <= w; x += w / 120) {
        var t = x / w * freq * Math.PI * 2 + phase;
        var y = duneY + Math.sin(t) * amp + Math.sin(t * 2.3 + 1.7) * amp * 0.35;
        if (x === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
      }
      ctx.stroke();
      ctx.globalAlpha = 1;
    }
  };

  GROUND.canyon = function (ctx, w, h, hz, P, spec, r) {
    /* Walls stepping in from both edges towards a floor you can see between
     * them — a gorge is the gap, not a shape drawn in the middle of one. */
    hills(ctx, w, hz, h * 0.02, 1.1, r() * 6, h, P.far(1));
    var floorY = hz + (h - hz) * 0.58;
    ctx.fillStyle = P.land(0.45);
    ctx.fillRect(0, floorY, w, h - floorY);
    ctx.fillStyle = P.css([P.sky.haze[0], 42, 44], 0.75);   // the river down there
    ctx.beginPath();
    ctx.moveTo(w * 0.44, floorY);
    ctx.lineTo(w * 0.56, floorY);
    ctx.lineTo(w * 0.70, h);
    ctx.lineTo(w * 0.30, h);
    ctx.closePath();
    ctx.fill();

    for (var layer = 0; layer < 3; layer++) {
      var depth = 0.88 - layer * 0.34;
      var top = hz - h * (0.02 + layer * 0.07);
      var inner = w * (0.16 + layer * 0.11);
      [-1, 1].forEach(function (side) {
        var edge = side < 0 ? 0 : w;
        var reach = side < 0 ? inner : w - inner;
        var foot = side < 0 ? inner * 0.62 : w - inner * 0.62;
        ctx.fillStyle = P.ink(depth);
        ctx.beginPath();
        ctx.moveTo(edge, top);
        ctx.lineTo(reach, top + h * (0.05 + layer * 0.02));
        ctx.lineTo(foot, h);
        ctx.lineTo(edge, h);
        ctx.closePath();
        ctx.fill();
        ctx.save();                                   // strata across the wall
        ctx.clip();
        ctx.globalAlpha = 0.12;
        for (var b = 0; b < 7; b++) {
          ctx.fillStyle = P.css([18, 52, b % 2 ? 72 : 24]);
          ctx.fillRect(0, top + (h - top) * (b / 7), w, (h - top) * 0.035);
        }
        ctx.restore();
        ctx.globalAlpha = 1;
      });
    }
  };

  GROUND.swamp = function (ctx, w, h, hz, P, spec, r, light) {
    hills(ctx, w, hz, h * 0.02, 1.3, r() * 6, h, P.far(1));
    water(ctx, w, h, hz + h * 0.02, P, spec, r, light);
    for (var i = 0; i < 12; i++) {             // dead trees, bare and crooked
      var x = r() * w, th = h * (0.10 + r() * 0.22);
      var base = hz + (h - hz) * (0.1 + r() * 0.6);
      ctx.strokeStyle = P.ink(0.2);
      ctx.lineWidth = Math.max(1.4, w * 0.006);
      ctx.beginPath();
      ctx.moveTo(x, base);
      ctx.quadraticCurveTo(x + (r() - 0.5) * w * 0.03, base - th * 0.6, x + (r() - 0.5) * w * 0.05, base - th);
      ctx.stroke();
      for (var b = 0; b < 3; b++) {
        ctx.beginPath();
        ctx.moveTo(x, base - th * (0.5 + b * 0.16));
        ctx.lineTo(x + (r() - 0.5) * w * 0.06, base - th * (0.62 + b * 0.16));
        ctx.stroke();
      }
    }
  };

  GROUND.meadow = function (ctx, w, h, hz, P, spec, r) {
    hills(ctx, w, hz, h * 0.035, 1.0, r() * 6, h, P.far(1));
    hills(ctx, w, hz + (h - hz) * 0.3, h * 0.03, 1.6, r() * 6, h, P.land(0.5));
    hills(ctx, w, hz + (h - hz) * 0.62, h * 0.025, 2.2, r() * 6, h, P.land(0.1));
    for (var i = 0; i < 90; i++) {             // grass and flower heads up front
      var t = Math.pow(r(), 0.5);
      var y = hz + (h - hz) * (0.5 + t * 0.5);
      var x = r() * w, gh = (h - hz) * 0.05 * (0.4 + t);
      ctx.strokeStyle = P.ink(0.1, 0.7);
      ctx.lineWidth = Math.max(1, w * 0.002);
      ctx.beginPath();
      ctx.moveTo(x, y);
      ctx.quadraticCurveTo(x + (r() - 0.5) * gh, y - gh * 0.6, x + (r() - 0.5) * gh * 1.6, y - gh);
      ctx.stroke();
      if (r() > 0.82) {
        ctx.beginPath();
        ctx.arc(x + (r() - 0.5) * gh * 1.6, y - gh, Math.max(1.2, gh * 0.12), 0, Math.PI * 2);
        ctx.fillStyle = P.css([[350, 48, 280, 24][Math.floor(r() * 4) % 4], 80, 68]);
        ctx.fill();
      }
    }
  };

  GROUND.plains = function (ctx, w, h, hz, P, spec, r) {
    hills(ctx, w, hz, h * 0.025, 0.8, r() * 6, h, P.far(1));
    hills(ctx, w, hz + (h - hz) * 0.35, h * 0.03, 1.3, r() * 6, h, P.land(0.45));
    hills(ctx, w, hz + (h - hz) * 0.7, h * 0.02, 2.0, r() * 6, h, P.land(0));
  };

  GROUND.volcano = function (ctx, w, h, hz, P, spec, r) {
    var cx = w * (0.3 + r() * 0.4), base = hz + (h - hz) * 0.35, ch = h * 0.34;
    ctx.fillStyle = P.far(0.9);
    ctx.beginPath();
    ctx.moveTo(cx - w * 0.34, base);
    ctx.lineTo(cx - w * 0.05, base - ch);
    ctx.lineTo(cx + w * 0.05, base - ch);
    ctx.lineTo(cx + w * 0.34, base);
    ctx.closePath();
    ctx.fill();
    for (var i = 0; i < 5; i++) {               // lava running down the cone
      ctx.strokeStyle = P.css([r() > 0.5 ? 14 : 32, 95, 58], 0.9);
      ctx.lineWidth = Math.max(1.5, w * 0.005);
      ctx.beginPath();
      ctx.moveTo(cx + (r() - 0.5) * w * 0.06, base - ch);
      ctx.quadraticCurveTo(cx + (r() - 0.5) * w * 0.2, base - ch * 0.5, cx + (r() - 0.5) * w * 0.5, base);
      ctx.stroke();
    }
    var g = ctx.createRadialGradient(cx, base - ch, 0, cx, base - ch, h * 0.3);
    g.addColorStop(0, P.css([20, 100, 60], 0.8));
    g.addColorStop(1, P.css([12, 100, 50], 0));
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, w, h);
    ctx.fillStyle = P.ink(0.1);
    ctx.fillRect(0, base, w, h - base);
    for (var c = 0; c < 30; c++) {              // cracks glowing in the crust
      ctx.strokeStyle = P.css([18, 96, 56], 0.5);
      ctx.lineWidth = Math.max(1, w * 0.002);
      var x0 = r() * w, y0 = base + r() * (h - base);
      ctx.beginPath();
      ctx.moveTo(x0, y0);
      ctx.lineTo(x0 + (r() - 0.5) * w * 0.12, y0 + (r() - 0.5) * h * 0.03);
      ctx.stroke();
    }
  };

  GROUND.ruins = function (ctx, w, h, hz, P, spec, r) {
    hills(ctx, w, hz, h * 0.03, 1.1, r() * 6, h, P.far(1));
    hills(ctx, w, hz + (h - hz) * 0.45, h * 0.02, 1.8, r() * 6, h, P.land(0.2));
    for (var i = 0; i < 7; i++) {               // broken columns, none the same
      var x = w * (0.06 + r() * 0.88);
      var base = hz + (h - hz) * (0.35 + r() * 0.4);
      var ch = (h - hz) * (0.2 + r() * 0.55);
      var cw = ch * 0.16;
      ctx.fillStyle = P.ink(0.25 + r() * 0.3);
      ctx.fillRect(x - cw / 2, base - ch, cw, ch);
      ctx.fillRect(x - cw * 0.8, base - ch - cw * 0.3, cw * 1.6, cw * 0.3);
    }
  };

  GROUND.cave = function (ctx, w, h, hz, P, spec, r) {
    ctx.fillStyle = P.ink(0.2);
    ctx.fillRect(0, 0, w, h);
    var g = ctx.createRadialGradient(w * 0.5, h * 0.45, 0, w * 0.5, h * 0.45, w * 0.55);
    g.addColorStop(0, P.css([P.sky.haze[0], 50, 46], 0.9));
    g.addColorStop(1, P.css([P.sky.haze[0], 50, 6], 1));
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, w, h);
    for (var i = 0; i < 26; i++) {              // stalactites and stalagmites
      var x = r() * w, len = h * (0.06 + r() * 0.2);
      ctx.fillStyle = P.ink(0.05);
      ctx.beginPath();
      ctx.moveTo(x - w * 0.02, 0);
      ctx.lineTo(x + w * 0.02, 0);
      ctx.lineTo(x, len);
      ctx.closePath();
      ctx.fill();
      var x2 = r() * w, len2 = h * (0.04 + r() * 0.14);
      ctx.beginPath();
      ctx.moveTo(x2 - w * 0.025, h);
      ctx.lineTo(x2 + w * 0.025, h);
      ctx.lineTo(x2, h - len2);
      ctx.closePath();
      ctx.fill();
    }
    ctx.fillStyle = P.css([P.sky.haze[0], 60, 30], 0.8);
    ctx.fillRect(0, h * 0.86, w, h * 0.14);
  };

  GROUND.city = function (ctx, w, h, hz, P, spec, r) {
    for (var layer = 0; layer < 3; layer++) {
      var depth = 0.9 - layer * 0.4;
      var baseY = hz + (h - hz) * (layer * 0.22);
      var x = -w * 0.05;
      while (x < w * 1.05) {
        var bw = w * (0.03 + r() * 0.07);
        var bh = (h * (0.10 + r() * 0.30)) * (1 - layer * 0.12);
        ctx.fillStyle = P.ink(depth);
        ctx.fillRect(x, baseY - bh, bw, bh + (h - baseY));
        if (layer < 2 && spec.time !== 'day') {   // lit windows
          for (var wy = baseY - bh + bh * 0.08; wy < baseY - bh * 0.05; wy += bh * 0.09) {
            for (var wx = x + bw * 0.12; wx < x + bw * 0.88; wx += bw * 0.22) {
              if (r() > 0.45) continue;
              ctx.fillStyle = P.css([r() > 0.7 ? 190 : 45, 90, 66], 0.55 + r() * 0.45);
              ctx.fillRect(wx, wy, bw * 0.12, bh * 0.045);
            }
          }
        }
        x += bw * 1.12;
      }
    }
    ctx.fillStyle = P.ink(0);
    ctx.fillRect(0, h * 0.9, w, h * 0.1);
  };

  GROUND.road = function (ctx, w, h, hz, P, spec, r) {
    hills(ctx, w, hz, h * 0.03, 1.0, r() * 6, h, P.far(1));
    ctx.fillStyle = P.land(0.3);
    ctx.fillRect(0, hz, w, h - hz);
    var vx = w * (0.35 + r() * 0.3);
    ctx.fillStyle = P.ink(0.1);                 // the road, running to a point
    ctx.beginPath();
    ctx.moveTo(vx - w * 0.02, hz);
    ctx.lineTo(vx + w * 0.02, hz);
    ctx.lineTo(w * 0.92, h);
    ctx.lineTo(w * 0.08, h);
    ctx.closePath();
    ctx.fill();
    for (var i = 0; i < 9; i++) {               // centre line, foreshortened
      var t = i / 9, t2 = t + 0.055;
      ctx.fillStyle = P.css([48, 80, 70], 0.7);
      ctx.beginPath();
      ctx.moveTo(vx - w * 0.004 * (1 + t * 8), hz + (h - hz) * t * t);
      ctx.lineTo(vx + w * 0.004 * (1 + t * 8), hz + (h - hz) * t * t);
      ctx.lineTo(vx + w * 0.004 * (1 + t2 * 8), hz + (h - hz) * t2 * t2);
      ctx.lineTo(vx - w * 0.004 * (1 + t2 * 8), hz + (h - hz) * t2 * t2);
      ctx.closePath();
      ctx.fill();
    }
  };

  GROUND.sky = function (ctx, w, h, hz, P, spec, r) {
    for (var i = 0; i < 16; i++) {               // a sea of cloud below you
      var y = hz - (h - hz) * 0.1 + r() * (h - hz) * 1.2;
      var t = clamp((y - hz) / Math.max(h - hz, 1), 0, 1);
      ctx.globalAlpha = 0.5 + t * 0.5;
      blob(ctx, r() * w, y, w * (0.14 + t * 0.2), h * (0.02 + t * 0.03), 12, r,
        P.css([P.sky.haze[0], 30, 80 + t * 12], 0.85));
      ctx.globalAlpha = 1;
    }
  };

  GROUND.space = function () { /* nothing underfoot out there */ };

  /* --------------------------------------------------------------- weather */

  function paintWeather(ctx, w, h, hz, P, spec, r) {
    var wx = spec.weather;
    if (wx === 'rain' || wx === 'storm') {
      ctx.strokeStyle = P.css([P.sky.haze[0], 30, 86], 0.32);
      ctx.lineWidth = Math.max(1, w * 0.0016);
      var drops = wx === 'storm' ? 420 : 260;
      for (var i = 0; i < drops; i++) {
        var x = r() * w * 1.2 - w * 0.1, y = r() * h;
        var len = h * (0.02 + r() * 0.04);
        ctx.beginPath();
        ctx.moveTo(x, y);
        ctx.lineTo(x - len * 0.28, y + len);
        ctx.stroke();
      }
    }
    if (wx === 'storm' && r() > 0.25) {          // one fork, drawn once
      var lx = w * (0.2 + r() * 0.6), ly = 0, steps = 7;
      ctx.strokeStyle = P.css([50, 100, 92], 0.92);
      ctx.lineWidth = Math.max(1.5, w * 0.004);
      ctx.beginPath();
      ctx.moveTo(lx, ly);
      for (var s = 0; s < steps; s++) {
        lx += (r() - 0.5) * w * 0.08;
        ly += hz / steps;
        ctx.lineTo(lx, ly);
      }
      ctx.stroke();
      var g = ctx.createRadialGradient(lx, hz * 0.5, 0, lx, hz * 0.5, w * 0.4);
      g.addColorStop(0, P.css([50, 100, 92], 0.22));
      g.addColorStop(1, P.css([50, 100, 92], 0));
      ctx.fillStyle = g;
      ctx.fillRect(0, 0, w, h);
    }
    if (wx === 'snowfall') {
      for (var f = 0; f < 320; f++) {
        var fx = r() * w, fy = r() * h, fr = Math.max(1, w * 0.0022 * (0.4 + r() * 1.8));
        ctx.globalAlpha = 0.35 + r() * 0.6;
        ctx.fillStyle = P.css([200, 20, 98]);
        ctx.beginPath();
        ctx.arc(fx, fy, fr, 0, Math.PI * 2);
        ctx.fill();
      }
      ctx.globalAlpha = 1;
    }
    if (wx === 'fog') {
      for (var b = 0; b < 7; b++) {
        var by = hz * (0.5 + b * 0.12);
        var g2 = ctx.createLinearGradient(0, by - h * 0.06, 0, by + h * 0.12);
        g2.addColorStop(0, P.haze(0));
        g2.addColorStop(0.5, P.haze(0.34));
        g2.addColorStop(1, P.haze(0));
        ctx.fillStyle = g2;
        ctx.fillRect(0, by - h * 0.06, w, h * 0.18);
      }
    }
  }

  /* ---------------------------------------------------- placing the subject
   * Where a thing stands depends on what it is: a dragon flies, a castle sits
   * on the land, a whale is in the water, a planet hangs in the sky.
   */
  function placeBox(w, h, hz, spec, subject, index, total, r) {
    var meta = (SUBJECTS && SUBJECTS.META[subject.draw]) || { anchor: 'ground', base: 0.3, aspect: 1 };
    var spread = total > 1 ? (index + 0.5) / total : 0.5;
    var jitter = (r() - 0.5) * (total > 1 ? 0.12 : 0.30);
    var cx = w * clamp(spread + jitter, 0.12, 0.88);
    var depth = total > 1 ? index / Math.max(total - 1, 1) : 0;
    var size = Math.min(w, h) * meta.base * subject.scale * (1 - depth * 0.25);
    var bw = size * (meta.aspect || 1);
    var bh = size;

    var y;
    if (meta.anchor === 'sky') {
      y = h * (0.10 + r() * 0.28) + depth * h * 0.06;
    } else if (meta.anchor === 'water') {
      y = hz + (h - hz) * (0.18 + r() * 0.3) - bh;
    } else {
      var standY = hz + (h - hz) * (0.06 + depth * 0.10 + r() * 0.14);
      y = standY - bh;
    }
    return { x: cx - bw / 2, y: y, w: bw, h: bh, depth: depth, anchor: meta.anchor };
  }

  /* A soft pool of shade where a thing meets the ground. Cheap, and the
   * difference between a wolf standing in a forest and a wolf pasted over one. */
  function groundShadow(ctx, box, P) {
    var cy = box.y + box.h;
    var rx = box.w * 0.55, ry = box.h * 0.05;
    var g = ctx.createRadialGradient(box.x + box.w / 2, cy, 0, box.x + box.w / 2, cy, rx);
    g.addColorStop(0, P.silhouette(0, 0.5));
    g.addColorStop(1, P.silhouette(0, 0));
    ctx.save();
    ctx.translate(box.x + box.w / 2, cy);
    ctx.scale(1, ry / rx);
    ctx.fillStyle = g;
    ctx.beginPath();
    ctx.arc(0, 0, rx, 0, Math.PI * 2);
    ctx.fill();
    ctx.restore();
  }

  /* ----------------------------------------------------------------- render */

  function render(ctx, w, h, spec) {
    var P = makePalette(spec);
    var r = PROMPT.rng(spec, 'scene');
    var hz = clamp(spec.scene.horizon + (r() - 0.5) * 0.05, 0.42, 1.3) * h;

    ctx.save();
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.fillStyle = P.css(P.sky.top);
    ctx.fillRect(0, 0, w, h);

    paintSky(ctx, w, h, hz, P, spec, PROMPT.rng(spec, 'sky'));
    var light = paintLight(ctx, w, h, hz, P, spec, PROMPT.rng(spec, 'light'));
    clouds(ctx, w, h, hz, P, spec, PROMPT.rng(spec, 'cloud'));

    (GROUND[spec.scene.id] || GROUND.plains)(ctx, w, h, hz, P, spec, PROMPT.rng(spec, 'ground'), light);

    /* Subjects, furthest first so a nearer one overlaps it. */
    var sr = PROMPT.rng(spec, 'subject');
    var queue = [];
    if (spec.companion) queue.push({ s: spec.companion, n: 1 });
    if (spec.subject) queue.push({ s: spec.subject, n: spec.subject.count });
    queue.forEach(function (item) {
      for (var i = item.n - 1; i >= 0; i--) {
        var box = placeBox(w, h, hz, spec, item.s, i, item.n, sr);
        if (box.anchor === 'ground') groundShadow(ctx, box, P);
        if (SUBJECTS) SUBJECTS.draw(ctx, item.s, box, P, sr, spec);
      }
    });

    paintWeather(ctx, w, h, hz, P, spec, PROMPT.rng(spec, 'weather'));
    ctx.restore();
    return P;
  }

  var API = {
    render: render,
    makePalette: makePalette,
    GROUND: GROUND,
    SKY: SKY,
    SCENE_COLOUR: SCENE_COLOUR,
    groundShadow: groundShadow,
    helpers: { ridge: ridge, fillPoly: fillPoly, hills: hills, pine: pine, blob: blob, clamp: clamp, lerp: lerp }
  };

  root.CodaPaint = API;
  if (typeof module !== 'undefined' && module.exports) module.exports = API;
})(typeof window !== 'undefined' ? window : this);
