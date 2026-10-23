/*
 * The globe. One sphere; the province-id texture tells the shader which province
 * every pixel belongs to, and small lookup textures give each province its colour
 * and flags for the current map mode. Colours are painted once into an
 * equirectangular colour map (with borders) whenever something changes, and the
 * sphere samples that with mipmaps so the world looks smooth from orbit and crisp
 * up close. Picking reads the raw id texture through a 1×1 render target.
 */
import * as THREE from 'three';

const LUT = 128;                 // 128×128 = 16,384 province slots
const PAINT_W = 4096, PAINT_H = 2048;

export function latLonToVector3(lat, lon, r = 1) {
  const phi = ((lon + 180) / 360) * Math.PI * 2;
  const theta = ((90 - lat) / 180) * Math.PI;
  return new THREE.Vector3(-r * Math.cos(phi) * Math.sin(theta), r * Math.cos(theta), r * Math.sin(phi) * Math.sin(theta));
}

export function vector3ToLatLon(v) {
  const r = v.length();
  const theta = Math.acos(Math.max(-1, Math.min(1, v.y / r)));
  let phi = Math.atan2(v.z, -v.x);
  if (phi < 0) phi += Math.PI * 2;
  return { lat: 90 - (theta / Math.PI) * 180, lon: (phi / (Math.PI * 2)) * 360 - 180 };
}

/** Sub-solar point for an hour index (UTC), good enough for a day/night terminator. */
export function sunDirection(hour, dayOfYear) {
  const decl = -23.44 * Math.cos(((2 * Math.PI) / 365) * (dayOfYear + 10));
  const lon = 180 - (hour % 24) * 15;
  return latLonToVector3(decl, lon, 1);
}

const PAINT_FRAG = /* glsl */`
precision highp float;
uniform sampler2D idMap;
uniform sampler2D colorLUT;
uniform sampler2D infoLUT;
uniform sampler2D flagLUT;
uniform vec2 texel;
uniform float borderLevel;   // 0 = country borders only … 2 = every province border
varying vec2 vUv;

int decodeId(vec2 uv) {
  vec3 c = texture2D(idMap, uv).rgb * 255.0 + 0.5;
  return int(c.r) + int(c.g) * 256 + int(c.b) * 65536;
}
vec2 lutUv(int id) {
  float x = float(id - (id / ${LUT}) * ${LUT});
  float y = float(id / ${LUT});
  return vec2((x + 0.5) / ${LUT}.0, (y + 0.5) / ${LUT}.0);
}
void main() {
  int id = decodeId(vUv);
  vec2 lu = lutUv(id);
  vec4 col = texture2D(colorLUT, lu);
  vec4 info = texture2D(infoLUT, lu);        // r,g = state id; b,a = owner index
  vec4 flags = texture2D(flagLUT, lu);       // r = highlight, g = dim, b = outline
  float country = 0.0, state = 0.0, prov = 0.0;
  for (int i = 0; i < 4; i++) {
    vec2 d = i == 0 ? vec2(texel.x, 0.0) : i == 1 ? vec2(-texel.x, 0.0) : i == 2 ? vec2(0.0, texel.y) : vec2(0.0, -texel.y);
    int n = decodeId(vUv + d);
    if (n != id) {
      vec4 ni = texture2D(infoLUT, lutUv(n));
      prov = 1.0;
      if (ni.rg != info.rg) state = 1.0;
      if (ni.ba != info.ba) country = 1.0;
    }
  }
  // a second ring for thicker country borders
  if (country < 0.5) {
    for (int i = 0; i < 4; i++) {
      vec2 d = i == 0 ? vec2(2.0 * texel.x, 0.0) : i == 1 ? vec2(-2.0 * texel.x, 0.0) : i == 2 ? vec2(0.0, 2.0 * texel.y) : vec2(0.0, -2.0 * texel.y);
      int n = decodeId(vUv + d);
      if (n != id && texture2D(infoLUT, lutUv(n)).ba != info.ba) country = 0.7;
    }
  }
  vec3 rgb = col.rgb;
  rgb = mix(rgb, rgb * 0.45, flags.g);
  rgb = mix(rgb, mix(rgb, vec3(1.0, 0.95, 0.7), 0.45), flags.r);
  float dark = 0.0;
  if (borderLevel > 1.5) dark = max(dark, prov * 0.22);
  if (borderLevel > 0.5) dark = max(dark, state * 0.4);
  dark = max(dark, country * 0.75);
  rgb *= (1.0 - dark);
  gl_FragColor = vec4(rgb, 1.0);
}`;

const PAINT_VERT = /* glsl */`
varying vec2 vUv;
void main() { vUv = uv; gl_Position = vec4(position.xy, 0.0, 1.0); }`;

const GLOBE_VERT = /* glsl */`
varying vec2 vUv;
varying vec3 vNormal;
varying vec3 vView;
void main() {
  vUv = uv;
  vNormal = normalize(mat3(modelMatrix) * normal);
  vec4 wp = modelMatrix * vec4(position, 1.0);
  vView = normalize(cameraPosition - wp.xyz);
  gl_Position = projectionMatrix * viewMatrix * wp;
}`;

const GLOBE_FRAG = /* glsl */`
precision highp float;
uniform sampler2D colorMap;
uniform sampler2D idMap;
uniform vec3 sunDir;
uniform float night;         // 0..1 how dark the night side gets
uniform float hoverId;
uniform float selectedId;
varying vec2 vUv;
varying vec3 vNormal;
varying vec3 vView;
int decodeId(vec2 uv) {
  vec3 c = texture2D(idMap, uv).rgb * 255.0 + 0.5;
  return int(c.r) + int(c.g) * 256 + int(c.b) * 65536;
}
void main() {
  vec3 rgb = texture2D(colorMap, vUv).rgb;
  int id = decodeId(vUv);
  if (id == int(hoverId + 0.5)) rgb = mix(rgb, vec3(1.0), 0.18);
  if (id == int(selectedId + 0.5)) rgb = mix(rgb, vec3(1.0, 0.9, 0.5), 0.35);
  float sun = dot(normalize(vNormal), normalize(sunDir));
  float light = mix(1.0 - night, 1.0, smoothstep(-0.12, 0.18, sun));
  float rim = pow(1.0 - max(dot(normalize(vNormal), normalize(vView)), 0.0), 3.0);
  rgb = rgb * light + vec3(0.25, 0.45, 0.8) * rim * 0.35;
  gl_FragColor = vec4(rgb, 1.0);
}`;

const PICK_FRAG = /* glsl */`
precision highp float;
uniform sampler2D idMap;
varying vec2 vUv;
void main() { gl_FragColor = vec4(texture2D(idMap, vUv).rgb, 1.0); }`;

export function createGlobe({ canvas, idImage, provinceCount }) {
  const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: false, powerPreference: 'high-performance' });
  renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
  renderer.setClearColor(0x05070c, 1);
  renderer.outputColorSpace = THREE.LinearSRGBColorSpace;

  const scene = new THREE.Scene();
  const camera = new THREE.PerspectiveCamera(42, 1, 0.005, 50);
  camera.position.set(0, 0.6, 2.6);

  const idMap = new THREE.Texture(idImage);
  idMap.magFilter = THREE.NearestFilter;
  idMap.minFilter = THREE.NearestFilter;
  idMap.generateMipmaps = false;
  idMap.colorSpace = THREE.NoColorSpace;
  idMap.flipY = true;
  idMap.needsUpdate = true;

  const makeLut = () => {
    const t = new THREE.DataTexture(new Uint8Array(LUT * LUT * 4), LUT, LUT, THREE.RGBAFormat);
    t.magFilter = THREE.NearestFilter; t.minFilter = THREE.NearestFilter; t.generateMipmaps = false;
    t.needsUpdate = true;
    return t;
  };
  const colorLUT = makeLut(), infoLUT = makeLut(), flagLUT = makeLut();

  const paintTarget = new THREE.WebGLRenderTarget(PAINT_W, PAINT_H, {
    minFilter: THREE.LinearMipmapLinearFilter, magFilter: THREE.LinearFilter, generateMipmaps: true,
    format: THREE.RGBAFormat, type: THREE.UnsignedByteType, depthBuffer: false, stencilBuffer: false,
  });
  paintTarget.texture.anisotropy = Math.min(8, renderer.capabilities.getMaxAnisotropy());
  const paintScene = new THREE.Scene();
  const paintCam = new THREE.OrthographicCamera(-1, 1, 1, -1, 0, 1);
  const paintMat = new THREE.ShaderMaterial({
    vertexShader: PAINT_VERT, fragmentShader: PAINT_FRAG, depthTest: false, depthWrite: false,
    uniforms: {
      idMap: { value: idMap }, colorLUT: { value: colorLUT }, infoLUT: { value: infoLUT }, flagLUT: { value: flagLUT },
      texel: { value: new THREE.Vector2(1 / idImage.width, 1 / idImage.height) }, borderLevel: { value: 2 },
    },
  });
  paintScene.add(new THREE.Mesh(new THREE.PlaneGeometry(2, 2), paintMat));

  const geometry = new THREE.SphereGeometry(1, 192, 96);
  const globeMat = new THREE.ShaderMaterial({
    vertexShader: GLOBE_VERT, fragmentShader: GLOBE_FRAG,
    uniforms: {
      colorMap: { value: paintTarget.texture }, idMap: { value: idMap },
      sunDir: { value: new THREE.Vector3(1, 0, 0) }, night: { value: 0.55 }, hoverId: { value: 0 }, selectedId: { value: 0 },
    },
  });
  const sphere = new THREE.Mesh(geometry, globeMat);
  scene.add(sphere);

  // picking
  const pickScene = new THREE.Scene();
  const pickMat = new THREE.ShaderMaterial({ vertexShader: GLOBE_VERT, fragmentShader: PICK_FRAG, uniforms: { idMap: { value: idMap } } });
  pickScene.add(new THREE.Mesh(geometry, pickMat));
  const pickTarget = new THREE.WebGLRenderTarget(1, 1, { depthBuffer: true });
  const pickBuf = new Uint8Array(4);

  let dirty = true;
  const api = {
    renderer, scene, camera, sphere, colorLUT, infoLUT, flagLUT, provinceCount,

    /** colors: Uint8Array of 4 bytes per province id (index 0 unused). */
    setColors(colors) { colorLUT.image.data.set(colors.subarray(0, LUT * LUT * 4)); colorLUT.needsUpdate = true; dirty = true; },
    setInfo(info) { infoLUT.image.data.set(info.subarray(0, LUT * LUT * 4)); infoLUT.needsUpdate = true; dirty = true; },
    setFlags(flags) { flagLUT.image.data.set(flags.subarray(0, LUT * LUT * 4)); flagLUT.needsUpdate = true; dirty = true; },
    setBorderLevel(level) { if (paintMat.uniforms.borderLevel.value !== level) { paintMat.uniforms.borderLevel.value = level; dirty = true; } },
    setHover(id) { globeMat.uniforms.hoverId.value = id || 0; },
    setSelected(id) { globeMat.uniforms.selectedId.value = id || 0; },
    setSun(hour, dayOfYear) { globeMat.uniforms.sunDir.value.copy(sunDirection(hour, dayOfYear)); },
    setNight(v) { globeMat.uniforms.night.value = v; },

    resize() {
      const w = canvas.clientWidth || window.innerWidth, h = canvas.clientHeight || window.innerHeight;
      renderer.setSize(w, h, false);
      camera.aspect = w / h; camera.updateProjectionMatrix();
    },

    repaint() {
      renderer.setRenderTarget(paintTarget);
      renderer.render(paintScene, paintCam);
      renderer.setRenderTarget(null);
      dirty = false;
    },

    render() {
      if (dirty) api.repaint();
      renderer.render(scene, camera);
    },

    /** Province id under a CSS pixel, or 0 for space. */
    pick(px, py) {
      const w = renderer.domElement.clientWidth, h = renderer.domElement.clientHeight;
      const dpr = renderer.getPixelRatio();
      camera.setViewOffset(w * dpr, h * dpr, Math.floor(px * dpr), Math.floor(py * dpr), 1, 1);
      renderer.setRenderTarget(pickTarget);
      renderer.setClearColor(0x000000, 1);
      renderer.clear();
      renderer.render(pickScene, camera);
      renderer.readRenderTargetPixels(pickTarget, 0, 0, 1, 1, pickBuf);
      renderer.setRenderTarget(null);
      renderer.setClearColor(0x05070c, 1);
      camera.clearViewOffset();
      const id = pickBuf[0] | (pickBuf[1] << 8) | (pickBuf[2] << 16);
      return id > provinceCount ? 0 : id;
    },

    /** Where the camera is looking on the sphere (lat/lon), or null if it is looking past it. */
    lookingAt() {
      const ray = new THREE.Raycaster();
      ray.setFromCamera(new THREE.Vector2(0, 0), camera);
      const hit = ray.intersectObject(sphere, false)[0];
      return hit ? vector3ToLatLon(hit.point) : null;
    },
  };
  api.resize();
  return api;
}
