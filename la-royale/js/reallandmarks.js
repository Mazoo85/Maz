/*
 * The landmarks of the true-scale map, modelled.
 *
 * OpenStreetMap gives every building its real outline and height, but a
 * landmark is more than its box: Capitol Records' stack of sunshades and
 * spire, City Hall's stepped pyramid, Griffith Observatory's copper domes,
 * Disney Hall's steel sails, the letters of the Hollywood Sign. Each entry
 * here finds its building in the real data (by name or position), takes
 * its real footprint and height, and either dresses it (a crown, a spire,
 * its real colour) or replaces it with a model built to its real size.
 *
 * Positions are real (lat, lon); sizes are the real ones, in metres.
 */

import * as THREE from '../vendor/three.module.min.js';
import { mergeGeometries } from '../vendor/jsm/utils/BufferGeometryUtils.js';

/* ------------------------------------------------------------ materials */

let MATS = null;
export function landmarkMaterials(withSurface) {
  if (MATS) return MATS;
  const std = (o, kind) => {
    const m = new THREE.MeshStandardMaterial({ roughness: 0.8, metalness: 0, ...o });
    return kind !== undefined ? withSurface(m, String(kind)) : m;
  };
  MATS = {
    white: std({ color: 0xece8de }, 1),          // smooth plaster / painted concrete
    cream: std({ color: 0xe2d6bc }, 1),
    stone: std({ color: 0xd7cfbd }, 4),
    concrete: std({ color: 0xc9c5bb }, 4),
    tile: std({ color: 0xb35d3a, roughness: 0.7 }, 5),
    copper: std({ color: 0x5f7f6c, metalness: 0.55, roughness: 0.45 }),    // weathered copper domes
    steel: std({ color: 0xdfe3e6, metalness: 0.95, roughness: 0.22 }),     // Disney Hall's brushed steel
    glass: std({ color: 0x223442, metalness: 0.7, roughness: 0.08 }),
    dark: std({ color: 0x2b2d30, roughness: 0.6 }),
    red: std({ color: 0x9b2a22, roughness: 0.6 }),
    bronze: std({ color: 0x4f5d47, metalness: 0.5, roughness: 0.5 }),
    gold: std({ color: 0xc9a648, metalness: 0.8, roughness: 0.35 }),
    grass: std({ color: 0x4f7f35, roughness: 0.95 }, 10),
    dirt: std({ color: 0xa8714a, roughness: 1 }, 13),
    sign: std({ color: 0xf5f4ef, roughness: 0.6, side: THREE.DoubleSide }),
    beacon: new THREE.MeshBasicMaterial({ color: 0xff2a1a, toneMapped: false }),
    lamp: new THREE.MeshStandardMaterial({ color: 0xffffff, emissive: 0xfff2d8, emissiveIntensity: 0.2 }),
    seatA: std({ color: 0xe8c547 }), seatB: std({ color: 0xd9823b }), seatC: std({ color: 0x3fa7b5 }), seatD: std({ color: 0x2d5aa6 }),
    orange: std({ color: 0xd9591f, roughness: 0.55 })
  };
  return MATS;
}

/* ------------------------------------------------------------- helpers */

/* Pieces collected per material, merged into one mesh each at the end. */
export class Kit {
  constructor() { this.parts = new Map(); this.colliders = []; }
  add(geo, mat, x = 0, y = 0, z = 0, ry = 0, rx = 0, rz = 0) {
    const g = geo.index ? geo.toNonIndexed() : geo.clone();
    g.deleteAttribute('uv');
    const m = new THREE.Matrix4().compose(new THREE.Vector3(x, y, z), new THREE.Quaternion().setFromEuler(new THREE.Euler(rx, ry, rz, 'YXZ')), new THREE.Vector3(1, 1, 1));
    g.applyMatrix4(m);
    (this.parts.get(mat) || this.parts.set(mat, []).get(mat)).push(g);
    return this;
  }
  box(w, h, d, mat, x, y, z, ry = 0) { return this.add(new THREE.BoxGeometry(w, h, d), mat, x, y + h / 2, z, ry); }
  cyl(r0, r1, h, mat, x, y, z, seg = 32) { return this.add(new THREE.CylinderGeometry(r1, r0, h, seg), mat, x, y + h / 2, z); }
  solidBox(w, h, d, mat, x, y, z, ry = 0) {
    this.box(w, h, d, mat, x, y, z, ry);
    this.colliders.push({ k: 0, x, z, hx: w / 2, hz: d / 2, c: Math.cos(ry), s: Math.sin(ry), y0: y, y1: y + h, r: Math.hypot(w, d) / 2 });
    return this;
  }
  solidCyl(r, h, mat, x, y, z, seg = 32) {
    this.cyl(r, r, h, mat, x, y, z, seg);
    this.colliders.push({ k: 1, x, z, r, y0: y, y1: y + h });
    return this;
  }
  object() {
    const g = new THREE.Group();
    for (const [mat, list] of this.parts) {
      const mesh = new THREE.Mesh(mergeGeometries(list, false), mat);
      mesh.castShadow = mesh.receiveShadow = true;
      g.add(mesh);
    }
    return g;
  }
}

/* A building record's centroid and its smallest rectangle. */
function centre(pts) { let x = 0, z = 0; for (const [a, b] of pts) { x += a; z += b; } return [x / pts.length, z / pts.length]; }

/* A dome: the top half of a sphere. */
const dome = (r, seg = 32) => new THREE.SphereGeometry(r, seg, Math.max(8, seg / 2), 0, Math.PI * 2, 0, Math.PI / 2);

/* The letters of a sign, white on transparent. */
function letterTexture(ch) {
  const c = document.createElement('canvas');
  c.width = 256; c.height = 300;
  const ctx = c.getContext('2d');
  ctx.fillStyle = '#fff';
  ctx.font = 'bold 300px "Arial Narrow", Helvetica, Arial, sans-serif';
  ctx.textAlign = 'center'; ctx.textBaseline = 'alphabetic';
  ctx.save(); ctx.translate(128, 282); ctx.scale(0.86, 1.0); ctx.fillText(ch, 0, 0); ctx.restore();
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  t.anisotropy = 8;
  return t;
}

/* The LA Metro logo: a white M on the dark circle. */
let METRO_TEX = null;
function metroTex() {
  if (METRO_TEX) return METRO_TEX;
  const c = document.createElement('canvas');
  c.width = c.height = 128;
  const ctx = c.getContext('2d');
  ctx.fillStyle = '#2b2b2b'; ctx.fillRect(0, 0, 128, 128);
  ctx.fillStyle = '#e8e8e8'; ctx.beginPath(); ctx.arc(64, 64, 52, 0, 6.283); ctx.fill();
  ctx.fillStyle = '#2b2b2b'; ctx.font = 'bold 74px Helvetica, Arial, sans-serif'; ctx.textAlign = 'center'; ctx.textBaseline = 'middle'; ctx.fillText('M', 64, 68);
  METRO_TEX = new THREE.CanvasTexture(c);
  METRO_TEX.colorSpace = THREE.SRGBColorSpace;
  return METRO_TEX;
}

/* An LA Metro subway entrance: the stair down under a glass canopy, and the pylon with the M. */
function metroPortal(name, lat, lon, deg) {
  return {
    name, at: [lat, lon], metro: true,
    build({ x, z, gh, M }) {
      const k = new Kit(), a = THREE.MathUtils.degToRad(deg), ux = Math.cos(a), uz = -Math.sin(a), y = gh(x, z);
      k.box(9, 0.3, 3.6, M.dark, x, y - 0.2, z, a);                       // the stairwell, dark below
      for (const s of [-1, 1]) k.box(9, 1.0, 0.25, M.concrete, x - uz * s * 1.9, y, z + ux * s * 1.9, a);
      k.box(10.5, 0.15, 5.2, M.glass, x, y + 3.6, z, a);                   // the canopy
      for (const [u, v] of [[-5, -2.4], [5, -2.4], [-5, 2.4], [5, 2.4]]) k.box(0.18, 3.6, 0.18, M.dark, x + ux * u - uz * v, y, z + uz * u + ux * v, a);
      const px = x + ux * 6.5, pz = z + uz * 6.5;
      k.box(0.9, 5.4, 0.9, M.dark, px, gh(px, pz), pz, a);
      const logo = new THREE.MeshStandardMaterial({ map: metroTex(), roughness: 0.4, emissive: 0xffffff, emissiveMap: metroTex(), emissiveIntensity: 0.25 });
      const g = new THREE.Group();
      for (let f = 0; f < 4; f++) {
        const p = new THREE.Mesh(new THREE.PlaneGeometry(0.85, 0.85), logo);
        const fa = a + (f * Math.PI) / 2;
        p.position.set(px + Math.sin(fa) * 0.46, gh(px, pz) + 4.7, pz + Math.cos(fa) * 0.46);
        p.rotation.y = fa;
        g.add(p);
      }
      k.extra = g;
      return k;
    }
  };
}

/* ----------------------------------------------------------- landmarks
 *
 * Each: { name, at: [lat, lon], find: (b) → is this OSM building it?,
 *         replace: drop the matching buildings and build in their place?,
 *         style: colours for the kept building, build(ctx) → Kit }
 * ctx: { x, z (anchor, world), host (the matched building with the most
 *        height: { pts, h, base, cx, cz, R }), gh, rect: minRect, M (materials) }
 */

export const REAL_LANDMARKS = [
  {
    name: 'Hollywood Sign', at: [34.13412, -118.32166],
    build({ x, z, gh, M }) {
      // nine letters 13.7 m tall across 107 m of Mount Lee's south face, on steel frames
      const k = new Kit(), letters = 'HOLLYWOOD', pitch = 11.9, h = 13.7, face = new THREE.Group();
      const ang = THREE.MathUtils.degToRad(-8);           // the line of letters runs a little south of east
      const ux = Math.cos(ang), uz = -Math.sin(ang);
      for (let i = 0; i < letters.length; i++) {
        const a = (i - (letters.length - 1) / 2) * pitch;
        const lx = x + ux * a, lz = z + uz * a, y = gh(lx, lz) - 1.5;
        const mat = new THREE.MeshStandardMaterial({ map: letterTexture(letters[i]), alphaTest: 0.5, roughness: 0.6, side: THREE.DoubleSide });
        const plane = new THREE.Mesh(new THREE.PlaneGeometry(10.6, h + 0.6), mat);
        plane.position.set(lx, y + h / 2 + 0.3, lz);
        plane.rotation.set(-0.12, -ang, 0, 'YXZ');            // facing south, leaning back into the hill
        plane.castShadow = true;
        face.add(plane);
        // the frame behind: posts and braces
        for (const s of [-3.5, 0, 3.5]) {
          const px = lx + ux * s - Math.sin(-ang) * 1.6, pz = lz + uz * s - Math.cos(-ang) * 1.6;
          k.add(new THREE.CylinderGeometry(0.12, 0.12, h + 1, 6), M.dark, px, gh(px, pz) + h / 2 - 1, pz);
        }
        k.add(new THREE.BoxGeometry(10, 0.15, 0.15), M.dark, lx - Math.sin(-ang) * 1.6, y + h * 0.75, lz - Math.cos(-ang) * 1.6, -ang);
        k.add(new THREE.BoxGeometry(10, 0.15, 0.15), M.dark, lx - Math.sin(-ang) * 1.6, y + h * 0.3, lz - Math.cos(-ang) * 1.6, -ang);
      }
      k.extra = face;
      return k;
    }
  },
  {
    name: 'Griffith Observatory', at: [34.11824, -118.30037], find: (b) => /Griffith Observatory/.test(b.name || ''), replace: true,
    build({ host, gh, M }) {
      // white Art Deco, east–west, front to the south lawn: the planetarium dome in the middle,
      // a smaller telescope dome on each wing
      const k = new Kit(), [cx, cz] = host ? [host.cx, host.cz] : [0, 0], y = host ? host.base : 0;
      k.solidBox(58, 11, 17, M.white, cx, y, cz + 3);
      k.solidBox(30, 10, 22, M.white, cx, y, cz - 14);
      k.cyl(12.6, 12.6, 9, M.white, cx, y + 11, cz + 1, 48);
      k.add(dome(12.8, 48), M.copper, cx, y + 20, cz + 1);
      k.add(new THREE.CylinderGeometry(0.6, 0.9, 2.2, 12), M.copper, cx, y + 33.8, cz + 1);
      for (const s of [-1, 1]) {
        k.solidBox(10, 15, 10, M.white, cx + s * 27, y, cz + 3);
        k.cyl(4.6, 4.6, 2, M.white, cx + s * 27, y + 15, cz + 3, 32);
        k.add(dome(4.8, 32), M.copper, cx + s * 27, y + 17, cz + 3);
      }
      // the colonnade across the front and the Astronomers Monument on the lawn
      for (let i = -5; i <= 5; i++) k.cyl(0.45, 0.45, 8, M.white, cx + i * 3.6, y, cz + 12.5, 12);
      k.box(42, 1, 3, M.white, cx, y + 8, cz + 12.5);
      const mz = cz + 48;
      k.solidBox(2.2, 12, 2.2, M.white, cx, gh(cx, mz), mz);
      return k;
    }
  },
  {
    name: 'Capitol Records Building', at: [34.10320, -118.32620], find: (b) => b.h > 40, near: 30, replace: true,
    build({ host, M }) {
      // a stack of thirteen floors, each shaded by a round overhang, and the spire with its red light
      // (it blinks "Hollywood" in Morse code)
      const k = new Kit(), [cx, cz] = [host.cx, host.cz], y = host.base, r = Math.min(host.R.L, host.R.Wd) * 0.88, fl = 3.25;
      k.solidCyl(r * 0.92, 13 * fl, M.glass, cx, y, cz, 40);
      for (let i = 1; i <= 13; i++) k.cyl(r * (1.06 - i * 0.004), r * (1.06 - i * 0.004), 0.28, M.white, cx, y + i * fl - 0.3, cz, 48);
      k.cyl(r * 0.6, r * 0.45, 4, M.white, cx, y + 13 * fl, cz, 32);
      k.add(new THREE.CylinderGeometry(0.15, 0.55, 28, 8), M.white, cx, y + 13 * fl + 4 + 14, cz);
      k.add(new THREE.SphereGeometry(0.5, 12, 8), M.beacon, cx, y + 13 * fl + 32.3, cz);
      k.beacon = [cx, y + 13 * fl + 32.3, cz];
      return k;
    }
  },
  {
    name: 'Los Angeles City Hall', at: [34.05374, -118.24277], find: (b) => b.h > 130, near: 40,
    style: { wall: '#e5dfcf', win: 1 },
    build({ host, M, rect }) {
      // the tower's stepped pyramid roof (after the Mausoleum at Halicarnassus) and its lantern
      const k = new Kit(), R = rect(host.pts), top = host.base + host.h, ry = -Math.atan2(R.uz, R.ux);
      let w = Math.min(R.L, R.Wd) * 2;
      for (let i = 0; i < 6; i++) { k.box(w, 1.6, w, M.cream, R.cx, top + i * 1.6, R.cz, ry); w *= 0.8; }
      k.add(new THREE.ConeGeometry(w * 0.75, 4.5, 4), M.cream, R.cx, top + 9.6 + 2.25, R.cz, ry + Math.PI / 4);
      k.add(new THREE.SphereGeometry(0.6, 12, 8), M.beacon, R.cx, top + 14.4, R.cz);
      return k;
    }
  },
  {
    name: 'US Bank Tower', at: [34.05110, -118.25410], find: (b) => b.h > 300, near: 50,
    build({ host, M }) {
      // the glass crown, lit at night
      const k = new Kit(), top = host.base + host.h, r = Math.sqrt(Math.abs(host.area) / Math.PI) * 0.92;
      k.cyl(r, r * 0.97, 9, M.glass, host.cx, top, host.cz, 40);
      k.cyl(r * 1.01, r * 1.01, 1.2, M.lamp, host.cx, top + 8.4, host.cz, 40);
      k.glow = true;
      return k;
    }
  },
  {
    name: 'Wilshire Grand Center', at: [34.05020, -118.26010], find: (b) => b.h > 250, near: 50,
    style: { win: 3 },
    build({ host, M }) {
      // the sail-shaped crown and the spire to 335 m, the tallest point west of the Mississippi
      const k = new Kit(), top = host.base + host.h;
      k.add(new THREE.CylinderGeometry(0.4, 1.6, 335 - host.h, 12), M.steel, host.cx, top + (335 - host.h) / 2, host.cz);
      k.add(new THREE.SphereGeometry(0.6, 12, 8), M.beacon, host.cx, host.base + 335.3, host.cz);
      k.cyl(1.6, 1.6, 1, M.lamp, host.cx, top + 2, host.cz, 16);
      return k;
    }
  },
  {
    name: 'Walt Disney Concert Hall', at: [34.05563, -118.24959], find: (b) => /Disney Concert/.test(b.name || ''), replace: true,
    build({ host, M, rect }) {
      // Gehry's stainless-steel sails over a stone podium
      const k = new Kit(), R = rect(host.pts), ry = -Math.atan2(R.uz, R.ux), y = host.base;
      const L = R.L, W = R.Wd;
      const P = (u, v) => [R.cx + R.ux * u - R.uz * v, R.cz + R.uz * u + R.ux * v];
      k.solidBox(L * 1.9, 9, W * 1.7, M.stone, R.cx, y, R.cz, ry);
      const sails = [[-0.45, 0.1, 26, 30, 0.2, 1.9], [-0.15, -0.25, 22, 34, 1.4, 1.6], [0.12, 0.2, 24, 32, 2.6, 1.7], [0.4, -0.15, 18, 26, 3.6, 1.5],
        [-0.3, 0.45, 16, 22, 4.6, 1.4], [0.25, 0.5, 15, 20, 5.4, 1.3], [0.55, 0.3, 14, 18, 0.9, 1.2], [-0.6, -0.35, 15, 20, 2.1, 1.3]];
      for (const [u, v, rad, hgt, rot, arc] of sails) {
        const [sx, sz] = P(u * L, v * W);
        const g = new THREE.CylinderGeometry(rad, rad * 0.78, hgt, 28, 1, true, 0, arc);
        k.add(g, M.steel, sx, y + 9 + hgt / 2 - 1, sz, ry + rot, 0.12 * Math.sin(rot * 3), 0.1 * Math.cos(rot * 2));
      }
      k.colliders.push({ k: 2, pts: host.pts, x: host.cx, z: host.cz, r: Math.hypot(L, W), y0: y, y1: y + host.h });
      return k;
    }
  },
  {
    name: 'Cinerama Dome', at: [34.09767, -118.32802], find: (b) => b.roof === 4, near: 30, replace: true,
    build({ host, M }) {
      // Buckminster Fuller's geodesic dome, 316 concrete panels
      const k = new Kit(), r = Math.min(host.R.L, host.R.Wd) * 0.95;
      const g = new THREE.IcosahedronGeometry(r, 2);
      const pos = g.attributes.position;
      for (let i = 0; i < pos.count; i++) if (pos.getY(i) < 0) pos.setY(i, 0);
      g.computeVertexNormals();
      k.add(g, M.concrete, host.cx, host.base, host.cz);
      k.colliders.push({ k: 1, x: host.cx, z: host.cz, r: r * 0.9, y0: host.base, y1: host.base + r * 0.8 });
      return k;
    }
  },
  {
    name: 'TCL Chinese Theatre', at: [34.10182, -118.34100], find: (b) => /Chinese Theat/i.test(b.name || ''),
    style: { wall: '#9d2b22', win: 0 },
    build({ host, M, rect }) {
      // the bronze pagoda over the forecourt, on its tall red columns, facing Hollywood Blvd (south)
      const k = new Kit(), R = rect(host.pts);
      let fx = 0, fz = 0, best = -Infinity;
      for (const [u, v] of [[R.L, 0], [-R.L, 0], [0, R.Wd], [0, -R.Wd]]) {         // the side facing south
        const x = R.cx + R.ux * u - R.uz * v, z = R.cz + R.uz * u + R.ux * v;
        if (z > best) { best = z; fx = x; fz = z; }
      }
      const dx = fx - R.cx, dz = fz - R.cz, dl = Math.hypot(dx, dz) || 1, nx = dx / dl, nz = dz / dl, ry = Math.atan2(nx, nz);
      const px = fx + nx * 6, pz = fz + nz * 6, y = host.base;
      for (const s of [-1, 1]) k.cyl(0.7, 0.7, 13, M.red, px + nz * s * 6, y, pz - nx * s * 6, 16);
      k.box(15, 1.4, 9, M.bronze, px, y + 13, pz, ry);
      for (let i = 0; i < 3; i++) {
        const g = new THREE.ConeGeometry(10 - i * 2.6, 3.4, 4, 1, true);
        g.scale(1, 1, 0.62);
        k.add(g, M.bronze, px, y + 15.8 + i * 2.6, pz, ry + Math.PI / 4);
      }
      k.add(new THREE.CylinderGeometry(0.2, 0.5, 4, 8), M.gold, px, y + 24.5, pz);
      return k;
    }
  },
  {
    name: 'Union Station', at: [34.05600, -118.23660], clear: 75,
    build({ x, z, gh, M }) {
      // Mission Revival along Alameda Street: long white wings under red tile, the clock tower at the entrance
      const k = new Kit(), y = gh(x, z), ry = 0;
      const roof = (w, d, h, cx, cy, cz) => { const g = new THREE.ConeGeometry(Math.hypot(w, d) / 2, h, 4); g.rotateY(Math.PI / 4); g.scale(w / Math.hypot(w, d) * Math.SQRT2, 1, d / Math.hypot(w, d) * Math.SQRT2); k.add(g, M.tile, cx, cy + h / 2, cz); };
      k.solidBox(26, 17, 60, M.white, x, y, z, ry); roof(27, 61, 6, x, y + 17, z);
      for (const s of [-1, 1]) { k.solidBox(20, 10, 70, M.white, x - 2, y, z + s * 65, ry); roof(21, 71, 4, x - 2, y + 10, z + s * 65); }
      k.solidBox(10, 38, 10, M.white, x - 10, y, z - 22, ry); roof(11, 11, 5, x - 10, y + 38, z - 22);
      for (const [ox, oz, r] of [[5.06, 0, Math.PI / 2], [-5.06, 0, -Math.PI / 2], [0, 5.06, 0], [0, -5.06, Math.PI]]) k.add(new THREE.CircleGeometry(1.8, 24), M.cream, x - 10 + ox, y + 31, z - 22 + oz, r);
      return k;
    }
  },
  {
    name: 'Hollywood Bowl', at: [34.11276, -118.33890], clear: 25,
    build({ x, z, gh, M }) {
      // the band shell's nested white arches, the seats climbing the canyon (uphill, wherever that is)
      const k = new Kit(), e = 25, y0 = gh(x, z);
      const up = [gh(x + e, z) - gh(x - e, z), gh(x, z + e) - gh(x, z - e)], ul = Math.hypot(...up) || 1, ux = up[0] / ul, uz = up[1] / ul;
      const ry = Math.atan2(ux, uz);
      for (let i = 0; i < 5; i++) {
        const g = new THREE.TorusGeometry(15 - i * 1.6, 0.7, 8, 32, Math.PI);
        k.add(g, M.white, x - ux * i * 2.2, y0, z - uz * i * 2.2, ry + Math.PI / 2, 0, 0);
      }
      k.add(new THREE.CircleGeometry(15, 32, 0, Math.PI), M.concrete, x, y0 + 0.4, z, ry + Math.PI / 2, -Math.PI / 2);
      // seating: 17,500 seats in a fan of terraces
      for (let row = 0; row < 26; row++) {
        const rad = 30 + row * 4.4, a = 1.05;
        const g = new THREE.RingGeometry(rad, rad + 3.2, 40, 1, -a / 2, a);
        g.rotateX(-Math.PI / 2);
        const cxr = x + ux * 0, czr = z;
        const yy = gh(cxr + ux * (rad + 1.6), czr + uz * (rad + 1.6)) + 0.6;
        k.add(g, row % 6 === 5 ? M.concrete : M.seatD, cxr, Math.max(yy, y0 + row * 1.1), czr, ry - Math.PI / 2);
      }
      return k;
    }
  },
  {
    name: 'Dodger Stadium', at: [34.07390, -118.24000], clear: 150,
    build({ x, z, gh, M }) {
      // the diamond (home plate to the south-west, the outfield toward the north-east) and five tiers of
      // pastel seats wrapped round the infield, the outfield pavilions and the light towers
      const k = new Kit(), y = gh(x, z) - 8, hx = -0.68, hz = 0.73;   // toward home plate
      const g = new THREE.CircleGeometry(150, 48); g.rotateX(-Math.PI / 2); k.add(g, M.grass, x, y, z);
      const homeX = x + hx * 60, homeZ = z + hz * 60, ry = Math.atan2(-hx, -hz);
      const inf = new THREE.PlaneGeometry(38, 38); inf.rotateX(-Math.PI / 2); k.add(inf, M.dirt, homeX - hx * 19, y + 0.05, homeZ - hz * 19, ry + Math.PI / 4);
      const grassIn = new THREE.PlaneGeometry(26, 26); grassIn.rotateX(-Math.PI / 2); k.add(grassIn, M.grass, homeX - hx * 19, y + 0.08, homeZ - hz * 19, ry + Math.PI / 4);
      const seats = [M.seatA, M.seatB, M.seatC, M.seatD, M.seatA];
      for (let t = 0; t < 5; t++) {
        const r0 = 52 + t * 15, a = 2.4;
        for (let s = 0; s < 4; s++) {
          const ring = new THREE.RingGeometry(r0 + s * 3, r0 + s * 3 + 3, 48, 1, -a / 2, a);
          ring.rotateX(-Math.PI / 2);
          k.add(ring, seats[t], homeX, y + t * 9 + s * 1.6 + 2, homeZ, ry + Math.PI / 2);
        }
        const wall = new THREE.CylinderGeometry(r0 + 12.5, r0 + 12.5, 3, 48, 1, true, ry + Math.PI - a / 2 + Math.PI / 2, a);
        k.add(wall, M.white, homeX, y + t * 9 + 9, homeZ);
      }
      for (const s of [-1, 1]) {
        const px = x - hx * 95 + hz * s * 60, pz = z - hz * 95 - hx * s * 60;
        k.solidBox(40, 7, 14, M.white, px, y, pz, ry + s * 0.6);
        for (let i = -2; i <= 2; i++) k.add(new THREE.ConeGeometry(4, 3, 4), M.white, px + Math.cos(ry + s * 0.6) * i * 8, y + 8.5, pz - Math.sin(ry + s * 0.6) * i * 8, ry + s * 0.6 + Math.PI / 4);
      }
      for (const a of [-1.2, -0.4, 0.4, 1.2]) {
        const lx = homeX + Math.sin(ry + a + Math.PI) * 125, lz = homeZ + Math.cos(ry + a + Math.PI) * 125;
        k.add(new THREE.CylinderGeometry(0.6, 0.9, 50, 8), M.dark, lx, y + 25, lz);
        k.box(14, 6, 1, M.lamp, lx, y + 50, lz, ry + a);
      }
      return k;
    }
  },
  {
    name: 'The Broad', at: [34.05440, -118.25060], find: (b) => /The Broad/.test(b.name || ''),
    style: { wall: '#f3f1ea', win: 0 }
  },
  { name: 'The Wiltern', at: [34.06145, -118.30886], find: (b) => b.h > 50, near: 45, style: { wall: '#6fa596', win: 1 } },
  { name: 'Bradbury Building', at: [34.05050, -118.24780], find: (b) => /Bradbury/.test(b.name || ''), style: { wall: '#9a5a43', win: 1 } },
  { name: 'Pantages Theatre', at: [34.10223, -118.32578], find: (b) => /Pantages/.test(b.name || ''), style: { wall: '#d8c7a2', win: 0 } },
  { name: 'Egyptian Theatre', at: [34.10081, -118.33659], find: (b) => /Egyptian/.test(b.name || ''), style: { wall: '#cdb48a', win: 0 } },
  { name: 'El Capitan Theatre', at: [34.10116, -118.34010], find: (b) => /El Capitan/.test(b.name || ''), style: { wall: '#e8dcc0', win: 1 } },
  // the LA Metro B and D Line stations: an entrance at each
  metroPortal('Hollywood/Highland station', 34.10172, -118.33858, 0),
  metroPortal('Hollywood/Vine station', 34.10118, -118.32588, 90),
  metroPortal('Hollywood/Western station', 34.10138, -118.30858, 0),
  metroPortal('Vermont/Sunset station', 34.09812, -118.29188, 90),
  metroPortal('Vermont/Santa Monica station', 34.09068, -118.29188, 90),
  metroPortal('Vermont/Beverly station', 34.07662, -118.29188, 90),
  metroPortal('Wilshire/Vermont station', 34.06238, -118.28998, 0),
  metroPortal('Wilshire/Normandie station', 34.06212, -118.30068, 0),
  metroPortal('Wilshire/Western station', 34.06212, -118.30858, 0),
  metroPortal('Westlake/MacArthur Park station', 34.05665, -118.27582, 90),
  metroPortal('7th St/Metro Center station', 34.04862, -118.25898, 54),
  metroPortal('Pershing Square station', 34.04922, -118.25112, 54),
  metroPortal('Civic Center/Grand Park station', 34.05532, -118.24598, 54),
  metroPortal('Little Tokyo/Arts District station', 34.05002, -118.23802, 0),
  {
    name: 'Angels Flight', at: [34.05130, -118.25020],
    build({ x, z, gh, M }) {
      // the funicular up Bunker Hill from Hill Street: the orange arch at the bottom, two cars on the incline
      const k = new Kit(), ang = THREE.MathUtils.degToRad(-50), ux = Math.cos(ang), uz = -Math.sin(ang), len = 91;
      const y0 = gh(x, z), y1 = Math.max(gh(x + ux * len, z + uz * len), y0 + 27);
      const slope = Math.atan2(y1 - y0, len), ry = -ang;
      for (const s of [-1.2, 1.2]) {
        const g = new THREE.BoxGeometry(Math.hypot(len, y1 - y0), 0.3, 0.3);
        k.add(g, M.dark, x + ux * len / 2 - uz * s, (y0 + y1) / 2 + 0.3, z + uz * len / 2 + ux * s, ry, 0, slope);
      }
      for (const t of [0.3, 0.7]) {
        const cx = x + ux * len * t, cz = z + uz * len * t, cy = y0 + (y1 - y0) * t;
        k.add(new THREE.BoxGeometry(7, 3, 2.6), M.orange, cx, cy + 2, cz, ry, 0, slope);
      }
      k.box(0.6, 6, 0.6, M.orange, x - uz * 2.6, y0, z + ux * 2.6); k.box(0.6, 6, 0.6, M.orange, x + uz * 2.6, y0, z - ux * 2.6);
      k.box(0.8, 1.2, 6.2, M.orange, x, y0 + 6, z, ry);
      return k;
    }
  }
];
