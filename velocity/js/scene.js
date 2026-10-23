/*
 * VELOCITY — the 3D world.
 *
 * Builds Harbor Loop and the cars with Three.js and draws each frame from the
 * car states. It reads car state and never changes it. Low-poly and
 * flat-shaded on purpose: one hemisphere light, one sun, no shadow maps, the
 * whole city in one instanced draw call, so a mid-range phone holds 60 fps.
 */
(function (root) {
  'use strict';

  const PALETTE = [0x5a3a6e, 0x7a4a7a, 0x3f2f5c, 0x8a5a6a, 0x2f2645, 0x6b4a86, 0x4a3d7a, 0x8c4f5f];

  function seeded(seed) {
    let s = seed;
    return () => { s = (s * 16807) % 2147483647; return (s - 1) / 2147483646; };
  }

  // Accumulates coloured quads into one BufferGeometry.
  function Builder(THREE) {
    this.THREE = THREE; this.pos = []; this.col = []; this.idx = []; this.n = 0;
  }
  Builder.prototype.quad = function (a, b, c, d, color) {
    // a,b = near left/right; c,d = far left/right. Two triangles, both windings so it is visible from above and below.
    const col = new this.THREE.Color(color);
    for (const v of [a, b, c, d]) { this.pos.push(v[0], v[1], v[2]); this.col.push(col.r, col.g, col.b); }
    const n = this.n;
    this.idx.push(n, n + 2, n + 1, n + 1, n + 2, n + 3);
    this.n += 4;
  };
  Builder.prototype.mesh = function (material) {
    const g = new this.THREE.BufferGeometry();
    g.setAttribute('position', new this.THREE.Float32BufferAttribute(this.pos, 3));
    g.setAttribute('color', new this.THREE.Float32BufferAttribute(this.col, 3));
    g.setIndex(this.idx);
    g.computeVertexNormals();
    return new this.THREE.Mesh(g, material);
  };

  function create(opts) {
    const THREE = opts.THREE || root.THREE;
    const track = opts.track;
    const canvas = opts.canvas;
    const mobile = !!opts.mobile;
    const rand = seeded(7);

    const renderer = new THREE.WebGLRenderer({ canvas, antialias: !mobile, powerPreference: 'high-performance' });
    renderer.setPixelRatio(Math.min(root.devicePixelRatio || 1, mobile ? 1.5 : 2));
    const scene = new THREE.Scene();
    scene.fog = new THREE.Fog(0x4a2050, 220, 1100);
    const camera = new THREE.PerspectiveCamera(72, 1, 0.5, 3000);
    scene.add(new THREE.HemisphereLight(0xffd0a0, 0x3a2448, 1.9));
    const fill = new THREE.DirectionalLight(0xff9a70, 0.6);
    fill.position.set(500, 200, -400);
    scene.add(fill);
    const sun = new THREE.DirectionalLight(0xffb070, 1.8);
    sun.position.set(-400, 250, 700);
    scene.add(sun);

    const lit = new THREE.MeshLambertMaterial({ vertexColors: true, side: THREE.DoubleSide });
    const unlit = new THREE.MeshBasicMaterial({ vertexColors: true, side: THREE.DoubleSide });
    const pts = track.points, N = pts.length;
    const edge = (p, side, extra) => {   // a point on the road edge, side -1 left / +1 right
      const half = p.w / 2 + (extra || 0);
      return [p.x - p.dz * half * side, p.rampHeight, p.z + p.dx * half * side];
    };
    const lift = (v, dy) => [v[0], v[1] + dy, v[2]];

    // --- Sky, sun and ground ---
    {
      const g = new THREE.SphereGeometry(2400, 24, 12);
      const colors = [];
      const top = new THREE.Color(0x160c33), mid = new THREE.Color(0x6a2a6a), hor = new THREE.Color(0xff8a3d);
      const p = g.attributes.position;
      for (let i = 0; i < p.count; i++) {
        const y = p.getY(i) / 2400;
        const c = y > 0.25 ? top.clone().lerp(mid, 1 - (y - 0.25) / 0.75) : hor.clone().lerp(mid, Math.max(0, y) / 0.25);
        colors.push(c.r, c.g, c.b);
      }
      g.setAttribute('color', new THREE.Float32BufferAttribute(colors, 3));
      scene.add(new THREE.Mesh(g, new THREE.MeshBasicMaterial({ vertexColors: true, side: THREE.BackSide, fog: false })));
      scene.sky = scene.children[scene.children.length - 1];

      const cv = document.createElement('canvas'); cv.width = cv.height = 128;
      const cx = cv.getContext('2d');
      const grad = cx.createRadialGradient(64, 64, 10, 64, 64, 64);
      grad.addColorStop(0, 'rgba(255,230,170,1)'); grad.addColorStop(0.35, 'rgba(255,160,80,0.9)'); grad.addColorStop(1, 'rgba(255,120,60,0)');
      cx.fillStyle = grad; cx.fillRect(0, 0, 128, 128);
      const sprite = new THREE.Sprite(new THREE.SpriteMaterial({ map: new THREE.CanvasTexture(cv), fog: false, transparent: true, depthWrite: false }));
      sprite.scale.set(700, 700, 1); sprite.position.set(300, 90, 2000);
      scene.add(sprite); scene.sunSprite = sprite;

      const ground = new THREE.Mesh(new THREE.PlaneGeometry(6000, 6000), new THREE.MeshLambertMaterial({ color: 0x241a30 }));
      ground.rotation.x = -Math.PI / 2; ground.position.y = -0.6; scene.add(ground);
    }

    // --- Road, dashes, kerbs, walls, tunnel, water ---
    {
      const road = new Builder(THREE), dash = new Builder(THREE), kerb = new Builder(THREE);
      const wall = new Builder(THREE), neon = new Builder(THREE), water = new Builder(THREE);
      for (let i = 0; i < N; i++) {
        const a = pts[i], b = pts[(i + 1) % N];
        const asphalt = a.tunnel ? 0x2b2b36 : 0x3a3a44;
        road.quad(edge(a, -1), edge(a, 1), edge(b, -1), edge(b, 1), asphalt);
        if (i % 4 < 2) dash.quad(lift(edge(a, -1, -a.w / 2 - 0.15), 0.02), lift(edge(a, 1, -a.w / 2 - 0.15), 0.02), lift(edge(b, -1, -b.w / 2 - 0.15), 0.02), lift(edge(b, 1, -b.w / 2 - 0.15), 0.02), 0xd8d8e0);
        const kc = (i >> 1) % 2 ? 0xe03030 : 0xf0f0f0;
        for (const side of [-1, 1]) {
          kerb.quad(lift(edge(a, side, -0.7), 0.03), lift(edge(a, side), 0.03), lift(edge(b, side, -0.7), 0.03), lift(edge(b, side), 0.03), kc);
          const h = a.tunnel ? 6 : 1.1;
          const wc = a.tunnel ? 0x3a3a4c : (side === 1 && a.water ? 0x6a6a78 : 0x8a8a98);
          wall.quad(edge(a, side), lift(edge(a, side), h), edge(b, side), lift(edge(b, side), h), wc);
          if (a.tunnel) {
            const nc = a.control % 2 ? 0xff2d95 : 0x19e6ff;
            neon.quad(lift(edge(a, side, -0.05), 3.0), lift(edge(a, side, -0.05), 3.3), lift(edge(b, side, -0.05), 3.0), lift(edge(b, side, -0.05), 3.3), nc);
          }
        }
        if (a.tunnel) wall.quad(lift(edge(a, -1), 6), lift(edge(a, 1), 6), lift(edge(b, -1), 6), lift(edge(b, 1), 6), 0x2a2a38);
        if (a.water) {
          const far = (p) => [p.x - p.dz * 400, -0.4, p.z + p.dx * 400];
          water.quad(lift(edge(a, 1, 2), -0.4), far(a), lift(edge(b, 1, 2), -0.4), far(b), 0x1d5a9a);
          wall.quad(lift(edge(a, 1, 2), -0.6), lift(edge(a, 1, 2), 0.0), lift(edge(b, 1, 2), -0.6), lift(edge(b, 1, 2), 0.0), 0x5a4a40);
        }
      }
      scene.add(road.mesh(lit), dash.mesh(unlit), kerb.mesh(lit), wall.mesh(lit), neon.mesh(unlit));
      const w = water.mesh(new THREE.MeshLambertMaterial({ vertexColors: true, emissive: 0x0a2a4a, side: THREE.DoubleSide }));
      scene.add(w);
    }

    // --- The city: one instanced box ---
    {
      const placements = [];
      const tryPlace = (p, side, offset, sx, h, sz, color) => {
        const half = p.w / 2;
        const cx = p.x - p.dz * side * (half + offset + Math.max(sx, sz) / 2);
        const cz = p.z + p.dx * side * (half + offset + Math.max(sx, sz) / 2);
        const n = track.nearest(cx, cz);
        if (Math.abs(n.lateral) < n.w / 2 + 3 + Math.max(sx, sz) * 0.72) return;
        placements.push({ x: cx, z: cz, sx, h, sz, color });
      };
      for (let i = 0; i < N; i += 7) {
        const p = pts[i];
        if (p.tunnel) continue;
        for (const side of [-1, 1]) {
          if (side === 1 && p.water) continue;
          const r = rand();
          tryPlace(p, side, 4 + rand() * 10, 8 + rand() * 14, 6 + r * r * 50, 8 + rand() * 14, PALETTE[Math.floor(rand() * PALETTE.length)]);
        }
      }
      for (let i = 0; i < N; i += 9) {
        const p = pts[i];
        for (const side of [-1, 1]) {
          if (side === 1 && p.water) continue;
          const r = rand();
          tryPlace(p, side, 40 + rand() * 90, 14 + rand() * 20, 25 + r * 100, 14 + rand() * 20, PALETTE[Math.floor(rand() * PALETTE.length)]);
        }
      }
      const box = new THREE.BoxGeometry(1, 1, 1);
      const city = new THREE.InstancedMesh(box, new THREE.MeshLambertMaterial({ color: 0xffffff }), placements.length);
      const m = new THREE.Matrix4(), q = new THREE.Quaternion(), s = new THREE.Vector3(), v = new THREE.Vector3();
      placements.forEach((b, k) => {
        m.compose(v.set(b.x, b.h / 2 - 0.5, b.z), q, s.set(b.sx, b.h, b.sz));
        city.setMatrixAt(k, m);
        const c = new THREE.Color(b.color); c.offsetHSL(0, 0, (rand() - 0.5) * 0.12);
        city.setColorAt(k, c);
      });
      city.instanceMatrix.needsUpdate = true;
      if (city.instanceColor) city.instanceColor.needsUpdate = true;
      scene.add(city);
    }

    // --- Cars ---
    const carMeshes = new Map();
    function makeCar(color) {
      const g = new THREE.Group();
      const bodyMat = new THREE.MeshLambertMaterial({ color });
      const body = new THREE.Mesh(new THREE.BoxGeometry(4.2, 0.8, 2.0), bodyMat); body.position.y = 0.75; g.add(body);
      const nose = new THREE.Mesh(new THREE.BoxGeometry(1.2, 0.5, 1.8), bodyMat); nose.position.set(2.3, 0.6, 0); g.add(nose);
      const cabin = new THREE.Mesh(new THREE.BoxGeometry(2.0, 0.65, 1.6), new THREE.MeshLambertMaterial({ color: 0x1a1a24 })); cabin.position.set(-0.3, 1.45, 0); g.add(cabin);
      const wheelGeo = new THREE.CylinderGeometry(0.42, 0.42, 0.35, 10); wheelGeo.rotateX(Math.PI / 2);
      const wheelMat = new THREE.MeshLambertMaterial({ color: 0x141418 });
      g.wheels = [];
      for (const [x, z] of [[1.4, 1.05], [1.4, -1.05], [-1.4, 1.05], [-1.4, -1.05]]) {
        const w = new THREE.Mesh(wheelGeo, wheelMat); w.position.set(x, 0.42, z); g.add(w); g.wheels.push(w);
      }
      const lamp = new THREE.MeshBasicMaterial({ color: 0xfff2c0 });
      for (const z of [0.6, -0.6]) { const l = new THREE.Mesh(new THREE.BoxGeometry(0.1, 0.25, 0.4), lamp); l.position.set(2.92, 0.7, z); g.add(l); }
      const tail = new THREE.MeshBasicMaterial({ color: 0xff2020 });
      for (const z of [0.7, -0.7]) { const l = new THREE.Mesh(new THREE.BoxGeometry(0.1, 0.2, 0.5), tail); l.position.set(-2.12, 0.8, z); g.add(l); }
      g.flames = [];
      const flameMat = new THREE.MeshBasicMaterial({ color: 0x4df0ff, transparent: true, opacity: 0.85 });
      for (const z of [0.5, -0.5]) {
        const f = new THREE.Mesh(new THREE.ConeGeometry(0.22, 1.6, 8), flameMat);
        f.rotation.z = Math.PI / 2; f.position.set(-2.9, 0.6, z); f.visible = false; g.add(f); g.flames.push(f);
      }
      const shadow = new THREE.Mesh(new THREE.CircleGeometry(2.6, 16), new THREE.MeshBasicMaterial({ color: 0x000000, transparent: true, opacity: 0.35, depthWrite: false }));
      shadow.rotation.x = -Math.PI / 2; shadow.scale.set(1, 0.55, 1);
      g.shadow = shadow;
      return g;
    }

    function setCars(cars) {
      for (const [, m] of carMeshes) { scene.remove(m); scene.remove(m.shadow); }
      carMeshes.clear();
      for (const car of cars) {
        const m = makeCar(car.color);
        scene.add(m); scene.add(m.shadow);
        carMeshes.set(car, m);
      }
    }

    // --- Sparks ---
    const SPARKS = 90;
    const sparkPos = new Float32Array(SPARKS * 3), sparkVel = new Float32Array(SPARKS * 3), sparkLife = new Float32Array(SPARKS);
    const sparkGeo = new THREE.BufferGeometry();
    sparkGeo.setAttribute('position', new THREE.BufferAttribute(sparkPos, 3));
    const sparks = new THREE.Points(sparkGeo, new THREE.PointsMaterial({ color: 0xffc070, size: 0.35, transparent: true, depthWrite: false }));
    scene.add(sparks);
    let sparkNext = 0;
    function spawnSparks(car) {
      const fx = Math.cos(car.heading), fz = Math.sin(car.heading);
      const side = car.lateral > 0 ? 1 : -1;
      const rx = -fz * side, rz = fx * side;
      for (let k = 0; k < 3; k++) {
        const i = sparkNext; sparkNext = (sparkNext + 1) % SPARKS;
        sparkPos[i * 3] = car.x + rx * 1.1; sparkPos[i * 3 + 1] = car.y + 0.4; sparkPos[i * 3 + 2] = car.z + rz * 1.1;
        sparkVel[i * 3] = -fx * Math.random() * 8 + rx * Math.random() * 2;
        sparkVel[i * 3 + 1] = 2 + Math.random() * 5;
        sparkVel[i * 3 + 2] = -fz * Math.random() * 8 + rz * Math.random() * 2;
        sparkLife[i] = 0.4 + Math.random() * 0.4;
      }
    }
    function updateSparks(dt) {
      for (let i = 0; i < SPARKS; i++) {
        if (sparkLife[i] <= 0) { sparkPos[i * 3 + 1] = -100; continue; }
        sparkLife[i] -= dt;
        sparkVel[i * 3 + 1] -= 20 * dt;
        sparkPos[i * 3] += sparkVel[i * 3] * dt; sparkPos[i * 3 + 1] += sparkVel[i * 3 + 1] * dt; sparkPos[i * 3 + 2] += sparkVel[i * 3 + 2] * dt;
      }
      sparkGeo.attributes.position.needsUpdate = true;
    }

    // --- Camera ---
    const camPos = new THREE.Vector3(0, 5, 0), camLook = new THREE.Vector3(1, 1, 0);
    let fov = 72, started = false;

    function resize() {
      const w = canvas.clientWidth || root.innerWidth, h = canvas.clientHeight || root.innerHeight;
      renderer.setSize(w, h, false);
      camera.aspect = w / h; camera.updateProjectionMatrix();
    }

    function render(dt, view) {
      const player = view.player;
      for (const car of view.cars || []) {
        const m = carMeshes.get(car); if (!m) continue;
        m.position.set(car.x, car.y, car.z);
        const fx = Math.cos(car.heading), fz = Math.sin(car.heading);
        const vR = car.vx * -fz + car.vz * fx;
        if (car.wrecked) {
          const t = 1 - car.wreckTimer / 2;
          m.rotation.set(Math.sin(t * 9) * 0.3, -car.heading + t * 7, Math.sin(t * 6) * 0.5);
          m.position.y = car.y + Math.max(0, Math.sin(t * Math.PI)) * 1.5;
        } else {
          m.rotation.set(0, -car.heading, Math.max(-0.12, Math.min(0.12, -vR * 0.015)));
        }
        const spin = car.speed * dt / 0.42;
        for (const w of m.wheels) w.rotation.z -= spin;
        for (const f of m.flames) { f.visible = car.nitroActive; f.scale.set(0.8 + Math.random() * 0.5, 1, 1); }
        m.shadow.position.set(car.x, (car.airborne ? 0 : car.y) + 0.03, car.z);
        m.shadow.rotation.z = -car.heading;
        if (car.sparks) spawnSparks(car);
      }
      updateSparks(dt);

      if (player) {
        const fx = Math.cos(player.heading), fz = Math.sin(player.heading);
        const sp = player.speed;
        const back = 7.5 + sp * 0.04, up = 3.0 + sp * 0.012;
        const want = new THREE.Vector3(player.x - fx * back, player.y + up, player.z - fz * back);
        const look = new THREE.Vector3(player.x + fx * 8, player.y + 1.2, player.z + fz * 8);
        if (!started) { camPos.copy(want); camLook.copy(look); started = true; }
        const k = player.wrecked ? 0.5 : 6;
        camPos.lerp(want, 1 - Math.exp(-k * dt));
        camLook.lerp(look, 1 - Math.exp(-10 * dt));
        const shake = (player.nitroActive ? 0.1 : 0) + (player.sparks ? 0.12 : 0) + (player.bumped ? 0.3 : 0);
        camera.position.set(camPos.x + (Math.random() - 0.5) * shake, camPos.y + (Math.random() - 0.5) * shake, camPos.z + (Math.random() - 0.5) * shake);
        camera.lookAt(camLook);
        const wantFov = 70 + (player.nitroActive ? 14 : 0) + sp * 0.1;
        fov += (wantFov - fov) * (1 - Math.exp(-5 * dt));
        camera.fov = fov; camera.updateProjectionMatrix();
      }
      scene.sky.position.copy(camera.position);
      scene.sunSprite.position.set(camera.position.x + 300, 90, camera.position.z + 2000);
      renderer.render(scene, camera);
    }

    resize();
    return { resize, setCars, render, renderer, scene, camera };
  }

  root.VelocityScene = { create };
})(typeof self !== 'undefined' ? self : this);
