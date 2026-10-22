/*
 * The things that move: you, your parachute, the cars, the blimp.
 * Built from boxes and cylinders like the rest of the city.
 */

import * as THREE from '../vendor/three.module.min.js';

const lam = (c) => new THREE.MeshLambertMaterial({ color: c });
const glow = (c) => new THREE.MeshBasicMaterial({ color: c, toneMapped: false });
const box = (w, h, d, mat, x = 0, y = 0, z = 0) => {
  const m = new THREE.Mesh(new THREE.BoxGeometry(w, h, d), mat);
  m.position.set(x, y, z);
  m.castShadow = true;
  return m;
};

/* ---------------------------------------------------------------- player */

export function makePlayer(look = {}) {
  const skin = lam(look.skin || '#c68d6a'), shirt = lam(look.shirt || '#f4f4f0'), pants = lam(look.pants || '#2d4a7a');
  const shoe = lam('#1d1d1d'), cap = lam(look.cap || '#c0392b');
  const root = new THREE.Group();
  const body = new THREE.Group();
  root.add(body);
  const torso = box(0.56, 0.66, 0.3, shirt, 0, 1.25, 0);
  body.add(torso);
  const head = box(0.34, 0.36, 0.34, skin, 0, 1.78, 0);
  body.add(head);
  body.add(box(0.37, 0.12, 0.37, cap, 0, 1.98, 0));
  body.add(box(0.36, 0.05, 0.2, cap, 0, 1.94, -0.26));     // brim, worn backwards
  body.add(box(0.36, 0.08, 0.04, lam('#151515'), 0, 1.82, 0.17)); // shades
  const limb = (w, h, mat, x, y, footMat) => {
    const pivot = new THREE.Group();
    pivot.position.set(x, y, 0);
    pivot.add(box(w, h, w, mat, 0, -h / 2, 0));
    if (footMat) pivot.add(box(w + 0.02, 0.1, w + 0.14, footMat, 0, -h + 0.03, 0.06));
    body.add(pivot);
    return pivot;
  };
  const armL = limb(0.16, 0.62, skin, -0.37, 1.55);
  const armR = limb(0.16, 0.62, skin, 0.37, 1.55);
  armL.children[0].add(box(0.17, 0.18, 0.17, shirt, 0, 0.21, 0));
  armR.children[0].add(box(0.17, 0.18, 0.17, shirt, 0, 0.21, 0));
  const legL = limb(0.2, 0.9, pants, -0.14, 0.92, shoe);
  const legR = limb(0.2, 0.9, pants, 0.14, 0.92, shoe);

  /* the parachute, hidden until you pull the cord */
  const chute = new THREE.Group();
  const canopy = new THREE.Mesh(new THREE.SphereGeometry(1, 20, 8, 0, Math.PI * 2, 0, Math.PI / 2.6), new THREE.MeshLambertMaterial({ color: '#ff4f6d', side: THREE.DoubleSide }));
  canopy.scale.set(4.6, 2.2, 3);
  canopy.position.y = 5.2;
  chute.add(canopy);
  const stripe = new THREE.Mesh(new THREE.SphereGeometry(1.01, 20, 8, 0, Math.PI * 2, 0, Math.PI / 6), new THREE.MeshLambertMaterial({ color: '#ffd23f', side: THREE.DoubleSide }));
  stripe.scale.copy(canopy.scale); stripe.position.copy(canopy.position);
  chute.add(stripe);
  const lineMat = new THREE.LineBasicMaterial({ color: '#333' });
  for (const [x, z] of [[-4, -2], [4, -2], [-4, 2], [4, 2], [0, 2.8], [0, -2.8]]) {
    chute.add(new THREE.Line(new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(0, 1.6, 0), new THREE.Vector3(x * 0.95, 5.6, z * 0.95)]), lineMat));
  }
  chute.visible = false;
  root.add(chute);
  root.traverse((o) => { if (o.isMesh) o.castShadow = true; });

  let phase = 0;
  /* Pose the body for what it's doing: walking cycles the limbs, falling
   * spreads them out, the chute hangs them down. */
  function animate(dt, state, speed) {
    chute.visible = state === 'chute';
    body.rotation.x = 0;
    body.position.y = 0;
    if (state === 'freefall') {
      body.rotation.x = Math.PI / 2.4;
      body.position.y = 1;
      armL.rotation.set(0, 0, 2.2); armR.rotation.set(0, 0, -2.2);
      legL.rotation.set(0, 0, 0.35); legR.rotation.set(0, 0, -0.35);
      return;
    }
    if (state === 'chute') {
      armL.rotation.set(0, 0, 2.6); armR.rotation.set(0, 0, -2.6);
      legL.rotation.set(0.2, 0, 0.08); legR.rotation.set(-0.1, 0, -0.08);
      return;
    }
    if (state === 'swim') {
      phase += dt * 6;
      body.rotation.x = Math.PI / 2.6;
      body.position.y = 0.6;
      armL.rotation.set(Math.sin(phase) * 2.4, 0, 0.2); armR.rotation.set(-Math.sin(phase) * 2.4, 0, -0.2);
      legL.rotation.set(Math.sin(phase * 1.5) * 0.4, 0, 0); legR.rotation.set(-Math.sin(phase * 1.5) * 0.4, 0, 0);
      return;
    }
    if (state === 'air') {
      armL.rotation.set(-0.4, 0, 0.5); armR.rotation.set(-0.4, 0, -0.5);
      legL.rotation.set(-0.5, 0, 0); legR.rotation.set(0.3, 0, 0);
      return;
    }
    const amp = Math.min(1, speed / 6) * (speed > 7 ? 1.1 : 0.7);
    phase += dt * (speed > 0.3 ? 2 + speed * 1.15 : 0);
    if (speed < 0.3) phase *= 0.9;
    const s = Math.sin(phase) * amp;
    legL.rotation.set(s, 0, 0); legR.rotation.set(-s, 0, 0);
    armL.rotation.set(-s * 0.9, 0, 0.06); armR.rotation.set(s * 0.9, 0, -0.06);
    body.position.y = Math.abs(Math.cos(phase)) * 0.06 * amp;
  }
  return { root, animate };
}

/* ------------------------------------------------------------------ cars */

export const CAR_TYPES = {
  convertible: { name: 'Convertible', top: 44, accel: 14, color: ['#d7263d', '#1b998b', '#f5f5f5', '#2e86de'], len: 4.6, w: 1.95, roof: false },
  lowrider: { name: 'Lowrider', top: 32, accel: 10, color: ['#7b2cbf', '#0fa3b1', '#c9a227', '#e63946'], len: 5.6, w: 2.0, roof: true, chrome: true, hop: true },
  cruiser: { name: 'Police Cruiser', top: 50, accel: 16, color: ['#111111'], len: 5.0, w: 2.0, roof: true, police: true },
  taxi: { name: 'Taxi', top: 40, accel: 13, color: ['#f6c90e'], len: 4.8, w: 1.95, roof: true, taxi: true },
  suv: { name: 'SUV', top: 40, accel: 12, color: ['#1d1d1f', '#f2f2f2', '#3c4a57'], len: 5.0, w: 2.1, roof: true, tall: true },
  exotic: { name: 'Supercar', top: 60, accel: 20, color: ['#ff8c00', '#a6e22e', '#e10600', '#f8f8f8'], len: 4.6, w: 2.05, roof: true, low: true },
  truck: { name: 'Pickup', top: 34, accel: 11, color: ['#7f8c8d', '#8e2b2b', '#2d3e50'], len: 5.6, w: 2.1, roof: true, bed: true, tall: true },
  tacotruck: { name: 'Taco Truck', top: 26, accel: 8, color: ['#f4f1ea'], len: 6.4, w: 2.4, van: true }
};

export function makeCar(type, colorIndex = 0) {
  const T = CAR_TYPES[type];
  const color = T.color[colorIndex % T.color.length];
  const root = new THREE.Group();
  const chassis = new THREE.Group();
  root.add(chassis);
  const paint = new THREE.MeshPhongMaterial({ color, shininess: 80, specular: 0x666666 });
  const dark = lam('#1e2329'), glass = new THREE.MeshPhongMaterial({ color: '#25313b', shininess: 120, specular: 0xaaaaaa });
  const L = T.len, Wd = T.w;
  const lift = T.low ? 0.32 : T.tall ? 0.6 : T.hop ? 0.3 : 0.42;
  if (T.van) {
    chassis.add(box(L, 2.3, Wd, paint, 0, lift + 1.35, 0));
    chassis.add(box(0.1, 0.9, Wd * 0.8, glass, L / 2, lift + 1.9, 0));
    chassis.add(box(L * 0.55, 1.1, 0.08, glass, -0.4, lift + 1.7, Wd / 2 + 0.02));
    const awning = box(L * 0.6, 0.08, 1, lam('#e63946'), -0.4, lift + 2.45, Wd / 2 + 0.5);
    awning.rotation.x = 0.25;
    chassis.add(awning);
    chassis.add(signBoard('TACOS', '#ffd23f', '#c1121f', L * 0.5, 0.7, -0.4, lift + 2.85, 0));
  } else {
    const bodyH = T.low ? 0.6 : 0.78;
    chassis.add(box(L, bodyH, Wd, paint, 0, lift + bodyH / 2 + 0.1, 0));
    if (T.police) {
      for (const s of [-1, 1]) chassis.add(box(L * 0.42, bodyH * 0.9, 0.05, lam('#f4f4f4'), -0.1, lift + bodyH / 2 + 0.1, s * (Wd / 2 + 0.01)));
    }
    if (T.roof) {
      const cabL = T.low ? L * 0.42 : L * 0.5, cabH = T.low ? 0.5 : T.tall ? 0.85 : 0.65;
      chassis.add(box(cabL, cabH, Wd * 0.9, glass, -0.25, lift + bodyH + 0.1 + cabH / 2, 0));
      chassis.add(box(cabL * 0.92, 0.08, Wd * 0.92, paint, -0.25, lift + bodyH + 0.1 + cabH, 0));
      if (T.bed) chassis.add(box(L * 0.36, 0.4, Wd * 0.94, dark, -L * 0.3, lift + bodyH + 0.12, 0));
      if (T.taxi) chassis.add(signBoard('TAXI', '#1b1b1b', '#fff3b0', 1.1, 0.35, -0.25, lift + bodyH + cabH + 0.32, 0));
      if (T.police) {
        const bar = new THREE.Group();
        bar.position.set(-0.25, lift + bodyH + cabH + 0.2, 0);
        const red = box(0.4, 0.18, Wd * 0.4, glow('#ff1e1e'), 0, 0, -Wd * 0.22);
        const blue = box(0.4, 0.18, Wd * 0.4, glow('#1e5bff'), 0, 0, Wd * 0.22);
        bar.add(red, blue);
        chassis.add(bar);
        root.userData.siren = { red, blue };
      }
    } else {
      chassis.add(box(0.08, 0.4, Wd * 0.86, glass, L * 0.12, lift + bodyH + 0.3, 0));
      for (const lx of [-0.3, -1.3]) chassis.add(box(0.6, 0.55, Wd * 0.8, lam('#e9dcc5'), lx, lift + bodyH + 0.1, 0));
    }
  }
  chassis.add(box(0.06, 0.18, 0.4, glow('#fff6d5'), L / 2 + 0.01, lift + 0.55, Wd * 0.33));
  chassis.add(box(0.06, 0.18, 0.4, glow('#fff6d5'), L / 2 + 0.01, lift + 0.55, -Wd * 0.33));
  chassis.add(box(0.06, 0.16, 0.42, glow('#ff2a2a'), -L / 2 - 0.01, lift + 0.6, Wd * 0.33));
  chassis.add(box(0.06, 0.16, 0.42, glow('#ff2a2a'), -L / 2 - 0.01, lift + 0.6, -Wd * 0.33));

  const wheels = [];
  const r = T.low ? 0.36 : T.tall ? 0.46 : 0.38;
  const tyre = lam('#151515'), rim = new THREE.MeshPhongMaterial({ color: T.chrome ? '#e6e6e6' : '#9aa0a6', shininess: 120 });
  for (const [x, z, front] of [[L * 0.32, Wd / 2, true], [L * 0.32, -Wd / 2, true], [-L * 0.32, Wd / 2, false], [-L * 0.32, -Wd / 2, false]]) {
    const steer = new THREE.Group();
    steer.position.set(x, r, z);
    const spin = new THREE.Group();
    const t = new THREE.Mesh(new THREE.CylinderGeometry(r, r, 0.3, 14), tyre);
    t.rotation.x = Math.PI / 2;
    const h = new THREE.Mesh(new THREE.CylinderGeometry(r * 0.6, r * 0.6, 0.32, 10), rim);
    h.rotation.x = Math.PI / 2;
    spin.add(t, h);
    steer.add(spin);
    root.add(steer);
    wheels.push({ steer, spin, front });
  }
  root.traverse((o) => { if (o.isMesh) o.castShadow = true; });
  return { root, chassis, wheels, type, T, radius: r };
}

function signBoard(text, fg, bg, w, h, x, y, z) {
  const c = document.createElement('canvas');
  c.width = 256; c.height = Math.round(256 * (h / w));
  const ctx = c.getContext('2d');
  ctx.fillStyle = bg; ctx.fillRect(0, 0, c.width, c.height);
  ctx.fillStyle = fg; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
  ctx.font = `900 ${c.height * 0.72}px "Arial Black", Impact, sans-serif`;
  ctx.fillText(text, c.width / 2, c.height / 2);
  const tex = new THREE.CanvasTexture(c);
  tex.colorSpace = THREE.SRGBColorSpace;
  const m = new THREE.Mesh(new THREE.BoxGeometry(w, h, 0.12), [lam(bg), lam(bg), lam(bg), lam(bg), new THREE.MeshBasicMaterial({ map: tex, toneMapped: false }), new THREE.MeshBasicMaterial({ map: tex, toneMapped: false })]);
  m.position.set(x, y, z);
  m.rotation.y = Math.PI / 2;
  return m;
}

/* ----------------------------------------------------------------- blimp */

export function makeBlimp() {
  const root = new THREE.Group();
  const hull = new THREE.Mesh(new THREE.SphereGeometry(1, 32, 16), new THREE.MeshPhongMaterial({ color: '#dfe3e8', shininess: 60 }));
  hull.scale.set(36, 9.5, 9.5);
  root.add(hull);
  const band = new THREE.Mesh(new THREE.CylinderGeometry(9.62, 9.62, 4, 32, 1, true), new THREE.MeshLambertMaterial({ color: '#1d3f8a', side: THREE.DoubleSide }));
  band.rotation.z = Math.PI / 2;
  root.add(band);
  const word = (side) => {
    const c = document.createElement('canvas');
    c.width = 1024; c.height = 160;
    const ctx = c.getContext('2d');
    ctx.fillStyle = '#ffd200';
    ctx.font = '900 120px "Arial Black", Impact, sans-serif';
    ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
    ctx.fillText('LA ROYALE', 512, 84);
    const tex = new THREE.CanvasTexture(c);
    tex.colorSpace = THREE.SRGBColorSpace;
    const m = new THREE.Mesh(new THREE.PlaneGeometry(34, 5.3), new THREE.MeshBasicMaterial({ map: tex, transparent: true, toneMapped: false }));
    m.position.set(0, 0, side * 9.7);
    if (side < 0) m.rotation.y = Math.PI;
    return m;
  };
  root.add(word(1), word(-1));
  const fin = new THREE.MeshLambertMaterial({ color: '#1d3f8a' });
  for (const [rx] of [[0], [Math.PI / 2]]) {
    const f = new THREE.Mesh(new THREE.BoxGeometry(9, 0.4, 16), fin);
    f.position.x = -31; f.rotation.x = rx;
    root.add(f);
  }
  const gondola = new THREE.Mesh(new THREE.BoxGeometry(10, 3, 3.6), new THREE.MeshLambertMaterial({ color: '#2a2d31' }));
  gondola.position.set(4, -10.5, 0);
  root.add(gondola);
  const prop = new THREE.Mesh(new THREE.BoxGeometry(0.3, 4, 0.4), new THREE.MeshLambertMaterial({ color: '#333' }));
  prop.position.set(-2, -10.5, 2.4);
  root.add(prop);
  root.userData.prop = prop;
  return root;
}
