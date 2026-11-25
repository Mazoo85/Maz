// Other Worlds: the Moon, the planets and the Sun as globes on their real shapes (radii from the IAU), with
// Solar System Scope 2K textures (CC BY 4.0). Earth layers switch off while you visit.
const WORLDS = [
  ['earth', 'Earth', null, [6378137, 6356752]],
  ['moon', 'Moon', 'moon', [1737400]], ['mars', 'Mars', 'mars', [3396190, 3376200]], ['mercury', 'Mercury', 'mercury', [2439700]],
  ['venus', 'Venus (surface)', 'venus_surface', [6051800]], ['jupiter', 'Jupiter', 'jupiter', [71492000, 66854000]], ['saturn', 'Saturn', 'saturn', [60268000, 54364000]],
  ['uranus', 'Uranus', 'uranus', [25559000, 24973000]], ['neptune', 'Neptune', 'neptune', [24764000, 24341000]], ['sun', 'Sun', 'sun', [696340000]],
];
const FACTS = { moon: 'Radius 1,737 km · 384,400 km from Earth', mars: 'Radius 3,390 km · day 24 h 37 min · Olympus Mons 21.9 km', mercury: 'Radius 2,440 km · closest to the Sun', venus: 'Radius 6,052 km · surface 465 °C under the clouds', jupiter: 'Radius 69,911 km · Great Red Spot', saturn: 'Radius 58,232 km · rings not drawn', uranus: 'Radius 25,362 km · tilted on its side', neptune: 'Radius 24,622 km · fastest winds', sun: 'Radius 696,340 km' };
let current = 'earth', saved = [];

async function go(T, L, id) {
  const w = WORLDS.find(x => x[0] === id);
  // Each world's map is loaded before leaving Earth, so a failed load keeps you on Earth with a message.
  if (w[2]) { const r = await fetch(`assets/worlds/${w[2]}.jpg`).catch(() => null); if (!r || !r.ok) throw new Error(`The ${w[1]} map could not be downloaded. It needs the internet the first time you visit.`); }
  if (id !== 'earth' && current === 'earth') {
    // switch Earth's own layers off for the visit, and remember them
    saved = window.terra.layers.filter(l => l.on && l.worldOnly).map(l => l.id);
    for (const l of window.terra.layers.filter(l => l.on && l.worldOnly)) { l.on = false; await l.disable(T); l.statusText = ''; }
  }
  current = id;
  await T.setWorld(id, w[2] ? `assets/worlds/${w[2]}.jpg` : null, w[3]);
  T.status(L, w[1], current === 'earth' ? '' : 'live');
  if (id === 'earth') { for (const lid of saved) { const l = window.terra.layers.find(x => x.id === lid); if (l && !l.on) { l.on = true; await l.enable(T); } } saved = []; }
  T.panel(L);
}

export default {
  id: 'worlds', name: 'Other Worlds', group: 'History and worlds', tool: true, sub: 'The Moon, planets and Sun', credit: '',
  info: 'Fly around the Moon, Mars, Mercury, Venus, Jupiter, Saturn, Uranus, Neptune or the Sun.',
  async enable(T) { this.on = true; },
  async disable(T) { if (current !== 'earth') await go(T, this, 'earth'); },
  goEarth(T) { return go(T, this, 'earth'); },
  panel(T) {
    return `<div class="list">${WORLDS.map(w => `<div class="it" data-w="${w[0]}"><b>${w[1]}</b><span class="r">${current === w[0] ? 'here' : ''}</span>${FACTS[w[0]] ? `<span class="s">${FACTS[w[0]]}</span>` : ''}</div>`).join('')}</div>
      <p class="note">Textures from Solar System Scope (CC BY 4.0), based on NASA mission data. Earth's live layers switch off while you visit and come back when you return.</p>`;
  },
  wire(T, el) { el.querySelector('.list').onclick = e => { const it = e.target.closest('[data-w]'); if (it) go(T, this, it.dataset.w).catch(err => T.toast(err.message, true)); }; },
  async test(T) { await go(T, this, 'mars'); const r = { world: T.world(), imagery: T.viewer.imageryLayers.length }; await new Promise(res => setTimeout(res, 2500)); await go(T, this, 'earth'); r.back = T.world(); return r; },
};
