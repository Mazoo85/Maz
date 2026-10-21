/*
 * GOD'S EYE VIEW — satellites.
 *
 * The space stations and the ~150 brightest satellites, from CelesTrak's two
 * groups, fetched every two hours (CelesTrak asks for no more). Between
 * fetches each satellite is moved by the orbit model once a second via tick(),
 * and the globe glides its marker to the new spot. Marker objects keep their
 * identity across ticks so the globe can tween them instead of redrawing.
 * Follows the layer contract in docs/superpowers/plans/2026-10-21-godview-step1.md,
 * with the html/tick additions in ...-step2-satellites.md.
 */
(function (global) {
  'use strict';

  const EARTH_RADIUS_KM = 6371;
  const STATION_COLOR = '#ffb300';
  const SAT_COLOR = '#1aff8c';
  /* The two crewed stations get a name on the globe; everything else is a dot. */
  const LABELS = { 25544: 'ISS', 48274: 'TIANGONG' };

  function create(opts) {
    const S = global.GV_SATS;
    const P = global.GV_POLLER;
    const F = global.GV_FORMAT;
    const satlib = opts.satlib || global.satellite;
    const now = opts.now || Date.now;
    const listeners = [];
    let markers = [];

    const layer = {
      id: 'sats',
      label: 'SATS',
      icon: '🛰',
      status: 'idle',
      count: 0,
      points: [],
      rings: [],
      html: [],
      start() { poller.start(); },
      stop() { poller.stop(); markers = []; place(); },
      refresh() { poller.refresh(); },
      tick() { place(); },
      onChange(fn) { listeners.push(fn); },
      describe: describe
    };

    function emit() {
      for (const fn of listeners) fn(layer);
    }

    /* Move every marker to where its satellite is now; drop the ones the
     * model cannot place. */
    function place() {
      const date = new Date(now());
      const shown = [];
      for (const m of markers) {
        const p = S.positionAt(m.item.satrec, date, satlib);
        if (!p) continue;
        Object.assign(m.item, p);
        m.lat = p.lat;
        m.lng = p.lng;
        m.altitude = p.altKm / EARTH_RADIUS_KM;
        shown.push(m);
      }
      layer.html = shown;
      layer.count = shown.length;
      emit();
    }

    function load(groups) {
      const seen = new Set();
      markers = [];
      for (const [kind, records] of groups) {
        for (const r of records) {
          if (seen.has(r.norad)) continue;
          seen.add(r.norad);
          const item = {
            norad: r.norad,
            name: r.name,
            kind: kind,
            satrec: satlib.twoline2satrec(r.line1, r.line2)
          };
          markers.push({
            layer: 'sats',
            item: item,
            lat: 0,
            lng: 0,
            altitude: 0,
            color: kind === 'station' ? STATION_COLOR : SAT_COLOR,
            label: LABELS[r.norad] || null
          });
        }
      }
      place();
    }

    /* Either group alone is worth showing; only both failing is a failure. */
    async function fetchBoth() {
      const settled = await Promise.allSettled([
        opts.fetchText(S.FEEDS.stations).then(S.parseTle),
        opts.fetchText(S.FEEDS.visual).then(S.parseTle)
      ]);
      const pick = (s) => (s.status === 'fulfilled' ? s.value : []);
      const stations = pick(settled[0]);
      const visual = pick(settled[1]);
      if (stations.length + visual.length === 0) throw new Error('no satellites from CelesTrak');
      return [['station', stations], ['sat', visual]];
    }

    function describe(sat) {
      const ns = sat.lat >= 0 ? 'N' : 'S';
      const ew = sat.lng >= 0 ? 'E' : 'W';
      return {
        title: sat.name,
        rows: [
          ['ALTITUDE', Math.round(sat.altKm) + ' km / ' + Math.round(sat.altKm / 1.609344) + ' mi'],
          ['SPEED', F.count(sat.speedKmS * 3600) + ' km/h / ' + F.count(sat.speedKmS * 3600 / 1.609344) + ' mph'],
          ['POSITION', Math.abs(sat.lat).toFixed(1) + '° ' + ns + ', ' + Math.abs(sat.lng).toFixed(1) + '° ' + ew],
          ['NORAD ID', String(sat.norad)]
        ],
        link: 'https://www.n2yo.com/satellite/?s=' + sat.norad
      };
    }

    const poller = P.createPoller({
      intervalMs: 2 * 3600 * 1000,
      retryMs: 10000,
      maxBackoffMs: 600000,
      setTimer: opts.setTimer,
      clearTimer: opts.clearTimer,
      fetchOnce: fetchBoth,
      onData: load,
      onStatus: (s) => { layer.status = s; emit(); }
    });

    return layer;
  }

  const API = { create: create };

  if (typeof module === 'object' && module.exports) module.exports = API;
  global.GV_SATS_LAYER = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
