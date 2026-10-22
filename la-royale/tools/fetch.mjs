#!/usr/bin/env node
/*
 * Downloads the real data for LA Royale's true-scale map:
 *
 *   node la-royale/tools/fetch.mjs osm     # buildings, roads, areas, trees, shops (OpenStreetMap, via Overpass)
 *   node la-royale/tools/fetch.mjs dem     # ground elevation (Terrarium tiles, AWS open data)
 *   node la-royale/tools/fetch.mjs far     # the mountains and coast round it, coarser, for the horizon
 *
 * Raw downloads go to la-royale/data/raw/ (not committed; it's large and
 * rebuildable). Then run tools/build-tiles.mjs.
 *
 * Uses curl, so it goes through whatever proxy the shell is set up with.
 * Overpass is a shared free service: requests are spaced out and retried.
 * OSM data is © OpenStreetMap contributors, ODbL.
 */

import { execFileSync } from 'node:child_process';
import { mkdirSync, existsSync, writeFileSync, statSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { REAL, FAR } from '../js/osm.js';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const RAW = join(ROOT, 'data', 'raw');
mkdirSync(RAW, { recursive: true });

const OVERPASS = process.env.OVERPASS || 'https://overpass-api.de/api/interpreter';
const CELL = 0.01;                          // ~1 km squares, so each query stays small

function curl(args, out) {
  for (let attempt = 1; attempt <= 5; attempt++) {
    try {
      execFileSync('curl', ['-sS', '--fail', '-m', '240', '-o', out, ...args], { stdio: ['ignore', 'ignore', 'pipe'] });
      return true;
    } catch (e) {
      const wait = 5 * 2 ** attempt;
      console.warn(`  attempt ${attempt} failed (${String(e.stderr || e.message).trim().slice(0, 120)}); retrying in ${wait}s`);
      execFileSync('sleep', [String(wait)]);
    }
  }
  return false;
}

function osm() {
  const cells = [];
  for (let lat = REAL.south; lat < REAL.north - 1e-9; lat += CELL) for (let lon = REAL.west; lon < REAL.east - 1e-9; lon += CELL) cells.push([lat, lon]);
  console.log(`${cells.length} cells of ~1 km² over ${REAL.name}`);
  let n = 0;
  for (const [lat, lon] of cells) {
    n++;
    const s = lat.toFixed(4), w = lon.toFixed(4), nn = Math.min(lat + CELL, REAL.north).toFixed(4), e = Math.min(lon + CELL, REAL.east).toFixed(4);
    const out = join(RAW, `osm_${s}_${w}.json`);
    if (existsSync(out) && statSync(out).size > 100) continue;
    const bbox = `${s},${w},${nn},${e}`;
    const q = `[out:json][timeout:180];
(
  way["building"](${bbox});
  relation["building"]["type"="multipolygon"](${bbox});
  way["building:part"](${bbox});
  way["highway"](${bbox});
  way["railway"~"rail|light_rail|tram"](${bbox});
  way["natural"~"water|beach|wood|scrub|sand|heath|grassland"](${bbox});
  way["landuse"~"grass|meadow|recreation_ground|forest|cemetery|reservoir|village_green"](${bbox});
  way["leisure"~"park|garden|pitch|playground|golf_course|swimming_pool|track|stadium"](${bbox});
  relation["leisure"="park"](${bbox});
  way["amenity"~"parking|grave_yard|marketplace"](${bbox});
  way["place"="square"](${bbox});
  node["natural"="tree"](${bbox});
  node["name"]["shop"](${bbox});
  node["name"]["amenity"~"restaurant|fast_food|cafe|bar|pub|bank|pharmacy|cinema|theatre|nightclub|fuel|ice_cream"](${bbox});
  node["name"]["tourism"](${bbox});
  node["name"]["office"](${bbox});
);
out geom;`;
    console.log(`[${n}/${cells.length}] ${bbox}`);
    if (!curl(['--data-urlencode', `data=${q}`, OVERPASS], out)) { console.error('  gave up on this cell; run again to retry'); continue; }
    execFileSync('sleep', ['2']);
  }
}

/* Terrarium elevation tiles at zoom 14 (~8 m a pixel here):
 * height = R * 256 + G + B / 256 - 32768 metres. */
function dem() {
  const z = 14;
  const tx = (lon) => Math.floor(((lon + 180) / 360) * 2 ** z);
  const ty = (lat) => { const r = (lat * Math.PI) / 180; return Math.floor(((1 - Math.log(Math.tan(r) + 1 / Math.cos(r)) / Math.PI) / 2) * 2 ** z); };
  const [x0, x1, y0, y1] = [tx(REAL.west), tx(REAL.east), ty(REAL.north), ty(REAL.south)];
  console.log(`${(x1 - x0 + 1) * (y1 - y0 + 1)} elevation tiles`);
  for (let y = y0; y <= y1; y++) {
    for (let x = x0; x <= x1; x++) {
      const out = join(RAW, `dem_${z}_${x}_${y}.png`);
      if (existsSync(out) && statSync(out).size > 100) continue;
      curl([`https://s3.amazonaws.com/elevation-tiles-prod/terrarium/${z}/${x}/${y}.png`], out);
    }
  }
}

/* The country round the map, coarser, for the horizon (FAR in osm.js). */
function far() {
  const z = FAR.z, n = 2 ** z;
  const midLat = (REAL.south + REAL.north) / 2, midLon = (REAL.west + REAL.east) / 2;
  const dLat = FAR.reach / 110574, dLon = FAR.reach / (111320 * Math.cos((midLat * Math.PI) / 180));
  const tx = (lon) => Math.floor(((lon + 180) / 360) * n);
  const ty = (lat) => { const r = (lat * Math.PI) / 180; return Math.floor(((1 - Math.log(Math.tan(r) + 1 / Math.cos(r)) / Math.PI) / 2) * n); };
  const [x0, x1, y0, y1] = [tx(midLon - dLon), tx(midLon + dLon), ty(midLat + dLat), ty(midLat - dLat)];
  console.log(`${(x1 - x0 + 1) * (y1 - y0 + 1)} elevation tiles for the horizon`);
  for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++) {
    const out = join(RAW, `far_${z}_${x}_${y}.png`);
    if (existsSync(out) && statSync(out).size > 100) continue;
    curl([`https://s3.amazonaws.com/elevation-tiles-prod/terrarium/${z}/${x}/${y}.png`], out);
  }
}

const what = process.argv[2];
if (what === 'osm') osm();
else if (what === 'dem') dem();
else if (what === 'far') far();
else { console.log('usage: fetch.mjs osm | dem | far'); process.exit(1); }
writeFileSync(join(RAW, '.fetched'), new Date().toISOString());
