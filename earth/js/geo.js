/*
 * EARTH EXPLORER — the maths and the saved-places file.
 *
 * Everything here is plain JavaScript with no map and no browser, so it runs
 * in Node for the tests and in the page as window.EarthGeo. Points are always
 * [longitude, latitude] in degrees, the order the map uses.
 */
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.EarthGeo = api;
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  // Mean radius of the Earth in metres (IUGG).
  const EARTH_RADIUS = 6371008.8;
  const METRES_PER_MILE = 1609.344;
  const METRES_PER_FOOT = 0.3048;
  const SQ_METRES_PER_ACRE = 4046.8564224;

  const rad = (d) => (d * Math.PI) / 180;
  const deg = (r) => (r * 180) / Math.PI;

  // Longitude difference b - a, taking the short way round (-180..180).
  function lngDelta(a, b) {
    let d = (b - a) % 360;
    if (d > 180) d -= 360;
    if (d < -180) d += 360;
    return d;
  }

  function wrapLng(lng) {
    const w = ((((lng + 180) % 360) + 360) % 360) - 180;
    return w === -180 && lng > 0 ? 180 : w;
  }

  // Great-circle distance in metres (haversine).
  function distance(a, b) {
    const dLat = rad(b[1] - a[1]);
    const dLng = rad(lngDelta(a[0], b[0]));
    const h = Math.sin(dLat / 2) ** 2 +
      Math.cos(rad(a[1])) * Math.cos(rad(b[1])) * Math.sin(dLng / 2) ** 2;
    return 2 * EARTH_RADIUS * Math.asin(Math.min(1, Math.sqrt(h)));
  }

  function pathLength(points) {
    let total = 0;
    for (let i = 1; i < points.length; i++) total += distance(points[i - 1], points[i]);
    return total;
  }

  // Area of a closed ring on the sphere, in square metres. The ring does not
  // need to repeat its first point. Same method as Chamberlain & Duquette
  // (JPL, 2007), which is what most web maps use.
  function polygonArea(points) {
    const n = points.length;
    if (n < 3) return 0;
    let total = 0;
    for (let i = 0; i < n; i++) {
      const p1 = points[i];
      const p2 = points[(i + 1) % n];
      total += rad(lngDelta(p1[0], p2[0])) * (2 + Math.sin(rad(p1[1])) + Math.sin(rad(p2[1])));
    }
    // A ring splits the sphere in two. When it goes round a pole the sum
    // above measures the other side, so take the smaller of the two.
    const area = Math.abs((total * EARTH_RADIUS * EARTH_RADIUS) / 2);
    return Math.min(area, 4 * Math.PI * EARTH_RADIUS * EARTH_RADIUS - area);
  }

  // The point a fraction t of the way along the great circle from a to b.
  function interpolate(a, b, t) {
    const lat1 = rad(a[1]), lng1 = rad(a[0]);
    const lat2 = rad(b[1]), lng2 = rad(b[0]);
    const d = distance(a, b) / EARTH_RADIUS;
    if (d < 1e-12) return [a[0], a[1]];
    const A = Math.sin((1 - t) * d) / Math.sin(d);
    const B = Math.sin(t * d) / Math.sin(d);
    const x = A * Math.cos(lat1) * Math.cos(lng1) + B * Math.cos(lat2) * Math.cos(lng2);
    const y = A * Math.cos(lat1) * Math.sin(lng1) + B * Math.cos(lat2) * Math.sin(lng2);
    const z = A * Math.sin(lat1) + B * Math.sin(lat2);
    return [deg(Math.atan2(y, x)), deg(Math.atan2(z, Math.sqrt(x * x + y * y)))];
  }

  // Adds points along each leg so a long line follows the curve of the Earth
  // instead of a straight line on the flat map. Longitudes are unwrapped (they
  // may run past ±180) so a line across the date line stays one piece.
  function densify(points, stepMetres) {
    const step = stepMetres || 50000;
    if (points.length < 2) return points.map((p) => [p[0], p[1]]);
    const out = [[points[0][0], points[0][1]]];
    for (let i = 1; i < points.length; i++) {
      const a = points[i - 1], b = points[i];
      const parts = Math.min(256, Math.max(1, Math.ceil(distance(a, b) / step)));
      for (let k = 1; k <= parts; k++) {
        const p = k === parts ? [b[0], b[1]] : interpolate(a, b, k / parts);
        const prev = out[out.length - 1];
        out.push([prev[0] + lngDelta(prev[0], p[0]), p[1]]);
      }
    }
    return out;
  }

  // ---- Words and numbers on screen -------------------------------------

  function grouped(n, decimals) {
    return n.toLocaleString('en-US', {
      minimumFractionDigits: decimals,
      maximumFractionDigits: decimals,
    });
  }

  function formatDistance(metres, units) {
    if (units === 'imperial') {
      const miles = metres / METRES_PER_MILE;
      if (miles < 0.1) return grouped(Math.round(metres / METRES_PER_FOOT), 0) + ' ft';
      if (miles < 10) return grouped(miles, 2) + ' mi';
      return grouped(Math.round(miles), 0) + ' mi';
    }
    if (metres < 1000) return grouped(Math.round(metres), 0) + ' m';
    if (metres < 10000) return grouped(metres / 1000, 2) + ' km';
    return grouped(Math.round(metres / 1000), 0) + ' km';
  }

  function formatArea(sqMetres, units) {
    if (units === 'imperial') {
      const acres = sqMetres / SQ_METRES_PER_ACRE;
      if (acres < 0.1) return grouped(Math.round(sqMetres / (METRES_PER_FOOT ** 2)), 0) + ' sq ft';
      if (acres < 640) return grouped(acres, acres < 10 ? 2 : 1) + ' acres';
      const sqMiles = sqMetres / (METRES_PER_MILE ** 2);
      return grouped(sqMiles, sqMiles < 10 ? 2 : 0) + ' sq mi';
    }
    if (sqMetres < 10000) return grouped(Math.round(sqMetres), 0) + ' m²';
    if (sqMetres < 1e6) return grouped(sqMetres / 10000, 2) + ' ha';
    const km2 = sqMetres / 1e6;
    return grouped(km2, km2 < 10 ? 2 : 0) + ' km²';
  }

  function formatElevation(metres, units) {
    if (units === 'imperial') return grouped(Math.round(metres / METRES_PER_FOOT), 0) + ' ft';
    return grouped(Math.round(metres), 0) + ' m';
  }

  function formatCoord(lat, lng) {
    const ns = lat >= 0 ? 'N' : 'S';
    const ew = lng >= 0 ? 'E' : 'W';
    return Math.abs(lat).toFixed(4) + '° ' + ns + ', ' + Math.abs(lng).toFixed(4) + '° ' + ew;
  }

  // Reads "48.8584, 2.2945" (or with spaces, or ° signs) typed into search.
  function parseCoordinates(text) {
    const m = String(text).trim().match(
      /^(-?\d{1,2}(?:\.\d+)?)\s*°?\s*([NS])?\s*[,\s]\s*(-?\d{1,3}(?:\.\d+)?)\s*°?\s*([EW])?$/i
    );
    if (!m) return null;
    let lat = parseFloat(m[1]);
    let lng = parseFloat(m[3]);
    if (m[2] && m[2].toUpperCase() === 'S') lat = -Math.abs(lat);
    if (m[4] && m[4].toUpperCase() === 'W') lng = -Math.abs(lng);
    if (lat < -90 || lat > 90 || lng < -180 || lng > 180) return null;
    return { lat, lng };
  }

  // ---- Saved places ------------------------------------------------------

  const NAME_MAX = 80;
  const NOTE_MAX = 1000;
  const FILE_APP = 'earth-explorer';
  const FILE_VERSION = 1;

  function newId() {
    return Date.now().toString(36) + Math.random().toString(36).slice(2, 8);
  }

  // Returns a clean copy of a saved place, or null if it can't be one.
  function normalizePlace(p) {
    if (!p || typeof p !== 'object') return null;
    const lat = Number(p.lat);
    const lng = Number(p.lng);
    if (!Number.isFinite(lat) || !Number.isFinite(lng) || lat < -90 || lat > 90) return null;
    const name = String(p.name == null ? '' : p.name).trim().slice(0, NAME_MAX) || 'Untitled place';
    const note = String(p.note == null ? '' : p.note).trim().slice(0, NOTE_MAX);
    const id = typeof p.id === 'string' && p.id ? p.id.slice(0, 40) : newId();
    const created = Number.isFinite(Number(p.created)) ? Number(p.created) : Date.now();
    const place = { id, name, note, lat, lng: wrapLng(lng), created };
    if (Number.isFinite(Number(p.zoom))) place.zoom = Math.min(22, Math.max(0, Number(p.zoom)));
    if (p.example === true) place.example = true;
    return place;
  }

  function serializePlaces(places) {
    return JSON.stringify({ app: FILE_APP, version: FILE_VERSION, places }, null, 2);
  }

  // Accepts our own file, or a bare list of places. Throws a readable error.
  function parsePlaces(text) {
    let data;
    try {
      data = JSON.parse(text);
    } catch (e) {
      throw new Error('That file is not a saved-places file.');
    }
    const list = Array.isArray(data) ? data : data && Array.isArray(data.places) ? data.places : null;
    if (!list) throw new Error('That file has no places in it.');
    return list.map(normalizePlace).filter(Boolean);
  }

  // Adds incoming places that aren't already saved: same id, or same name
  // within a metre of an existing place, counts as already saved.
  function mergePlaces(existing, incoming) {
    const out = existing.slice();
    let added = 0;
    for (const p of incoming) {
      const dup = out.some((q) => q.id === p.id ||
        (q.name === p.name && distance([q.lng, q.lat], [p.lng, p.lat]) < 1));
      if (!dup) {
        out.push(p);
        added++;
      }
    }
    return { places: out, added };
  }

  return {
    EARTH_RADIUS,
    lngDelta,
    wrapLng,
    distance,
    pathLength,
    polygonArea,
    interpolate,
    densify,
    formatDistance,
    formatArea,
    formatElevation,
    formatCoord,
    parseCoordinates,
    newId,
    normalizePlace,
    serializePlaces,
    parsePlaces,
    mergePlaces,
  };
});
