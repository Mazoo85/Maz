/* =============================================================================
 *  NEON CELLS  —  what survives death
 *
 *  A roguelite only works if dying still moves you forward. Cells spent at the
 *  Collector buy blueprints — weapons and skills that join the drop pool for
 *  every future run — and that, plus the run records and the Boss Cell
 *  difficulty you have earned, is what lives in here.
 *
 *  It persists to localStorage, and degrades to memory-only if storage is
 *  blocked (private windows, a test harness), so nothing ever throws over it.
 * ========================================================================== */
(function (global) {
  'use strict';

  const KEY = 'neon-cells-save-v1';

  const DEFAULTS = {
    unlocked: [],       // blueprint ids bought with cells
    cells: 0,           // cells banked but not yet spent
    bossCells: 0,       // difficulty earned by winning
    runs: 0,
    wins: 0,
    kills: 0,
    bestDepth: 0,
    bestTime: 0,
    deepestBiome: '',
    seenIntro: false
  };

  let memory = null;    // fallback when localStorage is unavailable

  function storage() {
    try {
      const s = global.localStorage;
      if (!s) return null;
      const probe = '__nc_probe__';
      s.setItem(probe, '1');
      s.removeItem(probe);
      return s;
    } catch (e) {
      return null;
    }
  }

  function load() {
    const s = storage();
    if (!s) return Object.assign({}, DEFAULTS, memory || {});
    try {
      const raw = s.getItem(KEY);
      if (!raw) return Object.assign({}, DEFAULTS);
      const parsed = JSON.parse(raw);
      return Object.assign({}, DEFAULTS, parsed && typeof parsed === 'object' ? parsed : {});
    } catch (e) {
      return Object.assign({}, DEFAULTS);
    }
  }

  function save(state) {
    memory = Object.assign({}, state);
    const s = storage();
    if (!s) return;
    try {
      s.setItem(KEY, JSON.stringify(state));
    } catch (e) {
      /* out of quota or blocked mid-session: memory still holds the run */
    }
  }

  function isUnlocked(id) {
    const state = load();
    return state.unlocked.indexOf(id) !== -1;
  }

  /* Returns true only if it was actually bought. */
  function unlock(id, cost) {
    const state = load();
    if (state.unlocked.indexOf(id) !== -1) return false;
    if (state.cells < cost) return false;
    state.cells -= cost;
    state.unlocked.push(id);
    save(state);
    return true;
  }

  function bankCells(n) {
    const state = load();
    state.cells += Math.max(0, Math.round(n));
    save(state);
    return state.cells;
  }

  function recordRun(summary) {
    const state = load();
    state.runs += 1;
    state.kills += summary.kills || 0;
    if (summary.won) {
      state.wins += 1;
      state.bossCells = Math.min(5, Math.max(state.bossCells, (summary.bossCells || 0) + 1));
      if (!state.bestTime || summary.time < state.bestTime) state.bestTime = summary.time;
    }
    if ((summary.depth || 0) > state.bestDepth) {
      state.bestDepth = summary.depth;
      state.deepestBiome = summary.biome || state.deepestBiome;
    }
    save(state);
    return state;
  }

  function markIntroSeen() {
    const state = load();
    state.seenIntro = true;
    save(state);
  }

  function reset() {
    memory = null;
    const s = storage();
    if (s) {
      try { s.removeItem(KEY); } catch (e) { /* nothing to do */ }
    }
    return Object.assign({}, DEFAULTS);
  }

  const API = {
    KEY: KEY,
    DEFAULTS: DEFAULTS,
    load: load,
    save: save,
    isUnlocked: isUnlocked,
    unlock: unlock,
    bankCells: bankCells,
    recordRun: recordRun,
    markIntroSeen: markIntroSeen,
    reset: reset
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  global.CELLS_META = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
