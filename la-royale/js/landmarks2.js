/*
 * More of Los Angeles: the theatres, studios, delis, murals, parks and
 * oddities that make each neighbourhood itself.
 *
 * These are smaller than the headline landmarks, and the squeezed map is
 * crowded, so each one starts at its real latitude/longitude and then
 * steps outwards to the nearest spot that is dry, reasonably flat and clear
 * of roads and of everything already built (see findSpot). Its name goes
 * on the map wherever it lands.
 *
 * Registered into landmarks.js's def() list, so pads, build order and the
 * landmark count all work the same way.
 */

export function register(def, h) {
  const { rotv, DTLA } = h;

  /* Where a footprint of radius `rad` fits near (x0, z0): spiral outwards
   * until every sample point is on dry, unclaimed ground. */
  function findSpot(api, x0, z0, rad, maxSlope = 6) {
    const { W, gh } = api;
    for (let ring = 0; ring <= 40; ring++) {
      const R = ring * 9;
      const n = ring === 0 ? 1 : 8 + ring * 3;
      for (let k = 0; k < n; k++) {
        const t = (k / n) * Math.PI * 2 + ring * 0.37;
        const x = x0 + Math.cos(t) * R, z = z0 + Math.sin(t) * R;
        let ok = true, lo = Infinity, hi = -Infinity;
        for (let s = 0; s < 17 && ok; s++) {
          const rr = s === 0 ? 0 : s <= 8 ? rad : rad * 0.5;
          const a = (s % 8) * (Math.PI / 4) + (s > 8 ? Math.PI / 8 : 0);
          const px = x + Math.cos(a) * rr, pz = z + Math.sin(a) * rr;
          if (Math.abs(px) > W.half - 30 || Math.abs(pz) > W.half - 30) { ok = false; break; }
          if (W.maskAt(px, pz) !== 0 || W.isSea(px, pz)) { ok = false; break; }
          const y = gh(px, pz);
          if (y < 1.2) { ok = false; break; }
          lo = Math.min(lo, y); hi = Math.max(hi, y);
        }
        if (ok && hi - lo <= maxSlope) return { x, z, y: lo };
      }
    }
    return null;
  }

  /* add(name, [lat, lon], radius, build, opts): place, build, claim, name. */
  const add = (name, ll, rad, build, opts = {}) =>
    def(name, ll, { poiR: 0 }, (api) => {
      const [x0, z0] = Array.isArray(ll[0]) ? ll[0] : h.P(ll[0], ll[1]);
      const s = findSpot(api, x0, z0, rad, opts.slope);
      if (!s) return;
      const a = opts.rot ?? 0;
      build(api, s.x, s.z, s.y, a, makeKit(api, s.x, s.z, a));
      api.reserveDisc(s.x, s.z, rad + 3);
      api.poi(name, s.x, s.z, opts.poiR || Math.max(40, rad * 1.8));
    });

  /* Small helpers bound to one site: local→world offsets and signs. */
  function makeKit(api, x, z, a) {
    const at = (lx, lz) => { const [ox, oz] = rotv(lx, lz, a); return [x + ox, z + oz]; };
    return {
      at,
      /* a flat sign on the front (local +z) face at depth d */
      sign(lx, y, d, w, hgt, str, fg, o = {}) {
        const [sx, sz] = at(lx, d + 0.25);
        api.text(sx, y, sz, w, hgt, str, fg, { ry: a, ...o });
      },
      /* a vertical blade sign sticking out from the facade, lit at night */
      blade(lx, y0, d, hgt, str, fg, bg = '#1b1b1b') {
        const [bx, bz] = at(lx, d + 1.2);
        api.box(bx, y0 + hgt / 2, bz, 0.6, hgt, 2.4, bg, { ry: a });
        const letters = str.split('').join('\n');
        for (const side of [-1, 1]) {
          const [tx, tz] = at(lx + side * 0.32, d + 1.2);
          api.text(tx, y0 + hgt / 2, tz, 2, hgt * 0.94, letters, fg, { ry: a + side * Math.PI / 2, glow: true, font: 40 });
        }
      },
      /* a theatre marquee: a lit box over the entrance */
      marquee(lx, y, d, w, str, fg = '#ffe9a8', bg = '#7a1212') {
        const [mx, mz] = at(lx, d + 1.6);
        api.box(mx, y, mz, w, 2.2, 3.2, bg, { ry: a });
        const [fx, fz] = at(lx, d + 3.25);
        api.text(fx, y, fz, w * 0.94, 1.8, str, fg, { ry: a, glow: true, font: 48 });
      }
    };
  }

  const pick = (api, arr) => api.rnd.pick(arr);

  /* ---------------------------------------------------------- Hollywood */

  // Disney's restored 1926 movie palace, with its glowing marquee
  add('El Capitan Theatre', [34.1013, -118.3394], 16, (api, x, z, y, a, k) => {
    api.building(x, z, 24, 26, 16, a, '#efe2c6', 2);
    k.marquee(0, y + 6, 13, 18, 'EL CAPITAN');
    k.blade(8, y + 8, 13, 12, 'CAPITAN', '#ffd54a', '#123b7a');
  });
  // Sid Grauman's 1922 Egyptian, the first Hollywood premiere house
  add('Egyptian Theatre', [34.1015, -118.3366], 16, (api, x, z, y, a, k) => {
    api.building(x, z, 22, 26, 11, a, '#d6b98a', 1);
    for (const lx of [-5, 5]) { const [cx, cz] = k.at(lx, 15); api.cyl(cx, y + 5, cz, 1.8, 10, 1.8, '#c79f62'); }
    k.sign(0, y + 10.5, 13, 16, 2, 'EGYPTIAN', '#2a5a8a', { bg: '#e8d3a8' });
  });
  // The 1927 hotel where the first Academy Awards were held
  add('Hollywood Roosevelt', [34.1011, -118.3416], 20, (api, x, z, y, a, k) => {
    api.building(x, z, 34, 22, 38, a, '#f1e4c8', 2, 'hip', '#a8492b');
    const [rx, rz] = k.at(0, 0);
    api.box(rx, y + 42, rz, 0.4, 5, 24, '#333', { ry: a + Math.PI / 2 });
    k.sign(0, y + 43, 1, 22, 3.6, 'Hollywood Roosevelt', '#ff3b4f', { glow: true, font: 60 });
  });
  // Hollywood's oldest restaurant, 1919
  add('Musso & Frank Grill', [34.1016, -118.3355], 10, (api, x, z, y, a, k) => {
    api.building(x, z, 16, 14, 6, a, '#7d3a2c', 1);
    k.sign(0, y + 5, 7, 12, 1.6, 'MUSSO & FRANK GRILL', '#fff4d6', { bg: '#1d3a2a', font: 40 });
  });
  // The giant record store on Sunset
  add('Amoeba Music', [34.0979, -118.3283], 18, (api, x, z, y, a, k) => {
    api.building(x, z, 34, 26, 9, a, '#2b2b2b', 4);
    k.sign(0, y + 7.5, 13, 22, 3.2, 'AMOEBA', '#ffcf3f', { bg: '#c0262d', font: 80 });
  });
  // The arched Bronson Gate on Melrose, soundstages behind
  add('Paramount Pictures', [34.0838, -118.3189], 34, (api, x, z, y, a, k) => {
    for (const s of [-1, 1]) { const [px, pz] = k.at(s * 7, 26); api.box(px, y + 5, pz, 3, 10, 3, '#efe6d2', { col: true }); }
    const [gx, gz] = k.at(0, 26);
    api.arch(gx, y + 8, gz, 7, 1.2, 2.4, a, '#efe6d2', 9);
    api.text(gx + Math.sin(a) * 1.3, y + 12.5, gz + Math.cos(a) * 1.3, 12, 2, 'PARAMOUNT', '#1d3f8a', { ry: a, font: 60 });
    for (const [lx, lz] of [[-18, -6], [18, -6], [0, -14]]) { const [bx, bz] = k.at(lx, lz); api.building(bx, bz, 22, 18, 15, a, '#dccfb4', 4); }
    const [tx, tz] = k.at(24, 14);
    api.cyl(tx, y + 20, tz, 8, 6, 8, '#d9d9d9');
    for (const [lx, lz] of [[-3, -3], [3, -3], [-3, 3], [3, 3]]) api.box(tx + lx, y + 8.5, tz + lz, 0.4, 17, 0.4, '#888');
  });
  // The chili-dog stand on La Brea, open since 1939
  add("Pink's Hot Dogs", [34.0839, -118.3446], 8, (api, x, z, y, a, k) => {
    api.building(x, z, 10, 8, 4, a, '#f6f3ea', 1);
    k.sign(0, y + 5, 4, 10, 2, "PINK'S", '#e8508f', { bg: '#ffffff', font: 80 });
  });
  // The 24-hour deli on Fairfax
  add("Canter's Deli", [34.0790, -118.3612], 10, (api, x, z, y, a, k) => {
    api.building(x, z, 18, 14, 7, a, '#c9b18a', 2);
    k.sign(0, y + 6, 7, 14, 2.2, "CANTER'S", '#ff7a2a', { glow: true, font: 64 });
  });
  // Where the stars are buried: lawns, headstones, palms, a lake
  add('Hollywood Forever Cemetery', [34.0897, -118.3194], 48, (api, x, z, y, a, k) => {
    api.paint({ t: 'circle', x, z, r: 48, c: '#6f9a4f' });
    api.paint({ t: 'ellipse', x: x + 18, z: z + 14, rx: 10, rz: 6, rot: 0, c: '#3d8fa8' }, 1);
    for (let i = -4; i <= 4; i++) for (let j = -3; j <= 3; j++) {
      if ((i + j) % 2) continue;
      const [sx, sz] = k.at(i * 8 - 6, j * 8 - 4);
      api.box(sx, api.gh(sx, sz) + 0.6, sz, 0.9, 1.2, 0.3, pick(api, ['#d9d6cf', '#c9c4ba', '#e8e4dc']), { ry: a });
    }
    const [mx, mz] = k.at(-20, 22);
    api.building(mx, mz, 16, 10, 7, a, '#e8e2d4', 0);
    for (let i = 0; i < 6; i++) { const [px, pz] = k.at(-40 + i * 16, -34); api.tree(px, pz, 0, 20); }
  }, { slope: 8 });
  // The TV studio on Fairfax where "The Price Is Right" tapes
  add('CBS Television City', [34.0745, -118.3601], 26, (api, x, z, y, a, k) => {
    api.building(x, z, 48, 36, 18, a, '#1d1d1d', 0);
    for (let i = 0; i < 6; i++) { const [px, pz] = k.at(-20 + i * 8, 18.2); api.box(px, y + 9, pz, 3.5, 18, 0.3, '#f4f4f4', { ry: a }); }
    k.sign(0, y + 15, 18.6, 18, 2.4, 'TELEVISION CITY', '#ffffff', { font: 50 });
  });
  // "The Blue Whale" and its green and red siblings in West Hollywood
  add('Pacific Design Center', [34.0815, -118.3846], 40, (api, x, z, y, a, k) => {
    for (const [lx, lz, w, d, hh, c] of [[-22, 0, 36, 26, 30, '#2d63b8'], [14, -10, 26, 22, 38, '#3e9a54'], [20, 16, 20, 18, 34, '#c0392b']]) {
      const [bx, bz] = k.at(lx, lz);
      api.building(bx, bz, w, d, hh, a, c, 3);
      api.prism(bx, y + hh + 4, bz, w * 0.7, 8, d, c, { ry: a, rz: Math.PI / 2 });
    }
  });
  // The mall that looks like a parking garage with a mall on top
  add('Beverly Center', [34.0754, -118.3768], 36, (api, x, z, y, a, k) => {
    api.building(x, z, 66, 52, 28, a, '#a9a39a', 4);
    for (let f = 1; f < 4; f++) { const [sx, sz] = k.at(0, 26.2); api.box(sx, y + f * 6.5, sz, 64, 0.6, 0.3, '#d8d2c6', { ry: a }); }
    k.sign(0, y + 24, 26.3, 30, 3, 'BEVERLY CENTER', '#ffffff', { font: 56 });
  });
  // The hospital of the stars
  add('Cedars-Sinai', [34.0754, -118.3803], 34, (api, x, z, y, a, k) => {
    for (const [lx, lz, hh] of [[-16, -8, 42], [14, -8, 34], [0, 16, 24]]) { const [bx, bz] = k.at(lx, lz); api.building(bx, bz, 24, 22, hh, a, '#e7e3dc', 2); }
    const [hx, hz] = k.at(-16, -8);
    api.cyl(hx, y + 42.2, hz, 14, 0.4, 14, '#4a4d52');
  });

  /* ------------------------------------------------------- Griffith Park */

  // The open-air amphitheatre in the canyon
  add('Greek Theatre', [34.1196, -118.2962], 34, (api, x, z, y, a, k) => {
    const [sx, sz] = k.at(0, -18);
    api.box(sx, y + 4, sz, 34, 8, 10, '#efe6d2', { ry: a, col: true });
    for (let i = -3; i <= 3; i++) { const [cx, cz] = k.at(i * 4.5, -12.5); api.cyl(cx, y + 4, cz, 1.2, 8, 1.2, '#f6f1e6'); }
    for (let r = 0; r < 8; r++) {
      const R = 10 + r * 3;
      for (let s = -3; s <= 3; s++) {
        const t = s * 0.28;
        const [px, pz] = k.at(Math.sin(t) * R, -18 + Math.cos(t) * R);
        api.box(px, y + 0.4 + r * 0.7, pz, 7, 0.6, 2.4, '#b9b3a6', { ry: a - t });
      }
    }
  }, { slope: 14 });
  // The zoo's entrance gate
  add('Los Angeles Zoo', [34.1484, -118.2841], 40, (api, x, z, y, a, k) => {
    api.paint({ t: 'circle', x, z, r: 40, c: '#7f9c55' });
    for (const s of [-1, 1]) { const [px, pz] = k.at(s * 9, 20); api.box(px, y + 4, pz, 3, 8, 3, '#8a6f55', { col: true }); }
    const [gx, gz] = k.at(0, 20);
    api.box(gx, y + 8.6, gz, 22, 1.4, 3, '#5d4a37', { ry: a });
    api.text(gx + Math.sin(a) * 1.6, y + 8.6, gz + Math.cos(a) * 1.6, 18, 1.2, 'LOS ANGELES ZOO', '#ffe08a', { ry: a, font: 40 });
    for (let i = 0; i < 6; i++) { const [px, pz] = k.at(-26 + i * 10, -6 + (i % 2) * 12); api.tree(px, pz, i % 3 ? 1 : 0, 14); }
  }, { slope: 14 });
  // The Western-heritage museum across from the zoo
  add('Autry Museum', [34.1486, -118.2810], 22, (api, x, z, y, a, k) => {
    api.building(x, z, 40, 28, 12, a, '#c99b6d', 1, 'hip', '#a8492b');
    k.sign(0, y + 9, 14, 14, 2, 'AUTRY', '#3b2a1a', { bg: '#e8d3a8' });
  }, { slope: 14 });
  // The Batcave: three dark tunnel mouths in a quarry face
  add('Bronson Caves', [34.1215, -118.3138], 22, (api, x, z, y, a, k) => {
    const [rx, rz] = k.at(0, -6);
    api.box(rx, y + 8, rz, 36, 16, 12, '#9a8f7c', { ry: a, col: true });
    for (const lx of [-9, 0, 9]) { const [cx, cz] = k.at(lx, 0.1); api.box(cx, y + 3, cz, 5, 6, 0.4, '#0e0c0a', { ry: a }); }
  }, { slope: 12 });

  /* ---------------------------------------------------------- Downtown */

  const dtRot = DTLA.rot;
  // The 1893 office building with the iron-lace atrium from "Blade Runner"
  add('Bradbury Building', [34.0505, -118.2479], 16, (api, x, z, y, a, k) => {
    api.building(x, z, 26, 22, 18, a, '#8d4a35', 2);
    k.sign(0, y + 4, 11, 12, 1.4, 'BRADBURY BUILDING', '#f4e6c8', { bg: '#3a1e14', font: 36 });
  }, { rot: dtRot });
  // The little orange funicular up Bunker Hill
  add('Angels Flight', [34.0513, -118.2502], 22, (api, x, z, y, a, k) => {
    const rise = 9, run = 30, ang = Math.atan2(rise, run);
    const [tx, tz] = k.at(0, 0);
    api.box(tx, y + rise / 2, tz, Math.hypot(run, rise), 0.6, 4, '#5d5246', { ry: a, rz: ang });
    for (const [t, c] of [[-0.25, '#e8692a'], [0.25, '#e8692a']]) {
      const [cx, cz] = k.at(t * run, t < 0 ? -1 : 1);
      api.box(cx, y + rise / 2 + t * rise + 2, cz, 5, 3, 2.4, c, { ry: a, rz: ang });
    }
    const [ax, az] = k.at(-run / 2 - 2, 0);
    api.arch(ax, y, az, 3.4, 0.8, 1.2, a + Math.PI / 2, '#e8692a', 7);
    api.text(ax, y + 4.2, az, 6, 1, 'ANGELS FLIGHT', '#ffffff', { ry: a - Math.PI / 2, bg: '#1d1d1d', font: 30 });
  }, { rot: dtRot });
  // Broadway's movie palaces and their blade signs
  for (const [name, ll, word, fg, bg] of [
    ['Million Dollar Theatre', [34.0502, -118.2487], 'MILLION DOLLAR', '#ffe27a', '#7a1212'],
    ['Los Angeles Theatre', [34.0451, -118.2554], 'LOS ANGELES', '#ff5a7a', '#1b1b1b'],
    ['Orpheum Theatre', [34.0429, -118.2563], 'ORPHEUM', '#ff3b3b', '#f2efe8']
  ]) {
    add(name, ll, 14, (api, x, z, y, a, k) => {
      api.building(x, z, 22, 24, 22, a, pick(api, ['#d9c7a7', '#c9b28c', '#e8dcc0']), 2);
      k.blade(0, y + 7, 12, 14, word.split(' ').pop(), fg, bg);
      k.marquee(0, y + 5, 12, 18, word);
    }, { rot: dtRot });
  }
  // The bookshop in an old bank, with a tunnel made of books
  add('The Last Bookstore', [34.0477, -118.2495], 14, (api, x, z, y, a, k) => {
    api.building(x, z, 24, 20, 14, a, '#c9b28c', 2);
    k.sign(0, y + 6, 10, 16, 2, 'THE LAST BOOKSTORE', '#ffffff', { bg: '#1b1b1b', font: 40 });
  }, { rot: dtRot });
  // Downtown's square, with its purple campanile and yellow ball
  add('Pershing Square', [34.0484, -118.2525], 36, (api, x, z, y, a, k) => {
    api.paint({ t: 'rect', x, z, w: 66, d: 66, rot: a, c: '#c4a77d' }, 1);
    api.paint({ t: 'circle', x, z, r: 9, c: '#7fbfd8' }, 1);
    const [tx, tz] = k.at(-24, -24);
    api.box(tx, y + 15, tz, 4, 30, 4, '#7b3fa0', { ry: a, col: true });
    api.sphere(tx, y + 32, tz, 4, 4, 4, '#f4c430');
    for (let i = 0; i < 4; i++) { const [px, pz] = k.at(20, -24 + i * 14); api.tree(px, pz, 0, 18); }
  }, { rot: dtRot });
  // Rafael Moneo's ochre cathedral and its bell tower
  add('Cathedral of Our Lady of the Angels', [34.0577, -118.2452], 34, (api, x, z, y, a, k) => {
    api.building(x, z, 56, 34, 26, a, '#c9a46e', 0);
    const [r1x, r1z] = k.at(0, 0);
    api.box(r1x, y + 28, r1z, 50, 4, 30, '#b8915c', { ry: a + 0.08 });
    const [cx, cz] = k.at(36, 10);
    api.box(cx, y + 20, cz, 5, 40, 5, '#c9a46e', { ry: a, col: true });
    api.box(cx, y + 41, cz, 1, 3, 4, '#d9d4c8', { ry: a });
  });
  // The Music Center: the Dorothy Chandler's colonnade on Grand Ave
  add('Dorothy Chandler Pavilion', [34.0577, -118.2489], 30, (api, x, z, y, a, k) => {
    api.building(x, z, 50, 40, 26, a, '#f2efe8', 1);
    for (let i = -6; i <= 6; i++) { const [cx, cz] = k.at(i * 3.8, 21.5); api.cyl(cx, y + 13, cz, 1.4, 26, 1.4, '#ffffff'); }
    const [rx, rz] = k.at(0, 0);
    api.box(rx, y + 26.6, rz, 54, 1.2, 46, '#e6e1d6', { ry: a });
  });
  // The art deco Times Mirror building
  add('Los Angeles Times Building', [34.0525, -118.2453], 22, (api, x, z, y, a, k) => {
    api.building(x, z, 40, 34, 16, a, '#e6dccb', 2);
    const [tx, tz] = k.at(-8, 4);
    api.building(tx, tz, 14, 14, 40, a, '#e6dccb', 2);
    k.sign(-8, y + 36, 7.5, 10, 1.6, 'Los Angeles Times', '#1b1b1b', { bg: '#e6dccb', font: 34 });
  }, { rot: dtRot });
  // Little Tokyo's Japanese Village Plaza and its fire-watch tower
  add('Japanese Village Plaza', [34.0490, -118.2400], 24, (api, x, z, y, a, k) => {
    for (let i = 0; i < 6; i++) { const [bx, bz] = k.at(-15 + i * 6, (i % 2 ? 1 : -1) * 9); api.building(bx, bz, 5.5, 7, 5, a, '#f2ead8', 1, 'hip', '#2f3a44'); }
    const [tx, tz] = k.at(0, 20);
    for (const [lx, lz] of [[-1.5, -1.5], [1.5, -1.5], [-1.5, 1.5], [1.5, 1.5]]) { const [px, pz] = k.at(lx, 20 + lz); api.box(px, y + 7, pz, 0.5, 14, 0.5, '#7a4b2e'); }
    api.box(tx, y + 14.5, tz, 4.5, 2.2, 4.5, '#7a4b2e', { ry: a });
    api.roof(tx, y + 15.6, tz, 5.5, 2.6, 5.5, '#2f3a44', { ry: a });
    const [lx, lz] = k.at(10, 18);
    api.sphere(lx, y + 4, lz, 1.4, 1.8, 1.4, '#e63946', { m: 'glow' });
  });
  // LAFC's stadium beside the Coliseum
  add('BMO Stadium', [34.0128, -118.2841], 46, (api, x, z, y, a, k) => {
    api.paint({ t: 'rect', x, z, w: 70, d: 44, rot: a, c: '#3f8f3f' }, 1);
    for (const [lx, lz, w, d] of [[0, -28, 80, 10], [0, 28, 80, 10], [-41, 0, 10, 50], [41, 0, 10, 50]]) {
      const [bx, bz] = k.at(lx, lz);
      api.box(bx, y + 9, bz, w, 18, d, '#1b1b1b', { ry: a, col: true });
    }
    const [rx, rz] = k.at(0, -28);
    api.box(rx, y + 19, rz, 84, 1, 16, '#c9a54a', { ry: a, m: 'metal' });
  });
  // USC's Bovard Auditorium tower
  add('USC', [34.0205, -118.2856], 30, (api, x, z, y, a, k) => {
    api.paint({ t: 'rect', x, z: z + 20, w: 50, d: 22, rot: a, c: '#6f9a4f' }, 1);
    api.building(x, z - 6, 44, 22, 14, a, '#a5543d', 2, 'hip', '#b5562f');
    const [tx, tz] = k.at(0, -6);
    api.building(tx, tz, 9, 9, 38, a, '#a5543d', 2);
    api.cone(tx, y + 41, tz, 7, 6, 7, '#b5562f');
    k.sign(0, y + 10, 5.2, 8, 1.6, 'FIGHT ON', '#ffc72c', { bg: '#990000', font: 40 });
  });

  /* ------------------------------------------- Mid-City and Koreatown */

  // The teal art deco Wiltern at Wilshire and Western
  add('The Wiltern', [34.0617, -118.3087], 18, (api, x, z, y, a, k) => {
    api.building(x, z, 30, 26, 14, a, '#5aa7a0', 2);
    const [tx, tz] = k.at(-10, 8);
    api.building(tx, tz, 10, 10, 44, a, '#5aa7a0', 2);
    k.blade(-10, y + 16, 13, 16, 'WILTERN', '#ffffff', '#0f5f5a');
  });
  // The Academy Museum's glass sphere next to LACMA
  add('Academy Museum', [34.0635, -118.3610], 22, (api, x, z, y, a, k) => {
    api.building(x - 8, z, 26, 26, 22, a, '#d8b37a', 2);
    const [sx, sz] = k.at(12, 0);
    for (const [lx, lz] of [[-5, -5], [5, -5], [-5, 5], [5, 5]]) api.cyl(sx + lx, y + 3, sz + lz, 1, 6, 1, '#d9d9d9');
    api.sphere(sx, y + 14, sz, 24, 18, 24, '#cfe4f2', { m: 'glass', col: true });
  });
  // The El Rey's neon on Wilshire's Miracle Mile
  add('El Rey Theatre', [34.0617, -118.3477], 12, (api, x, z, y, a, k) => {
    api.building(x, z, 18, 22, 11, a, '#e98aa3', 2);
    k.blade(0, y + 9, 11, 10, 'EL REY', '#7fff9b', '#1b1b1b');
  });

  /* ---------------------------------------------------------- Westside */

  // Frank Gehry's Binoculars Building on Main Street, Venice
  add('Binoculars Building', [33.9945, -118.4652], 18, (api, x, z, y, a, k) => {
    api.building(x, z, 30, 18, 14, a, '#e9e2d0', 2);
    for (const lx of [-2.2, 2.2]) {
      const [bx, bz] = k.at(lx, 10);
      api.cyl(bx, y + 7, bz, 4.2, 14, 4.2, '#1b1b1b', { col: true });
    }
    const [cx, cz] = k.at(0, 10);
    api.box(cx, y + 9, cz, 2, 4, 3, '#1b1b1b', { ry: a });
  });
  // The Ballerina Clown on Rose Avenue
  add('Ballerina Clown', [33.9952, -118.4743], 9, (api, x, z, y, a, k) => {
    api.building(x, z, 14, 12, 8, a, '#efe6d2', 1);
    const [sx, sz] = k.at(0, 6.5);
    api.box(sx, y + 11, sz, 1.6, 4, 1, '#f2f2f2', { ry: a });
    api.cone(sx, y + 9, sz, 3.4, 2, 3.4, '#ff8fc0');
    api.sphere(sx, y + 13.8, sz, 1.6, 1.8, 1.6, '#f2d1b5');
    api.cone(sx, y + 15.2, sz, 0.9, 1.6, 0.9, '#3a6fd8');
  });
  // Abbot Kinney Boulevard's boutiques
  add('Abbot Kinney', [33.9910, -118.4645], 22, (api, x, z, y, a, k) => {
    for (let i = 0; i < 6; i++) {
      const [bx, bz] = k.at(-15 + i * 6, (i % 2 ? 8 : -8));
      api.building(bx, bz, 5.6, 9, api.rnd.range(4, 7), a, pick(api, ['#9fd3d6', '#f4d06f', '#f28f6b', '#a8d5a2', '#f6f1e4', '#7fb7d9']), 1);
    }
  });
  // Santa Monica Place, the open-air mall at the end of the Promenade
  add('Santa Monica Place', [34.0137, -118.4945], 34, (api, x, z, y, a, k) => {
    for (const [lx, lz] of [[-14, 0], [14, 0]]) { const [bx, bz] = k.at(lx, lz); api.building(bx, bz, 22, 40, 14, a, '#e8e2d6', 2); }
    k.sign(0, y + 12, 20.2, 18, 2, 'SANTA MONICA PLACE', '#1b1b1b', { bg: '#f4f1ea', font: 40 });
  });
  // Palisades Park: the row of palms along the bluff above PCH
  add('Palisades Park', [34.0210, -118.5080], 10, (api, x, z, y, a, k) => {
    const f = h.shoreFrame(x, z);
    for (let i = -6; i <= 6; i++) {
      const px = x + f.tx * i * 14, pz = z + f.tz * i * 14;
      if (api.W.maskAt(px, pz) === 0 && !api.W.isSea(px, pz)) api.tree(px, pz, 0, api.rnd.range(20, 28));
    }
    void k;
  });
  // The Annenberg Community Beach House, Marion Davies' old estate
  add('Annenberg Beach House', [34.0295, -118.5195], 16, (api, x, z, y, a, k) => {
    api.building(x, z, 24, 16, 7, a, '#f4f1ea', 1);
    const [px, pz] = k.at(0, 14);
    api.paint({ t: 'rect', x: px, z: pz, w: 16, d: 6, rot: a, c: '#48c0e6' }, 1);
  });
  // Westwood Village and the Fox Theatre's white tower
  add('Fox Theatre Westwood', [34.0631, -118.4471], 16, (api, x, z, y, a, k) => {
    api.building(x, z, 24, 22, 12, a, '#f1ece2', 2, 'hip', '#b5562f');
    const [tx, tz] = k.at(-6, 6);
    api.building(tx, tz, 7, 7, 32, a, '#f6f3ec', 1);
    k.blade(-6, y + 18, 9.5, 12, 'FOX', '#3a7bd5', '#ffffff');
  });
  // The gates of Bel Air on Sunset
  add('Bel Air Gate', [34.0817, -118.4432], 12, (api, x, z, y, a, k) => {
    for (const s of [-1, 1]) { const [px, pz] = k.at(s * 7, 0); api.box(px, y + 2.5, pz, 2, 5, 2, '#efe6d2', { col: true }); }
    const [gx, gz] = k.at(0, 0);
    api.arch(gx, y + 4, gz, 6, 0.5, 0.5, a, '#2b2b2b', 9);
    api.text(gx, y + 7.4, gz, 7, 1, 'BEL AIR', '#c9a54a', { ry: a, font: 40 });
  }, { slope: 8 });
  // The 1931 bakery turned design district in Culver City
  add('Helms Bakery', [34.0303, -118.3870], 18, (api, x, z, y, a, k) => {
    api.building(x, z, 36, 22, 10, a, '#efe2c4', 2);
    const [sx, sz] = k.at(0, 0);
    api.box(sx, y + 13, sz, 0.4, 4, 16, '#333', { ry: a + Math.PI / 2 });
    k.sign(0, y + 14, 0.5, 14, 3, 'HELMS', '#e63946', { glow: true, font: 80 });
  });
  // The flatiron Culver Hotel
  add('Culver Hotel', [34.0233, -118.3962], 12, (api, x, z, y, a, k) => {
    api.prism(x, y + 13, z, 22, 26, 22, '#d9b48a', { ry: a + 0.5, m: 'win' });
    api.W.addCircle(x, z, 8, y, y + 26, 'building');
    k.sign(0, y + 24, 6, 10, 1.4, 'CULVER HOTEL', '#ffffff', { glow: true, font: 36 });
  });
  // The 282 steps up the Baldwin Hills from Culver City
  add('Baldwin Hills Scenic Overlook', [34.0177, -118.3836], 14, (api, x, z, y, a, k) => {
    const top = { x: -260, z: 140 };
    const dx = top.x - x, dz = top.z - z, L = Math.hypot(dx, dz);
    const ex = x + (dx / L) * Math.min(L, 120), ez = z + (dz / L) * Math.min(L, 120);
    const yTop = api.gh(ex, ez);
    if (yTop > y + 6) api.ramp(x, z, y, ex, ez, yTop, 2.4, '#bdb6a6', 0.3);
    void k;
  }, { slope: 10 });
  // Kenneth Hahn Recreation Area's fishing lake among the oil wells
  add('Kenneth Hahn Park', [34.0082, -118.3664], 30, (api, x, z) => {
    api.paint({ t: 'circle', x, z, r: 30, c: '#6f9a4f' });
    api.paint({ t: 'ellipse', x, z, rx: 14, rz: 9, rot: 0.3, c: '#3d8fa8' }, 1);
    for (let i = 0; i < 6; i++) { const t = (i / 6) * 6.28; api.tree(x + Math.cos(t) * 22, z + Math.sin(t) * 22, 1, 12); }
  }, { slope: 10 });

  /* ----------------------------------------------------- South and East */

  // Leimert Park's plaza, heart of Black LA's arts scene
  add('Leimert Park Plaza', [34.0066, -118.3317], 22, (api, x, z, y, a, k) => {
    api.paint({ t: 'rect', x, z, w: 40, d: 28, rot: a, c: '#7f9c55' }, 1);
    api.cyl(x, y + 1, z, 6, 2, 6, '#c9b48f', { col: true });
    api.cyl(x, y + 2.4, z, 1, 1.5, 1, '#9fd8ff', { m: 'glow' });
    for (let i = 0; i < 5; i++) { const [bx, bz] = k.at(-16 + i * 8, -18); api.building(bx, bz, 7, 8, 6, a, pick(api, ['#e9a03b', '#c0392b', '#2e86c1', '#f4d06f']), 1); }
  });
  // The Crenshaw mall and its tall sign
  add('Baldwin Hills Crenshaw Plaza', [34.0097, -118.3355], 30, (api, x, z, y, a, k) => {
    api.building(x, z, 56, 36, 14, a, '#d8cfc0', 2);
    const [sx, sz] = k.at(30, 22);
    api.box(sx, y + 9, sz, 1.2, 18, 1.2, '#666');
    api.text(sx, y + 18, sz, 12, 2.4, 'CRENSHAW PLAZA', '#ffffff', { ry: a, bg: '#7a1f4f', font: 40 });
  });
  // The hog-farm mural wrapping the old Farmer John plant in Vernon
  add('Farmer John Mural', [34.0063, -118.2205], 26, (api, x, z, y, a, k) => {
    api.building(x, z, 52, 30, 10, a, '#8fc3e0', 4);
    k.sign(0, y + 6, 15, 46, 6, 'FARMER JOHN', '#e8508f', { bg: '#a8d8f0', font: 90 });
  });
  // The art deco Sears tower in Boyle Heights
  add('Sears Boyle Heights', [34.0335, -118.2135], 28, (api, x, z, y, a, k) => {
    api.building(x, z, 60, 40, 24, a, '#dcd2bf', 2);
    const [tx, tz] = k.at(0, 6);
    api.building(tx, tz, 16, 16, 62, a, '#dcd2bf', 2);
    k.blade(0, y + 40, 14, 18, 'SEARS', '#3a7bd5', '#f4f1ea');
  });
  // Hollenbeck Park's lake under the freeway
  add('Hollenbeck Park', [34.0396, -118.2181], 30, (api, x, z) => {
    api.paint({ t: 'circle', x, z, r: 30, c: '#6f9a4f' });
    api.paint({ t: 'ellipse', x, z, rx: 18, rz: 8, rot: 0.6, c: '#3d8fa8' }, 1);
    for (let i = 0; i < 5; i++) { const t = (i / 5) * 6.28; api.tree(x + Math.cos(t) * 24, z + Math.sin(t) * 24, 0, 18); }
  });
  // East LA's mural walls at Estrada Courts
  add('Estrada Courts Murals', [34.0225, -118.2010], 22, (api, x, z, y, a, k) => {
    const colors = ['#e63946', '#f4a261', '#2a9d8f', '#e9c46a', '#264653', '#8e44ad', '#2e86c1'];
    for (let i = 0; i < 4; i++) {
      const [bx, bz] = k.at(-15 + i * 10, 0);
      api.building(bx, bz, 8, 14, 7, a, '#e8dcc4', 1);
      for (let j = 0; j < 3; j++) { const [mx, mz] = k.at(-17.5 + i * 10 + j * 2.5, 7.15); api.box(mx, y + 3.5, mz, 2.5, 6, 0.1, colors[(i * 3 + j) % colors.length], { ry: a }); }
    }
  });
  // Compton's courthouse tower
  add('Compton Courthouse', [33.8963, -118.2253], 24, (api, x, z, y, a, k) => {
    api.building(x, z, 44, 30, 14, a, '#d8d2c6', 2);
    const [tx, tz] = k.at(0, 0);
    api.building(tx, tz, 18, 18, 44, a, '#d8d2c6', 3);
  });
  // The Highland Theatre's rooftop sign on Figueroa
  add('Highland Theatre', [34.1108, -118.1915], 12, (api, x, z, y, a, k) => {
    api.building(x, z, 20, 26, 10, a, '#c9b28c', 2);
    const [sx, sz] = k.at(0, 0);
    for (const s of [-5, 5]) { const [px, pz] = k.at(s, 0); api.box(px, y + 13, pz, 0.4, 6, 0.4, '#555'); }
    api.text(sx, y + 15, sz, 14, 3.4, 'HIGHLAND', '#ff3b3b', { ry: a, glow: true, font: 70 });
  });
  // Eagle Rock: the boulder with an eagle's shadow on its face
  add('Eagle Rock', [34.1426, -118.1884], 18, (api, x, z, y, a, k) => {
    api.sphere(x, y + 8, z, 34, 24, 26, '#a39a86', { col: true });
    const [ex, ez] = k.at(0, 12.6);
    api.box(ex, y + 12, ez, 8, 3, 0.4, '#5d564a', { ry: a, rz: 0.2 });
    api.box(ex, y + 11.5, ez, 2.6, 4, 0.4, '#5d564a', { ry: a });
  }, { slope: 12 });
  // The Colorado Street Bridge's concrete arches over the Arroyo Seco
  add('Colorado Street Bridge', [34.1453, -118.1650], 40, (api, x, z, y, a, k) => {
    const [ax, az] = k.at(-40, 0), [bx, bz] = k.at(40, 0);
    const deck = Math.max(api.gh(ax, az), api.gh(bx, bz)) + 12;
    api.box(x, deck - 0.5, z, 84, 1, 9, '#d4cbbb', { ry: a, col: true });
    for (let i = 0; i < 5; i++) {
      const [px, pz] = k.at(-32 + i * 16, 0);
      api.arch(px, deck - 9, pz, 7, 1.4, 6, a, '#d4cbbb', 9);
      api.box(px - Math.cos(a) * 8, (deck + api.gh(px, pz)) / 2 - 1, pz + Math.sin(a) * 8, 1.6, deck - api.gh(px, pz), 6, '#cfc6b6', { ry: a });
    }
    for (let i = 0; i < 8; i++) { const [lx, lz] = k.at(-36 + i * 10, 4.2); api.sphere(lx, deck + 3, lz, 0.6, 0.6, 0.6, '#fff1c9', { m: 'glow' }); }
  }, { slope: 14 });
  // The Norton Simon Museum's brown-tile curves on Colorado Blvd
  add('Norton Simon Museum', [34.1462, -118.1595], 24, (api, x, z, y, a, k) => {
    api.building(x, z, 50, 34, 10, a, '#7a4b3a', 0);
    for (let i = -4; i <= 4; i++) { const [px, pz] = k.at(i * 5, 22); api.box(px, y + 1.4, pz, 1, 2.8, 1, '#5b4636'); }
  });

  /* ------------------------------------------------- the Valley and Burbank */

  // Bob's Big Boy, the googie diner in Toluca Lake
  add("Bob's Big Boy", [34.1531, -118.3460], 16, (api, x, z, y, a, k) => {
    api.building(x, z, 22, 14, 5, a, '#f4efe6', 2);
    const [rx, rz] = k.at(0, 0);
    api.box(rx, y + 5.6, rz, 26, 0.6, 18, '#c0392b', { ry: a, rz: 0.12 });
    const [px, pz] = k.at(14, 6);
    api.box(px, y + 6, pz, 1, 12, 1, '#c0392b');
    api.box(px, y + 13, pz, 7, 3, 0.6, '#ffffff', { ry: a });
    api.text(px + Math.sin(a) * 0.35, y + 13, pz + Math.cos(a) * 0.35, 6.6, 2.6, "BOB'S", '#c0392b', { ry: a, glow: true, font: 70 });
    const [sx, sz] = k.at(-12, 9);
    api.box(sx, y + 1.5, sz, 1.4, 3, 1, '#c0392b');
    api.box(sx, y + 4.2, sz, 2, 2.6, 1.2, '#ffffff');
    api.sphere(sx, y + 6.4, sz, 1.6, 1.8, 1.6, '#f2d1b5');
  });
  // The Team Disney building and soundstages in Burbank
  add('Walt Disney Studios', [34.1562, -118.3250], 34, (api, x, z, y, a, k) => {
    api.building(x, z, 54, 24, 18, a, '#efe2c4', 2);
    for (let i = -3; i <= 3; i++) { const [cx, cz] = k.at(i * 7, 12.6); api.box(cx, y + 9, cz, 1.6, 18, 1.2, '#c9b28c', { ry: a }); }
    for (const lx of [-20, 20]) { const [bx, bz] = k.at(lx, -26); api.building(bx, bz, 30, 22, 16, a, '#dccfb4', 4); }
  });
  // Glendale's Americana at Brand: shops, a lawn and the dancing fountain
  add('Americana at Brand', [34.1460, -118.2560], 30, (api, x, z, y, a, k) => {
    api.paint({ t: 'rect', x, z, w: 26, d: 30, rot: a, c: '#7f9c55' }, 1);
    api.paint({ t: 'circle', x, z, r: 6, c: '#78c4e6' }, 1);
    api.cyl(x, y + 3, z, 1, 6, 1, '#eaf6ff', { m: 'glow' });
    for (const s of [-1, 1]) for (let i = 0; i < 3; i++) { const [bx, bz] = k.at(s * 22, -12 + i * 12); api.building(bx, bz, 14, 11, api.rnd.range(9, 16), a, pick(api, ['#efe2c8', '#e9d1b0', '#d9c7a7']), 2); }
  });

  /* ----------------------------------------------------------- near LAX */

  // The Proud Bird: a restaurant with vintage planes parked outside
  add('The Proud Bird', [33.9450, -118.3696], 26, (api, x, z, y, a, k) => {
    api.building(x, z, 30, 18, 6, a, '#efe6d2', 2);
    for (const [lx, lz, c] of [[-16, 14, '#8a9aa6'], [14, 16, '#6b7a5a']]) {
      const [px, pz] = k.at(lx, lz);
      api.cyl(px, y + 2.2, pz, 1.6, 10, 1.6, c, { rz: Math.PI / 2, ry: a + 0.3 });
      api.box(px, y + 2.4, pz, 2.4, 0.3, 12, c, { ry: a + 0.3 });
    }
  });
}
