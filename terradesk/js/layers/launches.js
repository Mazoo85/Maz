// Launch Control: upcoming and recent launches from The Space Devs Launch Library 2 (no key; 15 requests an hour,
// so answers are cached for 15 minutes). Ascent paths are illustrations from the pad and the target orbit.
const LL = 'https://ll.thespacedevs.com/2.3.0';
let ents = [], list = [], recent = [], tick = 0, tab = 'up', path = null;

const until = t => { const s = Math.round((new Date(t) - Date.now()) / 1000); if (s <= 0) return 'T+' + fmt(-s); return 'T−' + fmt(s); };
const fmt = s => { const d = Math.floor(s / 86400), h = Math.floor(s / 3600) % 24, m = Math.floor(s / 60) % 60, x = s % 60; return (d ? d + 'd ' : '') + String(h).padStart(2, '0') + ':' + String(m).padStart(2, '0') + ':' + String(x).padStart(2, '0'); };
function azimuthFor(l, lat) {
  const o = (l.mission && l.mission.orbit && l.mission.orbit.abbrev) || '';
  if (/SSO|PO|polar/i.test(o)) return 190;
  if (/ISS/i.test((l.mission && l.mission.name) || '') || /LEO/i.test(o)) { const inc = 51.6; const r = Math.cos(inc * Math.PI / 180) / Math.cos(lat * Math.PI / 180); return Math.abs(r) <= 1 ? Math.asin(r) * 180 / Math.PI : 90; }
  return 90;
}
function ascent(T, l) {
  const { C } = T, lat = +l.pad.latitude, lon = +l.pad.longitude, az = azimuthFor(l, lat) * Math.PI / 180;
  const pts = []; const R = 6371e3;
  for (let i = 0; i <= 60; i++) {
    const f = i / 60, d = f * 1800e3, h = 200e3 * Math.sin(f * Math.PI / 2);
    const la1 = lat * Math.PI / 180, lo1 = lon * Math.PI / 180, dr = d / R;
    const la2 = Math.asin(Math.sin(la1) * Math.cos(dr) + Math.cos(la1) * Math.sin(dr) * Math.cos(az));
    const lo2 = lo1 + Math.atan2(Math.sin(az) * Math.sin(dr) * Math.cos(la1), Math.cos(dr) - Math.sin(la1) * Math.sin(la2));
    pts.push(C.Cartesian3.fromRadians(lo2, la2, h));
  }
  return pts;
}
async function load(T, layer) {
  const j = await T.json(`${LL}/launches/upcoming/?limit=30&mode=normal`);
  list = j.results || [];
  try { recent = (await T.json(`${LL}/launches/previous/?limit=20&mode=normal`)).results || []; } catch { recent = []; }
  draw(T);
  T.status(layer, `${list.length} upcoming`, 'live');
}
function draw(T) {
  const { C, viewer } = T;
  for (const e of ents.splice(0)) viewer.entities.remove(e);
  const byPad = new Map();
  for (const l of list) if (l.pad && l.pad.latitude) { const k = l.pad.id; if (!byPad.has(k)) byPad.set(k, l); }
  for (const l of byPad.values()) {
    const e = viewer.entities.add({ position: C.Cartesian3.fromDegrees(+l.pad.longitude, +l.pad.latitude),
      point: { pixelSize: 9, color: C.Color.fromCssColorString('#ffbf47'), outlineColor: C.Color.BLACK, outlineWidth: 1, disableDepthTestDistance: Number.POSITIVE_INFINITY },
      label: { text: '', font: '12px Segoe UI', pixelOffset: new C.Cartesian2(0, -14), showBackground: true, backgroundColor: C.Color.fromCssColorString('rgba(5,8,13,.8)'), disableDepthTestDistance: Number.POSITIVE_INFINITY, distanceDisplayCondition: new C.DistanceDisplayCondition(0, 2.5e7) } });
    e.td = { layer: 'launches', data: l }; e.launch = l; ents.push(e);
  }
}
function select(T, l) {
  const { C, viewer } = T;
  if (path) viewer.entities.remove(path);
  path = viewer.entities.add({ polyline: { positions: ascent(T, l), width: 3, material: new C.PolylineGlowMaterialProperty({ glowPower: 0.25, color: C.Color.fromCssColorString('#ffbf47') }) } });
  T.flyTo(+l.pad.longitude, +l.pad.latitude, 2500000);
  const vid = (l.vid_urls || l.vidURLs || [])[0];
  T.detail(l.name, `<div class="big">${until(l.net)}</div><div class="kv">
    <div>Rocket</div><div>${T.esc(l.rocket && l.rocket.configuration && l.rocket.configuration.full_name || '—')}</div>
    <div>Provider</div><div>${T.esc(l.launch_service_provider && l.launch_service_provider.name || '—')}</div>
    <div>Status</div><div>${T.esc(l.status && l.status.name || '—')}</div><div>Time (NET)</div><div>${new Date(l.net).toLocaleString()}</div>
    <div>Pad</div><div>${T.esc(l.pad.name)}, ${T.esc(l.pad.location && l.pad.location.name || '')}</div>
    <div>Orbit</div><div>${T.esc(l.mission && l.mission.orbit && l.mission.orbit.name || '—')}</div></div>
    ${l.mission && l.mission.description ? `<p class="small">${T.esc(l.mission.description)}</p>` : ''}
    ${vid ? `<button id="lVid">Watch the webcast</button>` : ''}<p class="note">The glowing line is an illustration of the ascent toward its orbit, not real tracking.</p>`, el => { const b = el.querySelector('#lVid'); if (b) b.onclick = () => T.td.openExternal(vid.url); });
}

export default {
  id: 'launches', name: 'Launch Control', group: 'Live now', worldOnly: true, sub: 'Rocket launches and countdowns', credit: 'Launches: The Space Devs',
  info: 'Upcoming and recent rocket launches worldwide with countdowns, pads and illustrated ascent paths (The Space Devs Launch Library 2).',
  async enable(T) { await load(T, this); tick = setInterval(() => { for (const e of ents) e.label.text = e.launch.name.split('|')[0].trim() + '  ' + until(e.launch.net); if (T.refreshPanel && this.on && document.querySelector('#llList')) for (const s of document.querySelectorAll('#llList [data-net]')) s.textContent = until(s.dataset.net); }, 1000); },
  async disable(T) { clearInterval(tick); for (const e of ents.splice(0)) T.viewer.entities.remove(e); if (path) { T.viewer.entities.remove(path); path = null; } },
  panel(T) {
    const rows = tab === 'up' ? list : recent;
    return `<div class="row"><button id="llUp" class="${tab === 'up' ? 'primary' : ''}">Upcoming</button><button id="llPrev" class="${tab === 'prev' ? 'primary' : ''}">Recent</button></div>
      <div class="list" id="llList">${rows.map((l, i) => `<div class="it" data-i="${i}"><b>${T.esc(l.name)}</b><span class="r" data-net="${l.net}">${until(l.net)}</span><span class="s">${T.esc(l.launch_service_provider && l.launch_service_provider.name || '')} · ${T.esc(l.pad && l.pad.location && l.pad.location.name || '')} · ${T.esc(l.status && l.status.abbrev || '')}</span></div>`).join('') || '<div class="it muted">Nothing loaded yet.</div>'}</div>
      <p class="note">Launch Library allows 15 requests an hour for free, so the list refreshes every 15 minutes.</p>`;
  },
  wire(T, el) {
    el.querySelector('#llUp').onclick = () => { tab = 'up'; T.panel(this); };
    el.querySelector('#llPrev').onclick = () => { tab = 'prev'; T.panel(this); };
    el.querySelector('#llList').onclick = e => { const it = e.target.closest('.it[data-i]'); if (it) select(T, (tab === 'up' ? list : recent)[+it.dataset.i]); };
  },
  onPick(T, l) { select(T, l); },
  async test() { return { upcoming: list.length, recent: recent.length, next: list[0] && list[0].name }; },
};
