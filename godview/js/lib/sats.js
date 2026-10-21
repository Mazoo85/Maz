/*
 * GOD'S EYE VIEW — satellites from CelesTrak.
 *
 * CelesTrak publishes every tracked satellite's orbit as "two-line elements"
 * (TLE): a name line and two fixed-column lines, each ending in a checksum.
 * parseTle() keeps only records that are whole and pass the checksum — a
 * corrupted orbit would put a satellite somewhere it is not. positionAt() runs
 * the orbit forward to a moment in time with the SGP4 model (satellite.js) and
 * returns null rather than a guess when the model cannot place it.
 */
(function (global) {
  'use strict';

  const BASE = 'https://celestrak.org/NORAD/elements/gp.php?FORMAT=tle&GROUP=';
  const FEEDS = {
    stations: BASE + 'stations',
    visual: BASE + 'visual'
  };

  /* The TLE checksum: sum of the digits in columns 1–68, each minus counting
   * as 1, mod 10. */
  function checksum(line) {
    let sum = 0;
    for (let i = 0; i < 68 && i < line.length; i++) {
      const c = line[i];
      if (c >= '0' && c <= '9') sum += c.charCodeAt(0) - 48;
      else if (c === '-') sum += 1;
    }
    return sum % 10;
  }

  function validLine(line, n) {
    return line.length >= 69 && line[0] === String(n) && line[1] === ' ' &&
      checksum(line) === Number(line[68]);
  }

  function parseTle(text) {
    if (typeof text !== 'string') throw new Error('not a TLE feed');
    const lines = text.split(/\r?\n/).map((l) => l.replace(/\s+$/, ''));
    const out = [];
    for (let i = 0; i + 2 < lines.length; i++) {
      const l1 = lines[i + 1];
      const l2 = lines[i + 2];
      if (!validLine(l1, 1) || !validLine(l2, 2)) continue;
      const norad = Number(l1.slice(2, 7));
      if (!isFinite(norad) || norad !== Number(l2.slice(2, 7))) continue;
      out.push({ name: lines[i].trim() || 'NORAD ' + norad, norad: norad, line1: l1, line2: l2 });
      i += 2;
    }
    return out;
  }

  /* Where a satellite is at `date`: degrees, km above the surface, km/s. */
  function positionAt(satrec, date, satlib) {
    let pv;
    try {
      pv = satlib.propagate(satrec, date);
    } catch (e) {
      return null;
    }
    if (!pv || !pv.position || !pv.velocity || satrec.error) return null;
    const geo = satlib.eciToGeodetic(pv.position, satlib.gstime(date));
    const lat = satlib.degreesLat(geo.latitude);
    const lng = satlib.degreesLong(geo.longitude);
    const altKm = geo.height;
    const v = pv.velocity;
    const speedKmS = Math.sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (![lat, lng, altKm, speedKmS].every(isFinite) || altKm < 80) return null;
    return { lat: lat, lng: lng, altKm: altKm, speedKmS: speedKmS };
  }

  const API = { FEEDS: FEEDS, checksum: checksum, parseTle: parseTle, positionAt: positionAt };

  if (typeof module === 'object' && module.exports) module.exports = API;
  global.GV_SATS = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
