/*
 * VELOCITY — the race.
 *
 * Owns the cars, the countdown, lap counting, positions, the finish and the
 * results, and the best-time record. Each update it asks the AI for its
 * inputs, steps every car, resolves collisions and near misses, and then
 * works out who is where. Records go through an injected storage object
 * ({get, set}) so the page can pass localStorage and the tests a fake.
 * No DOM, no Three.js.
 */
(function (root, factory) {
  const api = factory(root);
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.VelocityRace = api;
})(typeof self !== 'undefined' ? self : this, function (root) {
  'use strict';

  const inNode = typeof module === 'object' && module.exports;
  const Physics = inNode ? require('./physics.js') : root.VelocityPhysics;
  const AI = inNode ? require('./ai.js') : root.VelocityAI;

  const RECORD_KEY = 'velocity.records';
  const COUNTDOWN = 3;
  const NEAR_MISS_NITRO = 0.1;
  const AI_NITRO_TRICKLE = 0.04;   // per second, so rivals have nitro for the straights

  const DEFAULT_OPPONENTS = [
    { name: 'Vex', carKey: 'brawler', driver: { skill: 0.92, aggression: 0.6, lane: -2.5, phase: 0.3 } },
    { name: 'Nova', carKey: 'dart', driver: { skill: 0.85, aggression: 0.3, lane: 2.5, phase: 1.1 } },
    { name: 'Juno', carKey: 'drifter', driver: { skill: 0.8, aggression: 0.4, lane: 0, phase: 2.0 } },
    { name: 'Rook', carKey: 'comet', driver: { skill: 0.75, aggression: 0.5, lane: -1, phase: 2.9 } },
    { name: 'Sable', carKey: 'brawler', driver: { skill: 0.7, aggression: 0.2, lane: 1.5, phase: 3.7 } },
    { name: 'Pip', carKey: 'dart', driver: { skill: 0.6, aggression: 0.1, lane: -2, phase: 4.6 } },
    { name: 'Moss', carKey: 'comet', driver: { skill: 0.55, aggression: 0.3, lane: 2, phase: 5.4 } },
  ];

  function loadRecords(storage) {
    const empty = { bestTime: null, bestLap: null, carKey: null };
    if (!storage) return empty;
    try {
      const raw = storage.get(RECORD_KEY);
      if (!raw) return empty;
      const parsed = typeof raw === 'string' ? JSON.parse(raw) : raw;
      return Object.assign(empty, parsed);
    } catch (e) { return empty; }
  }

  function saveRecords(storage, records) {
    if (!storage) return false;
    try { storage.set(RECORD_KEY, JSON.stringify(records)); return true; }
    catch (e) { return false; }
  }

  function create(opts) {
    const track = opts.track;
    const laps = opts.laps || 3;
    const random = opts.random || Math.random;
    const slots = track.gridSlots();
    const opponents = (opts.opponents || DEFAULT_OPPONENTS).slice(0, 7);
    const entries = [];
    const add = (spec, slot, isPlayer) => {
      const car = Physics.createCar(spec.carKey, slot);
      car.isPlayer = isPlayer;
      entries.push({
        car, isPlayer, name: isPlayer ? 'You' : spec.name,
        driver: isPlayer ? null : AI.makeDriver(spec.driver),
        crossings: 0, progress: slot.s - track.length, lapTimes: [], lapStart: 0,
        finished: false, finishTime: null, place: entries.length + 1, nearMissCooldown: 0,
      });
    };
    opponents.forEach((o, i) => add(o, slots[i], false));
    add({ carKey: opts.playerCar || 'comet' }, slots[7], true);
    return {
      track, laps, random, storage: opts.storage || null,
      phase: 'countdown', countdown: COUNTDOWN, time: 0,
      entries, results: null, records: loadRecords(opts.storage), playerCarKey: opts.playerCar || 'comet',
    };
  }

  function playerEntry(race) { return race.entries.find((e) => e.isPlayer); }

  function lapNumber(entry, laps) { return Math.min(laps, Math.max(1, entry.crossings)); }

  function currentLapTime(race, entry) { return entry.finished ? entry.lapTimes[entry.lapTimes.length - 1] || 0 : race.time - entry.lapStart; }

  function standings(race) {
    return race.entries.slice().sort((a, b) => {
      if (a.finished && b.finished) return a.finishTime - b.finishTime;
      if (a.finished !== b.finished) return a.finished ? -1 : 1;
      return b.progress - a.progress;
    });
  }

  function resume(race) {
    if (race.phase === 'racing') { race.phase = 'countdown'; race.countdown = COUNTDOWN; }
  }

  function update(race, dt, playerInput) {
    if (race.phase === 'countdown') {
      race.countdown -= dt;
      if (race.countdown <= 0) { race.countdown = 0; race.phase = 'racing'; }
      return race;
    }
    if (race.phase !== 'racing') return race;
    race.time += dt;
    const L = race.track.length;
    const me = playerEntry(race);
    const cars = race.entries.map((e) => e.car);

    for (const e of race.entries) {
      let inp;
      if (e.isPlayer) inp = playerInput || { steer: 0 };
      else {
        inp = AI.input(e.car, e.driver, race.track, {
          time: race.time, cars, player: me.car, playerProgress: me.progress, progress: e.progress, random: race.random,
        });
        if (e.finished) inp.nitro = false;
        e.car.nitro = Math.min(1, e.car.nitro + dt * AI_NITRO_TRICKLE);
      }
      Physics.step(e.car, inp, race.track, dt);
    }

    for (let i = 0; i < cars.length; i++) {
      for (let j = i + 1; j < cars.length; j++) {
        if (Physics.collide(cars[i], cars[j])) continue;
        if ((cars[i].isPlayer || cars[j].isPlayer) && me.nearMissCooldown <= 0 && Physics.nearMiss(cars[i], cars[j])) {
          me.car.nitro = Math.min(1, me.car.nitro + NEAR_MISS_NITRO);
          me.car.nearMiss = true;
          me.nearMissCooldown = 1;
        }
      }
    }
    me.nearMissCooldown -= dt;

    for (const e of race.entries) {
      const c = e.car;
      if (c.prevS > L * 0.75 && c.s < L * 0.25) {
        e.crossings++;
        if (e.crossings === 1) e.lapStart = race.time;
        else { e.lapTimes.push(race.time - e.lapStart); e.lapStart = race.time; }
      } else if (c.prevS < L * 0.25 && c.s > L * 0.75) {
        e.crossings--;
        if (e.lapTimes.length) { e.lapStart -= e.lapTimes.pop(); }
      }
      e.progress = (e.crossings - 1) * L + c.s;
      if (!e.finished && e.crossings >= race.laps + 1) { e.finished = true; e.finishTime = race.time; }
    }

    standings(race).forEach((e, k) => { e.place = k + 1; });

    if (me.finished) {
      race.phase = 'finished';
      const bestLap = me.lapTimes.length ? Math.min.apply(null, me.lapTimes) : null;
      const rec = race.records;
      const newBest = rec.bestTime == null || me.finishTime < rec.bestTime;
      const newBestLap = bestLap != null && (rec.bestLap == null || bestLap < rec.bestLap);
      race.results = { place: me.place, total: me.finishTime, bestLap, lapTimes: me.lapTimes.slice(), newBest, newBestLap };
      if (newBest) { rec.bestTime = me.finishTime; rec.carKey = race.playerCarKey; }
      if (newBestLap) rec.bestLap = bestLap;
      if (newBest || newBestLap) saveRecords(race.storage, rec);
    }
    return race;
  }

  function formatTime(seconds) {
    if (seconds == null || !isFinite(seconds)) return '--:--.---';
    const ms = Math.round(seconds * 1000);
    const m = Math.floor(ms / 60000), s = Math.floor((ms % 60000) / 1000), r = ms % 1000;
    return m + ':' + String(s).padStart(2, '0') + '.' + String(r).padStart(3, '0');
  }

  return {
    create, update, playerEntry, standings, lapNumber, currentLapTime, formatTime,
    loadRecords, saveRecords, resume, DEFAULT_OPPONENTS, RECORD_KEY, COUNTDOWN,
  };
});
