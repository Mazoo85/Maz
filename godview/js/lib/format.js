/*
 * GOD'S EYE VIEW — the words and numbers on screen.
 *
 * Plain functions, no DOM: the clock, counts with thousands separators,
 * "12 min ago", and the hex → rgba conversion the fading rings need.
 */
(function (global) {
  'use strict';

  function pad(n) {
    return (n < 10 ? '0' : '') + n;
  }

  function utcClock(date) {
    return pad(date.getUTCHours()) + ':' + pad(date.getUTCMinutes()) + ':' +
      pad(date.getUTCSeconds()) + ' UTC';
  }

  function count(n) {
    return String(Math.round(n)).replace(/\B(?=(\d{3})+(?!\d))/g, ',');
  }

  function ago(ms) {
    const s = Math.max(0, Math.floor(ms / 1000));
    if (s < 60) return 'just now';
    const m = Math.floor(s / 60);
    if (m < 60) return m + ' min ago';
    const h = Math.floor(m / 60);
    if (h < 48) return h + ' h ago';
    return Math.floor(h / 24) + ' days ago';
  }

  function rgba(hex, alpha) {
    const v = parseInt(hex.slice(1), 16);
    return 'rgba(' + ((v >> 16) & 255) + ',' + ((v >> 8) & 255) + ',' + (v & 255) + ',' + alpha + ')';
  }

  const API = { utcClock: utcClock, count: count, ago: ago, rgba: rgba };

  if (typeof module === 'object' && module.exports) module.exports = API;
  global.GV_FORMAT = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
