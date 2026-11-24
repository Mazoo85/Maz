// Aircraft Spotter: live aircraft from adsb.lol (no key; 500 requests/day per address, ODbL) around the view,
// or every military aircraft worldwide (/v2/mil). Details are matched against your Global Force Registry.
let bb = null, timer = 0, planes = new Map(), registry = null, mode = 'here', icon = null, want = null;

function planeIcon() {
  if (icon) return icon;
  const c = document.createElement('canvas'); c.width = c.height = 32; const g = c.getContext('2d');
  g.translate(16, 16); g.fillStyle = '#ffffff'; g.strokeStyle = '#05080d'; g.lineWidth = 1.5; g.beginPath();
  // a simple top-down aircraft silhouette pointing north
  g.moveTo(0, -14); g.lineTo(2.5, -6); g.lineTo(13, 1); g.lineTo(13, 4); g.lineTo(2.5, 0); g.lineTo(2, 9); g.lineTo(6, 12); g.lineTo(6, 14); g.lineTo(0, 12.5);
  g.lineTo(-6, 14); g.lineTo(-6, 12); g.lineTo(-2, 9); g.lineTo(-2.5, 0); g.lineTo(-13, 4); g.lineTo(-13, 1); g.lineTo(-2.5, -6); g.closePath(); g.fill(); g.stroke();
  return (icon = c);
}
const norm = s => String(s || '').toUpperCase().replace(/[^A-Z0-9]/g, '');
// A registry system name to a short key that live data uses: "F-16C Fighting Falcon" -> F16, "C-130J" -> C130,
// "Eurofighter Typhoon" -> EUROFI (live descriptions spell the maker and the designation out).
export function typeKey(s) { const m = String(s || '').toUpperCase().match(/\b([A-Z]{1,4})-?(\d{1,3})/); return m ? m[1] + m[2] : norm(s).slice(0, 6); }
const wanted = a => { if (!want) return true; const t = norm(a.t), d = norm(a.desc); return d.includes(want.key) || t === want.key || (t.length >= 3 && want.key.startsWith(t)) || (want.key.length >= 4 && t.startsWith(want.key)); };
async function regMatches(a) {
  registry = registry || await fetch('data/aircraft-registry.json').then(r => r.json());
  const t = norm(a.t), d = norm(a.desc);
  if (!t && !d) return [];
  return registry.filter(r => { const s = norm(r.system); return (t.length >= 3 && s.includes(t)) || (d && d.length >= 4 && (s.includes(d) || d.includes(s.slice(0, 6)))); }).slice(0, 6);
}
async function poll(T, layer) {
  const { C } = T;
  const v = T.view();
  let url;
  if (mode === 'mil') url = 'https://api.adsb.lol/v2/mil';
  else { const nm = Math.max(25, Math.min(250, Math.round(v.height / 1852 / 3))); url = `https://api.adsb.lol/v2/point/${v.lat.toFixed(3)}/${v.lon.toFixed(3)}/${nm}`; }
  let j; try { j = await T.json(url); } catch (e) { T.status(layer, 'error', 'err'); return; }
  const seen = new Set();
  for (const a of j.ac || []) {
    if (a.lat == null || !wanted(a)) continue; seen.add(a.hex);
    let e = planes.get(a.hex);
    if (!e) { e = { b: bb.add({ image: planeIcon(), scale: 0.7, alignedAxis: C.Cartesian3.UNIT_Z, disableDepthTestDistance: Number.POSITIVE_INFINITY }) }; planes.set(a.hex, e); }
    e.d = a;
    const alt = a.alt_baro === 'ground' ? 0 : (a.alt_geom || a.alt_baro || 0) * 0.3048;
    e.b.position = C.Cartesian3.fromDegrees(a.lon, a.lat, alt);
    e.b.rotation = -C.Math.toRadians(a.track ?? a.true_heading ?? 0);
    e.b.color = (a.dbFlags & 1) ? C.Color.fromCssColorString('#ff6b6b') : C.Color.fromCssColorString(alt < 3000 ? '#ffd166' : '#ffffff');
    e.b.id = { td: { layer: 'aircraft', data: a } };
  }
  for (const [k, e] of planes) if (!seen.has(k)) { bb.remove(e.b); planes.delete(k); }
  T.status(layer, want ? `${planes.size} × ${want.label}` : `${planes.size} aircraft`, 'live');
}

export default {
  id: 'aircraft', name: 'Aircraft Spotter', group: 'Live now', worldOnly: true, sub: 'Live aircraft, military filter', credit: 'Aircraft: adsb.lol (ODbL)',
  info: 'Live aircraft from adsb.lol around the part of the globe you are looking at, or all military aircraft worldwide. Military types are matched with your Global Force Registry.',
  async enable(T) { bb = T.viewer.scene.primitives.add(new T.C.BillboardCollection()); await poll(T, this); timer = setInterval(() => poll(T, this), 15000); },
  async disable(T) { clearInterval(timer); if (bb) { T.viewer.scene.primitives.remove(bb); bb = null; } planes.clear(); },
  onView(T) { if (mode === 'here') poll(T, this); },
  panel(T) {
    return `<p class="note">Updates every 15 seconds. Red = military, yellow = below 3,000 m, white = cruising. adsb.lol allows about 500 requests a day without a key (a free key in Settings doubles it).</p>
      <label class="row"><input type="radio" name="am" value="here" ${mode === 'here' ? 'checked' : ''}> Aircraft around where I'm looking</label>
      <label class="row"><input type="radio" name="am" value="mil" ${mode === 'mil' ? 'checked' : ''}> All military aircraft worldwide</label>
      ${want ? `<div class="row"><span class="grow">Showing only <b>${T.esc(want.label)}</b></span><button id="acAll" class="small">Show all</button></div>` : ''}
      <div class="big">${planes.size}</div><span class="muted small">aircraft on the globe</span>`;
  },
  wire(T, el) {
    for (const r of el.querySelectorAll('input[name=am]')) r.onchange = () => { mode = r.value; poll(T, this); };
    const all = el.querySelector('#acAll'); if (all) all.onclick = () => { want = null; poll(T, this); T.refreshPanel(this); };
  },
  // From a link: show only one type, worldwide among military aircraft unless mil=0.
  async showType(T, type, mil = true) {
    want = type ? { label: String(type).slice(0, 60), key: typeKey(type) } : null; if (mil) mode = 'mil';
    if (bb) { for (const e of planes.values()) bb.remove(e.b); planes.clear(); await poll(T, this); }
    return { key: want && want.key, mode, shown: planes.size };
  },
  async onPick(T, a) {
    const alt = a.alt_baro === 'ground' ? 'on the ground' : `${Math.round((a.alt_baro || 0) * 0.3048).toLocaleString()} m (${(a.alt_baro || 0).toLocaleString()} ft)`;
    const reg = await regMatches(a);
    T.detail((a.flight || a.r || a.hex).trim(), `<div class="kv">
      <div>Type</div><div>${T.esc([a.t, a.desc].filter(Boolean).join(' · ') || '—')}</div><div>Registration</div><div>${T.esc(a.r || '—')}</div>
      <div>Operator</div><div>${T.esc(a.ownOp || '—')}</div><div>Altitude</div><div>${alt}</div><div>Speed</div><div>${a.gs != null ? Math.round(a.gs) + ' knots' : '—'}</div>
      <div>Track</div><div>${a.track != null ? Math.round(a.track) + '°' : '—'}</div><div>Squawk</div><div>${T.esc(a.squawk || '—')}</div>
      <div>Military</div><div>${a.dbFlags & 1 ? 'yes' : 'no'}</div><div>ICAO hex</div><div>${T.esc(a.hex)}</div></div>
      ${reg.length ? `<b>In your Global Force Registry</b>${reg.map(r => `<div class="note"><b>${T.esc(r.system)}</b> · ${T.esc(r.country)} · ${T.esc(r.category)} · ${T.esc(r.status)}<br>${T.esc(r.qty || '')}${r.year ? ' (' + r.year + ')' : ''}</div>`).join('')}` : ''}`);
  },
  async test(T) { T.viewer.camera.setView({ destination: T.C.Cartesian3.fromDegrees(-0.45, 51.47, 400000) }); await poll(T, this); return { aircraft: planes.size, near: 'London Heathrow', status: this.statusText }; },
};
