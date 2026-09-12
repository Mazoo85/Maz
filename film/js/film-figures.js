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
    var handL = segment(ctx, elbowL.x, elbowL.y, pose.armL + pose.foreL, h * 0.18, unit * 2.2, unit * 1.6);
    var elbowR = segment(ctx, torsoTip.x + unit * 5, torsoTip.y + unit * 1.5, pose.armR, h * 0.19, unit * 3, unit * 2.2);
    var handR = segment(ctx, elbowR.x, elbowR.y, pose.armR + pose.foreR, h * 0.18, unit * 2.2, unit * 1.6);

    // head — same rotation the old save/translate/rotate did, folded into the
    // ellipse call itself so the head is one more sub-path of the same fill.
    var headX = torsoTip.x + h * 0.055 * Math.sin(pose.head);
    var headY = torsoTip.y - h * 0.055 * Math.cos(pose.head);
    var headRx = h * 0.052, headRy = h * 0.062;
    ctx.ellipse(headX, headY, headRx, headRy, pose.head, 0, Math.PI * 2);

    ctx.fill();
    // Where the parts ended up, so a face and a held object can be put on top of
    // the silhouette afterwards. Returned rather than recomputed by the caller:
    // the planted-foot shift above moves the whole body, and a face computed
    // from the pose alone would float where the head used to be.
    return {
      shoulderY: shoulderY,
      head: { x: headX, y: headY, rx: headRx, ry: headRy, angle: pose.head },
      handL: { x: handL.x, y: handL.y, angle: pose.armL + pose.foreL },
      handR: { x: handR.x, y: handR.y, angle: pose.armR + pose.foreR }
    };
  }

  /* ----------------------------------------------------------------- faces
   *
   * The figures are silhouettes, and that is the look — so this is not a face
   * drawn on a silhouette, it is LIGHT CATCHING one: two small highlights where
   * the eyes are and a line where the mouth is, in the room's own key colour.
   * From across a room you see a shape; up close you see somebody thinking.
   *
   * It is gated on SIZE, not on framing, and that is deliberate: a head six
   * pixels across cannot hold a face, and any rule expressed in framings has to
   * be plumbed from the director down through the artist and kept in step
   * forever. "Draw it when it is big enough to read" needs no plumbing and
   * cannot fall out of step.
   *
   * Without this there is no reaction shot, and the reaction shot is half of
   * film grammar: the whole point of cutting to somebody is to watch them take
   * something in.
   */
  var FACE_MIN_HEAD = 13;    // px of head radius below which a face is mud

  /* Eyes shut for a moment, about every four seconds, off the figure's own seed
   * so two people in a two-shot never blink together. */
  function blinkAt(seconds, seed) {
    var period = 3.4 + ((seed >>> 3) % 24) * 0.1;
    var phase = ((seconds + (seed % 100) * 0.037) % period) / period;
    return phase > 0.972 ? 1 : 0;
  }

  function drawFace(ctx, p, head, spot) {
    var rx = head.rx, ry = head.ry;
    var open = spot.mouthOpen == null ? 0 : Math.max(0, Math.min(1, spot.mouthOpen));
    var shut = blinkAt(spot.seconds || 0, spot.seed || 0);
    // Eyes look where the head is turning. The head already rotates; shifting
    // the pupils on top of that is the difference between a head pointed at
    // somebody and a person looking at them.
    var lookX = Math.sin(head.angle) * rx * 0.34;

    ctx.save();
    ctx.translate(head.x, head.y);
    ctx.rotate(head.angle);

    var lit = spot.speaking ? 0.85 : 0.6;
    ctx.fillStyle = Art.rgb(p.key, lit);

    var eyeY = -ry * 0.16;
    var eyeDx = rx * 0.36;
    var eyeR = rx * 0.112;
    [-1, 1].forEach(function (side) {
      var x = side * eyeDx + lookX;
      if (shut) {
        // A closed eye is a line, not a dot. Drawing nothing reads as a skull.
        ctx.fillRect(x - eyeR * 1.3, eyeY - eyeR * 0.32, eyeR * 2.6, eyeR * 0.64);
      } else {
        ctx.beginPath();
        ctx.ellipse(x, eyeY, eyeR, eyeR * 1.05, 0, 0, Math.PI * 2);
        ctx.fill();
      }
    });

    // The mouth. Shut it is a line; open it is the same line given height, on
    // the syllable clock the gesture and the score already share.
    var mouthY = ry * 0.40;
    var mouthW = rx * 0.36;
    var mouthH = rx * 0.055 + open * rx * 0.34;
    ctx.fillStyle = Art.rgb(p.key, spot.speaking ? 0.7 : 0.34);
    ctx.beginPath();
    ctx.ellipse(0, mouthY, mouthW * (0.8 + open * 0.2), mouthH * 0.5, 0, 0, Math.PI * 2);
    ctx.fill();

    ctx.restore();
  }

  /* ------------------------------------------------------- something held
   *
   * The whole story turns on an object and until now no character ever touched
   * one: the insert shot drew it floating on its own. A held thing is small, so
   * it is a shape and a colour rather than a drawing — at this size the insert
   * glyphs are mud — and it is drawn in the key colour so it reads against a
   * near-black body.
   */
  var HELD_SHAPES = {
    long: function (ctx, u) { ctx.fillRect(-u * 0.25, -u * 1.5, u * 0.5, u * 3); ctx.fillRect(-u * 0.9, -u * 1.5, u * 1.8, u * 0.9); },
    flat: function (ctx, u) { ctx.fillRect(-u * 1.5, -u, u * 3, u * 2); },
    round: function (ctx, u) { ctx.beginPath(); ctx.arc(0, 0, u * 1.25, 0, Math.PI * 2); ctx.fill(); },
    box: function (ctx, u) { ctx.fillRect(-u * 1.2, -u * 1.1, u * 2.4, u * 2.2); }
  };

  var HELD_FOR = [
    [/key|knife|pen|screwdriver|wrench|torch|flashlight|bottle/, 'long'],
    [/letter|photo|photograph|card|note|map|ticket|page|book|file|receipt/, 'flat'],
    [/ring|coin|watch|stone|ball|locket|medal|disc|record/, 'round']
  ];

  function heldShapeFor(object) {
    var word = String(object || '').toLowerCase();
    for (var i = 0; i < HELD_FOR.length; i++) {
      if (HELD_FOR[i][0].test(word)) return HELD_FOR[i][1];
    }
    return 'box';
  }

  function drawHeld(ctx, p, hand, h, object) {
    var u = h * 0.016;
    if (u < 1.1) return;                       // too small to be anything but a speck
    ctx.save();
    ctx.translate(hand.x, hand.y);
    ctx.rotate(hand.angle);
    ctx.fillStyle = Art.rgb(p.accent, 0.92);
    (HELD_SHAPES[heldShapeFor(object)] || HELD_SHAPES.box)(ctx, u);
    ctx.restore();
  }

  function drawFigure(ctx, p, spot) {
    var h = spot.height;
    var w = h * 0.34;
    var pose = spot.pose || POSES.stand;
    var rim = spot.speaking ? 0.75 : 0.42;

    ctx.save();
    ctx.translate(spot.x, spot.groundY);
    ctx.rotate((spot.wobble || 0) * 0.004);

    // Where the light is, as a signed position across the frame: -1 hard left,
    // +1 hard right, 0 overhead. The set supplies it and it moves — a
    // lighthouse beam sweeps, headlights pass — so the rim and the shadow below
    // swing with the room rather than sitting where they were hardcoded.
    var lightX = spot.lightX == null ? -1 : (spot.lightX < -1 ? -1 : (spot.lightX > 1 ? 1 : spot.lightX));

    // The shadow falls away from the light and lengthens as the light drops
    // toward the horizon, which is what makes a floor read as a floor.
    ctx.fillStyle = 'rgba(0,0,0,0.45)';
    ctx.beginPath();
    ctx.ellipse(-lightX * w * 0.42, 2,
                w * (0.75 + Math.abs(lightX) * 0.35), h * 0.035, 0, 0, Math.PI * 2);
    ctx.fill();

    var halo = ctx.createRadialGradient(0, -h * 0.55, h * 0.05, 0, -h * 0.55, h * 0.75);
    halo.addColorStop(0, Art.rgb(p.key, 0.13));
    halo.addColorStop(1, Art.rgb(p.key, 0));
    ctx.fillStyle = halo;
    ctx.fillRect(-w * 1.6, -h * 1.25, w * 3.2, h * 1.4);

    // rim light: the same body, offset toward the light, in the key colour. This
    // used to be a constant up-and-left — correct for exactly one set and wrong
    // for the other fourteen.
    ctx.save();
    ctx.translate(lightX * w * 0.055, -h * 0.012);
    ctx.fillStyle = Art.rgb(p.key, rim);
    drawBody(ctx, h, pose);
    ctx.restore();

    ctx.fillStyle = 'rgba(6,6,10,0.97)';
    drawBody(ctx, h, pose);

    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    ctx.fillStyle = 'hsla(' + spot.tint + ',65%,58%,' + (spot.speaking ? 0.16 : 0.08) + ')';
    var joints = drawBody(ctx, h, pose);
    ctx.restore();

    // On top of the silhouette, and only when the camera is close enough for
    // either to be anything but a smudge.
    if (spot.holding) {
      // Whichever hand is nearer the camera, so the object is not behind them.
      var hand = joints.handR.x >= joints.handL.x ? joints.handR : joints.handL;
      drawHeld(ctx, p, hand, h, spot.holding);
    }
    // Not on the foreground of an over-the-shoulder shot. That figure is
    // deliberately a dark mass at the edge of frame -- it is what the shot is
    // looking PAST -- and it is drawn big, so the size gate lets a face onto it
    // and the mass becomes a second person staring at the camera.
    if (joints.head.rx >= FACE_MIN_HEAD && !spot.foreground) {
      drawFace(ctx, p, joints.head, spot);
    }

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
  /* Turn a figure toward whoever they are sharing the scene with.
   *
   * Two figures used to face straight out of the screen no matter where the
   * other one stood, which is why a two-shot read as two portraits instead of a
   * conversation. The head turns most, the torso follows about a third as far —
   * people lead with the head — and both are clamped, so no distance and no
   * amount can produce a neck a neck could not do.
   *
   * Positive angles turn toward +x, matching the head rotation that drawBody
   * hands to the canvas. Distance is normalised against a nominal shoulder-to-
   * shoulder span so a figure across the room and a figure an arm away both
   * turn a sensible amount rather than the far one turning further.
   */
  var GAZE_SPAN = 320;   // px at which the turn is essentially full
  var GAZE_HEAD = 0.34;  // radians of head turn at full
  var GAZE_TORSO = 0.11; // the torso follows, less

  /* A standing person is never still.
   *
   * Three slow cycles, all small, all on joints that already exist: weight
   * rocks between the legs, the chest rises and falls, and the head settles
   * after the weight does. This is most of what separates a puppet from
   * somebody waiting for an answer.
   *
   * Everything is driven by the shot clock and the character's seed — no
   * Math.random(), because two recordings of one film have to match frame for
   * frame, and the grain tile already broke that guarantee once. The seed only
   * shifts the phase, so two people in a two-shot are not a chorus line.
   *
   * Bounded to a tenth of each joint's range by the numbers below, and clamped
   * on the way out, so this can never become a pose of its own.
   */
  var BREATH_RATE = 0.55;   // chest cycles per second, a resting adult
  var WEIGHT_RATE = 0.21;   // the slower rock between one leg and the other
  var SETTLE_RATE = 0.13;   // the head, slower still, trailing the weight

  /* Ease from one pose to another instead of snapping at the cut.
   *
   * This existed once and was deleted when nothing called it; the shot-change
   * easing below is the caller it was waiting for. Every joint is clamped on
   * the way out, so a blend can never land somewhere neither pose could —
   * which matters because the two ends are each inside POSE_LIMITS but the
   * shortest path between two angles is not always inside anything.
   *
   * t is clamped rather than extrapolated: overshooting a pose is how you get
   * an elbow through a ribcage.
   */
  /* A walk cycle on the legs that are already there.
   *
   * phase runs 0..1 over one full stride. The two legs are half a cycle apart —
   * in phase they would be a bunny hop — and each knee bends only on its swing,
   * because a knee that bends on the stance leg drops the figure through the
   * floor.
   *
   * drawBody plants the figure by finding the lower foot and shifting the body
   * down to meet the ground, so a cycle that always leaves one leg near
   * straight keeps the walk on the floor instead of bobbing. The test asserts
   * exactly that, using drawBody's own foot arithmetic.
   */
  var STRIDE = 0.34;   // radians the hip swings either side of rest

  function walkAt(pose, phase) {
    var out = {};
    for (var i = 0; i < JOINTS.length; i++) out[JOINTS[i]] = pose[JOINTS[i]];
    var a = (phase || 0) * Math.PI * 2;
    var swingL = Math.sin(a);
    var swingR = Math.sin(a + Math.PI);          // half a cycle behind

    out.legL = clampJoint('legL', pose.legL + swingL * STRIDE);
    out.legR = clampJoint('legR', pose.legR + swingR * STRIDE);
    // The knee folds only while the leg is coming forward. max(0, ...) keeps the
    // stance leg straight, which is what holds the figure on the floor.
    out.shinL = clampJoint('shinL', pose.shinL - Math.max(0, swingL) * 0.44);
    out.shinR = clampJoint('shinR', pose.shinR - Math.max(0, swingR) * 0.44);
    // Arms counter-swing to the opposite leg, which is what makes it read as a
    // walk rather than a shuffle.
    out.armL = clampJoint('armL', pose.armL + swingR * 0.26);
    out.armR = clampJoint('armR', pose.armR + swingL * 0.26);
    return out;
  }

  function blendPoses(a, b, t) {
    var k = t < 0 ? 0 : (t > 1 ? 1 : t);
    var out = {};
    for (var i = 0; i < JOINTS.length; i++) {
      var joint = JOINTS[i];
      out[joint] = clampJoint(joint, a[joint] + (b[joint] - a[joint]) * k);
    }
    return out;
  }

  function aliveAt(pose, seconds, seed) {
    var out = {};
    for (var i = 0; i < JOINTS.length; i++) out[JOINTS[i]] = pose[JOINTS[i]];
    var t = seconds || 0;
    var phase = ((seed || 0) % 17) * 0.37;        // a different point in the cycle each

    var breath = Math.sin((t * BREATH_RATE + phase) * Math.PI * 2);
    var weight = Math.sin((t * WEIGHT_RATE + phase * 0.61) * Math.PI * 2);
    var settle = Math.sin((t * SETTLE_RATE + phase * 1.31) * Math.PI * 2);

    out.torso = clampJoint('torso', pose.torso + breath * 0.022);
    // The legs take the weight in opposition — one straightens as the other gives.
    out.legL = clampJoint('legL', pose.legL + weight * 0.020);
    out.legR = clampJoint('legR', pose.legR - weight * 0.020);
    out.shinL = clampJoint('shinL', pose.shinL - weight * 0.010);
    out.shinR = clampJoint('shinR', pose.shinR + weight * 0.010);
    // The head trails the weight rather than leading it.
    out.head = clampJoint('head', pose.head + settle * 0.026 + breath * 0.008);
    return out;
  }

  function gazeAt(pose, selfX, otherX, amount) {
    var out = {};
    for (var i = 0; i < JOINTS.length; i++) out[JOINTS[i]] = pose[JOINTS[i]];
    var dx = otherX - selfX;
    if (!dx) return out;                       // nobody turns toward themselves
    var strength = Math.max(0, Math.min(1, Math.abs(dx) / GAZE_SPAN));
    var scale = (dx < 0 ? -1 : 1) * strength * Math.max(0, Math.min(1, amount || 0));
    out.head = clampJoint('head', pose.head + GAZE_HEAD * scale);
    out.torso = clampJoint('torso', pose.torso + GAZE_TORSO * scale);
    return out;
  }

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

  /* A figure's own number, from its name: used to stagger the blinks so two
   * people in a two-shot are not a chorus line. */
  function hashName(name) {
    return PARSE.hashText(String(name || ''));
  }

  var API = {
    hashName: hashName,
    drawFace: drawFace,
    drawHeld: drawHeld,
    heldShapeFor: heldShapeFor,
    blinkAt: blinkAt,
    FACE_MIN_HEAD: FACE_MIN_HEAD,
    GLYPHS: GLYPHS,
    glyphFor: glyphFor,
    drawFigure: drawFigure,
    JOINTS: JOINTS,
    POSES: POSES,
    POSE_LIMITS: POSE_LIMITS,
    POSES_BY_BEAT: POSES_BY_BEAT,
    poseFor: poseFor,
    gestureAt: gestureAt,
    gazeAt: gazeAt,
    aliveAt: aliveAt,
    blendPoses: blendPoses,
    walkAt: walkAt
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmFigures = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
