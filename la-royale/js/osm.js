/*
 * Real Los Angeles from OpenStreetMap: turning OSM elements into the game's
 * compact tile records. Pure functions, no DOM — used by tools/build-tiles.mjs
 * at build time and by the tests.
 *
 * Coordinates are metres on a local plane centred on REAL.lat0/lon0:
 * x east, z south (three.js convention), at true 1:1 scale.
 *
 * Most LA buildings in OSM came from the LA County building import
 * (LARIAC), which carries a measured height for every footprint; where a
 * height is missing it is taken from building:levels, then estimated from
 * the building's type and size.
 */

/* The first true-scale area: Hollywood to Downtown. */
export const REAL = {
  name: 'Hollywood to Downtown',
  lat0: 34.0800, lon0: -118.3000,
  south: 34.0300, north: 34.1350, west: -118.3600, east: -118.2300,
  tile: 500                                  // metres per streamed tile
};

const M_LAT = 110574, M_LON = 111320;
export function projector(lat0 = REAL.lat0, lon0 = REAL.lon0) {
  const kx = M_LON * Math.cos((lat0 * Math.PI) / 180);
  return {
    toXZ: (lat, lon) => [(lon - lon0) * kx, (lat0 - lat) * M_LAT],
    toLatLon: (x, z) => [lat0 - z / M_LAT, lon0 + x / kx]
  };
}

/* ------------------------------------------------------------ buildings */

export const BTYPE = {
  house: 0, apartments: 1, commercial: 2, retail: 3, office: 4, industrial: 5, warehouse: 6,
  garage: 7, civic: 8, church: 9, school: 10, hospital: 11, hotel: 12, parking: 13, roof: 14, other: 15
};
const TYPE_ALIASES = {
  house: 'house', detached: 'house', residential: 'house', semidetached_house: 'house', bungalow: 'house', terrace: 'house',
  apartments: 'apartments', dormitory: 'apartments',
  commercial: 'commercial', retail: 'retail', supermarket: 'retail', kiosk: 'retail',
  office: 'office', industrial: 'industrial', warehouse: 'warehouse', manufacture: 'industrial',
  garage: 'garage', garages: 'garage', carport: 'roof', shed: 'garage',
  civic: 'civic', public: 'civic', government: 'civic', fire_station: 'civic', train_station: 'civic', transportation: 'civic',
  church: 'church', cathedral: 'church', chapel: 'church', temple: 'church', synagogue: 'church', mosque: 'church', religious: 'church',
  school: 'school', university: 'school', college: 'school', kindergarten: 'school',
  hospital: 'hospital', hotel: 'hotel', parking: 'parking', roof: 'roof', stadium: 'civic', grandstand: 'roof'
};
export const ROOF = { flat: 0, gabled: 1, hipped: 2, pyramidal: 3, dome: 4, skillion: 5, round: 6 };
export const MATERIAL = { stucco: 0, plaster: 1, brick: 2, wood: 3, concrete: 4, glass: 5, stone: 6, metal: 7 };

const num = (v) => {
  if (v === undefined || v === null) return null;
  const m = String(v).replace(',', '.').match(/-?\d+(\.\d+)?/);
  if (!m) return null;
  let n = parseFloat(m[0]);
  if (/ft|'/.test(String(v))) n *= 0.3048;
  return Number.isFinite(n) ? n : null;
};

export function buildingType(tags) {
  const b = tags.building === 'yes' || !tags.building ? (tags.amenity === 'place_of_worship' ? 'church' : tags.shop ? 'retail' : tags.office ? 'office' : tags.tourism === 'hotel' ? 'hotel' : 'other') : tags.building;
  return BTYPE[TYPE_ALIASES[b] || 'other'];
}

/* Height above ground of the roof top, and of the bottom (for parts that
 * float, like a canopy or an upper storey over a driveway). */
export function buildingHeights(tags, type, area) {
  const levels = num(tags['building:levels']);
  const minLevel = num(tags['building:min_level']);
  let h = num(tags.height);
  if (h === null && levels !== null) h = levels * (type === BTYPE.house ? 3.0 : 3.6) + (num(tags['roof:height']) || 0);
  if (h === null) {
    // no data: what that kind of LA building usually is
    h = {
      [BTYPE.house]: 5.2, [BTYPE.apartments]: area > 900 ? 13 : 9, [BTYPE.garage]: 3, [BTYPE.roof]: 4,
      [BTYPE.commercial]: 6.5, [BTYPE.retail]: 6, [BTYPE.office]: 14, [BTYPE.industrial]: 8, [BTYPE.warehouse]: 9,
      [BTYPE.church]: 12, [BTYPE.school]: 9, [BTYPE.hospital]: 20, [BTYPE.hotel]: 18, [BTYPE.civic]: 10, [BTYPE.parking]: 12
    }[type] ?? (area > 2000 ? 10 : 6);
  }
  let mh = num(tags.min_height);
  if (mh === null && minLevel !== null) mh = minLevel * 3.4;
  return { h: Math.max(2.4, Math.min(h, 400)), mh: Math.max(0, mh || 0), levels: levels ?? null };
}

export function roofShape(tags, type, area) {
  const s = tags['roof:shape'];
  if (s && ROOF[s] !== undefined) return ROOF[s];
  if (s === 'gambrel' || s === 'saltbox' || s === 'mansard') return ROOF.hipped;
  if (s === 'onion' || s === 'half-dome') return ROOF.dome;
  // LA houses are mostly pitched; anything big is flat
  if (type === BTYPE.house && area < 400) return area < 160 ? ROOF.gabled : ROOF.hipped;
  return ROOF.flat;
}

export function material(tags, type, h) {
  const m = (tags['building:material'] || tags['building:facade:material'] || '').toLowerCase();
  if (/brick/.test(m)) return MATERIAL.brick;
  if (/glass/.test(m)) return MATERIAL.glass;
  if (/wood|timber/.test(m)) return MATERIAL.wood;
  if (/concrete|cement/.test(m)) return MATERIAL.concrete;
  if (/stone|granite|marble|sandstone|limestone/.test(m)) return MATERIAL.stone;
  if (/metal|steel|alumin/.test(m)) return MATERIAL.metal;
  if (/plaster|stucco/.test(m)) return MATERIAL.stucco;
  if (h > 60) return MATERIAL.glass;
  if (type === BTYPE.warehouse || type === BTYPE.industrial || type === BTYPE.parking) return MATERIAL.concrete;
  return MATERIAL.stucco;
}

const NAMED = {
  white: 0xf2f0ea, black: 0x222222, grey: 0x8c8c8c, gray: 0x8c8c8c, silver: 0xbfc3c6, red: 0xa8432f, brown: 0x7a5a42,
  beige: 0xd8c8a8, tan: 0xc9b08a, yellow: 0xe8d27a, orange: 0xd98a4a, pink: 0xe8b4b8, blue: 0x5d7fa8, green: 0x6f8f6a, cream: 0xefe6cf
};
export function colour(v) {
  if (!v) return null;
  const s = String(v).trim().toLowerCase();
  if (/^#?[0-9a-f]{6}$/.test(s)) return parseInt(s.replace('#', ''), 16);
  if (/^#?[0-9a-f]{3}$/.test(s)) { const t = s.replace('#', ''); return parseInt(t[0] + t[0] + t[1] + t[1] + t[2] + t[2], 16); }
  return NAMED[s] ?? null;
}

/* Signed area of a ring (positive = counter-clockwise in x/z). */
export function ringArea(pts) {
  let a = 0;
  for (let i = 0, j = pts.length - 1; i < pts.length; j = i++) a += (pts[j][0] - pts[i][0]) * (pts[j][1] + pts[i][1]);
  return a / 2;
}

/* Drop repeated and nearly collinear points (within 5 cm). */
export function cleanRing(pts) {
  const out = [];
  for (const p of pts) {
    const q = out[out.length - 1];
    if (!q || Math.hypot(p[0] - q[0], p[1] - q[1]) > 0.05) out.push(p);
  }
  if (out.length > 1 && Math.hypot(out[0][0] - out[out.length - 1][0], out[0][1] - out[out.length - 1][1]) < 0.05) out.pop();
  for (let i = 0; i < out.length && out.length > 3; ) {
    const a = out[(i + out.length - 1) % out.length], b = out[i], c = out[(i + 1) % out.length];
    const cross = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
    if (Math.abs(cross) < 0.05 * Math.hypot(c[0] - a[0], c[1] - a[1])) out.splice(i, 1); else i++;
  }
  return out;
}

/* One OSM building (a closed way, or a multipolygon's outer ring) →
 * a compact record. */
export function toBuilding(tags, ring) {
  const pts = cleanRing(ring);
  if (pts.length < 3) return null;
  if (ringArea(pts) < 0) pts.reverse();
  const area = Math.abs(ringArea(pts));
  if (area < 4) return null;
  const type = buildingType(tags);
  const { h, mh, levels } = buildingHeights(tags, type, area);
  return {
    pts, h, mh, levels, type,
    roof: roofShape(tags, type, area),
    mat: material(tags, type, h),
    col: colour(tags['building:colour'] || tags['building:color']),
    roofCol: colour(tags['roof:colour'] || tags['roof:color']),
    name: tags.name || null
  };
}

/* ---------------------------------------------------------------- roads */

export const RCLASS = { motorway: 0, trunk: 1, primary: 2, secondary: 3, tertiary: 4, residential: 5, service: 6, footway: 7, rail: 8 };
const RALIAS = {
  motorway: 'motorway', motorway_link: 'motorway', trunk: 'trunk', trunk_link: 'trunk', primary: 'primary', primary_link: 'primary',
  secondary: 'secondary', secondary_link: 'secondary', tertiary: 'tertiary', tertiary_link: 'tertiary',
  residential: 'residential', unclassified: 'residential', living_street: 'residential', service: 'service',
  footway: 'footway', pedestrian: 'footway', path: 'footway', steps: 'footway', cycleway: 'footway'
};
export function roadClass(tags) {
  if (tags.railway && ['rail', 'light_rail', 'subway', 'tram'].includes(tags.railway)) return tags.tunnel === 'yes' || tags.railway === 'subway' ? null : RCLASS.rail;
  const c = RALIAS[tags.highway];
  return c === undefined ? null : RCLASS[c];
}
/* Carriageway width in metres: from lanes where tagged (3.6 m each, plus a
 * little for parking on surface streets), else typical for the class. */
export function roadWidth(tags, cls) {
  const w = num(tags.width);
  if (w) return w;
  const lanes = num(tags.lanes);
  const oneway = tags.oneway === 'yes' || cls === RCLASS.motorway && /_link/.test(tags.highway);
  if (lanes) return lanes * 3.6 + (cls >= RCLASS.primary && cls <= RCLASS.residential && !oneway ? 4.8 : 1.2);
  return [44, 30, 24, 20, 15, 10.5, 6, 2.5, 4][cls];
}
export function toRoad(tags, line) {
  const cls = roadClass(tags);
  if (cls === null || line.length < 2) return null;
  if (tags.tunnel === 'yes' || tags.covered === 'yes') return null;
  const layer = num(tags.layer) || 0;
  return { cls, w: roadWidth(tags, cls), name: tags.name || tags.ref || null, ref: tags.ref || null, bridge: tags.bridge === 'yes' ? 1 : 0, layer, oneway: tags.oneway === 'yes' ? 1 : 0, pts: line };
}

/* ---------------------------------------------------------------- areas */

export const AREA = { water: 0, park: 1, grass: 2, beach: 3, parking: 4, pitch: 5, wood: 6, scrub: 7, cemetery: 8, sand: 9, plaza: 10, landuse: 11 };
export function areaKind(tags) {
  if (tags.natural === 'water' || tags.waterway === 'riverbank' || tags.water || tags.landuse === 'reservoir' || tags.leisure === 'swimming_pool') return AREA.water;
  if (tags.leisure === 'park' || tags.leisure === 'garden' || tags.leisure === 'golf_course' || tags.leisure === 'playground') return AREA.park;
  if (tags.landuse === 'grass' || tags.landuse === 'meadow' || tags.landuse === 'recreation_ground' || tags.landuse === 'village_green') return AREA.grass;
  if (tags.natural === 'beach') return AREA.beach;
  if (tags.amenity === 'parking' && tags.parking !== 'underground' && tags.parking !== 'multi-storey') return AREA.parking;
  if (tags.leisure === 'pitch' || tags.leisure === 'track' || tags.leisure === 'stadium') return AREA.pitch;
  if (tags.natural === 'wood' || tags.landuse === 'forest') return AREA.wood;
  if (tags.natural === 'scrub' || tags.natural === 'heath' || tags.natural === 'grassland') return AREA.scrub;
  if (tags.landuse === 'cemetery' || tags.amenity === 'grave_yard') return AREA.cemetery;
  if (tags.natural === 'sand' || tags.natural === 'bare_rock') return AREA.sand;
  if (tags.highway === 'pedestrian' || tags.place === 'square' || tags.amenity === 'marketplace') return AREA.plaza;
  return null;
}

/* ------------------------------------------------- points of interest */

/* Shops, restaurants and the like become signs on the building they sit in. */
export function toPoi(tags) {
  const name = tags.name;
  if (!name) return null;
  const kind = tags.shop ? 'shop' : tags.amenity && /restaurant|fast_food|cafe|bar|pub|bank|pharmacy|cinema|theatre|nightclub|fuel|ice_cream/.test(tags.amenity) ? 'amenity'
    : tags.tourism ? 'tourism' : tags.office ? 'office' : null;
  if (!kind) return null;
  return { name, kind, what: tags.shop || tags.amenity || tags.tourism || tags.office };
}

/* Street trees and palms (natural=tree), with species where mapped. */
export function treeKind(tags) {
  const g = `${tags.genus || ''} ${tags.species || ''} ${tags['species:en'] || ''} ${tags.taxon || ''}`.toLowerCase();
  if (/washingtonia|fan palm/.test(g)) return 0;
  if (/phoenix|canary|date palm/.test(g)) return 6;
  if (/syagrus|archontophoenix|queen palm|king palm|palm/.test(g)) return 7;
  if (/jacaranda/.test(g)) return 2;
  if (/quercus|oak/.test(g)) return 5;
  if (/cupressus|cypress/.test(g)) return 4;
  return 1;
}

/* ------------------------------------------------------ tile encoding */

/* Pack a tile's features into arrays of numbers (decimetres relative to
 * the tile corner) so the JSON stays small. */
export function encodeTile(tile) {
  const q = (v) => Math.round(v * 10);
  const flat = (pts, ox, oz) => pts.flatMap(([x, z]) => [q(x - ox), q(z - oz)]);
  const { ox, oz } = tile;
  return {
    v: 1, ox, oz,
    b: tile.buildings.map((b) => [b.type, Math.round(b.h * 10) / 10, Math.round(b.mh * 10) / 10, b.roof, b.mat, b.col ?? -1, b.roofCol ?? -1, b.name || '', flat(b.pts, ox, oz)]),
    r: tile.roads.map((r) => [r.cls, Math.round(r.w * 10) / 10, r.bridge, r.layer, r.oneway, r.name || '', flat(r.pts, ox, oz)]),
    a: tile.areas.map((a) => [a.kind, flat(a.pts, ox, oz)]),
    t: tile.trees.flatMap((t) => [q(t.x - ox), q(t.z - oz), t.kind]),
    p: tile.pois.map((p) => [q(p.x - ox), q(p.z - oz), p.name, p.what])
  };
}

export function decodeTile(json) {
  const { ox, oz } = json;
  const pts = (arr) => { const out = []; for (let i = 0; i < arr.length; i += 2) out.push([ox + arr[i] / 10, oz + arr[i + 1] / 10]); return out; };
  return {
    ox, oz,
    buildings: json.b.map(([type, h, mh, roof, mat, col, roofCol, name, p]) => ({ type, h, mh, roof, mat, col: col < 0 ? null : col, roofCol: roofCol < 0 ? null : roofCol, name: name || null, pts: pts(p) })),
    roads: json.r.map(([cls, w, bridge, layer, oneway, name, p]) => ({ cls, w, bridge, layer, oneway, name: name || null, pts: pts(p) })),
    areas: json.a.map(([kind, p]) => ({ kind, pts: pts(p) })),
    trees: (() => { const out = []; for (let i = 0; i < json.t.length; i += 3) out.push({ x: ox + json.t[i] / 10, z: oz + json.t[i + 1] / 10, kind: json.t[i + 2] }); return out; })(),
    pois: json.p.map(([x, z, name, what]) => ({ x: ox + x / 10, z: oz + z / 10, name, what }))
  };
}
