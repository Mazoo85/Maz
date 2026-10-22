/*
 * The atmosphere shared by every map: a scattering sky with a little smog,
 * the sun (and the moon at night) with soft shadows that follow you, light
 * from the sky itself as an environment map, fog, stars, and bloom.
 *
 * makeAtmosphere(scene, renderer, { quality, uniforms }) → { setTime(hours),
 * update(dt, time, camera, focus), render(camera), setSize(w, h), sun,
 * onTime: [fn(night, elev)], onUpdate: [fn(dt, time, camera, focus)] }.
 */

import * as THREE from '../vendor/three.module.min.js';
import { Sky } from '../vendor/jsm/objects/Sky.js';
import { EffectComposer } from '../vendor/jsm/postprocessing/EffectComposer.js';
import { RenderPass } from '../vendor/jsm/postprocessing/RenderPass.js';
import { UnrealBloomPass } from '../vendor/jsm/postprocessing/UnrealBloomPass.js';
import { OutputPass } from '../vendor/jsm/postprocessing/OutputPass.js';

export function makeAtmosphere(scene, renderer, opts = {}) {
  const quality = opts.quality || 'high';
  const hi = quality === 'high', lo = quality === 'low';
  const uniforms = opts.uniforms || { uNight: { value: 0 }, uTime: { value: 0 } };
  const fogNear = opts.fogNear ?? 400, fogFar = opts.fogFar ?? 5200, groundAt = opts.groundAt || (() => 0);
  const onTime = [], onUpdate = [];
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
  const envGround = new THREE.Mesh(new THREE.SphereGeometry(900, 16, 8, 0, Math.PI * 2, Math.PI / 2, Math.PI / 2), new THREE.MeshBasicMaterial({ color: 0x7a6a55, side: THREE.BackSide }));
  envScene.add(envGround);
  const envVeil = new THREE.Mesh(new THREE.SphereGeometry(950, 16, 8), new THREE.MeshBasicMaterial({ color: 0x050a18, transparent: true, opacity: 0, side: THREE.BackSide, depthWrite: false }));
  envScene.add(envVeil);
  const pmrem = new THREE.PMREMGenerator(renderer);
  let envRT = null, envAge = 99, envNight = -1;
  const envSun = new THREE.Vector3(9, 9, 9);

  const hemi = new THREE.HemisphereLight(0xdfe6ee, 0x9a8466, 0.3);
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
    [0.1, C('#ffc88a'), 2.6, 0.3, C('#dcc0a0'), 0.5],
    [0.3, C('#fff0d8'), 3.6, 0.28, C('#d6d3c8'), 0.55],
    [1.0, C('#fff8ec'), 3.9, 0.28, C('#d2d4cc'), 0.6]
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
    hemi.groundColor.set(0x9a8466).lerp(C('#ff9a4a'), night * 0.7);
    hemi.color.set(0xdfe6ee).lerp(C('#4a5a90'), night);
    scene.fog.color.copy(tc[1].copy(a[4]).lerp(b[4], t));
    scene.environmentIntensity = a[5] + (b[5] - a[5]) * t;
    // at night the light is the moon: dim and blue
    if (sunDir.y < -0.05) { sun.color.set('#8fa6e0'); sun.intensity = 0.35 * night; }
    uniforms.uNight.value = night;
    stars.material.opacity = night;
    moon.material.opacity = night;
    if (bloom) { bloom.strength = 0.16 + night * 0.4; bloom.threshold = 0.9 - night * 0.12; }
    nightSky.material.opacity = night * 0.88;
    for (const fn of onTime) fn(night, elev);
    return { elev, night };
  }

  /* Rebuild the sky's reflection now and then as the sun moves. */
  function updateEnv() {
    if (envRT && envAge < 1.5 && envSun.distanceTo(sunDir) < 0.03 && Math.abs(envNight - nightNow) < 0.05) return;
    envNight = nightNow;
    envAge = 0;
    envSun.copy(sunDir);
    envGround.material.color.set(0x7a6a55).multiplyScalar(Math.max(0.08, 1 - nightNow * 0.9));
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
    sky.position.copy(camera.position);
    stars.position.copy(camera.position);
    nightSky.position.copy(camera.position);
    moon.position.copy(camera.position).addScaledVector(sunDir, -8000);
    moon.lookAt(camera.position);
    // the haze thins as you climb above the ground
    const high = Math.max(0, camera.position.y - groundAt(camera.position.x, camera.position.z) - 120);
    scene.fog.near = fogNear + high * 2;
    scene.fog.far = fogFar + high * 6;
    const lightDir = sunDir.y > -0.05 ? sunDir : tmpV.copy(sunDir).negate().setY(0.6).normalize();
    // snap the shadow box to its texels so shadows don't crawl as you move
    const step = (box * 2) / sm;
    const fx = Math.round(focus.x / step) * step, fz = Math.round(focus.z / step) * step, fy = Math.max(focus.y, 0);
    sun.position.set(fx + lightDir.x * 1400, fy + Math.max(lightDir.y, 0.1) * 1400, fz + lightDir.z * 1400);
    sun.target.position.set(fx, fy, fz);
    for (const fn of onUpdate) fn(dt, time, camera, focus);
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


  return { setTime, update, render, setSize, sun, uniforms, onTime, onUpdate };
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

function mulberry(a) {
  return () => {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}
