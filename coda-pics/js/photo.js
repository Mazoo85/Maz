/*
 * CODA PICS — reading a photograph.
 * ---------------------------------
 * Everything here turns a picture somebody already has into material the
 * painter can use: the colours that are actually in it, the shape of its
 * horizon, and where its light is coming from.
 *
 * Two things this deliberately is not. It is not a model — nothing here learns
 * anything or recognises anything, it measures pixels. And it never sends a
 * photograph anywhere: the browser reads the file, these functions read the
 * pixels, and that is the entire journey. The app works with the network off,
 * and a photograph put into it does not become an upload.
 *
 * The analysis is pure arithmetic over an ImageData, so the tests run it in
 * Node against images they build themselves — no browser, and no real
 * photographs needed to prove it works.
 *
 * Exposed as window.CodaPhoto (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  function clamp(v, lo, hi) { return v < lo ? lo : v > hi ? hi : v; }

  /* ------------------------------------------------------------- colour */
  function rgbToHsl(r, g, b) {
    r /= 255; g /= 255; b /= 255;
    var max = Math.max(r, g, b), min = Math.min(r, g, b);
    var l = (max + min) / 2, h = 0, s = 0;
    if (max !== min) {
      var d = max - min;
      s = l > 0.5 ? d / (2 - max - min) : d / (max + min);
      if (max === r) h = ((g - b) / d + (g < b ? 6 : 0));
      else if (max === g) h = (b - r) / d + 2;
      else h = (r - g) / d + 4;
      h *= 60;
    }
    return [h, s * 100, l * 100];
  }

  function luma(r, g, b) { return 0.2126 * r + 0.7152 * g + 0.0722 * b; }

  /* Average colour of a horizontal band, as HSL. Bands rather than single rows
   * because one row of a photograph is mostly noise. */
  function band(img, w, h, fromY, toY) {
    var d = img.data, r = 0, g = 0, b = 0, n = 0;
    var y0 = clamp(Math.floor(fromY * h), 0, h - 1);
    var y1 = clamp(Math.ceil(toY * h), y0 + 1, h);
    var step = Math.max(1, Math.floor((y1 - y0) * w / 4000));
    for (var y = y0; y < y1; y += 1) {
      for (var x = 0; x < w; x += step) {
        var i = (y * w + x) * 4;
        r += d[i]; g += d[i + 1]; b += d[i + 2]; n++;
      }
    }
    if (!n) return [0, 0, 50];
    return rgbToHsl(r / n, g / n, b / n);
  }

  /*
   * The colours a photograph is actually made of, commonest first. Pixels are
   * dropped into a coarse 4x4x4 cube and the fullest cells win — enough to tell
   * "a warm evening" from "a grey morning", which is all the painter needs.
   */
  function dominant(img, w, h, count) {
    var d = img.data;
    var bins = {};
    var step = Math.max(4, Math.floor((w * h) / 60000)) * 4;
    for (var i = 0; i < d.length; i += step) {
      if (d[i + 3] < 128) continue;
      var key = ((d[i] >> 6) << 4) | ((d[i + 1] >> 6) << 2) | (d[i + 2] >> 6);
      var bin = bins[key] || (bins[key] = { n: 0, r: 0, g: 0, b: 0 });
      bin.n++; bin.r += d[i]; bin.g += d[i + 1]; bin.b += d[i + 2];
    }
    var list = Object.keys(bins).map(function (k) { return bins[k]; });
    list.sort(function (a, b2) { return b2.n - a.n; });
    return list.slice(0, count || 5).map(function (bin) {
      return rgbToHsl(bin.r / bin.n, bin.g / bin.n, bin.b / bin.n);
    });
  }

  /* The darkest and lightest tenths, which is what a silhouette and a highlight
   * should be made of if they are to belong to this photograph. */
  function extremes(img, w, h) {
    var d = img.data, vals = [];
    var step = Math.max(4, Math.floor((w * h) / 40000)) * 4;
    for (var i = 0; i < d.length; i += step) {
      vals.push([luma(d[i], d[i + 1], d[i + 2]), d[i], d[i + 1], d[i + 2]]);
    }
    if (!vals.length) return { dark: [0, 0, 10], bright: [0, 0, 90] };
    vals.sort(function (a, b) { return a[0] - b[0]; });
    function avg(from, to) {
      var r = 0, g = 0, b = 0, n = 0;
      for (var i = from; i < to; i++) { r += vals[i][1]; g += vals[i][2]; b += vals[i][3]; n++; }
      return rgbToHsl(r / n, g / n, b / n);
    }
    var tenth = Math.max(1, Math.floor(vals.length / 10));
    return { dark: avg(0, tenth), bright: avg(vals.length - tenth, vals.length) };
  }

  /*
   * Where the light is. The frame is reduced to a coarse grid and the brightest
   * cell wins — in a landscape that is the sun, a bright sky, or a window, all
   * of which are the right answer for "which way do the shadows fall".
   */
  function lightPosition(img, w, h) {
    var d = img.data, GX = 12, GY = 8;
    var best = -1, bx = 0.5, by = 0.25;
    for (var gy = 0; gy < GY; gy++) {
      for (var gx = 0; gx < GX; gx++) {
        var x0 = Math.floor(gx * w / GX), x1 = Math.floor((gx + 1) * w / GX);
        var y0 = Math.floor(gy * h / GY), y1 = Math.floor((gy + 1) * h / GY);
        var sum = 0, n = 0;
        for (var y = y0; y < y1; y += 2) {
          for (var x = x0; x < x1; x += 2) {
            var i = (y * w + x) * 4;
            sum += luma(d[i], d[i + 1], d[i + 2]);
            n++;
          }
        }
        var mean = n ? sum / n : 0;
        if (mean > best) {
          best = mean;
          bx = (gx + 0.5) / GX;
          by = (gy + 0.5) / GY;
        }
      }
    }
    return { x: bx, y: by };
  }

  /*
   * The line where the photograph stops being sky. For each column, the row
   * with the sharpest vertical change in brightness — the skyline of a city, a
   * ridge of hills, the edge of a sea. Returned as a fraction of the height per
   * column, smoothed, because a real horizon does not jump about.
   *
   * `confidence` says whether this is worth using at all: a photograph with no
   * clear horizon (a close-up, a flat wall) produces a scattered line, and the
   * app would rather say so than draw nonsense.
   */
  function skyline(img, w, h, columns) {
    var d = img.data;
    var COLS = columns || 64;
    var raw = [];
    for (var c = 0; c < COLS; c++) {
      var x = Math.min(w - 1, Math.floor((c + 0.5) * w / COLS));
      var bestDrop = 0, bestY = 0.55;
      var prev = null;
      for (var y = Math.floor(h * 0.05); y < Math.floor(h * 0.95); y++) {
        var i = (y * w + x) * 4;
        var v = luma(d[i], d[i + 1], d[i + 2]);
        if (prev !== null) {
          var drop = prev - v;                 // bright above, darker below
          if (drop > bestDrop) { bestDrop = drop; bestY = y / h; }
        }
        prev = v;
      }
      raw.push({ y: bestY, strength: bestDrop });
    }

    /* Smooth, so one dark branch does not become a cliff. */
    var line = raw.map(function (p, i) {
      var a = raw[Math.max(0, i - 1)].y, b = raw[Math.min(raw.length - 1, i + 1)].y;
      return (a + p.y + b) / 3;
    });

    /* Confidence needs BOTH things, which is why they multiply rather than add.
     * A flat wall has columns in perfect agreement — on a default, because
     * there was no edge to find — and scoring that as agreement made a blank
     * image claim a horizon. Noise is the opposite: an edge in every column,
     * all of them somewhere different. Neither is a horizon. */
    var strong = raw.filter(function (p) { return p.strength > 18; }).length / raw.length;
    var mean = line.reduce(function (a, b) { return a + b; }, 0) / line.length;
    var spread = Math.sqrt(line.reduce(function (a, b) {
      return a + (b - mean) * (b - mean);
    }, 0) / line.length);
    var agree = clamp(1 - spread / 0.22, 0, 1);

    return { line: line, mean: mean, confidence: clamp(strong * agree, 0, 1) };
  }

  /*
   * Everything the painter can use from one photograph, in one pass-friendly
   * object. `palette` is shaped exactly like the painter's own colour table so
   * it can be dropped in where the built-in one would go.
   */
  function analyse(img, w, h) {
    var ext = extremes(img, w, h);
    var top = band(img, w, h, 0.00, 0.18);
    var mid = band(img, w, h, 0.18, 0.42);
    var low = band(img, w, h, 0.42, 0.58);
    var ground = band(img, w, h, 0.70, 1.00);
    var far = band(img, w, h, 0.55, 0.72);
    var sky = skyline(img, w, h);

    return {
      colours: dominant(img, w, h, 6),
      palette: {
        sky: {
          top: top, mid: mid, low: low,
          light: ext.bright,
          lightY: lightPosition(img, w, h).y,
          haze: low
        },
        scene: {
          ink: [ext.dark[0], Math.min(ext.dark[1], 55), Math.max(ext.dark[2], 6)],
          land: ground,
          far: far,
          sea: ground
        }
      },
      light: lightPosition(img, w, h),
      skyline: sky
    };
  }

  var API = {
    analyse: analyse,
    dominant: dominant,
    skyline: skyline,
    lightPosition: lightPosition,
    extremes: extremes,
    band: band,
    rgbToHsl: rgbToHsl
  };

  root.CodaPhoto = API;
  if (typeof module !== 'undefined' && module.exports) module.exports = API;
})(typeof window !== 'undefined' ? window : this);
