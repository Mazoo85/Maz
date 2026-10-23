/*
 * Saving and loading in the browser: named saves in localStorage, an autosave,
 * and gzip-compressed .warroom files for download and import. The sim's
 * toSave/fromSave do the real work; this is only storage.
 */
import { toSave, fromSave } from '../sim/save.js';

const PREFIX = 'warroom:save:';
const AUTOSAVE = 'autosave';

export function listSaves() {
  const out = [];
  for (let i = 0; i < localStorage.length; i++) {
    const k = localStorage.key(i);
    if (!k.startsWith(PREFIX)) continue;
    try {
      const meta = JSON.parse(localStorage.getItem(k)).meta;
      out.push({ name: k.slice(PREFIX.length), ...meta });
    } catch { /* ignore a broken entry */ }
  }
  return out.sort((a, b) => (a.name === AUTOSAVE ? -1 : b.name === AUTOSAVE ? 1 : b.at - a.at));
}

export function saveLocal(name, state) {
  const payload = { meta: { hour: state.hour, player: state.player, at: Date.now(), when: new Date().toLocaleString() }, save: toSave(state) };
  try {
    localStorage.setItem(PREFIX + name, JSON.stringify(payload));
    return true;
  } catch (e) {
    throw new Error('the browser refused to store the save (storage may be full): ' + e.message);
  }
}

export function loadLocal(name, data) {
  const raw = localStorage.getItem(PREFIX + name);
  if (!raw) throw new Error(`no save called "${name}"`);
  return fromSave(JSON.parse(raw).save, data);
}

export function deleteLocal(name) { localStorage.removeItem(PREFIX + name); }
export function autosave(state) { return saveLocal(AUTOSAVE, state); }

export async function exportFile(name, state) {
  const json = JSON.stringify(toSave(state));
  let blob;
  if (typeof CompressionStream === 'function') {
    const stream = new Blob([json]).stream().pipeThrough(new CompressionStream('gzip'));
    blob = await new Response(stream).blob();
  } else {
    blob = new Blob([json], { type: 'application/json' });
  }
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = name.replace(/[^\w.-]+/g, '_') + '.warroom';
  document.body.appendChild(a); a.click(); a.remove();
  setTimeout(() => URL.revokeObjectURL(a.href), 5000);
}

export async function importFile(file, data) {
  const buf = new Uint8Array(await file.arrayBuffer());
  let text;
  if (buf[0] === 0x1f && buf[1] === 0x8b) {
    if (typeof DecompressionStream !== 'function') throw new Error('this browser cannot open compressed saves');
    const stream = new Blob([buf]).stream().pipeThrough(new DecompressionStream('gzip'));
    text = await new Response(stream).text();
  } else {
    text = new TextDecoder().decode(buf);
  }
  return fromSave(JSON.parse(text), data);
}
