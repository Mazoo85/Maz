/*
 * Regenerates godview/data/countries.geojson — the glowing country outlines.
 *
 * The outlines are Natural Earth's 1:110m countries (public domain), as packed
 * by world-atlas (ISC). They are converted once and committed, so the page
 * needs no converter and no extra download at run time.
 *
 *   cd "$(mktemp -d)" && npm pack world-atlas@2.0.2 topojson-client@3.1.0
 *   mkdir wa tc && tar xzf world-atlas-2.0.2.tgz -C wa && tar xzf topojson-client-3.1.0.tgz -C tc
 *   node /path/to/Maz/godview/tools/build-countries.cjs wa/package tc/package
 */
'use strict';

const fs = require('fs');
const path = require('path');

const [worldAtlasDir, topojsonDir] = process.argv.slice(2);
if (!worldAtlasDir || !topojsonDir) {
  console.error('usage: node build-countries.cjs <world-atlas package dir> <topojson-client package dir>');
  process.exit(2);
}

const topojson = require(path.resolve(topojsonDir, 'dist', 'topojson-client.js'));
const topo = JSON.parse(fs.readFileSync(path.resolve(worldAtlasDir, 'countries-110m.json'), 'utf8'));
const geo = topojson.feature(topo, topo.objects.countries);

const round = (v) => (Array.isArray(v) ? v.map(round) : Math.round(v * 100) / 100);
const out = {
  type: 'FeatureCollection',
  features: geo.features.map((f) => ({
    type: 'Feature',
    properties: { name: f.properties.name },
    geometry: { type: f.geometry.type, coordinates: round(f.geometry.coordinates) }
  }))
};

const target = path.join(__dirname, '..', 'data', 'countries.geojson');
fs.mkdirSync(path.dirname(target), { recursive: true });
fs.writeFileSync(target, JSON.stringify(out));
console.log('wrote ' + out.features.length + ' countries, ' + fs.statSync(target).size + ' bytes → ' + target);
