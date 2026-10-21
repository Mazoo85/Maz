/*
 * GOD'S EYE VIEW — the USGS earthquake feed.
 *
 * USGS publishes every quake of the past day as GeoJSON, keyless and open to
 * web pages. parse() turns it into plain points and drops anything it cannot
 * place for certain — a marker in the wrong place is worse than no marker.
 */
(function (global) {
  'use strict';

  const FEED_URL = 'https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/all_day.geojson';

  function num(v) {
    return v === null || v === undefined || v === '' ? NaN : Number(v);
  }

  function parse(json) {
    if (!json || !Array.isArray(json.features)) throw new Error('not a USGS feed');
    const out = [];
    for (const f of json.features) {
      const c = f && f.geometry && f.geometry.coordinates;
      const p = f && f.properties;
      if (!Array.isArray(c) || c.length < 2 || !p) continue;
      const lng = num(c[0]);
      const lat = num(c[1]);
      const mag = num(p.mag);
      if (!isFinite(lat) || !isFinite(lng) || !isFinite(mag)) continue;
      if (lat < -90 || lat > 90 || lng < -180 || lng > 180) continue;
      const depth = num(c[2]);
      out.push({
        id: String(f.id),
        lat: lat,
        lng: lng,
        depthKm: isFinite(depth) ? depth : null,
        mag: mag,
        place: p.place ? String(p.place) : 'Unknown location',
        time: num(p.time) || 0,
        url: p.url ? String(p.url) : null
      });
    }
    out.sort((a, b) => b.mag - a.mag);
    return out;
  }

  function color(mag) {
    if (mag >= 6) return '#ff3b3b';
    if (mag >= 4.5) return '#ff8a00';
    if (mag >= 2.5) return '#ffc400';
    return '#ffe680';
  }

  /* Ring size in degrees of arc on the globe. */
  function ringRadius(mag) {
    return Math.max(0.5, Math.min(8, mag * 1.2));
  }

  const API = { FEED_URL: FEED_URL, parse: parse, color: color, ringRadius: ringRadius };

  if (typeof module === 'object' && module.exports) module.exports = API;
  global.GV_QUAKES = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
