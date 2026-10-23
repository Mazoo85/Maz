import { readFileSync, existsSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import { test, assert, eq, done } from './harness.js';

const dir = join(dirname(fileURLToPath(import.meta.url)), '..', 'models');
const manifest = JSON.parse(readFileSync(join(dir, 'manifest.json'), 'utf8'));
const REQUIRED = ['division', 'division_armoured', 'division_motorised', 'division_special', 'fleet', 'air_wing',
  'infantry', 'tank_light', 'tank_medium', 'tank_heavy', 'truck', 'artillery', 'fighter', 'bomber',
  'destroyer', 'cruiser', 'battleship', 'carrier', 'submarine', 'convoy',
  'factory', 'dockyard', 'airbase', 'naval_base', 'fort', 'radar', 'supply_hub', 'capital', 'victory_point', 'battle', 'nuke'];

test('the manifest lists every model the game needs', () => {
  for (const name of REQUIRED) assert(manifest.models[name], 'missing model ' + name);
});

test('every model is a small, valid binary glTF that stands on its base', () => {
  for (const [name, m] of Object.entries(manifest.models)) {
    const path = join(dir, m.file);
    assert(existsSync(path), name + ' file exists');
    const buf = readFileSync(path);
    eq(buf.toString('latin1', 0, 4), 'glTF', name + ' magic');
    eq(buf.readUInt32LE(4), 2, name + ' glTF version');
    eq(buf.readUInt32LE(8), buf.length, name + ' declared length');
    assert(buf.length < 200 * 1024, name + ' under 200 KB');
    assert(m.height > 0 && m.height <= 1.0001, name + ' height ' + m.height);
    const json = JSON.parse(buf.toString('utf8', 20, 20 + buf.readUInt32LE(12)));
    assert(json.meshes && json.meshes.length >= 1, name + ' has a mesh');
    assert(json.materials.some((mat) => mat.name === 'Body'), name + ' has a Body material');
  }
});

done('models');
