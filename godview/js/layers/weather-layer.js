/*
 * GOD'S EYE VIEW — weather.
 *
 * Live rain radar (RainViewer, new frame every 10 minutes) and yesterday's
 * cloud cover (NASA, whole world), baked into one texture for the globe's
 * surface. Either source alone is worth showing; only both failing is
 * OFFLINE. The clouds photo is large and changes daily, so it is only
 * downloaded again when its day changes or an hour has passed; a radar frame
 * already on screen is never downloaded twice.
 * Follows the layer contract (step 1 plan) with the texture / summary / credit
 * additions in docs/superpowers/plans/2026-10-21-godview-step3-weather.md.
 */
(function (global) {
  'use strict';

  const CLOUDS_MAX_AGE_MS = 3600 * 1000;

  function create(opts) {
    const W = global.GV_WEATHER;
    const P = global.GV_POLLER;
    const F = global.GV_FORMAT;
    const now = opts.now || Date.now;
    const size = opts.size || { width: W.WIDTH, height: W.HEIGHT };
    const listeners = [];
    let clouds = null; // { img, day, at }
    let rain = null; // { img, time }

    const layer = {
      id: 'weather',
      label: 'WEATHER',
      icon: '☁',
      status: 'idle',
      count: 0,
      points: [],
      rings: [],
      html: [],
      texture: null,
      summary: '',
      credit: '',
      start() { poller.start(); },
      stop() {
        poller.stop();
        clouds = null;
        rain = null;
        show();
      },
      refresh() { poller.refresh(); },
      onChange(fn) { listeners.push(fn); },
      describe: () => null
    };

    function emit() {
      for (const fn of listeners) fn(layer);
    }

    function show() {
      const parts = [];
      if (clouds) parts.push('Clouds: NASA GIBS / VIIRS');
      if (rain) parts.push('Rain: RainViewer');
      layer.credit = parts.join(' · ');
      layer.count = parts.length;
      layer.summary = rain ? F.utcClock(new Date(rain.time * 1000)).replace(/:\d\d UTC$/, ' UTC') :
        clouds ? 'CLOUDS' : '';
      layer.texture = clouds || rain ? W.composite({
        width: size.width,
        height: size.height,
        clouds: clouds && clouds.img,
        rain: rain && rain.img
      }) : null;
      emit();
    }

    async function loadClouds() {
      const day = W.cloudsDay(now());
      if (clouds && clouds.day === day && now() - clouds.at < CLOUDS_MAX_AGE_MS) return;
      const img = await opts.loadPixels(W.cloudsUrl(day));
      clouds = { img: img, day: day, at: now() };
    }

    async function loadRain() {
      const frame = W.latestRainFrame(await opts.fetchJson(W.RAIN_INDEX_URL));
      if (rain && rain.time === frame.time) return;
      const n = Math.pow(2, W.RAIN_ZOOM);
      const rows = [];
      for (let y = 0; y < n; y++) {
        const row = [];
        for (let x = 0; x < n; x++) {
          row.push(opts.loadPixels(W.rainTileUrl(frame.host, frame.path, W.RAIN_ZOOM, x, y))
            .catch(() => null));
        }
        rows.push(Promise.all(row));
      }
      const tiles = await Promise.all(rows);
      if (!tiles.some((row) => row.some(Boolean))) throw new Error('no radar tiles');
      rain = { img: W.stitch(tiles, n), time: frame.time };
    }

    /* A source that fails keeps its last good picture, if it has one. */
    async function fetchBoth() {
      await Promise.allSettled([loadClouds(), loadRain()]);
      if (!clouds && !rain) throw new Error('no weather from NASA or RainViewer');
    }

    const poller = P.createPoller({
      intervalMs: 600000,
      retryMs: 15000,
      maxBackoffMs: 600000,
      setTimer: opts.setTimer,
      clearTimer: opts.clearTimer,
      fetchOnce: fetchBoth,
      onData: show,
      onStatus: (s) => { layer.status = s; emit(); }
    });

    return layer;
  }

  const API = { create: create };

  if (typeof module === 'object' && module.exports) module.exports = API;
  global.GV_WEATHER_LAYER = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
