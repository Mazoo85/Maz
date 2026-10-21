/*
 * GOD'S EYE VIEW — cameras.
 *
 * Public road cameras from London, Finland and California. Every list is
 * fetched every 15 minutes; a source that refuses keeps its last good list, or
 * drops out, and only every source failing is OFFLINE. Thousands of cameras
 * would be too many markers for a phone, so only the 300 nearest the middle
 * of the screen are shown, re-picked from the lists already in hand whenever
 * the view settles somewhere new. Markers keep their identity across re-picks.
 * Follows the layer contract (step 1 plan) with describe().image from
 * docs/superpowers/plans/2026-10-21-godview-step5-cameras.md.
 */
(function (global) {
  'use strict';

  const CAP = 300;
  const LIST_MAX_AGE_MS = 15 * 60000;
  const COLOR = '#3ee6ff';

  function create(opts) {
    const CAM = global.GV_CAMERAS;
    const PL = global.GV_PLANES;
    const P = global.GV_POLLER;
    const now = opts.now || Date.now;
    const listeners = [];
    const lists = {}; // source id -> Camera[]
    const markers = new Map(); // camera id -> marker, so identity survives re-picks
    let fetchedAt = 0;

    const layer = {
      id: 'cameras',
      label: 'CAMERAS',
      icon: '📷',
      status: 'idle',
      count: 0,
      points: [],
      rings: [],
      html: [],
      credit: '',
      followsView: true,
      start() { poller.start(); },
      stop() {
        poller.stop();
        for (const k of Object.keys(lists)) delete lists[k];
        markers.clear();
        fetchedAt = 0;
        pick();
      },
      /* Re-pick from the lists in hand if they are fresh; otherwise fetch. */
      refresh() {
        if (layer.status === 'live' && now() - fetchedAt < LIST_MAX_AGE_MS) pick();
        else poller.refresh();
      },
      onChange(fn) { listeners.push(fn); },
      describe: describe
    };

    function emit() {
      for (const fn of listeners) fn(layer);
    }

    function pick() {
      const all = [];
      const seen = new Set();
      for (const src of CAM.SOURCES) {
        for (const c of lists[src.id] || []) {
          if (seen.has(c.id)) continue;
          seen.add(c.id);
          all.push(c);
        }
      }
      const chosen = all.length ? PL.nearest(all, opts.getView(), CAP) : [];
      layer.html = chosen.map((c) => {
        let m = markers.get(c.id);
        if (!m) {
          m = { layer: 'cameras', item: c, lat: c.lat, lng: c.lng, altitude: 0.002, color: COLOR, label: null };
          markers.set(c.id, m);
        }
        return m;
      });
      layer.count = all.length;
      layer.credit = CAM.SOURCES.filter((src) => (lists[src.id] || []).length).map((src) => src.credit).join(' · ');
      emit();
    }

    /* Every source, every list URL, at once. A source whose every URL fails
     * keeps the list it had. */
    async function fetchAll() {
      await Promise.all(CAM.SOURCES.map(async (src) => {
        const settled = await Promise.allSettled(src.urls.map((u) => opts.fetchJson(u).then(CAM.PARSERS[src.id])));
        const ok = settled.filter((s) => s.status === 'fulfilled');
        if (ok.length) lists[src.id] = ok.flatMap((s) => s.value);
      }));
      if (!CAM.SOURCES.some((src) => (lists[src.id] || []).length)) throw new Error('no camera lists');
      fetchedAt = now();
    }

    function describe(c) {
      const src = CAM.SOURCES.find((x) => x.id === c.source);
      const ns = c.lat >= 0 ? 'N' : 'S';
      const ew = c.lng >= 0 ? 'E' : 'W';
      return {
        title: c.name,
        rows: [
          ['SOURCE', src ? src.label : c.source],
          ['POSITION', Math.abs(c.lat).toFixed(3) + '° ' + ns + ', ' + Math.abs(c.lng).toFixed(3) + '° ' + ew]
        ],
        image: c.image,
        link: c.image
      };
    }

    const poller = P.createPoller({
      intervalMs: LIST_MAX_AGE_MS,
      retryMs: 15000,
      maxBackoffMs: 600000,
      setTimer: opts.setTimer,
      clearTimer: opts.clearTimer,
      fetchOnce: fetchAll,
      onData: pick,
      onStatus: (s) => { layer.status = s; emit(); }
    });

    return layer;
  }

  const API = { create: create };

  if (typeof module === 'object' && module.exports) module.exports = API;
  global.GV_CAMERAS_LAYER = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
