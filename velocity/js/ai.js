/*
 * VELOCITY — the opponents.
 *
 * An AI driver reads the car and the track and produces the same input a
 * human would: throttle, steer, drift, nitro. It aims at a point a little
 * way up the road in its lane, lifts for corners it is going too fast for,
 * drifts when it is still too fast, burns nitro on straights, steers round
 * the car in front, and (if aggressive) moves across to block the player
 * behind. Mild rubber-banding keeps the pack together. No DOM, no Three.js.
 */
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.VelocityAI = api;
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  const CORNER_G = 14;        // lateral m/s² the AI believes a car can hold
  const BAND_DISTANCE = 150;  // metres ahead/behind the player before rubber-banding kicks in

  const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
  function wrapAngle(a) {
    while (a > Math.PI) a -= 2 * Math.PI;
    while (a < -Math.PI) a += 2 * Math.PI;
    return a;
  }
  function wrapHalf(d, length) {
    while (d > length / 2) d -= length;
    while (d < -length / 2) d += length;
    return d;
  }

  function makeDriver(opts) {
    opts = opts || {};
    return {
      skill: opts.skill == null ? 0.7 : opts.skill,
      aggression: opts.aggression == null ? 0.3 : opts.aggression,
      lane: opts.lane == null ? 0 : opts.lane,
      phase: opts.phase == null ? 0 : opts.phase,
      blockUntil: 0, blockLane: 0,
    };
  }

  // The fastest speed a driver of this skill will take the road ahead of s.
  function cornerSpeed(track, s, skill) {
    let maxCurv = 0;
    for (let k = 8; k <= 70; k += 6) maxCurv = Math.max(maxCurv, Math.abs(track.at(s + k).curvature));
    const safe = Math.sqrt(CORNER_G / (maxCurv + 1e-4));
    return safe * (0.8 + 0.3 * skill);
  }

  function input(car, driver, track, ctx) {
    ctx = ctx || {};
    const time = ctx.time || 0;
    const L = track.length;
    const speed = car.vx * Math.cos(car.heading) + car.vz * Math.sin(car.heading);
    const half = Math.max(1.5, (car.roadWidth || 14) / 2 - 2);

    // Which lane: the driver's own, with a wobble that weaker drivers have more of.
    let lane = driver.lane + (1 - driver.skill) * 2.5 * Math.sin(time * 0.8 + driver.phase);

    // Steer round the car in front.
    const others = ctx.cars || [];
    for (const o of others) {
      if (o === car || o.wrecked) continue;
      const dS = wrapHalf(o.s - car.s, L);
      if (dS > 3 && dS < 18 && Math.abs(o.lateral - lane) < 2.6) lane = o.lateral + (o.lateral > 0 ? -3.2 : 3.2);
    }

    // Block the player behind, if that is the driver's style.
    const player = ctx.player;
    if (player && player !== car && !player.wrecked) {
      const dS = wrapHalf(player.s - car.s, L);
      const rnd = ctx.random || Math.random;
      if (time < driver.blockUntil) lane = driver.blockLane;
      else if (dS < -3 && dS > -14 && rnd() < driver.aggression * 0.02) {
        driver.blockLane = clamp(player.lateral, -half, half);
        driver.blockUntil = time + 1.5;
        lane = driver.blockLane;
      }
    }
    lane = clamp(lane, -half, half);

    // Aim at a point up the road in that lane.
    const look = 14 + Math.max(0, speed) * 0.4;
    const p = track.at(car.s + look);
    const tx = p.x - p.dz * lane, tz = p.z + p.dx * lane;
    const desired = Math.atan2(tz - car.z, tx - car.x);
    const diff = wrapAngle(desired - car.heading);
    const steer = clamp(diff * 2.2, -1, 1);

    // Speed for the road ahead.
    const safe = cornerSpeed(track, car.s, driver.skill);
    let throttle = speed < safe ? 1 : 0;
    const drift = speed > safe * 1.05 && Math.abs(steer) > 0.35 && speed > 15;
    let maxCurv = 0;
    for (let k = 8; k <= 70; k += 6) maxCurv = Math.max(maxCurv, Math.abs(track.at(car.s + k).curvature));
    let nitro = car.nitro > 0.35 && maxCurv < 0.006 && speed > 20;

    // Rubber-banding against the player.
    let powerScale = 1;
    if (ctx.playerProgress != null && ctx.progress != null) {
      const gap = ctx.playerProgress - ctx.progress;   // positive: the player is ahead
      if (gap < -BAND_DISTANCE) { powerScale = 0.9; nitro = false; }
      else if (gap > BAND_DISTANCE) { powerScale = 1.1; }
    }

    return { throttle, steer, drift, nitro, powerScale };
  }

  return { makeDriver, input, cornerSpeed, CORNER_G, BAND_DISTANCE };
});
