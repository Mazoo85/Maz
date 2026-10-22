/*
 * The on-screen bits: compass, minimap, "now entering" banner, and the
 * full map you open with M (and pick a drop spot on).
 */

const $ = (id) => document.getElementById(id);

export function makeHud(W, groundCanvas) {
  /* A 2048 copy of the ground painting for the maps. */
  const mapImg = document.createElement('canvas');
  mapImg.width = mapImg.height = 2048;
  mapImg.getContext('2d').drawImage(groundCanvas, 0, 0, 2048, 2048);
  const toMap = 2048 / W.size;

  /* ------------------------------------------------------------ compass */
  const compass = $('compass'), cctx = compass.getContext('2d');
  function drawCompass(yaw) {
    const w = compass.width, h = compass.height;
    cctx.clearRect(0, 0, w, h);
    // heading in degrees, 0 = north, clockwise
    const heading = ((-yaw * 180) / Math.PI + 360 * 4) % 360;
    const pxPerDeg = w / 120;
    cctx.textAlign = 'center';
    cctx.textBaseline = 'middle';
    for (let d = -70; d <= 70; d += 5) {
      const deg = Math.round((heading + d) / 5) * 5;
      const x = w / 2 + (deg - heading) * pxPerDeg;
      const norm = ((deg % 360) + 360) % 360;
      const label = { 0: 'N', 45: 'NE', 90: 'E', 135: 'SE', 180: 'S', 225: 'SW', 270: 'W', 315: 'NW' }[norm];
      cctx.fillStyle = label ? '#ffffff' : 'rgba(255,255,255,0.55)';
      if (label) {
        cctx.font = `800 ${h * 0.42}px system-ui, sans-serif`;
        cctx.fillStyle = norm === 0 ? '#ffd23f' : '#ffffff';
        cctx.fillText(label, x, h * 0.42);
      } else if (norm % 15 === 0) {
        cctx.font = `600 ${h * 0.26}px system-ui, sans-serif`;
        cctx.fillText(String(norm), x, h * 0.42);
      } else {
        cctx.fillRect(x - 0.5, h * 0.62, 1, h * 0.18);
      }
    }
    cctx.fillStyle = '#ffd23f';
    cctx.beginPath(); cctx.moveTo(w / 2 - 6, h); cctx.lineTo(w / 2 + 6, h); cctx.lineTo(w / 2, h - 8); cctx.fill();
  }

  /* ------------------------------------------------------------ minimap */
  const mini = $('minimap'), mctx = mini.getContext('2d');
  function drawMinimap(px, pz, yaw, extras) {
    const s = mini.width, r = s / 2;
    const metres = extras.radius || 420;            // half-width of what it shows
    const k = r / metres;
    mctx.save();
    mctx.clearRect(0, 0, s, s);
    mctx.beginPath(); mctx.arc(r, r, r - 2, 0, Math.PI * 2); mctx.clip();
    mctx.fillStyle = '#1f5f8b'; mctx.fillRect(0, 0, s, s);
    const sx = (px - metres + W.half) * toMap, sz = (pz - metres + W.half) * toMap;
    mctx.imageSmoothingEnabled = true;
    mctx.drawImage(mapImg, sx, sz, metres * 2 * toMap, metres * 2 * toMap, 0, 0, s, s);
    for (const v of extras.vehicles || []) {
      const x = r + (v.x - px) * k, y = r + (v.z - pz) * k;
      if ((x - r) ** 2 + (y - r) ** 2 > r * r) continue;
      mctx.fillStyle = '#ffd23f'; mctx.strokeStyle = '#1b1b1b'; mctx.lineWidth = 1.5;
      mctx.beginPath(); mctx.roundRect(x - 4, y - 3, 8, 6, 2); mctx.fill(); mctx.stroke();
    }
    if (extras.marker) {
      const x = r + (extras.marker.x - px) * k, y = r + (extras.marker.z - pz) * k;
      const cx = Math.max(10, Math.min(s - 10, x)), cy = Math.max(10, Math.min(s - 10, y));
      pin(mctx, cx, cy, 1);
    }
    mctx.restore();
    // the player arrow
    mctx.save();
    mctx.translate(r, r);
    mctx.rotate(-yaw);
    mctx.fillStyle = '#ffffff'; mctx.strokeStyle = '#111'; mctx.lineWidth = 2;
    mctx.beginPath(); mctx.moveTo(0, -9); mctx.lineTo(6.5, 7); mctx.lineTo(0, 3.5); mctx.lineTo(-6.5, 7); mctx.closePath();
    mctx.fill(); mctx.stroke();
    mctx.restore();
    mctx.strokeStyle = 'rgba(255,255,255,0.85)'; mctx.lineWidth = 3;
    mctx.beginPath(); mctx.arc(r, r, r - 2, 0, Math.PI * 2); mctx.stroke();
    mctx.fillStyle = '#fff'; mctx.font = '800 13px system-ui, sans-serif'; mctx.textAlign = 'center';
    mctx.fillText('N', r, 15);
  }

  /* ------------------------------------------------------------- banner */
  const banner = $('banner'), bTitle = $('bannerTitle'), bSub = $('bannerSub');
  let bannerTimer = 0, lastKey = '';
  function showLocation(loc, force) {
    const key = `${loc.poi}|${loc.district}`;
    if (key === lastKey && !force) return;
    const changedPlace = !lastKey || lastKey.split('|')[0] !== String(loc.poi) || lastKey.split('|')[1] !== loc.district;
    lastKey = key;
    if (!changedPlace && !force) return;
    bTitle.textContent = loc.poi || loc.district;
    bSub.textContent = loc.poi ? loc.district : 'Los Angeles';
    banner.classList.remove('show');
    void banner.offsetWidth;
    banner.classList.add('show');
    bannerTimer = 3.2;
  }
  function tick(dt) {
    if (bannerTimer > 0) { bannerTimer -= dt; if (bannerTimer <= 0) banner.classList.remove('show'); }
  }

  /* ----------------------------------------------------------- full map */
  const overlay = $('mapOverlay'), canvas = $('mapCanvas'), ctx = canvas.getContext('2d');
  const view = { zoom: 1, cx: 0, cz: 0 };
  let marker = null, onDrop = null, state = null, dragging = null, open = false;
  const majorKinds = new Set(['Santa Monica Pier', 'LAX', 'Hollywood Sign', 'Griffith Observatory', 'Dodger Stadium', 'SoFi Stadium',
    'Venice Beach', 'Getty Center', 'Universal Studios', 'Rose Bowl', 'Port of Los Angeles', 'Queen Mary', 'LA Memorial Coliseum',
    'City Hall', 'Malibu Pier', 'UCLA', 'Watts Towers', 'Crypto.com Arena', 'Union Station', 'Manhattan Beach Pier', 'Sunset Strip',
    'Walk of Fame', 'Rodeo Drive', 'Urban Light (LACMA)', 'Echo Park Lake', 'Goodyear Blimp Base', 'SpaceX Hawthorne', 'Marina del Rey',
    'Sixth Street Viaduct', 'Century City', 'Beverly Hills Hotel', 'Sony Pictures Studios', "Randy's Donuts", 'Hollywood Bowl', 'TCL Chinese Theatre']);

  function fit() {
    const dpr = Math.min(2, window.devicePixelRatio || 1);
    canvas.width = canvas.clientWidth * dpr;
    canvas.height = canvas.clientHeight * dpr;
  }
  function scale() { return (Math.min(canvas.width, canvas.height) / W.size) * view.zoom; }
  function toScreen(x, z) { const s = scale(); return [canvas.width / 2 + (x - view.cx) * s, canvas.height / 2 + (z - view.cz) * s]; }
  function toWorld(sx, sy) { const s = scale(); return [view.cx + (sx - canvas.width / 2) / s, view.cz + (sy - canvas.height / 2) / s]; }

  function drawMap() {
    if (!open) return;
    const s = scale();
    ctx.fillStyle = '#1f5f8b';
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    const [x0, y0] = toScreen(-W.half, -W.half);
    ctx.imageSmoothingEnabled = true;
    ctx.drawImage(mapImg, x0, y0, W.size * s, W.size * s);
    const dpr = canvas.width / canvas.clientWidth;
    // grid letters like a battle royale map
    ctx.strokeStyle = 'rgba(255,255,255,0.12)'; ctx.lineWidth = 1;
    ctx.fillStyle = 'rgba(255,255,255,0.5)'; ctx.font = `700 ${11 * dpr}px system-ui`;
    for (let i = 0; i <= 8; i++) {
      const g = -W.half + (i * W.size) / 8;
      const [gx] = toScreen(g, 0), [, gy] = toScreen(0, g);
      ctx.beginPath(); ctx.moveTo(gx, y0); ctx.lineTo(gx, y0 + W.size * s); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(x0, gy); ctx.lineTo(x0 + W.size * s, gy); ctx.stroke();
      if (i < 8) {
        ctx.fillText('ABCDEFGH'[i], gx + (W.size * s) / 16 - 4 * dpr, Math.max(y0, 0) + 14 * dpr);
        ctx.fillText(String(i + 1), Math.max(x0, 0) + 6 * dpr, gy + (W.size * s) / 16);
      }
    }
    // landmarks first, then neighbourhood names wherever they still fit
    ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
    const taken = [];
    const fits = (x, y, w, h) => {
      const r = [x - w / 2 - 3 * dpr, y - h / 2 - 2 * dpr, x + w / 2 + 3 * dpr, y + h / 2 + 2 * dpr];
      if (taken.some((t) => r[0] < t[2] && r[2] > t[0] && r[1] < t[3] && r[3] > t[1])) return false;
      taken.push(r);
      return true;
    };
    const big = W.pois.filter((p) => majorKinds.has(p.name)), rest = W.pois.filter((p) => !majorKinds.has(p.name));
    for (const p of view.zoom < 2.2 ? big : [...big, ...rest]) {
      const [x, y] = toScreen(p.x, p.z);
      if (x < -50 || y < -20 || x > canvas.width + 50 || y > canvas.height + 20) continue;
      ctx.font = `800 ${Math.max(10, 11 * Math.sqrt(view.zoom)) * dpr}px system-ui, sans-serif`;
      const w = ctx.measureText(p.name).width, h = 14 * dpr * Math.sqrt(view.zoom);
      ctx.fillStyle = '#ffd23f'; ctx.strokeStyle = '#111'; ctx.lineWidth = 1.5 * dpr;
      ctx.beginPath(); ctx.arc(x, y, 3.5 * dpr, 0, Math.PI * 2); ctx.fill(); ctx.stroke();
      if (fits(x, y - 11 * dpr, w, h)) label(ctx, p.name, x, y - 11 * dpr, '#ffffff', 'rgba(0,0,0,0.85)', dpr * 3);
    }
    for (const d of W.districts) {
      if (Math.abs(d.x) > W.half || Math.abs(d.z) > W.half) continue;
      const [x, y] = toScreen(d.x, d.z);
      ctx.font = `700 ${Math.max(9, 10 * Math.sqrt(view.zoom)) * dpr}px system-ui, sans-serif`;
      const name = d.name.toUpperCase();
      if (fits(x, y, ctx.measureText(name).width, 12 * dpr * Math.sqrt(view.zoom))) label(ctx, name, x, y, 'rgba(255,255,255,0.8)', 'rgba(0,0,0,0.55)', dpr * 2.5);
    }
    if (state) {
      for (const v of state.vehicles) {
        const [x, y] = toScreen(v.x, v.z);
        ctx.fillStyle = '#ffd23f'; ctx.strokeStyle = '#111'; ctx.lineWidth = 1.2 * dpr;
        ctx.beginPath(); ctx.roundRect(x - 4 * dpr, y - 3 * dpr, 8 * dpr, 6 * dpr, 2 * dpr); ctx.fill(); ctx.stroke();
      }
      if (state.blimp) {
        const [ax, ay] = toScreen(state.blimp.from[0], state.blimp.from[1]), [bx, by] = toScreen(state.blimp.to[0], state.blimp.to[1]);
        ctx.setLineDash([8 * dpr, 6 * dpr]); ctx.strokeStyle = '#ffffff'; ctx.lineWidth = 2 * dpr;
        ctx.beginPath(); ctx.moveTo(ax, ay); ctx.lineTo(bx, by); ctx.stroke(); ctx.setLineDash([]);
      }
      const [px, py] = toScreen(state.player.x, state.player.z);
      ctx.save(); ctx.translate(px, py); ctx.rotate(-state.yaw);
      ctx.fillStyle = '#fff'; ctx.strokeStyle = '#111'; ctx.lineWidth = 2 * dpr;
      ctx.beginPath(); ctx.moveTo(0, -11 * dpr); ctx.lineTo(8 * dpr, 9 * dpr); ctx.lineTo(0, 4 * dpr); ctx.lineTo(-8 * dpr, 9 * dpr); ctx.closePath(); ctx.fill(); ctx.stroke();
      ctx.restore();
    }
    if (marker) { const [mx, my] = toScreen(marker.x, marker.z); pin(ctx, mx, my, dpr * 1.3); }
  }

  canvas.addEventListener('wheel', (e) => {
    e.preventDefault();
    const [wx, wz] = toWorld(e.offsetX * (canvas.width / canvas.clientWidth), e.offsetY * (canvas.height / canvas.clientHeight));
    view.zoom = Math.max(1, Math.min(10, view.zoom * (e.deltaY < 0 ? 1.2 : 1 / 1.2)));
    const [nx, nz] = toWorld(e.offsetX * (canvas.width / canvas.clientWidth), e.offsetY * (canvas.height / canvas.clientHeight));
    view.cx += wx - nx; view.cz += wz - nz;
    clampView(); drawMap();
  }, { passive: false });
  const pointers = new Map();
  let pinchStart = null;
  canvas.addEventListener('pointerdown', (e) => {
    canvas.setPointerCapture(e.pointerId);
    pointers.set(e.pointerId, { x: e.offsetX, y: e.offsetY });
    dragging = { x: e.offsetX, y: e.offsetY, moved: 0 };
    if (pointers.size === 2) { const [a, b] = [...pointers.values()]; pinchStart = { d: Math.hypot(a.x - b.x, a.y - b.y), zoom: view.zoom }; }
  });
  canvas.addEventListener('pointermove', (e) => {
    if (!pointers.has(e.pointerId)) return;
    pointers.set(e.pointerId, { x: e.offsetX, y: e.offsetY });
    if (pointers.size === 2 && pinchStart) {
      const [a, b] = [...pointers.values()];
      view.zoom = Math.max(1, Math.min(10, pinchStart.zoom * Math.hypot(a.x - b.x, a.y - b.y) / pinchStart.d));
      if (dragging) dragging.moved = 99;
      clampView(); drawMap(); return;
    }
    if (!dragging) return;
    const k = canvas.width / canvas.clientWidth;
    const dx = (e.offsetX - dragging.x) * k, dy = (e.offsetY - dragging.y) * k;
    dragging.moved += Math.abs(dx) + Math.abs(dy);
    dragging.x = e.offsetX; dragging.y = e.offsetY;
    view.cx -= dx / scale(); view.cz -= dy / scale();
    clampView(); drawMap();
  });
  canvas.addEventListener('pointerup', (e) => {
    pointers.delete(e.pointerId);
    if (pointers.size < 2) pinchStart = null;
    if (dragging && dragging.moved < 8 && pointers.size === 0) {
      const k = canvas.width / canvas.clientWidth;
      const [x, z] = toWorld(e.offsetX * k, e.offsetY * k);
      if (Math.abs(x) < W.half - 40 && Math.abs(z) < W.half - 40) {
        marker = { x, z };
        $('mapDrop').disabled = false;
        const loc = state && state.locate ? state.locate(x, z) : null;
        $('mapPick').textContent = loc ? `${loc.poi ? loc.poi + ' · ' : ''}${loc.district}` : 'Marked';
      }
      drawMap();
    }
    if (pointers.size === 0) dragging = null;
  });
  function clampView() {
    const lim = W.half * (1 - 1 / view.zoom);
    view.cx = Math.max(-lim, Math.min(lim, view.cx));
    view.cz = Math.max(-lim, Math.min(lim, view.cz));
  }
  $('mapDrop').addEventListener('click', () => { if (marker && onDrop) onDrop(marker); });
  $('mapZoomIn').addEventListener('click', () => { view.zoom = Math.min(10, view.zoom * 1.5); clampView(); drawMap(); });
  $('mapZoomOut').addEventListener('click', () => { view.zoom = Math.max(1, view.zoom / 1.5); clampView(); drawMap(); });
  window.addEventListener('resize', () => { if (open) { fit(); drawMap(); } });

  function openMap(s, dropHandler, centre) {
    state = s; onDrop = dropHandler; open = true;
    overlay.hidden = false;
    fit();
    if (centre) { view.zoom = Math.max(view.zoom, 2.4); view.cx = s.player.x; view.cz = s.player.z; clampView(); }
    $('mapDrop').disabled = !marker;
    drawMap();
  }
  function closeMap() { open = false; overlay.hidden = true; }

  return {
    drawCompass, drawMinimap, showLocation, tick, openMap, closeMap, drawMap,
    get mapOpen() { return open; }, get marker() { return marker; }, clearMarker() { marker = null; }, mapImage: mapImg
  };
}

function label(ctx, text, x, y, fill, stroke, w) {
  ctx.lineWidth = w; ctx.strokeStyle = stroke; ctx.lineJoin = 'round';
  ctx.strokeText(text, x, y);
  ctx.fillStyle = fill;
  ctx.fillText(text, x, y);
}

function pin(ctx, x, y, k) {
  ctx.save(); ctx.translate(x, y); ctx.scale(k, k);
  ctx.fillStyle = '#ff3b5c'; ctx.strokeStyle = '#fff'; ctx.lineWidth = 2;
  ctx.beginPath(); ctx.moveTo(0, 0); ctx.bezierCurveTo(-9, -12, -8, -22, 0, -22); ctx.bezierCurveTo(8, -22, 9, -12, 0, 0); ctx.fill(); ctx.stroke();
  ctx.fillStyle = '#fff'; ctx.beginPath(); ctx.arc(0, -15, 3.2, 0, Math.PI * 2); ctx.fill();
  ctx.restore();
}
