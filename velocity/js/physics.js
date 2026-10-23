/*
 * VELOCITY — car physics.
 *
 * One car is a plain object; step() moves it one frame from an input of the
 * same shape a human or the AI produces. It is arcade physics: the car
 * accelerates on its own, grip pulls the car's velocity in line with its
 * heading, a drift loosens that grip, nitro raises the ceiling for a moment,
 * the ramp throws the car in the air, and walls scrape or wreck. No DOM, no
 * Three.js: this runs in plain Node.
 */
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.VelocityPhysics = api;
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  // Speeds in m/s (62 m/s is about 220 km/h), accelerations in m/s².
  const CARS = {
    comet: { key: 'comet', name: 'Comet', topSpeed: 62, accel: 14, handling: 1.0, nitroPower: 1.0, driftGrip: 2.2, nitroGain: 1.0, color: 0xff4d2e, blurb: 'The all-rounder. Good at everything, best at nothing.' },
    brawler: { key: 'brawler', name: 'Brawler', topSpeed: 70, accel: 11, handling: 0.75, nitroPower: 1.3, driftGrip: 2.6, nitroGain: 0.9, color: 0x2e8bff, blurb: 'Fastest in a straight line. Turns like a boat.' },
    dart: { key: 'dart', name: 'Dart', topSpeed: 56, accel: 20, handling: 1.35, nitroPower: 0.9, driftGrip: 2.0, nitroGain: 1.0, color: 0xffc81e, blurb: 'First off the line and into every gap.' },
    drifter: { key: 'drifter', name: 'Drifter', topSpeed: 60, accel: 13, handling: 1.1, nitroPower: 1.0, driftGrip: 1.2, nitroGain: 1.6, color: 0xb04dff, blurb: 'Lives sideways. Nitro for days.' },
  };
  const ORDER = ['comet', 'brawler', 'dart', 'drifter'];

  const GRIP = 9;                // how fast sideways velocity dies normally (per second)
  const CAR_HALF_WIDTH = 1.0;
  const CAR_RADIUS = 1.4;
  const WRECK_WALL_SPEED = 18;   // m/s into a wall
  const WRECK_RAM_SPEED = 22;    // m/s closing speed between two cars
  const WRECK_TIME = 2.0;
  const NITRO_BURN = 1.5;        // seconds one tap burns for
  const NITRO_FULL_BURN = 4.0;   // seconds a full bar lasts
  const NITRO_TOP = 1.3;         // top speed multiplier while burning
  const RAMP_LAUNCH = 0.22;      // vertical speed as a share of forward speed
  const GRAVITY = 16;            // snappier than Earth: this is arcade
  const EMPTY = { throttle: 1, steer: 0, drift: false, nitro: false };

  const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
  function wrapAngle(a) {
    while (a > Math.PI) a -= 2 * Math.PI;
    while (a < -Math.PI) a += 2 * Math.PI;
    return a;
  }

  function createCar(key, slot) {
    const stats = CARS[key] || CARS.comet;
    slot = slot || { x: 0, z: 0, heading: 0, s: 0, hint: 0 };
    return {
      stats, key: stats.key, color: stats.color,
      x: slot.x, z: slot.z, y: 0, vy: 0, heading: slot.heading, vx: 0, vz: 0, speed: 0,
      nitro: 0.25, nitroBurn: 0, nitroActive: false, drifting: false, braking: false,
      airborne: false, landed: false, wrecked: false, wreckTimer: 0, wrecks: 0,
      sparks: false, bumped: false,
      hint: slot.hint || 0, s: slot.s || 0, prevS: slot.s || 0, lateral: 0, roadWidth: 14, curvature: 0, tunnel: false,
    };
  }

  function forwardSpeed(car) {
    return car.vx * Math.cos(car.heading) + car.vz * Math.sin(car.heading);
  }

  function wreck(car) {
    car.wrecked = true;
    car.wreckTimer = WRECK_TIME;
    car.wrecks++;
    car.vx = 0; car.vz = 0; car.speed = 0;
    car.nitroBurn = 0; car.nitroActive = false; car.drifting = false; car.braking = false;
    car.airborne = false; car.vy = 0; car.y = 0;
  }

  function respawn(car, track) {
    const p = track.at(car.s);
    car.x = p.x; car.z = p.z; car.y = 0; car.vy = 0;
    car.heading = Math.atan2(p.dz, p.dx);
    car.vx = p.dx * 5; car.vz = p.dz * 5;
    car.hint = p.i; car.lateral = 0; car.wrecked = false; car.wreckTimer = 0;
  }

  function step(car, input, track, dt) {
    input = input || EMPTY;
    const st = car.stats;
    car.sparks = false; car.bumped = false; car.landed = false;
    car.prevS = car.s;

    if (car.wrecked) {
      car.wreckTimer -= dt;
      if (car.wreckTimer <= 0) respawn(car, track);
      return car;
    }

    const scale = input.powerScale || 1;
    let topSpeed = st.topSpeed * scale;
    let accel = st.accel * scale;

    // Nitro: a tap starts a burn that lasts while there is nitro left.
    if (input.nitro && car.nitro > 0 && !car.airborne) car.nitroBurn = NITRO_BURN;
    car.nitroActive = car.nitroBurn > 0 && car.nitro > 0 && !car.airborne;
    if (car.nitroActive) {
      car.nitroBurn -= dt;
      car.nitro = Math.max(0, car.nitro - dt / NITRO_FULL_BURN);
      topSpeed *= NITRO_TOP;
      accel += 20 * st.nitroPower;
    } else car.nitroBurn = 0;

    const steer = clamp(input.steer || 0, -1, 1);
    const throttle = car.airborne ? 0 : clamp(input.throttle == null ? 1 : input.throttle, 0, 1);
    let vF = forwardSpeed(car);

    car.drifting = false; car.braking = false;
    if (input.drift && !car.airborne) {
      if (Math.abs(steer) > 0.2 && vF > 12) car.drifting = true; else car.braking = true;
    }

    if (!car.airborne) {
      // Turn: slower at a crawl, heavier near top speed, sharper in a drift.
      const speedFactor = clamp(vF / 10, 0, 1) * (1 - 0.35 * clamp(vF / st.topSpeed, 0, 1));
      let turn = steer * st.handling * 2.4 * speedFactor;
      if (car.drifting) turn *= 1.6;
      car.heading = wrapAngle(car.heading + turn * dt);

      // Re-read the velocity against the new heading: in a drift the car turns faster than its velocity does.
      const fx = Math.cos(car.heading), fz = Math.sin(car.heading);
      const rx = -fz, rz = fx;
      vF = car.vx * fx + car.vz * fz;
      let vR = car.vx * rx + car.vz * rz;

      // Grip pulls the sideways velocity back into line; in a drift most of what is lost sideways is kept as speed.
      const grip = car.drifting ? st.driftGrip : GRIP;
      const kept = vR * Math.exp(-grip * dt);
      const lost = Math.abs(vR) - Math.abs(kept);
      vR = kept;
      vF += lost * (car.drifting ? 0.85 : 0.35);

      // Throttle against drag (settles at topSpeed), coasting, brakes.
      const ratio = vF / topSpeed;
      vF += accel * throttle * (1 - ratio * Math.abs(ratio)) * dt;
      if (vF > topSpeed) vF -= (vF - topSpeed) * 2 * dt;   // over the ceiling (nitro just ended): bleed off fast
      if (throttle < 0.05) vF -= vF * 0.4 * dt;
      if (car.braking) vF = Math.max(-6, vF - 28 * dt);
      if (car.drifting) car.nitro = Math.min(1, car.nitro + dt * 0.14 * st.nitroGain);

      car.vx = vF * fx + vR * rx;
      car.vz = vF * fz + vR * rz;
    } else {
      car.y += car.vy * dt;
      car.vy -= GRAVITY * dt;
      if (car.y <= 0) {
        car.y = 0; car.vy = 0; car.airborne = false; car.landed = true;
        car.nitro = Math.min(1, car.nitro + 0.2 * st.nitroGain);
      }
    }

    car.x += car.vx * dt;
    car.z += car.vz * dt;

    // Where on the track is the car now?
    const n = track.nearest(car.x, car.z, car.hint);
    car.hint = n.i; car.s = n.s; car.lateral = n.lateral; car.roadWidth = n.w;
    car.curvature = n.curvature; car.tunnel = n.tunnel;
    if (!car.airborne) car.y = n.rampHeight || 0;

    // Over a ramp lip at speed: fly.
    if (!car.airborne) {
      for (const lip of track.ramps) {
        let before = lip - car.prevS; if (before < -track.length / 2) before += track.length;
        let after = n.s - lip; if (after < -track.length / 2) after += track.length;
        if (before >= 0 && before < 40 && after >= 0 && after < 40 && vF > 8) {
          car.airborne = true; car.vy = vF * RAMP_LAUNCH; car.y = Math.max(car.y, track.RAMP_HEIGHT);
        }
      }
    }

    // Walls along both edges of the road.
    const half = n.w / 2 - CAR_HALF_WIDTH;
    if (Math.abs(n.lateral) > half) {
      const side = n.lateral > 0 ? 1 : -1;
      const wx = -n.dz * side, wz = n.dx * side;   // unit normal pointing into the wall
      const over = Math.abs(n.lateral) - half;
      car.x -= wx * over; car.z -= wz * over;
      car.lateral = half * side;
      const vn = car.vx * wx + car.vz * wz;
      if (vn > WRECK_WALL_SPEED) wreck(car);
      else if (vn > 0) {
        car.vx -= vn * wx * 1.15; car.vz -= vn * wz * 1.15;
        const slow = 1 - 0.6 * clamp(vn / WRECK_WALL_SPEED, 0, 1);
        car.vx *= slow; car.vz *= slow;
      }
      if (!car.wrecked && Math.hypot(car.vx, car.vz) > 4) {
        car.sparks = true;
        car.vx *= 1 - 0.5 * dt; car.vz *= 1 - 0.5 * dt;   // grinding along the wall costs speed
      }
    }

    car.speed = Math.hypot(car.vx, car.vz);
    return car;
  }

  function collide(a, b) {
    if (a.wrecked || b.wrecked) return false;
    let dx = b.x - a.x, dz = b.z - a.z;
    let d = Math.hypot(dx, dz);
    const minD = CAR_RADIUS * 2;
    if (d >= minD) return false;
    if (d < 1e-6) { dx = 1; dz = 0; d = 1e-6; }
    const nx = dx / d, nz = dz / d;
    const over = minD - d;
    a.x -= nx * over / 2; a.z -= nz * over / 2;
    b.x += nx * over / 2; b.z += nz * over / 2;
    const vrel = (b.vx - a.vx) * nx + (b.vz - a.vz) * nz;   // negative = closing
    if (vrel < 0) {
      const closing = -vrel;
      const sa = Math.hypot(a.vx, a.vz), sb = Math.hypot(b.vx, b.vz);   // who was slower before the hit
      const j = closing * 1.3 / 2;
      a.vx -= j * nx; a.vz -= j * nz;
      b.vx += j * nx; b.vz += j * nz;
      a.bumped = b.bumped = true;
      if (closing > WRECK_RAM_SPEED) wreck(sa < sb ? a : b);
    }
    return true;
  }

  function nearMiss(a, b) {
    if (a.wrecked || b.wrecked) return false;
    const d = Math.hypot(b.x - a.x, b.z - a.z);
    if (d < CAR_RADIUS * 2 || d > 5) return false;
    return Math.hypot(b.vx - a.vx, b.vz - a.vz) > 6;
  }

  return {
    CARS, ORDER, createCar, step, collide, nearMiss, wreck, forwardSpeed,
    CAR_RADIUS, WRECK_TIME, NITRO_BURN,
  };
});
