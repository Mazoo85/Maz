/*
 * GOD'S EYE VIEW — aircraft from the community ADS-B networks.
 *
 * Two keyless sources, both regional: adsb.lol answers for a 250 nautical-mile
 * circle, OpenSky for a box. Each has its own format and units; both become
 * the same plain Plane, in feet and knots. Rows without a position, with an
 * impossible one, or last placed more than a minute ago are dropped — a plane
 * drawn where it was two minutes ago is drawn where it is not.
 */
(function (global) {
  'use strict';

  const RADIUS_NM = 250;
  const NM_KM = 1.852;
  const EARTH_KM = 6371.0088;
  const KM_PER_DEG = 111.32;
  const MAX_AGE_S = 60;
  const M_TO_FT = 3.28084;
  const MS_TO_KT = 1.943844;

  function adsbUrl(lat, lng) {
    return 'https://api.adsb.lol/v2/point/' + lat.toFixed(2) + '/' + lng.toFixed(2) + '/' + RADIUS_NM;
  }

  /* The box around the 250 nm circle, kept on the map near the poles. */
  function openskyUrl(lat, lng) {
    const dLat = RADIUS_NM * NM_KM / KM_PER_DEG;
    const dLng = dLat / Math.max(0.05, Math.cos(lat * Math.PI / 180));
    const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
    const q = [
      'lamin=' + clamp(lat - dLat, -90, 90).toFixed(2),
      'lomin=' + clamp(lng - dLng, -180, 180).toFixed(2),
      'lamax=' + clamp(lat + dLat, -90, 90).toFixed(2),
      'lomax=' + clamp(lng + dLng, -180, 180).toFixed(2)
    ];
    return 'https://opensky-network.org/api/states/all?' + q.join('&');
  }

  function num(v) {
    return v === null || v === undefined || v === '' ? NaN : Number(v);
  }

  function placed(lat, lng) {
    return isFinite(lat) && isFinite(lng) && Math.abs(lat) <= 90 && Math.abs(lng) <= 180;
  }

  function plane(fields) {
    return {
      hex: String(fields.hex),
      callsign: fields.callsign ? String(fields.callsign).trim() : '',
      lat: fields.lat,
      lng: fields.lng,
      altFt: fields.onGround ? 0 : (isFinite(fields.altFt) ? fields.altFt : null),
      onGround: !!fields.onGround,
      speedKt: isFinite(fields.speedKt) ? fields.speedKt : 0,
      track: isFinite(fields.track) ? fields.track : 0,
      type: fields.type ? String(fields.type) : '',
      reg: fields.reg ? String(fields.reg) : ''
    };
  }

  function parseAdsb(json) {
    if (!json || !Array.isArray(json.ac)) throw new Error('not an adsb.lol answer');
    const out = [];
    for (const a of json.ac) {
      if (!a || !a.hex) continue;
      const lat = num(a.lat);
      const lng = num(a.lon);
      if (!placed(lat, lng)) continue;
      if (num(a.seen_pos) > MAX_AGE_S) continue;
      const ground = a.alt_baro === 'ground';
      out.push(plane({
        hex: a.hex, callsign: a.flight, lat: lat, lng: lng,
        altFt: ground ? 0 : num(a.alt_baro), onGround: ground,
        speedKt: num(a.gs), track: num(a.track), type: a.t, reg: a.r
      }));
    }
    return out;
  }

  /* OpenSky state vectors are positional arrays, in metres and m/s. */
  function parseOpensky(json) {
    if (!json || !('states' in json)) throw new Error('not an OpenSky answer');
    const now = num(json.time);
    const out = [];
    for (const s of json.states || []) {
      if (!Array.isArray(s) || !s[0]) continue;
      const lat = num(s[6]);
      const lng = num(s[5]);
      if (!placed(lat, lng)) continue;
      if (isFinite(now) && now - num(s[3]) > MAX_AGE_S) continue;
      out.push(plane({
        hex: s[0], callsign: s[1], lat: lat, lng: lng,
        altFt: num(s[7]) * M_TO_FT, onGround: s[8] === true,
        speedKt: num(s[9]) * MS_TO_KT, track: num(s[10])
      }));
    }
    return out;
  }

  const rad = (d) => d * Math.PI / 180;
  const deg = (r) => r * 180 / Math.PI;

  function distanceKm(a, b) {
    const dLat = rad(b.lat - a.lat);
    const dLng = rad(b.lng - a.lng);
    const h = Math.sin(dLat / 2) ** 2 + Math.cos(rad(a.lat)) * Math.cos(rad(b.lat)) * Math.sin(dLng / 2) ** 2;
    return 2 * EARTH_KM * Math.asin(Math.min(1, Math.sqrt(h)));
  }

  function nearest(list, centre, cap) {
    return list
      .map((p) => ({ p: p, d: distanceKm(centre, p) }))
      .sort((x, y) => x.d - y.d)
      .slice(0, cap)
      .map((x) => x.p);
  }

  /* Where a plane will be after `seconds` on its current track and speed. */
  function advance(p, seconds) {
    if (p.onGround || !(p.speedKt > 0)) return { lat: p.lat, lng: p.lng };
    const delta = p.speedKt * NM_KM * seconds / 3600 / EARTH_KM;
    const theta = rad(p.track);
    const phi1 = rad(p.lat);
    const lambda1 = rad(p.lng);
    const phi2 = Math.asin(Math.sin(phi1) * Math.cos(delta) + Math.cos(phi1) * Math.sin(delta) * Math.cos(theta));
    const lambda2 = lambda1 + Math.atan2(
      Math.sin(theta) * Math.sin(delta) * Math.cos(phi1),
      Math.cos(delta) - Math.sin(phi1) * Math.sin(phi2));
    return { lat: deg(phi2), lng: ((deg(lambda2) + 540) % 360) - 180 };
  }

  const API = {
    RADIUS_NM: RADIUS_NM,
    adsbUrl: adsbUrl,
    openskyUrl: openskyUrl,
    parseAdsb: parseAdsb,
    parseOpensky: parseOpensky,
    distanceKm: distanceKm,
    nearest: nearest,
    advance: advance
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  global.GV_PLANES = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
