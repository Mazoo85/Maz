/*
 * GOD'S EYE VIEW — fetch something on a timer, and survive it failing.
 *
 * Every live layer is "ask a source every N seconds". When the source fails
 * the layer says OFFLINE and retries sooner, doubling the wait each time up to
 * a cap, then drops back to the normal interval the moment it answers again.
 * A stop() — or a refresh() — makes any answer still in flight be ignored.
 * With offlineAfter set, a source that refuses now and then (a busy, rate-
 * limited service) keeps its last good status until that many failures in a
 * row, so one passing refusal does not flash OFFLINE.
 *
 * Timers are injectable so the tests can run a minute of polling instantly.
 */
(function (global) {
  'use strict';

  function createPoller(opts) {
    const intervalMs = opts.intervalMs;
    const retryMs = opts.retryMs || 5000;
    const maxBackoffMs = opts.maxBackoffMs || intervalMs * 5;
    const offlineAfter = opts.offlineAfter || 1;
    const setTimer = opts.setTimer || ((fn, ms) => setTimeout(fn, ms));
    const clearTimer = opts.clearTimer || ((h) => clearTimeout(h));

    let timer = null;
    let running = false;
    let failures = 0;
    let status = 'idle';
    let generation = 0;

    function setStatus(s) {
      if (s === status) return;
      status = s;
      if (opts.onStatus) opts.onStatus(s);
    }

    function cancelTimer() {
      if (timer !== null) {
        clearTimer(timer);
        timer = null;
      }
    }

    function nextDelay() {
      if (failures === 0) return intervalMs;
      return Math.min(maxBackoffMs, retryMs * Math.pow(2, failures - 1));
    }

    async function tick() {
      timer = null;
      if (!running) return;
      const mine = ++generation;
      try {
        const data = await opts.fetchOnce();
        if (!running || mine !== generation) return;
        opts.onData(data);
        failures = 0;
        setStatus('live');
      } catch (e) {
        if (!running || mine !== generation) return;
        failures++;
        if (failures >= offlineAfter) setStatus('offline');
      }
      timer = setTimer(tick, nextDelay());
    }

    return {
      start() {
        if (running) return;
        running = true;
        failures = 0;
        setStatus('loading');
        tick();
      },
      stop() {
        running = false;
        generation++;
        cancelTimer();
        setStatus('idle');
      },
      refresh() {
        if (!running) return;
        cancelTimer();
        tick();
      },
      get status() { return status; },
      get failures() { return failures; }
    };
  }

  const API = { createPoller: createPoller };

  if (typeof module === 'object' && module.exports) module.exports = API;
  global.GV_POLLER = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
