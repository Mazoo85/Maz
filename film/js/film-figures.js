/*
 * SCRIPT FORGE — the figures.
 * ----------------------------
 * Character silhouettes and the insert-shot object glyphs. Moved out of
 * film-art.js, which now keeps only the palette and the film-stock effects.
 *
 * Exposed as window.FilmFigures (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  var Art = root.FilmArt || (typeof require !== 'undefined' ? require('./film-art.js') : {});
  var rgb = Art.rgb;
  var PARSE = root.FilmParse || (typeof require !== 'undefined' ? require('./parse.js') : {});

  /* ------------------------------------------------------------- figures
   * A silhouette, not a blob: head, neck, shoulders, a tapering torso, arms and
   * legs. It is filled near-black and *offset-stroked* in the key colour, which
   * is the cheap old trick for a rim light — the light-coloured copy peeks out
   * on the side the key comes from and the black copy covers the rest.
   */
  /* A tapered segment from (x, y) at `angle`, `length` long, `w0` wide at the
   * root and `w1` at the tip. Adds it as one more closed sub-path of whatever
   * path is currently open (drawBody fills the whole body in one call, since
   * a fill() per limb is the kind of per-frame, per-figure draw-call count a
   * browser's canvas backend can stumble on). Returns the tip, so the next
   * segment hangs off it. */
  function segment(ctx, x, y, angle, length, w0, w1) {
    var dx = Math.sin(angle), dy = Math.cos(angle);
    var nx = dy, ny = -dx;                       // normal, for the taper
    var tipX = x + dx * length, tipY = y + dy * length;
    ctx.moveTo(x + nx * w0, y + ny * w0);
    ctx.lineTo(tipX + nx * w1, tipY + ny * w1);
    ctx.lineTo(tipX - nx * w1, tipY - ny * w1);
    ctx.lineTo(x - nx * w0, y - ny * w0);
    ctx.closePath();
    return { x: tipX, y: tipY };
  }

  /* The whole body, from the hips up and down, in one fill colour. */
  function drawBody(ctx, h, pose) {
    var hipY = -h * 0.46, hipX = 0;
    var shoulderY = -h * 0.72;
    var unit = h * 0.01;
    var legLen = h * 0.24;

    // Plant the figure: a bent leg is shorter end-to-end than a straight one
    // (fixed segment lengths, folded), so its foot stops short of the ground
    // line (y = 0 locally) and the figure would hang in the air. Find where
    // the lower of the two feet actually lands and, if that is above the
    // ground, shift the whole body down by the shortfall — once per call,
    // before any drawing, cheap (four cos() calls, no extra draw work).
    //
    // The cos()/sin() here is a hand-copy of segment()'s own angle
    // convention just below (0 hangs straight down, dy = cos(angle) is the
    // segment's own length along the ground axis) rather than a call to it,
    // because this only needs the tip's y, not a drawn quad. If that
    // convention in segment() ever changes, this must change with it or a
    // planted foot will drift off the ground line again.
    var footYL = hipY + Math.cos(pose.legL) * legLen + Math.cos(pose.legL + pose.shinL) * legLen;
    var footYR = hipY + Math.cos(pose.legR) * legLen + Math.cos(pose.legR + pose.shinR) * legLen;
    var lowestFootY = footYL > footYR ? footYL : footYR;
    if (lowestFootY < 0) {
      var groundShift = -lowestFootY;
      hipY += groundShift;
      shoulderY += groundShift;
    }

    ctx.beginPath();

    // legs — 0 already hangs straight down in segment()'s own convention, so
    // the rest angle here needs no flip, only the torso (which rests upright)
    // does.
    var kneeL = segment(ctx, hipX - unit * 3, hipY, pose.legL, legLen, unit * 4.5, unit * 3.2);
    segment(ctx, kneeL.x, kneeL.y, pose.legL + pose.shinL, legLen, unit * 3.2, unit * 2.4);
    var kneeR = segment(ctx, hipX + unit * 3, hipY, pose.legR, legLen, unit * 4.5, unit * 3.2);
    segment(ctx, kneeR.x, kneeR.y, pose.legR + pose.shinR, legLen, unit * 3.2, unit * 2.4);

    // torso, leaning from the hips
    var torsoTip = segment(ctx, hipX, hipY, Math.PI + pose.torso, h * 0.26, unit * 6.5, unit * 5.5);

    // arms, hung off the shoulders
    var elbowL = segment(ctx, torsoTip.x - unit * 5, torsoTip.y + unit * 1.5, pose.armL, h * 0.19, unit * 3, unit * 2.2);
    segment(ctx, elbowL.x, elbowL.y, pose.armL + pose.foreL, h * 0.18, unit * 2.2, unit * 1.6);
    var elbowR = segment(ctx, torsoTip.x + unit * 5, torsoTip.y + unit * 1.5, pose.armR, h * 0.19, unit * 3, unit * 2.2);
    segment(ctx, elbowR.x, elbowR.y, pose.armR + pose.foreR, h * 0.18, unit * 2.2, unit * 1.6);

    // head — same rotation the old save/translate/rotate did, folded into the
    // ellipse call itself so the head is one more sub-path of the same fill.
    var headX = torsoTip.x + h * 0.055 * Math.sin(pose.head);
    var headY = torsoTip.y - h * 0.055 * Math.cos(pose.head);
    ctx.ellipse(headX, headY, h * 0.052, h * 0.062, pose.head, 0, Math.PI * 2);

    ctx.fill();
    return { shoulderY: shoulderY };
  }

  function drawFigure(ctx, p, spot) {
    var h = spot.height;
    var w = h * 0.34;
    var pose = spot.pose || POSES.stand;
    var rim = spot.speaking ? 0.75 : 0.42;

    ctx.save();
    ctx.translate(spot.x, spot.groundY);
    ctx.rotate((spot.wobble || 0) * 0.004);

    ctx.fillStyle = 'rgba(0,0,0,0.45)';
    ctx.beginPath();
    ctx.ellipse(0, 2, w * 0.75, h * 0.035, 0, 0, Math.PI * 2);
    ctx.fill();

    var halo = ctx.createRadialGradient(0, -h * 0.55, h * 0.05, 0, -h * 0.55, h * 0.75);
    halo.addColorStop(0, Art.rgb(p.key, 0.13));
    halo.addColorStop(1, Art.rgb(p.key, 0));
    ctx.fillStyle = halo;
    ctx.fillRect(-w * 1.6, -h * 1.25, w * 3.2, h * 1.4);

    // rim light: the same body, offset up-left, in the key colour
    ctx.save();
    ctx.translate(-w * 0.055, -h * 0.012);
    ctx.fillStyle = Art.rgb(p.key, rim);
    drawBody(ctx, h, pose);
    ctx.restore();

    ctx.fillStyle = 'rgba(6,6,10,0.97)';
    drawBody(ctx, h, pose);

    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    ctx.fillStyle = 'hsla(' + spot.tint + ',65%,58%,' + (spot.speaking ? 0.16 : 0.08) + ')';
    drawBody(ctx, h, pose);
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

  /* A body is ten joints. A pose is what angle each one holds, in radians from
   * the rest axis: 0 hangs straight down for a limb, upright for head and
   * torso, and positive turns clockwise on screen. */
  var JOINTS = ['head', 'torso', 'armL', 'foreL', 'armR', 'foreR', 'legL', 'shinL', 'legR', 'shinR'];

  /* What a human can do, so a bad pose fails a test instead of looking wrong. */
  var POSE_LIMITS = {
    head:  [-0.6, 0.6],
    torso: [-0.5, 0.5],
    armL:  [-2.6, 2.6], foreL: [-2.4, 0.2],
    armR:  [-2.6, 2.6], foreR: [-0.2, 2.4],
    legL:  [-0.9, 0.9], shinL: [-2.4, 0.1],
    legR:  [-0.9, 0.9], shinR: [-2.4, 0.1]
  };

  var POSES = {
    'stand':            { head: 0,     torso: 0,     armL: 0.12, foreL: -0.10, armR: -0.12, foreR: 0.10, legL: 0.04, shinL: -0.02, legR: -0.04, shinR: -0.02 },
    'hands-in-pockets': { head: -0.06, torso: 0.04,  armL: 0.30, foreL: -0.70, armR: -0.30, foreR: 0.70, legL: 0.05, shinL: -0.02, legR: -0.05, shinR: -0.02 },
    'turn-away':        { head: 0.42,  torso: 0.22,  armL: 0.05, foreL: -0.20, armR: -0.35, foreR: 0.30, legL: 0.10, shinL: -0.04, legR: -0.12, shinR: -0.05 },
    'reach':            { head: -0.16, torso: -0.12, armL: 1.70, foreL: -0.30, armR: -0.20, foreR: 0.18, legL: 0.14, shinL: -0.05, legR: -0.16, shinR: -0.08 },
    'point':            { head: -0.10, torso: -0.06, armL: 1.45, foreL: -0.08, armR: -0.18, foreR: 0.16, legL: 0.08, shinL: -0.03, legR: -0.08, shinR: -0.03 },
    'recoil':           { head: 0.30,  torso: 0.34,  armL: 0.90, foreL: -1.40, armR: -0.85, foreR: 1.35, legL: -0.22, shinL: -0.30, legR: 0.26, shinR: -0.24 },
    'slump':            { head: 0.34,  torso: 0.40,  armL: 0.08, foreL: -0.12, armR: -0.08, foreR: 0.12, legL: 0.06, shinL: -0.10, legR: -0.06, shinR: -0.10 },
    'head-in-hands':    { head: 0.46,  torso: 0.30,  armL: 1.90, foreL: -2.00, armR: -1.90, foreR: 1.95, legL: 0.05, shinL: -0.05, legR: -0.05, shinR: -0.05 },
    'sit':              { head: 0.05,  torso: 0.08,  armL: 0.35, foreL: -0.55, armR: -0.35, foreR: 0.55, legL: 0.85, shinL: -0.85, legR: 0.80, shinR: -0.80 },
    'walk':             { head: -0.04, torso: -0.05, armL: 0.55, foreL: -0.35, armR: -0.55, foreR: 0.35, legL: 0.60, shinL: -0.08, legR: -0.35, shinR: -0.90 }
  };

  /* What a beat looks like on a body. The film's emotional shape reaches the
   * picture through this table and nothing else. */
  var POSES_BY_BEAT = {
    open:   ['stand', 'hands-in-pockets', 'sit'],
    spark:  ['turn-away', 'reach', 'stand'],
    push:   ['walk', 'point', 'reach'],
    turn:   ['stand', 'turn-away', 'hands-in-pockets'],
    crisis: ['recoil', 'slump', 'head-in-hands'],
    choice: ['stand', 'point', 'reach'],
    after:  ['stand', 'hands-in-pockets', 'sit']
  };

  /* One syllable of movement: the head dips and the nearer hand lifts, both
   * returning to rest by the end so syllables can run back to back without the
   * body drifting. The score fires a blip on the same clock. */
  function gestureAt(pose, phase) {
    var clamped = Math.max(0, Math.min(1, phase));
    // Math.sin(Math.PI) is not exactly 0 in double precision, and that residue
    // would keep a syllable from landing exactly back at rest; pin the ends.
    var swing = (clamped <= 0 || clamped >= 1) ? 0 : Math.sin(clamped * Math.PI);   // 0 → 1 → 0
    var out = {};
    for (var i = 0; i < JOINTS.length; i++) out[JOINTS[i]] = pose[JOINTS[i]];

    out.head = clampJoint('head', pose.head - swing * 0.07);
    out.torso = clampJoint('torso', pose.torso - swing * 0.02);
    out.armR = clampJoint('armR', pose.armR - swing * 0.22);
    out.foreR = clampJoint('foreR', pose.foreR + swing * 0.30);
    return out;
  }

  function clampJoint(joint, value) {
    var limit = POSE_LIMITS[joint];
    return value < limit[0] ? limit[0] : value > limit[1] ? limit[1] : value;
  }

  /* Someone mid-sentence faces the room; turning away is a thing you do while
   * someone else is talking. */
  function poseFor(beat, tension, isSpeaker, seed) {
    var pool = POSES_BY_BEAT[beat] || POSES_BY_BEAT.open;
    if (isSpeaker) {
      pool = pool.filter(function (name) { return name !== 'turn-away'; });
      if (!pool.length) pool = ['stand'];
    }
    // High tension leans on the later entries, which are the more extreme ones.
    var rng = PARSE.makeRng((PARSE.hashText(beat + ':' + seed) ^ Math.round(tension * 1000)) >>> 0);
    var bias = Math.min(0.999, Math.max(0, rng() * (1 - tension * 0.35) + tension * 0.35));
    return pool[Math.floor(bias * pool.length) % pool.length];
  }

  var API = {
    GLYPHS: GLYPHS,
    glyphFor: glyphFor,
    drawFigure: drawFigure,
    JOINTS: JOINTS,
    POSES: POSES,
    POSE_LIMITS: POSE_LIMITS,
    POSES_BY_BEAT: POSES_BY_BEAT,
    poseFor: poseFor,
    gestureAt: gestureAt
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmFigures = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
