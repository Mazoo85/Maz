/*
 * SCRIPT FORGE — the camera.
 * --------------------------
 * Draws any moment of a reel onto a canvas, and plays a reel in real time.
 *
 *   FilmPlayer.drawFrame(ctx, w, h, reel, time)   one frame, any time, no state
 *   new FilmPlayer.Player(canvas, reel, hooks)    play / stop / seek
 *
 * drawFrame is deliberately pure: the recorder, the poster frame and the live
 * playback all call the same function, so what you record is exactly what you
 * watched.
 */
(function (root) {
  'use strict';

  var Art = root.FilmArt || (typeof require !== 'undefined' ? require('./film-art.js') : {});
  var Sets = root.FilmSets || (typeof require !== 'undefined' ? require('./film-sets.js') : {});
  var Figures = root.FilmFigures || (typeof require !== 'undefined' ? require('./film-figures.js') : {});
  var Weather = root.FilmWeather || (typeof require !== 'undefined' ? require('./film-weather.js') : {});
  var Reel = root.FilmReel || (typeof require !== 'undefined' ? require('./film-reel.js') : {});
  var PARSE = root.FilmParse || (typeof require !== 'undefined' ? require('./parse.js') : {});

  var WORLD_W = 1000;
  var WORLD_H = 420;
  var ASPECT = 2.35;          // letterboxed widescreen
  var FADE = 0.45;            // seconds of dip-to-black between scenes

  function easeInOut(t) {
    return t < 0.5 ? 2 * t * t : 1 - Math.pow(-2 * t + 2, 2) / 2;
  }

  function clamp01(v) {
    return v < 0 ? 0 : v > 1 ? 1 : v;
  }

  /* ------------------------------------------------------------ typography */
  function wrapLines(ctx, text, maxWidth, maxLines) {
    var words = String(text).split(/\s+/);
    var lines = [];
    var line = '';
    for (var i = 0; i < words.length; i++) {
      var next = line ? line + ' ' + words[i] : words[i];
      if (ctx.measureText(next).width <= maxWidth || !line) {
        line = next;
      } else {
        lines.push(line);
        line = words[i];
        if (maxLines && lines.length === maxLines) break;
      }
    }
    if (line && (!maxLines || lines.length < maxLines)) lines.push(line);
    return lines;
  }

  function shadowedText(ctx, text, x, y, align) {
    ctx.textAlign = align || 'center';
    ctx.shadowColor = 'rgba(0,0,0,0.85)';
    ctx.shadowBlur = 14;
    ctx.shadowOffsetY = 2;
    ctx.fillText(text, x, y);
    ctx.shadowBlur = 0;
    ctx.shadowOffsetY = 0;
  }

  /* --------------------------------------------------------------- framing
   * How close the camera is, and where it is pointed, for each shot type.
   *
   * `time` is the film's own elapsed seconds — the clock handheld and whip
   * need, since `progress` alone (0..1 through the shot) would make two
   * shots of different lengths shake at different speeds. It is optional
   * only for callers that never ask for a moving camera; it defaults to 0.
   *
   * `panY` here is a framing *choice* — where the frame sits vertically —
   * never camera motion, so it stays out of `panYMove` below and drawFrame
   * applies it flat, the same on every plane. `panYMove` is vertical camera
   * *motion* (currently only handheld's shake); drawFrame parallaxes it
   * across planes exactly like panX, because a real handheld camera's shake
   * moves near things more than far ones. Keeping the two separate means a
   * still close-up's -0.05 offset never pulls the planes apart, while a
   * handheld shot's shake still reads as depth. */
  function framingFor(shot, progress, time) {
    var p = easeInOut(clamp01(progress));
    var t = time == null ? 0 : time;
    var zoom = 1;
    var panX = 0;
    var panY = 0;
    var panYMove = 0;

    if (shot.framing === 'mid') zoom = 1.35;
    else if (shot.framing === 'two') zoom = 1.5;
    else if (shot.framing === 'close') zoom = 1.95;
    else if (shot.framing === 'insert') zoom = 1.15;
    else if (shot.framing === 'ots') zoom = 1.75;
    else if (shot.framing === 'low') zoom = 1.45;

    // A close-up looks at whoever is speaking, so the frame sits on their side.
    if (shot.framing === 'close' && shot.speaker) panX = (shot.side || 0) * 0.16;
    if (shot.framing === 'close') panY = -0.05;

    switch (shot.camera) {
      case 'push': zoom *= 1 + 0.13 * p; break;
      case 'push-slow': zoom *= 1 + 0.06 * p; break;
      case 'pull': zoom *= 1.16 - 0.16 * p; break;
      case 'pan-l': panX += 0.09 - 0.18 * p; break;
      case 'pan-r': panX += -0.09 + 0.18 * p; break;
      case 'handheld':
        // Three detuned sines beat against each other, so it never loops
        // visibly. Tension decides how far it drifts.
        var shake = 0.004 + (shot.mood || 0.4) * 0.020;
        panX += Math.sin(t * 2.7) * shake + Math.sin(t * 6.1) * shake * 0.4;
        panYMove += Math.cos(t * 3.3) * shake * 0.8 + Math.sin(t * 5.2) * shake * 0.3;
        zoom *= 1 + 0.01 * p;
        break;
      case 'track-l': panX += 0.13 - 0.26 * p; break;
      case 'track-r': panX += -0.13 + 0.26 * p; break;
      case 'whip':
        // Fast at the start, settling: the tail of a whip pan, not the
        // middle. The back plane has no overscan margin of its own (every
        // set's `back` fills exactly world x[0..1000], no draw past the
        // edge to uncover), so whatever raw panX this move reaches, scaled
        // by PARALLAX.back (0.35), is exactly how much bare edge shows.
        // pan-l/pan-r reach a raw panX of 0.09 at their most extreme point:
        // a back-plane exposure of 0.09 * 0.35 = 3.15% of the frame. (An
        // earlier version of this comment quoted that worst case as 9% — it
        // read the raw panX itself as if it were already a screen fraction,
        // without PARALLAX.back's multiplier.) 0.09 here, in place of the
        // 0.23 first tried, keeps the whip at that 3.15% figure: this move
        // adds no zoom of its own, so unlike push/pull there is no overscan
        // covering for a wider swing.
        //
        // 3.15% is not the true worst case across every move, though — only
        // the one the whip is deliberately held to. track-l/track-r reach a
        // raw panX of 0.13, a back-plane exposure of 0.13 * 0.35 = 4.55%
        // (confirmed on an actual render, every set, wide framing: ~4.5%,
        // the small gap from 4.55% being pixel rounding at the sampled
        // resolution) — a genuinely larger bare edge, and the real ceiling
        // in this file today. It is left larger on purpose: a tracking shot
        // that barely travels reads worse than the extra sliver of bare
        // edge, so track's own swing was not trimmed to match pan-l/pan-r,
        // and the whip is not trimmed to match track either — each move is
        // pinned to whichever established ceiling actually motivated its
        // number, not to the single largest number every other move happens
        // to reach.
        panX += 0.09 * Math.pow(1 - p, 3);
        break;
      default: zoom *= 1 + 0.018 * p; break; // never perfectly still
    }

    // A couple of degrees of roll, and only where the story has come apart.
    var roll = (shot.mood || 0) > 0.7 ? ((shot.mood - 0.7) / 0.3) * 0.035 : 0;
    return { zoom: zoom, panX: panX, panY: panY, panYMove: panYMove, roll: roll };
  }

  // The reel is meant to stay plain, browser-free data (a native engine may
  // consume it later), so the syllable count is never memoized onto the
  // shot; it is cheap arithmetic (PARSE.syllablesFor) and the frame budget
  // has ample room to redo it every frame that needs it.
  function syllablesForShot(shot) {
    return PARSE.syllablesFor(shot.caption);
  }

  /* ---------------------------------------------------------- rack focus
   * On a close-up or two-shot, the fore element (one dark shape near the
   * lens — a table edge, a fence post, a branch) is meant to fall out of
   * focus, so the eye goes to the character rather than the foreground. A
   * real per-pixel blur was measured at ~4.5x cost scaling with pixel count
   * and deleted; this fakes the same read cheaply by lifting the shape
   * toward the palette's `deep` tone instead — hazier and lower-contrast,
   * the way an out-of-focus dark shape in front of a lens goes, never
   * darker (it is already drawn at ~90% black; there is no room to darken
   * it further).
   *
   * The lift must never touch anything it isn't the fore shape — not the
   * set behind it, not a figure, not the letterbox. `FilmSets`'s `fore`
   * functions now take that as a fourth argument (`lift`) and do the
   * recoloring themselves — see `foreInk` in film-sets.js for how, and why
   * that is exact, not approximate. (An earlier version of this fix
   * isolated the shape in an offscreen layer and composited it back with
   * `source-atop`, which is the more obvious way to confine an effect to
   * exactly the pixels a draw call touched — but it cost ~4-5ms per
   * close-up at 1920x1080, mostly `drawImage` copying a mostly-transparent
   * canvas-sized layer back onto the frame every single frame, near the
   * entire 6ms budget for one shot. `foreInk`'s route costs the same as
   * drawing the shape at all.)
   *
   * Most sets draw nothing inside the tight crop a close-up or two-shot
   * actually shows, so asking for the lift there would be free but pointless
   * work. Rather than a hand-written list of which sets qualify — a
   * snapshot that goes stale the moment a set's decor, or the camera's pan
   * range for that framing, changes — `foreCoversFraming` below answers the
   * question by actually drawing the set's `fore` once and checking whether
   * any pixel of it falls inside the world-space window that framing can
   * ever show, over every camera move this file knows and (for `close`)
   * either speaker side. It is cached per set + framing, so the one real
   * draw+readback it costs is paid once, not per frame. */
  var CAMERAS_FOR_COVERAGE = ['push', 'push-slow', 'pull', 'pan-l', 'pan-r',
    'handheld', 'track-l', 'track-r', 'whip', 'static'];

  // The four corners of the visible frame, in the frame-relative units this
  // function works in: (0,0) is the frame centre, (±0.5,±0.5) its edges.
  var FRAME_CORNERS = [[-0.5, -0.5], [0.5, -0.5], [-0.5, 0.5], [0.5, 0.5]];

  // The world-space rectangle the fore plane can show for a framing, over
  // every camera move (and, for close-ups, either side the frame can sit
  // on). Built from `framingFor` itself — the actual pan/zoom math — rather
  // than a second copy of its formulas, so a change there is picked up
  // automatically. This is the inverse of `plane()`'s own transform *and*
  // the Dutch-tilt rotation drawFrame applies ahead of it (see `cam.roll`
  // and the `ctx.rotate` around line 335): given a camera's zoom/pan/roll,
  // which world rectangle lands inside the visible frame.
  //
  // Because roll rotates the picture about the frame's own centre, the
  // frame's four corners no longer invert to world space independently by
  // axis (screen x=0 no longer maps to a single world x regardless of y) —
  // so each of the four corners is inverted as a pair and the bounding box
  // is taken over all of them, rather than four independent edges. Solving
  // screen = frameCentre + R(roll) * (planeSpace - frameCentre) for the
  // world point behind a given screen corner gives, per axis:
  //   world.x = WORLD_W/2 + ux*(WORLD_W/zoom)*cos(roll) + uy*(K/zoom)*sin(roll)
  //             - panX*rate*(WORLD_W/zoom)
  //   world.y = WORLD_H/2 - ux*(WORLD_W/zoom)*sin(roll) + uy*(K/zoom)*cos(roll)
  //             - vertPan*(K/zoom)
  // where (ux,uy) is the corner in frame-relative units above. At roll = 0
  // this collapses back to the plain left/right/top/bottom edges the
  // un-rolled version of this function used.
  //
  // shot.mood is fixed at 1 (the same synthetic value already used for
  // camera range above), which is also the highest mood this function ever
  // needs to sample for roll: roll only exists above mood 0.7 and grows
  // monotonically with it, so mood 1 is roll's own worst case too.
  function foreVisibleWorldRect(framing) {
    var rate = Sets.PARALLAX.fore;
    var K = WORLD_W / ASPECT; // frameH/frameW is always 1/ASPECT once letterboxed
    var x0 = Infinity, x1 = -Infinity, y0 = Infinity, y1 = -Infinity;
    var sides = framing === 'close' ? [-1, 1] : [0];
    var progresses = [0, 0.2, 0.4, 0.6, 0.8, 1];
    var times = [0, 0.9, 1.8, 2.7, 3.6, 4.5, 5.4, 6.3];
    CAMERAS_FOR_COVERAGE.forEach(function (camera) {
      sides.forEach(function (side) {
        progresses.forEach(function (progress) {
          times.forEach(function (time) {
            var shot = {
              framing: framing, camera: camera, side: side, mood: 1,
              speaker: framing === 'close' ? 'A' : null
            };
            var cam = framingFor(shot, progress, time);
            var vertPan = cam.panY + cam.panYMove * rate;
            var cosR = Math.cos(cam.roll), sinR = Math.sin(cam.roll);
            FRAME_CORNERS.forEach(function (corner) {
              var ux = corner[0], uy = corner[1];
              var wx = WORLD_W / 2 + ux * (WORLD_W / cam.zoom) * cosR + uy * (K / cam.zoom) * sinR
                - cam.panX * rate * (WORLD_W / cam.zoom);
              var wy = WORLD_H / 2 - ux * (WORLD_W / cam.zoom) * sinR + uy * (K / cam.zoom) * cosR
                - vertPan * (K / cam.zoom);
              if (wx < x0) x0 = wx;
              if (wx > x1) x1 = wx;
              if (wy < y0) y0 = wy;
              if (wy > y1) y1 = wy;
            });
          });
        });
      });
    });
    return { x0: x0, y0: y0, x1: x1, y1: y1 };
  }

  var DEFAULT_PAL = Art.palette('drama', 'NIGHT', 0.4);
  var DEFAULT_GRAIN = Art.noise('fore-coverage-mask', 80);
  var foreCoverageCache = {};

  /* Does `setName`'s fore drawing put any non-transparent pixel inside the
   * world-space window a close-up ('close') or two-shot ('two') can ever
   * show? None of the fore drawings key their shape off the palette or the
   * grain — every one fills a fixed rgba(0,0,0,x) — so the same measurement
   * is valid for every reel; only `setName` and `framing` matter, hence the
   * cache. `doc` is needed to build the offscreen canvas the measurement
   * draws into; without one (no live document — shouldn't happen outside a
   * browser, but drawFrame is never called outside one) this assumes the
   * worst rather than silently skipping the effect. */
  function foreCoversFraming(doc, setName, set, framing) {
    var cacheKey = setName + ':' + framing;
    if (foreCoverageCache.hasOwnProperty(cacheKey)) return foreCoverageCache[cacheKey];
    var result = true;
    if (doc) {
      var c = doc.createElement('canvas');
      c.width = WORLD_W;
      c.height = WORLD_H;
      var mctx = c.getContext('2d');
      set.fore(mctx, DEFAULT_PAL, DEFAULT_GRAIN);
      var rect = foreVisibleWorldRect(framing);
      var rx0 = Math.max(0, Math.floor(rect.x0)), rx1 = Math.min(WORLD_W, Math.ceil(rect.x1));
      var ry0 = Math.max(0, Math.floor(rect.y0)), ry1 = Math.min(WORLD_H, Math.ceil(rect.y1));
      result = false;
      if (rx1 > rx0 && ry1 > ry0) {
        var data = mctx.getImageData(rx0, ry0, rx1 - rx0, ry1 - ry0).data;
        for (var i = 3; i < data.length; i += 4) {
          if (data[i] !== 0) { result = true; break; }
        }
      }
    }
    foreCoverageCache[cacheKey] = result;
    return result;
  }

  /* Where the figures stand, and how tall they are, for each framing. */
  function figureLayout(shot) {
    var count = (shot.characters || []).length;
    if (!count) return [];
    if (shot.framing === 'close') {
      // A close-up sits the head in the upper third and lets the frame cut the
      // body off at the chest, the way a real one does.
      return [{ name: shot.speaker || shot.characters[0], x: 520, ground: 530, height: 340 }];
    }
    if (shot.framing === 'ots') {
      // The listener is a big dark shape at the edge; the speaker is beyond them.
      return [
        { name: shot.speaker, x: 610, ground: 372, height: 250 },
        { name: (shot.characters || []).filter(function (n) { return n !== shot.speaker; })[0] || shot.speaker,
          x: 250, ground: 470, height: 430, foreground: true }
      ];
    }
    if (shot.framing === 'low') {
      return [{ name: shot.speaker || shot.characters[0], x: 500, ground: 420, height: 330 }];
    }
    if (count === 1) {
      return [{ name: shot.characters[0], x: 560, ground: 356, height: shot.framing === 'mid' ? 210 : 170 }];
    }
    return [
      { name: shot.characters[0], x: 360, ground: 356, height: shot.framing === 'mid' ? 205 : 168 },
      { name: shot.characters[1], x: 660, ground: 350, height: shot.framing === 'mid' ? 198 : 162 }
    ];
  }

  /* --------------------------------------------------------------- a frame
   * `opts.rackFocus` (default true, if the shot's set and framing warrant
   * it — see `foreCoversFraming` above) can be forced false to disable the
   * close/two-shot fore lift. Nothing else in drawFrame reads `opts`; it
   * exists for the pixel-identity test in film-browser.test.js — proving
   * the lift never touches anything outside the fore element means
   * comparing a frame against itself with only the lift switched off,
   * everything else about the call identical. */
  /* How long a figure takes to settle into the pose a new shot puts them in.
   * Short enough to read as a person moving, long enough not to be a snap. */
  var POSE_EASE = 0.32;

  /* A walk: strides per second, and how far across the frame it carries them.
   * Travel is in the same world units drawFrame lays its figures out in. */
  var WALK_RATE = 0.85;
  var WALK_TRAVEL = 190;

  function drawFrame(ctx, width, height, reel, time, opts) {
    var shot = Reel.shotAt(reel, time);
    var elapsed = time - shot.start;
    var progress = clamp01(elapsed / shot.duration);
    var pal = Art.palette(reel.genre, shot.time, shot.mood);

    // The letterboxed window the film plays inside.
    var frameW = width;
    var frameH = Math.min(height, width / ASPECT);
    var frameY = (height - frameH) / 2;

    ctx.save();
    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, width, height);
    ctx.beginPath();
    ctx.rect(0, frameY, frameW, frameH);
    ctx.clip();

    var cam = framingFor(shot, progress, time);
    var scale = (frameW / WORLD_W) * cam.zoom;

    // A Dutch tilt rotates the picture — the set and figure planes — about
    // the centre of the frame, before any plane's own translate into world
    // space below, the way a real camera roll tilts everything the lens
    // sees. It is applied to the shared ctx, ahead of `plane`, rather than
    // inside it, so it is one rotation for the whole shot rather than a
    // per-plane one that could drift the planes apart at their corners.
    //
    // It is scoped to its own save/restore around just the picture, not the
    // whole frame: the weather, the light leak, the vignette, the grain and
    // the captions are drawn afterwards, once the roll has been undone, so
    // a caption never tilts with the lens and the vignette and light leak —
    // both exactly frame-sized rects — keep covering the frame exactly
    // rather than leaving unrotated wedges at the corners.
    ctx.save();
    if (cam.roll) {
      ctx.translate(frameW / 2, frameY + frameH / 2);
      ctx.rotate(cam.roll);
      ctx.translate(-frameW / 2, -(frameY + frameH / 2));
    }

    // Each plane gets its own transform, scaled by its own parallax rate —
    // back barely shifts under a pan, fore swings, and that difference in
    // speed is what tells the eye the picture has depth. At rest (panX/panY
    // at 0) every plane's translate collapses to the same point, so the
    // figures — drawn at the mid rate — land exactly where the old single-
    // plane transform put them.
    //
    // panY is treated differently from panYMove: panY is a framing choice
    // (the close-up's constant -0.05 lift) and stays flat across every
    // plane, the same way it always has — scaling a constant offset by rate
    // would shift the planes apart even on a shot with no camera move at
    // all. panYMove is vertical camera *motion* (handheld's shake) and gets
    // the same per-plane rate scaling panX already had, so a shaky frame
    // shows the same depth a shaky pan already does.
    function plane(rate, paint) {
      ctx.save();
      ctx.translate(
        frameW / 2 + cam.panX * frameW * rate,
        frameY + frameH / 2 + cam.panY * frameH + cam.panYMove * frameH * rate);
      ctx.scale(scale, scale);
      ctx.translate(-WORLD_W / 2, -WORLD_H / 2);
      paint();
      ctx.restore();
    }

    var setKey = Sets.SETS[shot.set] ? shot.set : 'room';
    var set = Sets.SETS[setKey];
    var grain = Art.noise('set-' + shot.set + '-' + reel.seed, 80);

    // Where the light is, this instant. A lighthouse sweep and a passing car move it; a kitchen
    // bulb does not. The figures have to be lit by the same source that lights the room — that is
    // the whole point — and the light-leak wash further down needs it too.
    //
    // Declared HERE rather than inside the branch below. It used to sit with the figures, which are
    // only painted for a non-insert shot, while the light leak reads `light.offset` unconditionally
    // 130 lines later: so every insert shot threw "Cannot read properties of undefined". `var` being
    // function-scoped is what hid it — the name existed, so nothing complained until it was read.
    // Found by porting this renderer to the engine and rendering a film that has an insert in it.
    var light = Sets.lightAt(Sets.LIGHT[shot.set] || 'none', time, shot.mood);

    if (shot.framing !== 'insert') {
      plane(Sets.PARALLAX.back, function () { set.back(ctx, pal, grain); });
      plane(Sets.PARALLAX.mid, function () { set.mid(ctx, pal, grain); });

      var spots = figureLayout(shot);

      function paintFigures(wantForeground) {
        spots.forEach(function (spot) {
          if (!!spot.foreground !== wantForeground) return;
          var voice = reel.voices[spot.name] || { hue: 200 };
          var speaking = shot.kind === 'line' && spot.name === shot.speaker;
          // A speaking figure breathes a little faster than a listening one.
          var wobble = Math.sin(time * (speaking ? 5.2 : 1.7) + spot.x) * (speaking ? 1.5 : 0.7);
          var shotKey = (reel.seed + Math.round(shot.start * 100)) >>> 0;
          var poseName = Figures.poseFor(shot.beat, shot.mood, speaking, shotKey);
          var pose = Figures.POSES[poseName];

          // Ease in from the pose the previous shot left them in, rather than
          // snapping at the cut. Derived from the reel, not remembered between
          // frames: drawFrame has to stay a pure function of (reel, time) or
          // seeking and recording would disagree with playback.
          var prev = Reel.shotAt(reel, Math.max(0, shot.start - 0.001));
          if (prev && prev !== shot) {
            var into = time - shot.start;
            if (into < POSE_EASE) {
              var prevKey = (reel.seed + Math.round(prev.start * 100)) >>> 0;
              var prevSpeaking = prev.kind === 'line' && spot.name === prev.speaker;
              var prevPose = Figures.POSES[
                Figures.poseFor(prev.beat, prev.mood, prevSpeaking, prevKey)];
              pose = Figures.blendPoses(prevPose, pose, into / POSE_EASE);
            }
          }

          // Look at whoever else is in the scene. Two figures used to face
          // straight out of the screen no matter where the other one stood,
          // which is what made a two-shot read as two portraits rather than a
          // conversation. The speaker turns further than the listener; someone
          // alone in the frame has nobody to turn to and stays as posed.
          var other = null;
          for (var s = 0; s < spots.length; s++) {
            if (spots[s] !== spot) { other = spots[s]; break; }
          }
          if (other) pose = Figures.gazeAt(pose, spot.x, other.x, speaking ? 1 : 0.55);

          // ...and is never perfectly still while doing it. Seeded off the
          // figure's own x so two people in a two-shot are not a chorus line.
          pose = Figures.aliveAt(pose, time, Math.round(spot.x));

          // The push beat is the one about momentum, so on it a character
          // actually crosses part of the frame rather than standing in it.
          // Walking overrides the breath on the legs, which is why it comes
          // after: you do not idly shift your weight while striding.
          var driftX = 0;
          if (shot.beat === 'push' && !spot.foreground) {
            var walkInto = Math.max(0, time - shot.start);
            // Walk from the walking pose, not from whatever the beat picked.
            // The push beat's usual pose is 'reach', whose arm is 1.7 radians —
            // straight out — and a stride on top of that is a zombie, which is
            // exactly what the first render of this looked like.
            pose = Figures.walkAt(Figures.aliveAt(Figures.POSES.walk, time, Math.round(spot.x)),
                                  (walkInto * WALK_RATE) % 1);
            if (other) pose = Figures.gazeAt(pose, spot.x, other.x, 0.35);
            var across = Math.min(1, walkInto / Math.max(0.6, shot.duration));
            driftX = (across - 0.5) * WALK_TRAVEL * (spot.x < 0.5 ? 1 : -1);
          }

          // A speaking figure's head and hand move in time with their own voice
          // — the score fires a blip on this same clock, so the two must agree.
          // The mouth is on that same clock, for the same reason: a mouth that
          // opens out of step with the gesture and the sound reads as a dub.
          var mouthOpen = 0;
          if (speaking && shot.kind === 'line') {
            var syllables = syllablesForShot(shot);
            var span = Math.max(0.4, shot.duration * 0.78);
            var gap = span / syllables;
            var into = time - shot.start;
            if (into < span) {
              var phase = (into % gap) / gap;
              pose = Figures.gestureAt(pose, phase);
              mouthOpen = Math.sin(phase * Math.PI);
            }
          }

          Figures.drawFigure(ctx, pal, {
            x: spot.x + driftX, groundY: spot.ground, height: spot.height,
            tint: voice.hue, speaking: speaking, wobble: wobble,
            pose: pose, lightX: light.offset,
            // For the face: the clock it blinks and speaks on, and a seed of its
            // own so two people in a two-shot never blink together.
            seconds: time, seed: Figures.hashName(spot.name),
            mouthOpen: mouthOpen,
            // And the thing they are carrying, if this scene says they have it.
            holding: (shot.holding && shot.holding.by === spot.name) ? shot.holding.what : null
          });
        });
      }
      var rackFocus = (shot.framing === 'close' || shot.framing === 'two') &&
        (!opts || opts.rackFocus !== false) &&
        foreCoversFraming(ctx.canvas && ctx.canvas.ownerDocument, setKey, set, shot.framing);
      plane(Sets.PARALLAX.mid, function () { paintFigures(false); });
      plane(Sets.PARALLAX.fore, function () {
        set.fore(ctx, pal, grain, rackFocus);
        paintFigures(true);
      });
    } else {
      // insert: the set still shows faintly behind the object, but there is
      // no camera depth to it — one plane, as before.
      plane(Sets.PARALLAX.mid, function () {
        set.back(ctx, pal, grain);
        set.mid(ctx, pal, grain);
        set.fore(ctx, pal, grain);
        ctx.fillStyle = 'rgba(0,0,0,0.55)';
        ctx.fillRect(0, 0, WORLD_W, WORLD_H);
        var glow = ctx.createRadialGradient(500, 220, 10, 500, 220, 330);
        glow.addColorStop(0, Art.rgb(pal.key, 0.30));
        glow.addColorStop(1, Art.rgb(pal.key, 0));
        ctx.fillStyle = glow;
        ctx.fillRect(120, 0, 760, WORLD_H);
        ctx.save();
        ctx.translate(500, 220);
        ctx.scale(0.9, 0.9);
        Figures.glyphFor(reel.object)(ctx, pal);
        ctx.restore();
      });
    }

    // Undoes the Dutch-tilt save above: everything from here on — the
    // weather, the light leak, the vignette, the captions and the grain —
    // is drawn in the plain, unrotated frame.
    ctx.restore();

    // Air between the audience and the picture — rain, dust, fog, shimmer,
    // embers or haze, chosen by genre and hour. Drawn in screen space (not
    // inside any world plane, and not rotated with the roll above) so it
    // never pans, scales or tilts with the set — the one camera move it
    // still doesn't follow is the whip/track pan, and that stays exactly as
    // it is: weather reads as air between the audience and the lens, not
    // part of the set, so it never moving with the camera is the point, not
    // an oversight this fix should touch.
    ctx.save();
    ctx.translate(0, frameY);
    Weather.draw(ctx, Weather.forShot(reel.genre, shot.time, shot.set),
      pal, time, reel.seed, frameW, frameH);
    ctx.restore();

    // light leak from the key, and a vignette to hold the eye in the middle.
    // A set that owns a moving light (a lighthouse sweep, headlights, a
    // failing bulb, cloud shadow) folds its brightness into this wash's
    // alpha and its offset into where the wash is centred, rather than
    // getting its own draw call — that keeps it out of the weather's and
    // the vignette's way, both of which are drawn in this same screen space.
    var leakX = light.offset * frameW * 0.3;
    var leak = ctx.createLinearGradient(leakX, frameY, leakX + frameW * 0.7, frameY + frameH);
    leak.addColorStop(0, Art.rgb(pal.key, (0.10 + pal.tension * 0.05) * light.brightness));
    leak.addColorStop(1, Art.rgb(pal.key, 0));
    ctx.fillStyle = leak;
    ctx.fillRect(0, frameY, frameW, frameH);

    var vig = ctx.createRadialGradient(
      frameW / 2, frameY + frameH / 2, frameH * 0.28,
      frameW / 2, frameY + frameH / 2, frameH * 0.95);
    vig.addColorStop(0, 'rgba(0,0,0,0)');
    vig.addColorStop(1, 'rgba(0,0,0,' + (0.55 + pal.tension * 0.2) + ')');
    ctx.fillStyle = vig;
    ctx.fillRect(0, frameY, frameW, frameH);

    drawCaptions(ctx, frameW, frameH, frameY, shot, pal, progress, reel);
    drawGrain(ctx, frameW, frameH, frameY, time, pal);

    ctx.restore();

    // letterbox bars sit outside the clip, over everything
    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, width, frameY);
    ctx.fillRect(0, frameY + frameH, width, height - frameY - frameH);

    // dip to black across a cut between scenes, and at the two ends
    var fade = fadeAmount(reel, shot, time);
    if (fade > 0) {
      ctx.fillStyle = 'rgba(0,0,0,' + fade + ')';
      ctx.fillRect(0, 0, width, height);
    }
    return shot;
  }

  /* Black at the head and tail of the film, and a dip on every scene change. */
  function fadeAmount(reel, shot, time) {
    if (time < FADE) return 1 - time / FADE;
    if (time > reel.duration - FADE) return clamp01((time - (reel.duration - FADE)) / FADE);

    var next = reel.shots[shot.index + 1];
    var prev = reel.shots[shot.index - 1];
    var out = 0;
    if (next && next.scene !== shot.scene) {
      var toEnd = shot.start + shot.duration - time;
      if (toEnd < FADE / 2) out = Math.max(out, 1 - toEnd / (FADE / 2));
    }
    if (prev && prev.scene !== shot.scene) {
      var since = time - shot.start;
      if (since < FADE / 2) out = Math.max(out, 1 - since / (FADE / 2));
    }
    return out;
  }

  /* -------------------------------------------------------------- captions */
  function drawCaptions(ctx, w, h, y, shot, pal, progress, reel) {
    var unit = h / 420;                       // type scales with the frame
    var fadeIn = clamp01(progress / 0.12);
    var fadeOut = 1 - clamp01((progress - 0.88) / 0.12);
    var alpha = Math.min(fadeIn, fadeOut);
    ctx.save();
    ctx.globalAlpha = alpha;

    if (shot.kind === 'title') {
      ctx.font = '700 ' + Math.round(52 * unit) + 'px "Trebuchet MS", system-ui, sans-serif';
      ctx.fillStyle = '#fff';
      var titleLines = wrapLines(ctx, shot.caption, w * 0.8, 3);
      titleLines.forEach(function (line, i) {
        shadowedText(ctx, line, w / 2, y + h * 0.46 + i * 60 * unit);
      });
      ctx.font = '600 ' + Math.round(17 * unit) + 'px "Courier New", monospace';
      ctx.fillStyle = Art.rgb(pal.key, 0.9);
      shadowedText(ctx, String(shot.subcaption || '').toUpperCase(),
        w / 2, y + h * 0.46 + titleLines.length * 60 * unit + 16 * unit);
    } else if (shot.kind === 'end') {
      ctx.font = '700 ' + Math.round(40 * unit) + 'px "Trebuchet MS", system-ui, sans-serif';
      ctx.fillStyle = '#fff';
      shadowedText(ctx, shot.caption, w / 2, y + h * 0.48);
      ctx.font = '400 ' + Math.round(15 * unit) + 'px "Courier New", monospace';
      ctx.fillStyle = Art.rgb(pal.key, 0.75);
      shadowedText(ctx, String(shot.subcaption || ''), w / 2, y + h * 0.60);
    } else if (shot.kind === 'establish') {
      // The slug line, bottom left, like a location stamp.
      ctx.font = '700 ' + Math.round(16 * unit) + 'px "Courier New", monospace';
      var text = shot.caption.toUpperCase();
      var tw = ctx.measureText(text).width;
      ctx.fillStyle = 'rgba(0,0,0,0.55)';
      ctx.fillRect(w * 0.06 - 10 * unit, y + h * 0.80, tw + 28 * unit, 30 * unit);
      ctx.fillStyle = Art.rgb(pal.key);
      ctx.fillRect(w * 0.06 - 10 * unit, y + h * 0.80, 4 * unit, 30 * unit);
      ctx.fillStyle = '#fff';
      ctx.textAlign = 'left';
      ctx.fillText(text, w * 0.06 + 6 * unit, y + h * 0.80 + 21 * unit);
    } else if (shot.kind === 'action') {
      // Captions are anchored to the bottom of the frame and grow upward, so a
      // three-line caption can never crawl over the picture or off the plate.
      var lead = 26 * unit;
      ctx.font = 'italic 400 ' + Math.round(19 * unit) + 'px Georgia, "Times New Roman", serif';
      ctx.fillStyle = 'rgba(255,255,255,0.92)';
      var lines = wrapLines(ctx, shot.caption, w * 0.76, 3);
      var bottom = y + h * 0.90;
      lines.forEach(function (line, i) {
        shadowedText(ctx, line, w / 2, bottom - (lines.length - 1 - i) * lead);
      });
    } else if (shot.kind === 'line') {
      var dlead = 30 * unit;
      ctx.font = '600 ' + Math.round(23 * unit) + 'px "Trebuchet MS", system-ui, sans-serif';
      var dl = wrapLines(ctx, shot.caption, w * 0.74, 3);
      var dbottom = y + h * 0.90;
      var dtop = dbottom - (dl.length - 1) * dlead;

      var name = shot.speaker + (shot.parenthetical ? '  ' + shot.parenthetical : '');
      ctx.font = '700 ' + Math.round(17 * unit) + 'px "Courier New", monospace';
      ctx.fillStyle = Art.rgb(pal.key, 0.95);
      shadowedText(ctx, name.toUpperCase().split('').join('\u2009'), w / 2, dtop - 34 * unit);

      ctx.font = '600 ' + Math.round(23 * unit) + 'px "Trebuchet MS", system-ui, sans-serif';
      ctx.fillStyle = '#fff';
      dl.forEach(function (line, i) {
        shadowedText(ctx, line, w / 2, dtop + i * dlead);
      });
    }
    ctx.restore();
  }

  /* ---------------------------------------------------------------- grain */
  var grainCanvas = null;
  function grainTile(doc) {
    if (grainCanvas) return grainCanvas;
    var size = 128;
    var c = doc.createElement('canvas');
    c.width = size;
    c.height = size;
    var g = c.getContext('2d');
    var img = g.createImageData(size, size);
    // Seeded, not Math.random(): the same film is supposed to come out the same
    // every time, and a randomised grain tile quietly broke that between page
    // loads — two recordings of one film differed in every frame's pixels.
    var grainRng = PARSE.makeRng(PARSE.hashText('film-grain'));
    for (var i = 0; i < img.data.length; i += 4) {
      var v = 110 + grainRng() * 90;
      img.data[i] = img.data[i + 1] = img.data[i + 2] = v;
      img.data[i + 3] = 26;
    }
    g.putImageData(img, 0, 0);
    grainCanvas = c;
    return c;
  }

  function drawGrain(ctx, w, h, y, time, pal) {
    var doc = ctx.canvas && ctx.canvas.ownerDocument;
    if (!doc) return;
    var tile = grainTile(doc);
    var pattern = ctx.createPattern(tile, 'repeat');
    if (!pattern) return;
    // The grain steps twelve times a second rather than sixty: it still crawls
    // like film, and it stops every single frame from being different, which is
    // what makes a recorded file enormous.
    var step = Math.floor(time * 12);
    ctx.save();
    ctx.globalAlpha = 0.3;
    ctx.translate(-(step * 53) % 128, -(step * 37) % 128);
    ctx.fillStyle = pattern;
    ctx.fillRect(0, y - 128, w + 256, h + 256);
    ctx.restore();
    // gate flicker, on the same clock
    ctx.fillStyle = 'rgba(0,0,0,' + (0.02 + 0.03 * Math.abs(Math.sin(step * 0.94))) + ')';
    ctx.fillRect(0, y, w, h);
  }

  /* The score is a live player, so the film drives it the way a projectionist
   * drives sound: same clock, same transport. */
  function beatAt(score, seconds) {
    if (!score || !score.player || !score.player.song) return 0;
    return (seconds * score.player.song.bpm) / 60;
  }

  /* --------------------------------------------------------------- player */
  function Player(canvas, reel, hooks) {
    this.canvas = canvas;
    this.ctx = canvas.getContext('2d');
    this.reel = reel;
    this.hooks = hooks || {};
    this.score = hooks && hooks.score ? hooks.score : null;
    this.time = 0;
    this.playing = false;
    this._raf = null;
    this._startedAt = 0;
    this._offset = 0;
    this._shot = null;
  }

  Player.prototype.drawAt = function (time) {
    return drawFrame(this.ctx, this.canvas.width, this.canvas.height, this.reel, time);
  };

  Player.prototype.play = function (from) {
    if (this.playing) return;
    this.playing = true;
    this._offset = typeof from === 'number' ? from : this.time;
    this._startedAt = (root.performance || Date).now();
    var self = this;

    function tick() {
      if (!self.playing) return;
      var now = (root.performance || Date).now();
      self.time = self._offset + (now - self._startedAt) / 1000;
      if (self.time >= self.reel.duration) {
        self.time = self.reel.duration;
        self.drawAt(self.time);
        self.stop(true);
        return;
      }
      var shot = self.drawAt(self.time);
      // The score follows the cut: a new shot retunes the bed and speaks the
      // line; every frame keeps the pulse honest.
      if (self.score) {
        if (shot !== self._shot) {
          self._shot = shot;
          self.score.enterShot(shot, self.time);
          if (self.hooks.onShot) self.hooks.onShot(shot);
        }
        self.score.tick(self.time);
      } else if (shot !== self._shot) {
        self._shot = shot;
        if (self.hooks.onShot) self.hooks.onShot(shot);
      }
      if (self.hooks.onFrame) self.hooks.onFrame(self.time, self.reel.duration);
      self._raf = root.requestAnimationFrame(tick);
    }
    if (this.score) this.score.start();
    if (this.score && this.score.player) {
      this.score.player.play(beatAt(this.score, this._offset));
      this.score.applyDuck(this._offset);
    }
    this._raf = root.requestAnimationFrame(tick);
    if (this.hooks.onPlay) this.hooks.onPlay();
  };

  /* Jump to a moment. The film is redrawn from the reel, so any frame can be
   * shown instantly whether or not it has been played yet. */
  Player.prototype.seek = function (time) {
    var target = Math.max(0, Math.min(this.reel.duration, time));
    var wasPlaying = this.playing;
    if (wasPlaying) this.pause();
    this.time = target;
    if (this.score && this.score.player) {
      // No duck envelope here: `pause()` above has already cleared `playing`,
      // and the `play(target)` below lays a fresh envelope from the new spot.
      this.score.player.seek(beatAt(this.score, target));
    }
    this.drawAt(target);
    if (this.hooks.onFrame) this.hooks.onFrame(target, this.reel.duration);
    if (wasPlaying) this.play(target);
  };

  Player.prototype.pause = function () {
    if (!this.playing) return;
    if (this._raf) root.cancelAnimationFrame(this._raf);
    this._raf = null;
    this.playing = false;
    this._shot = null;
    if (this.score && this.score.player) this.score.player.pause();
    if (this.score) this.score.stop();
    if (this.hooks.onPause) this.hooks.onPause(this.time);
  };

  Player.prototype.stop = function (ended) {
    if (this._raf) root.cancelAnimationFrame(this._raf);
    this._raf = null;
    this.playing = false;
    this._shot = null;
    if (this.score && this.score.player) this.score.player.stop();
    if (this.score) this.score.stop();
    if (!ended) this.time = 0;
    if (this.hooks.onStop) this.hooks.onStop(!!ended);
  };

  /* ------------------------------------------------------------- recorder
   * Records the film to a real video file by capturing the canvas and the
   * score together. It runs in real time, because that is the only way a
   * browser can record: a two-minute film takes two minutes.
   */
  /* Choosing a format is not as simple as asking for MP4.
   *
   * A browser can answer "yes" to the bare type `video/mp4` and then write VP9
   * video into an MP4 wrapper — a file named .mp4 that an iPhone still cannot
   * play, which is worse than an honest .webm because the name promises
   * otherwise. Only an explicit H.264 codec string is a real promise, so those
   * are asked for by name and the bare type is never used.
   *
   * H.264 in MP4 plays everywhere, iPhone and QuickTime included. WebM plays on
   * computers — Chrome, Edge, Firefox, VLC — and on Android, but not on Apple
   * devices. The app says which one you are getting before you record.
   */
  /* Every MP4 candidate names *both* codecs. Asking for `codecs=avc1` alone
   * leaves the audio to the browser, and Chrome will happily put Opus in an
   * MP4 — H.264 that an Apple device plays, carrying a soundtrack it does not.
   * A silent film is not what anyone recorded, so an MP4 we cannot fully name
   * is not worth having: it falls back to WebM, which at least warns. */
  var MP4_CANDIDATES = [
    'video/mp4;codecs=avc1.42E01E,mp4a.40.2',   // H.264 baseline + AAC
    'video/mp4;codecs=avc1.4D401E,mp4a.40.2',   // H.264 main + AAC
    'video/mp4;codecs=avc1.64001E,mp4a.40.2',   // H.264 high + AAC
    'video/mp4;codecs=avc1,mp4a.40.2',
    'video/mp4;codecs=avc1,mp4a',
    'video/mp4;codecs=h264,aac'
  ];

  var WEBM_CANDIDATES = [
    'video/webm;codecs=vp9,opus',
    'video/webm;codecs=vp8,opus',
    'video/webm;codecs=vp9',
    'video/webm'
  ];

  /* Pure, so the choice can be tested against any browser's answers. */
  function pickMimeType(isSupported) {
    var i;
    for (i = 0; i < MP4_CANDIDATES.length; i++) {
      if (isSupported(MP4_CANDIDATES[i])) {
        return { type: MP4_CANDIDATES[i], container: 'mp4', extension: '.mp4', playsOnApple: true };
      }
    }
    for (i = 0; i < WEBM_CANDIDATES.length; i++) {
      if (isSupported(WEBM_CANDIDATES[i])) {
        return { type: WEBM_CANDIDATES[i], container: 'webm', extension: '.webm', playsOnApple: false };
      }
    }
    return null;
  }

  function bestFormat() {
    if (typeof root.MediaRecorder === 'undefined') return null;
    return pickMimeType(function (type) {
      return root.MediaRecorder.isTypeSupported(type);
    });
  }

  function bestMimeType() {
    var format = bestFormat();
    return format ? format.type : null;
  }

  function canRecord(canvas) {
    return !!(bestMimeType() && canvas && canvas.captureStream);
  }

  /* Returns a promise for the finished file. Cancelling still resolves with
   * whatever was shot — a stopped take is a short film, not a lost one. */
  function record(player, opts) {
    opts = opts || {};
    var canvas = player.canvas;
    var format = bestFormat();
    if (!format || !canvas.captureStream) {
      return Promise.reject(new Error('This browser cannot record video from a canvas.'));
    }
    var mime = format.type;

    var fps = opts.fps || 30;
    var stream = canvas.captureStream(fps);
    if (player.score && player.score.streamDestination) {
      player.score.streamDestination.stream.getAudioTracks().forEach(function (track) {
        stream.addTrack(track);
      });
    }

    var recorder = new root.MediaRecorder(stream, {
      mimeType: mime,
      videoBitsPerSecond: opts.videoBitrate || 5000000,
      audioBitsPerSecond: 128000
    });
    var chunks = [];
    recorder.ondataavailable = function (e) {
      if (e.data && e.data.size) chunks.push(e.data);
    };

    return new Promise(function (resolve, reject) {
      recorder.onerror = function (e) { reject(e.error || new Error('Recording failed.')); };
      recorder.onstop = function () {
        stream.getTracks().forEach(function (t) { t.stop(); });
        resolve({ blob: new Blob(chunks, { type: mime }), mime: mime, format: format });
      };

      var previousStop = player.hooks.onStop;
      player.hooks.onStop = function (ended) {
        player.hooks.onStop = previousStop;
        if (previousStop) previousStop(ended);
        // Let the last frames reach the encoder before closing the file.
        setTimeout(function () {
          if (recorder.state !== 'inactive') recorder.stop();
        }, 220);
      };

      recorder.start(1000);
      player.play(0);
    });
  }

  var API = {
    drawFrame: drawFrame,
    Player: Player,
    record: record,
    canRecord: canRecord,
    bestMimeType: bestMimeType,
    bestFormat: bestFormat,
    pickMimeType: pickMimeType,
    MP4_CANDIDATES: MP4_CANDIDATES,
    WEBM_CANDIDATES: WEBM_CANDIDATES,
    framingFor: framingFor,
    figureLayout: figureLayout,
    fadeAmount: fadeAmount,
    foreCoversFraming: foreCoversFraming,
    foreVisibleWorldRect: foreVisibleWorldRect,
    wrapLines: wrapLines,
    WORLD_W: WORLD_W,
    WORLD_H: WORLD_H,
    ASPECT: ASPECT
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmPlayer = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
