/*
 * GOD'S EYE VIEW — the weather picture.
 *
 * Two sources become one texture for the globe's surface:
 *   · clouds — NASA's whole-world VIIRS photo of yesterday (UTC), one
 *     equirectangular image. Only the clouds are kept: bright, colourless
 *     pixels. Land, sea, desert and the black gaps between orbits drop out.
 *   · rain — RainViewer's newest radar frame, Web Mercator tiles stitched
 *     together and reprojected onto the same latitude/longitude grid.
 * Plain pixel arrays in, plain pixel array out: no DOM, so it is all tested
 * in Node.
 */
(function (global) {
  'use strict';

  const WIDTH = 2048;
  const HEIGHT = 1024;
  /* The globe's own dark green, so the texture looks like the bare globe
   * wherever there is no weather. */
  const BASE = [3, 20, 12];
  const CLOUD = [200, 245, 222];
  const MERCATOR_MAX_LAT = 85.0511;

  const RAIN_INDEX_URL = 'https://api.rainviewer.com/public/weather-maps.json';
  const RAIN_ZOOM = 2;

  function cloudsUrl(day) {
    return 'https://wvs.earthdata.nasa.gov/api/v1/snapshot?REQUEST=GetSnapshot' +
      '&LAYERS=VIIRS_NOAA21_CorrectedReflectance_TrueColor&CRS=EPSG:4326' +
      '&TIME=' + day + '&BBOX=-90,-180,90,180' +
      '&WIDTH=' + WIDTH + '&HEIGHT=' + HEIGHT + '&FORMAT=image/jpeg';
  }

  /* Yesterday in UTC: today's mosaic is still being filled in. */
  function cloudsDay(nowMs) {
    return new Date(nowMs - 86400000).toISOString().slice(0, 10);
  }

  function latestRainFrame(index) {
    const past = index && index.radar && index.radar.past;
    if (!index || typeof index.host !== 'string' || !Array.isArray(past) || past.length === 0) {
      throw new Error('no radar frames');
    }
    const f = past[past.length - 1];
    if (!f || typeof f.path !== 'string' || !isFinite(f.time)) throw new Error('bad radar frame');
    return { host: index.host, path: f.path, time: Number(f.time) };
  }

  /* 256 px tile, colour scheme 2 ("universal blue"), smoothed, with snow. */
  function rainTileUrl(host, path, z, x, y) {
    return host + path + '/256/' + z + '/' + x + '/' + y + '/2/1_1.png';
  }

  function ramp(v, lo, hi) {
    return Math.max(0, Math.min(1, (v - lo) / (hi - lo)));
  }

  /* How much of a true-colour pixel is cloud: bright AND colourless. */
  function cloudAlpha(r, g, b) {
    const max = Math.max(r, g, b);
    const min = Math.min(r, g, b);
    const brightness = (r + g + b) / 765;
    const saturation = max === 0 ? 0 : (max - min) / max;
    return ramp(brightness, 0.45, 0.8) * (1 - ramp(saturation, 0.12, 0.3));
  }

  /* Pixel row of a latitude on a square Web Mercator map `size` tall. */
  function mercatorY(lat, size) {
    const phi = lat * Math.PI / 180;
    return (1 - Math.log(Math.tan(Math.PI / 4 + phi / 2)) / Math.PI) / 2 * size;
  }

  /* tiles[row][col] = { width, height, data } or null; n × n of them. */
  function stitch(tiles, n) {
    let size = 0;
    for (const row of tiles) for (const t of row) if (t) size = t.width;
    const width = n * size;
    const data = new Uint8ClampedArray(width * width * 4);
    for (let ty = 0; ty < n; ty++) {
      for (let tx = 0; tx < n; tx++) {
        const t = tiles[ty] && tiles[ty][tx];
        if (!t) continue;
        for (let y = 0; y < size; y++) {
          const src = y * size * 4;
          data.set(t.data.subarray(src, src + size * 4), ((ty * size + y) * width + tx * size) * 4);
        }
      }
    }
    return { width: width, height: width, data: data };
  }

  function blend(out, i, color, alpha) {
    out[i] += (color[0] - out[i]) * alpha;
    out[i + 1] += (color[1] - out[i + 1]) * alpha;
    out[i + 2] += (color[2] - out[i + 2]) * alpha;
  }

  /* One opaque width × height equirectangular texture. `clouds` is any-size
   * equirectangular RGBA; `rain` is square Web Mercator RGBA. Either may be null. */
  function composite(opts) {
    const width = opts.width;
    const height = opts.height;
    const clouds = opts.clouds;
    const rain = opts.rain;
    const px = new Float32Array(width * height * 4);
    for (let y = 0; y < height; y++) {
      const lat = 90 - (y + 0.5) * 180 / height;
      const rainRow = rain && Math.abs(lat) <= MERCATOR_MAX_LAT ?
        Math.min(rain.height - 1, Math.floor(mercatorY(lat, rain.height))) : -1;
      const cloudRow = clouds ? Math.min(clouds.height - 1, Math.floor((y + 0.5) * clouds.height / height)) : -1;
      for (let x = 0; x < width; x++) {
        const i = (y * width + x) * 4;
        px[i] = BASE[0];
        px[i + 1] = BASE[1];
        px[i + 2] = BASE[2];
        px[i + 3] = 255;
        if (clouds) {
          const cx = Math.min(clouds.width - 1, Math.floor((x + 0.5) * clouds.width / width));
          const c = (cloudRow * clouds.width + cx) * 4;
          const a = cloudAlpha(clouds.data[c], clouds.data[c + 1], clouds.data[c + 2]);
          if (a > 0) blend(px, i, CLOUD, a * 0.85);
        }
        if (rainRow >= 0) {
          const rx = Math.min(rain.width - 1, Math.floor((x + 0.5) * rain.width / width));
          const r = (rainRow * rain.width + rx) * 4;
          const a = rain.data[r + 3] / 255;
          if (a > 0) blend(px, i, [rain.data[r], rain.data[r + 1], rain.data[r + 2]], a * 0.95);
        }
      }
    }
    return { width: width, height: height, data: Uint8ClampedArray.from(px) };
  }

  const API = {
    WIDTH: WIDTH,
    HEIGHT: HEIGHT,
    BASE: BASE,
    RAIN_INDEX_URL: RAIN_INDEX_URL,
    RAIN_ZOOM: RAIN_ZOOM,
    cloudsUrl: cloudsUrl,
    cloudsDay: cloudsDay,
    latestRainFrame: latestRainFrame,
    rainTileUrl: rainTileUrl,
    cloudAlpha: cloudAlpha,
    mercatorY: mercatorY,
    stitch: stitch,
    composite: composite
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  global.GV_WEATHER = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
