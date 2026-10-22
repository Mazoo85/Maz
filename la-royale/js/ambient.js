/*
 * The city going about its day: traffic on the boulevards, jets landing over
 * Inglewood and taking off over the Pacific at LAX, and an LAPD helicopter
 * circling with its searchlight at night.
 *
 * None of it is solid except the traffic, which stops for you and which
 * your car bumps off (see push()).
 */

import * as THREE from '../vendor/three.module.min.js';
import { mergeGeometries } from '../vendor/jsm/utils/BufferGeometryUtils.js';

const TRAFFIC = 260;

export function makeAmbient(W, scene, opts = {}) {
  const lo = opts.quality === 'low';
  const rnd = mulberry(4242);

  /* ------------------------------------------------------------ traffic */
  const roads = W.roads.filter((r) => r.kind === 'blvd').map((r) => {
    const pts = r.pts, cum = [0];
    for (let i = 1; i < pts.length; i++) cum.push(cum[i - 1] + Math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]));
    return { pts, cum, len: cum[cum.length - 1] };
  });
  const total = roads.reduce((s, r) => s + r.len, 0);
  const cars = [];
  for (let i = 0; i < TRAFFIC; i++) {
    let pick = rnd() * total, ri = 0;
    while (pick > roads[ri].len) { pick -= roads[ri].len; ri++; }
    const dir = rnd() < 0.5 ? 1 : -1;
    cars.push({ r: roads[ri], s: pick, dir, lane: rnd() < 0.5 ? 2.4 : 6.2, speed: 0, cruise: 11 + rnd() * 4, x: 0, y: 0, z: 0, yaw: 0 });
  }
  const palette = ['#c0392b', '#f5f5f5', '#1d1d1f', '#2c3e50', '#bdc3c7', '#7f8c8d', '#2980b9', '#f1c40f', '#16a085', '#e8e2d0', '#8e2b2b', '#3c4a57'];
  const vc = (g, hex) => {
    const c = new THREE.Color(hex), ng = g.index ? g.toNonIndexed() : g, n = ng.attributes.position.count, a = new Float32Array(n * 3);
    for (let i = 0; i < n; i++) { a[i * 3] = c.r; a[i * 3 + 1] = c.g; a[i * 3 + 2] = c.b; }
    ng.setAttribute('color', new THREE.BufferAttribute(a, 3));
    return ng;
  };
  const B = (w, h, d, x, y, z, hex) => { const g = new THREE.BoxGeometry(w, h, d); g.translate(x, y, z); return vc(g, hex); };
  const wheel = (x, z) => { const g = new THREE.CylinderGeometry(0.36, 0.36, 0.28, 12); g.rotateX(Math.PI / 2); g.translate(x, 0.36, z); return vc(g, '#141414'); };
  const bodyGeo = mergeGeometries([
    B(4.5, 0.72, 1.84, 0, 0.78, 0, '#ffffff'),
    B(2.3, 0.62, 1.66, -0.25, 1.45, 0, '#1b232b'),
    B(2.1, 0.06, 1.7, -0.25, 1.78, 0, '#ffffff'),
    B(0.2, 0.2, 1.86, 2.26, 0.6, 0, '#9aa0a6'), B(0.2, 0.2, 1.86, -2.26, 0.6, 0, '#9aa0a6'),
    wheel(1.45, 0.86), wheel(1.45, -0.86), wheel(-1.45, 0.86), wheel(-1.45, -0.86)
  ]);
  const lightGeo = mergeGeometries([
    B(0.05, 0.16, 0.36, 2.27, 0.86, 0.62, '#fff6d5'), B(0.05, 0.16, 0.36, 2.27, 0.86, -0.62, '#fff6d5'),
    B(0.05, 0.16, 0.4, -2.27, 0.9, 0.62, '#ff2a2a'), B(0.05, 0.16, 0.4, -2.27, 0.9, -0.62, '#ff2a2a')
  ]);
  const bodyMat = new THREE.MeshStandardMaterial({ vertexColors: true, roughness: 0.3, metalness: 0.4 });
  const lightMat = new THREE.MeshStandardMaterial({ vertexColors: true, emissive: 0xffffff, emissiveIntensity: 0.4, roughness: 0.4 });
  const bodies = new THREE.InstancedMesh(bodyGeo, bodyMat, TRAFFIC);
  const lights = new THREE.InstancedMesh(lightGeo, lightMat, TRAFFIC);
  cars.forEach((c, i) => bodies.setColorAt(i, new THREE.Color(palette[i % palette.length])));
  bodies.castShadow = !lo;
  bodies.frustumCulled = false; lights.frustumCulled = false;
  scene.add(bodies, lights);

  const m4 = new THREE.Matrix4(), q = new THREE.Quaternion(), up = new THREE.Vector3(0, 1, 0), v = new THREE.Vector3(), one = new THREE.Vector3(1, 1, 1);
  function along(r, s) {
    s = Math.max(0, Math.min(r.len - 0.01, s));
    let i = 1; while (i < r.cum.length - 1 && r.cum[i] < s) i++;
    const a = r.pts[i - 1], b = r.pts[i], seg = r.cum[i] - r.cum[i - 1], t = (s - r.cum[i - 1]) / seg;
    return [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, (b[0] - a[0]) / seg, (b[1] - a[1]) / seg];
  }

  /* ---------------------------------------------------------- airliners */
  const planeMat = new THREE.MeshStandardMaterial({ color: 0xf2f4f5, roughness: 0.35, metalness: 0.3 });
  const makeJet = (tailHex) => {
    const g = new THREE.Group();
    const add = (geo, mat, x, y, z, rx = 0, ry = 0, rz = 0) => { const m = new THREE.Mesh(geo, mat); m.position.set(x, y, z); m.rotation.set(rx, ry, rz); m.castShadow = !lo; g.add(m); return m; };
    add(new THREE.CapsuleGeometry(2.6, 38, 6, 16), planeMat, 0, 0, 0, 0, 0, Math.PI / 2);
    add(new THREE.BoxGeometry(9, 0.5, 44), planeMat, 1, -1.1, 0);
    add(new THREE.BoxGeometry(5, 0.4, 16), planeMat, -20, 0.6, 0);
    add(new THREE.BoxGeometry(7, 8, 0.6), new THREE.MeshStandardMaterial({ color: tailHex, roughness: 0.4 }), -19.5, 5, 0, 0, 0, 0.35);
    for (const s of [-1, 1]) add(new THREE.CylinderGeometry(1.4, 1.2, 5, 14), new THREE.MeshStandardMaterial({ color: 0xa9aeb3, metalness: 0.5, roughness: 0.3 }), 3, -2.7, s * 9, 0, 0, Math.PI / 2);
    add(new THREE.BoxGeometry(1, 0.6, 2.6), new THREE.MeshStandardMaterial({ color: 0x1b232b }), 21.5, 0.8, 0);
    const beacons = [add(new THREE.SphereGeometry(0.35, 8, 6), new THREE.MeshBasicMaterial({ color: 0xff2020, toneMapped: false }), 0, 2.8, 0),
      add(new THREE.SphereGeometry(0.3, 8, 6), new THREE.MeshBasicMaterial({ color: 0xff2020, toneMapped: false }), 1, -1, -22),
      add(new THREE.SphereGeometry(0.3, 8, 6), new THREE.MeshBasicMaterial({ color: 0x20ff40, toneMapped: false }), 1, -1, 22)];
    g.userData.beacons = beacons;
    scene.add(g);
    return g;
  };
  const north = 1158, south = 1503;
  const flights = [
    { jet: makeJet('#c8102e'), kind: 'land', t: 0, z: north },
    { jet: makeJet('#1f4e9c'), kind: 'depart', t: 18, z: south }
  ];
  const groundAt = (x, z) => Math.max(W.groundHeight(x, z), 0);
  function fly(f, dt) {
    f.t += dt;
    let x, y, pitch = 0;
    if (f.kind === 'land') {
      // a long glide in from the east over Inglewood, flare, roll out
      const T1 = 30, T2 = 44, T3 = 54;
      if (f.t < T1) { const k = f.t / T1; x = 1900 - k * 2280; y = groundAt(x, f.z) + 4 + (1 - k) * 300; pitch = 0.05; }
      else if (f.t < T2) { const k = (f.t - T1) / (T2 - T1); x = -380 - (k * 2 - k * k) * 560; y = groundAt(x, f.z) + 4.2; }
      else { x = -940; y = -50; }
      if (f.t > T3) f.t = 0;
    } else {
      // roll west down the runway, rotate, climb out over the Pacific
      const T1 = 16, T2 = 46, T3 = 56;
      if (f.t < T1) { const k = f.t / T1; x = -360 - k * k * 600; y = groundAt(x, f.z) + 4.2; }
      else if (f.t < T2) { const k = (f.t - T1) / (T2 - T1); x = -960 - k * 2600; y = groundAt(-960, f.z) + 4.2 + k * k * 380 + k * 40; pitch = 0.16; }
      else { x = -4000; y = -50; }
      if (f.t > T3) f.t = 0;
    }
    f.jet.position.set(x, y, f.z);
    f.jet.rotation.set(0, Math.PI, pitch);
    f.jet.visible = y > -10;
  }

  /* ------------------------------------------------------- helicopter */
  const heli = new THREE.Group();
  const hb = new THREE.Mesh(new THREE.SphereGeometry(1.6, 16, 12), new THREE.MeshStandardMaterial({ color: 0x15171a, roughness: 0.35, metalness: 0.4 }));
  hb.scale.set(1.9, 1, 1);
  heli.add(hb);
  const doors = new THREE.Mesh(new THREE.BoxGeometry(2.6, 1.2, 3.25), new THREE.MeshStandardMaterial({ color: 0xf4f4f4, roughness: 0.4 }));
  doors.position.set(-0.2, -0.2, 0);
  heli.add(doors);
  const boom = new THREE.Mesh(new THREE.CylinderGeometry(0.25, 0.45, 6, 8), hb.material);
  boom.rotation.z = Math.PI / 2; boom.position.set(-4.5, 0.4, 0);
  heli.add(boom);
  const rotor = new THREE.Mesh(new THREE.BoxGeometry(11, 0.08, 0.35), new THREE.MeshStandardMaterial({ color: 0x222222 }));
  rotor.position.y = 1.8;
  heli.add(rotor);
  const rotor2 = rotor.clone(); rotor2.rotation.y = Math.PI / 2; rotor.add(rotor2);
  for (const s of [-1, 1]) { const sk = new THREE.Mesh(new THREE.BoxGeometry(4, 0.12, 0.12), new THREE.MeshStandardMaterial({ color: 0x333333 })); sk.position.set(0, -1.7, s * 1); heli.add(sk); }
  const beam = new THREE.Mesh(new THREE.ConeGeometry(14, 170, 24, 1, true), new THREE.MeshBasicMaterial({ color: 0xfff4d6, transparent: true, opacity: 0.12, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide, toneMapped: false }));
  beam.geometry.translate(0, -85, 0);
  heli.add(beam);
  const spot = new THREE.Mesh(new THREE.CircleGeometry(15, 32), new THREE.MeshBasicMaterial({ color: 0xfff4d6, transparent: true, opacity: 0.35, blending: THREE.AdditiveBlending, depthWrite: false, toneMapped: false }));
  spot.rotation.x = -Math.PI / 2;
  scene.add(heli, spot);
  heli.traverse((o) => { if (o.isMesh && o !== beam) o.castShadow = !lo; });
  const patrol = [[1460, -540], [300, -1450], [900, 500], [-650, -880]];
  const h = { leg: 0, t: 0, cx: patrol[0][0], cz: patrol[0][1] };

  let clock = 0;
  function update(dt, player, night) {
    clock += dt;
    // traffic: cruise, stop for whoever is in the road ahead
    for (let i = 0; i < cars.length; i++) {
      const c = cars[i];
      const [x0, z0, dx0, dz0] = along(c.r, c.s);
      const fx = dx0 * c.dir, fz = dz0 * c.dir;
      const ahead = (player.x - c.x) * fx + (player.z - c.z) * fz, side = Math.abs((player.x - c.x) * -fz + (player.z - c.z) * fx);
      const blocked = ahead > 0 && ahead < 14 && side < 2.6;
      c.speed += ((blocked ? 0 : c.cruise) - c.speed) * Math.min(1, dt * (blocked ? 4 : 0.8));
      c.s += c.speed * c.dir * dt;
      if (c.s > c.r.len - 5) { c.s = c.r.len - 5; c.dir = -1; c.lane = 2.4 + (c.lane > 3 ? 0 : 3.8); }
      if (c.s < 5) { c.s = 5; c.dir = 1; }
      const nx = -fz, nz = fx;                 // the right-hand side
      c.x = x0 + nx * c.lane; c.z = z0 + nz * c.lane;
      c.y = W.groundHeight(c.x, c.z) + 0.34;
      c.yaw = Math.atan2(-fz, fx);
      q.setFromAxisAngle(up, c.yaw);
      m4.compose(v.set(c.x, c.y, c.z), q, one);
      bodies.setMatrixAt(i, m4);
      lights.setMatrixAt(i, m4);
    }
    bodies.instanceMatrix.needsUpdate = true;
    lights.instanceMatrix.needsUpdate = true;
    lightMat.emissiveIntensity = 0.4 + night * 3;

    for (const f of flights) {
      fly(f, dt);
      const blink = Math.floor(clock * 1.2 + f.z) % 2 === 0;
      for (const b of f.jet.userData.beacons) b.visible = blink || night < 0.2;
    }

    // the helicopter: orbit a spot, then move on to the next
    h.t += dt;
    if (h.t > 70) { h.t = 0; h.leg = (h.leg + 1) % patrol.length; }
    const [tx, tz] = patrol[h.leg];
    h.cx += (tx - h.cx) * Math.min(1, dt * 0.05); h.cz += (tz - h.cz) * Math.min(1, dt * 0.05);
    const a = clock * 0.12;
    const hx = h.cx + Math.cos(a) * 240, hz = h.cz + Math.sin(a) * 240;
    heli.position.set(hx, groundAt(hx, hz) + 170, hz);
    heli.rotation.set(0.08, -a - Math.PI / 2, 0.12);
    rotor.rotation.y += dt * 40;
    // the searchlight points at the middle of the orbit
    const gy = groundAt(h.cx, h.cz);
    const dir = new THREE.Vector3(h.cx - hx, gy - heli.position.y, h.cz - hz);
    const len = dir.length();
    beam.visible = spot.visible = night > 0.3;
    beam.scale.set(1, len / 170, 1);
    beam.quaternion.setFromUnitVectors(new THREE.Vector3(0, -1, 0), dir.normalize()).premultiply(heli.quaternion.clone().invert());
    spot.position.set(h.cx, gy + 0.6, h.cz);
  }

  /* Push a body out of any traffic car it overlaps; returns true on a hit. */
  function push(b, r) {
    let hit = false;
    for (const c of cars) {
      const dx = b.x - c.x, dz = b.z - c.z;
      if (dx * dx + dz * dz > 25 || Math.abs(b.y - c.y) > 2.5) continue;
      const fx = Math.cos(c.yaw), fz = -Math.sin(c.yaw);
      const lx = dx * fx + dz * fz, lz = -dx * fz + dz * fx;
      const px = 2.3 + r - Math.abs(lx), pz = 0.95 + r - Math.abs(lz);
      if (px <= 0 || pz <= 0) continue;
      hit = true;
      if (px < pz) { const s = Math.sign(lx) || 1; b.x += fx * px * s; b.z += fz * px * s; }
      else { const s = Math.sign(lz) || 1; b.x += -fz * pz * s; b.z += fx * pz * s; }
    }
    return hit;
  }

  return { update, push, cars };
}

function mulberry(a) {
  return () => {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}
