/*
 * Camera: orbit around the globe with zoom levels that the rest of the view uses to
 * decide how much detail to show. Phone gestures come from OrbitControls (one finger
 * turns, pinch zooms).
 */
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/OrbitControls.js';
import { latLonToVector3 } from './globe.js';

export const MIN_DIST = 1.025, MAX_DIST = 4.5;

export function createCamera(globe, domElement) {
  const controls = new OrbitControls(globe.camera, domElement);
  controls.enablePan = false;
  controls.enableDamping = true;
  controls.dampingFactor = 0.12;
  controls.minDistance = MIN_DIST;
  controls.maxDistance = MAX_DIST;
  controls.zoomSpeed = 0.9;
  controls.touches = { ONE: THREE.TOUCH.ROTATE, TWO: THREE.TOUCH.DOLLY_ROTATE };

  let flying = null;

  const api = {
    controls,
    distance() { return globe.camera.position.length(); },
    /** 'globe' | 'continent' | 'state' | 'province' */
    zoomLevel() {
      const d = api.distance() - 1;
      if (d > 1.6) return 'globe';
      if (d > 0.55) return 'continent';
      if (d > 0.18) return 'state';
      return 'province';
    },
    flyTo(lat, lon, distance) {
      const target = latLonToVector3(lat, lon, distance || Math.min(api.distance(), 1.45));
      flying = { from: globe.camera.position.clone(), to: target, t: 0 };
    },
    update(dt) {
      // slower turning when close in
      const d = api.distance() - 1;
      controls.rotateSpeed = Math.max(0.03, Math.min(1.0, d * 0.5));
      if (flying) {
        flying.t = Math.min(1, flying.t + dt / 0.9);
        const k = flying.t < 0.5 ? 2 * flying.t * flying.t : 1 - Math.pow(-2 * flying.t + 2, 2) / 2;
        const from = flying.from, to = flying.to;
        const dir = from.clone().normalize().lerp(to.clone().normalize(), k).normalize();
        const len = from.length() + (to.length() - from.length()) * k;
        globe.camera.position.copy(dir.multiplyScalar(len));
        if (flying.t >= 1) flying = null;
      }
      controls.update();
    },
  };
  return api;
}
