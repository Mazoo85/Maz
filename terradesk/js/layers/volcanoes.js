// Volcano Watch: the Smithsonian GVP list of 1,214 Holocene volcanoes (bundled), coloured by this week's activity
// from GVP's Weekly Volcanic Activity Report (RSS) and US alert levels from USGS HANS.
let pts = null, base = [], weekly = new Map(), usgs = new Map(), timer = 0;
const COLOR = { report: '#ff8a5c', WARNING: '#ff3b3b', WATCH: '#ff8a5c', ADVISORY: '#ffd166', NORMAL: '#66d19e' };
const key = s => String(s || '').toLowerCase().replace(/[^a-z]/g, '');

async function refresh(T, L) {
  try {
    const rss = await T.text('https://volcano.si.edu/news/WeeklyVolcanoRSS.xml');
    weekly.clear();
    for (const m of rss.matchAll(/<item>([\s\S]*?)<\/item>/g)) {
      const title = (m[1].match(/<title>([\s\S]*?)<\/title>/) || [])[1] || '', desc = (m[1].match(/<description>([\s\S]*?)<\/description>/) || [])[1] || '', link = (m[1].match(/<link>([\s\S]*?)<\/link>/) || [])[1] || '';
      const name = title.replace(/<!\[CDATA\[|\]\]>/g, '').split(/\s*\(/)[0].trim();
      weekly.set(key(name), { title: title.replace(/<!\[CDATA\[|\]\]>/g, ''), text: desc.replace(/<!\[CDATA\[|\]\]>/g, '').replace(/<[^>]+>/g, ' ').replace(/&[a-z]+;/g, ' ').replace(/\s+/g, ' ').trim(), link: link.trim() });
    }
  } catch { /* keep the last report */ }
  try {
    const h = await T.json('https://volcanoes.usgs.gov/hans-public/api/volcano/getElevatedVolcanoes');
    usgs.clear(); for (const v of h) usgs.set(key(v.volcano_name), v);
  } catch { /* optional */ }
  for (const e of base) {
    const w = weekly.get(key(e.d.name)), u = usgs.get(key(e.d.name));
    e.state = u ? (u.alert_level || 'ADVISORY').toUpperCase() : w ? 'report' : '';
    e.p.color = T.C.Color.fromCssColorString(e.state ? COLOR[e.state] || '#ff8a5c' : '#9aa4b2');
    e.p.pixelSize = e.state ? 9 : 4;
  }
  const active = base.filter(e => e.state).length;
  T.status(L, `${active} active this week`, 'live');
}

export default {
  id: 'volcanoes', name: 'Volcano Watch', group: 'Live now', worldOnly: true, sub: 'Volcanoes and this week\'s activity', credit: 'Volcanoes: Smithsonian GVP, USGS',
  info: 'All 1,214 volcanoes active in the last 12,000 years (Smithsonian Global Volcanism Program). Orange = in this week\'s activity report; US volcanoes show their USGS alert level.',
  async enable(T) {
    const { C, viewer } = T;
    const list = await fetch('data/volcanoes.json').then(r => r.json());
    pts = viewer.scene.primitives.add(new C.PointPrimitiveCollection());
    base = list.map(d => ({ d, p: pts.add({ position: C.Cartesian3.fromDegrees(d.lon, d.lat, Math.max(0, d.elev || 0)), pixelSize: 4, color: C.Color.fromCssColorString('#9aa4b2'), outlineColor: C.Color.BLACK, outlineWidth: 1, id: { td: { layer: 'volcanoes', data: d } }, disableDepthTestDistance: 2e6 }) }));
    await refresh(T, this);
    timer = setInterval(() => refresh(T, this), 60 * 60 * 1000);
  },
  async disable(T) { clearInterval(timer); if (pts) T.viewer.scene.primitives.remove(pts); pts = null; base = []; },
  panel(T) {
    const act = base.filter(e => e.state);
    return `<div class="legend"><span><i style="background:#ff3b3b"></i>Warning</span><span><i style="background:#ff8a5c"></i>Watch / weekly report</span><span><i style="background:#ffd166"></i>Advisory</span><span><i style="background:#9aa4b2"></i>Quiet</span></div>
      <div class="list" id="vList">${act.map((e, i) => `<div class="it" data-n="${e.d.n}"><b>${T.esc(e.d.name)}</b><span class="r">${T.esc(e.state === 'report' ? 'report' : e.state.toLowerCase())}</span><span class="s">${T.esc(e.d.country)} · ${T.esc(e.d.type)}</span></div>`).join('') || '<div class="it muted">Loading this week\'s report…</div>'}</div>
      <p class="note">The weekly report comes out on Wednesdays from the Smithsonian and USGS.</p>`;
  },
  wire(T, el) { el.querySelector('#vList').onclick = e => { const it = e.target.closest('[data-n]'); if (!it) return; const v = base.find(x => String(x.d.n) === it.dataset.n); T.flyTo(v.d.lon, v.d.lat, 120000, { orientation: { heading: 0, pitch: T.C.Math.toRadians(-35), roll: 0 } }); this.onPick(T, v.d); }; },
  onPick(T, d) {
    const w = weekly.get(key(d.name)), u = usgs.get(key(d.name));
    T.detail(d.name, `<div class="kv"><div>Country</div><div>${T.esc(d.country)}</div><div>Type</div><div>${T.esc(d.type)}</div><div>Summit</div><div>${d.elev != null ? d.elev.toLocaleString() + ' m' : '—'}</div>
      <div>Last eruption</div><div>${d.last == null ? 'unknown' : d.last < 0 ? Math.abs(d.last) + ' BCE' : d.last}</div>
      ${u ? `<div>USGS alert</div><div>${T.esc(u.alert_level)} · aviation ${T.esc(u.color_code)}</div>` : ''}</div>
      ${w ? `<b>This week</b><p class="small">${T.esc(w.text.slice(0, 900))}</p>` : ''}
      <button id="vGvp">Open its Smithsonian page</button>`, el => { el.querySelector('#vGvp').onclick = () => T.td.openExternal(`https://volcano.si.edu/volcano.cfm?vn=${d.n}`); });
  },
  async test() { return { volcanoes: base.length, active: base.filter(e => e.state).length, weeklyReports: weekly.size, usgsElevated: usgs.size }; },
};
