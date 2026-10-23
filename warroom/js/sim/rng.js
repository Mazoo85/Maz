/*
 * Seeded random numbers for the simulation: xoshiro128** on four 32-bit words.
 *
 * The state is a plain array of four numbers so it lives inside the game state
 * and survives JSON save/load. Every subsystem gets its own stream
 * (state.rng.weather, state.rng.events, ...) so adding a roll in one place never
 * changes another's results. Nothing in sim/ may use Math.random.
 */

function hashSeed(seed) {
  // FNV-1a over the seed's string form, then splitmix32 to spread it into four words.
  const str = String(seed);
  let h = 0x811c9dc5;
  for (let i = 0; i < str.length; i++) {
    h ^= str.charCodeAt(i);
    h = Math.imul(h, 0x01000193) >>> 0;
  }
  const out = [];
  for (let i = 0; i < 4; i++) {
    h = (h + 0x9e3779b9) >>> 0;
    let z = h;
    z = Math.imul(z ^ (z >>> 16), 0x85ebca6b) >>> 0;
    z = Math.imul(z ^ (z >>> 13), 0xc2b2ae35) >>> 0;
    z = (z ^ (z >>> 16)) >>> 0;
    out.push(z);
  }
  if (out.every((w) => w === 0)) out[0] = 1;
  return out;
}

export function seedRng(seed) {
  return hashSeed(seed);
}

/** Derive an independent stream from a seed and a name, e.g. seedStream(seed, 'weather'). */
export function seedStream(seed, name) {
  return hashSeed(String(seed) + '/' + name);
}

function rotl(x, k) {
  return ((x << k) | (x >>> (32 - k))) >>> 0;
}

export function nextU32(s) {
  const result = Math.imul(rotl(Math.imul(s[1], 5) >>> 0, 7), 9) >>> 0;
  const t = (s[1] << 9) >>> 0;
  s[2] = (s[2] ^ s[0]) >>> 0;
  s[3] = (s[3] ^ s[1]) >>> 0;
  s[1] = (s[1] ^ s[2]) >>> 0;
  s[0] = (s[0] ^ s[3]) >>> 0;
  s[2] = (s[2] ^ t) >>> 0;
  s[3] = rotl(s[3], 11);
  return result;
}

/** A float in [0, 1). */
export function nextFloat(s) {
  return (nextU32(s) >>> 8) / 16777216;
}

/** An integer in 0..n-1. */
export function nextInt(s, n) {
  return Math.floor(nextFloat(s) * n);
}

export function pick(s, array) {
  return array[nextInt(s, array.length)];
}

/** Pick an index from a list of non-negative weights. */
export function pickWeighted(s, weights) {
  let total = 0;
  for (const w of weights) total += w;
  let r = nextFloat(s) * total;
  for (let i = 0; i < weights.length; i++) {
    r -= weights[i];
    if (r < 0) return i;
  }
  return weights.length - 1;
}
