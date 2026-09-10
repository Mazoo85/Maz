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
  var Reel = root.FilmReel || (typeof require !== 'undefined' ? require('./film-reel.js') : {});

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
   * How close the camera is, and where it is pointed, for each shot type. */
  function framingFor(shot, progress) {
    var p = easeInOut(clamp01(progress));
    var zoom = 1;
    var panX = 0;
    var panY = 0;

    if (shot.framing === 'mid') zoom = 1.35;
    else if (shot.framing === 'two') zoom = 1.5;
    else if (shot.framing === 'close') zoom = 1.95;
    else if (shot.framing === 'insert') zoom = 1.15;

    // A close-up looks at whoever is speaking, so the frame sits on their side.
    if (shot.framing === 'close' && shot.speaker) panX = (shot.side || 0) * 0.16;
    if (shot.framing === 'close') panY = -0.05;

    switch (shot.camera) {
      case 'push': zoom *= 1 + 0.13 * p; break;
      case 'push-slow': zoom *= 1 + 0.06 * p; break;
      case 'pull': zoom *= 1.16 - 0.16 * p; break;
      case 'pan-l': panX += 0.09 - 0.18 * p; break;
      case 'pan-r': panX += -0.09 + 0.18 * p; break;
      default: zoom *= 1 + 0.018 * p; break; // never perfectly still
    }
    return { zoom: zoom, panX: panX, panY: panY };
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
    if (count === 1) {
      return [{ name: shot.characters[0], x: 560, ground: 356, height: shot.framing === 'mid' ? 210 : 170 }];
    }
    return [
      { name: shot.characters[0], x: 360, ground: 356, height: shot.framing === 'mid' ? 205 : 168 },
      { name: shot.characters[1], x: 660, ground: 350, height: shot.framing === 'mid' ? 198 : 162 }
    ];
  }

  /* --------------------------------------------------------------- a frame */
  function drawFrame(ctx, width, height, reel, time) {
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

    var cam = framingFor(shot, progress);
    var scale = (frameW / WORLD_W) * cam.zoom;

    ctx.save();
    ctx.translate(frameW / 2 + cam.panX * frameW, frameY + frameH / 2 + cam.panY * frameH);
    ctx.scale(scale, scale);
    ctx.translate(-WORLD_W / 2, -WORLD_H / 2);

    // the set
    var draw = Art.SETS[shot.set] || Art.SETS.room;
    draw(ctx, pal, Art.noise('set-' + shot.set + '-' + reel.seed, 80));

    // the people in it
    if (shot.framing !== 'insert') {
      figureLayout(shot).forEach(function (spot) {
        var voice = reel.voices[spot.name] || { hue: 200 };
        var speaking = shot.kind === 'line' && spot.name === shot.speaker;
        // A speaking figure breathes a little faster than a listening one.
        var wobble = Math.sin(time * (speaking ? 5.2 : 1.7) + spot.x) * (speaking ? 1.5 : 0.7);
        Art.drawFigure(ctx, pal, spot.x, spot.ground, spot.height, voice.hue, speaking, wobble);
      });
    } else {
      // insert: the object, lit, on a dark surface
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
      Art.glyphFor(reel.object)(ctx, pal);
      ctx.restore();
    }
    ctx.restore();

    // light leak from the key, and a vignette to hold the eye in the middle
    var leak = ctx.createLinearGradient(0, frameY, frameW * 0.7, frameY + frameH);
    leak.addColorStop(0, Art.rgb(pal.key, 0.10 + pal.tension * 0.05));
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
    for (var i = 0; i < img.data.length; i += 4) {
      var v = 110 + Math.random() * 90;
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
    if (this.score) this.score.stop();
    if (this.hooks.onPause) this.hooks.onPause(this.time);
  };

  Player.prototype.stop = function (ended) {
    if (this._raf) root.cancelAnimationFrame(this._raf);
    this._raf = null;
    this.playing = false;
    this._shot = null;
    if (this.score) this.score.stop();
    if (!ended) this.time = 0;
    if (this.hooks.onStop) this.hooks.onStop(!!ended);
  };

  /* ------------------------------------------------------------- recorder
   * Records the film to a real video file by capturing the canvas and the
   * score together. It runs in real time, because that is the only way a
   * browser can record: a two-minute film takes two minutes.
   */
  var MIME_CANDIDATES = [
    'video/webm;codecs=vp9,opus',
    'video/webm;codecs=vp8,opus',
    'video/webm;codecs=vp9',
    'video/webm',
    'video/mp4'
  ];

  function bestMimeType() {
    if (typeof root.MediaRecorder === 'undefined') return null;
    for (var i = 0; i < MIME_CANDIDATES.length; i++) {
      if (root.MediaRecorder.isTypeSupported(MIME_CANDIDATES[i])) return MIME_CANDIDATES[i];
    }
    return null;
  }

  function canRecord(canvas) {
    return !!(bestMimeType() && canvas && canvas.captureStream);
  }

  /* Returns a promise for the finished file. Cancelling still resolves with
   * whatever was shot — a stopped take is a short film, not a lost one. */
  function record(player, opts) {
    opts = opts || {};
    var canvas = player.canvas;
    var mime = bestMimeType();
    if (!mime || !canvas.captureStream) {
      return Promise.reject(new Error('This browser cannot record video from a canvas.'));
    }

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
        resolve({ blob: new Blob(chunks, { type: mime }), mime: mime });
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
    framingFor: framingFor,
    figureLayout: figureLayout,
    fadeAmount: fadeAmount,
    wrapLines: wrapLines,
    WORLD_W: WORLD_W,
    WORLD_H: WORLD_H,
    ASPECT: ASPECT
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmPlayer = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
