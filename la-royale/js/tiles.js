/*
 * Surface tiles baked in Blender (tools/blender_tiles.py): a seamless colour
 * tile and a bump (normal) tile for each material LA is built from. They're
 * packed into two texture arrays and sampled by surfaceDetail() in render.js
 * in place of its procedural patterns; until they load (or if they can't),
 * the procedural patterns stand in.
 */
import * as THREE from '../vendor/three.module.min.js';

/* The layers, in the order the bake writes them, with the size of ground or
 * wall one tile covers. tests/real.test.js checks this against tiles.json. */
export const TILE_LAYERS = [
  ['stucco', 2.0], ['plaster', 2.0], ['brick', 1.6], ['siding', 2.0], ['block', 2.0], ['clay', 2.0], ['shingle', 2.0],
  ['metal', 2.0], ['sidewalk', 3.0], ['asphalt', 4.0], ['lawn', 2.0], ['dryGrass', 3.0], ['dirt', 3.0]
];

/* Which tile each surface kind wears, and how rough it is.
 * (kinds as in SURFACE_GLSL: 4 concrete takes the sidewalk tile on the
 * ground only; walls of it keep the board-formed procedural pattern) */
const KIND_TILES = {
  0: ['stucco', 0.94], 1: ['plaster', 0.85], 2: ['brick', 0.92], 3: ['siding', 0.8], 4: ['sidewalk', 0.9],
  5: ['clay', 0.72], 6: ['shingle', 0.95], 7: ['metal', 0.5], 10: ['lawn', 0.97], 12: ['asphalt', 0.9],
  13: ['dirt', 0.98], 14: ['block', 0.9], 15: ['dryGrass', 0.97]
};

export const TILE_UNIFORMS = {
  uTileC: { value: null }, uTileN: { value: null }, uTileOn: { value: 0 }
};

/* Hand the shared tile uniforms to a shader that includes SURFACE_GLSL. */
export function tileUniforms(sh) { Object.assign(sh.uniforms, TILE_UNIFORMS); }

const layerOf = (name) => TILE_LAYERS.findIndex(([n]) => n === name);
const f = (x) => x.toFixed(2);

/* GLSL: tileOf(kind) → the layer, the metres a tile covers and the roughness. */
export const TILE_GLSL = `
uniform sampler2DArray uTileC;
uniform sampler2DArray uTileN;
uniform float uTileOn;
bool tileOf(int kind, out float layer, out float metres, out float rough) {
${Object.entries(KIND_TILES).map(([k, [name, rough]]) => {
  const L = layerOf(name);
  const cond = k === '4' ? `kind == 4 && sFlat > 0.5` : `kind == ${k}`;
  return `  if (${cond}) { layer = ${L}.0; metres = ${f(TILE_LAYERS[L][1])}; rough = ${f(rough)}; return true; }`;
}).join('\n')}
  return false;
}`;

function loadImage(url) {
  return new Promise((resolve, reject) => {
    const img = new Image();
    img.onload = () => resolve(img);
    img.onerror = () => reject(new Error(`could not load ${url}`));
    img.src = url;
  });
}

function arrayTexture(data, size, count) {
  const t = new THREE.DataArrayTexture(data, size, size, count);
  t.format = THREE.RGBAFormat;
  t.type = THREE.UnsignedByteType;
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  t.minFilter = THREE.LinearMipmapLinearFilter;
  t.magFilter = THREE.LinearFilter;
  t.generateMipmaps = true;
  t.anisotropy = 8;
  t.needsUpdate = true;
  return t;
}

/* Load the tiles and switch the shaders over to them. Never throws: on any
 * failure the procedural surfaces simply stay. `size` downsamples (for Low). */
export async function loadTiles(base = 'textures/', size = 512) {
  try {
    const n = TILE_LAYERS.length, px = size * size * 4;
    const colour = new Uint8Array(px * n), normal = new Uint8Array(px * n);
    const canvas = document.createElement('canvas');
    canvas.width = canvas.height = size;
    const ctx = canvas.getContext('2d', { willReadFrequently: true });
    const imgs = await Promise.all(TILE_LAYERS.flatMap(([name]) => [loadImage(`${base}${name}_c.jpg`), loadImage(`${base}${name}_n.jpg`)]));
    for (let i = 0; i < n; i++) {
      for (const [img, out] of [[imgs[2 * i], colour], [imgs[2 * i + 1], normal]]) {
        ctx.drawImage(img, 0, 0, size, size);
        out.set(ctx.getImageData(0, 0, size, size).data, i * px);
      }
    }
    TILE_UNIFORMS.uTileC.value = arrayTexture(colour, size, n);
    TILE_UNIFORMS.uTileN.value = arrayTexture(normal, size, n);
    TILE_UNIFORMS.uTileOn.value = 1;
    return true;
  } catch (e) {
    console.warn('surface tiles unavailable, using procedural surfaces:', e);
    return false;
  }
}
