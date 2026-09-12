/*
 * CODA PICS — the finish.
 * -----------------------
 * The painted scene goes in; a picture in one of fourteen styles comes out.
 * This is the pass that decides whether you are looking at a watercolour, a
 * pixel-art sprite, a blueprint or a stained-glass window — the shapes
 * underneath are the same either way.
 *
 *   CodaFinish.apply(ctx, width, height, spec, palette)
 *
 * Everything here is done on the raw pixels rather than with canvas filters or
 * a second canvas, for one reason: it then behaves identically in every
 * browser, and the tests can run the whole pipeline in Node. The cost is that
 * blur, bloom and edge-detection are written out by hand below.
 *
 * Exposed as window.CodaFinish (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  var PROMPT = root.CodaPrompt ||
    (typeof require !== 'undefined' ? require('./prompt.js') : null);

  var UTIL = root.MazUtil ||
    (typeof require !== 'undefined' ? require('../../shared/maz-util.js') : {});
  var clamp = UTIL.clamp;

  /* Pixels in, pixels out — both tolerate a canvas that will not give them up
   * (a tainted or zero-sized one), in which case the style falls back to
   * whatever it can do by drawing. */
  function read(ctx, w, h) {
    try {
      var img = ctx.getImageData(0, 0, w, h);
      return img && img.data && img.data.length >= w * h * 4 ? img : null;
    } catch (e) { return null; }
  }

  function write(ctx, img) {
    if (!img) return;
    try { ctx.putImageData(img, 0, 0); } catch (e) { /* nothing to be done */ }
  }

  /* ------------------------------------------------------------- pixel maths */

  function eachPixel(img, fn) {
    var d = img.data;
    for (var i = 0; i < d.length; i += 4) {
      var o = fn(d[i], d[i + 1], d[i + 2], i);
      if (o) { d[i] = o[0]; d[i + 1] = o[1]; d[i + 2] = o[2]; }
    }
  }

  function luma(r, g, b) { return 0.2126 * r + 0.7152 * g + 0.0722 * b; }

  /* Contrast around mid-grey, as an S-curve rather than a hard clip. */
  function contrast(img, amount) {
    eachPixel(img, function (r, g, b) {
      function f(v) {
        var t = v / 255 - 0.5;
        return clamp((t * (1 + amount) + 0.5) * 255, 0, 255);
      }
      return [f(r), f(g), f(b)];
    });
  }

  function saturate(img, k) {
    eachPixel(img, function (r, g, b) {
      var l = luma(r, g, b);
      return [clamp(l + (r - l) * k, 0, 255), clamp(l + (g - l) * k, 0, 255), clamp(l + (b - l) * k, 0, 255)];
    });
  }

  function posterize(img, levels) {
    var step = 255 / (levels - 1);
    eachPixel(img, function (r, g, b) {
      return [Math.round(r / step) * step, Math.round(g / step) * step, Math.round(b / step) * step];
    });
  }

  /* Tint the whole image towards one colour, more in the shadows than the
   * highlights — the trick that makes a grade feel like film rather than a
   * sheet of coloured glass laid on top. */
  function grade(img, shadow, highlight, strength) {
    eachPixel(img, function (r, g, b) {
      var t = luma(r, g, b) / 255;
      var tr = shadow[0] + (highlight[0] - shadow[0]) * t;
      var tg = shadow[1] + (highlight[1] - shadow[1]) * t;
      var tb = shadow[2] + (highlight[2] - shadow[2]) * t;
      return [
        r + (tr - r) * strength,
        g + (tg - g) * strength,
        b + (tb - b) * strength
      ];
    });
  }

  /* A separable box blur, run twice — close enough to a gaussian by eye, and
   * fast enough for a 1600px canvas. */
  /* Every pass here is linear in pixels, so a 2560 x 1440 wallpaper is four
   * times the work of a 1280 x 720 one. Past a few megapixels the second
   * smoothing pass costs more than it shows, so it is dropped rather than
   * letting a big export take seconds. */
  function heavy(w, h) { return w * h > 2600000; }

  function blur(img, w, h, radius, passes) {
    if (radius < 1) return;
    var d = img.data;
    var tmp = new Uint8ClampedArray(d.length);
    var runs = passes || 2;
    if (heavy(w, h) && runs > 1) runs = 1;
    for (var p = 0; p < runs; p++) {
      boxPass(d, tmp, w, h, radius, true);
      boxPass(tmp, d, w, h, radius, false);
    }
  }

  function boxPass(src, dst, w, h, radius, horizontal) {
    var lenOuter = horizontal ? h : w;
    var lenInner = horizontal ? w : h;
    var span = radius * 2 + 1;
    for (var o = 0; o < lenOuter; o++) {
      var sum = [0, 0, 0, 0];
      for (var k = -radius; k <= radius; k++) {
        var idx = index(clamp(k, 0, lenInner - 1), o, w, horizontal);
        sum[0] += src[idx]; sum[1] += src[idx + 1]; sum[2] += src[idx + 2]; sum[3] += src[idx + 3];
      }
      for (var i = 0; i < lenInner; i++) {
        var out = index(i, o, w, horizontal);
        dst[out] = sum[0] / span;
        dst[out + 1] = sum[1] / span;
        dst[out + 2] = sum[2] / span;
        dst[out + 3] = sum[3] / span;
        var add = index(clamp(i + radius + 1, 0, lenInner - 1), o, w, horizontal);
        var sub = index(clamp(i - radius, 0, lenInner - 1), o, w, horizontal);
        sum[0] += src[add] - src[sub];
        sum[1] += src[add + 1] - src[sub + 1];
        sum[2] += src[add + 2] - src[sub + 2];
        sum[3] += src[add + 3] - src[sub + 3];
      }
    }
  }

  function index(i, o, w, horizontal) {
    return horizontal ? (o * w + i) * 4 : (i * w + o) * 4;
  }

  /* Bright parts, blurred and added back — what makes neon glow. */
  function bloom(img, w, h, threshold, radius, strength) {
    var d = img.data;
    var bright = { data: new Uint8ClampedArray(d.length), width: w, height: h };
    for (var i = 0; i < d.length; i += 4) {
      var l = luma(d[i], d[i + 1], d[i + 2]);
      var k = l > threshold ? (l - threshold) / (255 - threshold) : 0;
      bright.data[i] = d[i] * k;
      bright.data[i + 1] = d[i + 1] * k;
      bright.data[i + 2] = d[i + 2] * k;
      bright.data[i + 3] = 255;
    }
    blur(bright, w, h, radius, 2);
    for (var j = 0; j < d.length; j += 4) {
      d[j] = clamp(d[j] + bright.data[j] * strength, 0, 255);
      d[j + 1] = clamp(d[j + 1] + bright.data[j + 1] * strength, 0, 255);
      d[j + 2] = clamp(d[j + 2] + bright.data[j + 2] * strength, 0, 255);
    }
  }

  /* Average each block down to one colour — a real pixel-art downsample, not a
   * blurred scale. */
  function pixelate(img, w, h, block) {
    var d = img.data;
    for (var by = 0; by < h; by += block) {
      for (var bx = 0; bx < w; bx += block) {
        var r = 0, g = 0, b = 0, n = 0;
        for (var y = by; y < Math.min(by + block, h); y++) {
          for (var x = bx; x < Math.min(bx + block, w); x++) {
            var i = (y * w + x) * 4;
            r += d[i]; g += d[i + 1]; b += d[i + 2]; n++;
          }
        }
        r /= n; g /= n; b /= n;
        for (var y2 = by; y2 < Math.min(by + block, h); y2++) {
          for (var x2 = bx; x2 < Math.min(bx + block, w); x2++) {
            var j = (y2 * w + x2) * 4;
            d[j] = r; d[j + 1] = g; d[j + 2] = b;
          }
        }
      }
    }
  }

  /* Ordered dithering, the way a 16-bit console faked colours it didn't have. */
  var BAYER = [
    [0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]
  ];

  function dither(img, w, h, levels, spread) {
    var d = img.data, step = 255 / (levels - 1);
    for (var y = 0; y < h; y++) {
      for (var x = 0; x < w; x++) {
        var i = (y * w + x) * 4;
        var t = (BAYER[y & 3][x & 3] / 16 - 0.5) * spread;
        d[i] = clamp(Math.round((d[i] + t) / step) * step, 0, 255);
        d[i + 1] = clamp(Math.round((d[i + 1] + t) / step) * step, 0, 255);
        d[i + 2] = clamp(Math.round((d[i + 2] + t) / step) * step, 0, 255);
      }
    }
  }

  /* Where the picture changes fastest — the lines an inker would draw. */
  function edges(img, w, h) {
    var d = img.data;
    var out = new Float32Array(w * h);
    for (var y = 1; y < h - 1; y++) {
      for (var x = 1; x < w - 1; x++) {
        var i = (y * w + x) * 4;
        var l = luma(d[i], d[i + 1], d[i + 2]);
        var rx = (y * w + x + 1) * 4, dy = ((y + 1) * w + x) * 4;
        out[y * w + x] = Math.abs(l - luma(d[rx], d[rx + 1], d[rx + 2])) +
          Math.abs(l - luma(d[dy], d[dy + 1], d[dy + 2]));
      }
    }
    return out;
  }

  function inkEdges(img, w, h, threshold, darkness) {
    var e = edges(img, w, h), d = img.data;
    for (var p = 0; p < e.length; p++) {
      if (e[p] < threshold) continue;
      var k = clamp((e[p] - threshold) / 60, 0, 1) * darkness;
      var i = p * 4;
      d[i] *= 1 - k; d[i + 1] *= 1 - k; d[i + 2] *= 1 - k;
    }
  }

  function grain(img, amount, r) {
    eachPixel(img, function (rr, g, b) {
      var n = (r() - 0.5) * amount;
      return [rr + n, g + n, b + n];
    });
  }

  function vignette(img, w, h, strength) {
    var d = img.data, cx = w / 2, cy = h / 2, max = Math.sqrt(cx * cx + cy * cy);
    for (var y = 0; y < h; y++) {
      for (var x = 0; x < w; x++) {
        var dx = x - cx, dy = y - cy;
        var t = Math.sqrt(dx * dx + dy * dy) / max;
        var k = 1 - Math.pow(t, 2.2) * strength;
        var i = (y * w + x) * 4;
        d[i] *= k; d[i + 1] *= k; d[i + 2] *= k;
      }
    }
  }

  /* Paper: a wash of tone plus fibres, for the styles that live on a page. */
  function paper(ctx, w, h, r, tone, strength) {
    ctx.save();
    ctx.globalAlpha = strength;
    ctx.fillStyle = tone;
    ctx.fillRect(0, 0, w, h);
    ctx.globalAlpha = strength * 0.5;
    for (var i = 0; i < 160; i++) {
      ctx.fillStyle = r() > 0.5 ? 'rgba(255,255,255,0.5)' : 'rgba(0,0,0,0.35)';
      ctx.fillRect(r() * w, r() * h, w * (0.01 + r() * 0.06), 1);
    }
    ctx.restore();
  }

  function sample(img, w, h, x, y) {
    var i = (clamp(Math.round(y), 0, h - 1) * w + clamp(Math.round(x), 0, w - 1)) * 4;
    return 'rgb(' + Math.round(img.data[i]) + ',' + Math.round(img.data[i + 1]) + ',' +
      Math.round(img.data[i + 2]) + ')';
  }

  /* ------------------------------------------------------------------ styles
   * Each one is handed the context, the size, the scene it came from and its
   * own generator. They may work on pixels, draw on top, or both.
   */
  var STYLE = {};

  STYLE.neon = function (ctx, w, h, spec, P, r) {
    var img = read(ctx, w, h);
    if (img) {
      saturate(img, 1.55);
      grade(img, [24, 6, 48], [255, 210, 255], 0.24);
      contrast(img, 0.25);
      bloom(img, w, h, 140, Math.max(2, Math.round(Math.min(w, h) * 0.012)), 0.85);
      vignette(img, w, h, 0.45);
      write(ctx, img);
    }
    ctx.save();                                        // the horizon grid
    ctx.globalAlpha = 0.35;
    ctx.strokeStyle = 'rgba(255,45,149,0.9)';
    ctx.lineWidth = Math.max(1, w * 0.0015);
    var hz = h * 0.72;
    for (var i = 0; i <= 22; i++) {
      var t = i / 22;
      ctx.beginPath();
      ctx.moveTo(w * 0.5, hz);
      ctx.lineTo(w * (t * 3 - 1), h);
      ctx.stroke();
    }
    for (var j = 1; j <= 9; j++) {
      var y = hz + (h - hz) * Math.pow(j / 9, 2.1);
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(w, y);
      ctx.stroke();
    }
    ctx.restore();
  };

  STYLE.pixel = function (ctx, w, h, spec, P, r) {
    var img = read(ctx, w, h);
    if (!img) return;
    saturate(img, 1.2);
    pixelate(img, w, h, Math.max(3, Math.round(Math.min(w, h) / 108)));
    posterize(img, 6);
    contrast(img, 0.16);
    write(ctx, img);
  };

  STYLE.retro16 = function (ctx, w, h, spec, P, r) {
    var img = read(ctx, w, h);
    if (img) {
      saturate(img, 1.3);
      pixelate(img, w, h, Math.max(2, Math.round(Math.min(w, h) / 190)));
      dither(img, w, h, 7, 46);
      contrast(img, 0.2);
      write(ctx, img);
    }
    ctx.save();                                        // CRT scanlines
    ctx.globalAlpha = 0.17;
    ctx.fillStyle = '#000';
    for (var y = 0; y < h; y += Math.max(2, Math.round(h / 320))) {
      ctx.fillRect(0, y, w, Math.max(1, Math.round(h / 900)));
    }
    ctx.restore();
  };

  STYLE.watercolour = function (ctx, w, h, spec, P, r) {
    var img = read(ctx, w, h);
    if (img) {
      blur(img, w, h, Math.max(1, Math.round(Math.min(w, h) * 0.003)), 1);
      posterize(img, 10);
      saturate(img, 1.04);
      grade(img, [44, 38, 30], [252, 248, 238], 0.12);
      contrast(img, 0.10);
      inkEdges(img, w, h, 30, 0.45);                   // the drawing under the wash
      write(ctx, img);
    }
    ctx.save();                                        // pigment pooling
    ctx.globalCompositeOperation = 'multiply';
    for (var i = 0; i < 22; i++) {
      ctx.globalAlpha = 0.04 + r() * 0.05;
      ctx.fillStyle = 'rgb(' + Math.round(120 + r() * 60) + ',' +
        Math.round(120 + r() * 60) + ',' + Math.round(140 + r() * 60) + ')';
      ctx.beginPath();
      var cx = r() * w, cy = r() * h, rad = Math.min(w, h) * (0.04 + r() * 0.14);
      for (var a = 0; a <= Math.PI * 2 + 0.01; a += Math.PI / 10) {
        var k = 0.7 + r() * 0.6;
        ctx.lineTo(cx + Math.cos(a) * rad * k, cy + Math.sin(a) * rad * k);
      }
      ctx.closePath();
      ctx.fill();
    }
    ctx.restore();
    paper(ctx, w, h, r, 'rgb(250,246,232)', 0.10);
  };

  STYLE.noir = function (ctx, w, h, spec, P, r) {
    var img = read(ctx, w, h);
    if (!img) return;
    eachPixel(img, function (rr, g, b) {
      var l = luma(rr, g, b);
      return [l, l, l];
    });
    contrast(img, 0.62);
    grade(img, [10, 12, 22], [250, 248, 240], 0.18);
    grain(img, 30, r);
    vignette(img, w, h, 0.62);
    write(ctx, img);
  };

  STYLE.comic = function (ctx, w, h, spec, P, r) {
    var img = read(ctx, w, h);
    if (img) {
      saturate(img, 1.4);
      posterize(img, 5);
      inkEdges(img, w, h, 26, 0.9);
      contrast(img, 0.2);
      write(ctx, img);
    }
    var src = read(ctx, w, h);                         // halftone in the darks
    if (!src) return;
    var cell = Math.max(4, Math.round(Math.min(w, h) / 130));
    ctx.save();
    ctx.globalAlpha = 0.3;
    ctx.fillStyle = '#0b0714';
    for (var y = 0; y < h; y += cell) {
      for (var x = 0; x < w; x += cell) {
        var i = (Math.min(y, h - 1) * w + Math.min(x, w - 1)) * 4;
        var l = luma(src.data[i], src.data[i + 1], src.data[i + 2]) / 255;
        if (l > 0.62) continue;
        var rad = (1 - l) * cell * 0.48;
        ctx.beginPath();
        ctx.arc(x + cell / 2, y + cell / 2, rad, 0, Math.PI * 2);
        ctx.fill();
      }
    }
    ctx.restore();
  };

  STYLE.blueprint = function (ctx, w, h, spec, P, r) {
    var img = read(ctx, w, h);
    if (!img) return;
    var e = edges(img, w, h), d = img.data;
    for (var p = 0; p < e.length; p++) {
      var line = clamp(e[p] / 34, 0, 1);
      var i = p * 4;
      d[i] = 18 + line * 220;
      d[i + 1] = 42 + line * 200;
      d[i + 2] = 96 + line * 150;
    }
    write(ctx, img);
    ctx.save();                                        // drafting grid
    ctx.strokeStyle = 'rgba(200,230,255,0.16)';
    ctx.lineWidth = 1;
    var step = Math.max(16, Math.round(Math.min(w, h) / 26));
    for (var x = 0; x < w; x += step) {
      ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke();
    }
    for (var y = 0; y < h; y += step) {
      ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke();
    }
    ctx.strokeStyle = 'rgba(200,230,255,0.5)';
    ctx.lineWidth = Math.max(1, w * 0.002);
    ctx.strokeRect(step, step, w - step * 2, h - step * 2);
    ctx.restore();
  };

  STYLE.storybook = function (ctx, w, h, spec, P, r) {
    var img = read(ctx, w, h);
    if (img) {
      blur(img, w, h, Math.max(1, Math.round(Math.min(w, h) * 0.002)), 1);
      posterize(img, 10);
      saturate(img, 1.16);
      grade(img, [70, 56, 48], [252, 244, 224], 0.18);
      contrast(img, 0.04);
      inkEdges(img, w, h, 34, 0.45);
      write(ctx, img);
    }
    paper(ctx, w, h, r, 'rgb(255,248,230)', 0.16);
  };

  STYLE.ukiyo = function (ctx, w, h, spec, P, r) {
    var img = read(ctx, w, h);
    if (img) {
      posterize(img, 5);
      saturate(img, 0.85);
      grade(img, [46, 40, 32], [246, 238, 214], 0.26);
      inkEdges(img, w, h, 24, 0.8);
      write(ctx, img);
    }
    ctx.save();                                        // carved line texture
    ctx.globalAlpha = 0.10;
    ctx.strokeStyle = '#2b2418';
    ctx.lineWidth = Math.max(1, w * 0.0012);
    for (var i = 0; i < 90; i++) {
      var y = r() * h;
      ctx.beginPath();
      ctx.moveTo(0, y);
      for (var x = 0; x <= w; x += w / 12) ctx.lineTo(x, y + Math.sin(x / w * 8 + i) * h * 0.004);
      ctx.stroke();
    }
    ctx.restore();
    paper(ctx, w, h, r, 'rgb(244,236,214)', 0.18);
  };

  STYLE.poster = function (ctx, w, h, spec, P, r) {
    var img = read(ctx, w, h);
    if (!img) return;
    saturate(img, 1.18);
    dither(img, w, h, 6, 26);
    contrast(img, 0.20);
    inkEdges(img, w, h, 34, 0.30);
    grade(img, [26, 24, 40], [255, 246, 232], 0.12);
    write(ctx, img);
  };

  /* Low poly and stained glass are one idea with two settings: cut the frame
   * into cells, give each the colour at its middle, and either leave the seams
   * open or lead them in black. */
  function mosaic(ctx, w, h, r, opts) {
    var img = read(ctx, w, h);
    if (!img) return;
    var cols = Math.min(opts.cols, Math.max(12, Math.round(w / 26)));
    var rows = Math.max(3, Math.round(cols * h / w));
    var cw = w / cols, ch = h / rows;
    var grid = [];
    for (var y = 0; y <= rows; y++) {
      grid[y] = [];
      for (var x = 0; x <= cols; x++) {
        var jx = (x === 0 || x === cols) ? 0 : (r() - 0.5) * cw * opts.jitter;
        var jy = (y === 0 || y === rows) ? 0 : (r() - 0.5) * ch * opts.jitter;
        grid[y][x] = [x * cw + jx, y * ch + jy];
      }
    }
    ctx.save();
    if (opts.lead) {
      ctx.strokeStyle = 'rgba(8,6,14,0.92)';
      ctx.lineJoin = 'round';
      ctx.lineWidth = Math.max(1.5, Math.min(w, h) * 0.004);
    }
    for (var gy = 0; gy < rows; gy++) {
      for (var gx = 0; gx < cols; gx++) {
        var a = grid[gy][gx], b = grid[gy][gx + 1], c = grid[gy + 1][gx + 1], d = grid[gy + 1][gx];
        var tris = opts.lead
          ? [[a, b, c, d]]
          : [[a, b, c], [a, c, d]];
        tris.forEach(function (t) {
          var mx = 0, my = 0;
          t.forEach(function (p) { mx += p[0] / t.length; my += p[1] / t.length; });
          ctx.beginPath();
          ctx.moveTo(t[0][0], t[0][1]);
          for (var i = 1; i < t.length; i++) ctx.lineTo(t[i][0], t[i][1]);
          ctx.closePath();
          ctx.fillStyle = sample(img, w, h, mx, my);
          ctx.fill();
          if (opts.lead) ctx.stroke();
        });
      }
    }
    ctx.restore();
  }

  STYLE.lowpoly = function (ctx, w, h, spec, P, r) {
    mosaic(ctx, w, h, r, { cols: 52, jitter: 0.75, lead: false });
    var img = read(ctx, w, h);
    if (img) {
      saturate(img, 1.18);
      contrast(img, 0.14);
      write(ctx, img);
    }
  };

  STYLE.glass = function (ctx, w, h, spec, P, r) {
    var img = read(ctx, w, h);
    if (img) {
      saturate(img, 1.7);
      posterize(img, 6);
      write(ctx, img);
    }
    mosaic(ctx, w, h, r, { cols: 22, jitter: 0.5, lead: true });
    var lit = read(ctx, w, h);
    if (lit) {
      bloom(lit, w, h, 150, Math.max(2, Math.round(Math.min(w, h) * 0.006)), 0.5);
      write(ctx, lit);
    }
  };

  STYLE.oil = function (ctx, w, h, spec, P, r) {
    var img = read(ctx, w, h);
    if (!img) return;
    var strokes = Math.round((w * h) / 1100);
    var len = Math.min(w, h) * 0.042;
    ctx.save();
    ctx.lineCap = 'round';
    for (var i = 0; i < strokes; i++) {
      var x = r() * w, y = r() * h;
      var a = (r() - 0.5) * 0.9 + (y / h) * 0.4;
      ctx.strokeStyle = sample(img, w, h, x, y);
      ctx.lineWidth = len * (0.28 + r() * 0.4);
      ctx.globalAlpha = 0.5 + r() * 0.45;
      ctx.beginPath();
      ctx.moveTo(x - Math.cos(a) * len, y - Math.sin(a) * len);
      ctx.lineTo(x + Math.cos(a) * len, y + Math.sin(a) * len);
      ctx.stroke();
    }
    ctx.restore();
    var out = read(ctx, w, h);
    if (out) {
      saturate(out, 1.16);
      contrast(out, 0.14);
      vignette(out, w, h, 0.3);
      write(ctx, out);
    }
  };

  STYLE.photo = function (ctx, w, h, spec, P, r) {
    var img = read(ctx, w, h);
    if (!img) return;
    contrast(img, 0.22);
    saturate(img, 1.1);
    grade(img, [16, 20, 34], [255, 244, 226], 0.16);
    bloom(img, w, h, 190, Math.max(2, Math.round(Math.min(w, h) * 0.008)), 0.35);
    grain(img, 12, r);
    vignette(img, w, h, 0.42);
    write(ctx, img);
  };

  /*
   * Run the style. Two style words run both passes, in the order they were
   * asked for — "watercolour pixel art" is a wash, then blocked out, and looks
   * like neither on its own.
   */
  function apply(ctx, w, h, spec, P) {
    var list = (spec.styles && spec.styles.length) ? spec.styles : [spec.style];
    list.forEach(function (id, i) {
      var fn = STYLE[id] || STYLE.poster;
      var r = PROMPT.rng(spec, 'finish' + (i ? i : ''));
      ctx.save();
      ctx.setTransform(1, 0, 0, 1, 0, 0);
      fn(ctx, w, h, spec, P, r);
      ctx.restore();
    });
    return list.join(' + ');
  }

  var API = {
    apply: apply,
    STYLE: STYLE,
    helpers: {
      read: read, write: write, blur: blur, bloom: bloom, pixelate: pixelate,
      posterize: posterize, dither: dither, edges: edges, inkEdges: inkEdges,
      contrast: contrast, saturate: saturate, grade: grade, grain: grain,
      vignette: vignette, luma: luma, mosaic: mosaic
    }
  };

  root.CodaFinish = API;
  if (typeof module !== 'undefined' && module.exports) module.exports = API;
})(typeof window !== 'undefined' ? window : this);
