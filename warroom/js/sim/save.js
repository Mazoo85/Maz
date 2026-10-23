/*
 * Saves: { version, state }. Migrations carry an older save forward one version
 * at a time, so a save from any earlier stage still loads.
 */
import { STATE_VERSION } from './state.js';

export const SAVE_VERSION = STATE_VERSION;

const MIGRATIONS = {
  // 1 → 2 will go here when the state shape changes.
};

export function toSave(state) {
  return { version: SAVE_VERSION, state: JSON.parse(JSON.stringify(state)) };
}

export function fromSave(obj, data) {
  if (!obj || typeof obj !== 'object' || !obj.state) throw new Error('not a War Room save');
  if (typeof obj.version !== 'number') throw new Error('save has no version');
  if (obj.version > SAVE_VERSION) {
    throw new Error(`this save is from a newer version of the game (${obj.version}); update the game`);
  }
  let state = JSON.parse(JSON.stringify(obj.state));
  for (let v = obj.version; v < SAVE_VERSION; v++) {
    const m = MIGRATIONS[v];
    if (!m) throw new Error(`no migration from save version ${v}`);
    state = m(state, data);
  }
  state.version = SAVE_VERSION;
  for (const id of Object.keys(state.states)) {
    if (!data.stateById[id]) throw new Error(`save refers to unknown state ${id}; the map changed`);
  }
  return state;
}
