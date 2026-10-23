/* The side panel: province, state, country and saves views share this frame. */
import { flagSvg } from '../flags.js';
import { formatDate } from '../../sim/clock.js';

const el = () => document.getElementById('panel');

export function closePanel() { el().hidden = true; }

function frame(title, bodyHtml) {
  const p = el();
  p.innerHTML = `<button class="close" aria-label="Close">×</button><h2>${title}</h2>${bodyHtml}`;
  p.hidden = false;
  p.scrollTop = 0;
  p.querySelector('.close').addEventListener('click', closePanel);
  return p;
}

const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
const flag = (c) => `<span class="flag">${flagSvg(c.flag)}</span>`;
const cap = (s) => s.charAt(0).toUpperCase() + s.slice(1).replace('_', '-');
const cap2 = (s) => s.split('_').map((w) => w.charAt(0).toUpperCase() + w.slice(1)).join('-');

export function showProvince(id, { state, data, weather, onProvince, onState, onCountry }) {
  const p = data.provinces[id];
  if (!p) return;
  const st = p.state ? data.stateById[p.state] : null;
  const ctrl = st ? state.countries[state.states[st.id].controller] : null;
  const owner = st ? state.countries[state.states[st.id].owner] : null;
  const region = data.regionById[p.region];
  const neighbours = data.adjacency[id].map((n) => {
    const q = data.provinces[n];
    const river = data.riverSet.has(id < n ? `${id}-${n}` : `${n}-${id}`) ? ' ~' : '';
    return `<li><button data-prov="${n}" title="${esc(q.terrain)}">${esc(q.name)}${river}</button></li>`;
  }).join('');
  const rows = [
    ['Type', p.type === 'land' ? cap(p.terrain) : cap(p.type)],
    st ? ['State', `<button class="linkish" data-state="${st.id}">${esc(st.name)}</button>`] : null,
    ctrl ? ['Controlled by', `<button class="linkish" data-country="${ctrl.tag}">${flag(ctrl)} ${esc(ctrl.name)}</button>`] : null,
    owner && owner !== ctrl ? ['Owned by', `<button class="linkish" data-country="${owner.tag}">${flag(owner)} ${esc(owner.name)}</button>`] : null,
    p.vp ? ['Victory points', p.vp] : null,
    ['Region', esc(region ? region.name : '—')],
    ['Weather', cap(weather)],
    p.coastal ? ['Coast', 'yes'] : null,
    ['Position', `${p.lat.toFixed(2)}°, ${p.lon.toFixed(2)}°`],
  ].filter(Boolean).map(([k, v]) => `<dt>${k}</dt><dd>${v}</dd>`).join('');
  const panel = frame(esc(p.name), `<dl>${rows}</dl><h3>Neighbours</h3><ul class="chips">${neighbours}</ul>`);
  wire(panel, { onProvince, onState, onCountry });
}

export function showState(id, { state, data, onProvince, onCountry }) {
  const st = data.stateById[id];
  const s = state.states[id];
  const owner = state.countries[s.owner], ctrl = state.countries[s.controller];
  const cores = s.cores.map((t) => `<button class="linkish" data-country="${t}">${flag(state.countries[t])} ${esc(state.countries[t].name)}</button>`).join(', ') || 'none';
  const provs = st.provinces.map((pid) => `<li><button data-prov="${pid}">${esc(data.provinces[pid].name)}${data.provinces[pid].vp ? ' ★' : ''}</button></li>`).join('');
  const rows = [
    ['Owner', `<button class="linkish" data-country="${owner.tag}">${flag(owner)} ${esc(owner.name)}</button>`],
    ctrl !== owner ? ['Controller', `<button class="linkish" data-country="${ctrl.tag}">${flag(ctrl)} ${esc(ctrl.name)}</button>`] : null,
    ['Cores', cores],
    ['Capital', `<button class="linkish" data-prov="${st.capital_province}">${esc(data.provinces[st.capital_province].name)}</button>`],
    ['Manpower', st.manpower.toLocaleString('en-GB')],
    ['Infrastructure', `${s.infrastructure} / 5`],
    ['Provinces', st.provinces.length],
  ].filter(Boolean).map(([k, v]) => `<dt>${k}</dt><dd>${v}</dd>`).join('');
  const panel = frame(esc(st.name), `<dl>${rows}</dl><h3>Provinces</h3><ul class="chips">${provs}</ul>`);
  wire(panel, { onProvince, onCountry, onState: () => {} });
}

export function showCountry(tag, opts) {
  const { state, data, onProvince, onState, send } = opts;
  const c = state.countries[tag];
  const capState = data.stateById[c.capital];
  const states = c.states.map((sid) => data.stateById[sid]);
  const provinces = states.reduce((n, s) => n + s.provinces.length, 0);
  const manpower = states.reduce((n, s) => n + s.manpower, 0);
  const cores = states.filter((s) => state.states[s.id].cores.includes(tag)).length;
  const army = (state.land.armies[tag] || []).reduce((n, a) => n + a.divisions, 0);
  const list = states.slice().sort((a, b) => a.name.localeCompare(b.name)).map((s) =>
    `<li><button data-state="${s.id}">${esc(s.name)}${state.states[s.id].cores.includes(tag) ? '' : ' (non-core)'}</button></li>`).join('');
  const rows = [
    ['Leader', esc(c.leader)],
    ['Ideology', cap2(c.ideology)],
    ['Capital', `<button class="linkish" data-state="${capState.id}">${esc(capState.name)}</button>`],
    ['States', `${states.length} (${cores} core)`],
    ['Provinces', provinces],
    ['Manpower', manpower.toLocaleString('en-GB')],
    ['Divisions', army],
  ].map(([k, v]) => `<dt>${k}</dt><dd>${v}</dd>`).join('');
  const playing = state.player === tag;
  const panel = frame(`${flag(c)} ${esc(c.name)}`,
    `<dl>${rows}</dl><div class="row"><button class="primary" id="play-as" ${playing ? 'disabled' : ''}>${playing ? 'You play this country' : 'Play as ' + esc(c.name)}</button></div><h3>States</h3><ul class="chips">${list}</ul>`);
  panel.querySelector('#play-as').addEventListener('click', () => { send({ type: 'select_country', tag }); showCountry(tag, opts); });
  wire(panel, { onProvince, onState, onCountry: () => {} });
}

function wire(panel, { onProvince, onState, onCountry }) {
  panel.querySelectorAll('[data-prov]').forEach((b) => b.addEventListener('click', () => onProvince(Number(b.dataset.prov))));
  panel.querySelectorAll('[data-state]').forEach((b) => b.addEventListener('click', () => onState(Number(b.dataset.state))));
  panel.querySelectorAll('[data-country]').forEach((b) => b.addEventListener('click', () => onCountry(b.dataset.country)));
}

export function showSaves({ list, onSave, onLoad, onDelete, onExport, onImport, current }) {
  const items = list.map((s) => `<li><span><strong>${esc(s.name)}</strong><br><small>${esc(formatDate(s.hour))} · ${esc(s.player || 'Observer')} · saved ${esc(s.when)}</small></span>
    <button data-load="${esc(s.name)}">Load</button><button data-delete="${esc(s.name)}" title="Delete">🗑</button></li>`).join('') || '<li><span>No saves yet.</span></li>';
  const panel = frame('Saves', `
    <div class="row"><input id="save-name" placeholder="Name this save" value="${esc(current)}"><button class="primary" id="save-now">Save</button></div>
    <ul class="saves-list">${items}</ul>
    <h3>Files</h3>
    <div class="row"><button id="export-save">Download save file</button><input type="file" id="import-save" accept=".warroom,.json,application/json,application/gzip" hidden><button id="import-button">Open save file…</button></div>
    <p><small>Saves stay in this browser. Download a file to keep one or move it to another device.</small></p>`);
  panel.querySelector('#save-now').addEventListener('click', () => onSave(panel.querySelector('#save-name').value.trim() || 'My game'));
  panel.querySelectorAll('[data-load]').forEach((b) => b.addEventListener('click', () => onLoad(b.dataset.load)));
  panel.querySelectorAll('[data-delete]').forEach((b) => b.addEventListener('click', () => onDelete(b.dataset.delete)));
  panel.querySelector('#export-save').addEventListener('click', () => onExport(panel.querySelector('#save-name').value.trim() || 'war-room'));
  const input = panel.querySelector('#import-save');
  panel.querySelector('#import-button').addEventListener('click', () => input.click());
  input.addEventListener('change', () => { if (input.files[0]) onImport(input.files[0]); });
}

export function toast(msg, ms = 2500) {
  const t = document.getElementById('toast');
  t.textContent = msg; t.hidden = false;
  clearTimeout(toast.timer);
  toast.timer = setTimeout(() => { t.hidden = true; }, ms);
}
