/* Find a province, state or country by name. */
export function createSearch(data, { onProvince, onState, onCountry }) {
  const input = document.getElementById('search');
  const list = document.getElementById('search-results');
  const index = [];
  for (const c of data.countries) index.push({ kind: 'country', name: c.name, key: c.name.toLowerCase(), id: c.tag });
  for (const s of data.states) index.push({ kind: 'state', name: s.name, key: s.name.toLowerCase(), id: s.id });
  for (const p of data.provinces) if (p) index.push({ kind: p.type === 'land' ? 'province' : p.type, name: p.name, key: p.name.toLowerCase(), id: p.id, vp: p.vp });
  index.sort((a, b) => (b.vp || 0) - (a.vp || 0));

  function render(q) {
    q = q.trim().toLowerCase();
    if (q.length < 2) { list.hidden = true; return; }
    const starts = [], contains = [];
    for (const e of index) {
      if (e.key.startsWith(q)) starts.push(e); else if (e.key.includes(q)) contains.push(e);
      if (starts.length > 12) break;
    }
    const hits = starts.concat(contains).slice(0, 12);
    list.innerHTML = hits.map((h, i) => `<li><button data-i="${i}">${h.name}<small>${h.kind}</small></button></li>`).join('') || '<li><button disabled>Nothing found</button></li>';
    list.hidden = false;
    list.querySelectorAll('button[data-i]').forEach((b) => b.addEventListener('click', () => go(hits[Number(b.dataset.i)])));
  }
  function go(h) {
    list.hidden = true; input.blur();
    if (h.kind === 'country') onCountry(h.id); else if (h.kind === 'state') onState(h.id); else onProvince(h.id);
  }
  input.addEventListener('input', () => render(input.value));
  input.addEventListener('keydown', (e) => {
    if (e.key === 'Enter') { const b = list.querySelector('button[data-i]'); if (b) b.click(); }
    if (e.key === 'Escape') { list.hidden = true; input.blur(); }
  });
  document.addEventListener('pointerdown', (e) => { if (!list.contains(e.target) && e.target !== input) list.hidden = true; });
}
