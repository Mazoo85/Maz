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

  /* The hour between the hours. There were four skies here and every picture
   * borrowed one of them; what a sky is actually made of is how high the sun
   * is standing, which is a number and not a name. These are the anchors — the
   * sun at -15 degrees is night, at -6 the blue hour, at the horizon a
   * sunrise or a sunset depending which way it is going, at 60 full day — and
   * every angle in between is mixed from the two it falls between. */
  var TWILIGHT = { top: [238, 60, 10], mid: [226, 54, 20], low: [212, 48, 33],
    light: [214, 42, 62], lightY: 0.90, haze: [220, 46, 34] };

  function radians(deg) { return deg * Math.PI / 180; }

  /*
   * Two skies mixed. Not by rotating the hue: the orange at a sunset horizon
   * and the pale blue overhead are 174 degrees apart, so rotating between them
   * goes through green and the sky turns bilious at mid-morning. Light does
   * not do that — one colour fades out as the other fades in, and the middle
   * of the fade is a washed-out warm grey, which is exactly what late-morning
   * haze looks like. So the mix happens in red, green and blue.
   */
  function toRGB(c) {
    var h = ((c[0] % 360) + 360) % 360 / 360, sat = c[1] / 100, l = c[2] / 100;
    if (sat === 0) return [l, l, l];
    var q = l < 0.5 ? l * (1 + sat) : l + sat - l * sat;
    var pp = 2 * l - q;
    function chan(t) {
      if (t < 0) t += 1;
      if (t > 1) t -= 1;
      if (t < 1 / 6) return pp + (q - pp) * 6 * t;
      if (t < 1 / 2) return q;
      if (t < 2 / 3) return pp + (q - pp) * (2 / 3 - t) * 6;
      return pp;
    }
    return [chan(h + 1 / 3), chan(h), chan(h - 1 / 3)];
  }

  function toHSL(c) {
    var r = c[0], g = c[1], b = c[2];
    var mx = Math.max(r, g, b), mn = Math.min(r, g, b);
    var l = (mx + mn) / 2, h = 0, sat = 0;
    if (mx !== mn) {
      var d = mx - mn;
      sat = l > 0.5 ? d / (2 - mx - mn) : d / (mx + mn);
      if (mx === r) h = (g - b) / d + (g < b ? 6 : 0);
      else if (mx === g) h = (b - r) / d + 2;
      else h = (r - g) / d + 4;
      h *= 60;
    }
    return [h, sat * 100, l * 100];
  }

  function mixHSL(a, b, t) {
    var x = toRGB(a), y = toRGB(b);
    return toHSL([lerp(x[0], y[0], t), lerp(x[1], y[1], t), lerp(x[2], y[2], t)]);
  }

  function mixSky(a, b, t) {
    return {
      top: mixHSL(a.top, b.top, t),
      mid: mixHSL(a.mid, b.mid, t),
      low: mixHSL(a.low, b.low, t),
      light: mixHSL(a.light, b.light, t),
      haze: mixHSL(a.haze, b.haze, t),
      lightY: lerp(a.lightY, b.lightY, t)
    };
  }

  /* The sky at a given sun angle. `rising` picks which horizon it is heading
   * for: a sunrise is not a sunset run backwards — morning air is cleaner, so
   * dawn is the cooler of the two. */
  function skyAt(sun, rising) {
    var horizon = rising ? SKY.dawn : SKY.dusk;
    var stops = [
      { at: -15, sky: SKY.night },
      { at: -6, sky: TWILIGHT },
      { at: 0.5, sky: horizon },
      { at: 60, sky: SKY.day }
    ];
    if (sun <= stops[0].at) return SKY.night;
    if (sun >= stops[stops.length - 1].at) return SKY.day;
    for (var i = 1; i < stops.length; i++) {
      if (sun > stops[i].at) continue;
      var lo = stops[i - 1], hi = stops[i];
      return mixSky(lo.sky, hi.sky, (sun - lo.at) / (hi.at - lo.at));
    }
    return SKY.day;
  }

  /*
   * How much light the ground has to hand back up at whatever stands on it.
   *
   * This is reflected sunlight, so it follows the sun: overhead at noon the
   * ground is flooded, at the horizon it is getting a fraction of that, and
   * once the sun has gone there is only the moon. It was a table of four
   * numbers; it is now the angle it was always standing in for.
   */
  function groundLit(sun) {
    if (sun == null) return 0.6;
    if (sun > 0) return 0.30 + 0.70 * Math.sin(radians(Math.min(sun, 90)));
    return Math.max(0.08, 0.30 + (sun / 18) * 0.22);
  }

  /*
   * How much of the light arrives straight from the sun, and how much has been
   * through a cloud first.
   *
   * This is the difference between a sunny day and an overcast one, and the
   * engine did not have it: an overcast picture was a sunny picture with grey
   * clouds pasted over the top, still throwing hard black shadows from a sun
   * nobody could see. Cloud turns the sun into the whole sky — the shadows go
   * pale and spread until they are barely there, the lit side and the dark
   * side of a thing draw together, and the disc itself goes behind the weather.
   */
  var DIFFUSE = { clear: 0, aurora: 0.12, clouds: 0.62, snowfall: 0.70,
    rain: 0.76, storm: 0.84, fog: 0.90 };

  /*
   * How wet the ground is.
   *
   * Rain was drawn as streaks in the air and nothing else: it fell in front of
   * a dry field. A wet surface is darker and more saturated than a dry one —
   * water fills the pores and light that would have scattered straight back at
   * you goes into the ground instead — and it throws a long smear of whatever
   * light there is back up at the camera. Those two things are most of what
   * makes a photograph look as though it has been raining.
   */
  var WETNESS = { clear: 0, clouds: 0, aurora: 0, fog: 0.22, snowfall: 0.14,
    rain: 1, storm: 0.92 };

  function wetness(spec) {
    if (!spec || spec.weather == null) return 0;
    if (WETNESS[spec.weather] == null) return 0;
    return clamp(WETNESS[spec.weather] * howMuch(spec), 0, 1);
  }

  /* How much of the weather there is, if the words said. "A light drizzle" and
   * "a downpour" are the same weather at two strengths. */
  function howMuch(spec) {
    var k = spec && spec.weatherStrength;
    return typeof k === 'number' ? clamp(k, 0.2, 2) : 1;
  }

  function softness(spec) {
    if (!spec || spec.weather == null) return 0;
    if (DIFFUSE[spec.weather] == null) return 0;
    return clamp(DIFFUSE[spec.weather] * howMuch(spec), 0, 0.95);
  }

  /*
   * How far a shadow runs, as a share of how wide the thing throwing it is.
   * A shadow's length is the cotangent of the sun's angle: straight overhead
   * it is a puddle at your feet, and near the horizon it runs away across the
   * ground. The old version knew two lengths, one for "dawn or dusk".
   */
  function shadowReach(sun) {
    var above = sun == null ? 35 : Math.max(sun, 6);
    return clamp(0.45 / Math.tan(radians(above)), 0.22, 2.4);
  }

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
  function makePalette(spec, opts) {
    var sky = spec.sun == null ? (SKY[spec.time] || SKY.dusk)
      : skyAt(spec.sun, spec.rising);
    var scene = SCENE_COLOUR[spec.scene.id] || SCENE_COLOUR.plains;
    /* A photograph the person chose replaces the built-in colour table: the
     * hour and the setting still decide the shapes, the photograph decides what
     * they are made of. */
    if (spec.photo && spec.photo.use && spec.photo.use.colours && spec.photo.palette) {
      var pal = spec.photo.palette;
      sky = {
        top: pal.sky.top, mid: pal.sky.mid, low: pal.sky.low,
        light: pal.sky.light,
        lightY: pal.sky.lightY == null ? sky.lightY : pal.sky.lightY,
        haze: pal.sky.haze
      };
      scene = {
        ink: pal.scene.ink, land: pal.scene.land, far: pal.scene.far,
        sea: pal.scene.sea, water: scene.water
      };
    }
    /*
     * The ground is only as bright as the light falling on it.
     *
     * This was missing entirely: the grass in a meadow was the same green at
     * midnight as at noon, with a dark sky hung above it, and no amount of
     * work on the sky could fix a field that was still lit. A surface at night
     * is dim, nearly colourless, and takes the colour of what little light
     * there is — so lightness and saturation both follow the sun, and the hue
     * drifts towards the sky as the light goes.
     *
     * A photograph the person brought already has its own light in it, so it
     * is left alone.
     */
    var lit = groundLit(spec.sun);
    if (!(spec.photo && spec.photo.use && spec.photo.use.colours && spec.photo.palette)) {
      var toward = sky.haze;
      scene = (function (src) {
        /* Wet ground is darker and more saturated than dry: the water fills
         * the pores, and light that would have scattered straight back goes
         * into the ground instead. */
        var wet = wetness(spec);
        function litten(c) {
          if (!c) return c;
          var dh = ((toward[0] - c[0]) % 360 + 540) % 360 - 180;
          return [
            (c[0] + dh * (1 - lit) * 0.30 + 360) % 360,
            clamp(c[1] * (0.42 + 0.58 * lit) * (1 + 0.34 * wet), 0, 100),
            c[2] * (0.32 + 0.68 * lit) * (1 - 0.30 * wet)
          ];
        }
        return {
          ink: litten(src.ink), land: litten(src.land), far: litten(src.far),
          sea: litten(src.sea), water: src.water
        };
      })(scene);
    }

    var tint = spec.palette;
    var drama = spec.mood;
    /* How hard a colour word pulls. "A red dragon" should give you a red
     * dragon, not a red world — so the scene is mixed weakly and the subject
     * strongly, and the painter builds one palette of each. With no subject to
     * carry it, the colour has nowhere to go but the scene. */
    var pull = (opts && opts.tintStrength != null) ? opts.tintStrength
      : (spec.subject ? 0.18 : 0.5);

    function bend(c) {
      var h = c[0], s = c[1], l = c[2];
      if (tint) {
        var d = ((tint.hue - h) % 360 + 540) % 360 - 180;   // shortest way round
        h = (h + d * pull + 360) % 360;
        s = clamp(s * (1 + (tint.sat - 1) * (pull / 0.55)), 0, 100);
        l = clamp(l * (0.94 + (tint.warm - 1) * 0.22 * (pull / 0.55)), 0, 100);
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
      light_at: null,                 /* filled in once the sun is placed */
      css: css,
      bend: bend,
      /* A silhouette at `depth`, optionally lightened towards the land tone. */
      ink: function (depth, a) { return css(depthMix(scene.ink, depth), a); },
      /* What the *subject* is cut from. Darker than the land it stands on, so
       * a fox against a treeline is still a fox and not a smudge — this one
       * difference is most of what makes the pictures read. */
      silhouette: function (depth, a) {
        var c = depthMix(scene.ink, depth);
        /* A silhouette is dark by design, but a subject the prompt gave a
         * colour has to be light enough for that colour to survive — a red
         * dragon painted at silhouette darkness is just a dragon. */
        var lift = (tint && pull > 0.5) ? 1.55 : 1;
        return css([c[0], c[1] * 0.92 * (lift > 1 ? 1.25 : 1),
          clamp(c[2] * (0.62 - drama * 0.10) * lift, 0, 100)], a);
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

  /*
   * The ridge line out of a photograph, scaled to this frame — so the hills in
   * the picture are the hills the person photographed. Returns null when there
   * was no clear horizon to read, and the fractal one is used instead: a
   * close-up of a wall has no skyline, and inventing one from it looks worse
   * than not trying.
   */
  function photoRidge(spec, w, h, baseY, amplitude) {
    if (!spec.photo || !spec.photo.use || !spec.photo.use.skyline) return null;
    var sky = spec.photo.skyline;
    if (!sky || !sky.line || sky.line.length < 8 || sky.confidence < 0.35) return null;
    var line = sky.line;
    var mean = sky.mean || 0.5;
    var pts = [];
    for (var i = 0; i < line.length; i++) {
      var x = -w * 0.05 + (w * 1.10) * (i / (line.length - 1));
      /* The photograph gives the shape; this frame gives where it sits and how
       * tall it is, so a flat-ish horizon still reads as mountains. */
      pts.push([x, baseY + (line[i] - mean) * amplitude * 2.4]);
    }
    return pts;
  }

  /*
   * Give a band of land its form.
   *
   * Every ridge, hill, dune and bank in this engine was one flat area of
   * colour, and a landscape made of flat areas is a landscape made of cut
   * paper however carefully the colours recede. Real ground turns away from
   * you: its top catches the sky, its foot sits in its own shade. So after the
   * flat fill, the same path is filled again with light-to-dark down its
   * height. It costs one extra fill and it is the difference between a band of
   * colour and a hillside.
   *
   * Done here rather than in each scene on purpose — every kind of ground in
   * the engine already comes through these two functions, so they all gain it
   * at once and none can be forgotten.
   */
  function shadeBand(ctx, topY, closeY, strength) {
    var span = closeY - topY;
    if (!(span > 2)) return null;
    var g = ctx.createLinearGradient(0, topY, 0, closeY);
    var k = (strength == null ? 1 : strength);
    var up = 0.19 * k;
    var down = 0.28 * k;
    g.addColorStop(0, 'rgba(255,255,255,' + up.toFixed(3) + ')');
    g.addColorStop(0.26, 'rgba(255,255,255,' + (up * 0.20).toFixed(3) + ')');
    g.addColorStop(0.50, 'rgba(0,0,0,0)');
    g.addColorStop(1, 'rgba(0,0,0,' + down.toFixed(3) + ')');
    return g;
  }

  /*
   * The air in front of a band of land.
   *
   * Two bands of colour meeting at a hard line is the other half of why this
   * looked like cut paper: in the world there is a mile of air between one
   * ridge and the next, and it shows as the near one's top edge being slightly
   * washed out by it. A thin band of the sky's own haze along the top of each
   * layer is the whole of that, and it does more to separate the layers than
   * any amount of shading within them.
   */
  var HAZE_EDGE = null;   // set per render from the palette
  /* Set per render too, so every band in the engine gets its grain without each
   * one having to remember to ask. Adding it at each call site would mean
   * forty-odd places to get right and one of them silently missed. */
  var GRAIN = null;
  function hazeEdge(ctx, topY, closeY) {
    if (!HAZE_EDGE) return null;
    var span = closeY - topY;
    if (!(span > 3)) return null;
    var reach = Math.min(span, Math.max(6, span * 0.30));
    var g = ctx.createLinearGradient(0, topY, 0, topY + reach);
    g.addColorStop(0, HAZE_EDGE.on);
    g.addColorStop(1, HAZE_EDGE.off);
    return g;
  }

  function fillPoly(ctx, pts, closeY, style, shade) {
    ctx.beginPath();
    ctx.moveTo(pts[0][0], pts[0][1]);
    for (var i = 1; i < pts.length; i++) ctx.lineTo(pts[i][0], pts[i][1]);
    ctx.lineTo(pts[pts.length - 1][0], closeY);
    ctx.lineTo(pts[0][0], closeY);
    ctx.closePath();
    ctx.fillStyle = style;
    ctx.fill();
    if (shade !== false) {
      var top = pts[0][1];
      for (var j = 1; j < pts.length; j++) if (pts[j][1] < top) top = pts[j][1];
      var g = shadeBand(ctx, top, closeY, shade);
      if (g) { ctx.fillStyle = g; ctx.fill(); }
      var hz2 = hazeEdge(ctx, top, closeY);
      if (hz2) { ctx.fillStyle = hz2; ctx.fill(); }
      if (GRAIN) grainIn(ctx, pts, closeY, GRAIN.w, GRAIN.h, GRAIN.P, GRAIN.r, {
        density: 0.75, size: 0.0021, streak: 1.4
      });
    }
  }

  /* Soft rolling ground — hills, dunes, swells — as one wavy band. */
  /* The points along a wavy band, so grain can be clipped to it the same way
   * it is clipped to a ridge. */
  function hillPoints(w, baseY, amp, freq, phase) {
    var pts = [];
    for (var x = 0; x <= w; x += Math.max(2, w / 90)) {
      var t = x / w * freq * Math.PI * 2 + phase;
      pts.push([x, baseY + Math.sin(t) * amp + Math.sin(t * 2.3 + 1.7) * amp * 0.35]);
    }
    return pts;
  }

  function hills(ctx, w, baseY, amp, freq, phase, closeY, style, shade) {
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
    if (shade !== false) {
      var g = shadeBand(ctx, baseY - amp * 1.35, closeY, shade);
      if (g) { ctx.fillStyle = g; ctx.fill(); }
      var he = hazeEdge(ctx, baseY - amp * 1.35, closeY);
      if (he) { ctx.fillStyle = he; ctx.fill(); }
    }
    if (shade !== false && GRAIN) {
      grainIn(ctx, hillPoints(w, baseY, amp, freq, phase), closeY,
        GRAIN.w, GRAIN.h, GRAIN.P, GRAIN.r, { density: 1.2, size: 0.0024 });
    }
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

    /* Behind cloud the disc goes soft and dim and its glow spreads across the
     * whole sky — which is the sky becoming the light. */
    var soft = spec.scene.id === 'space' ? 0 : softness(spec);

    var glow = ctx.createRadialGradient(x, y, 0, x, y, rad * 9 * (1 + soft * 1.1));
    glow.addColorStop(0, P.css(P.sky.light, 0.55 * (1 - soft * 0.45)));
    glow.addColorStop(0.25, P.css(P.sky.light, 0.16 * (1 - soft * 0.3)));
    glow.addColorStop(1, P.css(P.sky.light, 0));
    ctx.fillStyle = glow;
    ctx.fillRect(0, 0, w, h);

    ctx.beginPath();
    ctx.arc(x, y, rad * (1 + soft * 0.45), 0, Math.PI * 2);
    ctx.fillStyle = P.light((spec.time === 'night' ? 0.92 : 1) * (1 - soft * 0.80));
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

  /*
   * Clouds with an inside.
   *
   * A cloud here was two flat blobs, one pale and one paler, and it read as a
   * sticker on the sky — which matters more than it sounds, because a cloud is
   * in most of these pictures and it is the largest thing in a lot of them.
   *
   * A real cloud is a pile of lumps with a flat wet bottom. The sun hits the
   * tops, so they are bright on the side facing it; the underside is in the
   * shadow of everything above it, so it is dark, flat and slightly blue; and
   * the edge where the two meet is the only part that looks like cotton. Those
   * three passes, plus lumps of different sizes sat on one base line rather
   * than one blob stretched wide, are the whole difference.
   *
   * What kind of cloud is the weather's business. A clear day has a few small
   * fair-weather lumps and some thin streaks much higher up; an overcast one
   * has a low grey deck; a storm has towers with ragged bottoms.
   */
  var CLOUD_KINDS = {
    clear:    { count: 4, lump: 0.55, tall: 0.85, dark: 0.30, alpha: 0.55, wisps: 5, deck: 0 },
    clouds:   { count: 11, lump: 1.15, tall: 1.05, dark: 0.66, alpha: 0.86, wisps: 2, deck: 0.52 },
    rain:     { count: 9, lump: 1.10, tall: 0.80, dark: 0.80, alpha: 0.90, wisps: 0, deck: 0.62 },
    storm:    { count: 10, lump: 1.25, tall: 1.45, dark: 0.92, alpha: 0.94, wisps: 0, deck: 0.55 },
    snowfall: { count: 8, lump: 1.00, tall: 0.75, dark: 0.55, alpha: 0.80, wisps: 1, deck: 0.45 },
    fog:      { count: 3, lump: 0.85, tall: 0.55, dark: 0.35, alpha: 0.45, wisps: 0, deck: 0.20 },
    aurora:   { count: 3, lump: 0.60, tall: 0.80, dark: 0.35, alpha: 0.50, wisps: 3, deck: 0 }
  };

  function clouds(ctx, w, h, horizon, P, spec, r, light) {
    if (spec.scene.id === 'space' || spec.scene.id === 'cave') return;
    var kind = CLOUD_KINDS[spec.weather] || CLOUD_KINDS.clear;
    var lightX = light ? light.x : w * 0.5;
    var lightY = light ? light.y : 0;

    /* The overcast deck: not a cloud but the absence of a sky, so it goes
     * behind everything else and simply lowers the lid. */
    if (kind.deck > 0.01) {
      var lid = ctx.createLinearGradient(0, 0, 0, Math.max(horizon, h * 0.4));
      var grey = [P.sky.haze[0], P.sky.haze[1] * 0.55, P.sky.haze[2] * 0.45];
      lid.addColorStop(0, P.css(grey, 0.78 * kind.deck));
      lid.addColorStop(0.72, P.css(grey, 0.34 * kind.deck));
      lid.addColorStop(1, P.css(grey, 0));
      ctx.fillStyle = lid;
      ctx.fillRect(0, 0, w, Math.max(horizon, h * 0.4));
    }

    /* Thin streaks, very high up and going nowhere: the fair-weather sky is
     * never quite empty. */
    for (var s = 0; s < kind.wisps; s++) {
      var wy = horizon * (0.05 + r() * 0.30);
      var wx = r() * w;
      var wl = w * (0.12 + r() * 0.26);
      ctx.globalAlpha = 0.10 + r() * 0.14;
      ctx.fillStyle = P.css(P.sky.haze, 1);
      for (var q = 0; q < 5; q++) {
        var qy = wy + q * h * 0.004 + (r() - 0.5) * h * 0.004;
        ctx.fillRect(wx + q * wl * 0.06, qy, wl * (0.5 + r() * 0.6), Math.max(1, h * 0.0035));
      }
      ctx.globalAlpha = 1;
    }

    for (var i = 0; i < kind.count; i++) {
      /* Where it sits, and therefore how far off it is: a cloud low in the
       * frame is not a low cloud, it is the same cloud a long way off, and it
       * is smaller, flatter and hazier for it. Spread evenly from overhead to
       * the horizon, so a sky has big ones above and a crowd of small ones
       * along the bottom rather than all of one or all of the other. */
      var t = (i + r()) / kind.count;
      var y = horizon * (0.08 + t * 0.78);
      var far = clamp(y / Math.max(horizon, 1), 0, 1);
      var x = r() * w * 1.1 - w * 0.05;
      /* One place, and only one, where distance shrinks a cloud: two of them
       * would each hide the other going missing. */
      var scale = (0.55 + r() * 0.85) * (1.30 - far * 0.85) * kind.lump;
      var rx = w * 0.13 * scale;
      var ry = h * 0.026 * scale * kind.tall;
      var baseY = y + ry * 0.55;

      /* The lumps. Sat side by side on one flat base rather than stacked
       * anywhere, which is what gives a cloud a bottom. */
      var lumps = 3 + Math.floor(r() * 4);
      var puffs = [];
      for (var k = 0; k < lumps; k++) {
        var px = x + (k / Math.max(lumps - 1, 1) - 0.5) * rx * 1.75;
        var size = ry * (0.75 + r() * 1.25) * (1 - Math.abs(k / Math.max(lumps - 1, 1) - 0.5) * 0.7);
        puffs.push({ x: px, y: baseY - size * 0.72, rx: size * (1.5 + r() * 0.7), ry: size });
      }

      /* Underside first: in the shadow of everything above it, flat along the
       * base, and blue rather than grey because what light reaches it comes
       * from the sky. */
      ctx.globalAlpha = kind.alpha * (0.55 + kind.dark * 0.45) * (1 - far * 0.3);
      var under = [P.sky.haze[0] + 8, P.sky.haze[1] * 0.75,
        P.sky.haze[2] * (0.72 - kind.dark * 0.34)];
      puffs.forEach(function (pf) {
        blob(ctx, pf.x, pf.y + pf.ry * 0.36, pf.rx, pf.ry * 0.78, 9, r, P.css(under, 1));
      });

      /* Then the body, which is most of the cloud. */
      ctx.globalAlpha = kind.alpha * (1 - far * 0.25);
      var body = [P.sky.haze[0], P.sky.haze[1] * 0.8,
        P.sky.haze[2] * (1.04 - kind.dark * 0.55)];
      puffs.forEach(function (pf) {
        blob(ctx, pf.x, pf.y, pf.rx, pf.ry, 11, r, P.css(body, 1));
      });

      /* And the tops that can see the sun. Offset towards it, so a cloud at
       * dusk is lit along its underside and one at noon along its crown —
       * which is the single thing that tells you where the light is. */
      var vx = lightX - x, vy = lightY - baseY;
      var len = Math.sqrt(vx * vx + vy * vy) || 1;
      ctx.globalAlpha = kind.alpha * (0.55 - kind.dark * 0.34) * (1 - far * 0.4);
      puffs.forEach(function (pf) {
        blob(ctx, pf.x + (vx / len) * pf.ry * 0.5, pf.y + (vy / len) * pf.ry * 0.5,
          pf.rx * 0.72, pf.ry * 0.62, 9, r, P.light(0.85));
      });
      ctx.globalAlpha = 1;
    }
  }

  /* ------------------------------------------------------------- the ground
   * One function per setting. Each gets the frame, where the horizon is, and
   * its own generator, and is free to draw as far forward as it likes.
   */
  /*
   * What a ridge is made of.
   *
   * A mountain in this engine was an outline and nothing else — no snow where
   * it is cold, no trees where they stop, no rock showing through. An outline
   * is a shape; these are what make it a mountain. All three clip to the ridge
   * that has already been drawn, so they follow whatever shape it happens to
   * have, including one read off somebody's photograph.
   */
  function clipTo(ctx, pts, closeY) {
    ctx.beginPath();
    ctx.moveTo(pts[0][0], pts[0][1]);
    for (var i = 1; i < pts.length; i++) ctx.lineTo(pts[i][0], pts[i][1]);
    ctx.lineTo(pts[pts.length - 1][0], closeY);
    ctx.lineTo(pts[0][0], closeY);
    ctx.closePath();
    ctx.clip();
  }

  /* Snow lies above a line, and the line is not straight: it dips where the
   * slope faces the sun and reaches down the gullies. */
  function snowCap(ctx, pts, closeY, w, h, P, r, strength) {
    var top = pts[0][1];
    for (var i = 1; i < pts.length; i++) if (pts[i][1] < top) top = pts[i][1];
    var bottom = pts[0][1];
    for (var j = 1; j < pts.length; j++) if (pts[j][1] > bottom) bottom = pts[j][1];
    if (bottom - top < h * 0.03) return;
    var line = top + (bottom - top) * (0.30 + r() * 0.22);

    ctx.save();
    clipTo(ctx, pts, closeY);
    ctx.beginPath();
    ctx.moveTo(-w * 0.1, top - h * 0.2);
    ctx.lineTo(w * 1.1, top - h * 0.2);
    var wob = (bottom - top) * 0.26;
    for (var x = w * 1.1; x >= -w * 0.1; x -= w / 26) {
      var t = x / w;
      ctx.lineTo(x, line + Math.sin(t * 7.3 + r() * 0.001) * wob * 0.5 +
        Math.sin(t * 17.1) * wob * 0.28);
    }
    ctx.closePath();
    ctx.fillStyle = P.css([P.sky.haze[0], 14, 95], 0.72 * (strength == null ? 1 : strength));
    ctx.fill();
    ctx.restore();
  }

  /* Rock shows in bands, because that is how rock was laid down. */
  function strata(ctx, pts, closeY, w, h, P, r, n) {
    var top = pts[0][1], bottom = pts[0][1];
    for (var i = 1; i < pts.length; i++) {
      if (pts[i][1] < top) top = pts[i][1];
      if (pts[i][1] > bottom) bottom = pts[i][1];
    }
    var span = Math.max(bottom - top, h * 0.04);
    /* More bands of rock show as you get closer to the rock. */
    var bands = Math.max(2, Math.round((n || 5) * ((GRAIN && GRAIN.detail) || 1)));
    ctx.save();
    clipTo(ctx, pts, closeY);
    for (var b = 0; b < bands; b++) {
      var y = top + span * (0.22 + b * (0.72 / bands)) + (r() - 0.5) * span * 0.06;
      var thick = Math.max(1.5, span * (0.012 + r() * 0.02));
      ctx.fillStyle = b % 2 ? 'rgba(255,255,255,0.055)' : 'rgba(0,0,0,0.085)';
      ctx.beginPath();
      ctx.moveTo(-w * 0.1, y);
      for (var x2 = -w * 0.1; x2 <= w * 1.1; x2 += w / 18) {
        ctx.lineTo(x2, y + Math.sin(x2 / w * 5.1 + b) * span * 0.02);
      }
      ctx.lineTo(w * 1.1, y + thick);
      for (var x3 = w * 1.1; x3 >= -w * 0.1; x3 -= w / 18) {
        ctx.lineTo(x3, y + thick + Math.sin(x3 / w * 5.1 + b) * span * 0.02);
      }
      ctx.closePath();
      ctx.fill();
    }
    ctx.restore();
  }

  /*
   * Grain.
   *
   * Nothing in the world is one smooth colour — not rock, not sand, not a
   * painted wall. Scattered marks at very low contrast, clipped to the band
   * they belong to, are the cheapest thing on this whole list that makes a
   * surface stop looking like a fill and start looking like a material.
   *
   * Deliberately not a noise texture stretched over the finished picture: that
   * lies across everything at the same scale regardless of how far away it is,
   * which reads as dirt on the lens rather than as surface.
   */
  function grainIn(ctx, pts, closeY, w, h, P, r, opts) {
    opts = opts || {};
    var top = pts[0][1], bottom = pts[0][1];
    for (var i = 1; i < pts.length; i++) {
      if (pts[i][1] < top) top = pts[i][1];
      if (pts[i][1] > bottom) bottom = pts[i][1];
    }
    /* A caller can say how deep the band really is. Ground drawn as a shape has
     * its depth in its points; ground drawn as a filled rectangle — a cave wall,
     * say — does not, and would otherwise be measured as having almost none. */
    var span = opts.span != null ? opts.span : Math.max(bottom - top, h * 0.02);
    /* One mark per ten-by-ten patch or so. Sparser than that and it reads as
     * specks of dust rather than as a surface; the first attempt here was one
     * per thirty-by-thirty and was invisible at any size. */
    var detail = (GRAIN && GRAIN.detail) || 1;
    var n = Math.round((opts.density == null ? 1 : opts.density) * detail * (w * span) / 90);
    if (n < 8) return;
    n = Math.min(n, 14000);
    var long = opts.streak || 1;           // >1 draws marks along, like sediment
    /* Closer means finer, not merely more: coarse marks enlarged would read as
     * a picture of a texture rather than as the surface itself. */
    var size = Math.max(0.7, Math.min(w, h) * (opts.size || 0.0022) / Math.sqrt(detail));

    ctx.save();
    clipTo(ctx, pts, closeY);
    for (var m = 0; m < n; m++) {
      var x = -w * 0.05 + r() * w * 1.1;
      var y = top - span * 0.05 + r() * span * 1.1;
      var dark = r() < 0.5;
      ctx.fillStyle = dark
        ? 'rgba(0,0,0,' + (0.030 + r() * 0.045).toFixed(3) + ')'
        : 'rgba(255,255,255,' + (0.022 + r() * 0.038).toFixed(3) + ')';
      ctx.fillRect(x, y, size * long * (0.6 + r() * 1.5), size * (0.6 + r() * 1.1));
    }
    ctx.restore();
  }

  /* Trees stop at a height, and thin out before they stop. */
  function treeLine(ctx, pts, closeY, w, h, P, r, depth) {
    var top = pts[0][1], bottom = pts[0][1];
    for (var i = 1; i < pts.length; i++) {
      if (pts[i][1] < top) top = pts[i][1];
      if (pts[i][1] > bottom) bottom = pts[i][1];
    }
    if (bottom - top < h * 0.02) return;
    var line = top + (bottom - top) * (0.55 + r() * 0.18);
    var closeness = (GRAIN && GRAIN.detail) || 1;
    var size = Math.max(2.5, h * 0.016);
    ctx.save();
    clipTo(ctx, pts, closeY);
    ctx.fillStyle = P.ink(Math.max(0, (depth || 0) - 0.18), 0.55);
    /* Individual trees, closer together, the nearer you stand to them. */
    var step = Math.max(3, w / (90 * closeness));
    for (var x = -w * 0.05; x < w * 1.05; x += step) {
      var jitter = (r() - 0.5) * step * 1.4;
      var y = line + (r() - 0.3) * (bottom - top) * 0.30;
      if (y < line - (bottom - top) * 0.05) continue;   // thinning towards the top
      var tall = size * (0.6 + r() * 0.9);
      ctx.beginPath();
      ctx.moveTo(x + jitter, y - tall);
      ctx.lineTo(x + jitter + tall * 0.32, y);
      ctx.lineTo(x + jitter - tall * 0.32, y);
      ctx.closePath();
      ctx.fill();
    }
    ctx.restore();
  }

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
      var pts = photoRidge(spec, w, h, baseY - peak * 0.55, peak * (1 - layer * 0.3)) ||
        ridge(-w * 0.05, baseY - peak * (0.4 + r() * 0.5), w * 1.05,
          baseY - peak * (0.4 + r() * 0.5), peak * 1.1, r, 7);
      fillPoly(ctx, pts, h, P.ink(depth));
      strata(ctx, pts, h, w, h, P, r, 4 - layer);
      if (layer < 2) snowCap(ctx, pts, h, w, h, P, r, 0.55 - layer * 0.18);
      if (layer > 0) treeLine(ctx, pts, h, w, h, P, r, depth);
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
      var pts = photoRidge(spec, w, h, baseY - peak * 0.7, peak * (1 - layer * 0.3)) ||
        ridge(-w * 0.05, baseY - peak * 0.6, w * 1.05, baseY - peak * 0.85, peak * 0.95, r, 7);
      fillPoly(ctx, pts, h, P.ink(0.85 - layer * 0.38));
      snowCap(ctx, pts, h, w, h, P, r, 0.85 - layer * 0.14);
      if (layer === 2) strata(ctx, pts, h, w, h, P, r, 3);
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

  /*
   * Water, which is mostly a mirror.
   *
   * It was the sea's own colour with distance haze mixed into the far end —
   * which gets the lightness right and the colour wrong. Water far from you is
   * seen at a grazing angle and is almost entirely a reflection of the sky, so
   * under a sunset a lake goes orange; this one stayed blue under an orange
   * sky, and no amount of haze fixes that. Water near you is seen from above,
   * so you are looking into it rather than off it, and it keeps its own
   * colour.
   *
   * And the reflection is broken. The surface is never flat, so what it gives
   * back is the sky in pieces, shuffled sideways, and more broken the nearer
   * it comes — which is the steepening angle, again.
   */
  function water(ctx, w, h, top, P, spec, r, light) {
    var g = ctx.createLinearGradient(0, top, 0, h);
    /* The far band is the sky coming back off it; the near band is the water
     * itself, looked into rather than off. */
    g.addColorStop(0, P.css(mixHSL(P.sky.low, P.scene.sea || P.scene.far, 0.35), 1));
    g.addColorStop(0.18, P.css(mixHSL(P.sky.low, P.scene.sea || P.scene.far, 0.62), 1));
    g.addColorStop(1, P.sea(0));
    ctx.fillStyle = g;
    ctx.fillRect(0, top, w, h - top);

    /* The reflection, in pieces. Thin slices of the sky's own colour, each
     * shoved sideways by a different amount and fading out as the water comes
     * towards you and the angle steepens. */
    var slices = 22;
    for (var s2 = 0; s2 < slices; s2++) {
      var ts = s2 / slices;
      var sy = top + (h - top) * Math.pow(ts, 1.4);
      var deep = 1 - ts;
      ctx.globalAlpha = 0.30 * deep * deep;
      ctx.fillStyle = P.css(P.sky.low, 1);
      var shove = (r() - 0.5) * w * 0.10 * (0.3 + ts);
      ctx.fillRect(shove, sy, w * (0.5 + r() * 0.7),
        Math.max(1, (h - top) * 0.02 * (0.5 + ts)));
    }
    ctx.globalAlpha = 1;
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
    /* A cave is the one place in the engine that is rock from edge to edge, and
     * it was the one place with no grain at all: its walls are drawn as filled
     * rectangles rather than as bands, so the shared dressing never saw them. */
    grainIn(ctx, [[0, 0], [w, 0]], h, w, h, P, r, { span: h, density: 1.3, size: 0.0024 });
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
    var much = howMuch(spec);
    if (wx === 'rain' || wx === 'storm') {
      ctx.strokeStyle = P.css([P.sky.haze[0], 30, 86], 0.32);
      ctx.lineWidth = Math.max(1, w * 0.0016);
      var drops = Math.round((wx === 'storm' ? 420 : 260) * much);
      for (var i = 0; i < drops; i++) {
        var x = r() * w * 1.2 - w * 0.1, y = r() * h;
        var len = h * (0.02 + r() * 0.04);
        ctx.beginPath();
        ctx.moveTo(x, y);
        ctx.lineTo(x - len * 0.28, y + len);
        ctx.stroke();
      }
    }
    if (wx === 'storm' && r() > 0.25 / Math.max(much, 0.25)) {          // one fork, drawn once
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
      for (var f = 0; f < Math.round(320 * much); f++) {
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
        g2.addColorStop(0.5, P.haze(clamp(0.34 * much, 0, 0.7)));
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
  /*
   * Where the skyline goes.
   *
   * Dead centre is the one place a photographer never puts it: it cuts the
   * picture in half and says neither "this is about the sky" nor "this is
   * about the ground". Anyone framing a shot pushes it off towards a third,
   * one way or the other, and which way is a decision about what the picture
   * is of. So a skyline that lands near the middle is moved to whichever third
   * it was already nearer, and one that is already well off centre is left
   * where the setting put it.
   */
  function skylineAt(spec, shot, wobble) {
    var at = clamp(spec.scene.horizon + (wobble || 0) * 0.05 + (shot ? shot.horizon : 0),
      0.22, 1.3);
    if (at > 0.42 && at < 0.58) at = lerp(at, at < 0.5 ? 0.36 : 0.64, 0.8);
    return at;
  }

  function placeBox(w, h, hz, spec, subject, index, total, r) {
    var meta = (SUBJECTS && SUBJECTS.META[subject.draw]) || { anchor: 'ground', base: 0.3, aspect: 1 };
    var cx;
    if (total > 1) {
      cx = w * clamp((index + 0.5) / total + (r() - 0.5) * 0.12, 0.12, 0.88);
    } else {
      /* Off-centre by default: a third of the way in, either side, with dead
       * centre kept as one option among three rather than the only one. */
      var thirds = [0.33, 0.5, 0.67];
      cx = w * clamp(thirds[Math.floor(r() * 3) % 3] + (r() - 0.5) * 0.09, 0.14, 0.86);
    }
    var depth = total > 1 ? index / Math.max(total - 1, 1) : 0;
    /* Scale contrast: a lone subject is sometimes near and large, sometimes a
     * small thing in a big landscape. Both read better than always mid-sized. */
    var swing = total > 1 ? 1 : (0.72 + r() * 0.75);
    /* Told where the picture is taken from, the roll of the dice gives way:
     * asked for a close-up somebody wants a close-up, not a close-up two times
     * in three. */
    var shot = spec.shot;
    /* Asked for a close-up of a crowd, the whole crowd cannot fill the frame:
     * the nearest would be capped to fit and the rest would be capped to very
     * nearly the same size, which flattens the group into a line. A group
     * takes half the step towards the framing that was asked for, and keeps
     * its depth. */
    if (shot) {
      swing = total > 1 ? 1 + (shot.size - 1) * 0.45
        : shot.size * (0.9 + r() * 0.2);
    }
    /*
     * How far away it is, which on a flat piece of ground is one number and
     * not two.
     *
     * A thing standing further off has its feet closer to the skyline AND is
     * smaller. Those are the same fact seen twice, so they cannot disagree —
     * and they did: the further members of a group were drawn smaller and
     * stood *lower* in the frame, which is a smaller thing standing nearer the
     * camera and reads as a toy rather than as distance.
     *
     * Where it stands is decided first, and the size follows from it, so the
     * two cannot come apart. Each one also stands at its own random distance,
     * the way a group of anything really does, and that randomness has to feed
     * the size as well or it puts them back into disagreement.
     */
    var stand = 0;
    if (meta.anchor === 'ground' || meta.anchor === 'water') {
      /* A boat obeys the same rule as a stag: further out is nearer the
       * skyline and smaller. Water simply starts further down, because the
       * near shore is where you are standing. */
      var middle = meta.anchor === 'water' ? 0.33 : 0.13;
      /* A lone thing may stand anywhere between near and far. Inside a group
       * the spread has to come from the depth rather than from the roll, or
       * three of them can each roll a middling distance and the group comes
       * out standing in a line — so the roll is narrowed to a wobble about the
       * middle and the depth does the work. */
      var base = total > 1 ? middle * (0.85 + r() * 0.30)
        : (meta.anchor === 'water' ? 0.18 + r() * 0.30
          : (shot ? shot.stand : 0.06 + r() * 0.14));
      stand = base * (1 - depth * 0.45);
      /* A lone subject's framing wobbles a little; a group's must not, or the
       * wobble is bigger than the recession and the depth disappears under
       * it. */
      if (shot && total === 1 && meta.anchor === 'ground') stand += (r() - 0.5) * 0.06;
      stand = Math.max(0.015, stand);
    }
    /* A lone thing has nothing to be compared against, so its size is free;
     * inside a group it is set by where it is standing. `0.13` is the middle
     * of the range a thing can stand at, so the nearest keeps the size it
     * would have had on its own. */
    var recede = total > 1
      ? clamp(stand / (meta.anchor === 'water' ? 0.33 : 0.13), 0.4, 1.7) : 1;
    var size = Math.min(w, h) * meta.base * subject.scale * swing * recede;
    var bw = size * (meta.aspect || 1);
    var bh = size;

    var y;
    if (meta.anchor === 'sky') {
      y = h * (0.10 + r() * 0.28) + depth * h * 0.06;
    } else if (meta.anchor === 'water') {
      y = hz + (h - hz) * stand - bh;
    } else {
      var standY = hz + (h - hz) * stand;
      /* A close-up fills the frame; it does not leave the head outside it. The
       * shot says how big to be, the frame says how big will fit, and the
       * smaller of the two wins — losing a stag's antlers off the top is not a
       * close-up of a stag. */
      var room = standY - h * 0.035;
      if (bh > room && room > h * 0.1) {
        var fit = room / bh;
        bh = room;
        bw *= fit;
      }
      y = standY - bh;
      /*
       * And it must not end exactly on the skyline. A head that lands on the
       * horizon line reads as stuck to it — a tangent, the oldest mistake in
       * framing a photograph — so anything that close is moved clear, whichever
       * way is nearer.
       */
      /* Only for a lone subject. In a group, where each one stands is what
       * says how far off it is, and nudging one of them clear of the skyline
       * would put a small one lower than a big one — which is a worse mistake
       * than a tangent, because it is the wrong distance rather than an
       * awkward one. */
      var clear = h * 0.035;
      if (total === 1 && Math.abs(y - hz) < clear) {
        y = (y < hz ? hz - clear : hz + clear);
        if (y + bh > h * 1.02) y = h * 1.02 - bh;
      }
    }
    return { x: cx - bw / 2, y: y, w: bw, h: bh, depth: depth, anchor: meta.anchor };
  }

  /* A soft pool of shade where a thing meets the ground, thrown away from the
   * light rather than straight down — the give-away that a picture was lit by
   * something in particular and not by nothing. */
  /*
   * A shadow is sharp where the thing touches the ground and spreads as it runs
   * away from it — because the further from the contact point, the more of the
   * sky the ground can still see. One blob of even softness is the shape a
   * sticker casts, and it is one of the reasons things looked stuck on.
   *
   * Drawn as a run of ellipses along the direction the light throws it, each
   * further one wider, softer and fainter than the last. The first sits under
   * the feet and is nearly hard.
   */
  function groundShadow(ctx, box, P, light, spec) {
    var cx = box.x + box.w / 2, cy = box.y + box.h;
    var lean = 0;
    if (light) lean = clamp((cx - light.x) / Math.max(box.w, 1), -2.2, 2.2);
    /* A low sun throws a long shadow. */
    var reach = box.w * shadowReach(spec ? spec.sun : null);
    var steps = 6;
    /* Cloud spreads a shadow out and washes it away: under a heavy sky there
     * is hardly an edge to it at all. */
    var soft = softness(spec);

    ctx.save();
    for (var i = 0; i < steps; i++) {
      var t = i / (steps - 1);                  // 0 at the feet, 1 at the far end
      var rx = box.w * (0.30 + t * 0.58) * (1 + soft * 0.65);
      var ry = box.h * (0.030 + t * 0.055) * (1 + soft * 0.4);
      var alpha = (0.46 - t * 0.34) / (1 + t * 1.4) * (1 - soft * 0.72);
      var g = ctx.createRadialGradient(0, 0, 0, 0, 0, rx);
      /* Tight near the contact, feathered far from it. */
      g.addColorStop(0, P.silhouette(0, alpha));
      g.addColorStop(Math.max(0.05, 0.62 - t * 0.55), P.silhouette(0, alpha * 0.75));
      g.addColorStop(1, P.silhouette(0, 0));
      ctx.save();
      ctx.translate(cx + lean * (box.w * 0.10 + reach * t), cy + box.h * 0.004 * t);
      ctx.scale(1, ry / rx);
      ctx.fillStyle = g;
      ctx.beginPath();
      ctx.arc(0, 0, rx, 0, Math.PI * 2);
      ctx.fill();
      ctx.restore();
    }
    ctx.restore();
  }

  /* A palette that answers every question with one colour. Handing this to a
   * subject routine redraws the very same shapes as a flat stencil, which is
   * what the shadow, rim-light and settled-snow passes below are made of — so
   * all 42 routines gain them without knowing they exist. */
  function flat(P, colour) {
    function same() { return colour; }
    return {
      sky: P.sky, scene: P.scene, drama: P.drama, isWater: P.isWater,
      light_at: P.light_at, bend: P.bend,
      css: same, ink: same, silhouette: same, land: same, far: same,
      sea: same, light: same, haze: same, shade: same
    };
  }

  function stencil(ctx, subject, box, P, r, spec, colour, dx, dy, alpha) {
    if (!SUBJECTS || !SUBJECTS.setStencil) return;
    ctx.save();
    ctx.globalAlpha = alpha;
    ctx.translate(dx, dy);
    SUBJECTS.setStencil(true);
    SUBJECTS.draw(ctx, subject, box, flat(P, colour), r, spec);
    SUBJECTS.setStencil(false);
    ctx.restore();
  }

  /*
   * Things that are a light themselves.
   *
   * A campfire was an orange shape on a dark field. It lit nothing: not the
   * ground it was burning on, not the face of whoever was standing by it, not
   * the underside of the tree above it. The one thing everyone knows about a
   * fire at night is what it does to everything around it, and the picture had
   * none of it — which is also why every night picture had exactly one light
   * in it, the moon, no matter what was in the scene.
   *
   * `at` is where the light sits inside the thing, as a share of its box.
   * `reach` is how far it carries, in widths of that box.
   */
  var GLOWING = {
    campfire:   { hue: [26, 100, 58],  at: [0.50, 0.78], reach: 3.6, strength: 1.00 },
    lighthouse: { hue: [48, 100, 80],  at: [0.50, 0.08], reach: 4.2, strength: 0.85 },
    crystal:    { hue: [190, 92, 68],  at: [0.50, 0.46], reach: 2.6, strength: 0.70 },
    portal:     { hue: [282, 92, 66],  at: [0.50, 0.46], reach: 3.0, strength: 0.90 },
    rocket:     { hue: [28, 100, 62],  at: [0.50, 0.97], reach: 2.8, strength: 0.95 },
    ufo:        { hue: [150, 92, 62],  at: [0.50, 0.88], reach: 3.0, strength: 0.80 },
    city:       { hue: [38, 92, 62],   at: [0.50, 0.82], reach: 2.2, strength: 0.60 },
    volcano:    { hue: [14, 100, 56],  at: [0.50, 0.30], reach: 3.2, strength: 0.85 }
  };

  /*
   * What the extra lights in a picture are, and where they stand.
   *
   * A light only shows against the dark: a campfire at noon is a campfire, at
   * midnight it is the light in the picture. So every one of them is scaled by
   * how little light the sun is giving — which is the same number the ground
   * is lit by, read the other way round.
   */
  function extraLights(spec, placed, w, h) {
    var dim = clamp(1 - groundLit(spec.sun) * 0.92, 0.06, 1);
    if (dim < 0.12) return [];
    var out = [];
    placed.forEach(function (item) {
      var glow = GLOWING[item.s.draw];
      if (!glow) return;
      out.push({
        x: item.box.x + item.box.w * glow.at[0],
        y: item.box.y + item.box.h * glow.at[1],
        hue: glow.hue,
        /* In widths of the thing, but never most of the picture: a close-up of
         * a campfire is a bigger fire, not a fire that lights the county. */
        reach: Math.min(item.box.w * glow.reach, Math.max(w, h) * 0.42),
        strength: glow.strength * dim,
        box: item.box
      });
    });
    return out;
  }

  /* The pool of light a glowing thing lays down around itself. Added to what
   * is already there rather than painted over it, because that is what light
   * does — it is why a fire warms a whole clearing without hiding it. */
  function spill(ctx, w, h, hz, L, P) {
    /* Twice: a pool on the ground, which is where a light lands, and a much
     * weaker halo in the air around the source. Painting one strong gradient
     * over the whole frame lit the sky as brightly as the field, and a sky
     * that glows around a campfire is a smoke ring, not a night. */
    function lay(alpha, radius) {
      var g = ctx.createRadialGradient(L.x, L.y, 0, L.x, L.y, radius);
      /* A smooth square-law falloff in a dozen steps rather than four hand-set
       * ones: four leaves visible rings round the fire, and a ring of light is
       * something nobody has ever seen. */
      for (var i = 0; i <= 12; i++) {
        var t = i / 12;
        g.addColorStop(t, P.css(L.hue, 0.50 * alpha * Math.pow(1 - t, 2.6)));
      }
      ctx.fillStyle = g;
      ctx.fillRect(0, 0, w, h);
    }
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    ctx.save();
    ctx.beginPath();
    ctx.rect(0, Math.max(0, hz - L.reach * 0.10), w, h);
    ctx.clip();
    lay(L.strength, L.reach);
    ctx.restore();
    lay(L.strength * 0.30, L.reach * 0.55);
    ctx.restore();
  }

  /*
   * A subject made of something.
   *
   * A colour word says a dragon is red. It does not say that a stone dragon is
   * dull and chalky, that a bronze one is dark with a hard bright edge where
   * the sun catches it, or that a glass one has the sky showing through. This
   * hands the subject's own palette the material's colour, so everything drawn
   * with it — body, wings, legs, the lot — is cut from that instead of from
   * the scene's shadow colour. What the material does to the *light* is a
   * separate pass, below.
   */
  function clad(PS, material) {
    var out = {};
    for (var k in PS) out[k] = PS[k];
    function of(depth, a) { return PS.css(material.colour, a); }
    out.silhouette = of;
    out.ink = of;
    out.land = of;
    out.far = of;
    return out;
  }

  /*
   * Air with something in it.
   *
   * Light is invisible. You only ever see it when it hits something, and in a
   * clear sky between you and the sun there is nothing for it to hit — which
   * is why a shaft of light is a sign that the air is full of water or dust.
   * Put cloud, mist or smoke in a picture and the beams come out, in a fan
   * from wherever the sun is.
   *
   * It is one of the strongest things on this list for making a picture look
   * photographed rather than drawn, and it costs a dozen triangles.
   */
  function shafts(ctx, w, h, hz, P, spec, r, light) {
    if (!light || spec.scene.id === 'space' || spec.scene.id === 'cave') return;
    var haze = softness(spec);
    /* Nothing in the air, nothing to see — and nothing to shine through once
     * the sun has properly gone. */
    var lowSun = spec.sun == null ? 1 : clamp(1 - Math.abs(spec.sun) / 70, 0.15, 1);
    var strength = haze * lowSun;
    if (strength < 0.10) return;

    var reach = Math.max(w, h) * 1.5;
    var n = 7 + Math.floor(r() * 5);
    /* Fanned about the straight-down direction from the light, because that is
     * the way the beams go when the cloud is between you and the sun. */
    var middle = Math.PI / 2 + (light.x < w * 0.5 ? 0.35 : -0.35);
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    for (var i = 0; i < n; i++) {
      var a = middle + (i / (n - 1) - 0.5) * 1.5 + (r() - 0.5) * 0.12;
      var wide = 0.012 + r() * 0.045;
      var g = ctx.createLinearGradient(light.x, light.y,
        light.x + Math.cos(a) * reach, light.y + Math.sin(a) * reach);
      g.addColorStop(0, P.css(P.sky.light, 0.30 * strength));
      g.addColorStop(0.35, P.css(P.sky.light, 0.13 * strength));
      g.addColorStop(1, P.css(P.sky.light, 0));
      ctx.fillStyle = g;
      ctx.beginPath();
      ctx.moveTo(light.x, light.y);
      ctx.lineTo(light.x + Math.cos(a - wide) * reach, light.y + Math.sin(a - wide) * reach);
      ctx.lineTo(light.x + Math.cos(a + wide) * reach, light.y + Math.sin(a + wide) * reach);
      ctx.closePath();
      ctx.fill();
    }
    ctx.restore();
  }

  /*
   * The smear a light leaves on wet ground.
   *
   * Look down a road in the rain and every light in front of you is drawn out
   * into a long vertical streak, because the ground has become a bad mirror:
   * rough enough to smear the reflection, wet enough to give one at all. It
   * costs one soft gradient per light and it is the single clearest sign that
   * a picture has been rained on.
   */
  function sheen(ctx, w, h, hz, P, spec, at, hue, strength) {
    var wet = wetness(spec);
    if (wet < 0.25 || !at || hz >= h - 2) return;
    var top = Math.max(hz, 0);
    var spread = Math.min(w, h) * (0.055 + 0.05 * wet);
    var g = ctx.createLinearGradient(0, top, 0, h);
    g.addColorStop(0, P.css(hue, 0.30 * wet * strength));
    g.addColorStop(0.35, P.css(hue, 0.13 * wet * strength));
    g.addColorStop(1, P.css(hue, 0));
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    ctx.fillStyle = g;
    /* Narrow down the middle and feathered at both sides. A canvas fill takes
     * one gradient, so the sideways falloff is drawn as slices: a streak with
     * straight edges is a painted rectangle, not a reflection. */
    var slices = 11;
    for (var i = 0; i < slices; i++) {
      var t = (i + 0.5) / slices;
      var edge = Math.abs(t - 0.5) * 2;
      ctx.globalAlpha = Math.pow(1 - edge, 1.8);
      ctx.fillRect(at.x - spread + spread * 2 * (i / slices), top,
        spread * 2 / slices + 1, h - top);
    }
    ctx.restore();
  }

  /*
   * Draw one subject, lit. The order is the order a painter would work in:
   * the shape it throws away from the light, the shape itself, the edge the
   * light catches, then whatever the weather is doing to it.
   */
  function paintSubject(ctx, subject, box, P, PS, r, spec, light, hz, h, extras) {
    var cx = box.x + box.w / 2, cy = box.y + box.h / 2;
    var dx = 0, dy = 0;
    if (light) {
      var vx = cx - light.x, vy = cy - light.y;
      var len = Math.sqrt(vx * vx + vy * vy) || 1;
      dx = vx / len; dy = vy / len;
    }
    /* The shadow and the lit edge are drawn as the subject's own shape, shifted.
     * Shifted far enough and that stops reading as an edge and starts reading
     * as a second, paler animal standing behind the first — which is what a
     * giant subject got, because the shift was a share of the subject's size
     * with nothing holding it down. An edge is an edge at any size. */
    var off = Math.max(1.2, Math.min(Math.min(box.w, box.h) * 0.030, h * 0.012));

    /* Water gives it back, upside down and dimmer. */
    if (P.isWater && REFLECTS[spec.scene.id] && box.y + box.h <= h) {
      ctx.save();
      ctx.beginPath();
      ctx.rect(0, Math.max(hz, box.y + box.h), ctx.canvas ? ctx.canvas.width : box.x + box.w * 4, h);
      ctx.clip();
      ctx.globalAlpha = 0.26;
      ctx.translate(0, (box.y + box.h) * 2);
      ctx.scale(1, -1);
      SUBJECTS.draw(ctx, subject, box, P, r, spec);
      ctx.restore();
      ctx.globalAlpha = 1;
    }

    if (box.anchor === 'ground') groundShadow(ctx, box, P, light, spec);

    /*
     * How far away this thing is, which is not box.depth: that only separates
     * one subject from another when there are several, so a lone subject was
     * always at distance nought and was painted at full strength against a
     * landscape that had properly receded behind it. That mismatch is what made
     * things look stuck on rather than standing in it. Distance here is read
     * off where it stands — at the horizon it is far, at the bottom of the
     * frame it is near — which is the cue the eye actually uses.
     */
    var base = box.y + box.h;
    var far = box.anchor === 'ground'
      ? clamp(1 - (base - hz) / Math.max(h - hz, 1), 0, 1)
      : clamp(box.depth * 0.7 + 0.15, 0, 1);
    /* A dramatic picture keeps its contrast; a calm one lets the air in. */
    var air = (0.30 + far * 0.55) * (1 - P.drama * 0.45);

    /* The edge first. A shape cut out with scissors has an outline the
     * background never touches; a thing in the air has the air creeping a
     * little way into its outline. Eight small offsets of the silhouette in the
     * sky's own haze is enough to stop the edge reading as a cut. */
    var feather = Math.max(0.9, Math.min(box.w, box.h) * 0.022) * (0.5 + far);
    for (var e = 0; e < 8; e++) {
      var ea = (e / 8) * Math.PI * 2;
      stencil(ctx, subject, box, PS, r, spec, P.haze(1),
        Math.cos(ea) * feather, Math.sin(ea) * feather, 0.05 + air * 0.06);
    }

    /*
     * Coat.
     *
     * An animal drawn as a filled shape has an outline no animal has: a clean
     * curve. A real one is ragged for a centimetre all the way round, and the
     * light catches that fringe. Copies of the silhouette at small random
     * offsets, in the animal's own colour, give the edge that thickness — and
     * a few in the light's colour on the lit side give it the rim you see on
     * anything furry with the sun behind it.
     *
     * Only on things that have a coat. A castle does not, and giving one to a
     * castle would read as a mistake rather than as fur.
     */
    if (COATED[subject.draw]) {
      var tuft = Math.max(0.8, Math.min(box.w, box.h) * 0.016);
      var fr = PROMPT.rng(spec, 'coat');
      for (var c2 = 0; c2 < 18; c2++) {
        var ca = fr() * Math.PI * 2;
        var cd = tuft * (0.35 + fr() * 1.25);
        stencil(ctx, subject, box, PS, r, spec, PS.silhouette(0.1, 1),
          Math.cos(ca) * cd, Math.sin(ca) * cd, 0.055);
      }
      /* The lit fringe, tight against the light side only. */
      for (var c3 = 0; c3 < 5; c3++) {
        var jitter = (fr() - 0.5) * 0.8;
        stencil(ctx, subject, box, PS, r, spec, P.light(0.9),
          (-dx + jitter) * tuft * (0.7 + fr() * 0.8),
          (-dy + jitter) * tuft * (0.7 + fr() * 0.8), 0.075);
      }
    }

    /* Everything that shows only at the edges goes down first, and the subject
     * is drawn once on top of the lot. */
    stencil(ctx, subject, box, PS, r, spec, PS.silhouette(0.7, 1), dx * off, dy * off, 0.6);
    /* The lit edge is the sun catching one side. There is no one side to catch
     * when the light is the whole sky, so cloud takes it away. */
    stencil(ctx, subject, box, PS, r, spec, P.light(0.95), -dx * off, -dy * off,
      0.65 * (1 - softness(spec) * 0.62));
    /*
     * What the weather does to the thing itself.
     *
     * Weather was painted in front of everything — streaks of rain, flecks of
     * snow — and then stopped. It never landed. A thing standing out in the
     * rain is wet, and a thing standing out in falling snow has snow on top of
     * it, and those are the two things that make a picture look as though the
     * weather is happening to it rather than in front of it.
     */
    if (spec.weather === 'snowfall') {
      /* Snow gathers on whatever faces upwards, so a copy of the shape shifted
       * up shows only along its top edges. Three of them at different heights,
       * because snow piles unevenly and one clean line reads as a hat. */
      var deep = softness(spec) * 1.4;
      for (var sn = 0; sn < 3; sn++) {
        stencil(ctx, subject, box, PS, r, spec, P.css([205, 18, 97], 1),
          (r() - 0.5) * off * 0.7, -off * (0.7 + sn * 0.5) * (0.6 + deep),
          0.34 - sn * 0.08);
      }
    }

    var soaked = wetness(spec);
    if (soaked > 0.35) {
      /* Wet is darker and deeper, the same as wet ground — and it runs down,
       * so the bottom of a thing standing in the rain is wetter than its top. */
      var damp = ctx.createLinearGradient(0, base, 0, box.y);
      damp.addColorStop(0, P.css(P.scene.ink, 0.62));
      damp.addColorStop(0.55, P.css(P.scene.ink, 0.22));
      damp.addColorStop(1, P.css(P.scene.ink, 0.06));
      stencil(ctx, subject, box, PS, r, spec, damp, 0, 0, 0.42 * soaked);
      /* And the sky comes off it, because a wet surface is a mirror too. */
      var slick = ctx.createLinearGradient(0, box.y, 0, base);
      slick.addColorStop(0, P.css(P.sky.low, 0.55));
      slick.addColorStop(0.5, P.css(P.sky.low, 0.10));
      slick.addColorStop(1, P.css(P.sky.low, 0));
      stencil(ctx, subject, box, PS, r, spec, slick, 0, 0, 0.30 * soaked);
    }

    var made = spec.material || null;
    if (SUBJECTS) SUBJECTS.draw(ctx, subject, box, made ? clad(PS, made) : PS, r, spec);

    /*
     * The one that matters. Every subject here is drawn as flat areas of a
     * single colour, so a dragon was one red shape and a whale one dark one —
     * and a flat shape on a landscape is a cut-out however carefully the
     * landscape behind it recedes. What was missing is not air over the top of
     * it, it is the thing having a lit side and a dark side at all.
     *
     * So: a gradient run along the line from the light to the subject, clipped
     * to the subject's own shape. The side facing the light takes the light's
     * colour, the side away from it goes towards the scene's shadow, and in
     * between it turns. That is what makes a shape read as a solid rather than
     * a hole cut in paper, and it costs one more pass.
     */
    var mR = Math.max(box.w, box.h) * 0.62;
    var mdx = dx || 0.55, mdy = dy || -0.45;
    var model = ctx.createLinearGradient(
      cx - mdx * mR, cy - mdy * mR, cx + mdx * mR, cy + mdy * mR);
    /* Gently. Set strong this reads as chrome rather than as form — hard bright
     * bands down an animal's legs, a face lit like a car bonnet. Form is a
     * suggestion of where the light is, not a repaint of the subject. */
    model.addColorStop(0, P.css(P.sky.light, 0.30));
    model.addColorStop(0.44, P.css(P.sky.light, 0.03));
    model.addColorStop(0.62, 'rgba(0,0,0,0.03)');
    model.addColorStop(1, 'rgba(0,0,0,0.34)');
    /* Under cloud the lit side and the dark side draw together, because the
     * light is arriving from the whole sky rather than from one point in it. */
    stencil(ctx, subject, box, PS, r, spec, model, 0, 0,
      (0.42 + P.drama * 0.12) * (1 - softness(spec) * 0.55));

    /*
     * And every other light in the picture. A person standing by a fire is lit
     * by the fire on one side and by the night on the other; that second light
     * is most of what makes a night picture read as a photograph rather than a
     * dark drawing. Each one falls off with distance and comes from its own
     * direction, so the side facing it is the side that catches it.
     */
    (extras || []).forEach(function (L) {
      if (L.box === box) return;                 // a lamp does not light itself
      var lx = cx - L.x, ly = cy - L.y;
      var dist = Math.sqrt(lx * lx + ly * ly) || 1;
      if (dist > L.reach) return;
      var fall = 1 - dist / L.reach;
      fall *= fall;
      if (fall < 0.05) return;
      var ux = lx / dist, uy = ly / dist;
      var span = Math.max(box.w, box.h) * 0.62;
      var side = ctx.createLinearGradient(
        cx - ux * span, cy - uy * span, cx + ux * span, cy + uy * span);
      side.addColorStop(0, P.css(L.hue, 0.85));
      side.addColorStop(0.5, P.css(L.hue, 0.22));
      side.addColorStop(1, P.css(L.hue, 0));
      stencil(ctx, subject, box, PS, r, spec, side, 0, 0,
        clamp(0.62 * fall * L.strength, 0, 0.85));
    });

    /*
     * What it is made of, in light rather than in colour.
     *
     * Three passes, and which of them shows depends on the three numbers the
     * material carries. A metal is mostly a reflection of where it is — sky
     * above, ground below — which is the whole of why metal looks like metal.
     * A smooth thing has a tight bright highlight on the side facing the
     * light, and a chalky one has none at all. And something you can see
     * through takes the colour of what is behind it and lights up at its
     * edges, because light that goes in has to come out somewhere.
     */
    if (made) {
      if (made.metal > 0.02) {
        var mirror = ctx.createLinearGradient(0, box.y, 0, base);
        mirror.addColorStop(0, P.css(P.sky.mid, 0.85));
        mirror.addColorStop(0.42, P.css(P.sky.low, 0.30));
        mirror.addColorStop(0.58, P.css(P.scene.ink, 0.30));
        mirror.addColorStop(1, P.css(P.scene.land, 0.80));
        stencil(ctx, subject, box, PS, r, spec, mirror, 0, 0, 0.48 * made.metal);
      }
      /* A hundred years of weather takes the shine off anything. */
      var shine = (1 - made.rough) * (1 - (spec.age || 0) * 0.75);
      if (shine > 0.05) {
        /* Tight and bright for a polished thing, broad and faint for a dull
         * one: that width is what the eye reads as "how smooth is this". */
        var sR = Math.max(box.w, box.h) * 0.62;
        var sdx = dx || 0.55, sdy = dy || -0.45;
        var hot = ctx.createLinearGradient(
          cx - sdx * sR, cy - sdy * sR, cx + sdx * sR, cy + sdy * sR);
        var tight = 0.06 + (1 - shine) * 0.34;
        hot.addColorStop(0, P.css(P.sky.light, 0.95));
        hot.addColorStop(tight, P.css(P.sky.light, 0.28));
        hot.addColorStop(Math.min(0.92, tight + 0.28), P.css(P.sky.light, 0));
        hot.addColorStop(1, P.css(P.sky.light, 0));
        stencil(ctx, subject, box, PS, r, spec, hot, 0, 0,
          clamp(0.30 + shine * 0.55, 0, 0.9) * (1 - softness(spec) * 0.45));
      }
      if (made.clear > 0.05) {
        /* What is behind it shows through, and the edges glow where the light
         * that went in comes back out. */
        stencil(ctx, subject, box, PS, r, spec, P.haze(1), 0, 0, 0.42 * made.clear);
        var lip = Math.max(1, Math.min(box.w, box.h) * 0.020);
        for (var q = 0; q < 6; q++) {
          var qa = (q / 6) * Math.PI * 2;
          stencil(ctx, subject, box, PS, r, spec, P.css(P.sky.light, 0.8),
            Math.cos(qa) * lip, Math.sin(qa) * lip, 0.12 * made.clear);
        }
      }
    }

    /*
     * What time has done to it.
     *
     * Everything here was brand new — no streak of dirt down it, no moss at
     * its foot, nothing that had been rained on for a hundred years — and new
     * is the one thing almost nothing in the world actually is.
     *
     * Two passes, both clipped to the thing's own shape. Dirt runs downwards
     * in streaks, because that is the way rain runs, and a canvas gradient can
     * make streaks: stops laid across the shape, alternating between grime and
     * nothing, at widths that never repeat. Then growth at the foot of it,
     * where the damp is — moss on stone, verdigris on bronze, weeds against a
     * wall. Nothing gathers on the top of a thing, which is why both of these
     * read as age rather than as dirty paint.
     */
    var age = spec.age || 0;
    if (age > 0.02) {
      var grimy = PROMPT.rng(spec, 'wear');
      var streaks = ctx.createLinearGradient(box.x, 0, box.x + box.w, 0);
      var grime = [28, 14, 22];
      var at = 0;
      streaks.addColorStop(0, P.css(grime, 0));
      while (at < 0.98) {
        var gap = 0.03 + grimy() * 0.10;
        var wide = 0.012 + grimy() * 0.05;
        at = Math.min(0.98, at + gap);
        streaks.addColorStop(at, P.css(grime, 0));
        streaks.addColorStop(Math.min(0.99, at + wide * 0.35),
          P.css(grime, 0.5 + grimy() * 0.5));
        at = Math.min(0.98, at + wide);
        streaks.addColorStop(at, P.css(grime, 0));
      }
      streaks.addColorStop(1, P.css(grime, 0));
      stencil(ctx, subject, box, PS, r, spec, streaks, 0, 0, 0.46 * age);

      /* And the damp at its foot. Whatever grows there is the colour of the
       * ground it is growing out of, shifted towards green. */
      var mossy = ctx.createLinearGradient(0, base, 0, base - box.h * 0.45);
      var moss = [lerp(P.scene.land[0], 108, 0.75), 46, 24];
      mossy.addColorStop(0, P.css(moss, 0.85));
      mossy.addColorStop(0.42, P.css(moss, 0.28));
      mossy.addColorStop(1, P.css(moss, 0));
      stencil(ctx, subject, box, PS, r, spec, mossy, 0, 0, 0.72 * age);
    }

    /*
     * Then the air it is standing in: what the sky and the ground throw back at
     * it, and then how far away it is. Both after the modelling, because
     * distance flattens form — that is exactly what distance does.
     */
    var bleed = ctx.createLinearGradient(0, box.y, 0, base);
    bleed.addColorStop(0, P.css(P.sky.mid, 1));
    bleed.addColorStop(0.55, P.css(P.sky.haze, 1));
    bleed.addColorStop(1, P.css(P.scene.land, 1));
    stencil(ctx, subject, box, PS, r, spec, bleed, 0, 0, 0.10 + air * 0.16);

    /*
     * Light that bounces.
     *
     * Sunlight hitting snow throws blue up under everything standing on it; red
     * rock warms the belly of an animal above it. Nothing in the picture lit
     * anything else, which is why undersides read as simply dark rather than as
     * being in shade. The ground's own colour, thrown up into the lower part of
     * the subject and fading out before it reaches the top.
     */
    if (box.anchor === 'ground' || box.anchor === 'water') {
      /* How far up it reaches. Bounced light falls away fast with distance from
       * the ground, so what sets its height is how wide the thing standing
       * there is, not how tall: it licks the belly of a horse and the foot of a
       * castle wall, and a wash halfway up a keep is a mistake. */
      var lick = Math.min(box.h * 0.62, box.w * 0.80);
      var bounced = ctx.createLinearGradient(0, base, 0, base - lick);
      var from = box.anchor === 'water' ? (P.scene.sea || P.scene.far) : P.scene.land;
      bounced.addColorStop(0, P.css(from, 0.62));
      bounced.addColorStop(0.45, P.css(from, 0.18));
      bounced.addColorStop(1, P.css(from, 0));
      /* Weakest at noon overhead, strongest with a low sun raking the ground. */
      var raking = (spec.sun != null && spec.sun < 12 && spec.sun > -8) ? 1.25 : 1;
      /* And only as much as the ground has to give back: this is reflected
       * sunlight, so it follows the hour. A castle lit green from below at
       * midnight is the picture admitting it was drawn. */
      stencil(ctx, subject, box, PS, r, spec, bounced, 0, 0,
        0.34 * raking * groundLit(spec.sun));
    }

    if (far > 0.02) {
      stencil(ctx, subject, box, PS, r, spec, P.haze(1), 0, 0, far * 0.40 * (1 - P.drama * 0.4));
    }

    /* Where it meets the ground, the ground stops it seeing the sky. Tight and
     * dark, and quite separate from the shadow it throws, which is long and
     * goes wherever the light is not. */
    if (box.anchor === 'ground' && base <= h) {
      /* Plain darkness, not the scene's ink. P.ink takes a depth, and a depth
       * mixes the sky into the colour to push it away — so the ink asked for
       * here came back lighter than the body it was meant to be shading, and
       * quietly bleached the legs of every animal in the engine. Where a thing
       * meets the ground it is darker. That is all this is. */
      var occ = ctx.createLinearGradient(0, base - box.h * 0.16, 0, base);
      occ.addColorStop(0, 'rgba(0,0,0,0)');
      occ.addColorStop(1, 'rgba(0,0,0,0.42)');
      stencil(ctx, subject, box, PS, r, spec, occ, 0, 0, 0.6);
    }

    if (spec.weather === 'fog') {               // distance eats it, softly
      var fg = ctx.createRadialGradient(cx, cy, 0, cx, cy, Math.max(box.w, box.h) * 0.8);
      fg.addColorStop(0, P.haze(0.10 + box.depth * 0.22));
      fg.addColorStop(1, P.haze(0));
      ctx.save();
      ctx.fillStyle = fg;
      ctx.fillRect(box.x - box.w * 0.6, box.y - box.h * 0.6, box.w * 2.2, box.h * 2.2);
      ctx.restore();
    }
  }

  /* What has a coat. Fur, feathers, hair — anything whose outline is ragged
   * rather than cut. A tower is not on this list, and should not be. */
  var COATED = { quadruped: true, bird: true, humanoid: true };

  /* Where a subject's reflection makes sense: open water that runs to the
   * bottom of the frame. A beach or an island has land in the way. */
  var REFLECTS = { ocean: true, lake: true, swamp: true };

  /*
   * Something close to the viewer, at the very front. Two layers of distance
   * make a landscape; three make a photograph of one. Not every picture gets
   * it — a frame that is always framed is its own kind of sameness.
   */
  /*
   * The small things lying about.
   *
   * Ground in these pictures was a clean sheet of colour with a texture over
   * it. Real ground is covered in things: stones, tufts, sticks, shells,
   * patches of bare earth. None of them is interesting on its own — that is
   * exactly why they matter, because a surface with nothing on it reads as a
   * painted backdrop no matter how well it is shaded.
   *
   * They obey perspective rather than being sprinkled evenly: everything sits
   * between the skyline and the bottom of the frame, and what is near the
   * bottom is near the camera, so it is bigger and there is more space between
   * one and the next.
   */
  var SCATTER = {
    meadow:    { kinds: ['tuft', 'flower', 'pebble'], n: 40 },
    plains:    { kinds: ['tuft', 'pebble', 'stick'],  n: 34 },
    forest:    { kinds: ['tuft', 'stick', 'rock'],    n: 34 },
    jungle:    { kinds: ['tuft', 'leaf', 'rock'],     n: 38 },
    desert:    { kinds: ['pebble', 'rock', 'stick'],  n: 26 },
    canyon:    { kinds: ['rock', 'pebble'],           n: 28 },
    mountains: { kinds: ['rock', 'pebble', 'patch'],  n: 30 },
    snow:      { kinds: ['rock', 'patch'],            n: 14 },
    /* On a shore the sand starts well down the frame and everything above it
     * is water, so the shells and pebbles start there too — a pebble lying on
     * the open sea is not a small thing lying about, it is a mistake. */
    shore:     { kinds: ['pebble', 'shell', 'weed'],  n: 32, from: 0.62 },
    swamp:     { kinds: ['weed', 'stick', 'tuft'],    n: 30, from: 0.35 },
    ruins:     { kinds: ['rock', 'pebble', 'stick'],  n: 34 },
    road:      { kinds: ['pebble', 'stick'],          n: 22 },
    city:      { kinds: ['pebble', 'stick'],          n: 18 },
    volcano:   { kinds: ['rock', 'pebble'],           n: 30 },
    cave:      { kinds: ['rock', 'pebble'],           n: 24 }
  };

  function scatter(ctx, w, h, hz, P, spec, r) {
    var kit = SCATTER[spec.scene.id];
    if (!kit || hz >= h - 4) return;
    var dark = P.silhouette(0.05, 0.55);
    var mid = P.land(0.1, 0.7);
    var pale = P.css(P.sky.light, 0.35);
    var count = Math.round(kit.n * ((P.detail || 1) * 0.5 + 0.5));

    for (var i = 0; i < count; i++) {
      /* Squared, so they crowd towards the skyline the way a receding plane
       * makes everything crowd towards it. */
      var t = r();
      var from = kit.from || 0;
      var y = hz + (h - hz) * (from + (1 - from) * t * t);
      var near = (y - hz) / Math.max(h - hz, 1);      // 0 at the skyline, 1 at your feet
      var x = r() * w;
      var size = Math.max(0.8, Math.min(w, h) * 0.004 * (0.25 + near * 2.4));
      var kind = kit.kinds[Math.floor(r() * kit.kinds.length) % kit.kinds.length];

      if (kind === 'tuft' || kind === 'weed') {
        var blades = kind === 'weed' ? 5 : 3;
        ctx.strokeStyle = dark;
        ctx.lineWidth = Math.max(0.6, size * 0.22);
        ctx.beginPath();
        for (var b = 0; b < blades; b++) {
          var lean = (r() - 0.5) * size * 2.2;
          ctx.moveTo(x, y);
          ctx.quadraticCurveTo(x + lean * 0.4, y - size * 1.6, x + lean, y - size * 2.8);
        }
        ctx.stroke();
      } else if (kind === 'flower') {
        ctx.fillStyle = pale;
        ctx.beginPath();
        ctx.arc(x, y - size * 1.2, size * 0.5, 0, Math.PI * 2);
        ctx.fill();
        ctx.strokeStyle = dark;
        ctx.lineWidth = Math.max(0.5, size * 0.16);
        ctx.beginPath();
        ctx.moveTo(x, y);
        ctx.lineTo(x, y - size * 1.1);
        ctx.stroke();
      } else if (kind === 'stick') {
        var a = (r() - 0.5) * 1.2;
        ctx.strokeStyle = dark;
        ctx.lineWidth = Math.max(0.6, size * 0.30);
        ctx.beginPath();
        ctx.moveTo(x - Math.cos(a) * size * 1.6, y - Math.sin(a) * size * 0.6);
        ctx.lineTo(x + Math.cos(a) * size * 1.6, y + Math.sin(a) * size * 0.6);
        ctx.stroke();
      } else if (kind === 'patch') {
        blob(ctx, x, y, size * 3.2, size * 1.1, 8, r, mid);
      } else if (kind === 'leaf') {
        blob(ctx, x, y, size * 1.4, size * 0.7, 7, r, dark);
      } else if (kind === 'shell') {
        blob(ctx, x, y, size * 0.9, size * 0.6, 7, r, pale);
      } else {
        /* A stone: a lump with a lit top and a shadow where it meets the
         * ground, which is the difference between a stone and a dot. */
        var rx = size * (kind === 'rock' ? 2.2 : 1.1);
        var ry = rx * (0.45 + r() * 0.30);
        ctx.globalAlpha = 0.5;
        blob(ctx, x + rx * 0.2, y + ry * 0.55, rx * 1.15, ry * 0.5, 7, r, dark);
        ctx.globalAlpha = 1;
        blob(ctx, x, y, rx, ry, 8, r, mid);
        ctx.globalAlpha = 0.45;
        blob(ctx, x - rx * 0.16, y - ry * 0.34, rx * 0.6, ry * 0.42, 7, r, pale);
        ctx.globalAlpha = 1;
      }
    }
  }

  var FRAMED = {
    forest: 'leaves', jungle: 'leaves', meadow: 'grass', plains: 'grass',
    swamp: 'reeds', mountains: 'rocks', canyon: 'rocks', desert: 'rocks',
    snow: 'rocks', shore: 'rocks', ruins: 'rocks', lake: 'reeds'
  };

  function foreground(ctx, w, h, hz, P, spec, r) {
    var kind = FRAMED[spec.scene.id];
    if (!kind || r() > 0.55) return;
    var ink = P.silhouette(0, 0.92);

    if (kind === 'leaves') {                    // a branch across one top corner
      var left = r() < 0.5;
      var ox = left ? 0 : w;
      var dir = left ? 1 : -1;
      ctx.strokeStyle = ink;
      ctx.lineCap = 'round';
      ctx.lineWidth = Math.max(2, w * 0.012);
      ctx.beginPath();
      ctx.moveTo(ox, -h * 0.02);
      ctx.quadraticCurveTo(ox + dir * w * 0.26, h * 0.10, ox + dir * w * 0.52, h * 0.05);
      ctx.stroke();
      for (var i = 0; i < 9; i++) {
        var t = 0.15 + i * 0.095;
        var lx = ox + dir * w * 0.52 * t;
        var ly = h * (0.02 + Math.sin(t * 3) * 0.05);
        blob(ctx, lx, ly + h * 0.035, w * 0.045, h * 0.028, 8, r, ink);
      }
    } else if (kind === 'grass' || kind === 'reeds') {
      var tall = kind === 'reeds' ? 0.30 : 0.16;
      for (var g = 0; g < 46; g++) {
        var x = r() * w;
        var gh = h * tall * (0.5 + r() * 0.9);
        ctx.strokeStyle = ink;
        ctx.lineWidth = Math.max(1.5, w * (kind === 'reeds' ? 0.004 : 0.003));
        ctx.beginPath();
        ctx.moveTo(x, h);
        ctx.quadraticCurveTo(x + (r() - 0.5) * w * 0.03, h - gh * 0.6,
          x + (r() - 0.5) * w * 0.06, h - gh);
        ctx.stroke();
      }
    } else {                                    // a rock shelf along the bottom
      var side = r() < 0.5 ? 0 : 1;
      ctx.beginPath();
      ctx.moveTo(side ? w : 0, h);
      ctx.lineTo(side ? w : 0, h * (0.80 + r() * 0.08));
      for (var k = 0; k <= 6; k++) {
        var kx = (side ? w : 0) + (side ? -1 : 1) * w * (k / 6) * (0.30 + r() * 0.16);
        ctx.lineTo(kx, h * (0.84 + r() * 0.14));
      }
      ctx.lineTo(side ? w * 0.55 : w * 0.45, h);
      ctx.closePath();
      ctx.fillStyle = ink;
      ctx.fill();
    }
  }

  /*
   * Put the first-named subject where the sentence said, relative to the
   * second. "A cat under a tree" is a different picture from "a cat and a
   * tree", and until now they were the same one.
   */
  function arrange(relation, placed, w, h) {
    var anchor = placed[0].box;                      // the companion
    var movers = placed.slice(1);
    movers.forEach(function (m) {
      var b = m.box;
      var cx = anchor.x + anchor.w / 2;
      switch (relation.id) {
        case 'under':
          b.x = cx - b.w / 2 + (b.x - cx) * 0.15;
          b.y = Math.max(anchor.y + anchor.h * 0.55, b.y);
          m.z = 2;                                   // nearer than what it is under
          break;
        case 'above':
          b.x = cx - b.w / 2 + (b.x - cx) * 0.15;
          b.y = anchor.y - b.h * 0.85;
          m.z = 2;
          break;
        case 'behind':
          b.x = cx - b.w * 0.35;
          b.y -= b.h * 0.18;
          b.w *= 0.8; b.h *= 0.8;
          b.depth = Math.min(1, (b.depth || 0) + 0.35);
          m.z = -1;                                  // drawn first, so it sits behind
          break;
        case 'front':
          b.w *= 1.18; b.h *= 1.18;
          b.y = anchor.y + anchor.h - b.h + h * 0.03;
          m.z = 3;
          break;
        default:                                     // beside
          b.x = anchor.x + anchor.w * 1.15;
          if (b.x + b.w > w * 0.96) b.x = anchor.x - b.w * 1.15;
          b.x = clamp(b.x, w * 0.02, w - b.w - w * 0.02);
          break;
      }
    });
  }

  /* ----------------------------------------------------------------- render */

  /* Draw a photograph to fill the frame without squashing it. */
  function coverDraw(ctx, media, w, h) {
    var iw = media.width || media.videoWidth || w;
    var ih = media.height || media.videoHeight || h;
    var scale = Math.max(w / iw, h / ih);
    var dw = iw * scale, dh = ih * scale;
    ctx.drawImage(media, (w - dw) / 2, (h - dh) / 2, dw, dh);
  }

  function render(ctx, w, h, spec, media) {
    var P = makePalette(spec);                              // the world
    var PS = makePalette(spec, { tintStrength: 0.85 });     // the thing in it
    var r = PROMPT.rng(spec, 'scene');
    /* Where the picture is taken from. A close-up looks up at its subject, so
     * the skyline rises; looking down on a landscape drops it. */
    var shot = spec.shot || null;
    /* Underground there is no sky to put air between anything. */
    HAZE_EDGE = spec.scene.id === 'cave'
      ? null
      : { on: P.haze(0.30 - P.drama * 0.12), off: P.haze(0) };
    /*
     * How close the camera is, and therefore how fine the detail should be.
     *
     * A mountain a mile off is a silhouette; the same mountain from its foot is
     * rock faces and boulders and individual trees. Grain already gets denser on
     * a close-up simply because the same ground covers more pixels — what was
     * missing is that it should also get *finer*, and that there should be more
     * kinds of it. One number, read off the framing the picture already has,
     * and every dressing pass takes its lead from it.
     */
    var CLOSENESS = { closeup: 2.1, near: 1.5, low: 1.35, wide: 0.55, aerial: 0.65 };
    P.detail = shot ? (CLOSENESS[shot.id] || 1) : 1;
    GRAIN = { w: w, h: h, P: P, r: PROMPT.rng(spec, 'grain'), detail: P.detail };
    var hz = skylineAt(spec, shot, r() - 0.5) * h;

    /* Painting onto a photograph: the photograph is the sky and the ground, so
     * neither is drawn. Everything after this — the subject, its shadow, its
     * lit edge, the weather, the style — happens on top of it exactly as it
     * would on a painted scene. */
    var onPhoto = !!(media && media.backdrop && spec.photo && spec.photo.use &&
      spec.photo.use.backdrop);

    ctx.save();
    ctx.setTransform(1, 0, 0, 1, 0, 0);

    var light;
    if (onPhoto) {
      coverDraw(ctx, media.backdrop, w, h);
      hz = (spec.photo.skyline && spec.photo.skyline.confidence > 0.35)
        ? spec.photo.skyline.mean * h
        : h * 0.62;
      light = spec.photo.light
        ? { x: spec.photo.light.x * w, y: spec.photo.light.y * h, r: Math.min(w, h) * 0.06 }
        : null;
      P.light_at = PS.light_at = light;
    } else {
      ctx.fillStyle = P.css(P.sky.top);
      ctx.fillRect(0, 0, w, h);

      paintSky(ctx, w, h, hz, P, spec, PROMPT.rng(spec, 'sky'));
      light = paintLight(ctx, w, h, hz, P, spec, PROMPT.rng(spec, 'light'));
      P.light_at = PS.light_at = light;
      clouds(ctx, w, h, hz, P, spec, PROMPT.rng(spec, 'cloud'), light);

      /* Beams, before the ground: they are in the air between you and it, and
       * they land on it rather than lying over the top of everything. */
      shafts(ctx, w, h, hz, P, spec, PROMPT.rng(spec, 'shafts'), light);

      (GROUND[spec.scene.id] || GROUND.plains)(ctx, w, h, hz, P, spec, PROMPT.rng(spec, 'ground'), light);
    }

    /* The small things lying on the ground, before anything stands on it. */
    if (!onPhoto) scatter(ctx, w, h, hz, P, spec, PROMPT.rng(spec, 'scatter'));

    /* Subjects, furthest first so a nearer one overlaps it. */
    var sr = PROMPT.rng(spec, 'subject');
    var placed = [];
    if (spec.companion) {
      placed.push({ s: spec.companion, box: placeBox(w, h, hz, spec, spec.companion, 0, 1, sr), z: 0 });
    }
    if (spec.subject) {
      for (var i = spec.subject.count - 1; i >= 0; i--) {
        placed.push({
          s: spec.subject, z: 1,
          box: placeBox(w, h, hz, spec, spec.subject, i, spec.subject.count, sr)
        });
      }
    }
    if (spec.relation && placed.length > 1) arrange(spec.relation, placed, w, h);
    placed.sort(function (a, b) { return a.z - b.z; });

    /* Anything in the picture that is a light in its own right. The pool it
     * throws goes down before the things standing in it, so they are standing
     * in it rather than in front of it. */
    var extras = extraLights(spec, placed, w, h);
    extras.forEach(function (L) { spill(ctx, w, h, hz, L, P); });

    /* Wet ground is a bad mirror, so every light in the picture is drawn down
     * it in a long streak — the sun or the moon first, then anything else. */
    if (!onPhoto && light) sheen(ctx, w, h, hz, P, spec, light, P.sky.light, 1);
    extras.forEach(function (L) {
      sheen(ctx, w, h, hz, P, spec, L, L.hue, 0.9 * L.strength);
    });

    /* Parts belong to the thing the sentence is about. "A haloed knight by a
     * campfire" is a knight with a halo standing next to an ordinary fire, and
     * putting one over the fire as well is the picture misreading the
     * sentence. */
    var plain = null;
    if (spec.companion && spec.parts && spec.parts.length) {
      plain = {};
      for (var key in spec) plain[key] = spec[key];
      plain.parts = [];
    }
    placed.forEach(function (item) {
      var mine = (plain && item.s === spec.companion) ? plain : spec;
      paintSubject(ctx, item.s, item.box, P, PS, sr, mine, light, hz, h, extras);
    });

    if (!onPhoto) foreground(ctx, w, h, hz, P, spec, PROMPT.rng(spec, 'fore'));

    paintWeather(ctx, w, h, hz, P, spec, PROMPT.rng(spec, 'weather'));

    /*
     * Where the lens is focused, and how shallowly.
     *
     * A real camera has one distance sharp and everything else soft, and how
     * soft depends on how close you are: a close-up is shallow, a landscape is
     * sharp front to back. Both of those come free from the framing the picture
     * was already given.
     *
     * Depth is taken as height in the frame, which is not depth but is a good
     * stand-in for it in a landscape: things near the horizon are far, things
     * at the bottom of the frame are near. It is wrong for a bird in the sky,
     * which is why the subject sets the focus when there is one — the thing
     * asked for is the thing that should be sharp.
     */
    var focused = placed.length ? placed[placed.length - 1].box : null;
    var focusY = focused ? (focused.y + focused.h * 0.5) / h : hz / h;
    /* How deep the sharp band is. It has to be at least as deep as the thing
     * being focused on, or the lens is focused on the middle of an animal and
     * blurs its own head and feet — which is not shallow focus, it is a mistake.
     * Everything past it falls away. */
    var focusReach = focused
      ? clamp((focused.h * 0.75) / h, 0.16, 0.9)
      : 0.45;
    var DEPTH_OF_FIELD = { closeup: 0.72, near: 0.5, low: 0.42, wide: 0.14, aerial: 0.18 };
    P.focus = {
      y: clamp(focusY, 0, 1),
      reach: focusReach,
      strength: shot ? (DEPTH_OF_FIELD[shot.id] || 0.26) : 0.26
    };
    ctx.restore();
    return P;
  }

  var API = {
    render: render,
    COATED: COATED,
    grainIn: grainIn,
    makePalette: makePalette,
    GROUND: GROUND,
    SKY: SKY,
    SCENE_COLOUR: SCENE_COLOUR,
    photoRidge: photoRidge,
    coverDraw: coverDraw,
    groundShadow: groundShadow,
    skyAt: skyAt,
    groundLit: groundLit,
    shadowReach: shadowReach,
    softness: softness,
    wetness: wetness,
    clad: clad,
    clouds: clouds,
    shafts: shafts,
    CLOUD_KINDS: CLOUD_KINDS,
    extraLights: extraLights,
    GLOWING: GLOWING,
    paintSubject: paintSubject,
    arrange: arrange,
    placeBox: placeBox,
    skylineAt: skylineAt,
    foreground: foreground,
    scatter: scatter,
    SCATTER: SCATTER,
    helpers: { ridge: ridge, fillPoly: fillPoly, hills: hills, pine: pine, blob: blob, clamp: clamp, lerp: lerp }
  };

  root.CodaPaint = API;
  if (typeof module !== 'undefined' && module.exports) module.exports = API;
})(typeof window !== 'undefined' ? window : this);
