/*
 * Turns the world into a three.js scene, aiming for the look of a 2008-era
 * console open world: physically based materials lit by the sun and by the
 * sky itself (an environment map rebuilt from a scattering sky as the day
 * turns), glass towers and car paint that reflect it, soft shadows, baked
 * contact shadows, bloom on everything that glows at night, and dense
 * street-level detail.
 *
 * Almost everything is instanced, in tiles, so the camera skips what it
 * can't see and small things far away.
 */

import * as THREE from '../vendor/three.module.min.js';
import { Sky } from '../vendor/jsm/objects/Sky.js';
import { EffectComposer } from '../vendor/jsm/postprocessing/EffectComposer.js';
import { RenderPass } from '../vendor/jsm/postprocessing/RenderPass.js';
import { UnrealBloomPass } from '../vendor/jsm/postprocessing/UnrealBloomPass.js';
import { OutputPass } from '../vendor/jsm/postprocessing/OutputPass.js';
import { mergeGeometries } from '../vendor/jsm/utils/BufferGeometryUtils.js';
import { paintGround, paintNight } from './paint.js';
import { ROAD_WIDTH } from './geo.js';
import { SIGN_TEXTS, PROP } from './street.js';
import { buildFlora } from './flora.js';
import { buildStreetLife } from './streetlife.js';

const TILE = 1000;

export function buildScene(W, renderer, opts = {}) {
  const quality = opts.quality || 'high';
  const hi = quality === 'high', lo = quality === 'low';
  const scene = new THREE.Scene();
  const uniforms = { uNight: { value: 0 }, uTime: { value: 0 } };
  const tiles = [];
  const tileKey = (x, z) => `${Math.floor((x + W.half) / TILE)},${Math.floor((z + W.half) / TILE)}`;
  const maxAniso = renderer.capabilities.getMaxAnisotropy();
  const glowMats = [];           // emissive materials that brighten at night

  /* ------------------------------------------------------------- ground */
  const texSize = lo ? 2048 : 4096;
  const groundCanvas = document.createElement('canvas');
  groundCanvas.width = groundCanvas.height = texSize;
  paintGround(W, groundCanvas.getContext('2d'), texSize);
  const groundTex = new THREE.CanvasTexture(groundCanvas);
  groundTex.colorSpace = THREE.SRGBColorSpace;
  groundTex.anisotropy = maxAniso;
  const nightCanvas = document.createElement('canvas');
  nightCanvas.width = nightCanvas.height = 2048;
  paintNight(W, nightCanvas.getContext('2d'), 2048);
  const nightTex = new THREE.CanvasTexture(nightCanvas);
  nightTex.colorSpace = THREE.SRGBColorSpace;
  const detail = makeDetailTexture();

  const N = W.gridN, N1 = N + 1;
  const pos = new Float32Array(N1 * N1 * 3), uv = new Float32Array(N1 * N1 * 2);
  for (let j = 0; j <= N; j++) {
    for (let i = 0; i <= N; i++) {
      const k = j * N1 + i;
      pos[k * 3] = -W.half + i * W.cell; pos[k * 3 + 1] = W.heights[k]; pos[k * 3 + 2] = -W.half + j * W.cell;
      uv[k * 2] = i / N; uv[k * 2 + 1] = 1 - j / N;
    }
  }
  const idx = new Uint32Array(N * N * 6);
  let q = 0;
  for (let j = 0; j < N; j++) {
    for (let i = 0; i < N; i++) {
      const a = j * N1 + i, d = a + 1, b = a + N1, c = b + 1;
      idx[q++] = a; idx[q++] = b; idx[q++] = d;      // same split as world.groundHeight
      idx[q++] = b; idx[q++] = c; idx[q++] = d;
    }
  }
  const tg = new THREE.BufferGeometry();
  tg.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  tg.setAttribute('uv', new THREE.BufferAttribute(uv, 2));
  tg.setIndex(new THREE.BufferAttribute(idx, 1));
  tg.computeVertexNormals();
  const terrainMat = new THREE.MeshStandardMaterial({ map: groundTex, roughness: 0.95, metalness: 0, emissiveMap: nightTex, emissive: 0xffffff, emissiveIntensity: 0 });
  terrainMat.onBeforeCompile = (sh) => {
    sh.uniforms.uDetail = { value: detail };
    sh.vertexShader = sh.vertexShader
      .replace('#include <common>', '#include <common>\nvarying vec3 vTN;')
      .replace('#include <beginnormal_vertex>', '#include <beginnormal_vertex>\nvTN = objectNormal;');
    sh.fragmentShader = sh.fragmentShader
      .replace('#include <common>', '#include <common>\nuniform sampler2D uDetail;\nvarying vec3 vTN;')
      .replace('#include <map_fragment>', `#include <map_fragment>
        float dn = texture2D(uDetail, vMapUv * 760.0).r * 0.55 + texture2D(uDetail, vMapUv * 113.0).g * 0.45;
        float slope = 1.0 - clamp(vTN.y, 0.0, 1.0);
        vec3 rock = vec3(0.47, 0.42, 0.36) * (0.75 + 0.5 * texture2D(uDetail, vMapUv * 300.0).b);
        diffuseColor.rgb = mix(diffuseColor.rgb, rock, smoothstep(0.32, 0.55, slope));
        diffuseColor.rgb *= 0.8 + 0.4 * dn;`);
  };
  const terrain = new THREE.Mesh(tg, terrainMat);
  terrain.receiveShadow = !lo;
  scene.add(terrain);

  /* An apron of land out to the horizon, so the map has no edge. */
  const apronMat = new THREE.MeshStandardMaterial({ color: 0x7d7a5e, roughness: 1 });
  // (land to the north and east only: west and south is the Pacific)
  for (const [x, z, w, d] of [[0, -W.half - 6000, 40000, 12000], [W.half + 6000, 0, 12000, 40000]]) {
    const m = new THREE.Mesh(new THREE.PlaneGeometry(w, d), apronMat);
    m.rotation.x = -Math.PI / 2; m.position.set(x, 4, z);
    scene.add(m);
  }

  /* ------------------------------------------------------------- water */
  const waterNormals = makeWaterNormals();
  waterNormals.wrapS = waterNormals.wrapT = THREE.RepeatWrapping;
  waterNormals.repeat.set(1600, 1600);
  const oceanMat = new THREE.MeshStandardMaterial({
    color: 0x1a6b92, roughness: 0.06, metalness: 0.05, normalMap: waterNormals, normalScale: new THREE.Vector2(0.55, 0.55),
    transparent: true, opacity: 0.93, envMapIntensity: 1.4
  });
  const ocean = new THREE.Mesh(new THREE.PlaneGeometry(60000, 60000), oceanMat);
  ocean.rotation.x = -Math.PI / 2;
  ocean.renderOrder = 1;
  scene.add(ocean);
  const lakeNormals = waterNormals.clone();
  lakeNormals.repeat.set(0.06, 0.06);
  const lakeMat = new THREE.MeshStandardMaterial({ color: 0x1f6a7d, roughness: 0.06, metalness: 0.1, normalMap: lakeNormals, normalScale: new THREE.Vector2(0.3, 0.3) });
  const tarMat = new THREE.MeshStandardMaterial({ color: 0x0b0907, roughness: 0.02, metalness: 0.3 });
  for (const l of W.lakes) {
    const m = new THREE.Mesh(new THREE.CircleGeometry(l.r + 1.5, 48), l.tar ? tarMat : lakeMat);
    m.rotation.x = -Math.PI / 2; m.position.set(l.x, l.level, l.z);
    scene.add(m);
  }
  for (const [a, b] of W.canals) {
    const len = Math.hypot(b[0] - a[0], b[1] - a[1]);
    const m = new THREE.Mesh(new THREE.PlaneGeometry(len + 8, 8), lakeMat);
    m.rotation.x = -Math.PI / 2; m.rotation.z = Math.atan2(-(b[1] - a[1]), b[0] - a[0]);
    m.position.set((a[0] + b[0]) / 2, W.canalLevel, (a[1] + b[1]) / 2);
    scene.add(m);
  }

  /* --------------------------------------------------------- materials */
  const facade = facadeMaterial(uniforms);
  const glow = new THREE.MeshBasicMaterial({ toneMapped: false });
  const mats = {
    matte: new THREE.MeshStandardMaterial({ roughness: 0.82, metalness: 0 }),
    glow,
    metal: new THREE.MeshStandardMaterial({ roughness: 0.22, metalness: 0.9 }),
    glass: new THREE.MeshStandardMaterial({ roughness: 0.05, metalness: 0.3, transparent: true, opacity: 0.55, depthWrite: false, side: THREE.DoubleSide }),
    win: facade,
    // surface materials for architecture (textured in surfaceMaterials below)
    ...surfaceMaterials(maxAniso)
  };

  /* ---------------------------------------------------------- geometry */
  const pyramid = (() => {
    const g = new THREE.ConeGeometry(Math.SQRT1_2, 1, 4, 1);
    g.rotateY(Math.PI / 4); g.translate(0, 0.5, 0);
    const ng = g.toNonIndexed(); ng.computeVertexNormals();
    return ng;
  })();
  const geo = {
    box: new THREE.BoxGeometry(1, 1, 1),
    cyl: new THREE.CylinderGeometry(0.5, 0.5, 1, 24),
    cone: new THREE.ConeGeometry(0.5, 1, 20),
    sphere: new THREE.SphereGeometry(0.5, 24, 16),
    dome: new THREE.SphereGeometry(0.5, 28, 12, 0, Math.PI * 2, 0, Math.PI / 2),
    roof: pyramid,
    torus: new THREE.TorusGeometry(0.4, 0.1, 14, 48),
    prism: new THREE.CylinderGeometry(0.5, 0.5, 1, 3),
    gable: gableGeometry(),
    arch: archGeometry()
  };

  /* ---------------------------------------------------------- batching */
  const m4 = new THREE.Matrix4(), qt = new THREE.Quaternion(), eu = new THREE.Euler(0, 0, 0, 'YXZ');
  const v3 = new THREE.Vector3(), s3 = new THREE.Vector3(), col = new THREE.Color();
  function batch(items, geometry, material, { cast = true, receive = true, far = 0, attrs = null } = {}) {
    const byTile = new Map();
    for (const it of items) {
      const k = tileKey(it.x, it.z);
      let arr = byTile.get(k);
      if (!arr) byTile.set(k, (arr = []));
      arr.push(it);
    }
    for (const [k, arr] of byTile) {
      const g = attrs ? geometry.clone() : geometry;
      const mesh = new THREE.InstancedMesh(g, material, arr.length);
      const extra = attrs ? Object.fromEntries(Object.entries(attrs).map(([name, size]) => [name, new Float32Array(arr.length * size)])) : null;
      arr.forEach((it, n) => {
        eu.set(it.rx || 0, it.ry || 0, it.rz || 0, 'YXZ');
        qt.setFromEuler(eu);
        m4.compose(v3.set(it.px ?? it.x, it.py, it.pz ?? it.z), qt, s3.set(it.sx ?? 1, it.sy ?? 1, it.sz ?? 1));
        mesh.setMatrixAt(n, m4);
        mesh.setColorAt(n, col.set(it.c || '#ffffff'));
        if (extra) for (const [name, size] of Object.entries(attrs)) for (let c = 0; c < size; c++) extra[name][n * size + c] = size === 1 ? it[name] : it[name][c];
      });
      if (extra) for (const [name, size] of Object.entries(attrs)) g.setAttribute(name, new THREE.InstancedBufferAttribute(extra[name], size));
      mesh.instanceMatrix.needsUpdate = true;
      if (mesh.instanceColor) mesh.instanceColor.needsUpdate = true;
      mesh.castShadow = cast && !lo;
      mesh.receiveShadow = receive && !lo;
      mesh.computeBoundingSphere();
      const [ti, tj] = k.split(',').map(Number);
      tiles.push({ mesh, cx: -W.half + (ti + 0.5) * TILE, cz: -W.half + (tj + 0.5) * TILE, far });
      scene.add(mesh);
    }
  }

  /* --------------------------------------------------------- buildings */
  batch(W.buildings.map((b) => ({ x: b.x, z: b.z, py: b.y + b.h / 2, sx: b.w, sy: b.h, sz: b.d, ry: b.rot, c: b.color, aWin: b.win })), geo.box, facade, { attrs: { aWin: 1 } });
  batch(W.buildings.filter((b) => b.roof).map((b) => ({ x: b.x, z: b.z, py: b.y + b.h, sx: b.w * 1.08, sy: Math.min(b.w, b.d) * 0.3, sz: b.d * 1.08, ry: b.rot, c: b.roofColor })), geo.roof, tileRoofMaterial(), { far: 2400 });
  // flat roofs get a parapet lip
  const lips = [];
  for (const b of W.buildings) {
    if (b.roof || b.h < 7 || b.win === 0) continue;
    const top = b.y + b.h, c = Math.cos(b.rot), s = Math.sin(b.rot);
    for (const [lx, lz, w, d] of [[0, b.d / 2, b.w, 0.35], [0, -b.d / 2, b.w, 0.35], [b.w / 2, 0, 0.35, b.d], [-b.w / 2, 0, 0.35, b.d]]) {
      lips.push({ x: b.x + lx * c + lz * s, z: b.z - lx * s + lz * c, py: top + 0.45, sx: w, sy: 0.9, sz: d, ry: b.rot, c: b.color });
    }
  }
  batch(lips, geo.box, mats.matte, { far: 1400, cast: false });

  /* ---------------------------------------------------- landmark parts */
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
    arr.push({ x: p.x, z: p.z, py: p.y, sx: p.sx, sy: p.sy, sz: p.sz, rx: p.rx, ry: p.ry, rz: p.rz, c: p.c, aWin: p.win ?? 3 });
  }
  for (const [k, items] of buckets) {
    const [shape, m, big] = k.split('|');
    batch(items, geo[shape], mats[m], { far: big === '1' ? 0 : 1500, cast: m !== 'glow' && m !== 'glass', attrs: m === 'win' ? { aWin: 1 } : null });
  }
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
      const base = mats[p.m || 'matte'];
      const mat = base === facade ? mats.matte.clone() : base.clone();
      mat.color = new THREE.Color(p.c);
      const mesh = new THREE.Mesh(geo[p.s], mat);
      mesh.position.set(p.x, p.y, p.z);
      mesh.rotation.set(p.rx || 0, p.ry || 0, p.rz || 0, 'YXZ');
      mesh.scale.set(p.sx, p.sy, p.sz);
      mesh.castShadow = !lo;
      inner.add(mesh);
    }
    scene.add(outer);
    animated.push({ g, inner });
  }
  for (const p of textParts) scene.add(textMesh(p, glowMats));

  /* ------------------------------------------------------------- roads */
  const roadMats = buildRoads(W, scene, maxAniso, lo);

  /* --------------------------------------------- shop signs (an atlas) */
  const signAtlas = makeSignAtlas();
  const signMat = new THREE.MeshStandardMaterial({ map: signAtlas, emissiveMap: signAtlas, emissive: 0xffffff, emissiveIntensity: 0.15, roughness: 0.6 });
  atlasUV(signMat, 8, 8);
  glowMats.push({ mat: signMat, day: 0.12, night: 1.6 });
  batch(W.signs.map((s) => ({ x: s.x, z: s.z, py: s.y, sx: s.w, sy: s.h, sz: 1, ry: s.ry, aCell: [s.cell % 8, Math.floor(s.cell / 8)] })), new THREE.PlaneGeometry(1, 1), signMat, { far: 1100, cast: false, attrs: { aCell: 2 } });

  /* -------------------------------------------------- street furniture */
  const furniture = buildFurniture(glowMats);
  for (const [type, parts] of Object.entries(furniture)) {
    const items = W.props.filter((p) => p.t === Number(type));
    if (!items.length) continue;
    for (const part of parts) {
      batch(items.map((p) => ({ x: p.x, z: p.z, py: p.y, ry: p.ry, sx: part.arm ? (p.arm || 10) / 10 : 1, c: part.tint ? part.tint(p) : '#ffffff' })),
        part.geo, part.mat, { far: part.far || 900, cast: part.cast !== false });
    }
  }
  // wires, in tiles so far-off ones drop out
  const wireTiles = new Map();
  for (let i = 0; i < W.wires.length; i += 6) {
    const k = tileKey(W.wires[i], W.wires[i + 2]);
    if (!wireTiles.has(k)) wireTiles.set(k, []);
    const arr = wireTiles.get(k);
    for (let c = 0; c < 6; c++) arr.push(W.wires[i + c]);
  }
  const wireMat = new THREE.LineBasicMaterial({ color: 0x1d1d1d, transparent: true, opacity: 0.75 });
  for (const [k, arr] of wireTiles) {
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.Float32BufferAttribute(arr, 3));
    const lines = new THREE.LineSegments(g, wireMat);
    const [ti, tj] = k.split(',').map(Number);
    tiles.push({ mesh: lines, cx: -W.half + (ti + 0.5) * TILE, cz: -W.half + (tj + 0.5) * TILE, far: 650 });
    scene.add(lines);
  }
  // zebra crosswalks
  const zebra = (() => {
    const parts = [];
    for (let u = -ROAD_WIDTH.blvd / 2 + 0.8; u < ROAD_WIDTH.blvd / 2 - 0.5; u += 1.25) {
      const b = new THREE.BoxGeometry(3, 0.04, 0.6); b.translate(0, 0, u); parts.push(b);
    }
    return mergeGeometries(parts);
  })();
  batch(W.crosswalks.map((c) => ({ x: c.x, z: c.z, py: c.y + 0.42, ry: c.ry, c: '#eeeeea' })), zebra, mats.matte, { far: 700, cast: false });
  buildGantries(W, scene, glowMats);

  /* ------------------------------------- trees, plants, street life */
  const ctx = { THREE, batch, lo, hi, fadeNearCamera, mulberry, shadeHex, glowMats, tiles, TILE, tileKey };
  buildFlora(W, scene, ctx);
  const streetLife = buildStreetLife(W, scene, ctx);

  /* ------------------------------------------ sky, sun, environment light */
  const skyParams = (s) => {
    const u = s.material.uniforms;
    u.turbidity.value = 7;          // a little smog
    u.rayleigh.value = 1.5;
    u.mieCoefficient.value = 0.006;
    u.mieDirectionalG.value = 0.86;
  };
  const sky = new Sky();
  sky.scale.setScalar(9000);
  skyParams(sky);
  sky.frustumCulled = false;
  scene.add(sky);
  // the night sky: a deep blue veil over the scattering sky, then stars
  const nightSky = new THREE.Mesh(new THREE.SphereGeometry(8800, 24, 12), new THREE.MeshBasicMaterial({ color: 0x050a18, transparent: true, opacity: 0, side: THREE.BackSide, depthWrite: false, fog: false }));
  nightSky.renderOrder = -1;
  nightSky.frustumCulled = false;
  scene.add(nightSky);
  const stars = makeStars();
  scene.add(stars);
  const moon = new THREE.Mesh(new THREE.CircleGeometry(90, 32), new THREE.MeshBasicMaterial({ color: 0xf2f0e6, transparent: true, fog: false, toneMapped: false }));
  scene.add(moon);

  const envScene = new THREE.Scene();
  const envSky = new Sky();
  envSky.scale.setScalar(1000);
  skyParams(envSky);
  envScene.add(envSky);
  const envGround = new THREE.Mesh(new THREE.SphereGeometry(900, 16, 8, 0, Math.PI * 2, Math.PI / 2, Math.PI / 2), new THREE.MeshBasicMaterial({ color: 0x5a5448, side: THREE.BackSide }));
  envScene.add(envGround);
  const envVeil = new THREE.Mesh(new THREE.SphereGeometry(950, 16, 8), new THREE.MeshBasicMaterial({ color: 0x050a18, transparent: true, opacity: 0, side: THREE.BackSide, depthWrite: false }));
  envScene.add(envVeil);
  const pmrem = new THREE.PMREMGenerator(renderer);
  let envRT = null, envAge = 99, envNight = -1;
  const envSun = new THREE.Vector3(9, 9, 9);

  const hemi = new THREE.HemisphereLight(0xcfe3ff, 0x8a7a5c, 0.35);
  scene.add(hemi);
  const sun = new THREE.DirectionalLight(0xfff1d6, 3);
  sun.castShadow = !lo;
  const sm = hi ? 4096 : 2048;
  sun.shadow.mapSize.set(sm, sm);
  const sc = sun.shadow.camera;
  const box = hi ? 260 : 200;
  sc.left = -box; sc.right = box; sc.top = box; sc.bottom = -box; sc.near = 10; sc.far = 3000;
  sun.shadow.bias = -0.0003;
  sun.shadow.normalBias = 0.5;
  sun.shadow.radius = 3;
  scene.add(sun, sun.target);
  scene.fog = new THREE.Fog(0xcfd3d0, 400, 5200);

  /* ---------------------------------------------------- post-processing */
  let composer = null, bloom = null;
  if (!lo) {
    const size = renderer.getDrawingBufferSize(new THREE.Vector2());
    const target = new THREE.WebGLRenderTarget(size.x, size.y, { type: THREE.HalfFloatType, samples: hi ? 4 : 2 });
    composer = new EffectComposer(renderer, target);
    composer.addPass(new RenderPass(scene, null));
    bloom = new UnrealBloomPass(new THREE.Vector2(size.x / 2, size.y / 2), 0.2, 0.55, 0.9);
    composer.addPass(bloom);
    composer.addPass(new OutputPass());
  }

  const C = (h) => new THREE.Color(h);
  // elevation → sun colour, sun intensity, ambient, fog, sky light
  const keys = [
    [-0.4, C('#7f93c8'), 0.25, 0.85, C('#0e1424'), 0.1],
    [-0.08, C('#8fa0d0'), 0.2, 0.7, C('#2a2236'), 0.18],
    [0.0, C('#ff8a4a'), 0.8, 0.35, C('#c2876b'), 0.45],
    [0.1, C('#ffc88a'), 2.2, 0.35, C('#dcc0a0'), 0.8],
    [0.3, C('#fff0d8'), 3.0, 0.35, C('#cfd2cc'), 1.0],
    [1.0, C('#ffffff'), 3.3, 0.35, C('#c9d3d6'), 1.0]
  ];
  const tc = [new THREE.Color(), new THREE.Color()];
  const sunDir = new THREE.Vector3();
  let nightNow = 0;

  /* Time of day in hours. The sun rises over Pasadena and sets over the
   * Pacific; the moon takes over at night. */
  function setTime(hours) {
    const ang = ((hours - 6) / 12) * Math.PI;
    const elev = Math.sin(ang);
    sunDir.set(Math.cos(ang), elev, 0.32).normalize();
    let i = 0; while (i < keys.length - 2 && keys[i + 1][0] < elev) i++;
    const a = keys[i], b = keys[i + 1];
    const t = Math.min(1, Math.max(0, (elev - a[0]) / (b[0] - a[0])));
    const night = Math.min(1, Math.max(0, (0.06 - elev) / 0.2));
    nightNow = night;
    sky.material.uniforms.sunPosition.value.copy(sunDir);
    envSky.material.uniforms.sunPosition.value.copy(sunDir);
    sun.color.copy(tc[0].copy(a[1]).lerp(b[1], t));
    sun.intensity = a[2] + (b[2] - a[2]) * t;
    hemi.intensity = a[3] + (b[3] - a[3]) * t;
    hemi.groundColor.set(0x8a7a5c).lerp(C('#ff9a4a'), night * 0.7);
    hemi.color.set(0xcfe3ff).lerp(C('#4a5a90'), night);
    scene.fog.color.copy(tc[1].copy(a[4]).lerp(b[4], t));
    scene.environmentIntensity = a[5] + (b[5] - a[5]) * t;
    // at night the light is the moon: dim and blue
    if (sunDir.y < -0.05) { sun.color.set('#8fa6e0'); sun.intensity = 0.35 * night; }
    uniforms.uNight.value = night;
    terrainMat.emissiveIntensity = night * 0.9;
    for (const m of roadMats) m.emissiveIntensity = night * 1.1;
    for (const g of glowMats) g.mat.emissiveIntensity = g.day + (g.night - g.day) * night;
    glow.color.setScalar(0.85 + night * 0.9);
    stars.material.opacity = night;
    moon.material.opacity = night;
    oceanMat.color.set(0x1a6b92).lerp(C('#04121c'), night * 0.75);
    if (bloom) { bloom.strength = 0.16 + night * 0.4; bloom.threshold = 0.9 - night * 0.12; }
    nightSky.material.opacity = night * 0.88;
    return { elev, night };
  }

  /* Rebuild the sky's reflection now and then as the sun moves. */
  function updateEnv() {
    if (envRT && envAge < 1.5 && envSun.distanceTo(sunDir) < 0.03 && Math.abs(envNight - nightNow) < 0.05) return;
    envNight = nightNow;
    envAge = 0;
    envSun.copy(sunDir);
    envGround.material.color.set(0x5a5448).multiplyScalar(Math.max(0.08, 1 - nightNow * 0.9));
    envVeil.material.opacity = nightNow * 0.92;
    const rt = pmrem.fromScene(envScene, 0, 1, 2000);
    if (envRT) envRT.dispose();
    envRT = rt;
    scene.environment = rt.texture;
  }

  const tmpV = new THREE.Vector3();
  /* Each frame: follow the player with the shadow box, animate, cull. */
  function update(dt, time, camera, focus) {
    uniforms.uTime.value = time;
    envAge += dt;
    updateEnv();
    waterNormals.offset.set((time * 0.0021) % 1, (time * 0.0013) % 1);
    lakeNormals.offset.set((time * 0.01) % 1, (time * 0.007) % 1);
    sky.position.copy(camera.position);
    stars.position.copy(camera.position);
    nightSky.position.copy(camera.position);
    moon.position.copy(camera.position).addScaledVector(sunDir, -8000);
    moon.lookAt(camera.position);
    const high = Math.max(0, camera.position.y - 120);
    scene.fog.near = 400 + high * 2;
    scene.fog.far = 5200 + high * 6;
    const lightDir = sunDir.y > -0.05 ? sunDir : tmpV.copy(sunDir).negate().setY(0.6).normalize();
    // snap the shadow box to its texels so shadows don't crawl as you move
    const step = (box * 2) / sm;
    const fx = Math.round(focus.x / step) * step, fz = Math.round(focus.z / step) * step, fy = Math.max(focus.y, 0);
    sun.position.set(fx + lightDir.x * 1400, fy + Math.max(lightDir.y, 0.1) * 1400, fz + lightDir.z * 1400);
    sun.target.position.set(fx, fy, fz);
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

  function render(camera) {
    if (composer) {
      composer.passes[0].camera = camera;
      composer.render();
    } else renderer.render(scene, camera);
  }
  function setSize(w, h) {
    if (!composer) return;
    const pr = renderer.getPixelRatio();
    composer.setPixelRatio(pr);
    composer.setSize(w, h);
  }

  return { scene, update, setTime, render, setSize, groundCanvas, sun, uniforms, streetLife, stats: { drawBatches: tiles.length } };
}

/* ================================================================ helpers */

function surfaceMaterials() {
  const m = (o) => new THREE.MeshStandardMaterial({ roughness: 0.9, metalness: 0, ...o });
  return {
    stucco: m({}), plaster: m({}), brick: m({}), wood: m({ roughness: 0.8 }), concrete: m({}),
    tile: m({ roughness: 0.7 }), asphalt: m({ roughness: 0.95 }), metal: m({ roughness: 0.35, metalness: 0.8 }),
    chrome: m({ roughness: 0.15, metalness: 1 }), fabric: m({ roughness: 1, side: THREE.DoubleSide }), foliage: m({ roughness: 0.9 })
  };
}

/* A gable roof: a triangular prism, ridge along x, base (1 × 1) at y = 0,
 * apex at y = 1. Scale x = length, z = span, y = rise. */
function gableGeometry() {
  const v = [
    -0.5, 0, -0.5, 0.5, 0, -0.5, 0.5, 1, 0, -0.5, 1, 0,   // north slope
    0.5, 0, 0.5, -0.5, 0, 0.5, -0.5, 1, 0, 0.5, 1, 0,     // south slope
    -0.5, 0, 0.5, -0.5, 0, -0.5, -0.5, 1, 0,              // west gable end
    0.5, 0, -0.5, 0.5, 0, 0.5, 0.5, 1, 0                  // east gable end
  ];
  const idx = [0, 2, 1, 0, 3, 2, 4, 6, 5, 4, 7, 6, 8, 10, 9, 11, 13, 12];
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.Float32BufferAttribute(v, 3));
  g.setIndex(idx);
  const ng = g.toNonIndexed();
  ng.computeVertexNormals();
  // box-projected UVs so textures land on the slopes
  const p = ng.attributes.position, uv = [];
  for (let i = 0; i < p.count; i++) uv.push(p.getX(i) + p.getZ(i), p.getY(i) + p.getZ(i));
  ng.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2));
  return ng;
}

/* A round-headed arch opening: a 1 × 1 × 1 block with a semicircular top,
 * base at y = 0 (for arcades, Spanish porches, Union Station). */
function archGeometry() {
  const shape = new THREE.Shape();
  shape.moveTo(-0.5, 0); shape.lineTo(-0.5, 0.5);
  shape.absarc(0, 0.5, 0.5, Math.PI, 0, true);
  shape.lineTo(0.5, 0); shape.lineTo(-0.5, 0);
  const g = new THREE.ExtrudeGeometry(shape, { depth: 1, bevelEnabled: false, curveSegments: 12 });
  g.translate(0, 0, -0.5);
  g.scale(1, 1 / 1.0, 1);
  return g;
}

/* Facades: windows, frames, floor lines, shop windows, grime and a contact
 * shadow at the foot of each wall, all from the wall's position in the
 * world, so one mesh serves a bungalow and a 250 m tower. The glass is
 * mirror-smooth and picks up the sky; at night some windows light up. */
function facadeMaterial(uniforms) {
  const m = new THREE.MeshStandardMaterial({ roughness: 0.85, metalness: 0 });
  m.onBeforeCompile = (sh) => {
    sh.uniforms.uNight = uniforms.uNight;
    sh.vertexShader = sh.vertexShader
      .replace('#include <common>', `#include <common>
        attribute float aWin;
        varying vec3 vWPos; varying vec3 vWNrm; varying float vWin; varying float vSeed; varying float vH; varying float vTop;`)
      .replace('#include <project_vertex>', `#include <project_vertex>
        vec4 wp = vec4(transformed, 1.0);
        vec3 wn = objectNormal;
        float sy = 1.0;
        #ifdef USE_INSTANCING
          wp = instanceMatrix * wp;
          wn = mat3(instanceMatrix) * wn;
          sy = length(instanceMatrix[1].xyz);
          vSeed = fract(sin(dot(instanceMatrix[3].xz, vec2(12.9898, 78.233))) * 43758.5453);
        #else
          vSeed = 0.5;
        #endif
        wp = modelMatrix * wp;
        vWPos = wp.xyz;
        vWNrm = normalize(mat3(modelMatrix) * wn);
        vWin = aWin;
        vH = (position.y + 0.5) * sy;
        vTop = sy;`);
    sh.fragmentShader = sh.fragmentShader
      .replace('#include <common>', `#include <common>
        uniform float uNight;
        varying vec3 vWPos; varying vec3 vWNrm; varying float vWin; varying float vSeed; varying float vH; varying float vTop;
        float h21(vec2 p) { return fract(sin(dot(p, vec2(41.3, 289.1))) * 43758.5453); }
        float vn(vec2 p) { vec2 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
          return mix(mix(h21(i), h21(i + vec2(1, 0)), f.x), mix(h21(i + vec2(0, 1)), h21(i + vec2(1, 1)), f.x), f.y); }`)
      .replace('#include <color_fragment>', `#include <color_fragment>
        float winMask = 0.0; float lit = 0.0; float glassy = 0.0; vec3 winTint = vec3(1.0, 0.8, 0.55);
        vec3 n = normalize(vWNrm);
        if (n.y > 0.5) {
          float g = vn(vWPos.xz * 0.6) * 0.5 + vn(vWPos.xz * 3.1) * 0.5;
          diffuseColor.rgb = mix(diffuseColor.rgb, vec3(0.46, 0.45, 0.43), 0.6) * (0.8 + 0.3 * g);
        } else if (abs(n.y) < 0.5) {
          vec2 t = normalize(vec2(-n.z, n.x));
          float u = dot(vWPos.xz, t);
          float v = vWPos.y;
          float grain = vn(vec2(u, v) * 1.7) * 0.5 + vn(vec2(u * 0.25, v * 0.05) * 3.0) * 0.5;
          diffuseColor.rgb *= 0.86 + 0.2 * grain;
          if (vWin > 0.5) {
            vec2 cell; vec2 f; float frame = 0.0;
            if (vWin < 1.5)      { cell = vec2(4.2, 3.3); f = fract(vec2(u, v) / cell); winMask = step(0.3, f.x) * step(f.x, 0.68) * step(0.38, f.y) * step(f.y, 0.78); frame = step(0.27, f.x) * step(f.x, 0.71) * step(0.35, f.y) * step(f.y, 0.81) - winMask; }
            else if (vWin < 2.5) { cell = vec2(3.2, 3.6); f = fract(vec2(u, v) / cell); winMask = step(0.14, f.x) * step(f.x, 0.86) * step(0.26, f.y) * step(f.y, 0.86); frame = step(0.1, f.x) * step(f.x, 0.9) * step(0.2, f.y) * step(f.y, 0.9) - winMask; }
            else if (vWin < 3.5) { cell = vec2(1.6, 3.8); f = fract(vec2(u, v) / cell); winMask = step(0.05, f.x) * step(0.14, f.y); }
            else                 { cell = vec2(9.0, 7.0); f = fract(vec2(u, v) / cell); winMask = step(0.2, f.x) * step(f.x, 0.8) * step(0.72, f.y) * step(f.y, 0.9); }
            if (vWin > 1.5 && vWin < 2.5 && vH < 4.2) { float sf = fract(u / 6.0); winMask = step(0.06, sf) * step(sf, 0.94) * step(0.5, vH) * step(vH, 3.6); frame = 0.0; }
            vec2 id = floor(vec2(u, v) / cell);
            float floorOn = step(0.25, h21(vec2(id.y, vSeed * 31.0)));
            lit = step(0.58, h21(id + vSeed * 97.0)) * floorOn * winMask;
            winTint = vWin > 2.5 ? vec3(0.78, 0.86, 1.0) : vec3(1.0, 0.78, 0.5);
            vec3 glass = vWin > 2.5 ? mix(vec3(0.16, 0.23, 0.3), diffuseColor.rgb, 0.3) : vec3(0.1, 0.12, 0.15);
            float blind = step(0.5, h21(id * 1.7 + vSeed)) * (vWin < 2.5 ? 0.25 : 0.0);
            diffuseColor.rgb = mix(diffuseColor.rgb, glass + blind * vec3(0.5, 0.48, 0.42), winMask);
            diffuseColor.rgb *= 1.0 - frame * 0.35;
            glassy = winMask * (1.0 - blind * 2.0);
            if (vWin > 1.5) diffuseColor.rgb *= 1.0 - 0.18 * (1.0 - step(0.06, fract(v / 3.6)));
          }
          diffuseColor.rgb *= mix(0.42, 1.0, smoothstep(0.0, 3.5, vH));
          diffuseColor.rgb *= 1.0 - 0.25 * (1.0 - smoothstep(0.0, 0.6, vTop - vH));
        }`)
      .replace('#include <roughnessmap_fragment>', `#include <roughnessmap_fragment>
        roughnessFactor = mix(roughnessFactor, 0.04, glassy);`)
      .replace('#include <metalnessmap_fragment>', `#include <metalnessmap_fragment>
        metalnessFactor = mix(metalnessFactor, 0.85, glassy * step(2.5, vWin)) + glassy * 0.25;`)
      .replace('#include <emissivemap_fragment>', `#include <emissivemap_fragment>
        totalEmissiveRadiance += lit * uNight * winTint * (0.25 + 0.35 * h21(floor(vWPos.xz * 0.3) + floor(vWPos.y / 3.6)));`);
  };
  return m;
}

/* Terracotta tile roofs: rows of barrel tiles, from world position. */
function tileRoofMaterial() {
  const m = new THREE.MeshStandardMaterial({ roughness: 0.7 });
  m.onBeforeCompile = (sh) => {
    sh.vertexShader = sh.vertexShader
      .replace('#include <common>', '#include <common>\nvarying vec3 vRP;')
      .replace('#include <project_vertex>', `#include <project_vertex>
        vec4 rp = vec4(transformed, 1.0);
        #ifdef USE_INSTANCING
          rp = instanceMatrix * rp;
        #endif
        vRP = (modelMatrix * rp).xyz;`);
    sh.fragmentShader = sh.fragmentShader
      .replace('#include <common>', '#include <common>\nvarying vec3 vRP;')
      .replace('#include <color_fragment>', `#include <color_fragment>
        float rows = abs(sin(vRP.y * 9.0));
        float cols = abs(sin((vRP.x + vRP.z) * 3.2));
        diffuseColor.rgb *= 0.72 + 0.22 * rows + 0.1 * cols;`);
  };
  return m;
}

/* A shared atlas: an instance attribute picks which cell each plane shows. */
function atlasUV(mat, cols, rows) {
  mat.onBeforeCompile = (sh) => {
    sh.vertexShader = sh.vertexShader
      .replace('#include <common>', '#include <common>\nattribute vec2 aCell;')
      .replace('#include <uv_vertex>', `#include <uv_vertex>
        vec2 cellUv = vec2((uv.x + aCell.x) / ${cols.toFixed(1)}, 1.0 - ((1.0 - uv.y) + aCell.y) / ${rows.toFixed(1)});
        #ifdef USE_MAP
          vMapUv = cellUv;
        #endif
        #ifdef USE_EMISSIVEMAP
          vEmissiveMapUv = cellUv;
        #endif`);
  };
}

function makeSignAtlas() {
  const cw = 512, ch = 128, cols = 8, rows = 8;
  const c = document.createElement('canvas');
  c.width = cw * cols; c.height = ch * rows;
  const ctx = c.getContext('2d');
  const looks = [
    ['#c8102e', '#fff6d8'], ['#1d3f8a', '#ffd23f'], ['#fff6d8', '#c8102e'], ['#0e5c3b', '#ffffff'], ['#111111', '#ff4fa3'],
    ['#f2c14e', '#1b1b1b'], ['#ffffff', '#1d6fa5'], ['#5b2a86', '#7cff6b'], ['#e86a1a', '#ffffff'], ['#0a7f8c', '#fff2c6']
  ];
  const fonts = ['900 SIZEpx "Arial Black", Impact, sans-serif', 'italic 800 SIZEpx Georgia, serif', '800 SIZEpx "Trebuchet MS", system-ui, sans-serif'];
  SIGN_TEXTS.forEach((text, i) => {
    const x = (i % cols) * cw, y = Math.floor(i / cols) * ch;
    const [bg, fg] = looks[(i * 7) % looks.length];
    ctx.fillStyle = bg; ctx.fillRect(x, y, cw, ch);
    ctx.strokeStyle = fg; ctx.lineWidth = 6; ctx.strokeRect(x + 8, y + 8, cw - 16, ch - 16);
    ctx.fillStyle = fg;
    ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
    const font = fonts[i % fonts.length];
    let size = 78;
    ctx.font = font.replace('SIZE', size);
    while (ctx.measureText(text).width > cw - 50 && size > 20) { size -= 4; ctx.font = font.replace('SIZE', size); }
    ctx.shadowColor = fg; ctx.shadowBlur = 10;
    ctx.fillText(text, x + cw / 2, y + ch / 2 + 4);
    ctx.shadowBlur = 0;
  });
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  t.anisotropy = 8;
  return t;
}

/* Streetlights, poles, hydrants, benches, bins, mailboxes, signals and stop
 * signs: one merged geometry per piece, instanced per tile. */
function buildFurniture(glowMats) {
  const std = (o) => new THREE.MeshStandardMaterial({ roughness: 0.7, ...o });
  const paint = std({ vertexColors: true });
  const vc = (g, hex) => {
    const c = new THREE.Color(hex);
    const ng = g.index ? g.toNonIndexed() : g;
    const n = ng.attributes.position.count;
    const a = new Float32Array(n * 3);
    for (let i = 0; i < n; i++) { a[i * 3] = c.r; a[i * 3 + 1] = c.g; a[i * 3 + 2] = c.b; }
    ng.setAttribute('color', new THREE.BufferAttribute(a, 3));
    return ng;
  };
  const B = (w, h, d, x, y, z, hex, rz = 0) => { const g = new THREE.BoxGeometry(w, h, d); if (rz) g.rotateZ(rz); g.translate(x, y, z); return vc(g, hex); };
  const Cy = (r0, r1, h, x, y, z, hex, seg = 10, rot = null) => { const g = new THREE.CylinderGeometry(r0, r1, h, seg); if (rot === 'x') g.rotateX(Math.PI / 2); if (rot === 'z') g.rotateZ(Math.PI / 2); g.translate(x, y, z); return vc(g, hex); };
  const merge = (list) => mergeGeometries(list);

  const lampOn = std({ color: 0x2a2a28, emissive: 0xffc98a, emissiveIntensity: 0 });
  glowMats.push({ mat: lampOn, day: 0, night: 6 });
  const signalLamp = std({ color: 0x222222, emissive: 0xffffff, emissiveIntensity: 0.6, vertexColors: true });
  glowMats.push({ mat: signalLamp, day: 0.8, night: 3 });

  const stopTex = (() => {
    const c = document.createElement('canvas'); c.width = c.height = 128;
    const x = c.getContext('2d');
    const oct = (r, col) => { x.fillStyle = col; x.beginPath(); for (let k = 0; k < 8; k++) { const a = Math.PI / 8 + (k * Math.PI) / 4; x.lineTo(64 + Math.cos(a) * r, 64 + Math.sin(a) * r); } x.fill(); };
    oct(62, '#ffffff'); oct(56, '#c8102e');
    x.fillStyle = '#fff'; x.font = '900 38px Arial, sans-serif'; x.textAlign = 'center'; x.textBaseline = 'middle'; x.fillText('STOP', 64, 66);
    const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; return t;
  })();
  const stopFace = new THREE.PlaneGeometry(0.76, 0.76); stopFace.translate(0, 2.3, 0.04);

  const binColors = ['#2b2b2b', '#2a5caa', '#3f7a3a'];
  return {
    [PROP.STREETLIGHT]: [
      { geo: merge([Cy(0.09, 0.15, 9, 0, 4.5, 0, '#6f7468'), Cy(0.25, 0.3, 0.6, 0, 0.3, 0, '#5d6158'), B(2.8, 0.12, 0.12, 1.4, 8.85, 0, '#6f7468', 0.08), B(0.9, 0.22, 0.42, 2.75, 8.95, 0, '#4c4f4a')]), mat: paint, far: 1100 },
      { geo: (() => { const g = new THREE.BoxGeometry(0.75, 0.06, 0.34); g.translate(2.75, 8.82, 0); return g; })(), mat: lampOn, far: 1600, cast: false }
    ],
    [PROP.POWER_POLE]: [
      { geo: merge([Cy(0.13, 0.19, 10.6, 0, 5.3, 0, '#5a4632', 7), B(0.14, 0.14, 2.4, 0, 9.6, 0, '#4f3d2c'), Cy(0.05, 0.05, 0.25, 0, 9.78, 0.95, '#cfcfc8', 6), Cy(0.05, 0.05, 0.25, 0, 9.78, -0.95, '#cfcfc8', 6), Cy(0.05, 0.05, 0.25, 0, 10.35, 0, '#cfcfc8', 6), Cy(0.32, 0.32, 1.1, 0.4, 8.2, 0, '#8b9093', 10)]), mat: paint, far: 750 }
    ],
    [PROP.HYDRANT]: [
      { geo: merge([Cy(0.17, 0.19, 0.62, 0, 0.31, 0, '#e3b92a'), Cy(0.2, 0.2, 0.08, 0, 0.66, 0, '#d7ab1f'), Cy(0.07, 0.07, 0.42, 0, 0.42, 0, '#d7ab1f', 8, 'z'), Cy(0.07, 0.07, 0.42, 0, 0.42, 0, '#d7ab1f', 8, 'x')]), mat: paint, far: 350 }
    ],
    [PROP.BENCH]: [
      { geo: merge([B(2, 0.08, 0.5, 0, 0.45, 0, '#4e6f45'), B(2, 0.6, 0.06, 0, 0.8, -0.24, '#e9e3d2'), B(0.08, 0.45, 0.45, -0.9, 0.22, 0, '#3a3a3a'), B(0.08, 0.45, 0.45, 0.9, 0.22, 0, '#3a3a3a'), B(0.12, 2.4, 0.1, 1.4, 1.2, -0.25, '#4e6f45'), B(0.9, 0.6, 0.06, 1.4, 2.5, -0.25, '#1d6fa5')]), mat: paint, far: 400 }
    ],
    [PROP.BIN]: [
      { geo: merge([B(0.66, 1.0, 0.72, 0, 0.55, 0, '#ffffff'), B(0.7, 0.06, 0.78, 0, 1.08, 0, '#dddddd'), Cy(0.1, 0.1, 0.06, -0.3, 0.1, 0.28, '#333333', 8, 'z')]), mat: std({ vertexColors: true }), far: 300, tint: (p) => binColors[p.color || 0] }
    ],
    [PROP.MAILBOX]: [
      { geo: merge([B(0.5, 0.95, 0.55, 0, 0.62, 0, '#2a4f9c'), Cy(0.25, 0.25, 0.55, 0, 1.1, 0, '#2a4f9c', 12, 'x'), B(0.3, 0.15, 0.4, 0, 0.08, 0, '#222222')]), mat: paint, far: 300 }
    ],
    [PROP.SIGNAL]: [
      { geo: merge([Cy(0.13, 0.16, 6.6, 0, 3.3, 0, '#5f6460'), B(0.4, 1.1, 0.35, 0, 3.2, 0.2, '#d8b02a')]), mat: paint, far: 900 },
      { geo: merge([B(10, 0.16, 0.16, 5, 6.3, 0, '#5f6460'), B(0.4, 1.15, 0.38, 9.6, 5.6, 0, '#d8b02a'), B(0.4, 1.15, 0.38, 5.5, 5.6, 0, '#d8b02a')]), mat: paint, far: 900, arm: true },
      { geo: merge([B(0.26, 0.26, 0.05, 9.6, 5.95, 0.2, '#ff3a2a'), B(0.26, 0.26, 0.05, 9.6, 5.25, 0.2, '#2aff6a'), B(0.26, 0.26, 0.05, 5.5, 5.95, 0.2, '#ff3a2a'), B(0.26, 0.26, 0.05, 5.5, 5.25, 0.2, '#2aff6a')]), mat: signalLamp, far: 1100, arm: true, cast: false }
    ],
    [PROP.STOP_SIGN]: [
      { geo: merge([Cy(0.04, 0.04, 2.3, 0, 1.15, 0, '#9a9a9a', 6)]), mat: paint, far: 300 },
      { geo: stopFace, mat: std({ map: stopTex, roughness: 0.5 }), far: 300, cast: false }
    ]
  };
}

/* The big green freeway signs: a truss over the carriageways and a panel
 * for each direction with the route shield and the control city. */
function buildGantries(W, scene, glowMats) {
  if (!W.gantries.length) return;
  const n = W.gantryTexts.length, cols = 8, rows = Math.ceil(n / cols), cw = 512, ch = 256;
  const c = document.createElement('canvas');
  c.width = cw * cols; c.height = ch * rows;
  const ctx = c.getContext('2d');
  W.gantryTexts.forEach((t, i) => {
    const x = (i % cols) * cw, y = Math.floor(i / cols) * ch;
    ctx.fillStyle = '#0d6b3a'; ctx.fillRect(x, y, cw, ch);
    ctx.strokeStyle = '#ffffff'; ctx.lineWidth = 6; roundRect(ctx, x + 10, y + 10, cw - 20, ch - 20, 18); ctx.stroke();
    shield(ctx, t.kind, t.num, x + 92, y + 98, 64);
    ctx.fillStyle = '#ffffff'; ctx.textAlign = 'left'; ctx.textBaseline = 'middle';
    ctx.font = '700 40px "Arial Narrow", Arial, sans-serif';
    ctx.fillText(t.dir, x + 180, y + 70);
    let s = 54;
    ctx.font = `700 ${s}px "Arial Narrow", Arial, sans-serif`;
    while (ctx.measureText(t.dest).width > cw - 200 && s > 24) { s -= 3; ctx.font = `700 ${s}px "Arial Narrow", Arial, sans-serif`; }
    ctx.fillText(t.dest, x + 180, y + 128);
    if (t.next) {
      ctx.fillStyle = '#ffd23f'; ctx.fillRect(x + 24, y + 186, cw - 48, 46);
      ctx.fillStyle = '#111111'; ctx.font = '700 30px "Arial Narrow", Arial, sans-serif';
      ctx.fillText(`EXIT  ${t.next}`.slice(0, 30), x + 40, y + 210);
    }
  });
  const tex = new THREE.CanvasTexture(c);
  tex.colorSpace = THREE.SRGBColorSpace;
  tex.anisotropy = 8;
  const faceMat = new THREE.MeshStandardMaterial({ map: tex, emissiveMap: tex, emissive: 0xffffff, emissiveIntensity: 0.05, roughness: 0.5 });
  glowMats.push({ mat: faceMat, day: 0.05, night: 0.55 });
  atlasUV(faceMat, cols, rows);
  const steel = new THREE.MeshStandardMaterial({ color: 0x8d9296, roughness: 0.5, metalness: 0.6 });
  const legs = [], beams = [], faces = [], backs = [];
  for (const g of W.gantries) {
    const c0 = Math.cos(g.ry), s0 = Math.sin(g.ry);
    const at = (lx, lz) => [g.x + lx * c0 + lz * s0, g.z - lx * s0 + lz * c0];
    for (const sd of [-1, 1]) { const [x, z] = at((sd * g.span) / 2, 0); legs.push({ x, z, py: g.y + 4, sx: 0.7, sy: 8, sz: 0.7, ry: g.ry }); }
    beams.push({ x: g.x, z: g.z, py: g.y + 7.6, sx: g.span, sy: 0.9, sz: 1.1, ry: g.ry });
    for (const p of g.panels) {
      const [x, z] = at(p.off, p.flip ? -0.7 : 0.7);
      const ry = g.ry + (p.flip ? Math.PI : 0);
      backs.push({ x, z, py: g.y + 8.6, sx: 10.2, sy: 5.2, sz: 0.25, ry, c: '#0d6b3a' });
      const [fx, fz] = at(p.off, p.flip ? -0.86 : 0.86);
      faces.push({ x: fx, z: fz, py: g.y + 8.6, sx: 10, sy: 5, sz: 1, ry, aCell: [p.cell % cols, Math.floor(p.cell / cols)] });
    }
  }
  const mk = (items, geometry, mat, cells) => {
    const mesh = new THREE.InstancedMesh(cells ? geometry.clone() : geometry, mat, items.length);
    const m4 = new THREE.Matrix4(), q = new THREE.Quaternion(), e = new THREE.Euler();
    const cell = cells ? new Float32Array(items.length * 2) : null;
    items.forEach((it, i) => {
      e.set(0, it.ry, 0); q.setFromEuler(e);
      m4.compose(new THREE.Vector3(it.x, it.py, it.z), q, new THREE.Vector3(it.sx, it.sy, it.sz));
      mesh.setMatrixAt(i, m4);
      if (it.c) mesh.setColorAt(i, new THREE.Color(it.c));
      if (cell) { cell[i * 2] = it.aCell[0]; cell[i * 2 + 1] = it.aCell[1]; }
    });
    if (cell) mesh.geometry.setAttribute('aCell', new THREE.InstancedBufferAttribute(cell, 2));
    mesh.castShadow = true;
    mesh.computeBoundingSphere();
    scene.add(mesh);
  };
  mk(legs, new THREE.BoxGeometry(1, 1, 1), steel);
  mk(beams, new THREE.BoxGeometry(1, 1, 1), steel);
  mk(backs, new THREE.BoxGeometry(1, 1, 1), new THREE.MeshStandardMaterial({ roughness: 0.6 }));
  mk(faces, new THREE.PlaneGeometry(1, 1), faceMat, true);
}

/* Interstate (red, white and blue), US route (black on white) or California
 * (green spade) shield. */
function shield(ctx, kind, num, cx, cy, r) {
  ctx.save();
  if (kind === 'interstate') {
    ctx.fillStyle = '#ffffff';
    ctx.beginPath(); ctx.moveTo(cx - r, cy - r * 0.8); ctx.quadraticCurveTo(cx, cy - r * 1.05, cx + r, cy - r * 0.8);
    ctx.quadraticCurveTo(cx + r * 1.05, cy + r * 0.3, cx, cy + r); ctx.quadraticCurveTo(cx - r * 1.05, cy + r * 0.3, cx - r, cy - r * 0.8); ctx.fill();
    ctx.fillStyle = '#1d3f8a';
    ctx.beginPath(); ctx.moveTo(cx - r * 0.86, cy - r * 0.42); ctx.lineTo(cx + r * 0.86, cy - r * 0.42);
    ctx.quadraticCurveTo(cx + r * 0.9, cy + r * 0.32, cx, cy + r * 0.86); ctx.quadraticCurveTo(cx - r * 0.9, cy + r * 0.32, cx - r * 0.86, cy - r * 0.42); ctx.fill();
    ctx.fillStyle = '#c8102e';
    ctx.beginPath(); ctx.moveTo(cx - r * 0.86, cy - r * 0.5); ctx.quadraticCurveTo(cx, cy - r * 0.92, cx + r * 0.86, cy - r * 0.5); ctx.lineTo(cx + r * 0.86, cy - r * 0.46); ctx.lineTo(cx - r * 0.86, cy - r * 0.46); ctx.fill();
    ctx.fillStyle = '#ffffff';
  } else if (kind === 'us') {
    ctx.fillStyle = '#ffffff'; ctx.fillRect(cx - r * 0.85, cy - r * 0.9, r * 1.7, r * 1.8);
    ctx.strokeStyle = '#111111'; ctx.lineWidth = 5;
    ctx.beginPath(); ctx.moveTo(cx - r * 0.75, cy - r * 0.8); ctx.lineTo(cx + r * 0.75, cy - r * 0.8); ctx.lineTo(cx + r * 0.7, cy + r * 0.1);
    ctx.quadraticCurveTo(cx, cy + r * 0.95, cx - r * 0.7, cy + r * 0.1); ctx.closePath(); ctx.stroke();
    ctx.fillStyle = '#111111';
  } else {
    ctx.fillStyle = '#ffffff';
    ctx.beginPath(); ctx.moveTo(cx - r * 0.8, cy - r * 0.9); ctx.lineTo(cx + r * 0.8, cy - r * 0.9); ctx.lineTo(cx + r * 0.65, cy + r * 0.2); ctx.quadraticCurveTo(cx, cy + r, cx - r * 0.65, cy + r * 0.2); ctx.closePath(); ctx.fill();
    ctx.fillStyle = '#0d6b3a';
    ctx.beginPath(); ctx.moveTo(cx - r * 0.7, cy - r * 0.8); ctx.lineTo(cx + r * 0.7, cy - r * 0.8); ctx.lineTo(cx + r * 0.57, cy + r * 0.17); ctx.quadraticCurveTo(cx, cy + r * 0.88, cx - r * 0.57, cy + r * 0.17); ctx.closePath(); ctx.fill();
    ctx.fillStyle = '#ffffff';
  }
  ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
  ctx.font = `800 ${num.length > 2 ? r * 0.72 : r * 0.9}px "Arial Narrow", Arial, sans-serif`;
  ctx.fillText(num, cx, cy + r * 0.12);
  ctx.restore();
}

function roundRect(ctx, x, y, w, h, r) {
  ctx.beginPath(); ctx.moveTo(x + r, y); ctx.arcTo(x + w, y, x + w, y + h, r); ctx.arcTo(x + w, y + h, x, y + h, r);
  ctx.arcTo(x, y + h, x, y, r); ctx.arcTo(x, y, x + w, y, r); ctx.closePath();
}

function textMesh(p, glowMats) {
  const font = p.font || 64;
  const lines = String(p.text).split('\n');
  const ch = Math.max(64, Math.round(font * 1.6 * Math.max(1, lines.length > 1 ? lines.length * 0.35 : 1)));
  const c = document.createElement('canvas');
  c.width = Math.min(2048, Math.max(64, Math.round(ch * (p.sx / p.sy)))); c.height = ch;
  const ctx = c.getContext('2d');
  if (p.bg) { ctx.fillStyle = p.bg; ctx.fillRect(0, 0, c.width, c.height); }
  ctx.fillStyle = p.c;
  ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
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
  let mat;
  if (p.glow) {
    mat = new THREE.MeshStandardMaterial({ map: tex, emissiveMap: tex, emissive: 0xffffff, emissiveIntensity: 0.6, transparent: true, side: THREE.DoubleSide, depthWrite: !!p.bg, roughness: 0.5 });
    glowMats.push({ mat, day: 0.5, night: 2.4 });
  } else {
    mat = new THREE.MeshStandardMaterial({ map: tex, transparent: !p.bg, alphaTest: p.bg ? 0 : 0.4, side: THREE.DoubleSide, roughness: 0.6 });
  }
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

/* Roads: ribbons laid on the terrain with crisp markings, raised kerbs and
 * sidewalks along the boulevards, a concrete barrier down the freeways. */
function buildRoads(W, scene, aniso, lo) {
  const kinds = { freeway: [], blvd: [], local: [] };
  W.roads.forEach((r, idx) => kinds[r.kind].push([r, idx]));
  const mats = [];
  const bridgeLift = (x, z, y) => {
    for (const br of W.bridges) {
      const dx = x - br.x, dz = z - br.z, c = Math.cos(br.a), s = Math.sin(br.a);
      if (Math.abs(dx * c - dz * s) < br.span / 2 && Math.abs(dx * s + dz * c) < br.w / 2 + 3) return Math.max(y, br.y + 0.35);
    }
    return y;
  };
  /* A strip from u0 to u1 metres off the centre line, `lift` above the
   * ground; with h > 0 it is a raised block with sides. */
  const strip = (pts, u0, u1, lift, h, tile, out) => {
    let dist = 0;
    const base = out.pos.length / 3;
    const across = h > 0 ? [[u0, 0, 0], [u0, h, 0.02], [u1, h, 0.98], [u1, 0, 1]] : [[u0, 0, 0], [(u0 + u1) / 2, 0, 0.5], [u1, 0, 1]];
    for (let i = 0; i < pts.length; i++) {
      const a = pts[Math.max(0, i - 1)], b = pts[Math.min(pts.length - 1, i + 1)];
      const L = Math.hypot(b[0] - a[0], b[1] - a[1]) || 1;
      const nx = -(b[1] - a[1]) / L, nz = (b[0] - a[0]) / L;
      if (i > 0) dist += Math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]);
      for (const [u, yy, uu] of across) {
        const x = pts[i][0] + nx * u, z = pts[i][1] + nz * u;
        out.pos.push(x, bridgeLift(x, z, W.groundHeight(x, z) + lift) + yy, z);
        out.uv.push(uu, dist / tile);
      }
      if (i > 0) {
        const p = across.length, k = base + i * p;
        for (let c = 0; c < p - 1; c++) {
          const a0 = k - p + c, a1 = a0 + 1, b0 = k + c, b1 = b0 + 1;
          out.idx.push(a0, a1, b0, a1, b1, b0);
        }
      }
    }
  };
  const finish = (out, mat) => {
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.Float32BufferAttribute(out.pos, 3));
    g.setAttribute('uv', new THREE.Float32BufferAttribute(out.uv, 2));
    g.setIndex(out.idx);
    g.computeVertexNormals();
    const mesh = new THREE.Mesh(g, mat);
    mesh.receiveShadow = !lo;
    scene.add(mesh);
    return mesh;
  };
  for (const [kind, list] of Object.entries(kinds)) {
    const w = ROAD_WIDTH[kind], tile = kind === 'freeway' ? 18 : 14;
    const lift = kind === 'freeway' ? 0.42 : kind === 'blvd' ? 0.34 : 0.28;
    const out = { pos: [], uv: [], idx: [] };
    for (const [r, idx] of list) strip(resample(r.pts, 6), -w / 2, w / 2, lift + idx * 0.004, 0, tile, out);
    const tex = roadTexture(kind);
    tex.anisotropy = aniso;
    // pools of sodium light where the streetlights stand, every 46 m, alternating sides
    const pools = lampPools(kind);
    pools.repeat.set(1, tile / 92);
    const mat = new THREE.MeshStandardMaterial({ map: tex, roughness: 0.88, emissiveMap: pools, emissive: 0xffffff, emissiveIntensity: 0 });
    finish(out, mat);
    mats.push(mat);
  }
  const walkTex = sidewalkTexture();
  walkTex.anisotropy = aniso;
  const walks = { pos: [], uv: [], idx: [] };
  for (const [r, idx] of kinds.blvd) {
    const w = ROAD_WIDTH.blvd;
    for (const pts of clearOfFreeways(W, resample(r.pts, 6))) {
      strip(pts, w / 2 + 0.1, w / 2 + 3.2, 0.34 + idx * 0.004, 0.16, 6, walks);
      strip(pts, -w / 2 - 3.2, -w / 2 - 0.1, 0.34 + idx * 0.004, 0.16, 6, walks);
    }
  }
  finish(walks, new THREE.MeshStandardMaterial({ map: walkTex, roughness: 0.9 }));
  const barrier = { pos: [], uv: [], idx: [] };
  for (const [r, idx] of kinds.freeway) strip(resample(r.pts, 6), -0.35, 0.35, 0.42 + idx * 0.004, 0.9, 10, barrier);
  finish(barrier, new THREE.MeshStandardMaterial({ color: 0xb9b5aa, roughness: 0.85 })).castShadow = !lo;
  return mats;
}

function lampPools(kind) {
  const c = document.createElement('canvas');
  c.width = 64; c.height = 256;
  const ctx = c.getContext('2d');
  ctx.fillStyle = '#000'; ctx.fillRect(0, 0, 64, 256);
  const pool = (x, y, r, a) => {
    const g = ctx.createRadialGradient(x, y, 0, x, y, r);
    g.addColorStop(0, `rgba(255,170,90,${a})`); g.addColorStop(1, 'rgba(255,150,70,0)');
    ctx.fillStyle = g; ctx.fillRect(0, 0, 64, 256);
  };
  const k = kind === 'freeway' ? 0.25 : kind === 'blvd' ? 0.55 : 0.3;
  for (const y of [0, 256]) pool(10, y, 70, k);
  pool(54, 128, 70, k);
  ctx.fillStyle = `rgba(255,160,80,${k * 0.25})`; ctx.fillRect(0, 0, 64, 256);
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  t.wrapS = THREE.ClampToEdgeWrapping; t.wrapT = THREE.RepeatWrapping;
  return t;
}

/* Split a polyline into runs that keep clear of the freeways (no sidewalks
 * across the 405). */
function clearOfFreeways(W, pts) {
  const fw = W.roads.filter((r) => r.kind === 'freeway');
  const near = ([x, z]) => fw.some((r) => {
    for (let i = 0; i < r.pts.length - 1; i++) {
      const [ax, az] = r.pts[i], [bx, bz] = r.pts[i + 1];
      if (Math.max(ax, bx) + 30 < x || Math.min(ax, bx) - 30 > x || Math.max(az, bz) + 30 < z || Math.min(az, bz) - 30 > z) continue;
      const dx = bx - ax, dz = bz - az, L2 = dx * dx + dz * dz;
      const t = Math.max(0, Math.min(1, ((x - ax) * dx + (z - az) * dz) / L2));
      if (Math.hypot(ax + dx * t - x, az + dz * t - z) < ROAD_WIDTH.freeway / 2 + 5) return true;
    }
    return false;
  });
  const runs = [];
  let cur = [];
  for (const p of pts) {
    if (near(p)) { if (cur.length > 1) runs.push(cur); cur = []; } else cur.push(p);
  }
  if (cur.length > 1) runs.push(cur);
  return runs;
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

function noiseFill(ctx, w, h, base, amt, seed) {
  ctx.fillStyle = base; ctx.fillRect(0, 0, w, h);
  const img = ctx.getImageData(0, 0, w, h);
  const rnd = mulberry(seed);
  for (let k = 0; k < img.data.length; k += 4) {
    const n = (rnd() - 0.5) * amt;
    img.data[k] += n; img.data[k + 1] += n; img.data[k + 2] += n;
  }
  ctx.putImageData(img, 0, 0);
}

function roadTexture(kind) {
  const c = document.createElement('canvas');
  c.width = 256; c.height = 512;
  const ctx = c.getContext('2d');
  noiseFill(ctx, 256, 512, kind === 'freeway' ? '#3b3d40' : kind === 'blvd' ? '#434548' : '#525458', 22, kind.length * 99);
  // patches, cracks, and oil down the middle of each lane
  const rnd = mulberry(kind.length * 7);
  for (let k = 0; k < 14; k++) {
    ctx.fillStyle = `rgba(${rnd() < 0.5 ? '20,20,22' : '90,90,92'},${0.08 + rnd() * 0.12})`;
    ctx.fillRect(rnd() * 230, rnd() * 490, 10 + rnd() * 50, 20 + rnd() * 90);
  }
  ctx.strokeStyle = 'rgba(15,15,15,0.5)'; ctx.lineWidth = 1;
  for (let k = 0; k < 10; k++) { ctx.beginPath(); let x = rnd() * 256, y = rnd() * 512; ctx.moveTo(x, y); for (let s = 0; s < 6; s++) { x += (rnd() - 0.5) * 30; y += rnd() * 25; ctx.lineTo(x, y); } ctx.stroke(); }
  const line = (u, w, color, dash) => { ctx.fillStyle = color; ctx.fillRect(u * 256 - w / 2, 0, w, 512 * (dash || 1)); };
  if (kind === 'freeway') {
    line(0.035, 4, '#e2c04a'); line(0.965, 4, '#f0f0ea');
    line(0.47, 3, '#e2c04a'); line(0.53, 3, '#e2c04a');
    for (const u of [0.18, 0.33, 0.67, 0.82]) line(u, 3, '#efefe8', 0.3);
    ctx.fillStyle = 'rgba(0,0,0,0.12)'; for (const u of [0.1, 0.25, 0.4, 0.6, 0.75, 0.9]) ctx.fillRect(u * 256 - 6, 0, 12, 512);
  } else if (kind === 'blvd') {
    line(0.488, 3, '#e3c04a'); line(0.512, 3, '#e3c04a');
    for (const u of [0.25, 0.75]) line(u, 3, '#ececec', 0.35);
    ctx.fillStyle = 'rgba(0,0,0,0.1)'; for (const u of [0.13, 0.37, 0.63, 0.87]) ctx.fillRect(u * 256 - 6, 0, 12, 512);
  } else line(0.5, 3, '#d9b84a', 0.5);
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  t.wrapS = THREE.ClampToEdgeWrapping; t.wrapT = THREE.RepeatWrapping;
  return t;
}

function sidewalkTexture() {
  const c = document.createElement('canvas');
  c.width = 128; c.height = 128;
  const ctx = c.getContext('2d');
  noiseFill(ctx, 128, 128, '#b9b4a8', 16, 5);
  ctx.fillStyle = 'rgba(60,55,50,0.35)';
  ctx.fillRect(0, 0, 128, 2); ctx.fillRect(0, 64, 128, 2);
  ctx.fillStyle = '#9d988d'; ctx.fillRect(0, 0, 10, 128);
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  t.wrapS = THREE.ClampToEdgeWrapping; t.wrapT = THREE.RepeatWrapping;
  return t;
}

function makeDetailTexture() {
  const S = 256;
  const c = document.createElement('canvas');
  c.width = c.height = S;
  const ctx = c.getContext('2d');
  const img = ctx.createImageData(S, S);
  const rnd = mulberry(3);
  const grids = [0, 1, 2].map(() => Array.from({ length: 33 * 33 }, () => rnd()));
  for (let y = 0; y < S; y++) {
    for (let x = 0; x < S; x++) {
      const k = (y * S + x) * 4;
      for (let ch = 0; ch < 3; ch++) {
        const sc = [32, 16, 8][ch];
        const gx = (x / S) * sc, gy = (y / S) * sc, ix = Math.floor(gx), iy = Math.floor(gy), fx = gx - ix, fy = gy - iy;
        const g = (i, j) => grids[ch][(j % sc) * 33 + (i % sc)];
        const v = (g(ix, iy) * (1 - fx) + g(ix + 1, iy) * fx) * (1 - fy) + (g(ix, iy + 1) * (1 - fx) + g(ix + 1, iy + 1) * fx) * fy;
        img.data[k + ch] = (v * 0.65 + rnd() * 0.35) * 255;
      }
      img.data[k + 3] = 255;
    }
  }
  ctx.putImageData(img, 0, 0);
  const t = new THREE.CanvasTexture(c);
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  return t;
}





function makeStars() {
  const n = 2500, p = new Float32Array(n * 3), rnd = mulberry(99);
  for (let i = 0; i < n; i++) {
    const y = rnd() * 0.95 + 0.05, a = rnd() * Math.PI * 2, r = Math.sqrt(1 - y * y);
    p[i * 3] = Math.cos(a) * r * 8500; p[i * 3 + 1] = y * 8500; p[i * 3 + 2] = Math.sin(a) * r * 8500;
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(p, 3));
  const pts = new THREE.Points(g, new THREE.PointsMaterial({ color: 0xffffff, size: 2.2, sizeAttenuation: false, transparent: true, opacity: 0, fog: false, depthWrite: false }));
  pts.frustumCulled = false;
  return pts;
}

function makeWaterNormals() {
  const S = 512;
  const c = document.createElement('canvas');
  c.width = c.height = S;
  const ctx = c.getContext('2d');
  const img = ctx.createImageData(S, S);
  const T = (Math.PI * 2) / S;
  const h = (x, y) =>
    Math.sin(x * T * 3 + Math.sin(y * T * 2) * 1.5) * 0.5 + Math.sin(y * T * 5 + x * T) * 0.32 +
    Math.sin((x + y) * T * 7) * 0.2 + Math.sin((x * 2 - y) * T * 11) * 0.12 + Math.sin((x + 3 * y) * T * 17) * 0.07;
  for (let y = 0; y < S; y++) {
    for (let x = 0; x < S; x++) {
      const dx = h(x + 1, y) - h(x - 1, y), dy = h(x, y + 1) - h(x, y - 1);
      const nx = -dx * 3, ny = -dy * 3, l = Math.hypot(nx, ny, 1);
      const k = (y * S + x) * 4;
      img.data[k] = ((nx / l) * 0.5 + 0.5) * 255;
      img.data[k + 1] = ((ny / l) * 0.5 + 0.5) * 255;
      img.data[k + 2] = ((1 / l) * 0.5 + 0.5) * 255;
      img.data[k + 3] = 255;
    }
  }
  ctx.putImageData(img, 0, 0);
  return new THREE.CanvasTexture(c);
}

function shadeHex(h, k) {
  return `#${new THREE.Color(h).multiplyScalar(k).getHexString()}`;
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
