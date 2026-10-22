/*
 * Trees and plants: the palms, street trees and chaparral of Los Angeles.
 * Called from render.js with its batching helpers (ctx).
 */

import * as THREE from '../vendor/three.module.min.js';

export function buildFlora(W, scene, ctx) {
  const { batch, lo, hi, fadeNearCamera, mulberry, shadeHex } = ctx;
  const leaf = makeLeafTexture();
  const frondTex = makeFrondTexture();
  const palmTrunk = new THREE.CylinderGeometry(0.2, 0.36, 1, 7, 12); palmTrunk.translate(0, 0.5, 0);
  ringTrunk(palmTrunk);
  const fronds = makeFronds();
  const trunk = new THREE.CylinderGeometry(0.22, 0.42, 1, 7); trunk.translate(0, 0.5, 0);
  const canopy = new THREE.SphereGeometry(1, 14, 10);
  const canopyCore = new THREE.IcosahedronGeometry(0.82, 1);
  const scrub = new THREE.IcosahedronGeometry(1, 1);
  const cypress = new THREE.SphereGeometry(1, 10, 10);
  const T = { palmTrunk: [], fronds: [], trunk: [], canopy: [], scrub: [], cypress: [] };
  const rnd = mulberry(7);
  const greens = ['#5d8a3f', '#6b9645', '#4f7a38', '#7a9a4c'];
  for (const t of W.trees) {
    const s = t.s;
    if (t.t === 0) {
      const lean = (rnd() - 0.5) * 0.14;
      T.palmTrunk.push({ x: t.x, z: t.z, py: t.y - 0.3, sx: s, sy: t.h, sz: s, rx: lean, ry: t.r, c: '#9a8468' });
      T.fronds.push({ x: t.x, z: t.z, px: t.x + Math.sin(t.r) * Math.sin(lean) * t.h, pz: t.z + Math.cos(t.r) * Math.sin(lean) * t.h, py: t.y + t.h - 0.3, sx: s * 1.25, sy: s * 1.25, sz: s * 1.25, ry: t.r, c: greens[(t.r * 10 | 0) % 4] });
    } else if (t.t === 1 || t.t === 2 || t.t === 5) {
      const r = t.t === 5 ? t.h * 0.55 : t.h * 0.4;
      T.trunk.push({ x: t.x, z: t.z, py: t.y - 0.2, sx: s, sy: t.h * 0.6, sz: s, c: '#6b5440' });
      T.canopy.push({ x: t.x, z: t.z, py: t.y + t.h * 0.64, sx: r * s, sy: r * 0.8 * s, sz: r * s, ry: t.r, c: t.t === 2 ? '#a68ae0' : t.t === 5 ? '#55703a' : greens[(t.r * 10 | 0) % 4] });
    } else if (t.t === 3) {
      T.scrub.push({ x: t.x, z: t.z, py: t.y + t.h * 0.22, sx: t.h * s * 0.9, sy: t.h * 0.55, sz: t.h * s * 0.8, ry: t.r, c: rnd() < 0.5 ? '#66713f' : '#7a7a4c' });
    } else if (t.t === 4) {
      T.cypress.push({ x: t.x, z: t.z, py: t.y + t.h * 0.5, sx: 1.4 * s, sy: t.h * 0.5, sz: 1.4 * s, c: '#35532f' });
    }
  }
  const barkMat = fadeNearCamera(new THREE.MeshStandardMaterial({ roughness: 0.95 }));
  const leafMat = fadeNearCamera(new THREE.MeshStandardMaterial({ map: leaf, alphaTest: 0.45, roughness: 0.85, side: THREE.DoubleSide }));
  const leafCore = fadeNearCamera(new THREE.MeshStandardMaterial({ roughness: 0.9 }));
  const frondMat = fadeNearCamera(new THREE.MeshStandardMaterial({ map: frondTex, alphaTest: 0.4, roughness: 0.8, side: THREE.DoubleSide }));
  const far = lo ? 900 : hi ? 2000 : 1600;
  batch(T.palmTrunk, palmTrunk, barkMat, { far, receive: false });
  batch(T.fronds, fronds, frondMat, { far, receive: false });
  batch(T.trunk, trunk, barkMat, { far: far * 0.8, receive: false });
  batch(T.canopy.map((c) => ({ ...c, c: shadeHex(c.c, 0.6) })), canopyCore, leafCore, { far: far * 0.9, receive: false });
  batch(T.canopy, canopy, leafMat, { far, receive: false });
  batch(T.scrub, scrub, leafMat, { far: far * 0.7, cast: false, receive: false });
  batch(T.cypress, cypress, leafMat, { far, receive: false });

}

/* Clusters of leaves on a transparent background, for canopies. */
function makeLeafTexture() {
  const S = 256;
  const c = document.createElement('canvas');
  c.width = c.height = S;
  const ctx = c.getContext('2d');
  const rnd = mulberry(11);
  for (let k = 0; k < 900; k++) {
    const x = rnd() * S, y = rnd() * S, r = 4 + rnd() * 9, a = rnd() * Math.PI;
    const l = 0.55 + rnd() * 0.45;
    ctx.fillStyle = `rgb(${(210 * l) | 0},${(225 * l) | 0},${(190 * l) | 0})`;
    ctx.beginPath(); ctx.ellipse(x, y, r, r * 0.45, a, 0, Math.PI * 2); ctx.fill();
  }
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  t.repeat.set(3, 2);
  return t;
}

/* One palm frond: a rib with leaflets fanning off it, on transparency. */
function makeFrondTexture() {
  const c = document.createElement('canvas');
  c.width = 64; c.height = 256;
  const ctx = c.getContext('2d');
  ctx.strokeStyle = '#d9e3c4'; ctx.lineCap = 'round';
  for (let y = 6; y < 250; y += 5) {
    const t = y / 256, len = 30 * Math.sin(Math.PI * Math.min(1, t * 1.05 + 0.05));
    ctx.lineWidth = 2.6;
    for (const s of [-1, 1]) { ctx.beginPath(); ctx.moveTo(32, y); ctx.lineTo(32 + s * len, y + 10 + len * 0.3); ctx.stroke(); }
  }
  ctx.strokeStyle = '#b8c49a'; ctx.lineWidth = 3; ctx.beginPath(); ctx.moveTo(32, 0); ctx.lineTo(32, 256); ctx.stroke();
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  return t;
}

/* A palm crown: thirteen drooping fronds, each a curved strip with UVs. */
function makeFronds() {
  const pos = [], uv = [], index = [];
  const blades = 13;
  for (let b = 0; b < blades; b++) {
    const a = (b / blades) * Math.PI * 2 + (b % 2) * 0.25;
    const ca = Math.cos(a), sa = Math.sin(a);
    const segs = 6, len = 4.4 + (b % 3) * 0.6, droop = b % 4 === 0 ? 1.4 : 2.4;
    const base = pos.length / 3;
    for (let s = 0; s <= segs; s++) {
      const t = s / segs, r = t * len;
      const y = 1.1 * t - droop * t * t;
      const px = ca * r, pz = sa * r, lx = -sa, lz = ca;
      const twist = 0.25 * t;
      pos.push(px + lx, y + twist, pz + lz, px - lx, y - twist, pz - lz);
      uv.push(0, 1 - t, 1, 1 - t);
      if (s > 0) { const k = base + s * 2; index.push(k - 2, k - 1, k, k - 1, k + 1, k); }
    }
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  g.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2));
  g.setIndex(index);
  g.computeVertexNormals();
  return g;
}

/* Growth rings round a palm trunk. */
function ringTrunk(g) {
  const p = g.attributes.position;
  for (let i = 0; i < p.count; i++) {
    const k = 1 + 0.08 * Math.sin(p.getY(i) * 40);
    p.setX(i, p.getX(i) * k); p.setZ(i, p.getZ(i) * k);
  }
  g.computeVertexNormals();
}
