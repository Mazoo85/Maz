// Battle Atlas: 8,740 battles from Wikidata (CC0) with places and dates, from 1500 BCE to today, bundled with the
// app. Pick a span of years or play history forward; filter by war or name. (No open data exists for troop
// movements, so battles are shown as dated places.)
let pts = null, all = [], from = 1914, to = 1918, filter = '', play = 0, span = 4;
const ERA = y => y < 500 ? '#c792ea' : y < 1500 ? '#82aaff' : y < 1800 ? '#66d19e' : y < 1900 ? '#ffd166' : y < 1946 ? '#ff8a5c' : '#ff5c7a';

function apply(T, L) {
  const f = filter.toLowerCase(); let n = 0;
  for (const e of all) {
    const show = e.d.year >= from && e.d.year <= to && (!f || e.d.name.toLowerCase().includes(f) || e.d.war.some(w => w.toLowerCase().includes(f)));
    e.p.show = show; if (show) n++;
  }
  T.status(L, `${n.toLocaleString()} battles`, '');
  const out = document.getElementById('bCount'); if (out) out.textContent = `${n.toLocaleString()} battles · ${yr(from)} to ${yr(to)}`;
}
const yr = y => y < 0 ? `${-y} BCE` : String(y);

export default {
  id: 'battles', name: 'Battle Atlas', group: 'History and worlds', worldOnly: true, sub: '8,740 battles through history', credit: 'Battles: Wikidata (CC0)',
  info: 'Every battle Wikidata records with a place and a date: 8,740 of them from 1500 BCE to 2026. Pick a span of years, play history forward, or search a war.',
  async enable(T) {
    const { C, viewer } = T;
    const list = await fetch('data/battles.json').then(r => r.json());
    pts = viewer.scene.primitives.add(new C.PointPrimitiveCollection());
    all = list.map(d => ({ d, p: pts.add({ position: C.Cartesian3.fromDegrees(d.lon, d.lat, 200), pixelSize: 6, color: C.Color.fromCssColorString(ERA(d.year)), outlineColor: C.Color.BLACK, outlineWidth: 1, id: { td: { layer: 'battles', data: d } }, disableDepthTestDistance: 3e6 }) }));
    apply(T, this);
  },
  async disable(T) { clearInterval(play); play = 0; if (pts) T.viewer.scene.primitives.remove(pts); pts = null; all = []; },
  panel(T) {
    return `<div class="row"><label class="f">From <input id="bFrom" type="number" value="${from}" style="width:90px"></label><label class="f">To <input id="bTo" type="number" value="${to}" style="width:90px"></label></div>
      <div class="row">${[['Ancient', -1500, 499], ['Medieval', 500, 1499], ['1500s–1700s', 1500, 1799], ['1800s', 1800, 1899], ['World War I', 1914, 1918], ['World War II', 1939, 1945], ['Since 1946', 1946, 2026]].map(([n, a, b]) => `<button class="small" data-a="${a}" data-b="${b}">${n}</button>`).join('')}</div>
      <input id="bFilter" placeholder="Search a war or battle, e.g. Napoleonic, Gettysburg" value="${T.esc(filter)}">
      <div class="row"><button id="bPlay" class="primary">${play ? 'Pause' : 'Play history'}</button><label class="f" style="flex-direction:row;align-items:center;gap:6px">window <input id="bSpan" type="number" value="${span}" min="1" style="width:60px"> years</label></div>
      <div id="bCount" class="muted small"></div>
      <div class="legend"><span><i style="background:#c792ea"></i>before 500</span><span><i style="background:#82aaff"></i>500–1499</span><span><i style="background:#66d19e"></i>1500–1799</span><span><i style="background:#ffd166"></i>1800s</span><span><i style="background:#ff8a5c"></i>1900–1945</span><span><i style="background:#ff5c7a"></i>since 1946</span></div>
      <p class="note">Negative years are BCE. Play moves the window forward one year about every half second.</p>`;
  },
  wire(T, el) {
    const L = this;
    const read = () => { from = +el.querySelector('#bFrom').value; to = +el.querySelector('#bTo').value; if (to < from) [from, to] = [to, from]; apply(T, L); };
    el.querySelector('#bFrom').onchange = read; el.querySelector('#bTo').onchange = read;
    for (const b of el.querySelectorAll('[data-a]')) b.onclick = () => { el.querySelector('#bFrom').value = b.dataset.a; el.querySelector('#bTo').value = b.dataset.b; read(); };
    el.querySelector('#bFilter').oninput = e => { filter = e.target.value.trim(); apply(T, L); };
    el.querySelector('#bSpan').onchange = e => { span = Math.max(1, +e.target.value || 1); };
    el.querySelector('#bPlay').onclick = e => {
      if (play) { clearInterval(play); play = 0; e.target.textContent = 'Play history'; return; }
      e.target.textContent = 'Pause'; to = from + span;
      play = setInterval(() => { from += 1; to = from + span; if (from > 2026) { clearInterval(play); play = 0; } el.querySelector('#bFrom').value = from; el.querySelector('#bTo').value = to; apply(T, L); }, 500);
    };
    apply(T, L);
  },
  onPick(T, d) {
    T.detail(d.name, `<div class="kv"><div>Date</div><div>${T.esc(d.year < 0 ? `${-d.year} BCE` : d.date)}</div><div>Part of</div><div>${T.esc(d.war.join(', ') || '—')}</div><div>Place</div><div>${T.esc(T.G.formatCoord(d.lat, d.lon))}</div></div>
      <div class="row">${d.wiki ? '<button id="bWiki">Read on Wikipedia</button>' : ''}<button id="bWd">Wikidata</button></div>`, el => {
      const w = el.querySelector('#bWiki'); if (w) w.onclick = () => T.td.openExternal(d.wiki);
      el.querySelector('#bWd').onclick = () => T.td.openExternal('https://www.wikidata.org/wiki/' + d.id);
    });
  },
  setRange(T, a, b, q) {
    if (a != null && a !== '' && isFinite(+a)) from = +a; if (b != null && b !== '' && isFinite(+b)) to = +b; if (to < from) [from, to] = [to, from];
    if (q != null) filter = String(q).trim(); apply(T, this);
    return { from, to, filter, shown: all.filter(e => e.p.show).length };
  },
  async test(T) { from = 1939; to = 1945; apply(T, this); T.viewer.camera.setView({ destination: T.C.Cartesian3.fromDegrees(15, 48, 5000000) }); return { total: all.length, ww2: all.filter(e => e.p.show).length }; },
};
