/*
 * GOD'S EYE VIEW — earthquakes.
 *
 * Every quake of the past day from USGS, refreshed each minute: a dot sized and
 * coloured by magnitude, and a pulsing ring for anything M2.5 or bigger.
 * Follows the layer contract in docs/superpowers/plans/2026-10-21-godview-step1.md.
 */
(function (global) {
  'use strict';

  function create(opts) {
    const Q = global.GV_QUAKES;
    const P = global.GV_POLLER;
    const F = global.GV_FORMAT;
    const now = opts.now || Date.now;
    const listeners = [];

    const layer = {
      id: 'quakes',
      label: 'QUAKES',
      icon: '〰',
      status: 'idle',
      count: 0,
      points: [],
      rings: [],
      start() { poller.start(); },
      stop() { poller.stop(); show([]); },
      refresh() { poller.refresh(); },
      onChange(fn) { listeners.push(fn); },
      describe: describe
    };

    function emit() {
      for (const fn of listeners) fn(layer);
    }

    function show(list) {
      layer.count = list.length;
      layer.points = list.map((q) => ({
        layer: 'quakes',
        item: q,
        lat: q.lat,
        lng: q.lng,
        color: Q.color(q.mag),
        radius: 0.15 + Math.max(0, q.mag) * 0.08,
        altitude: 0.01
      }));
      layer.rings = list.filter((q) => q.mag >= 2.5).map((q) => ({
        lat: q.lat,
        lng: q.lng,
        color: Q.color(q.mag),
        maxRadius: Q.ringRadius(q.mag)
      }));
      emit();
    }

    function describe(q) {
      return {
        title: 'M' + q.mag.toFixed(1) + ' EARTHQUAKE',
        rows: [
          ['WHERE', q.place],
          ['WHEN', F.ago(now() - q.time)],
          ['DEPTH', q.depthKm === null ? 'unknown' : Math.round(q.depthKm) + ' km'],
          ['TIME', new Date(q.time).toISOString().replace('T', ' ').slice(0, 19) + ' UTC']
        ],
        link: q.url
      };
    }

    const poller = P.createPoller({
      intervalMs: 60000,
      retryMs: 5000,
      maxBackoffMs: 300000,
      setTimer: opts.setTimer,
      clearTimer: opts.clearTimer,
      fetchOnce: () => opts.fetchJson(Q.FEED_URL).then(Q.parse),
      onData: show,
      onStatus: (s) => { layer.status = s; emit(); }
    });

    return layer;
  }

  const API = { create: create };

  if (typeof module === 'object' && module.exports) module.exports = API;
  global.GV_QUAKES_LAYER = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
