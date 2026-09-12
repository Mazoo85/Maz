/*
 * CODA PICS — the subjects.
 * -------------------------
 * Everything that can be the *thing* in a picture, drawn as a silhouette with
 * a rim of the scene's own light on it. There are more subjects than routines
 * on purpose: a fox and a wolf are one animal with different ears, an owl and
 * an eagle one bird with different wings, so the shared builders (quadruped,
 * bird, humanoid, tree) carry a `form` and the lexicon points at them.
 *
 *   CodaSubjects.draw(ctx, subject, box, palette, rng, spec)
 *
 * `box` is the rectangle the painter has already chosen (see paint.js
 * placeBox); every routine draws inside it, so scale, distance and placement
 * are none of its business.
 *
 * META tells the painter how a thing exists in the world — whether it stands
 * on the ground, floats in the sky or sits in the water, and how big it is
 * relative to the frame.
 *
 * Exposed as window.CodaSubjects (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  var META = {
    dragon:     { anchor: 'sky',    base: 0.44, aspect: 1.7 },
    whale:      { anchor: 'water',  base: 0.34, aspect: 2.0 },
    jellyfish:  { anchor: 'sky',    base: 0.32, aspect: 0.8 },
    fish:       { anchor: 'water',  base: 0.28, aspect: 1.8 },
    serpent:    { anchor: 'ground', base: 0.36, aspect: 1.5 },
    butterfly:  { anchor: 'sky',    base: 0.28, aspect: 1.3 },
    crab:       { anchor: 'ground', base: 0.24, aspect: 1.5 },
    quadruped:  { anchor: 'ground', base: 0.34, aspect: 1.5 },
    bird:       { anchor: 'sky',    base: 0.31, aspect: 1.8 },
    humanoid:   { anchor: 'ground', base: 0.36, aspect: 0.46 },
    castle:     { anchor: 'ground', base: 0.44, aspect: 1.1 },
    tower:      { anchor: 'ground', base: 0.52, aspect: 0.38 },
    lighthouse: { anchor: 'ground', base: 0.48, aspect: 0.42 },
    cabin:      { anchor: 'ground', base: 0.30, aspect: 1.3 },
    temple:     { anchor: 'ground', base: 0.36, aspect: 1.2 },
    torii:      { anchor: 'ground', base: 0.36, aspect: 1.1 },
    pyramid:    { anchor: 'ground', base: 0.38, aspect: 1.6 },
    windmill:   { anchor: 'ground', base: 0.44, aspect: 0.9 },
    bridge:     { anchor: 'ground', base: 0.30, aspect: 2.2 },
    city:       { anchor: 'ground', base: 0.40, aspect: 2.2 },
    ruins:      { anchor: 'ground', base: 0.30, aspect: 1.8 },
    ship:       { anchor: 'water',  base: 0.38, aspect: 1.0 },
    rocket:     { anchor: 'sky',    base: 0.46, aspect: 0.42 },
    balloon:    { anchor: 'sky',    base: 0.38, aspect: 0.72 },
    ufo:        { anchor: 'sky',    base: 0.20, aspect: 2.2 },
    train:      { anchor: 'ground', base: 0.24, aspect: 2.6 },
    car:        { anchor: 'ground', base: 0.20, aspect: 2.3 },
    portal:     { anchor: 'ground', base: 0.46, aspect: 0.7 },
    sword:      { anchor: 'ground', base: 0.36, aspect: 0.4 },
    campfire:   { anchor: 'ground', base: 0.26, aspect: 1.3 },
    crystal:    { anchor: 'ground', base: 0.36, aspect: 0.8 },
    skull:      { anchor: 'ground', base: 0.32, aspect: 1.1 },
    tree:       { anchor: 'ground', base: 0.46, aspect: 0.95 },
    cactus:     { anchor: 'ground', base: 0.34, aspect: 0.65 },
    flower:     { anchor: 'ground', base: 0.34, aspect: 0.55 },
    mushroom:   { anchor: 'ground', base: 0.28, aspect: 1.2 },
    waterfall:  { anchor: 'ground', base: 0.52, aspect: 0.6 },
    peak:       { anchor: 'ground', base: 0.58, aspect: 1.5 },
    island:     { anchor: 'sky',    base: 0.32, aspect: 1.6 },
    planet:     { anchor: 'sky',    base: 0.38, aspect: 1.5 },
    bigmoon:    { anchor: 'sky',    base: 0.40, aspect: 1.0 },
    eye:        { anchor: 'sky',    base: 0.32, aspect: 1.7 }
  };

  /* ------------------------------------------------------------------ paint
   * Every routine works from these, so the whole cast shares one light.
   */
  function ink(P, b, a) { return P.silhouette(0.08 + b.depth * 0.4, a); }
  function rim(P, a) { return P.light(a == null ? 0.85 : a); }

  function poly(ctx, pts, style) {
    ctx.beginPath();
    ctx.moveTo(pts[0][0], pts[0][1]);
    for (var i = 1; i < pts.length; i++) ctx.lineTo(pts[i][0], pts[i][1]);
    ctx.closePath();
    ctx.fillStyle = style;
    ctx.fill();
  }

  function ellipse(ctx, x, y, rx, ry, style, rot) {
    ctx.save();
    ctx.translate(x, y);
    if (rot) ctx.rotate(rot);
    ctx.beginPath();
    ctx.moveTo(rx, 0);
    for (var a = 0; a <= Math.PI * 2 + 0.01; a += Math.PI / 24) {
      ctx.lineTo(Math.cos(a) * rx, Math.sin(a) * ry);
    }
    ctx.closePath();
    ctx.fillStyle = style;
    ctx.fill();
    ctx.restore();
  }

  /* A soft halo — what makes a dragon at dusk read as lit from behind. */
  function glow(ctx, x, y, rad, style) {
    var g = ctx.createRadialGradient(x, y, 0, x, y, rad);
    g.addColorStop(0, style);
    g.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = g;
    ctx.fillRect(x - rad, y - rad, rad * 2, rad * 2);
  }

  var DRAW = {};

  /* ================================================================ animals */

  /* Wing, body, neck, tail — a dragon in six curves. */
  DRAW.dragon = function (ctx, b, P, r) {
    var cx = b.x + b.w * 0.5, cy = b.y + b.h * 0.55;
    var col = ink(P, b);
    glow(ctx, cx, cy, b.w * 0.8, P.light(0.16));

    ctx.fillStyle = col;
    ctx.beginPath();                                   // body
    ctx.moveTo(cx - b.w * 0.26, cy);
    ctx.quadraticCurveTo(cx, cy - b.h * 0.16, cx + b.w * 0.20, cy - b.h * 0.02);
    ctx.quadraticCurveTo(cx, cy + b.h * 0.14, cx - b.w * 0.26, cy);
    ctx.fill();

    ctx.beginPath();                                   // tail, whipping back
    ctx.moveTo(cx - b.w * 0.22, cy - b.h * 0.02);
    ctx.quadraticCurveTo(cx - b.w * 0.48, cy + b.h * 0.16, cx - b.w * 0.5, cy + b.h * 0.34);
    ctx.lineTo(cx - b.w * 0.40, cy + b.h * 0.22);
    ctx.quadraticCurveTo(cx - b.w * 0.34, cy + b.h * 0.10, cx - b.w * 0.20, cy + b.h * 0.06);
    ctx.closePath();
    ctx.fill();

    ctx.beginPath();                                   // neck and head
    ctx.moveTo(cx + b.w * 0.14, cy - b.h * 0.04);
    ctx.quadraticCurveTo(cx + b.w * 0.34, cy - b.h * 0.22, cx + b.w * 0.46, cy - b.h * 0.30);
    ctx.lineTo(cx + b.w * 0.5, cy - b.h * 0.24);
    ctx.quadraticCurveTo(cx + b.w * 0.36, cy - b.h * 0.12, cx + b.w * 0.18, cy + b.h * 0.03);
    ctx.closePath();
    ctx.fill();
    poly(ctx, [                                        // jaw + horn
      [cx + b.w * 0.44, cy - b.h * 0.32], [cx + b.w * 0.56, cy - b.h * 0.26],
      [cx + b.w * 0.44, cy - b.h * 0.21]
    ], col);
    poly(ctx, [
      [cx + b.w * 0.42, cy - b.h * 0.33], [cx + b.w * 0.46, cy - b.h * 0.46],
      [cx + b.w * 0.36, cy - b.h * 0.34]
    ], col);

    [1, -1].forEach(function (s) {                     // two wings, one nearer
      ctx.beginPath();
      ctx.moveTo(cx - b.w * 0.02, cy - b.h * 0.04);
      ctx.quadraticCurveTo(cx - b.w * 0.10, cy - b.h * (0.52 * s > 0 ? 0.52 : 0.34),
        cx + b.w * 0.22 * s, cy - b.h * 0.46);
      ctx.quadraticCurveTo(cx + b.w * 0.06 * s, cy - b.h * 0.26, cx + b.w * 0.26 * s, cy - b.h * 0.18);
      ctx.quadraticCurveTo(cx + b.w * 0.06 * s, cy - b.h * 0.12, cx - b.w * 0.02, cy - b.h * 0.02);
      ctx.closePath();
      ctx.fillStyle = s > 0 ? col : P.ink(0.3 + b.depth * 0.3);
      ctx.fill();
    });

    ctx.fillStyle = rim(P, 0.9);                       // eye
    ctx.beginPath();
    ctx.arc(cx + b.w * 0.45, cy - b.h * 0.30, Math.max(1, b.w * 0.012), 0, Math.PI * 2);
    ctx.fill();
  };

  DRAW.whale = function (ctx, b, P, r) {
    var cx = b.x + b.w * 0.5, cy = b.y + b.h * 0.6, col = ink(P, b);
    ctx.fillStyle = col;
    ctx.beginPath();
    ctx.moveTo(cx - b.w * 0.46, cy);
    ctx.quadraticCurveTo(cx - b.w * 0.1, cy - b.h * 0.46, cx + b.w * 0.34, cy - b.h * 0.10);
    ctx.quadraticCurveTo(cx + b.w * 0.10, cy + b.h * 0.26, cx - b.w * 0.46, cy);
    ctx.closePath();
    ctx.fill();
    poly(ctx, [                                        // fluke
      [cx - b.w * 0.40, cy - b.h * 0.02], [cx - b.w * 0.56, cy - b.h * 0.30],
      [cx - b.w * 0.46, cy + b.h * 0.02], [cx - b.w * 0.54, cy + b.h * 0.22]
    ], col);
    poly(ctx, [                                        // pectoral fin
      [cx + b.w * 0.02, cy + b.h * 0.10], [cx + b.w * 0.16, cy + b.h * 0.34],
      [cx + b.w * 0.18, cy + b.h * 0.08]
    ], col);
    ctx.strokeStyle = P.light(0.35);                   // belly pleats
    ctx.lineWidth = Math.max(1, b.h * 0.012);
    for (var i = 0; i < 5; i++) {
      ctx.beginPath();
      ctx.moveTo(cx - b.w * 0.02 + i * b.w * 0.06, cy + b.h * 0.06);
      ctx.lineTo(cx + b.w * 0.06 + i * b.w * 0.06, cy + b.h * 0.16);
      ctx.stroke();
    }
    ctx.fillStyle = rim(P, 0.8);
    ctx.beginPath();
    ctx.arc(cx + b.w * 0.24, cy - b.h * 0.08, Math.max(1, b.w * 0.014), 0, Math.PI * 2);
    ctx.fill();
  };

  DRAW.jellyfish = function (ctx, b, P, r) {
    var cx = b.x + b.w * 0.5, cy = b.y + b.h * 0.3;
    glow(ctx, cx, cy, b.w * 1.1, P.light(0.18));
    ctx.beginPath();
    ctx.moveTo(cx - b.w * 0.4, cy);
    ctx.quadraticCurveTo(cx, cy - b.h * 0.6, cx + b.w * 0.4, cy);
    ctx.quadraticCurveTo(cx + b.w * 0.2, cy + b.h * 0.1, cx, cy + b.h * 0.06);
    ctx.quadraticCurveTo(cx - b.w * 0.2, cy + b.h * 0.1, cx - b.w * 0.4, cy);
    ctx.closePath();
    ctx.fillStyle = P.light(0.55);
    ctx.fill();
    ctx.strokeStyle = P.light(0.45);
    for (var i = 0; i < 9; i++) {
      var x = cx + (i / 8 - 0.5) * b.w * 0.64;
      ctx.lineWidth = Math.max(1, b.w * 0.012);
      ctx.beginPath();
      ctx.moveTo(x, cy + b.h * 0.05);
      ctx.quadraticCurveTo(x + (r() - 0.5) * b.w * 0.2, cy + b.h * 0.4,
        x + (r() - 0.5) * b.w * 0.3, cy + b.h * (0.5 + r() * 0.3));
      ctx.stroke();
    }
  };

  DRAW.fish = function (ctx, b, P, r) {
    var cx = b.x + b.w * 0.5, cy = b.y + b.h * 0.5, col = ink(P, b);
    ctx.fillStyle = col;
    ctx.beginPath();
    ctx.moveTo(cx - b.w * 0.3, cy);
    ctx.quadraticCurveTo(cx, cy - b.h * 0.42, cx + b.w * 0.4, cy);
    ctx.quadraticCurveTo(cx, cy + b.h * 0.42, cx - b.w * 0.3, cy);
    ctx.closePath();
    ctx.fill();
    poly(ctx, [[cx - b.w * 0.26, cy], [cx - b.w * 0.5, cy - b.h * 0.3],
      [cx - b.w * 0.44, cy], [cx - b.w * 0.5, cy + b.h * 0.3]], col);
    poly(ctx, [[cx - b.w * 0.05, cy - b.h * 0.22], [cx + b.w * 0.08, cy - b.h * 0.46],
      [cx + b.w * 0.16, cy - b.h * 0.14]], col);
    ctx.fillStyle = rim(P, 0.85);
    ctx.beginPath();
    ctx.arc(cx + b.w * 0.28, cy - b.h * 0.04, Math.max(1, b.w * 0.02), 0, Math.PI * 2);
    ctx.fill();
  };

  DRAW.serpent = function (ctx, b, P, r) {
    var col = ink(P, b);
    ctx.strokeStyle = col;
    ctx.lineCap = 'round';
    ctx.lineWidth = b.h * 0.16;
    ctx.beginPath();
    ctx.moveTo(b.x, b.y + b.h * 0.95);
    ctx.bezierCurveTo(b.x + b.w * 0.25, b.y + b.h * 0.55, b.x + b.w * 0.7, b.y + b.h * 1.0,
      b.x + b.w * 0.78, b.y + b.h * 0.45);
    ctx.stroke();
    ctx.lineWidth = b.h * 0.10;
    ctx.beginPath();
    ctx.moveTo(b.x + b.w * 0.78, b.y + b.h * 0.45);
    ctx.quadraticCurveTo(b.x + b.w * 0.84, b.y + b.h * 0.10, b.x + b.w * 0.62, b.y + b.h * 0.06);
    ctx.stroke();
    ellipse(ctx, b.x + b.w * 0.58, b.y + b.h * 0.06, b.w * 0.12, b.h * 0.07, col);
    ctx.strokeStyle = rim(P, 0.8);
    ctx.lineWidth = Math.max(1, b.h * 0.012);
    ctx.beginPath();                                   // forked tongue
    ctx.moveTo(b.x + b.w * 0.47, b.y + b.h * 0.06);
    ctx.lineTo(b.x + b.w * 0.36, b.y + b.h * 0.03);
    ctx.stroke();
  };

  DRAW.butterfly = function (ctx, b, P, r) {
    var cx = b.x + b.w * 0.5, cy = b.y + b.h * 0.5, col = ink(P, b);
    [1, -1].forEach(function (s) {
      ctx.fillStyle = s > 0 ? col : P.ink(0.25 + b.depth * 0.3);
      ctx.beginPath();
      ctx.moveTo(cx, cy);
      ctx.quadraticCurveTo(cx + b.w * 0.5 * s, cy - b.h * 0.62, cx + b.w * 0.44 * s, cy - b.h * 0.06);
      ctx.quadraticCurveTo(cx + b.w * 0.42 * s, cy + b.h * 0.46, cx, cy + b.h * 0.06);
      ctx.closePath();
      ctx.fill();
      ctx.fillStyle = P.light(0.4);
      ellipse(ctx, cx + b.w * 0.3 * s, cy - b.h * 0.2, b.w * 0.07, b.h * 0.10, P.light(0.5));
    });
    ellipse(ctx, cx, cy, b.w * 0.03, b.h * 0.3, col);
  };

  DRAW.crab = function (ctx, b, P, r) {
    var cx = b.x + b.w * 0.5, cy = b.y + b.h * 0.62, col = ink(P, b);
    ellipse(ctx, cx, cy, b.w * 0.3, b.h * 0.24, col);
    ctx.strokeStyle = col;
    ctx.lineWidth = Math.max(1.5, b.h * 0.05);
    for (var i = 0; i < 3; i++) {
      [1, -1].forEach(function (s) {
        ctx.beginPath();
        ctx.moveTo(cx + b.w * 0.22 * s, cy + b.h * 0.05);
        ctx.quadraticCurveTo(cx + b.w * (0.4 + i * 0.06) * s, cy + b.h * 0.1,
          cx + b.w * (0.34 + i * 0.08) * s, cy + b.h * 0.36);
        ctx.stroke();
      });
    }
    [1, -1].forEach(function (s) {                     // claws
      ctx.beginPath();
      ctx.moveTo(cx + b.w * 0.24 * s, cy - b.h * 0.1);
      ctx.lineTo(cx + b.w * 0.44 * s, cy - b.h * 0.34);
      ctx.stroke();
      poly(ctx, [[cx + b.w * 0.40 * s, cy - b.h * 0.3], [cx + b.w * 0.56 * s, cy - b.h * 0.46],
        [cx + b.w * 0.5 * s, cy - b.h * 0.24]], col);
    });
    ctx.fillStyle = rim(P, 0.8);
    [-1, 1].forEach(function (s) {
      ctx.beginPath();
      ctx.arc(cx + b.w * 0.1 * s, cy - b.h * 0.14, Math.max(1, b.w * 0.022), 0, Math.PI * 2);
      ctx.fill();
    });
  };

  /* One four-legged animal, shaped by its form: the ears, the tail, the line
   * of the back and whether it carries antlers are all this table. */
  var QUAD = {
    wolf:   { leg: 0.34, body: 0.26, neck: 0.28, head: 0.20, ear: 'point', tail: 'brush', snout: 0.9 },
    fox:    { leg: 0.30, body: 0.22, neck: 0.24, head: 0.19, ear: 'tall',  tail: 'bush',  snout: 1.0 },
    cat:    { leg: 0.30, body: 0.22, neck: 0.20, head: 0.18, ear: 'tall',  tail: 'curl',  snout: 0.6 },
    dog:    { leg: 0.32, body: 0.25, neck: 0.24, head: 0.19, ear: 'flop',  tail: 'up',    snout: 0.9 },
    deer:   { leg: 0.46, body: 0.24, neck: 0.36, head: 0.16, ear: 'tall',  tail: 'stub',  snout: 0.9, antlers: true },
    horse:  { leg: 0.46, body: 0.28, neck: 0.34, head: 0.18, ear: 'point', tail: 'flow',  snout: 1.1, mane: true },
    bear:   { leg: 0.26, body: 0.34, neck: 0.18, head: 0.22, ear: 'round', tail: 'stub',  snout: 0.7, hump: true },
    lion:   { leg: 0.32, body: 0.28, neck: 0.22, head: 0.20, ear: 'round', tail: 'tuft',  snout: 0.8, mane: 'ruff' },
    rabbit: { leg: 0.20, body: 0.20, neck: 0.16, head: 0.18, ear: 'ears',  tail: 'puff',  snout: 0.6 }
  };

  /* A leg: thicker at the shoulder, narrower at the hoof, bent at the knee.
   * Four sticks of even width read as a table; this reads as an animal. */
  function limb(ctx, x, topY, botY, wTop, wBot, bend, style) {
    var kneeY = topY + (botY - topY) * 0.52;
    var kx = x + bend;
    ctx.beginPath();
    ctx.moveTo(x - wTop, topY);
    ctx.quadraticCurveTo(kx - wTop * 0.7, kneeY, kx - wBot, botY);
    ctx.lineTo(kx + wBot, botY);
    ctx.quadraticCurveTo(kx + wTop * 0.7, kneeY, x + wTop, topY);
    ctx.closePath();
    ctx.fillStyle = style;
    ctx.fill();
  }

  DRAW.quadruped = function (ctx, b, P, r, spec, form) {
    var f = QUAD[form] || QUAD.wolf;
    var col = ink(P, b);
    var back = P.ink(0.34 + b.depth * 0.3);            // the far pair of legs
    var groundY = b.y + b.h;
    var legLen = b.h * f.leg;
    var bodyH = b.h * f.body;
    var bodyTop = groundY - legLen - bodyH;
    var cx = b.x + b.w * 0.46, bw = b.w * 0.5;
    var lw = b.w * 0.045 * (f.leg > 0.4 ? 0.85 : 1.15);

    /* Far legs first, a shade lighter, so the animal has a near and a far
     * side instead of four legs in one plane. */
    limb(ctx, cx - bw * 0.26, bodyTop + bodyH * 0.72, groundY - b.h * 0.004, lw, lw * 0.45, -b.w * 0.012, back);
    limb(ctx, cx + bw * 0.30, bodyTop + bodyH * 0.66, groundY - b.h * 0.004, lw, lw * 0.45, b.w * 0.012, back);

    ctx.beginPath();                                   // barrel: chest, back, haunch
    ctx.moveTo(cx + bw * 0.42, bodyTop + bodyH * 0.24);
    ctx.quadraticCurveTo(cx + bw * 0.1, bodyTop - bodyH * (f.hump ? 0.22 : 0.04),
      cx - bw * 0.34, bodyTop + bodyH * 0.10);
    ctx.quadraticCurveTo(cx - bw * 0.62, bodyTop + bodyH * 0.22, cx - bw * 0.54, bodyTop + bodyH * 0.72);
    ctx.quadraticCurveTo(cx - bw * 0.3, bodyTop + bodyH * 1.02, cx + bw * 0.06, bodyTop + bodyH * 0.96);
    ctx.quadraticCurveTo(cx + bw * 0.40, bodyTop + bodyH * 0.92, cx + bw * 0.48, bodyTop + bodyH * 0.62);
    ctx.closePath();
    ctx.fillStyle = col;
    ctx.fill();

    var neckLen = b.h * f.neck;                        // neck, tapering to the head
    var hx = cx + bw * (0.55 + f.neck * 0.5);
    var hy = bodyTop - neckLen * 0.62;
    var nw = b.h * f.head * 0.46;
    ctx.beginPath();
    ctx.moveTo(cx + bw * 0.24, bodyTop + bodyH * 0.36);
    ctx.quadraticCurveTo(cx + bw * 0.46, bodyTop - neckLen * 0.1, hx - nw * 0.5, hy + nw * 0.5);
    ctx.lineTo(hx + nw * 0.3, hy + nw * 0.9);
    ctx.quadraticCurveTo(cx + bw * 0.62, bodyTop + bodyH * 0.12, cx + bw * 0.50, bodyTop + bodyH * 0.62);
    ctx.closePath();
    ctx.fillStyle = col;
    ctx.fill();

    limb(ctx, cx - bw * 0.34, bodyTop + bodyH * 0.74, groundY, lw * 1.1, lw * 0.5, -b.w * 0.016, col);
    limb(ctx, cx + bw * 0.38, bodyTop + bodyH * 0.68, groundY, lw * 1.1, lw * 0.5, b.w * 0.016, col);

    var hr = b.h * f.head;                             // head and muzzle
    ellipse(ctx, hx, hy, hr * 0.46, hr * 0.40, col);
    ellipse(ctx, hx + hr * 0.44 * f.snout, hy + hr * 0.14,
      hr * 0.30 * f.snout, hr * 0.19, col);

    if (f.ear === 'tall' || f.ear === 'point') {
      [[-0.20, 1], [0.08, 0.88]].forEach(function (e) {
        poly(ctx, [
          [hx + hr * e[0], hy - hr * 0.24],
          [hx + hr * (e[0] + 0.02), hy - hr * (f.ear === 'tall' ? 0.86 : 0.66) * e[1]],
          [hx + hr * (e[0] + 0.22), hy - hr * 0.22]
        ], col);
      });
    } else if (f.ear === 'ears') {
      [[-0.14, -0.3], [0.10, -0.1]].forEach(function (e) {
        ellipse(ctx, hx + hr * e[0] + hr * e[1] * 0.2, hy - hr * 0.78, hr * 0.11, hr * 0.46, col, e[1] * 0.4);
      });
    } else if (f.ear === 'flop') {
      ellipse(ctx, hx - hr * 0.22, hy + hr * 0.06, hr * 0.16, hr * 0.34, col, 0.3);
    } else {
      ellipse(ctx, hx - hr * 0.14, hy - hr * 0.30, hr * 0.20, hr * 0.20, col);
    }

    if (f.antlers) {                                   // a stag is its antlers
      ctx.strokeStyle = col;
      ctx.lineCap = 'round';
      ctx.lineWidth = Math.max(2, hr * 0.15);
      [-1, 1].forEach(function (side) {
        var sway = side * 1.15;
        ctx.beginPath();
        ctx.moveTo(hx - hr * 0.08, hy - hr * 0.3);
        ctx.quadraticCurveTo(hx + hr * sway * 0.4, hy - hr * 1.5,
          hx + hr * sway * 1.5, hy - hr * 2.7);
        ctx.stroke();
        for (var t = 0; t < 4; t++) {                  // tines off the main beam
          var along = 0.28 + t * 0.24;
          var px = hx + hr * sway * along * 1.2;
          var py = hy - hr * (0.55 + t * 0.62);
          ctx.lineWidth = Math.max(1.5, hr * 0.10);
          ctx.beginPath();
          ctx.moveTo(px, py);
          ctx.quadraticCurveTo(px + hr * sway * 0.55, py - hr * 0.55,
            px + hr * sway * 0.85, py - hr * 1.05);
          ctx.stroke();
        }
        ctx.lineWidth = Math.max(2, hr * 0.15);
      });
    }
    if (f.mane === 'ruff') {                           // a lion's mane, round the head
      ctx.fillStyle = col;
      ctx.beginPath();
      for (var m = 0; m <= 16; m++) {
        var ma = (m / 16) * Math.PI * 2;
        var spike = m % 2 ? 1.02 : 0.78;
        var mx2 = hx - hr * 0.16 + Math.cos(ma) * hr * 0.95 * spike;
        var my2 = hy + Math.sin(ma) * hr * 0.95 * spike;
        if (m === 0) ctx.moveTo(mx2, my2); else ctx.lineTo(mx2, my2);
      }
      ctx.closePath();
      ctx.fill();
      ellipse(ctx, hx, hy, hr * 0.44, hr * 0.38, col);
      ellipse(ctx, hx + hr * 0.44 * f.snout, hy + hr * 0.14, hr * 0.30 * f.snout, hr * 0.19, col);
    } else if (f.mane) {                               // a horse's, along the neck
      ctx.fillStyle = col;
      ctx.beginPath();
      ctx.moveTo(hx - hr * 0.42, hy - hr * 0.18);
      ctx.quadraticCurveTo(cx + bw * 0.46, bodyTop - bodyH * 0.30, cx + bw * 0.02, bodyTop - bodyH * 0.04);
      ctx.lineTo(cx + bw * 0.16, bodyTop + bodyH * 0.16);
      ctx.quadraticCurveTo(cx + bw * 0.50, bodyTop + bodyH * 0.02, hx - hr * 0.12, hy + hr * 0.34);
      ctx.closePath();
      ctx.fill();
    }

    ctx.strokeStyle = col;                             // tail
    ctx.lineCap = 'round';
    ctx.lineWidth = b.h * (f.tail === 'brush' || f.tail === 'bush' ? 0.075 : 0.032);
    var tailX = cx - bw * 0.52, tailY = bodyTop + bodyH * 0.24;
    ctx.beginPath();
    ctx.moveTo(tailX, tailY);
    if (f.tail === 'up' || f.tail === 'curl') {
      ctx.quadraticCurveTo(tailX - bw * 0.34, tailY - bodyH * 0.9, tailX - bw * 0.06, tailY - bodyH * 1.15);
      ctx.stroke();
    } else if (f.tail === 'puff') {
      ctx.stroke();
      ellipse(ctx, tailX - bw * 0.06, tailY + bodyH * 0.3, b.h * 0.05, b.h * 0.05, col);
    } else if (f.tail === 'stub') {
      ctx.lineTo(tailX - bw * 0.12, tailY + bodyH * 0.18);
      ctx.stroke();
    } else if (f.tail === 'tuft') {
      ctx.quadraticCurveTo(tailX - bw * 0.26, tailY + bodyH * 0.5, tailX - bw * 0.12, tailY + bodyH * 0.95);
      ctx.stroke();
      ellipse(ctx, tailX - bw * 0.12, tailY + bodyH * 1.0, b.h * 0.045, b.h * 0.05, col);
    } else {
      ctx.quadraticCurveTo(tailX - bw * 0.40, tailY + bodyH * 0.2, tailX - bw * 0.34, tailY + bodyH * 1.0);
      ctx.stroke();
    }

    ctx.fillStyle = rim(P, 0.85);                      // eye
    ctx.beginPath();
    ctx.arc(hx + hr * 0.16, hy - hr * 0.04, Math.max(1, b.h * 0.011), 0, Math.PI * 2);
    ctx.fill();
  };

  var BIRDS = {
    bird:    { span: 1.0, tail: 0.3, crest: false, fire: false },
    owl:     { span: 0.8, tail: 0.2, crest: true,  fire: false, round: true },
    phoenix: { span: 1.15, tail: 0.8, crest: true, fire: true }
  };

  DRAW.bird = function (ctx, b, P, r, spec, form) {
    var f = BIRDS[form] || BIRDS.bird;
    var cx = b.x + b.w * 0.5, cy = b.y + b.h * 0.5;
    var col = f.fire ? P.css([22, 92, 58]) : ink(P, b);
    if (f.fire) glow(ctx, cx, cy, b.w * 0.9, P.css([26, 96, 60], 0.4));

    ellipse(ctx, cx, cy, b.w * 0.12, b.h * (f.round ? 0.30 : 0.20), col);
    [1, -1].forEach(function (s) {
      ctx.beginPath();
      ctx.moveTo(cx, cy - b.h * 0.06);
      ctx.quadraticCurveTo(cx + b.w * 0.3 * s * f.span, cy - b.h * 0.5,
        cx + b.w * 0.5 * s * f.span, cy - b.h * 0.1);
      ctx.quadraticCurveTo(cx + b.w * 0.3 * s * f.span, cy - b.h * 0.02, cx, cy + b.h * 0.06);
      ctx.closePath();
      ctx.fillStyle = s > 0 ? col : (f.fire ? P.css([12, 90, 46]) : P.ink(0.3 + b.depth * 0.3));
      ctx.fill();
    });
    poly(ctx, [                                        // tail
      [cx - b.w * 0.08, cy + b.h * 0.14],
      [cx - b.w * 0.22, cy + b.h * (0.2 + f.tail)],
      [cx + b.w * 0.06, cy + b.h * 0.18]
    ], col);
    ellipse(ctx, cx + b.w * 0.10, cy - b.h * (f.round ? 0.28 : 0.20), b.w * 0.07, b.h * 0.12, col);
    poly(ctx, [                                        // beak
      [cx + b.w * 0.15, cy - b.h * 0.22], [cx + b.w * 0.24, cy - b.h * 0.18],
      [cx + b.w * 0.15, cy - b.h * 0.15]
    ], col);
    if (f.crest) {
      poly(ctx, [[cx + b.w * 0.04, cy - b.h * 0.32], [cx + b.w * 0.14, cy - b.h * 0.52],
        [cx + b.w * 0.16, cy - b.h * 0.30]], col);
    }
    ctx.fillStyle = rim(P, 0.9);
    ctx.beginPath();
    ctx.arc(cx + b.w * 0.13, cy - b.h * 0.25, Math.max(1, b.w * 0.018), 0, Math.PI * 2);
    ctx.fill();
  };

  /* ================================================================= people */

  var PEOPLE = {
    figure:    { head: 'plain', pack: true },
    astronaut: { head: 'helmet', bulk: 1.35 },
    robot:     { head: 'box', bulk: 1.2, antenna: true },
    wizard:    { head: 'hat', robe: true, staff: true },
    knight:    { head: 'helm', sword: true, cape: true },
    ghost:     { head: 'plain', ghostly: true },
    diver:     { head: 'helmet', bulk: 1.2 }
  };

  DRAW.humanoid = function (ctx, b, P, r, spec, form) {
    var f = PEOPLE[form] || PEOPLE.figure;
    var col = f.ghostly ? P.light(0.42) : ink(P, b);
    var cx = b.x + b.w * 0.5, top = b.y, groundY = b.y + b.h;
    var headR = b.h * 0.09 * (f.bulk || 1);
    var shoulder = top + headR * 2.6;
    var hip = groundY - b.h * 0.34;
    var halfW = b.w * 0.30 * (f.bulk || 1);

    if (f.ghostly) glow(ctx, cx, b.y + b.h * 0.45, b.w * 1.4, P.light(0.2));

    if (f.robe || f.ghostly) {                         // one flowing shape
      ctx.beginPath();
      ctx.moveTo(cx - halfW * 0.7, shoulder);
      ctx.quadraticCurveTo(cx - halfW * 1.5, groundY, cx - halfW * 1.3, groundY);
      if (f.ghostly) {
        for (var i = 0; i <= 4; i++) {
          ctx.quadraticCurveTo(cx + halfW * (-1.3 + i * 0.65), groundY - b.h * 0.06,
            cx + halfW * (-1.0 + i * 0.65), groundY);
        }
      } else {
        ctx.lineTo(cx + halfW * 1.3, groundY);
      }
      ctx.quadraticCurveTo(cx + halfW * 1.5, groundY, cx + halfW * 0.7, shoulder);
      ctx.closePath();
      ctx.fillStyle = col;
      ctx.fill();
    } else {
      ctx.fillStyle = col;                             // torso
      ctx.beginPath();
      ctx.moveTo(cx - halfW * 0.8, shoulder);
      ctx.lineTo(cx + halfW * 0.8, shoulder);
      ctx.lineTo(cx + halfW * 0.6, hip);
      ctx.lineTo(cx - halfW * 0.6, hip);
      ctx.closePath();
      ctx.fill();
      ctx.strokeStyle = col;                           // legs + arms
      ctx.lineCap = 'round';
      ctx.lineWidth = Math.max(2, b.w * 0.16);
      [-0.3, 0.3].forEach(function (o) {
        ctx.beginPath();
        ctx.moveTo(cx + halfW * o, hip);
        ctx.lineTo(cx + halfW * o * 1.5, groundY);
        ctx.stroke();
      });
      ctx.lineWidth = Math.max(1.5, b.w * 0.12);
      [-1, 1].forEach(function (s) {
        ctx.beginPath();
        ctx.moveTo(cx + halfW * 0.7 * s, shoulder + b.h * 0.02);
        ctx.lineTo(cx + halfW * 1.1 * s, hip + b.h * 0.04);
        ctx.stroke();
      });
    }

    if (f.cape) {
      ctx.beginPath();
      ctx.moveTo(cx - halfW * 0.8, shoulder);
      ctx.quadraticCurveTo(cx - halfW * 2.0, hip, cx - halfW * 1.2, groundY - b.h * 0.05);
      ctx.lineTo(cx + halfW * 0.4, shoulder);
      ctx.closePath();
      ctx.fillStyle = P.ink(0.3);
      ctx.fill();
    }
    if (f.pack) {
      ctx.fillStyle = col;
      ctx.fillRect(cx - halfW * 1.25, shoulder + b.h * 0.02, halfW * 0.6, b.h * 0.14);
    }

    var hy = top + headR;                               // head
    if (f.head === 'box') {
      ctx.fillStyle = col;
      ctx.fillRect(cx - headR, hy - headR, headR * 2, headR * 2);
      ctx.fillStyle = rim(P, 0.9);
      ctx.fillRect(cx - headR * 0.55, hy - headR * 0.25, headR * 1.1, headR * 0.4);
    } else if (f.head === 'helmet') {
      ellipse(ctx, cx, hy, headR * 1.2, headR * 1.2, col);
      ctx.fillStyle = rim(P, 0.55);
      ctx.beginPath();
      ctx.arc(cx + headR * 0.12, hy - headR * 0.1, headR * 0.78, Math.PI * 0.85, Math.PI * 2.15);
      ctx.fill();
    } else if (f.head === 'hat') {
      ellipse(ctx, cx, hy, headR, headR, col);
      poly(ctx, [[cx - headR * 1.9, hy - headR * 0.7], [cx + headR * 1.9, hy - headR * 0.7],
        [cx + headR * 0.2, hy - headR * 4.2]], col);
    } else if (f.head === 'helm') {
      ellipse(ctx, cx, hy, headR, headR * 1.1, col);
      ctx.fillStyle = rim(P, 0.7);
      ctx.fillRect(cx - headR * 0.6, hy - headR * 0.2, headR * 1.2, headR * 0.22);
      poly(ctx, [[cx, hy - headR], [cx - headR * 0.3, hy - headR * 2.2],
        [cx + headR * 0.3, hy - headR * 2.0]], col);
    } else {
      ellipse(ctx, cx, hy, headR, headR * 1.05, col);
    }
    if (f.antenna) {
      ctx.strokeStyle = col;
      ctx.lineWidth = Math.max(1, b.w * 0.05);
      ctx.beginPath();
      ctx.moveTo(cx, hy - headR);
      ctx.lineTo(cx + headR * 0.5, hy - headR * 2.2);
      ctx.stroke();
      ctx.fillStyle = rim(P, 0.95);
      ctx.beginPath();
      ctx.arc(cx + headR * 0.5, hy - headR * 2.4, Math.max(1.2, headR * 0.3), 0, Math.PI * 2);
      ctx.fill();
    }
    if (f.staff) {
      ctx.strokeStyle = col;
      ctx.lineWidth = Math.max(1.5, b.w * 0.07);
      ctx.beginPath();
      ctx.moveTo(cx + halfW * 1.2, groundY);
      ctx.lineTo(cx + halfW * 1.0, top + headR * 0.2);
      ctx.stroke();
      glow(ctx, cx + halfW * 1.0, top + headR * 0.2, b.w * 0.7, P.light(0.5));
    }
    if (f.sword) {
      ctx.strokeStyle = rim(P, 0.85);
      ctx.lineWidth = Math.max(1.5, b.w * 0.07);
      ctx.beginPath();
      ctx.moveTo(cx + halfW * 1.15, hip + b.h * 0.04);
      ctx.lineTo(cx + halfW * 1.5, top + b.h * 0.02);
      ctx.stroke();
    }
  };

  /* ============================================================== buildings */

  DRAW.castle = function (ctx, b, P, r) {
    var col = ink(P, b), groundY = b.y + b.h;
    var wallTop = b.y + b.h * 0.52;
    ctx.fillStyle = col;
    ctx.fillRect(b.x + b.w * 0.12, wallTop, b.w * 0.76, groundY - wallTop);
    for (var x = b.x + b.w * 0.12; x < b.x + b.w * 0.88; x += b.w * 0.09) {
      ctx.fillRect(x, wallTop - b.h * 0.04, b.w * 0.05, b.h * 0.04);
    }
    [[0.06, 0.30], [0.5, 0.10], [0.82, 0.34]].forEach(function (t) {   // towers
      var tx = b.x + b.w * t[0], tw = b.w * 0.18, ty = b.y + b.h * t[1];
      ctx.fillStyle = col;
      ctx.fillRect(tx, ty, tw, groundY - ty);
      poly(ctx, [[tx - tw * 0.16, ty], [tx + tw * 0.5, ty - b.h * 0.14],
        [tx + tw * 1.16, ty]], col);
      ctx.fillStyle = rim(P, 0.75);                    // a lit window each
      ctx.fillRect(tx + tw * 0.36, ty + b.h * 0.10, tw * 0.26, b.h * 0.07);
    });
    ctx.fillStyle = P.ink(0.02);                       // gate
    ctx.beginPath();
    ctx.moveTo(b.x + b.w * 0.44, groundY);
    ctx.lineTo(b.x + b.w * 0.44, groundY - b.h * 0.14);
    ctx.quadraticCurveTo(b.x + b.w * 0.5, groundY - b.h * 0.22, b.x + b.w * 0.56, groundY - b.h * 0.14);
    ctx.lineTo(b.x + b.w * 0.56, groundY);
    ctx.closePath();
    ctx.fill();
  };

  DRAW.tower = function (ctx, b, P, r) {
    var col = ink(P, b), groundY = b.y + b.h;
    poly(ctx, [[b.x + b.w * 0.2, groundY], [b.x + b.w * 0.34, b.y + b.h * 0.1],
      [b.x + b.w * 0.66, b.y + b.h * 0.1], [b.x + b.w * 0.8, groundY]], col);
    poly(ctx, [[b.x + b.w * 0.28, b.y + b.h * 0.1], [b.x + b.w * 0.5, b.y],
      [b.x + b.w * 0.72, b.y + b.h * 0.1]], col);
    ctx.fillStyle = rim(P, 0.8);
    for (var i = 1; i < 5; i++) {
      ctx.fillRect(b.x + b.w * 0.44, b.y + b.h * (0.2 + i * 0.16), b.w * 0.12, b.h * 0.06);
    }
  };

  DRAW.lighthouse = function (ctx, b, P, r, spec) {
    var col = ink(P, b), groundY = b.y + b.h;
    poly(ctx, [[b.x + b.w * 0.22, groundY], [b.x + b.w * 0.36, b.y + b.h * 0.18],
      [b.x + b.w * 0.64, b.y + b.h * 0.18], [b.x + b.w * 0.78, groundY]], col);
    ctx.fillStyle = P.light(0.25);                     // painted bands
    for (var i = 0; i < 3; i++) {
      ctx.fillRect(b.x + b.w * 0.26, b.y + b.h * (0.3 + i * 0.22), b.w * 0.48, b.h * 0.08);
    }
    ctx.fillStyle = col;
    ctx.fillRect(b.x + b.w * 0.3, b.y + b.h * 0.10, b.w * 0.4, b.h * 0.09);
    ctx.fillStyle = rim(P, 0.95);
    ctx.fillRect(b.x + b.w * 0.36, b.y + b.h * 0.04, b.w * 0.28, b.h * 0.08);
    glow(ctx, b.x + b.w * 0.5, b.y + b.h * 0.08, b.w * 1.8, P.light(0.45));
    ctx.beginPath();                                   // the beam
    ctx.moveTo(b.x + b.w * 0.5, b.y + b.h * 0.08);
    ctx.lineTo(b.x - b.w * 2.2, b.y - b.h * 0.5);
    ctx.lineTo(b.x - b.w * 2.2, b.y + b.h * 0.4);
    ctx.closePath();
    ctx.fillStyle = P.light(0.12);
    ctx.fill();
  };

  DRAW.cabin = function (ctx, b, P, r) {
    var col = ink(P, b), groundY = b.y + b.h;
    var top = b.y + b.h * 0.42;
    ctx.fillStyle = col;
    ctx.fillRect(b.x + b.w * 0.18, top, b.w * 0.64, groundY - top);
    poly(ctx, [[b.x + b.w * 0.10, top], [b.x + b.w * 0.5, b.y + b.h * 0.06],
      [b.x + b.w * 0.90, top]], col);
    ctx.fillStyle = rim(P, 0.85);                      // warm window
    ctx.fillRect(b.x + b.w * 0.30, top + b.h * 0.12, b.w * 0.16, b.h * 0.16);
    glow(ctx, b.x + b.w * 0.38, top + b.h * 0.2, b.w * 0.6, P.light(0.35));
    ctx.fillStyle = col;
    ctx.fillRect(b.x + b.w * 0.56, top + b.h * 0.1, b.w * 0.14, groundY - top - b.h * 0.1);
    ctx.fillRect(b.x + b.w * 0.66, b.y + b.h * 0.14, b.w * 0.08, b.h * 0.2);  // chimney
    ctx.globalAlpha = 0.4;                             // smoke
    for (var i = 0; i < 4; i++) {
      ctx.beginPath();
      ctx.arc(b.x + b.w * (0.70 + i * 0.05), b.y + b.h * (0.10 - i * 0.03),
        b.w * (0.04 + i * 0.02), 0, Math.PI * 2);
      ctx.fillStyle = P.haze(0.7);
      ctx.fill();
    }
    ctx.globalAlpha = 1;
  };

  DRAW.temple = function (ctx, b, P, r) {
    var col = ink(P, b), groundY = b.y + b.h;
    for (var t = 0; t < 3; t++) {                      // stacked tiers
      var ty = b.y + b.h * (0.16 + t * 0.24);
      var tw = b.w * (0.34 + t * 0.2);
      ctx.fillStyle = col;
      ctx.fillRect(b.x + b.w * 0.5 - tw / 2, ty, tw, b.h * 0.2);
      poly(ctx, [[b.x + b.w * 0.5 - tw * 0.78, ty], [b.x + b.w * 0.5, ty - b.h * 0.12],
        [b.x + b.w * 0.5 + tw * 0.78, ty]], col);
    }
    ctx.fillStyle = P.ink(0.02);
    ctx.fillRect(b.x + b.w * 0.44, groundY - b.h * 0.16, b.w * 0.12, b.h * 0.16);
  };

  DRAW.torii = function (ctx, b, P, r) {
    var col = P.css([2, 74, 44]), groundY = b.y + b.h;
    ctx.fillStyle = col;
    ctx.fillRect(b.x + b.w * 0.16, b.y + b.h * 0.18, b.w * 0.10, groundY - b.y - b.h * 0.18);
    ctx.fillRect(b.x + b.w * 0.74, b.y + b.h * 0.18, b.w * 0.10, groundY - b.y - b.h * 0.18);
    poly(ctx, [[b.x, b.y + b.h * 0.12], [b.x + b.w, b.y + b.h * 0.12],
      [b.x + b.w * 0.94, b.y + b.h * 0.02], [b.x + b.w * 0.06, b.y + b.h * 0.02]], col);
    ctx.fillRect(b.x + b.w * 0.10, b.y + b.h * 0.28, b.w * 0.80, b.h * 0.07);
  };

  DRAW.pyramid = function (ctx, b, P, r) {
    var col = ink(P, b), groundY = b.y + b.h;
    poly(ctx, [[b.x, groundY], [b.x + b.w * 0.5, b.y], [b.x + b.w, groundY]], col);
    poly(ctx, [[b.x + b.w * 0.5, b.y], [b.x + b.w, groundY],
      [b.x + b.w * 0.5, groundY]], P.ink(0.32 + b.depth * 0.3));
  };

  DRAW.windmill = function (ctx, b, P, r) {
    var col = ink(P, b), groundY = b.y + b.h;
    var cx = b.x + b.w * 0.5, hubY = b.y + b.h * 0.24;
    poly(ctx, [[cx - b.w * 0.22, groundY], [cx - b.w * 0.13, hubY],
      [cx + b.w * 0.13, hubY], [cx + b.w * 0.22, groundY]], col);
    poly(ctx, [[cx - b.w * 0.17, hubY], [cx, b.y + b.h * 0.10],
      [cx + b.w * 0.17, hubY]], col);
    ctx.strokeStyle = col;
    ctx.lineWidth = Math.max(1.5, b.w * 0.04);
    for (var i = 0; i < 4; i++) {
      var a = i * Math.PI / 2 + 0.4;
      ctx.beginPath();
      ctx.moveTo(cx, hubY);
      ctx.lineTo(cx + Math.cos(a) * b.w * 0.46, hubY + Math.sin(a) * b.w * 0.46);
      ctx.stroke();
    }
  };

  DRAW.bridge = function (ctx, b, P, r) {
    var col = ink(P, b), groundY = b.y + b.h;
    ctx.fillStyle = col;
    ctx.fillRect(b.x, b.y + b.h * 0.34, b.w, b.h * 0.10);
    for (var i = 0; i < 4; i++) {                      // arches under the deck
      var ax = b.x + b.w * (0.12 + i * 0.25);
      ctx.beginPath();
      ctx.moveTo(ax, groundY);
      ctx.lineTo(ax, b.y + b.h * 0.44);
      ctx.lineTo(ax + b.w * 0.16, b.y + b.h * 0.44);
      ctx.lineTo(ax + b.w * 0.16, groundY);
      ctx.closePath();
      ctx.fillStyle = col;
      ctx.fill();
    }
    ctx.strokeStyle = col;
    ctx.lineWidth = Math.max(1, b.h * 0.02);
    for (var c = 0; c <= 10; c++) {                    // railing
      var rx = b.x + b.w * c / 10;
      ctx.beginPath();
      ctx.moveTo(rx, b.y + b.h * 0.34);
      ctx.lineTo(rx, b.y + b.h * 0.2);
      ctx.stroke();
    }
    ctx.beginPath();
    ctx.moveTo(b.x, b.y + b.h * 0.2);
    ctx.lineTo(b.x + b.w, b.y + b.h * 0.2);
    ctx.stroke();
  };

  DRAW.city = function (ctx, b, P, r, spec) {
    var groundY = b.y + b.h, x = b.x;
    while (x < b.x + b.w) {
      var bw = b.w * (0.06 + r() * 0.1);
      var bh = b.h * (0.3 + r() * 0.7);
      ctx.fillStyle = ink(P, b);
      ctx.fillRect(x, groundY - bh, bw, bh);
      if (spec.time !== 'day') {
        for (var wy = groundY - bh + b.h * 0.04; wy < groundY - b.h * 0.03; wy += b.h * 0.07) {
          for (var wx = x + bw * 0.16; wx < x + bw * 0.84; wx += bw * 0.3) {
            if (r() > 0.55) continue;
            ctx.fillStyle = rim(P, 0.4 + r() * 0.5);
            ctx.fillRect(wx, wy, bw * 0.16, b.h * 0.03);
          }
        }
      }
      x += bw * 1.15;
    }
  };

  DRAW.ruins = function (ctx, b, P, r) {
    var col = ink(P, b), groundY = b.y + b.h;
    for (var i = 0; i < 6; i++) {
      var x = b.x + b.w * (0.05 + i * 0.16);
      var ch = b.h * (0.3 + r() * 0.7);
      var cw = b.w * 0.07;
      ctx.fillStyle = col;
      ctx.fillRect(x, groundY - ch, cw, ch);
      ctx.fillRect(x - cw * 0.3, groundY - ch - b.h * 0.03, cw * 1.6, b.h * 0.03);
    }
    ctx.fillStyle = col;                               // fallen lintel
    ctx.fillRect(b.x + b.w * 0.1, groundY - b.h * 0.06, b.w * 0.5, b.h * 0.05);
  };

  /* ================================================================ objects */

  DRAW.ship = function (ctx, b, P, r) {
    var col = ink(P, b), waterY = b.y + b.h;
    var cx = b.x + b.w * 0.5;
    ctx.fillStyle = col;
    ctx.beginPath();                                   // hull
    ctx.moveTo(cx - b.w * 0.42, waterY - b.h * 0.14);
    ctx.lineTo(cx + b.w * 0.42, waterY - b.h * 0.14);
    ctx.quadraticCurveTo(cx + b.w * 0.30, waterY, cx, waterY);
    ctx.quadraticCurveTo(cx - b.w * 0.30, waterY, cx - b.w * 0.42, waterY - b.h * 0.14);
    ctx.closePath();
    ctx.fill();
    ctx.strokeStyle = col;                             // masts
    ctx.lineWidth = Math.max(1.5, b.w * 0.025);
    [-0.2, 0.16].forEach(function (o, i) {
      var mx = cx + b.w * o, mh = b.h * (i ? 0.62 : 0.82);
      ctx.beginPath();
      ctx.moveTo(mx, waterY - b.h * 0.14);
      ctx.lineTo(mx, waterY - b.h * 0.14 - mh);
      ctx.stroke();
      for (var s = 0; s < 2; s++) {                    // square sails, bellied out
        var sy = waterY - b.h * 0.16 - mh * (0.30 + s * 0.36);
        var sh = mh * 0.30, sw = b.w * 0.17;
        ctx.beginPath();
        ctx.moveTo(mx - sw * 0.7, sy);
        ctx.lineTo(mx + sw * 0.7, sy);
        ctx.quadraticCurveTo(mx + sw * 1.15, sy + sh * 0.5, mx + sw * 0.85, sy + sh);
        ctx.lineTo(mx - sw * 0.85, sy + sh);
        ctx.quadraticCurveTo(mx - sw * 0.55, sy + sh * 0.5, mx - sw * 0.7, sy);
        ctx.closePath();
        ctx.fillStyle = P.css([40, 30, 88], 0.95);
        ctx.fill();
        ctx.strokeStyle = col;                         // yard across the top
        ctx.lineWidth = Math.max(1, b.w * 0.012);
        ctx.beginPath();
        ctx.moveTo(mx - sw * 0.9, sy);
        ctx.lineTo(mx + sw * 0.9, sy);
        ctx.stroke();
        ctx.lineWidth = Math.max(1.5, b.w * 0.025);
      }
    });
  };

  DRAW.rocket = function (ctx, b, P, r) {
    var col = ink(P, b), cx = b.x + b.w * 0.5;
    ctx.fillStyle = col;
    ctx.beginPath();
    ctx.moveTo(cx, b.y);
    ctx.quadraticCurveTo(cx + b.w * 0.32, b.y + b.h * 0.34, cx + b.w * 0.28, b.y + b.h * 0.74);
    ctx.lineTo(cx - b.w * 0.28, b.y + b.h * 0.74);
    ctx.quadraticCurveTo(cx - b.w * 0.32, b.y + b.h * 0.34, cx, b.y);
    ctx.closePath();
    ctx.fill();
    poly(ctx, [[cx - b.w * 0.28, b.y + b.h * 0.52], [cx - b.w * 0.5, b.y + b.h * 0.80],
      [cx - b.w * 0.28, b.y + b.h * 0.74]], col);
    poly(ctx, [[cx + b.w * 0.28, b.y + b.h * 0.52], [cx + b.w * 0.5, b.y + b.h * 0.80],
      [cx + b.w * 0.28, b.y + b.h * 0.74]], col);
    ctx.fillStyle = rim(P, 0.85);
    ctx.beginPath();
    ctx.arc(cx, b.y + b.h * 0.34, b.w * 0.10, 0, Math.PI * 2);
    ctx.fill();
    glow(ctx, cx, b.y + b.h * 0.92, b.w * 0.9, P.css([32, 100, 62], 0.7));
    poly(ctx, [[cx - b.w * 0.16, b.y + b.h * 0.74], [cx, b.y + b.h * 1.12],
      [cx + b.w * 0.16, b.y + b.h * 0.74]], P.css([44, 100, 68], 0.95));
  };

  DRAW.balloon = function (ctx, b, P, r) {
    var cx = b.x + b.w * 0.5, top = b.y + b.h * 0.30;
    ctx.beginPath();
    ctx.moveTo(cx, b.y);
    ctx.bezierCurveTo(cx + b.w * 0.62, b.y + b.h * 0.06, cx + b.w * 0.44, top + b.h * 0.16,
      cx + b.w * 0.14, top + b.h * 0.26);
    ctx.lineTo(cx - b.w * 0.14, top + b.h * 0.26);
    ctx.bezierCurveTo(cx - b.w * 0.44, top + b.h * 0.16, cx - b.w * 0.62, b.y + b.h * 0.06, cx, b.y);
    ctx.closePath();
    ctx.fillStyle = P.css([14, 78, 54]);
    ctx.fill();
    ctx.fillStyle = P.css([44, 88, 62]);               // gores
    for (var i = -1; i <= 1; i += 2) {
      ctx.beginPath();
      ctx.moveTo(cx + b.w * 0.06 * i, b.y + b.h * 0.01);
      ctx.quadraticCurveTo(cx + b.w * 0.26 * i, b.y + b.h * 0.24, cx + b.w * 0.07 * i, top + b.h * 0.26);
      ctx.lineTo(cx, top + b.h * 0.26);
      ctx.closePath();
      ctx.fill();
    }
    ctx.strokeStyle = ink(P, b);
    ctx.lineWidth = Math.max(1, b.w * 0.012);
    [-0.12, 0.12].forEach(function (o) {
      ctx.beginPath();
      ctx.moveTo(cx + b.w * o, top + b.h * 0.26);
      ctx.lineTo(cx + b.w * o * 0.7, b.y + b.h * 0.86);
      ctx.stroke();
    });
    ctx.fillStyle = ink(P, b);
    ctx.fillRect(cx - b.w * 0.11, b.y + b.h * 0.86, b.w * 0.22, b.h * 0.12);
  };

  DRAW.ufo = function (ctx, b, P, r) {
    var cx = b.x + b.w * 0.5, cy = b.y + b.h * 0.5, col = ink(P, b);
    ellipse(ctx, cx, cy + b.h * 0.06, b.w * 0.48, b.h * 0.16, col);
    ellipse(ctx, cx, cy - b.h * 0.08, b.w * 0.22, b.h * 0.2, P.light(0.5));
    for (var i = -3; i <= 3; i++) {
      ctx.fillStyle = rim(P, 0.9);
      ctx.beginPath();
      ctx.arc(cx + b.w * 0.11 * i, cy + b.h * 0.1, Math.max(1.2, b.w * 0.022), 0, Math.PI * 2);
      ctx.fill();
    }
    var g = ctx.createLinearGradient(0, cy, 0, cy + b.h * 2.4);   // tractor beam
    g.addColorStop(0, P.light(0.35));
    g.addColorStop(1, P.light(0));
    ctx.fillStyle = g;
    ctx.beginPath();
    ctx.moveTo(cx - b.w * 0.16, cy + b.h * 0.14);
    ctx.lineTo(cx + b.w * 0.16, cy + b.h * 0.14);
    ctx.lineTo(cx + b.w * 0.5, cy + b.h * 2.4);
    ctx.lineTo(cx - b.w * 0.5, cy + b.h * 2.4);
    ctx.closePath();
    ctx.fill();
  };

  DRAW.train = function (ctx, b, P, r, spec) {
    var col = ink(P, b), groundY = b.y + b.h;
    ctx.fillStyle = col;
    ctx.fillRect(b.x + b.w * 0.02, groundY - b.h * 0.52, b.w * 0.30, b.h * 0.42);
    ctx.fillRect(b.x + b.w * 0.06, groundY - b.h * 0.78, b.w * 0.12, b.h * 0.28);   // funnel
    for (var c = 0; c < 3; c++) {
      ctx.fillRect(b.x + b.w * (0.36 + c * 0.22), groundY - b.h * 0.44, b.w * 0.19, b.h * 0.34);
    }
    ctx.fillStyle = ink(P, b);
    for (var wl = 0; wl < 7; wl++) {
      ctx.beginPath();
      ctx.arc(b.x + b.w * (0.06 + wl * 0.14), groundY - b.h * 0.06, b.h * 0.07, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.globalAlpha = 0.45;                            // smoke trailing back
    for (var s = 0; s < 6; s++) {
      ctx.beginPath();
      ctx.arc(b.x + b.w * (0.1 - s * 0.06), groundY - b.h * (0.9 + s * 0.12),
        b.h * (0.08 + s * 0.04), 0, Math.PI * 2);
      ctx.fillStyle = P.haze(0.8);
      ctx.fill();
    }
    ctx.globalAlpha = 1;
    if (spec.time !== 'day') glow(ctx, b.x + b.w * 0.02, groundY - b.h * 0.4, b.w * 0.5, P.light(0.5));
  };

  DRAW.car = function (ctx, b, P, r, spec) {
    var col = ink(P, b), groundY = b.y + b.h;
    ctx.fillStyle = col;
    ctx.beginPath();
    ctx.moveTo(b.x, groundY - b.h * 0.28);
    ctx.lineTo(b.x + b.w * 0.22, groundY - b.h * 0.34);
    ctx.quadraticCurveTo(b.x + b.w * 0.42, groundY - b.h * 0.86, b.x + b.w * 0.68, groundY - b.h * 0.36);
    ctx.lineTo(b.x + b.w, groundY - b.h * 0.30);
    ctx.lineTo(b.x + b.w, groundY - b.h * 0.10);
    ctx.lineTo(b.x, groundY - b.h * 0.10);
    ctx.closePath();
    ctx.fill();
    ctx.fillStyle = P.light(0.45);
    ctx.beginPath();
    ctx.moveTo(b.x + b.w * 0.3, groundY - b.h * 0.38);
    ctx.quadraticCurveTo(b.x + b.w * 0.44, groundY - b.h * 0.72, b.x + b.w * 0.62, groundY - b.h * 0.4);
    ctx.closePath();
    ctx.fill();
    ctx.fillStyle = ink(P, b);
    [0.2, 0.78].forEach(function (o) {
      ctx.beginPath();
      ctx.arc(b.x + b.w * o, groundY - b.h * 0.1, b.h * 0.11, 0, Math.PI * 2);
      ctx.fill();
    });
    if (spec.time !== 'day') {
      glow(ctx, b.x + b.w, groundY - b.h * 0.24, b.w * 0.5, P.light(0.55));
      glow(ctx, b.x, groundY - b.h * 0.24, b.w * 0.3, P.css([2, 90, 56], 0.5));
    }
  };

  DRAW.portal = function (ctx, b, P, r) {
    var cx = b.x + b.w * 0.5, cy = b.y + b.h * 0.5;
    glow(ctx, cx, cy, b.w * 1.4, P.light(0.35));
    ctx.beginPath();
    ctx.moveTo(cx - b.w * 0.38, b.y + b.h);
    ctx.lineTo(cx - b.w * 0.38, cy);
    ctx.quadraticCurveTo(cx, b.y - b.h * 0.06, cx + b.w * 0.38, cy);
    ctx.lineTo(cx + b.w * 0.38, b.y + b.h);
    ctx.closePath();
    var g = ctx.createLinearGradient(0, b.y, 0, b.y + b.h);
    g.addColorStop(0, P.light(0.85));
    g.addColorStop(1, P.css([P.sky.top[0], 80, 40], 0.8));
    ctx.fillStyle = g;
    ctx.fill();
    ctx.strokeStyle = ink(P, b);
    ctx.lineWidth = Math.max(2, b.w * 0.07);
    ctx.stroke();
  };

  DRAW.sword = function (ctx, b, P, r) {
    var cx = b.x + b.w * 0.5, groundY = b.y + b.h;
    ctx.fillStyle = ink(P, b);                         // the stone
    ctx.beginPath();
    ctx.moveTo(cx - b.w * 0.5, groundY);
    ctx.quadraticCurveTo(cx - b.w * 0.4, groundY - b.h * 0.2, cx, groundY - b.h * 0.22);
    ctx.quadraticCurveTo(cx + b.w * 0.4, groundY - b.h * 0.2, cx + b.w * 0.5, groundY);
    ctx.closePath();
    ctx.fill();
    ctx.fillStyle = P.light(0.8);                      // blade
    ctx.beginPath();
    ctx.moveTo(cx - b.w * 0.07, groundY - b.h * 0.2);
    ctx.lineTo(cx - b.w * 0.05, b.y + b.h * 0.2);
    ctx.lineTo(cx, b.y + b.h * 0.1);
    ctx.lineTo(cx + b.w * 0.05, b.y + b.h * 0.2);
    ctx.lineTo(cx + b.w * 0.07, groundY - b.h * 0.2);
    ctx.closePath();
    ctx.fill();
    ctx.fillStyle = ink(P, b);
    ctx.fillRect(cx - b.w * 0.26, b.y + b.h * 0.24, b.w * 0.52, b.h * 0.05);
    glow(ctx, cx, b.y + b.h * 0.2, b.w * 1.2, P.light(0.3));
  };

  DRAW.campfire = function (ctx, b, P, r) {
    var cx = b.x + b.w * 0.5, groundY = b.y + b.h;
    glow(ctx, cx, groundY - b.h * 0.3, b.w * 2.0, P.css([28, 96, 58], 0.45));
    ctx.strokeStyle = ink(P, b);
    ctx.lineWidth = Math.max(2, b.h * 0.07);
    [-1, 1, 0].forEach(function (s, i) {
      ctx.beginPath();
      ctx.moveTo(cx + b.w * 0.3 * s, groundY);
      ctx.lineTo(cx - b.w * 0.22 * s, groundY - b.h * (0.26 + i * 0.05));
      ctx.stroke();
    });
    [[0.5, 34, 100, 62], [0.34, 18, 100, 56], [0.18, 48, 100, 74]].forEach(function (f) {
      ctx.beginPath();
      ctx.moveTo(cx - b.w * f[0] * 0.42, groundY - b.h * 0.12);
      ctx.quadraticCurveTo(cx - b.w * f[0] * 0.2, groundY - b.h * (0.2 + f[0]),
        cx, groundY - b.h * (0.3 + f[0] * 1.3));
      ctx.quadraticCurveTo(cx + b.w * f[0] * 0.2, groundY - b.h * (0.2 + f[0]),
        cx + b.w * f[0] * 0.42, groundY - b.h * 0.12);
      ctx.closePath();
      ctx.fillStyle = P.css([f[1], f[2], f[3]], 0.95);
      ctx.fill();
    });
  };

  DRAW.crystal = function (ctx, b, P, r) {
    var cx = b.x + b.w * 0.5, groundY = b.y + b.h;
    glow(ctx, cx, b.y + b.h * 0.5, b.w * 1.6, P.light(0.3));
    [[0, 1, 0.34], [-0.32, 0.62, 0.22], [0.30, 0.5, 0.2]].forEach(function (c, i) {
      var x = cx + b.w * c[0], hgt = b.h * c[1], wd = b.w * c[2];
      poly(ctx, [
        [x - wd, groundY], [x - wd * 0.7, groundY - hgt * 0.82], [x, groundY - hgt],
        [x + wd * 0.7, groundY - hgt * 0.82], [x + wd, groundY]
      ], P.css([P.sky.light[0], 60, 58 - i * 6], 0.9));
      poly(ctx, [[x, groundY - hgt], [x + wd * 0.7, groundY - hgt * 0.82],
        [x + wd, groundY], [x, groundY]], P.css([P.sky.light[0], 60, 40], 0.7));
    });
  };

  DRAW.skull = function (ctx, b, P, r) {
    var cx = b.x + b.w * 0.5, cy = b.y + b.h * 0.45, col = P.css([40, 18, 84]);
    ellipse(ctx, cx, cy, b.w * 0.34, b.h * 0.34, col);
    ctx.fillStyle = col;
    ctx.fillRect(cx - b.w * 0.2, cy + b.h * 0.22, b.w * 0.4, b.h * 0.24);
    ctx.fillStyle = P.ink(0);
    ellipse(ctx, cx - b.w * 0.14, cy - b.h * 0.04, b.w * 0.1, b.h * 0.12, P.ink(0));
    ellipse(ctx, cx + b.w * 0.14, cy - b.h * 0.04, b.w * 0.1, b.h * 0.12, P.ink(0));
    poly(ctx, [[cx, cy + b.h * 0.08], [cx - b.w * 0.05, cy + b.h * 0.2],
      [cx + b.w * 0.05, cy + b.h * 0.2]], P.ink(0));
    for (var i = 0; i < 4; i++) {
      ctx.fillStyle = P.ink(0, 0.8);
      ctx.fillRect(cx - b.w * 0.18 + i * b.w * 0.09, cy + b.h * 0.24, b.w * 0.02, b.h * 0.2);
    }
  };

  /* ================================================================= plants */

  var TREES = {
    oak:  { trunk: 0.10, crown: 'round' },
    pine: { trunk: 0.06, crown: 'cone' },
    palm: { trunk: 0.05, crown: 'fronds' }
  };

  DRAW.tree = function (ctx, b, P, r, spec, form) {
    var f = TREES[form] || TREES.oak;
    var col = ink(P, b), cx = b.x + b.w * 0.5, groundY = b.y + b.h;

    if (f.crown === 'fronds') {
      ctx.strokeStyle = col;
      ctx.lineWidth = Math.max(2, b.w * f.trunk);
      ctx.beginPath();
      ctx.moveTo(cx - b.w * 0.06, groundY);
      ctx.quadraticCurveTo(cx + b.w * 0.04, b.y + b.h * 0.5, cx + b.w * 0.1, b.y + b.h * 0.22);
      ctx.stroke();
      for (var i = 0; i < 7; i++) {
        var a = -Math.PI * (0.15 + i * 0.12);
        ctx.beginPath();
        ctx.moveTo(cx + b.w * 0.1, b.y + b.h * 0.22);
        ctx.quadraticCurveTo(cx + b.w * 0.1 + Math.cos(a) * b.w * 0.3,
          b.y + b.h * 0.22 + Math.sin(a) * b.h * 0.16,
          cx + b.w * 0.1 + Math.cos(a) * b.w * 0.52, b.y + b.h * 0.30 + Math.sin(a) * b.h * 0.1);
        ctx.lineWidth = Math.max(2, b.w * 0.05);
        ctx.stroke();
      }
      return;
    }

    ctx.fillStyle = col;                               // trunk, splitting up
    ctx.beginPath();
    ctx.moveTo(cx - b.w * f.trunk, groundY);
    ctx.quadraticCurveTo(cx - b.w * f.trunk * 0.5, b.y + b.h * 0.5, cx - b.w * f.trunk * 0.4, b.y + b.h * 0.34);
    ctx.lineTo(cx + b.w * f.trunk * 0.4, b.y + b.h * 0.34);
    ctx.quadraticCurveTo(cx + b.w * f.trunk * 0.5, b.y + b.h * 0.5, cx + b.w * f.trunk, groundY);
    ctx.closePath();
    ctx.fill();
    ctx.strokeStyle = col;
    ctx.lineWidth = Math.max(1.5, b.w * f.trunk * 0.5);
    [-1, 1].forEach(function (s) {
      ctx.beginPath();
      ctx.moveTo(cx, b.y + b.h * 0.46);
      ctx.quadraticCurveTo(cx + b.w * 0.16 * s, b.y + b.h * 0.38, cx + b.w * 0.26 * s, b.y + b.h * 0.26);
      ctx.stroke();
    });

    if (f.crown === 'cone') {
      for (var t = 0; t < 4; t++) {
        var ty = b.y + b.h * (0.06 + t * 0.12), tw = b.w * (0.16 + t * 0.1);
        poly(ctx, [[cx - tw, ty + b.h * 0.16], [cx, ty], [cx + tw, ty + b.h * 0.16]], col);
      }
    } else {
      for (var c = 0; c < 7; c++) {
        var a2 = (c / 7) * Math.PI * 2;
        ellipse(ctx, cx + Math.cos(a2) * b.w * 0.22, b.y + b.h * 0.22 + Math.sin(a2) * b.h * 0.12,
          b.w * (0.16 + r() * 0.08), b.h * (0.11 + r() * 0.05), col);
      }
      ellipse(ctx, cx, b.y + b.h * 0.22, b.w * 0.3, b.h * 0.18, col);
    }
  };

  DRAW.cactus = function (ctx, b, P, r) {
    var col = ink(P, b), cx = b.x + b.w * 0.5, groundY = b.y + b.h;
    var tw = b.w * 0.22;
    ctx.fillStyle = col;
    ctx.beginPath();
    ctx.moveTo(cx - tw, groundY);
    ctx.lineTo(cx - tw, b.y + b.h * 0.1);
    ctx.quadraticCurveTo(cx, b.y - b.h * 0.02, cx + tw, b.y + b.h * 0.1);
    ctx.lineTo(cx + tw, groundY);
    ctx.closePath();
    ctx.fill();
    [[-1, 0.42], [1, 0.56]].forEach(function (arm) {
      var s = arm[0], ay = b.y + b.h * arm[1];
      ctx.lineWidth = tw * 1.1;
      ctx.strokeStyle = col;
      ctx.lineCap = 'round';
      ctx.beginPath();
      ctx.moveTo(cx + tw * 0.6 * s, ay);
      ctx.lineTo(cx + b.w * 0.34 * s, ay);
      ctx.lineTo(cx + b.w * 0.34 * s, ay - b.h * 0.2);
      ctx.stroke();
    });
  };

  DRAW.flower = function (ctx, b, P, r) {
    var cx = b.x + b.w * 0.5, groundY = b.y + b.h, headY = b.y + b.h * 0.22;
    ctx.strokeStyle = P.css([110, 42, 34]);
    ctx.lineWidth = Math.max(2, b.w * 0.07);
    ctx.beginPath();
    ctx.moveTo(cx, groundY);
    ctx.quadraticCurveTo(cx + b.w * 0.12, b.y + b.h * 0.6, cx, headY);
    ctx.stroke();
    [[-1, 0.55], [1, 0.68]].forEach(function (l) {
      ctx.beginPath();
      ctx.moveTo(cx + b.w * 0.02, b.y + b.h * l[1]);
      ctx.quadraticCurveTo(cx + b.w * 0.3 * l[0], b.y + b.h * (l[1] - 0.1),
        cx + b.w * 0.42 * l[0], b.y + b.h * (l[1] + 0.04));
      ctx.lineWidth = Math.max(2, b.w * 0.1);
      ctx.stroke();
    });
    for (var p = 0; p < 9; p++) {                      // petals
      var a = (p / 9) * Math.PI * 2;
      ellipse(ctx, cx + Math.cos(a) * b.w * 0.22, headY + Math.sin(a) * b.w * 0.22,
        b.w * 0.16, b.w * 0.09, P.css([[48, 350, 288, 18][Math.floor(r() * 4) % 4], 84, 66]), a);
    }
    ctx.fillStyle = P.css([36, 70, 40]);
    ctx.beginPath();
    ctx.arc(cx, headY, b.w * 0.14, 0, Math.PI * 2);
    ctx.fill();
  };

  DRAW.mushroom = function (ctx, b, P, r) {
    var groundY = b.y + b.h;
    [[0.5, 1], [0.22, 0.6], [0.76, 0.48]].forEach(function (m, i) {
      var cx = b.x + b.w * m[0], sc = m[1];
      var capY = groundY - b.h * 0.5 * sc;
      ctx.fillStyle = P.css([40, 16, 88], 0.95);
      ctx.fillRect(cx - b.w * 0.05 * sc, capY, b.w * 0.1 * sc, groundY - capY);
      ctx.beginPath();
      ctx.moveTo(cx - b.w * 0.22 * sc, capY);
      ctx.quadraticCurveTo(cx, capY - b.h * 0.36 * sc, cx + b.w * 0.22 * sc, capY);
      ctx.closePath();
      ctx.fillStyle = P.css([2, 74, 48]);
      ctx.fill();
      for (var d = 0; d < 3; d++) {
        ctx.beginPath();
        ctx.arc(cx + b.w * (d - 1) * 0.09 * sc, capY - b.h * 0.12 * sc, b.w * 0.03 * sc, 0, Math.PI * 2);
        ctx.fillStyle = P.css([40, 20, 94]);
        ctx.fill();
      }
      void i;
    });
  };

  DRAW.waterfall = function (ctx, b, P, r) {
    var col = ink(P, b), cx = b.x + b.w * 0.5, groundY = b.y + b.h;
    ctx.fillStyle = col;                               // the cliff either side
    ctx.fillRect(b.x - b.w * 0.5, b.y, b.w * 0.8, b.h);
    ctx.fillRect(b.x + b.w * 0.7, b.y, b.w * 0.8, b.h);
    var g = ctx.createLinearGradient(0, b.y, 0, groundY);
    g.addColorStop(0, P.light(0.75));
    g.addColorStop(1, P.light(0.42));
    ctx.fillStyle = g;
    ctx.fillRect(cx - b.w * 0.2, b.y, b.w * 0.4, b.h * 0.94);
    ctx.globalAlpha = 0.6;                             // spray at the bottom
    for (var i = 0; i < 16; i++) {
      ctx.beginPath();
      ctx.arc(cx + (r() - 0.5) * b.w * 0.8, groundY - r() * b.h * 0.1, b.w * 0.06 * r(), 0, Math.PI * 2);
      ctx.fillStyle = P.light(0.5);
      ctx.fill();
    }
    ctx.globalAlpha = 1;
  };

  DRAW.peak = function (ctx, b, P, r) {
    var col = ink(P, b), groundY = b.y + b.h;
    poly(ctx, [[b.x, groundY], [b.x + b.w * 0.3, b.y + b.h * 0.24], [b.x + b.w * 0.46, b.y],
      [b.x + b.w * 0.68, b.y + b.h * 0.3], [b.x + b.w, groundY]], col);
    poly(ctx, [[b.x + b.w * 0.46, b.y], [b.x + b.w * 0.68, b.y + b.h * 0.3],
      [b.x + b.w, groundY], [b.x + b.w * 0.5, groundY]], P.ink(0.3 + b.depth * 0.3));
    poly(ctx, [[b.x + b.w * 0.34, b.y + b.h * 0.2], [b.x + b.w * 0.46, b.y],
      [b.x + b.w * 0.6, b.y + b.h * 0.22], [b.x + b.w * 0.5, b.y + b.h * 0.16],
      [b.x + b.w * 0.42, b.y + b.h * 0.26]], P.css([200, 18, 94], 0.9));
  };

  DRAW.island = function (ctx, b, P, r) {
    var col = ink(P, b), topY = b.y + b.h * 0.5;
    ctx.beginPath();                                   // a rock torn from below
    ctx.moveTo(b.x, topY);
    ctx.lineTo(b.x + b.w, topY);
    ctx.lineTo(b.x + b.w * 0.72, topY + b.h * 0.36);
    ctx.lineTo(b.x + b.w * 0.5, topY + b.h * 0.9);
    ctx.lineTo(b.x + b.w * 0.3, topY + b.h * 0.3);
    ctx.closePath();
    ctx.fillStyle = col;
    ctx.fill();
    ctx.fillStyle = P.css([116, 44, 38]);              // grass on top
    ctx.beginPath();
    ctx.moveTo(b.x, topY);
    ctx.quadraticCurveTo(b.x + b.w * 0.5, topY - b.h * 0.2, b.x + b.w, topY);
    ctx.closePath();
    ctx.fill();
    ctx.strokeStyle = P.light(0.5);                    // water falling off it
    ctx.lineWidth = Math.max(1, b.w * 0.012);
    for (var i = 0; i < 3; i++) {
      var x = b.x + b.w * (0.3 + i * 0.2);
      ctx.beginPath();
      ctx.moveTo(x, topY + b.h * 0.05);
      ctx.lineTo(x + (r() - 0.5) * b.w * 0.05, topY + b.h * (0.8 + r() * 0.6));
      ctx.stroke();
    }
  };

  DRAW.planet = function (ctx, b, P, r) {
    var cx = b.x + b.w * 0.5, cy = b.y + b.h * 0.5, rad = b.h * 0.42;
    glow(ctx, cx, cy, rad * 2.6, P.light(0.18));
    ctx.save();                                        // the far half of the ring
    ctx.beginPath();
    ctx.rect(b.x - b.w, b.y - b.h, b.w * 3, cy - b.y + b.h);
    ctx.clip();
    ellipse(ctx, cx, cy, rad * 1.9, rad * 0.42, P.css([P.sky.light[0], 40, 62], 0.75), -0.24);
    ctx.restore();
    var g = ctx.createRadialGradient(cx - rad * 0.4, cy - rad * 0.4, rad * 0.1, cx, cy, rad);
    g.addColorStop(0, P.css([P.sky.light[0], 52, 62]));
    g.addColorStop(1, P.css([P.sky.top[0], 56, 24]));
    ctx.beginPath();
    ctx.arc(cx, cy, rad, 0, Math.PI * 2);
    ctx.fillStyle = g;
    ctx.fill();
    ctx.save();                                        // the near half, in front
    ctx.beginPath();
    ctx.rect(b.x - b.w, cy, b.w * 3, b.h * 2);
    ctx.clip();
    ellipse(ctx, cx, cy, rad * 1.9, rad * 0.42, P.css([P.sky.light[0], 40, 68], 0.9), -0.24);
    ctx.restore();
  };

  DRAW.bigmoon = function (ctx, b, P, r) {
    var cx = b.x + b.w * 0.5, cy = b.y + b.h * 0.5, rad = b.h * 0.45;
    glow(ctx, cx, cy, rad * 3, P.light(0.22));
    ctx.beginPath();
    ctx.arc(cx, cy, rad, 0, Math.PI * 2);
    ctx.fillStyle = P.light(0.97);
    ctx.fill();
    for (var i = 0; i < 9; i++) {                      // seas and craters
      var a = r() * Math.PI * 2, d = r() * rad * 0.72;
      ctx.globalAlpha = 0.12 + r() * 0.12;
      ctx.beginPath();
      ctx.arc(cx + Math.cos(a) * d, cy + Math.sin(a) * d, rad * (0.08 + r() * 0.2), 0, Math.PI * 2);
      ctx.fillStyle = P.css([228, 26, 44]);
      ctx.fill();
    }
    ctx.globalAlpha = 1;
  };

  DRAW.eye = function (ctx, b, P, r) {
    var cx = b.x + b.w * 0.5, cy = b.y + b.h * 0.5;
    ctx.beginPath();
    ctx.moveTo(b.x, cy);
    ctx.quadraticCurveTo(cx, cy - b.h * 0.62, b.x + b.w, cy);
    ctx.quadraticCurveTo(cx, cy + b.h * 0.62, b.x, cy);
    ctx.closePath();
    ctx.fillStyle = P.light(0.9);
    ctx.fill();
    ctx.save();
    ctx.clip();
    ellipse(ctx, cx, cy, b.h * 0.3, b.h * 0.3, P.css([P.sky.top[0], 70, 44]));
    ellipse(ctx, cx, cy, b.h * 0.13, b.h * 0.13, P.ink(0));
    ctx.restore();
    ctx.strokeStyle = ink(P, b);
    ctx.lineWidth = Math.max(2, b.h * 0.035);
    ctx.beginPath();
    ctx.moveTo(b.x, cy);
    ctx.quadraticCurveTo(cx, cy - b.h * 0.62, b.x + b.w, cy);
    ctx.quadraticCurveTo(cx, cy + b.h * 0.62, b.x, cy);
    ctx.stroke();
  };

  /* ------------------------------------------------------------------ entry */
  function draw(ctx, subject, box, P, r, spec) {
    var fn = DRAW[subject.draw];
    if (!fn) return false;
    ctx.save();
    fn(ctx, box, P, r, spec, subject.form);
    ctx.restore();
    return true;
  }

  var API = { draw: draw, META: META, DRAW: DRAW, QUAD: QUAD, BIRDS: BIRDS, PEOPLE: PEOPLE, TREES: TREES };
  root.CodaSubjects = API;
  if (typeof module !== 'undefined' && module.exports) module.exports = API;
})(typeof window !== 'undefined' ? window : this);
