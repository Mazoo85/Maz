// Trail Finder: hiking routes from OpenStreetMap (route=hiking relations, through the Overpass API) in the area you
// are looking at, drawn on the 3D terrain with an elevation profile, and saved as GPX. Data © OpenStreetMap (ODbL).
let found = [], ent = null, cur = null;
const OVERPASS = 'https://overpass-api.de/api/interpreter';

async function search(T, L) {
  const [w, s, e, n] = T.view().box;
  if ((e - w) * (n - s) > 1.5) { T.toast('Zoom in closer (about a national park or a county) to look for trails.'); return; }
  T.status(L, 'searching…');
  const q = `[out:json][timeout:60];relation["route"~"hiking|foot"](${s},${w},${n},${e});out tags center 80;`;
  const j = await T.json(OVERPASS, { method: 'POST', body: 'data=' + encodeURIComponent(q), headers: { 'content-type': 'application/x-www-form-urlencoded' } });
  found = (j.elements || []).filter(x => x.tags && (x.tags.name || x.tags.ref)).map(x => ({ id: x.id, name: x.tags.name || x.tags.ref, net: x.tags.network || '', dist: x.tags.distance || '', from: x.tags.from || '', to: x.tags.to || '', center: x.center }));
  T.status(L, `${found.length} trails here`, '');
  T.panel(L);
}
async function open(T, L, t) {
  const j = await T.json(OVERPASS, { method: 'POST', body: 'data=' + encodeURIComponent(`[out:json][timeout:60];relation(${t.id});way(r);out geom;`), headers: { 'content-type': 'application/x-www-form-urlencoded' } });
  const lines = (j.elements || []).filter(x => x.geometry).map(x => x.geometry.map(g => [g.lon, g.lat]));
  if (!lines.length) throw new Error('this trail has no drawn path');
  const { C, viewer } = T;
  if (ent) await viewer.dataSources.remove(ent, true);
  ent = new C.CustomDataSource('trail'); await viewer.dataSources.add(ent);
  for (const l of lines) ent.entities.add({ polyline: { positions: C.Cartesian3.fromDegreesArray(l.flat()), width: 4, material: C.Color.fromCssColorString('#ff5c7a'), clampToGround: true } });
  viewer.flyTo(ent, { duration: 2 });
  // OpenStreetMap keeps a trail as unordered pieces: join them end to end (nearest free end, reversing as needed)
  // so the profile runs along the trail instead of jumping between pieces.
  const pts = chain(T, lines);
  const step = Math.max(1, Math.ceil(pts.length / 400));
  const sample = T.G.densify(pts.filter((_, i) => i % step === 0), 150).slice(0, 700);
  const hs = await Promise.all(sample.map(p => T.heightAt(p[0], p[1], 12)));
  // climb and descent with a 5 m dead band, so tile noise does not add up
  let up = 0, down = 0, ref = hs.find(h => h != null) ?? 0;
  for (const h of hs) { if (h == null) continue; const d = h - ref; if (d > 5) { up += d; ref = h; } else if (d < -5) { down -= d; ref = h; } }
  cur = { t, lines, hs, len: lines.reduce((a, l) => a + T.G.pathLength(l), 0), up, down };
  T.panel(L);
}
function chain(T, lines) {
  const left = lines.map(l => l.slice());
  left.sort((a, b) => b.length - a.length);
  let path = left.shift();
  while (left.length) {
    const end = path[path.length - 1], start = path[0];
    let best = null;
    left.forEach((l, i) => {
      for (const [d, mode] of [[T.G.distance(end, l[0]), 'end'], [T.G.distance(end, l[l.length - 1]), 'end-rev'], [T.G.distance(start, l[l.length - 1]), 'start'], [T.G.distance(start, l[0]), 'start-rev']])
        if (!best || d < best.d) best = { d, i, mode };
    });
    const l = left.splice(best.i, 1)[0];
    if (best.d > 3000) break;                        // a separate branch far away: stop the main line here
    if (best.mode === 'end') path = path.concat(l); else if (best.mode === 'end-rev') path = path.concat(l.reverse());
    else if (best.mode === 'start') path = l.concat(path); else path = l.reverse().concat(path);
  }
  return path;
}
function gpx(t, lines) {
  return `<?xml version="1.0" encoding="UTF-8"?>\n<gpx version="1.1" creator="Terra Desk (data © OpenStreetMap contributors, ODbL)" xmlns="http://www.topografix.com/GPX/1/1"><trk><name>${t.name.replace(/[<&>]/g, '')}</name>\n` +
    lines.map(l => '<trkseg>\n' + l.map(([lon, lat]) => `<trkpt lat="${lat.toFixed(6)}" lon="${lon.toFixed(6)}"></trkpt>`).join('\n') + '\n</trkseg>').join('\n') + '\n</trk></gpx>\n';
}
function drawProfile(c, hs) {
  const g = c.getContext('2d'), W = c.width = c.clientWidth * 2, H = c.height = 180, v = hs.filter(h => h != null), lo = Math.min(...v), hi = Math.max(...v);
  g.clearRect(0, 0, W, H); g.beginPath(); hs.forEach((h, i) => { const x = i / (hs.length - 1) * W, y = H - ((h ?? lo) - lo) / (hi - lo || 1) * (H - 10) - 5; i ? g.lineTo(x, y) : g.moveTo(x, y); });
  g.lineTo(W, H); g.lineTo(0, H); g.closePath(); g.fillStyle = 'rgba(255,92,122,.35)'; g.fill(); g.strokeStyle = '#ff5c7a'; g.lineWidth = 3; g.stroke();
}

export default {
  id: 'trails', name: 'Trail Finder', group: 'Tools', tool: true, worldOnly: true, sub: 'Hiking trails, profiles, GPX', credit: 'Trails: © OpenStreetMap contributors (ODbL)',
  info: 'Find hiking trails from OpenStreetMap where you are looking, see them on the 3D terrain with climb and descent, and save them as GPX.',
  async enable(T) { this.on = true; },
  async disable(T) { if (ent) await T.viewer.dataSources.remove(ent, true); ent = null; cur = null; },
  panel(T) {
    const u = T.settings().units;
    return `<p class="note">Zoom to a park or mountain area, then press Find.</p><button id="trFind" class="primary">Find trails here</button>
      <div class="list" id="trList">${found.map((t, i) => `<div class="it" data-i="${i}"><b>${T.esc(t.name)}</b><span class="r">${T.esc(t.dist ? t.dist + ' km' : '')}</span><span class="s">${T.esc([t.net, t.from && t.to ? t.from + ' → ' + t.to : ''].filter(Boolean).join(' · '))}</span></div>`).join('') || '<div class="it muted">No search yet.</div>'}</div>
      ${cur ? `<b>${T.esc(cur.t.name)}</b><div class="kv"><div>Length</div><div>${T.esc(T.G.formatDistance(cur.len, u))}</div><div>Climb</div><div>${Math.round(cur.up).toLocaleString()} m</div><div>Descent</div><div>${Math.round(cur.down).toLocaleString()} m</div></div><canvas class="profile" id="trCanvas"></canvas><div class="row"><button id="trGpx">Save GPX</button><button id="trWm">Open on Waymarked Trails</button></div>` : ''}`;
  },
  wire(T, el) {
    const L = this;
    el.querySelector('#trFind').onclick = () => search(T, L).catch(e => { T.status(L, 'error', 'err'); T.toast('Trails: ' + e.message, true); });
    el.querySelector('#trList').onclick = e => { const it = e.target.closest('[data-i]'); if (it) open(T, L, found[+it.dataset.i]).catch(er => T.toast('Trail: ' + er.message, true)); };
    if (cur) {
      drawProfile(el.querySelector('#trCanvas'), cur.hs);
      el.querySelector('#trGpx').onclick = async () => { const r = await T.td.saveFile({ name: cur.t.name.replace(/[\\/:*?"<>|]+/g, ' ') + '.gpx', data: gpx(cur.t, cur.lines), filters: [{ name: 'GPX', extensions: ['gpx'] }] }); if (r.file) T.toast('Saved ' + r.file); };
      el.querySelector('#trWm').onclick = () => T.td.openExternal(`https://hiking.waymarkedtrails.org/#route?id=${cur.t.id}`);
    }
  },
  async test(T) {
    T.viewer.camera.setView({ destination: T.C.Rectangle.fromDegrees(-105.75, 40.25, -105.55, 40.42) });   // Rocky Mountain National Park
    await search(T, this); if (found[0]) await open(T, this, found[0]);
    return { trails: found.length, first: found[0] && found[0].name, length_km: cur && Math.round(cur.len / 100) / 10, climb_m: cur && Math.round(cur.up) };
  },
};
