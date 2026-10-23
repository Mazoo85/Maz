/*
 * Map modes: each one fills the colour lookup (4 bytes per province id).
 */
const TERRAIN_COLORS = {
  plains: [168, 196, 118], forest: [66, 122, 66], hills: [168, 140, 92], mountain: [128, 118, 118], urban: [196, 88, 88],
  jungle: [34, 112, 54], marsh: [102, 150, 140], desert: [228, 208, 150], wasteland: [238, 240, 248], ocean: [38, 66, 112], lake: [78, 128, 196],
};
const WEATHER_COLORS = {
  clear: [230, 220, 150], rain: [90, 130, 200], storm: [60, 70, 140], snow: [240, 244, 255], blizzard: [200, 210, 240],
  sandstorm: [220, 170, 90], mud: [130, 100, 60], frost: [170, 210, 240],
};
const SEA = [38, 66, 112], LAKE = [78, 128, 196], WASTE = [228, 230, 238];

export const MAP_MODES = ['political', 'terrain', 'states', 'regions', 'weather', 'vp'];

export function hexToRgb(hex) {
  const n = parseInt(hex.slice(1), 16);
  return [(n >> 16) & 255, (n >> 8) & 255, n & 255];
}

function hashColor(n, s = 0.55, l = 0.55) {
  const h = ((n * 0.618033988749895) % 1) * 360;
  return hslToRgb(h, s, l);
}

function hslToRgb(h, s, l) {
  const c = (1 - Math.abs(2 * l - 1)) * s, x = c * (1 - Math.abs(((h / 60) % 2) - 1)), m = l - c / 2;
  let r = 0, g = 0, b = 0;
  if (h < 60) [r, g, b] = [c, x, 0]; else if (h < 120) [r, g, b] = [x, c, 0]; else if (h < 180) [r, g, b] = [0, c, x];
  else if (h < 240) [r, g, b] = [0, x, c]; else if (h < 300) [r, g, b] = [x, 0, c]; else [r, g, b] = [c, 0, x];
  return [Math.round((r + m) * 255), Math.round((g + m) * 255), Math.round((b + m) * 255)];
}

function mix(a, b, k) { return [a[0] + (b[0] - a[0]) * k, a[1] + (b[1] - a[1]) * k, a[2] + (b[2] - a[2]) * k]; }

/** Fill `out` (Uint8Array, 4 bytes per id) for the given mode. */
export function paintColors(mode, state, data, out) {
  const n = data.provinceCount;
  const ownerColor = {};
  for (const [tag, c] of Object.entries(state.countries)) ownerColor[tag] = hexToRgb(c.color);
  for (let id = 1; id <= n; id++) {
    const p = data.provinces[id];
    let c;
    if (p.type === 'sea') c = mode === 'regions' ? mix(SEA, hashColor(p.region, 0.4, 0.4), 0.6) : mode === 'weather' ? mix(SEA, WEATHER_COLORS[state.regions[p.region].weather], 0.5) : SEA;
    else if (p.type === 'lake') c = LAKE;
    else if (p.terrain === 'wasteland' && mode !== 'terrain') c = WASTE;
    else {
      const st = state.states[p.state];
      switch (mode) {
        case 'terrain': c = TERRAIN_COLORS[p.terrain]; break;
        case 'states': c = hashColor(p.state); break;
        case 'regions': c = hashColor(p.region * 7 + 3, 0.5, 0.5); break;
        case 'weather': c = WEATHER_COLORS[state.regions[p.region].weather]; break;
        case 'vp': c = p.vp >= 15 ? [220, 60, 60] : p.vp >= 5 ? [230, 150, 70] : p.vp >= 1 ? [230, 210, 120] : [120, 130, 120]; break;
        default: {
          c = ownerColor[st.controller] || [128, 128, 128];
          if (st.controller !== st.owner) c = mix(c, ownerColor[st.owner] || c, 0.35);
        }
      }
    }
    const o = id * 4;
    out[o] = c[0]; out[o + 1] = c[1]; out[o + 2] = c[2]; out[o + 3] = 255;
  }
  return out;
}

/** The info lookup: state id (16 bit) and owner index (16 bit) per province, for border drawing. */
export function paintInfo(state, data, out) {
  const tags = Object.keys(state.countries).sort();
  const ownerIndex = {};
  tags.forEach((t, i) => { ownerIndex[t] = i + 1; });
  for (let id = 1; id <= data.provinceCount; id++) {
    const p = data.provinces[id];
    const o = id * 4;
    let sid = 0, own = 0;
    if (p.type === 'land') { sid = p.state; own = ownerIndex[state.states[p.state].controller] || 0; }
    else if (p.type === 'sea') { sid = 0xffff; own = 0xfffe; }
    else if (p.type === 'lake') { sid = 0xfffe; own = 0xfffd; }
    out[o] = sid & 255; out[o + 1] = (sid >> 8) & 255; out[o + 2] = own & 255; out[o + 3] = (own >> 8) & 255;
  }
  return out;
}

/** Highlight a set of province ids (brighten) and optionally dim everything else. */
export function paintFlags(ids, dimOthers, count, out) {
  out.fill(0);
  if (dimOthers) for (let id = 1; id <= count; id++) out[id * 4 + 1] = 255;
  for (const id of ids) { out[id * 4] = 255; out[id * 4 + 1] = 0; }
  return out;
}
