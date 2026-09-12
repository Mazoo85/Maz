/*
 * SCRIPT FORGE — the poster.
 * --------------------------
 * A film you cannot show anybody is a film nobody sees. The video is three
 * minutes long and 20 MB; a poster is one image, it loads instantly, and it is
 * the thing a person actually posts when they want to say "look what I made".
 *
 * It is drawn with the film's own renderer rather than with a second set of
 * drawing code — the picture on a poster IS a frame of the film, chosen rather
 * than composed, which is how a real one works and which means a poster can
 * never show something the film does not contain.
 *
 * Two decisions worth knowing about.
 *
 * WHICH FRAME. Not the first, which is a title card over an empty set, and not
 * a random one, which lands on somebody mid-blink in a corridor. The frame is
 * the middle of the tensest shot that has a person in it, because the tensest
 * moment is what a poster is for, and a poster with nobody in it is a
 * landscape.
 *
 * THE TYPE IS THE DESIGN. There is no artwork here beyond the frame, so the
 * whole thing rests on the title block: a wide-tracked genre line, the title
 * at whatever size makes it fill the width, and the credits small enough to
 * read as credits. Everything is measured from the poster's own width, so it
 * looks the same at 800px and at 2000.
 *
 * Exposed as window.FilmPoster (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  var ASPECT = 2 / 3;           // a poster is 2:3, which every print size is

  /* The frame a poster should be made from.
   *
   * Returns a time in seconds. Deterministic: the same film always posters the
   * same moment, so re-exporting does not quietly give you a different picture.
   */
  function bestMoment(reel) {
    if (!reel || !reel.shots || !reel.shots.length) return 0;
    var best = null;
    reel.shots.forEach(function (shot) {
      // Somebody has to be in it. A poster of an empty room is a landscape.
      if (!shot.characters || !shot.characters.length) return;
      // Not the title card, not the end card: both are text over a wide shot,
      // and the poster is about to put its own text over a wide shot.
      if (shot.kind === 'title' || shot.kind === 'end') return;
      if (shot.framing === 'insert') return;
      var score = (shot.mood == null ? 0.4 : shot.mood)
        // A wider shot shows the set as well as the person, which is what a
        // poster wants; a close-up of a silhouette is a shape.
        + (shot.framing === 'wide' ? 0.12 : 0)
        + (shot.framing === 'two' ? 0.06 : 0);
      if (!best || score > best.score) best = { shot: shot, score: score };
    });
    if (!best) {
      // A film with nobody in any shot: take the middle of it rather than
      // returning nothing, because a poster is still better than no poster.
      return reel.duration * 0.5;
    }
    return best.shot.start + best.shot.duration * 0.5;
  }

  /* The block of type under the picture, as measured lines. Pure: no canvas, so
   * the layout can be checked without drawing it. */
  function layout(reel, width) {
    var height = Math.round(width / ASPECT);
    // The picture takes the top two thirds and is cropped to it, rather than
    // letterboxed inside it: black bars on a poster read as a mistake.
    var pictureH = Math.round(height * 0.66);
    var pad = Math.round(width * 0.075);
    return {
      width: width,
      height: height,
      pictureH: pictureH,
      pad: pad,
      genreY: pictureH + Math.round(width * 0.11),
      titleY: pictureH + Math.round(width * 0.215),
      loglineY: pictureH + Math.round(width * 0.30),
      creditY: height - Math.round(width * 0.075),
      genreSize: Math.round(width * 0.028),
      titleSize: Math.round(width * 0.105),
      loglineSize: Math.round(width * 0.032),
      creditSize: Math.round(width * 0.024)
    };
  }

  /* Break a title across lines so it fills the poster's width rather than
   * shrinking to fit on one. A long title set small looks like a mistake; the
   * same title on two lines looks like a design. */
  function titleLines(title, maxChars) {
    var words = String(title).trim().split(/\s+/).filter(Boolean);
    if (!words.length) return [''];
    var lines = [];
    var line = '';
    words.forEach(function (word) {
      var candidate = line ? line + ' ' + word : word;
      if (candidate.length > maxChars && line) {
        lines.push(line);
        line = word;
      } else {
        line = candidate;
      }
    });
    if (line) lines.push(line);
    return lines.slice(0, 3);
  }

  /* Draw the poster. `drawFrame` is the film's own renderer, handed in rather
   * than imported, so this file has no opinion about how a film looks. */
  function draw(ctx, reel, drawFrame, opts) {
    opts = opts || {};
    var width = opts.width || 1000;
    var L = layout(reel, width);
    var moment = opts.time == null ? bestMoment(reel) : opts.time;

    ctx.fillStyle = '#07060c';
    ctx.fillRect(0, 0, L.width, L.height);

    /* The picture. The film is 2.35:1 and this slot is about 1.5:1, so the
     * frame is drawn oversize and cropped rather than letterboxed -- bars on a
     * poster read as a mistake, and cropping a wide frame to a narrower one is
     * what a poster has always done. */
    var frameW = L.width;
    var frameH = Math.round(frameW / 2.35);
    if (frameH < L.pictureH) {
      frameW = Math.round(L.pictureH * 2.35);
      frameH = L.pictureH;
    }
    ctx.save();
    ctx.beginPath();
    ctx.rect(0, 0, L.width, L.pictureH);
    ctx.clip();
    ctx.translate((L.width - frameW) / 2, (L.pictureH - frameH) / 2);
    // Without the film's own captions: the poster is about to write its own
    // type over this, and two sets of type on one frame is a mistake.
    drawFrame(ctx, frameW, frameH, reel, moment, { captions: false });
    ctx.restore();

    // A fade from the picture into the type, so the two are one object.
    var fade = ctx.createLinearGradient(0, L.pictureH - Math.round(width * 0.16), 0, L.pictureH);
    fade.addColorStop(0, 'rgba(7,6,12,0)');
    fade.addColorStop(1, 'rgba(7,6,12,1)');
    ctx.fillStyle = fade;
    ctx.fillRect(0, L.pictureH - Math.round(width * 0.16), L.width, Math.round(width * 0.16));

    ctx.textAlign = 'center';

    // Genre and runtime, wide-tracked, the way a one-sheet does it.
    ctx.fillStyle = 'rgba(255,255,255,0.62)';
    ctx.font = '600 ' + L.genreSize + 'px system-ui, sans-serif';
    var above = (reel.genreLabel || '').toUpperCase().split('').join(' ');
    ctx.fillText(above, L.width / 2, L.genreY);

    /* The title, MEASURED rather than assumed.
     *
     * Picking a size from the poster's width alone and hoping the title fits is
     * how "AFTER THE KEY" ran off both edges of its own poster. So: break the
     * title into lines, then shrink the size until the widest line fits inside
     * the margins. The first draft is almost always the right size and the loop
     * does nothing; it is there for the titles that are not. */
    var lines = titleLines(reel.title || 'UNTITLED', 16);
    var maxTitleWidth = L.width - L.pad * 2;
    var size = L.titleSize;
    for (var tries = 0; tries < 24; tries++) {
      ctx.font = '800 ' + size + 'px system-ui, sans-serif';
      var widest = 0;
      lines.forEach(function (line) {
        widest = Math.max(widest, ctx.measureText(line.toUpperCase()).width);
      });
      if (widest <= maxTitleWidth || size <= L.genreSize * 1.4) break;
      size = Math.floor(size * 0.92);
    }
    ctx.fillStyle = '#ffffff';
    ctx.font = '800 ' + size + 'px system-ui, sans-serif';
    var lineStep = Math.round(size * 1.06);
    lines.forEach(function (line, i) {
      ctx.fillText(line.toUpperCase(), L.width / 2, L.titleY + i * lineStep);
    });
    L.fittedTitleSize = size;

    // The logline, wrapped by measurement rather than by guess.
    if (opts.logline) {
      ctx.fillStyle = 'rgba(255,255,255,0.72)';
      ctx.font = '400 ' + L.loglineSize + 'px system-ui, sans-serif';
      var y = L.loglineY + (lines.length - 1) * lineStep;
      var words = String(opts.logline).split(/\s+/);
      var line = '';
      var maxWidth = L.width - L.pad * 2;
      words.forEach(function (word) {
        var candidate = line ? line + ' ' + word : word;
        if (ctx.measureText(candidate).width > maxWidth && line) {
          ctx.fillText(line, L.width / 2, y);
          y += Math.round(L.loglineSize * 1.4);
          line = word;
        } else {
          line = candidate;
        }
      });
      if (line) ctx.fillText(line, L.width / 2, y);
    }

    // Credits.
    ctx.fillStyle = 'rgba(255,255,255,0.4)';
    ctx.font = '500 ' + L.creditSize + 'px system-ui, sans-serif';
    ctx.fillText('WRITTEN AND SHOT BY SCRIPT FORGE'.split('').join(' ').replace(/\s\s/g, ' '),
      L.width / 2, L.creditY - Math.round(L.creditSize * 1.6));
    // The runtime lives on the SCRIPT, not the reel, so it has to be handed in
    // -- the first version read reel.runtime, got undefined, and printed a
    // credit line that began with a stray separator.
    var runtime = opts.runtime || (reel.duration ? Math.round(reel.duration / 60) + ' MIN' : '');
    ctx.fillText((runtime ? runtime + '  ·  ' : '') + 'SEED ' + reel.seed,
      L.width / 2, L.creditY);

    return { width: L.width, height: L.height, moment: moment, titleSize: size, titleLines: lines };
  }

  var API = {
    ASPECT: ASPECT,
    bestMoment: bestMoment,
    layout: layout,
    titleLines: titleLines,
    draw: draw
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmPoster = API;
})(typeof window !== 'undefined' ? window : globalThis);
