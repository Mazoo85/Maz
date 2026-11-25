// Ship Watch 3D: live AIS positions from aisstream.io (your free key). The main process holds the socket (the
// service forbids browser connections) and sends batches; ships older than 20 minutes fade out of the list.
const TYPES = [[20, 29, 'Wing in ground', '#9aa4b2'], [30, 30, 'Fishing', '#ffd166'], [31, 32, 'Towing', '#c9a227'], [35, 35, 'Military', '#ff6b6b'],
  [36, 37, 'Sailing / pleasure', '#c792ea'], [40, 49, 'High speed craft', '#89ddff'], [50, 59, 'Special (pilot, tug, rescue)', '#f78c6c'],
  [60, 69, 'Passenger', '#82aaff'], [70, 79, 'Cargo', '#66d19e'], [80, 89, 'Tanker', '#ff8a5c']];
const typeOf = t => TYPES.find(([a, b]) => t >= a && t <= b) || [0, 0, t == null ? 'Unknown yet' : 'Other', '#b0bec5'];
let pts = null, ships = new Map(), wired = false, sweep = 0;

function boxFor(T, v) { return v.height > 6e6 ? [[-90, -180], [90, 180]] : [[v.box[1], v.box[0]], [v.box[3], v.box[2]]]; }

export default {
  id: 'ships', name: 'Ship Watch 3D', group: 'Live now', worldOnly: true, sub: 'Live ships (AIS)', credit: 'Ships: aisstream.io',
  info: 'Live ship positions from AIS radio, worldwide, through aisstream.io. Needs your free aisstream.io key (Settings).',
  async enable(T) {
    const { C, viewer, td } = T;
    pts = viewer.scene.primitives.add(new C.PointPrimitiveCollection());
    if (!wired) {
      wired = true;
      td.on('ais', batch => {
        if (!pts) return;
        for (const s of batch) {
          if (s.lat == null || Math.abs(s.lat) > 90) continue;
          let e = ships.get(s.mmsi);
          if (!e) { e = { d: {}, p: pts.add({ pixelSize: 6, outlineWidth: 1, outlineColor: C.Color.BLACK, disableDepthTestDistance: Number.POSITIVE_INFINITY }) }; ships.set(s.mmsi, e); }
          for (const [k, v] of Object.entries(s)) if (v !== undefined && v !== '') e.d[k] = v;
          e.p.position = C.Cartesian3.fromDegrees(e.d.lon, e.d.lat, 5);
          e.p.color = C.Color.fromCssColorString(typeOf(e.d.type)[3]);
          e.p.id = { td: { layer: 'ships', data: e.d } };
        }
        this.statusOn && T.status(this, `${ships.size} ships`, 'live');
      });
      td.on('ais-status', s => {
        const m = { nokey: ['needs key', 'err'], connecting: ['connecting…', ''], live: [`${ships.size} ships`, 'live'], reconnecting: ['reconnecting…', ''], error: ['error', 'err'], off: ['', ''] }[s.status] || ['', ''];
        if (this.on) T.status(this, m[0], m[1]);
        if (s.status === 'error' && this.on) T.toast('Ships: ' + (s.message || 'the stream stopped'), true);
        if (s.status === 'nokey' && this.on) { T.toast('Ship Watch needs your free aisstream.io key: Settings → Keys.'); }
      });
    }
    this.statusOn = true;
    await td.aisStart(boxFor(T, T.view()));
    sweep = setInterval(() => { const old = Date.now() - 20 * 60 * 1000; for (const [k, e] of ships) if (e.d.t < old) { pts.remove(e.p); ships.delete(k); } }, 60000);
  },
  async disable(T) { this.statusOn = false; clearInterval(sweep); await T.td.aisStop(); if (pts) { T.viewer.scene.primitives.remove(pts); pts = null; } ships.clear(); },
  onView(T, v) { T.td.aisBox(boxFor(T, v)); },
  panel(T) {
    const s = T.settings();
    return `${s.hasAis ? '' : '<p class="warn">Add your free aisstream.io key in Settings to see ships.</p>'}
      <p class="note">Live AIS positions. Zoomed in, only ships in view are streamed; zoomed out, the whole world. Colour shows the kind of ship.</p>
      <div class="legend">${TYPES.map(t => `<span><i style="background:${t[3]}"></i>${t[2]}</span>`).join('')}</div>
      <div class="big">${ships.size}</div><span class="muted small">ships on the globe now</span>`;
  },
  onPick(T, d) {
    const t = typeOf(d.type);
    T.detail(d.name || `MMSI ${d.mmsi}`, `<div class="kv">
      <div>Kind</div><div>${T.esc(t[2])}</div><div>MMSI</div><div>${d.mmsi}</div><div>Call sign</div><div>${T.esc(d.callsign || '—')}</div>
      <div>Speed</div><div>${d.sog != null ? d.sog.toFixed(1) + ' knots' : '—'}</div><div>Course</div><div>${d.cog != null ? Math.round(d.cog) + '°' : '—'}</div>
      <div>Heading to</div><div>${T.esc(d.dest || '—')}</div><div>Length</div><div>${d.len ? d.len + ' m' : '—'}</div>
      <div>Position</div><div>${T.esc(T.G.formatCoord(d.lat, d.lon))}</div><div>Last seen</div><div>${new Date(d.t).toLocaleTimeString()}</div></div>`);
  },
  async test(T) { return { status: this.statusText || '', key: T.settings().hasAis ? 'saved' : 'none (expected until you add yours)' }; },
};
