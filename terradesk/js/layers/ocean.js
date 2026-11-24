// Ocean Deep: the sea floor in 3D from the same terrain tiles (ETOPO1 and GEBCO bathymetry), with taller relief,
// famous deeps to fly to, and a depth profile between two clicks.
const DEEPS = [['Challenger Deep, Mariana Trench', 142.199, 11.371, -10935], ['Tonga Trench (Horizon Deep)', -174.73, -23.26, -10800], ['Puerto Rico Trench', -66.0, 19.8, -8376],
  ['Mid-Atlantic Ridge (Iceland to the Azores)', -30.0, 45.0, -2500], ['Hawaiian–Emperor seamounts', -160.0, 25.0, -5000], ['Java (Sunda) Trench', 106.0, -10.3, -7290], ['Kuril–Kamchatka Trench', 155.0, 46.0, -9600], ['Monterey Canyon', -122.05, 36.75, -2000]];
let ents = [], keepEx = null, prof = [], profEnt = null;

async function profile(T, a, b) {
  const pts = T.G.densify([a, b], Math.max(500, T.G.distance(a, b) / 160));
  const hs = await Promise.all(pts.map(p => T.heightAt(p[0], p[1], 9)));
  return { pts, hs, len: T.G.pathLength(pts) };
}
function drawProfile(canvas, hs) {
  const g = canvas.getContext('2d'); const W = canvas.width = canvas.clientWidth * 2, H = canvas.height = 180;
  const v = hs.filter(h => h != null), lo = Math.min(...v, 0), hi = Math.max(...v, 0);
  g.clearRect(0, 0, W, H); g.fillStyle = '#123a63'; g.fillRect(0, 0, W, H * (hi / (hi - lo || 1)) + 0);
  g.beginPath(); hs.forEach((h, i) => { const x = i / (hs.length - 1) * W, y = (hi - (h ?? 0)) / (hi - lo || 1) * H; i ? g.lineTo(x, y) : g.moveTo(x, y); });
  g.lineTo(W, H); g.lineTo(0, H); g.closePath(); g.fillStyle = '#c9a26b'; g.fill();
  g.strokeStyle = 'rgba(255,255,255,.4)'; g.beginPath(); const sy = hi / (hi - lo || 1) * H; g.moveTo(0, sy); g.lineTo(W, sy); g.stroke();
}

export default {
  id: 'ocean', name: 'Ocean Deep', group: 'History and worlds', worldOnly: true, sub: 'The sea floor in 3D', credit: 'Bathymetry: ETOPO1 / GEBCO via AWS Terrain Tiles',
  info: 'Drains the oceans: the sea floor in 3D with taller relief, the deepest places to fly to, and a depth profile between any two points.',
  async enable(T) {
    const { C, viewer } = T;
    keepEx = viewer.scene.verticalExaggeration;
    T.setOceanFloor(true);                                   // reloads the terrain (which resets the height), then:
    viewer.scene.verticalExaggeration = Math.max(3, keepEx);
    for (const [name, lon, lat, d] of DEEPS) { const e = viewer.entities.add({ position: C.Cartesian3.fromDegrees(lon, lat), point: { pixelSize: 8, color: C.Color.fromCssColorString('#8fd0ff'), outlineColor: C.Color.BLACK, outlineWidth: 1, heightReference: C.HeightReference.CLAMP_TO_GROUND, disableDepthTestDistance: Number.POSITIVE_INFINITY }, label: { text: name, font: '12px Segoe UI', pixelOffset: new C.Cartesian2(0, -14), showBackground: true, backgroundColor: C.Color.fromCssColorString('rgba(5,8,13,.75)'), disableDepthTestDistance: Number.POSITIVE_INFINITY, distanceDisplayCondition: new C.DistanceDisplayCondition(0, 1.2e7) } }); e.td = { layer: 'ocean', data: { name, lon, lat, d } }; ents.push(e); }
    T.status(this, '3D sea floor', 'live');
  },
  async disable(T) { T.setOceanFloor(false); T.viewer.scene.verticalExaggeration = keepEx || 1.2; for (const e of ents.splice(0)) T.viewer.entities.remove(e); if (profEnt) T.viewer.entities.remove(profEnt); profEnt = null; T.capture(null); },
  panel(T) {
    return `<p class="note">The sea floor now shows in 3D, three times taller than life. Tap the ocean to read its depth in the bottom bar.</p>
      <div class="list" id="odList">${DEEPS.map((d, i) => `<div class="it" data-i="${i}"><b>${d[0]}</b><span class="r">${Math.abs(d[3]).toLocaleString()} m</span></div>`).join('')}</div>
      <div class="row"><button id="odProf" class="primary">Depth profile</button><span class="muted small">tap two points on the globe</span></div>
      <canvas class="profile" id="odCanvas" hidden></canvas><div id="odOut" class="muted small"></div>`;
  },
  wire(T, el) {
    el.querySelector('#odList').onclick = e => { const it = e.target.closest('[data-i]'); if (!it) return; const d = DEEPS[+it.dataset.i]; T.viewer.camera.flyTo({ destination: T.C.Cartesian3.fromDegrees(d[1], d[2] - 1.2, 160000), orientation: { heading: 0, pitch: T.C.Math.toRadians(-25), roll: 0 }, duration: 3 }); };
    el.querySelector('#odProf').onclick = () => {
      prof = []; T.toast('Tap the start of the profile on the globe, then the end.');
      T.capture(async (lon, lat) => {
        prof.push([lon, lat]); if (prof.length < 2) return; T.capture(null);
        if (profEnt) T.viewer.entities.remove(profEnt);
        profEnt = T.viewer.entities.add({ polyline: { positions: T.C.Cartesian3.fromDegreesArray(prof.flat()), width: 3, material: T.C.Color.fromCssColorString('#8fd0ff'), clampToGround: true } });
        const r = await profile(T, prof[0], prof[1]); const c = el.querySelector('#odCanvas'); c.hidden = false; drawProfile(c, r.hs);
        const v = r.hs.filter(h => h != null);
        el.querySelector('#odOut').textContent = `${T.G.formatDistance(r.len, T.settings().units)} long · deepest ${Math.round(-Math.min(...v)).toLocaleString()} m · highest ${Math.round(Math.max(...v)).toLocaleString()} m`;
      });
    };
  },
  onPick(T, d) { T.detail(d.name, `<div class="kv"><div>Depth</div><div>${Math.abs(d.d).toLocaleString()} m</div><div>Where</div><div>${T.esc(T.G.formatCoord(d.lat, d.lon))}</div></div><button id="odGo">Fly down</button>`, el => { el.querySelector('#odGo').onclick = () => T.viewer.camera.flyTo({ destination: T.C.Cartesian3.fromDegrees(d.lon, d.lat - 1.2, 160000), orientation: { heading: 0, pitch: T.C.Math.toRadians(-25), roll: 0 }, duration: 3 }); }); },
  async test(T) { const h = await T.heightAt(142.199, 11.371, 8); return { challengerDeep: h != null ? Math.round(h) + ' m' : null, exaggeration: T.viewer.scene.verticalExaggeration }; },
};
