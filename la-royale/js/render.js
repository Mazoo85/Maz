/*
 * Turns the world into a three.js scene.
 *
 * Almost everything is instanced: thousands of buildings, parts and trees
 * become a few hundred draw calls, split into tiles so the camera can skip
 * what it can't see and small things far away. Buildings get their windows
 * from a shader (from their position in the world, so a 250 m tower and a
 * bungalow share one mesh), and the windows light up at night.
 */

import * as THREE from '../vendor/three.module.min.js';
import { paintGround, paintNight } from './paint.js';
import { ROAD_WIDTH } from './geo.js';

const TILE = 1000;

export function buildScene(W, opts = {}) {
  const quality = opts.quality || 'high';
  const scene = new THREE.Scene();
  const uniforms = { uNight: { value: 0 }, uTime: { value: 0 } };
  const tiles = [];          // { mesh, cx, cz, far } for distance culling
  const tileKey = (x, z) => `${Math.floor((x + W.half) / TILE)},${Math.floor((z + W.half) / TILE)}`;

  /* ---------------------------------------------------------- ground */
  const texSize = quality === 'low' ? 2048 : 4096;
  const groundCanvas = document.createElement('canvas');
  groundCanvas.width = groundCanvas.height = texSize;
  paintGround(W, groundCanvas.getContext('2d'), texSize);
  const groundTex = new THREE.CanvasTexture(groundCanvas);
  groundTex.colorSpace = THREE.SRGBColorSpace;
  groundTex.anisotropy = opts.maxAnisotropy || 8;
  groundTex.generateMipmaps = true;
  groundTex.minFilter = THREE.LinearMipmapLinearFilter;

  const N = W.gridN, N1 = N + 1;
  const pos = new Float32Array(N1 * N1 * 3), uv = new Float32Array(N1 * N1 * 2);
  for (let j = 0; j <= N; j++) {
    for (let i = 0; i <= N; i++) {
      const k = j * N1 + i;
      pos[k * 3] = -W.half + i * W.cell;
      pos[k * 3 + 1] = W.heights[k];
      pos[k * 3 + 2] = -W.half + j * W.cell;
      uv[k * 2] = i / N; uv[k * 2 + 1] = 1 - j / N;
    }
  }
  const idx = new Uint32Array(N * N * 6);
  let q = 0;
  for (let j = 0; j < N; j++) {
    for (let i = 0; i < N; i++) {
      const a = j * N1 + i, d = j * N1 + i + 1, b = (j + 1) * N1 + i, c = (j + 1) * N1 + i + 1;
      idx[q++] = a; idx[q++] = b; idx[q++] = d;      // same split as world.groundHeight
      idx[q++] = b; idx[q++] = c; idx[q++] = d;
    }
  }
  const tg = new THREE.BufferGeometry();
  tg.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  tg.setAttribute('uv', new THREE.BufferAttribute(uv, 2));
  tg.setIndex(new THREE.BufferAttribute(idx, 1));
  tg.computeVertexNormals();
  const nightCanvas = document.createElement('canvas');
  nightCanvas.width = nightCanvas.height = 2048;
  paintNight(W, nightCanvas.getContext('2d'), 2048);
  const nightTex = new THREE.CanvasTexture(nightCanvas);
  nightTex.colorSpace = THREE.SRGBColorSpace;
  const detail = makeDetailTexture();
  const terrainMat = new THREE.MeshLambertMaterial({ map: groundTex, emissiveMap: nightTex, emissive: 0xffffff, emissiveIntensity: 0 });
  terrainMat.onBeforeCompile = (sh) => {
    sh.uniforms.uDetail = { value: detail };
    sh.fragmentShader = sh.fragmentShader
      .replace('#include <common>', '#include <common>\nuniform sampler2D uDetail;')
      .replace('#include <map_fragment>', `#include <map_fragment>
        float dn = texture2D(uDetail, vMapUv * 700.0).r * 0.6 + texture2D(uDetail, vMapUv * 97.0).r * 0.4;
        diffuseColor.rgb *= 0.84 + 0.32 * dn;`);
  };
  const terrain = new THREE.Mesh(tg, terrainMat);
  terrain.receiveShadow = true;
  scene.add(terrain);

  /* Roads as real surfaces with crisp lane markings, laid on the terrain. */
  const roadMats = buildRoads(W, scene, quality);

  /* An apron of land and sea out to the horizon, so the map has no edge. */
  const apron = new THREE.Mesh(new THREE.RingGeometry(W.half * 1.42, 40000, 64, 1), new THREE.MeshLambertMaterial({ color: 0x8b8a6e }));
  apron.rotation.x = -Math.PI / 2;
  apron.position.y = 6;
  scene.add(apron);
  for (const [x, z, w, d] of [[0, -W.half - 6000, 40000, 12000], [W.half + 6000, 0, 12000, 40000]]) {
    const m = new THREE.Mesh(new THREE.PlaneGeometry(w, d), apron.material);
    m.rotation.x = -Math.PI / 2; m.position.set(x, 4, z);
    scene.add(m);
  }

  /* ----------------------------------------------------------- water */
  const waterNormals = makeWaterNormals();
  waterNormals.wrapS = waterNormals.wrapT = THREE.RepeatWrapping;
  waterNormals.repeat.set(900, 900);
  const ocean = new THREE.Mesh(new THREE.PlaneGeometry(60000, 60000), new THREE.MeshPhongMaterial({
    color: 0x2d7fae, specular: 0xffffff, shininess: 140, normalMap: waterNormals, normalScale: new THREE.Vector2(0.35, 0.35),
    transparent: true, opacity: 0.9
  }));
  ocean.rotation.x = -Math.PI / 2;
  ocean.position.y = 0;
  ocean.renderOrder = 1;
  scene.add(ocean);
  const lakeMat = new THREE.MeshPhongMaterial({ color: 0x3a8bb0, specular: 0xffffff, shininess: 120, normalMap: waterNormals, normalScale: new THREE.Vector2(0.25, 0.25) });
  const tarMat = new THREE.MeshPhongMaterial({ color: 0x0d0b09, specular: 0x888888, shininess: 200 });
  for (const l of W.lakes) {
    const m = new THREE.Mesh(new THREE.CircleGeometry(l.r + 1.5, 40), l.tar ? tarMat : lakeMat);
    m.rotation.x = -Math.PI / 2;
    m.position.set(l.x, l.level, l.z);
    scene.add(m);
  }
  for (const c of W.canals) {
    const [a, b] = c;
    const len = Math.hypot(b[0] - a[0], b[1] - a[1]);
    const m = new THREE.Mesh(new THREE.PlaneGeometry(len + 8, 8), lakeMat);
    m.rotation.x = -Math.PI / 2;
    m.rotation.z = Math.atan2(-(b[1] - a[1]), b[0] - a[0]);
    m.position.set((a[0] + b[0]) / 2, W.canalLevel, (a[1] + b[1]) / 2);
    scene.add(m);
  }

  /* --------------------------------------------------------- materials */
  const winMat = windowMaterial(uniforms);
  const mats = {
    matte: new THREE.MeshLambertMaterial(),
    glow: new THREE.MeshBasicMaterial({ toneMapped: false }),
    metal: new THREE.MeshPhongMaterial({ shininess: 90, specular: 0xbfc6cc }),
    glass: new THREE.MeshPhongMaterial({ shininess: 120, specular: 0xffffff, transparent: true, opacity: 0.55, depthWrite: false, side: THREE.DoubleSide }),
    win: winMat
  };
  uniforms.glowMat = mats.glow;

  /* ---------------------------------------------------------- geometry */
  const pyramid = (() => {
    const g = new THREE.ConeGeometry(Math.SQRT1_2, 1, 4, 1);
    g.rotateY(Math.PI / 4); g.translate(0, 0.5, 0);
    const ng = g.toNonIndexed(); ng.computeVertexNormals();
    return ng;
  })();
  const geo = {
    box: new THREE.BoxGeometry(1, 1, 1),
    cyl: new THREE.CylinderGeometry(0.5, 0.5, 1, 20),
    cone: new THREE.ConeGeometry(0.5, 1, 16),
    sphere: new THREE.SphereGeometry(0.5, 18, 12),
    dome: new THREE.SphereGeometry(0.5, 22, 10, 0, Math.PI * 2, 0, Math.PI / 2),
    roof: pyramid,
    torus: new THREE.TorusGeometry(0.4, 0.1, 12, 40),
    prism: new THREE.CylinderGeometry(0.5, 0.5, 1, 3)
  };

  /* ---------------------------------------------------------- batching */
  const m4 = new THREE.Matrix4(), qt = new THREE.Quaternion(), eu = new THREE.Euler(0, 0, 0, 'YXZ');
  const v3 = new THREE.Vector3(), s3 = new THREE.Vector3(), col = new THREE.Color();
  function batchInstances(items, geometry, material, { cast = true, receive = true, farCull = 0, extra } = {}) {
    const byTile = new Map();
    for (const it of items) {
      const k = tileKey(it.x, it.z);
      let arr = byTile.get(k);
      if (!arr) byTile.set(k, (arr = []));
      arr.push(it);
    }
    for (const [k, arr] of byTile) {
      const mesh = new THREE.InstancedMesh(geometry, material, arr.length);
      let wins = null;
      if (extra) wins = new Float32Array(arr.length);
      arr.forEach((it, n) => {
        eu.set(it.rx || 0, it.ry || 0, it.rz || 0, 'YXZ');
        qt.setFromEuler(eu);
        m4.compose(v3.set(it.px ?? it.x, it.py, it.pz ?? it.z), qt, s3.set(it.sx, it.sy, it.sz));
        mesh.setMatrixAt(n, m4);
        mesh.setColorAt(n, col.set(it.c));
        if (wins) wins[n] = it.win || 0;
      });
      if (wins) {
        const attr = new THREE.InstancedBufferAttribute(wins, 1);
        mesh.geometry = geometry.clone();
        mesh.geometry.setAttribute('aWin', attr);
      }
      mesh.instanceMatrix.needsUpdate = true;
      if (mesh.instanceColor) mesh.instanceColor.needsUpdate = true;
      mesh.castShadow = cast && quality !== 'low';
      mesh.receiveShadow = receive && quality !== 'low';
      mesh.computeBoundingSphere();
      const [ti, tj] = k.split(',').map(Number);
      tiles.push({ mesh, cx: -W.half + (ti + 0.5) * TILE, cz: -W.half + (tj + 0.5) * TILE, far: farCull });
      scene.add(mesh);
    }
  }

  /* buildings */
  batchInstances(W.buildings.map((b) => ({ x: b.x, z: b.z, py: b.y + b.h / 2, sx: b.w, sy: b.h, sz: b.d, ry: b.rot, c: b.color, win: b.win })), geo.box, winMat, { extra: true });
  batchInstances(W.buildings.filter((b) => b.roof).map((b) => ({ x: b.x, z: b.z, py: b.y + b.h, sx: b.w * 1.08, sy: Math.min(b.w, b.d) * 0.3, sz: b.d * 1.08, ry: b.rot, c: b.roofColor })), geo.roof, mats.matte, { farCull: 2200 });

  /* parts, by shape and material, big ones always drawn */
  const groups = {};
  const buckets = new Map();
  const textParts = [];
  for (const p of W.parts) {
    if (p.s === 'text') { textParts.push(p); continue; }
    if (p.g) { (groups[p.g] ||= []).push(p); continue; }
    const m = p.m || 'matte';
    const big = Math.max(p.sx, p.sy, p.sz) > 24 || m === 'glow';
    const k = `${p.s}|${m}|${big ? 1 : 0}`;
    let arr = buckets.get(k);
    if (!arr) buckets.set(k, (arr = []));
    arr.push({ x: p.x, z: p.z, py: p.y, sx: p.sx, sy: p.sy, sz: p.sz, rx: p.rx, ry: p.ry, rz: p.rz, c: p.c, win: 3 });
  }
  for (const [k, items] of buckets) {
    const [shape, m, big] = k.split('|');
    batchInstances(items, geo[shape], mats[m], { farCull: big === '1' ? 0 : 1500, cast: m !== 'glow' && m !== 'glass', extra: m === 'win' });
  }

  /* animated groups: the Ferris wheel, the pumpjacks */
  const animated = [];
  for (const [id, parts] of Object.entries(groups)) {
    const g = W.groups[id];
    if (!g) continue;
    const outer = new THREE.Group();
    outer.position.set(g.x, g.y, g.z);
    outer.rotation.y = g.ry || 0;
    const inner = new THREE.Group();
    outer.add(inner);
    for (const p of parts) {
      const mat = (mats[p.m || 'matte']).clone();
      mat.color = new THREE.Color(p.c);
      const mesh = new THREE.Mesh(geo[p.s], mat);
      mesh.position.set(p.x, p.y, p.z);
      mesh.rotation.set(p.rx || 0, p.ry || 0, p.rz || 0, 'YXZ');
      mesh.scale.set(p.sx, p.sy, p.sz);
      mesh.castShadow = quality !== 'low';
      inner.add(mesh);
    }
    scene.add(outer);
    animated.push({ g, inner, outer });
  }

  /* signs and lettering */
  for (const p of textParts) scene.add(textMesh(p));

  /* ------------------------------------------------------------- trees */
  const palmTrunk = new THREE.CylinderGeometry(0.2, 0.36, 1, 6); palmTrunk.translate(0, 0.5, 0);
  const fronds = makeFronds();
  const trunk = new THREE.CylinderGeometry(0.25, 0.4, 1, 6); trunk.translate(0, 0.5, 0);
  const canopy = new THREE.IcosahedronGeometry(1, 1);
  const scrub = new THREE.IcosahedronGeometry(1, 0);
  const cypress = new THREE.SphereGeometry(1, 8, 8);
  const T = { palmTrunk: [], fronds: [], trunk: [], canopy: [], scrub: [], cypress: [] };
  const rnd = mulberry(7);
  const greens = ['#4e7a3a', '#5b8a41', '#3f6b34', '#6a8f45'];
  for (const t of W.trees) {
    const s = t.s;
    if (t.t === 0) {
      const lean = (rnd() - 0.5) * 0.12;
      T.palmTrunk.push({ x: t.x, z: t.z, py: t.y - 0.3, sx: s, sy: t.h, sz: s, rx: lean, ry: t.r, c: '#8a7660' });
      T.fronds.push({ x: t.x + Math.sin(lean) * 0, z: t.z - Math.sin(lean) * t.h * Math.cos(t.r) * 0, px: t.x + Math.sin(t.r) * Math.sin(lean) * t.h, pz: t.z + Math.cos(t.r) * Math.sin(lean) * t.h, py: t.y + t.h - 0.3, sx: s * 1.2, sy: s * 1.2, sz: s * 1.2, ry: t.r, c: greens[(t.r * 10 | 0) % 4] });
    } else if (t.t === 1 || t.t === 2 || t.t === 5) {
      const r = t.t === 5 ? t.h * 0.55 : t.h * 0.38;
      T.trunk.push({ x: t.x, z: t.z, py: t.y - 0.2, sx: s, sy: t.h * 0.55, sz: s, c: '#6b5440' });
      T.canopy.push({ x: t.x, z: t.z, py: t.y + t.h * 0.62, sx: r * s, sy: r * 0.75 * s, sz: r * s, ry: t.r, c: t.t === 2 ? '#9b7fd6' : t.t === 5 ? '#4a5e34' : greens[(t.r * 10 | 0) % 4] });
    } else if (t.t === 3) {
      T.scrub.push({ x: t.x, z: t.z, py: t.y + t.h * 0.25, sx: t.h * s, sy: t.h * 0.6, sz: t.h * s * 0.9, ry: t.r, c: rnd() < 0.5 ? '#56603a' : '#6b6b45' });
    } else if (t.t === 4) {
      T.cypress.push({ x: t.x, z: t.z, py: t.y + t.h * 0.5, sx: 1.4 * s, sy: t.h * 0.5, sz: 1.4 * s, c: '#2f4a2c' });
    }
  }
  const treeMat = fadeNearCamera(new THREE.MeshLambertMaterial());
  const far = quality === 'low' ? 900 : 1700;
  batchInstances(T.palmTrunk, palmTrunk, treeMat, { farCull: far, receive: false });
  batchInstances(T.fronds, fronds, fadeNearCamera(new THREE.MeshLambertMaterial({ side: THREE.DoubleSide })), { farCull: far, receive: false });
  batchInstances(T.trunk, trunk, treeMat, { farCull: far * 0.8, receive: false });
  batchInstances(T.canopy, canopy, treeMat, { farCull: far, receive: false });
  batchInstances(T.scrub, scrub, treeMat, { farCull: far * 0.7, cast: false, receive: false });
  batchInstances(T.cypress, cypress, treeMat, { farCull: far, receive: false });

  /* -------------------------------------------------- sky, sun, weather */
  const sky = makeSky();
  scene.add(sky.mesh);
  const hemi = new THREE.HemisphereLight(0xcfe3ff, 0x8a7a5c, 0.9);
  scene.add(hemi);
  const sun = new THREE.DirectionalLight(0xfff1d6, 2.2);
  sun.castShadow = quality !== 'low';
  sun.shadow.mapSize.set(quality === 'high' ? 4096 : 2048, quality === 'high' ? 4096 : 2048);
  const sc = sun.shadow.camera;
  sc.left = -220; sc.right = 220; sc.top = 220; sc.bottom = -220; sc.near = 10; sc.far = 2400;
  sun.shadow.bias = -0.0004;
  sun.shadow.normalBias = 0.6;
  scene.add(sun, sun.target);
  scene.fog = new THREE.Fog(0xd9cdb2, 300, 4200);

  const C = (h) => new THREE.Color(h);
  const keys = [
    // elevation, zenith, horizon, sun colour, sun intensity, hemi intensity, fog
    [-0.35, C('#05070f'), C('#0b1426'), C('#405080'), 0.0, 0.18, C('#0d1424')],
    [-0.08, C('#101a3a'), C('#3a2f4a'), C('#ff7a4a'), 0.0, 0.3, C('#2a2238')],
    [0.02, C('#2b3f73'), C('#f08a5d'), C('#ff8a4a'), 0.6, 0.45, C('#c98b6e')],
    [0.12, C('#4a78b5'), C('#f5c18a'), C('#ffcf8a'), 1.6, 0.7, C('#e8c9a0')],
    [0.35, C('#3f7fd0'), C('#d9d6c4'), C('#fff1d6'), 2.4, 0.95, C('#d9d2bf')],
    [1.0, C('#2f6fd0'), C('#cfd8dc'), C('#ffffff'), 2.8, 1.0, C('#d2d4cc')]
  ];
  const tmpC = [new THREE.Color(), new THREE.Color(), new THREE.Color(), new THREE.Color()];
  const sunDir = new THREE.Vector3();

  /* Time of day in hours; the sun rises over Pasadena and sets over the
   * Pacific, a little to the south. */
  function setTime(hours) {
    const ang = ((hours - 6) / 12) * Math.PI;
    const elev = Math.sin(ang);
    sunDir.set(Math.cos(ang), Math.max(elev, -0.3), 0.35).normalize();
    let i = 0; while (i < keys.length - 2 && keys[i + 1][0] < elev) i++;
    const a = keys[i], b = keys[i + 1];
    const t = Math.min(1, Math.max(0, (elev - a[0]) / (b[0] - a[0])));
    const zen = tmpC[0].copy(a[1]).lerp(b[1], t), hor = tmpC[1].copy(a[2]).lerp(b[2], t);
    const sc2 = tmpC[2].copy(a[3]).lerp(b[3], t), fog = tmpC[3].copy(a[6]).lerp(b[6], t);
    sky.uniforms.uZenith.value.copy(zen);
    sky.uniforms.uHorizon.value.copy(hor).lerp(fog, 0.75);
    sky.uniforms.uSunColor.value.copy(sc2);
    sky.uniforms.uSunDir.value.copy(sunDir);
    sky.uniforms.uNight.value = Math.min(1, Math.max(0, (0.05 - elev) / 0.3));
    scene.fog.color.copy(fog);
    sun.color.copy(sc2);
    sun.intensity = a[4] + (b[4] - a[4]) * t;
    hemi.intensity = a[5] + (b[5] - a[5]) * t;
    hemi.color.copy(zen).lerp(C('#ffffff'), 0.6);
    const night = Math.min(1, Math.max(0, (0.08 - elev) / 0.22));
    uniforms.uNight.value = night;
    terrainMat.emissiveIntensity = night * 1.1;
    for (const m of roadMats) m.emissiveIntensity = night * 0.32;
    hemi.groundColor.set(0x8a7a5c).lerp(C('#ff9a4a'), night * 0.6);
    mats.glow.color.setScalar(0.75 + night * 0.6);
    ocean.material.color.set(0x2d7fae).lerp(C('#0a1a2a'), night * 0.8);
    return { elev, night };
  }

  /* Each frame: follow the player with the sun's shadow box, animate,
   * cull small things far away. */
  function update(dt, time, camera, focus) {
    uniforms.uTime.value = time;
    waterNormals.offset.x = (time * 0.004) % 1;
    waterNormals.offset.y = (time * 0.0025) % 1;
    sky.mesh.position.copy(camera.position);
    const high = Math.max(0, camera.position.y - 120);
    scene.fog.near = 300 + high * 2;
    scene.fog.far = 4200 + high * 6;
    const fx = focus.x, fz = focus.z;
    sun.position.set(fx + sunDir.x * 1200, Math.max(focus.y, 0) + Math.max(sunDir.y, 0.08) * 1200, fz + sunDir.z * 1200);
    sun.target.position.set(fx, focus.y, fz);
    for (const a of animated) {
      if (a.g.anim === 'spinZ') a.inner.rotation.z = time * a.g.speed;
      else if (a.g.anim === 'nod') a.inner.rotation.z = Math.sin(time * a.g.speed + a.g.phase) * 0.35;
    }
    const cx = camera.position.x, cz = camera.position.z;
    for (const t of tiles) {
      if (!t.far) continue;
      const dx = Math.max(0, Math.abs(cx - t.cx) - TILE / 2), dz = Math.max(0, Math.abs(cz - t.cz) - TILE / 2);
      t.mesh.visible = dx * dx + dz * dz < t.far * t.far;
    }
  }

  return { scene, update, setTime, groundCanvas, sun, uniforms, stats: { drawBatches: tiles.length } };
}

/* -------------------------------------------------------------- helpers */

function windowMaterial(uniforms) {
  const m = new THREE.MeshLambertMaterial();
  m.onBeforeCompile = (sh) => {
    sh.uniforms.uNight = uniforms.uNight;
    sh.vertexShader = sh.vertexShader
      .replace('#include <common>', `#include <common>
        attribute float aWin;
        varying vec3 vWPos; varying vec3 vWNrm; varying float vWin; varying float vSeed;`)
      .replace('#include <project_vertex>', `#include <project_vertex>
        vec4 wp = vec4(transformed, 1.0);
        vec3 wn = objectNormal;
        #ifdef USE_INSTANCING
          wp = instanceMatrix * wp;
          wn = mat3(instanceMatrix) * wn;
          vSeed = fract(sin(dot(instanceMatrix[3].xz, vec2(12.9898, 78.233))) * 43758.5453);
        #else
          vSeed = 0.5;
        #endif
        wp = modelMatrix * wp;
        vWPos = wp.xyz;
        vWNrm = normalize(mat3(modelMatrix) * wn);
        vWin = aWin;`);
    sh.fragmentShader = sh.fragmentShader
      .replace('#include <common>', `#include <common>
        uniform float uNight;
        varying vec3 vWPos; varying vec3 vWNrm; varying float vWin; varying float vSeed;
        float h21(vec2 p) { return fract(sin(dot(p, vec2(41.3, 289.1))) * 43758.5453); }`)
      .replace('#include <color_fragment>', `#include <color_fragment>
        float winMask = 0.0; float lit = 0.0; vec3 winTint = vec3(1.0, 0.8, 0.55);
        vec3 n = normalize(vWNrm);
        if (n.y > 0.5) {
          diffuseColor.rgb = mix(diffuseColor.rgb, vec3(0.52, 0.52, 0.5), 0.45);
        } else if (abs(n.y) < 0.5 && vWin > 0.5) {
          vec2 t = normalize(vec2(-n.z, n.x));
          float u = dot(vWPos.xz, t);
          float v = vWPos.y;
          vec2 cell; vec2 f;
          if (vWin < 1.5)      { cell = vec2(4.2, 3.3); f = fract(vec2(u, v) / cell); winMask = step(0.3, f.x) * step(f.x, 0.68) * step(0.38, f.y) * step(f.y, 0.78); }
          else if (vWin < 2.5) { cell = vec2(3.2, 3.6); f = fract(vec2(u, v) / cell); winMask = step(0.14, f.x) * step(f.x, 0.86) * step(0.26, f.y) * step(f.y, 0.86); }
          else if (vWin < 3.5) { cell = vec2(1.6, 3.8); f = fract(vec2(u, v) / cell); winMask = step(0.06, f.x) * step(0.18, f.y); }
          else                 { cell = vec2(9.0, 7.0); f = fract(vec2(u, v) / cell); winMask = step(0.2, f.x) * step(f.x, 0.8) * step(0.72, f.y) * step(f.y, 0.9); }
          vec2 id = floor(vec2(u, v) / cell);
          float floorOn = step(0.25, h21(vec2(id.y, vSeed * 31.0)));
          lit = step(0.6, h21(id + vSeed * 97.0)) * floorOn * winMask;
          winTint = vWin > 2.5 ? vec3(0.78, 0.86, 1.0) : vec3(1.0, 0.78, 0.5);
          vec3 glass = vWin > 2.5 ? mix(vec3(0.24, 0.33, 0.42), diffuseColor.rgb, 0.35) : vec3(0.16, 0.19, 0.23);
          diffuseColor.rgb = mix(diffuseColor.rgb, glass, winMask * (vWin > 2.5 ? 0.75 : 0.85));
        }`)
      .replace('#include <emissivemap_fragment>', `#include <emissivemap_fragment>
        totalEmissiveRadiance += lit * uNight * winTint * (0.45 + 0.4 * h21(floor(vWPos.xz * 0.3) + floor(vWPos.y / 3.6)));`);
  };
  return m;
}

function textMesh(p) {
  const font = p.font || 64;
  const lines = String(p.text).split('\n');
  const cw = Math.max(64, Math.min(2048, Math.round((p.sx / p.sy) * font * 1.6 * (lines.length > 1 ? 1 : 1))));
  const ch = Math.max(64, Math.round(font * 1.6 * Math.max(1, lines.length > 1 ? lines.length * 0.35 : 1)));
  const c = document.createElement('canvas');
  c.width = Math.min(2048, Math.max(cw, ch * (p.sx / p.sy))); c.height = ch;
  const ctx = c.getContext('2d');
  if (p.bg) { ctx.fillStyle = p.bg; ctx.fillRect(0, 0, c.width, c.height); }
  ctx.fillStyle = p.c;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  const lh = c.height / lines.length;
  let size = lh * 0.82;
  ctx.font = `900 ${size}px "Arial Black", Impact, system-ui, sans-serif`;
  const widest = Math.max(...lines.map((l) => ctx.measureText(l).width));
  if (widest > c.width * 0.94) { size *= (c.width * 0.94) / widest; ctx.font = `900 ${size}px "Arial Black", Impact, system-ui, sans-serif`; }
  if (p.glow) { ctx.shadowColor = p.c; ctx.shadowBlur = size * 0.25; }
  lines.forEach((l, i) => ctx.fillText(l, c.width / 2, lh * (i + 0.5)));
  const tex = new THREE.CanvasTexture(c);
  tex.colorSpace = THREE.SRGBColorSpace;
  tex.anisotropy = 4;
  const mat = p.glow
    ? new THREE.MeshBasicMaterial({ map: tex, transparent: true, side: THREE.DoubleSide, toneMapped: false, depthWrite: !!p.bg })
    : new THREE.MeshLambertMaterial({ map: tex, transparent: !p.bg, alphaTest: p.bg ? 0 : 0.4, side: THREE.DoubleSide });
  const m = new THREE.Mesh(new THREE.PlaneGeometry(p.sx, p.sy), mat);
  m.position.set(p.x, p.y, p.z);
  m.rotation.set(p.rx || 0, p.ry || 0, 0, 'YXZ');
  m.castShadow = !!p.solid;
  return m;
}

/* Leaves right in front of the camera would fill the screen: let the
 * camera see through anything of this material that's very close. */
function fadeNearCamera(m) {
  m.onBeforeCompile = (sh) => {
    sh.vertexShader = sh.vertexShader
      .replace('#include <common>', '#include <common>\nvarying vec3 vFadePos;')
      .replace('#include <project_vertex>', `#include <project_vertex>
        vec4 fp = vec4(transformed, 1.0);
        #ifdef USE_INSTANCING
          fp = instanceMatrix * fp;
        #endif
        vFadePos = (modelMatrix * fp).xyz;`);
    sh.fragmentShader = sh.fragmentShader
      .replace('#include <common>', '#include <common>\nvarying vec3 vFadePos;')
      .replace('#include <clipping_planes_fragment>', `#include <clipping_planes_fragment>
        float camD = distance(vFadePos, cameraPosition);
        if (camD < 5.0 && fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453) > (camD - 2.0) / 3.0) discard;`);
  };
  return m;
}

/* Ribbons of road following the ground, one mesh per kind of road. */
function buildRoads(W, scene, quality) {
  const kinds = { freeway: [], blvd: [], local: [] };
  W.roads.forEach((r, idx) => kinds[r.kind].push([r, idx]));
  const mats = [];
  for (const [kind, list] of Object.entries(kinds)) {
    const pos = [], uv = [], index = [];
    const w = ROAD_WIDTH[kind], tile = kind === 'freeway' ? 18 : 14;
    const lift = (kind === 'freeway' ? 0.42 : kind === 'blvd' ? 0.34 : 0.28);
    for (const [r, idx] of list) {
      const pts = resample(r.pts, 6);
      let dist = 0;
      const base = pos.length / 3;
      for (let i = 0; i < pts.length; i++) {
        const a = pts[Math.max(0, i - 1)], b = pts[Math.min(pts.length - 1, i + 1)];
        const L = Math.hypot(b[0] - a[0], b[1] - a[1]) || 1;
        const nx = -(b[1] - a[1]) / L, nz = (b[0] - a[0]) / L;
        if (i > 0) dist += Math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]);
        for (const u of [0, 0.5, 1]) {
          const off = (u - 0.5) * w;
          const x = pts[i][0] + nx * off, z = pts[i][1] + nz * off;
          let y = W.groundHeight(x, z) + lift + idx * 0.004;
          for (const br of W.bridges) {
            const dx = x - br.x, dz = z - br.z, c = Math.cos(br.a), s = Math.sin(br.a);
            if (Math.abs(dx * c - dz * s) < br.span / 2 && Math.abs(dx * s + dz * c) < br.w / 2 + 1) y = Math.max(y, br.y + 0.35);
          }
          pos.push(x, y, z);
          uv.push(u, dist / tile);
        }
        if (i > 0) {
          const k = base + i * 3;
          for (const [p0, p1] of [[0, 1], [1, 2]]) {
            const a0 = k - 3 + p0, a1 = k - 3 + p1, b0 = k + p0, b1 = k + p1;
            index.push(a0, a1, b0, a1, b1, b0);
          }
        }
      }
    }
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
    g.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2));
    g.setIndex(index);
    g.computeVertexNormals();
    const tex = roadTexture(kind);
    tex.anisotropy = quality === 'low' ? 4 : 8;
    const mat = new THREE.MeshLambertMaterial({ map: tex, emissive: 0xffa060, emissiveIntensity: 0 });
    const mesh = new THREE.Mesh(g, mat);
    mesh.receiveShadow = quality !== 'low';
    scene.add(mesh);
    mats.push(mat);
  }
  return mats;
}

function resample(pts, step) {
  const out = [pts[0]];
  for (let i = 0; i < pts.length - 1; i++) {
    const [ax, az] = pts[i], [bx, bz] = pts[i + 1];
    const n = Math.max(1, Math.ceil(Math.hypot(bx - ax, bz - az) / step));
    for (let k = 1; k <= n; k++) out.push([ax + ((bx - ax) * k) / n, az + ((bz - az) * k) / n]);
  }
  return out;
}

function roadTexture(kind) {
  const c = document.createElement('canvas');
  c.width = 256; c.height = 256;
  const ctx = c.getContext('2d');
  ctx.fillStyle = kind === 'freeway' ? '#3d3f42' : kind === 'blvd' ? '#45474a' : '#54565a';
  ctx.fillRect(0, 0, 256, 256);
  const img = ctx.getImageData(0, 0, 256, 256);
  const rnd = mulberry(kind.length * 99);
  for (let k = 0; k < img.data.length; k += 4) {
    const n = (rnd() - 0.5) * 18;
    img.data[k] += n; img.data[k + 1] += n; img.data[k + 2] += n;
  }
  ctx.putImageData(img, 0, 0);
  const line = (u, w, color, dash) => {
    ctx.fillStyle = color;
    if (dash) ctx.fillRect(u * 256 - w / 2, 0, w, 256 * dash);
    else ctx.fillRect(u * 256 - w / 2, 0, w, 256);
  };
  if (kind === 'freeway') {
    line(0.035, 4, '#e8c547'); line(0.965, 4, '#e8c547');
    line(0.5, 10, '#9a968c');
    line(0.47, 3, '#f0f0f0'); line(0.53, 3, '#f0f0f0');
    for (const u of [0.18, 0.33, 0.67, 0.82]) line(u, 3, '#f2f2f2', 0.35);
  } else if (kind === 'blvd') {
    line(0.012, 6, '#b9b4a9'); line(0.988, 6, '#b9b4a9');
    line(0.488, 3, '#e3c04a'); line(0.512, 3, '#e3c04a');
    for (const u of [0.25, 0.75]) line(u, 3, '#ececec', 0.4);
  } else {
    line(0.5, 3, '#d9b84a', 0.5);
  }
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  t.wrapS = THREE.ClampToEdgeWrapping;
  t.wrapT = THREE.RepeatWrapping;
  return t;
}

function makeDetailTexture() {
  const S = 256;
  const c = document.createElement('canvas');
  c.width = c.height = S;
  const ctx = c.getContext('2d');
  const img = ctx.createImageData(S, S);
  const rnd = mulberry(3);
  const grid = Array.from({ length: 33 * 33 }, () => rnd());
  for (let y = 0; y < S; y++) {
    for (let x = 0; x < S; x++) {
      const gx = (x / S) * 32, gy = (y / S) * 32, ix = Math.floor(gx), iy = Math.floor(gy), fx = gx - ix, fy = gy - iy;
      const g = (i, j) => grid[(j % 32) * 33 + (i % 32)];
      const v = (g(ix, iy) * (1 - fx) + g(ix + 1, iy) * fx) * (1 - fy) + (g(ix, iy + 1) * (1 - fx) + g(ix + 1, iy + 1) * fx) * fy;
      const n = v * 0.6 + rnd() * 0.4;
      const k = (y * S + x) * 4;
      img.data[k] = img.data[k + 1] = img.data[k + 2] = n * 255; img.data[k + 3] = 255;
    }
  }
  ctx.putImageData(img, 0, 0);
  const t = new THREE.CanvasTexture(c);
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  return t;
}

/* A palm crown: ten long drooping fronds round a point. */
function makeFronds() {
  const pos = [];
  const blades = 10;
  for (let b = 0; b < blades; b++) {
    const a = (b / blades) * Math.PI * 2 + (b % 2) * 0.2;
    const ca = Math.cos(a), sa = Math.sin(a);
    const segs = 4, len = 4.2 + (b % 3) * 0.5;
    let prev = null;
    for (let s = 0; s <= segs; s++) {
      const t = s / segs;
      const r = t * len;
      const y = 0.9 * t - 2.4 * t * t;
      const w = 0.75 * Math.sin(Math.PI * Math.min(1, t * 1.1 + 0.08));
      const px = ca * r, pz = sa * r;
      const lx = -sa * w, lz = ca * w;
      const cur = [[px + lx, y, pz + lz], [px - lx, y, pz - lz]];
      if (prev) pos.push(...prev[0], ...prev[1], ...cur[0], ...prev[1], ...cur[1], ...cur[0]);
      prev = cur;
    }
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  g.computeVertexNormals();
  return g;
}

function makeSky() {
  const uniforms = {
    uZenith: { value: new THREE.Color('#3f7fd0') }, uHorizon: { value: new THREE.Color('#d9d6c4') },
    uSunColor: { value: new THREE.Color('#fff1d6') }, uSunDir: { value: new THREE.Vector3(0, 1, 0) }, uNight: { value: 0 }
  };
  const mat = new THREE.ShaderMaterial({
    uniforms, side: THREE.BackSide, depthWrite: false, fog: false,
    vertexShader: `varying vec3 vDir; void main() { vDir = normalize(position); gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }`,
    fragmentShader: `
      uniform vec3 uZenith, uHorizon, uSunColor, uSunDir; uniform float uNight; varying vec3 vDir;
      float hash(vec3 p) { return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453); }
      void main() {
        vec3 d = normalize(vDir);
        float h = clamp(d.y, -0.1, 1.0);
        vec3 col = mix(uHorizon, uZenith, pow(smoothstep(0.0, 0.6, h), 0.7));
        if (d.y < 0.0) col = mix(uHorizon, uHorizon * 0.7, min(1.0, -d.y * 8.0));
        float s = max(dot(d, normalize(uSunDir)), 0.0);
        col += uSunColor * (pow(s, 900.0) * 6.0 + pow(s, 12.0) * 0.35);
        if (uNight > 0.0 && d.y > 0.0) {
          vec3 cell = floor(d * 380.0);
          float st = step(0.9975, hash(cell));
          col += vec3(st) * uNight * (0.5 + 0.5 * d.y);
        }
        gl_FragColor = vec4(col, 1.0);
        #include <tonemapping_fragment>
        #include <colorspace_fragment>
      }`
  });
  const mesh = new THREE.Mesh(new THREE.SphereGeometry(9000, 32, 16), mat);
  mesh.renderOrder = -1;
  mesh.frustumCulled = false;
  return { mesh, uniforms };
}

function makeWaterNormals() {
  const S = 256;
  const c = document.createElement('canvas');
  c.width = c.height = S;
  const ctx = c.getContext('2d');
  const img = ctx.createImageData(S, S);
  const h = (x, y) =>
    Math.sin((x / S) * Math.PI * 2 * 3 + Math.sin((y / S) * Math.PI * 2 * 2) * 1.5) * 0.5 +
    Math.sin((y / S) * Math.PI * 2 * 5 + (x / S) * Math.PI * 2 * 1) * 0.3 +
    Math.sin(((x + y) / S) * Math.PI * 2 * 7) * 0.2;
  for (let y = 0; y < S; y++) {
    for (let x = 0; x < S; x++) {
      const dx = h(x + 1, y) - h(x - 1, y), dy = h(x, y + 1) - h(x, y - 1);
      const nx = -dx * 2, ny = -dy * 2, nz = 1, l = Math.hypot(nx, ny, nz);
      const k = (y * S + x) * 4;
      img.data[k] = ((nx / l) * 0.5 + 0.5) * 255;
      img.data[k + 1] = ((ny / l) * 0.5 + 0.5) * 255;
      img.data[k + 2] = ((nz / l) * 0.5 + 0.5) * 255;
      img.data[k + 3] = 255;
    }
  }
  ctx.putImageData(img, 0, 0);
  return new THREE.CanvasTexture(c);
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
