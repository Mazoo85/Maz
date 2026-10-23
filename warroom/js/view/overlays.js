/*
 * Things that stand on the globe: the Blender-made counters for armies, placed on
 * the surface and scaled with the camera so they stay readable. One instanced mesh
 * per glTF primitive; the Body primitive is tinted with the owner's colour.
 */
import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/GLTFLoader.js';
import { latLonToVector3 } from './globe.js';
import { hexToRgb } from './mapmodes.js';

const UP = new THREE.Vector3(0, 1, 0);

export async function loadModels(baseUrl, names) {
  const manifest = await (await fetch(baseUrl + 'manifest.json')).json();
  const loader = new GLTFLoader();
  const out = {};
  await Promise.all(names.map(async (name) => {
    const m = manifest.models[name];
    if (!m) throw new Error(`models/manifest.json has no model "${name}"`);
    const gltf = await loader.loadAsync(baseUrl + m.file);
    const prims = [];
    gltf.scene.traverse((o) => { if (o.isMesh) prims.push({ geometry: o.geometry, material: o.material }); });
    out[name] = { prims, height: m.height, radius: m.radius };
  }));
  return out;
}

export function createOverlays(globe, data, models) {
  const group = new THREE.Group();
  globe.scene.add(group);
  const light = new THREE.DirectionalLight(0xffffff, 2.2);
  light.position.set(1, 2, 2);
  globe.scene.add(light);
  globe.scene.add(new THREE.AmbientLight(0xffffff, 0.9));

  let meshes = [];
  let labels = [];
  const placements = [];   // { lat, lon, color, count }

  function makeLabel(text) {
    const size = 64;
    const labelCanvas = document.createElement('canvas');
    labelCanvas.width = size; labelCanvas.height = size;
    const ctx = labelCanvas.getContext('2d');
    ctx.clearRect(0, 0, size, size);
    ctx.font = 'bold 34px system-ui, sans-serif';
    ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
    ctx.lineWidth = 6; ctx.strokeStyle = 'rgba(0,0,0,0.8)'; ctx.strokeText(text, size / 2, size / 2);
    ctx.fillStyle = '#fff'; ctx.fillText(text, size / 2, size / 2);
    const tex = new THREE.CanvasTexture(labelCanvas);
    tex.needsUpdate = true;
    const mat = new THREE.SpriteMaterial({ map: tex, depthTest: false, transparent: true });
    const sprite = new THREE.Sprite(mat);
    sprite.renderOrder = 10;
    return sprite;
  }

  const api = {
    /** armies: { tag: [{ state, divisions }] } from state.land.armies */
    setArmies(state) {
      for (const m of meshes) { group.remove(m); m.dispose(); }
      for (const l of labels) { group.remove(l); l.material.map.dispose(); l.material.dispose(); }
      meshes = []; labels = []; placements.length = 0;
      for (const [tag, list] of Object.entries(state.land.armies)) {
        const c = state.countries[tag];
        const rgb = hexToRgb(c.color);
        for (const a of list) {
          const st = data.stateById[a.state];
          const p = data.provinces[st.capital_province];
          placements.push({ lat: p.lat, lon: p.lon, color: new THREE.Color(rgb[0] / 255, rgb[1] / 255, rgb[2] / 255), count: a.divisions });
        }
      }
      const model = models.division;
      const count = placements.length;
      const dummy = new THREE.Object3D();
      for (const prim of model.prims) {
        const mat = prim.material.clone();
        const tinted = prim.material.name === 'Body';
        const mesh = new THREE.InstancedMesh(prim.geometry, mat, count);
        mesh.frustumCulled = false;
        placements.forEach((pl, i) => {
          const pos = latLonToVector3(pl.lat, pl.lon, 1.0);
          dummy.position.copy(pos);
          dummy.quaternion.setFromUnitVectors(UP, pos.clone().normalize());
          dummy.scale.setScalar(1);
          dummy.updateMatrix();
          mesh.setMatrixAt(i, dummy.matrix);
          if (tinted) mesh.setColorAt(i, pl.color);
        });
        mesh.instanceMatrix.needsUpdate = true;
        if (tinted && mesh.instanceColor) mesh.instanceColor.needsUpdate = true;
        group.add(mesh); meshes.push(mesh);
      }
      placements.forEach((pl) => {
        const s = makeLabel(String(pl.count));
        group.add(s); labels.push(s);
      });
    },

    /** Rescale with the camera so counters stay a readable size on screen. */
    update(cameraDistance) {
      const d = cameraDistance - 1;
      const scale = Math.max(0.004, Math.min(0.06, d * 0.035));
      const visible = d < 2.2;
      const dummy = new THREE.Object3D();
      for (const m of meshes) {
        m.visible = visible;
        if (!visible) continue;
        placements.forEach((pl, i) => {
          const pos = latLonToVector3(pl.lat, pl.lon, 1.0);
          dummy.position.copy(pos);
          dummy.quaternion.setFromUnitVectors(UP, pos.clone().normalize());
          dummy.scale.setScalar(scale);
          dummy.updateMatrix();
          m.setMatrixAt(i, dummy.matrix);
        });
        m.instanceMatrix.needsUpdate = true;
      }
      labels.forEach((l, i) => {
        l.visible = visible;
        const pl = placements[i];
        l.position.copy(latLonToVector3(pl.lat, pl.lon, 1.0 + scale * 0.9));
        l.scale.setScalar(scale * 1.4);
      });
    },
  };
  return api;
}
