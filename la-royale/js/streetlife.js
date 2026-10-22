/*
 * Street life: parked cars along the kerbs (and the abandoned ones on the
 * freeways), from W.parked. Called from render.js; returns an object with
 * update(dt, player, night) for anything that moves.
 */

import * as THREE from '../vendor/three.module.min.js';

export function buildStreetLife(W, scene, ctx) {
  const { batch } = ctx;
  const body = new THREE.BoxGeometry(1, 1, 1);
  batch(W.parked.map((p) => ({ x: p.x, z: p.z, py: p.y + p.hgt / 2, sx: p.len, sy: p.hgt, sz: p.wid, ry: p.ry, c: p.color })), body,
    new THREE.MeshStandardMaterial({ roughness: 0.35, metalness: 0.4 }), { far: 900 });
  return { update() {} };
}
