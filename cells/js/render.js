/* =============================================================================
 *  NEON CELLS  —  drawing
 *
 *  Everything you see is drawn from code: there are no image files in this
 *  project at all. Rock is tiled with a deterministic per-tile hash so it looks
 *  hewn rather than repeated, every torch and explosion is an additive radial
 *  light, and the characters are built out of a handful of rectangles each.
 *
 *  The canvas is kept at a small internal resolution and scaled up by a whole
 *  number of pixels, which is what gives the picture crisp pixel edges instead
 *  of a blur.
 * ========================================================================== */
(function (global) {
  'use strict';

  const LG = global.CELLS_LEVELGEN;
  const CONTENT = global.CELLS_CONTENT;
  const CB = global.CELLS_COMBAT;
  const T = LG.T;
  const TILE = LG.TILE;

  let canvas = null;
  let ctx = null;
  let VW = 480;
  let VH = 270;
  let scale = 3;

  const cam = { x: 0, y: 0, tx: 0, ty: 0, shakeX: 0, shakeY: 0 };
  const gradientCache = new Map();

  /* Screen shake is the one effect here that can actually bother someone, so it
   * is damped right down for anyone who has asked for less motion. */
  const calmMotion = !!(global.matchMedia && global.matchMedia('(prefers-reduced-motion: reduce)').matches);
  const SHAKE = calmMotion ? 0.15 : 1;

  /* ------------------------------------------------------------------ setup */
  function init(el) {
    canvas = el;
    ctx = canvas.getContext('2d', { alpha: false });
    resize();
    return { ctx: ctx };
  }

  function resize() {
    const cssW = Math.max(240, global.innerWidth || 960);
    const cssH = Math.max(180, global.innerHeight || 540);
    /* One whole-pixel zoom level. The view wants to be about 400x220 game
     * pixels — roughly 25 tiles across — so the player is big enough to read
     * at a glance. Never below 2, or a phone ends up looking at the whole
     * level at once and the player becomes a speck. */
    scale = Math.max(2, Math.min(5, Math.floor(Math.min(cssW / 360, cssH / 200)) || 2));
    VW = Math.ceil(cssW / scale);
    VH = Math.ceil(cssH / scale);
    canvas.width = VW;
    canvas.height = VH;
    canvas.style.width = cssW + 'px';
    canvas.style.height = cssH + 'px';
    ctx.imageSmoothingEnabled = false;
  }

  function view() {
    return { w: VW, h: VH, scale: scale };
  }

  /* ----------------------------------------------------------------- camera */
  function centreCamera(world) {
    const p = world.player;
    cam.tx = p.x - VW / 2;
    cam.ty = p.y - p.h / 2 - VH / 2;
    cam.x = cam.tx;
    cam.y = cam.ty;
    clampCamera(world);
  }

  function updateCamera(world, dt) {
    const p = world.player;
    /* look a little the way the player is moving, and a little where they aim */
    const lead = Math.max(-60, Math.min(60, p.vx * 0.22));
    cam.tx = p.x + lead - VW / 2;
    cam.ty = p.y - p.h / 2 - VH / 2 - 12;

    const k = 1 - Math.pow(0.0001, dt);
    cam.x += (cam.tx - cam.x) * k;
    cam.y += (cam.ty - cam.y) * k;
    clampCamera(world);

    const s = (world.shake || 0) * SHAKE;
    cam.shakeX = (Math.random() - 0.5) * s;
    cam.shakeY = (Math.random() - 0.5) * s;
  }

  function clampCamera(world) {
    const level = world.level;
    /* A little margin past the edges, so the floor of a flat arena is never
     * jammed against the bottom of the screen. */
    const margin = 22;
    const maxX = level.w * TILE - VW;
    const maxY = level.h * TILE - VH + margin;
    cam.x = maxX <= 0 ? maxX / 2 : Math.max(0, Math.min(maxX, cam.x));
    cam.y = maxY <= 0 ? maxY / 2 : Math.max(-margin, Math.min(maxY, cam.y));
  }

  function camera() {
    return cam;
  }

  function ox() {
    return Math.round(cam.x + cam.shakeX);
  }

  function oy() {
    return Math.round(cam.y + cam.shakeY);
  }

  /* -------------------------------------------------------------- utilities */
  function hash2(x, y) {
    let h = (x * 73856093) ^ (y * 19349663);
    h = (h ^ (h >> 13)) * 1274126177;
    return ((h ^ (h >> 16)) >>> 0) / 4294967296;
  }

  function radial(color, radius) {
    const k = color + '|' + radius;
    let g = gradientCache.get(k);
    if (!g) {
      g = ctx.createRadialGradient(0, 0, 0, 0, 0, radius);
      g.addColorStop(0, color);
      g.addColorStop(1, 'rgba(0,0,0,0)');
      gradientCache.set(k, g);
    }
    return g;
  }

  function light(x, y, radius, color) {
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    ctx.translate(x, y);
    ctx.fillStyle = radial(color, radius);
    ctx.beginPath();
    ctx.arc(0, 0, radius, 0, Math.PI * 2);
    ctx.fill();
    ctx.restore();
  }

  function rect(x, y, w, h, color) {
    ctx.fillStyle = color;
    ctx.fillRect(Math.round(x), Math.round(y), Math.round(w), Math.round(h));
  }

  function text(str, x, y, opts) {
    const o = opts || {};
    const size = o.size || 8;
    ctx.font = (o.bold ? 'bold ' : '') + size + 'px "Courier New", monospace';
    ctx.textAlign = o.align || 'left';
    ctx.textBaseline = o.baseline || 'alphabetic';
    if (o.shadow !== false) {
      ctx.fillStyle = 'rgba(0,0,0,.75)';
      ctx.fillText(str, Math.round(x) + 1, Math.round(y) + 1);
    }
    ctx.fillStyle = o.color || '#fff';
    ctx.fillText(str, Math.round(x), Math.round(y));
    return ctx.measureText(str).width;
  }

  function measure(str, size, bold) {
    ctx.font = (bold ? 'bold ' : '') + (size || 8) + 'px "Courier New", monospace';
    return ctx.measureText(str).width;
  }

  function panel(x, y, w, h, accent) {
    ctx.fillStyle = 'rgba(7,6,12,.92)';
    ctx.fillRect(Math.round(x), Math.round(y), Math.round(w), Math.round(h));
    ctx.strokeStyle = accent || 'rgba(255,255,255,.22)';
    ctx.lineWidth = 1;
    ctx.strokeRect(Math.round(x) + 0.5, Math.round(y) + 0.5, Math.round(w) - 1, Math.round(h) - 1);
  }

  /* =========================================================================
   *  THE WORLD
   * ====================================================================== */
  function drawWorld(world) {
    const level = world.level;
    const pal = level.biome.palette;

    drawBackdrop(world, pal);

    ctx.save();
    ctx.translate(-ox(), -oy());

    drawTiles(world, pal);
    drawObjects(world, pal);
    drawTraps(world);
    drawDrops(world);
    drawEnemies(world);
    drawAllies(world);
    drawPlayer(world);
    drawProjectiles(world);
    drawEffects(world);
    drawLights(world, pal);

    ctx.restore();

    drawVignette(pal);
  }

  /* Parallax: a gradient sky, two layers of silhouetted architecture, and dust
   * drifting through the light. */
  function drawBackdrop(world, pal) {
    const g = ctx.createLinearGradient(0, 0, 0, VH);
    g.addColorStop(0, pal.sky1);
    g.addColorStop(1, pal.sky2);
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, VW, VH);

    for (let layer = 0; layer < 2; layer++) {
      const depth = layer === 0 ? 0.18 : 0.38;
      const px = -ox() * depth;
      const py = -oy() * depth;
      const step = layer === 0 ? 86 : 54;
      const h = layer === 0 ? 120 : 76;
      ctx.fillStyle = layer === 0 ? 'rgba(0,0,0,.22)' : 'rgba(0,0,0,.34)';
      const start = Math.floor(-px / step) - 1;
      for (let i = start; i < start + Math.ceil(VW / step) + 3; i++) {
        const x = px + i * step;
        const n = hash2(i, layer);
        const top = VH * 0.32 + py * 0.25 + n * 40;
        ctx.fillRect(Math.round(x), Math.round(top), Math.round(step * 0.62), h + n * 60);
        /* an arch over every other pillar */
        if (i % 2 === 0) {
          ctx.beginPath();
          ctx.arc(Math.round(x + step * 0.31), Math.round(top), step * 0.31, Math.PI, 0);
          ctx.fill();
        }
      }
    }

    /* motes */
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    const t = world.time || 0;
    for (let i = 0; i < 34; i++) {
      const n = hash2(i, 7);
      const m = hash2(i, 13);
      const x = ((n * VW + t * (6 + m * 10)) % (VW + 20)) - 10;
      const y = ((m * VH + Math.sin(t * 0.4 + i) * 12 + t * 4) % (VH + 20)) - 10;
      ctx.fillStyle = pal.mote;
      ctx.fillRect(Math.round(x), Math.round(y), 1, 1);
    }
    ctx.restore();
  }

  function drawTiles(world, pal) {
    const level = world.level;
    /* Deliberately not clamped to the level bounds: everything outside is solid
     * rock (LG.at says so), so drawing it means the level reads as carved out
     * of a mountain rather than floating in a void. */
    const x0 = Math.floor(ox() / TILE) - 1;
    const y0 = Math.floor(oy() / TILE) - 1;
    const x1 = Math.ceil((ox() + VW) / TILE);
    const y1 = Math.ceil((oy() + VH) / TILE);

    for (let ty = y0; ty <= y1; ty++) {
      for (let tx = x0; tx <= x1; tx++) {
        const v = LG.at(level, tx, ty);
        if (v === T.EMPTY) continue;
        const x = tx * TILE;
        const y = ty * TILE;
        const n = hash2(tx, ty);

        if (v === T.SOLID) {
          const open = LG.at(level, tx, ty - 1) !== T.SOLID;
          rect(x, y, TILE, TILE, n < 0.5 ? pal.rock : pal.rockLit);
          /* hewn texture */
          ctx.fillStyle = 'rgba(0,0,0,.22)';
          ctx.fillRect(x + Math.floor(n * 10), y + Math.floor(hash2(ty, tx) * 10), 3, 2);
          ctx.fillStyle = 'rgba(255,255,255,.05)';
          ctx.fillRect(x + Math.floor(hash2(tx + 5, ty) * 11), y + Math.floor(n * 12), 2, 1);

          if (open) {
            /* a lit edge and a line of moss on every exposed surface */
            rect(x, y, TILE, 2, pal.edge);
            if (n < 0.42) {
              ctx.fillStyle = pal.moss;
              ctx.globalAlpha = 0.5;
              ctx.fillRect(x + Math.floor(n * 12), y - 1, 2 + Math.floor(n * 4), 2);
              ctx.globalAlpha = 1;
            }
          }
        } else if (v === T.PLATFORM) {
          rect(x, y, TILE, 4, pal.rockLit);
          rect(x, y, TILE, 1, pal.edge);
          ctx.fillStyle = 'rgba(0,0,0,.35)';
          ctx.fillRect(x, y + 4, TILE, 2);
        } else if (v === T.SPIKE) {
          rect(x, y, TILE, TILE, 'rgba(12,10,16,.9)');
          light(x + TILE / 2, y + TILE / 2, 18, 'rgba(255,59,92,.18)');
          ctx.fillStyle = '#d8b0b8';
          for (let i = 0; i < 4; i++) {
            const sx = x + i * 4;
            ctx.beginPath();
            ctx.moveTo(sx, y + TILE);
            ctx.lineTo(sx + 2, y + 3);
            ctx.lineTo(sx + 4, y + TILE);
            ctx.fill();
          }
        }
      }
    }
  }

  /* ----------------------------------------------------- doors, chests, signs */
  function drawObjects(world, pal) {
    const level = world.level;
    const t = world.time || 0;

    /* the way out */
    if (level.exit) {
      const e = level.exit;
      const open = !level.exit.locked;
      const x = e.x - e.w / 2;
      const y = e.y - e.h;
      rect(x - 2, y - 2, e.w + 4, e.h + 2, '#0a0810');
      const glow = open ? pal.moss : '#553';
      ctx.fillStyle = glow;
      ctx.globalAlpha = open ? 0.45 + Math.sin(t * 3) * 0.15 : 0.2;
      ctx.fillRect(Math.round(x), Math.round(y), e.w, e.h);
      ctx.globalAlpha = 1;
      rect(x, y, e.w, 2, glow);
      if (open) light(e.x, e.y - e.h / 2, 56, 'rgba(47,230,200,.22)');
      text(open ? 'EXIT' : 'SEALED', e.x, y - 6, { align: 'center', color: open ? '#fff' : '#888', size: 7 });
    }

    for (const o of level.objects) {
      const x = o.pos.x;
      const y = o.pos.y;
      switch (o.type) {
        case 'chest':
        case 'timed_chest':
        case 'cursed_chest': {
          const cursed = o.type === 'cursed_chest';
          const timed = o.type === 'timed_chest';
          const body = o.opened ? '#2a2430' : cursed ? '#2b1430' : '#4a3520';
          rect(x - 8, y - 11, 16, 11, body);
          rect(x - 8, y - 13, 16, 3, o.opened ? '#3a3340' : cursed ? '#6a2a7a' : '#6b4f2a');
          rect(x - 1, y - 9, 2, 4, o.opened ? '#555' : '#ffe600');
          if (!o.opened) {
            const c = cursed ? 'rgba(180,60,220,.3)' : timed ? 'rgba(255,230,0,.26)' : 'rgba(255,200,80,.26)';
            light(x, y - 6, 30, c);
            if (timed) {
              const left = Math.max(0, (level.timeLimit || 0) - (world.runTime || 0));
              text(left > 0 ? Math.ceil(left) + 's' : 'LOCKED', x, y - 18, {
                align: 'center', size: 7, color: left > 0 ? '#ffe600' : '#888'
              });
            }
            if (cursed) text('CURSED', x, y - 18, { align: 'center', size: 7, color: '#d06bff' });
          }
          break;
        }

        case 'vault': {
          rect(x - 10, y - 20, 20, 20, o.opened ? '#2a2430' : '#3a2f45');
          rect(x - 10, y - 22, 20, 3, '#6a5f7a');
          if (o.locked && !o.opened) {
            rect(x - 2, y - 12, 4, 6, '#ffe600');
            light(x, y - 10, 26, 'rgba(255,230,0,.22)');
            text('LOCKED', x, y - 28, { align: 'center', size: 7, color: '#ffe600' });
          }
          break;
        }

        case 'scroll':
          if (!o.taken) {
            const bob = Math.sin(t * 2.4 + x) * 2;
            rect(x - 5, y - 26 + bob, 10, 3, '#d8cfa8');
            rect(x - 3, y - 23 + bob, 6, 12, '#efe6c0');
            rect(x - 5, y - 11 + bob, 10, 3, '#d8cfa8');
            light(x, y - 18 + bob, 40, o.dual ? 'rgba(255,230,0,.3)' : 'rgba(255,255,255,.22)');
            rect(x - 6, y - 2, 12, 2, 'rgba(0,0,0,.4)');
            text(o.dual ? 'DUAL SCROLL' : 'SCROLL', x, y - 32 + bob, { align: 'center', size: 7, color: o.dual ? '#ffe600' : '#fff' });
          }
          break;

        case 'food':
          if (!o.taken) {
            ctx.fillStyle = '#b5543a';
            ctx.beginPath();
            ctx.arc(x, y - 5, 5, 0, Math.PI * 2);
            ctx.fill();
            rect(x - 1, y - 12, 2, 5, '#c8b08a');
            light(x, y - 6, 22, 'rgba(255,140,90,.2)');
          }
          break;

        case 'shop':
          rect(x - 14, y - 26, 28, 26, '#241a2e');
          rect(x - 16, y - 28, 32, 4, '#6a4f86');
          rect(x - 10, y - 20, 20, 12, '#120d18');
          text('SHOP', x, y - 31, { align: 'center', size: 7, color: '#ffd34a' });
          light(x, y - 14, 44, 'rgba(255,211,74,.18)');
          break;

        case 'collector': {
          const pulse = 0.5 + Math.sin(t * 2) * 0.2;
          rect(x - 9, y - 30, 18, 30, '#161020');
          rect(x - 11, y - 32, 22, 4, '#2fe6c8');
          ctx.save();
          ctx.globalAlpha = pulse;
          rect(x - 4, y - 24, 8, 12, '#2fe6c8');
          ctx.restore();
          light(x, y - 18, 54, 'rgba(47,230,200,.26)');
          text('COLLECTOR', x, y - 37, { align: 'center', size: 7, color: '#2fe6c8' });
          break;
        }
      }
    }

    /* torches */
    for (const tor of level.torches) {
      const flick = 0.8 + Math.sin(t * 9 + tor.seed) * 0.12 + Math.sin(t * 23 + tor.seed * 3) * 0.06;
      rect(tor.x - 1, tor.y, 2, 6, '#3a2a1a');
      ctx.fillStyle = pal.torch;
      ctx.globalAlpha = flick;
      ctx.beginPath();
      ctx.arc(tor.x, tor.y - 2, 2.4, 0, Math.PI * 2);
      ctx.fill();
      ctx.globalAlpha = 1;
    }
  }

  function drawTraps(world) {
    for (const tr of world.traps) {
      rect(tr.x - 8, tr.y - 3, 16, 4, tr.armed ? '#8a7a4a' : '#4a4238');
      if (tr.armed) {
        ctx.strokeStyle = '#c9a227';
        ctx.beginPath();
        ctx.moveTo(tr.x - 7, tr.y - 3);
        ctx.lineTo(tr.x - 3, tr.y - 8);
        ctx.moveTo(tr.x + 7, tr.y - 3);
        ctx.lineTo(tr.x + 3, tr.y - 8);
        ctx.stroke();
      }
    }
  }

  function drawDrops(world) {
    const t = world.time || 0;
    for (const d of world.drops) {
      const bob = Math.sin(t * 6 + d.anim) * 1.2;
      if (d.type === 'cell') {
        light(d.x, d.y + bob, 16, 'rgba(47,230,200,.5)');
        ctx.fillStyle = '#9ffff0';
        ctx.beginPath();
        ctx.moveTo(d.x, d.y - 3 + bob);
        ctx.lineTo(d.x + 2.5, d.y + bob);
        ctx.lineTo(d.x, d.y + 3 + bob);
        ctx.lineTo(d.x - 2.5, d.y + bob);
        ctx.fill();
      } else if (d.type === 'gold') {
        light(d.x, d.y + bob, 12, 'rgba(255,211,74,.35)');
        rect(d.x - 2, d.y - 2 + bob, 4, 4, '#ffd34a');
      } else if (d.type === 'heart') {
        light(d.x, d.y + bob, 14, 'rgba(255,59,92,.4)');
        rect(d.x - 3, d.y - 2 + bob, 6, 4, '#ff3b5c');
        rect(d.x - 2, d.y - 3 + bob, 4, 1, '#ff8aa8');
      } else if (d.type === 'key') {
        light(d.x, d.y + bob, 18, 'rgba(255,230,0,.4)');
        rect(d.x - 1, d.y - 4 + bob, 2, 8, '#ffe600');
        rect(d.x - 3, d.y - 4 + bob, 6, 2, '#ffe600');
      }
    }
  }

  /* ---------------------------------------------------------------- people */
  function shadow(e) {
    ctx.fillStyle = 'rgba(0,0,0,.35)';
    ctx.beginPath();
    ctx.ellipse(e.x, e.y, e.w * 0.6, 2.5, 0, 0, Math.PI * 2);
    ctx.fill();
  }

  function statusTint(e) {
    if (CB.hasStatus(e, 'frozen')) return 'rgba(127,216,255,.55)';
    if (CB.hasStatus(e, 'fire')) return 'rgba(255,138,30,.45)';
    if (CB.hasStatus(e, 'poison')) return 'rgba(141,219,58,.4)';
    if (CB.hasStatus(e, 'bleed')) return 'rgba(255,59,92,.28)';
    if (CB.hasStatus(e, 'stun')) return 'rgba(255,230,0,.3)';
    return null;
  }

  function drawPlayer(world) {
    const p = world.player;
    if (p.dead) {
      /* the body falls, the flame goes out */
      shadow(p);
      rect(p.x - 8, p.y - 5, 16, 5, '#1b2430');
      return;
    }
    shadow(p);

    const walking = Math.abs(p.vx) > 20 && p.onGround;
    const phase = walking ? Math.sin(p.anim * 16) : 0;
    const rolling = p.rollTimer > 0;
    const f = p.facing;
    const bodyTop = p.y - p.h;

    ctx.save();
    if (p.invuln > 0 && Math.floor(p.time * 30) % 2 === 0) ctx.globalAlpha = 0.55;

    if (rolling) {
      /* tucked into a ball, spinning */
      const spin = (1 - p.rollTimer / 0.3) * Math.PI * 2 * f;
      ctx.save();
      ctx.translate(p.x, p.y - 7);
      ctx.rotate(spin);
      rect(-6, -6, 12, 12, '#1b2430');
      rect(-6, -6, 12, 3, '#2f4154');
      ctx.restore();
      light(p.x, p.y - 7, 26, 'rgba(47,230,200,.3)');
      ctx.restore();
      return;
    }

    /* legs */
    rect(p.x - 4 + phase * 2, p.y - 8, 3, 8, '#141b26');
    rect(p.x + 1 - phase * 2, p.y - 8, 3, 8, '#1b2430');

    /* cloak, dragged by momentum */
    const drag = Math.max(-5, Math.min(5, -p.vx * 0.02));
    ctx.fillStyle = '#10161f';
    ctx.beginPath();
    ctx.moveTo(p.x - f * 3, bodyTop + 5);
    ctx.lineTo(p.x - f * 3 + drag, p.y - 2);
    ctx.lineTo(p.x + f * 2, p.y - 3);
    ctx.lineTo(p.x + f * 2, bodyTop + 5);
    ctx.fill();

    /* torso and shoulder light */
    rect(p.x - 4, bodyTop + 5, 8, 10, '#1b2430');
    rect(p.x - 4, bodyTop + 5, 8, 2, '#32465c');
    if (p.hitFlash > 0) {
      ctx.fillStyle = 'rgba(255,90,120,.5)';
      ctx.fillRect(p.x - 5, bodyTop + 4, 10, 12);
    }

    /* the head: a dark husk wearing a flame */
    rect(p.x - 3, bodyTop, 6, 6, '#0f141c');
    const flick = 1 + Math.sin(p.anim * 12) * 0.18;
    light(p.x, bodyTop + 1, 16 * flick, 'rgba(47,230,200,.55)');
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    ctx.fillStyle = 'rgba(160,255,240,.9)';
    ctx.beginPath();
    ctx.arc(p.x, bodyTop - 1, 2.4 * flick, 0, Math.PI * 2);
    ctx.fill();
    ctx.fillStyle = 'rgba(47,230,200,.6)';
    ctx.beginPath();
    ctx.arc(p.x, bodyTop - 4 - Math.sin(p.anim * 9) * 1.5, 1.6, 0, Math.PI * 2);
    ctx.fill();
    ctx.restore();

    drawHeldWeapon(world, p, f, bodyTop);

    if (p.blocking) {
      const shield = p.weapons[p.blockSlot];
      const parry = p.blockHeld <= (shield && shield.block ? shield.block : 0.4);
      rect(p.x + f * 6, bodyTop + 3, 3, 14, parry ? '#7fd8ff' : '#8fa7c4');
      if (parry) light(p.x + f * 8, bodyTop + 10, 22, 'rgba(127,216,255,.4)');
    }

    if (p.cursed) {
      ctx.save();
      ctx.globalCompositeOperation = 'lighter';
      light(p.x, p.y - 10, 26, 'rgba(180,60,220,.35)');
      ctx.restore();
    }

    ctx.restore();
  }

  /* The weapon in hand, plus the arc of a swing in progress. */
  function drawHeldWeapon(world, p, f, bodyTop) {
    const sw = p.swing;
    const weapon = (sw && sw.weapon) || p.weapons[p.activeSlot] || p.weapons[0];
    if (!weapon) return;

    const swinging = sw && sw.kind !== 'shoot';
    const prog = swinging ? 1 - sw.t / sw.total : 0;
    const hy = bodyTop + 9;

    if (swinging) {
      const reach = weapon.reach + (sw.step > 2 ? 5 : 0);
      const arc = weapon.arc || 1.2;
      const a0 = -arc / 2;
      const a1 = arc / 2;
      const ang = a0 + (a1 - a0) * prog;

      /* the sweep */
      ctx.save();
      ctx.translate(p.x + f * 4, hy);
      ctx.scale(f, 1);
      ctx.globalCompositeOperation = 'lighter';
      const g = ctx.createLinearGradient(0, 0, reach, 0);
      g.addColorStop(0, 'rgba(255,255,255,0)');
      g.addColorStop(1, weapon.kind === 'slam' ? 'rgba(255,140,60,.5)' : 'rgba(200,240,255,.5)');
      ctx.fillStyle = g;
      ctx.beginPath();
      ctx.moveTo(0, 0);
      ctx.arc(0, 0, reach, ang - 0.45, ang + 0.2);
      ctx.closePath();
      ctx.fill();
      ctx.restore();

      /* the weapon itself, mid-swing */
      ctx.save();
      ctx.translate(p.x + f * 4, hy);
      ctx.rotate(ang * f);
      ctx.scale(f, 1);
      drawWeaponShape(weapon, weapon.reach);
      ctx.restore();
      return;
    }

    ctx.save();
    ctx.translate(p.x + f * 5, hy);
    ctx.rotate(-0.35 * f);
    ctx.scale(f, 1);
    drawWeaponShape(weapon, weapon.reach);
    ctx.restore();
  }

  function drawWeaponShape(weapon, reach) {
    const len = Math.max(12, reach || 20);
    switch (weapon.kind) {
      case 'slam':
        rect(0, -1, len * 0.6, 2, '#6b5a44');
        rect(len * 0.6, -5, 8, 10, '#aab4c4');
        rect(len * 0.6, -5, 8, 2, '#e2e9f2');
        break;
      case 'thrust':
        rect(0, -1, len, 2, '#cfd6e4');
        rect(len - 3, -2, 4, 4, '#e8eef8');
        rect(-2, -2, 4, 4, '#6b5a44');
        break;
      case 'shoot':
        ctx.strokeStyle = '#8a6a3a';
        ctx.lineWidth = 1.5;
        ctx.beginPath();
        ctx.arc(2, 0, 7, -1.1, 1.1);
        ctx.stroke();
        ctx.strokeStyle = 'rgba(255,255,255,.5)';
        ctx.beginPath();
        ctx.moveTo(-1, -6);
        ctx.lineTo(-1, 6);
        ctx.stroke();
        break;
      case 'shield':
        rect(0, -7, 3, 14, '#8fa7c4');
        break;
      default:
        rect(0, -1, len, 2, '#dfe6f2');
        rect(len - 2, -1.5, 3, 3, '#ffffff');
        rect(-3, -2.5, 4, 5, '#6b5a44');
    }
  }

  function drawEnemies(world) {
    for (const e of world.enemies) drawEnemy(world, e);
    if (world.boss) drawBoss(world, world.boss);
  }

  function drawEnemy(world, e) {
    const def = e.def;
    const f = e.facing;
    const top = e.y - e.h;
    const hurt = e.hurtT > 0;
    const windup = e.state === 'windup';

    if (e.dead) {
      ctx.save();
      ctx.globalAlpha = Math.max(0, 1 - (e.fade || 0) / 1.2);
      rect(e.x - e.w / 2, e.y - 4, e.w, 4, '#2a1a1a');
      ctx.restore();
      return;
    }

    shadow(e);

    /* the wind-up tell: the body flares before the blow */
    if (windup) {
      const pulse = 0.4 + Math.sin(world.time * 30) * 0.3;
      ctx.save();
      ctx.globalAlpha = pulse;
      ctx.fillStyle = '#fff';
      ctx.fillRect(e.x - e.w / 2 - 1, top - 1, e.w + 2, e.h + 2);
      ctx.restore();
      light(e.x, top + e.h / 2, e.w * 2.2, 'rgba(255,80,80,.28)');
    }

    const base = hurt ? '#ffffff' : def.hex;
    const dark = hurt ? '#ffffff' : 'rgba(0,0,0,.45)';

    switch (e.ai) {
      case 'flyer': {
        const flap = Math.sin(e.anim * 18) * 4;
        ctx.fillStyle = base;
        ctx.beginPath();
        ctx.moveTo(e.x - e.w / 2, top + 4 - flap);
        ctx.lineTo(e.x, top + 5);
        ctx.lineTo(e.x + e.w / 2, top + 4 + flap);
        ctx.lineTo(e.x, top + 9);
        ctx.fill();
        rect(e.x - 2, top + 3, 4, 6, dark);
        break;
      }

      case 'caster': {
        ctx.fillStyle = base;
        ctx.beginPath();
        ctx.moveTo(e.x - e.w / 2, e.y);
        ctx.lineTo(e.x + e.w / 2, e.y);
        ctx.lineTo(e.x + 2, top + 2);
        ctx.lineTo(e.x - 2, top + 2);
        ctx.fill();
        rect(e.x - 3, top, 6, 5, '#1a1020');
        break;
      }

      case 'shielder': {
        rect(e.x - e.w / 2, top + 4, e.w, e.h - 4, base);
        rect(e.x - e.w / 2, top + 4, e.w, 2, '#cfd6e4');
        rect(e.x - 3, top, 6, 5, '#2a3442');
        if (!e.shieldBroken) {
          /* the shield dims as it takes punishment, so "nearly broken" is readable */
          const worn = Math.max(0, Math.min(1, e.shieldHp / Math.max(1, e.maxHp * 0.55)));
          rect(e.x + f * (e.w / 2), top + 3, 3, e.h - 6, '#cfd6e4');
          rect(e.x + f * (e.w / 2), top + 3 + (e.h - 6) * (1 - worn), 3, 2 + (e.h - 6) * worn * 0.1, '#ffffff');
          ctx.save();
          ctx.globalAlpha = 0.35 * (1 - worn);
          ctx.fillStyle = '#3a2a2a';
          ctx.fillRect(e.x + f * (e.w / 2), top + 3, 3, e.h - 6);
          ctx.restore();
        }
        break;
      }

      case 'slammer':
        rect(e.x - e.w / 2, top + 6, e.w, e.h - 6, base);
        rect(e.x - e.w / 2, top + 6, e.w, 2, '#ffb08a');
        rect(e.x - 5, top, 10, 7, '#3a1a14');
        rect(e.x + f * (e.w / 2 - 1), top + 8, 7, 5, '#8a8f9a');
        break;

      case 'archer':
      case 'bomber': {
        const step = e.onGround && Math.abs(e.vx) > 10 ? Math.sin(e.anim * 14) * 2 : 0;
        rect(e.x - 4 + step, e.y - 7, 3, 7, dark);
        rect(e.x + 1 - step, e.y - 7, 3, 7, dark);
        rect(e.x - e.w / 2 + 2, top + 5, e.w - 4, e.h - 12, base);
        rect(e.x - 3, top, 6, 6, base);
        rect(e.x + f * 2 - 1, top + 1, 2, 2, '#000');
        if (e.ai === 'archer') {
          ctx.strokeStyle = '#8a6a3a';
          ctx.lineWidth = 1;
          ctx.beginPath();
          ctx.arc(e.x + f * 6, top + 9, 6, -1.2, 1.2);
          ctx.stroke();
        } else {
          ctx.fillStyle = '#2a2a2a';
          ctx.beginPath();
          ctx.arc(e.x + f * 6, top + 10, 3, 0, Math.PI * 2);
          ctx.fill();
        }
        break;
      }

      default: {
        /* the shambling dead: hunched, one arm out */
        const step = e.onGround && Math.abs(e.vx) > 10 ? Math.sin(e.anim * 13) * 2.4 : 0;
        rect(e.x - 4 + step, e.y - 8, 3, 8, dark);
        rect(e.x + 1 - step, e.y - 8, 3, 8, dark);
        rect(e.x - e.w / 2 + 1, top + 5, e.w - 2, e.h - 13, base);
        rect(e.x - e.w / 2 + 1, top + 5, e.w - 2, 2, 'rgba(255,255,255,.25)');
        rect(e.x - 3, top, 6, 6, base);
        rect(e.x + f * 2 - 1, top + 2, 2, 2, '#300');
        rect(e.x + f * (e.w / 2 - 1), top + 7, 6, 2, base);
        break;
      }
    }

    const tint = statusTint(e);
    if (tint) {
      ctx.save();
      ctx.globalAlpha = 0.55;
      ctx.fillStyle = tint;
      ctx.fillRect(e.x - e.w / 2 - 1, top - 1, e.w + 2, e.h + 2);
      ctx.restore();
    }

    if (e.elite) {
      light(e.x, top + e.h / 2, e.w * 2.4, 'rgba(255,79,216,.22)');
      ctx.strokeStyle = 'rgba(255,79,216,.8)';
      ctx.lineWidth = 1;
      ctx.strokeRect(Math.round(e.x - e.w / 2) - 1.5, Math.round(top) - 1.5, e.w + 3, e.h + 3);
    }
    if (e.hasKey) {
      rect(e.x - 1, top - 6, 2, 4, '#ffe600');
      light(e.x, top - 5, 14, 'rgba(255,230,0,.35)');
    }

    /* a health pip, so you can read how close it is to dying */
    if (e.hp < e.maxHp) {
      const w = e.w + 4;
      rect(e.x - w / 2, top - 4, w, 2, 'rgba(0,0,0,.6)');
      rect(e.x - w / 2, top - 4, w * (e.hp / e.maxHp), 2, e.elite ? '#ff4fd8' : '#ff3b5c');
    }
  }

  function drawBoss(world, b) {
    if (b.dead) {
      ctx.save();
      ctx.globalAlpha = Math.max(0, 1 - (b.fade || 0) / 1.2);
      rect(b.x - b.w / 2, b.y - 6, b.w, 6, '#2a1a1a');
      ctx.restore();
      return;
    }
    shadow(b);
    const top = b.y - b.h;
    const f = b.facing;
    const hurt = b.hurtT > 0;
    const base = hurt ? '#ffffff' : b.def.hex;

    if (b.state === 'telegraph') {
      const pulse = 0.3 + Math.sin(world.time * 26) * 0.25;
      ctx.save();
      ctx.globalAlpha = pulse;
      ctx.fillStyle = '#fff';
      ctx.fillRect(b.x - b.w / 2 - 2, top - 2, b.w + 4, b.h + 4);
      ctx.restore();
    }

    /* legs, bulk, pauldrons, helm */
    rect(b.x - 9, b.y - 12, 7, 12, 'rgba(0,0,0,.6)');
    rect(b.x + 2, b.y - 12, 7, 12, 'rgba(0,0,0,.5)');
    rect(b.x - b.w / 2, top + 8, b.w, b.h - 18, base);
    rect(b.x - b.w / 2, top + 8, b.w, 3, 'rgba(255,255,255,.3)');
    rect(b.x - b.w / 2 - 3, top + 8, 5, 7, base);
    rect(b.x + b.w / 2 - 2, top + 8, 5, 7, base);
    rect(b.x - 6, top, 12, 9, '#17121c');
    ctx.fillStyle = '#ffe600';
    ctx.fillRect(b.x - 4, top + 3, 3, 2);
    ctx.fillRect(b.x + 1, top + 3, 3, 2);

    /* the weapon: a cleaver for the Warden, a long blade for the Hand */
    if (b.id === 'warden') {
      rect(b.x + f * (b.w / 2), top + 10, f > 0 ? 4 : -4, 20, '#8a8f9a');
      rect(b.x + f * (b.w / 2 + 2), top + 8, f > 0 ? 10 : -10, 8, '#cfd6e4');
    } else {
      ctx.save();
      ctx.translate(b.x + f * (b.w / 2), top + 14);
      ctx.rotate(f > 0 ? -0.4 : 0.4);
      rect(0, -1.5, f > 0 ? 34 : -34, 3, '#ffe600');
      ctx.restore();
      rect(b.x - 7, top - 4, 14, 4, '#ffe600');
    }

    if (b.vulnerable) {
      /* unmissable: this is the window the whole fight is built around */
      const pulse = 0.5 + Math.sin(world.time * 18) * 0.3;
      ctx.save();
      ctx.globalAlpha = pulse;
      ctx.strokeStyle = '#ffe600';
      ctx.lineWidth = 2;
      ctx.strokeRect(Math.round(b.x - b.w / 2) - 3.5, Math.round(top) - 3.5, b.w + 7, b.h + 7);
      ctx.restore();
      light(b.x, top + b.h / 2, b.w * 2.2, 'rgba(255,230,0,.26)');
    }
    if (b.phase === 2) {
      light(b.x, top + b.h / 2, b.w * 2.6, 'rgba(255,59,92,.28)');
    }
    const tint = statusTint(b);
    if (tint) {
      ctx.save();
      ctx.globalAlpha = 0.45;
      ctx.fillStyle = tint;
      ctx.fillRect(b.x - b.w / 2 - 2, top - 2, b.w + 4, b.h + 4);
      ctx.restore();
    }
  }

  function drawAllies(world) {
    for (const t of world.turrets) {
      rect(t.x - 6, t.y - 10, 12, 10, '#3a2f45');
      rect(t.x - 8, t.y - 12, 16, 3, '#b46bff');
      light(t.x, t.y - 8, 26, 'rgba(180,107,255,.25)');
    }
    for (const o of world.orbs) {
      ctx.save();
      ctx.translate(o.x, o.y);
      ctx.rotate(o.spin);
      ctx.fillStyle = '#ff3b5c';
      ctx.fillRect(-7, -2, 14, 4);
      ctx.fillRect(-2, -7, 4, 14);
      ctx.restore();
      light(o.x, o.y, 26, 'rgba(255,59,92,.35)');
    }
  }

  function drawProjectiles(world) {
    for (const pr of world.projectiles) {
      const ang = Math.atan2(pr.vy, pr.vx);
      ctx.save();
      ctx.translate(pr.x, pr.y);
      ctx.rotate(ang);
      switch (pr.kind) {
        case 'arrow':
          rect(-6, -0.5, 10, 1, '#e8dfc8');
          rect(3, -1, 3, 2, '#ffffff');
          break;
        case 'knife':
          rect(-4, -1, 8, 2, '#dfe6f2');
          break;
        case 'bolt':
          ctx.fillStyle = '#b46bff';
          ctx.fillRect(-8, -1, 16, 2);
          light(0, 0, 16, 'rgba(180,107,255,.45)');
          break;
        case 'spit':
          ctx.fillStyle = '#8ddb3a';
          ctx.beginPath();
          ctx.arc(0, 0, 3, 0, Math.PI * 2);
          ctx.fill();
          light(0, 0, 14, 'rgba(141,219,58,.4)');
          break;
        case 'orb':
          ctx.fillStyle = '#ff4fd8';
          ctx.beginPath();
          ctx.arc(0, 0, 4, 0, Math.PI * 2);
          ctx.fill();
          light(0, 0, 22, 'rgba(255,79,216,.45)');
          break;
        case 'grenade':
        case 'bomb':
          ctx.fillStyle = '#2a2a2a';
          ctx.beginPath();
          ctx.arc(0, 0, 4, 0, Math.PI * 2);
          ctx.fill();
          ctx.fillStyle = Math.floor(pr.anim * 12) % 2 ? '#ff8a1e' : '#ffe600';
          ctx.fillRect(-1, -6, 2, 3);
          light(0, 0, 18, 'rgba(255,138,30,.35)');
          break;
        case 'shard':
          ctx.fillStyle = '#ffe600';
          ctx.beginPath();
          ctx.moveTo(0, 6);
          ctx.lineTo(3, -4);
          ctx.lineTo(-3, -4);
          ctx.fill();
          break;
        case 'wave':
          ctx.restore();
          ctx.save();
          ctx.translate(pr.x, pr.y);
          ctx.fillStyle = 'rgba(255,140,60,.5)';
          ctx.fillRect(-6, -10, 12, 14);
          light(0, 0, 24, 'rgba(255,140,60,.4)');
          break;
        default:
          rect(-2, -2, 4, 4, '#fff');
      }
      ctx.restore();
    }
  }

  function drawEffects(world) {
    for (const s of world.slashes) {
      const a = s.life / s.total;
      ctx.save();
      ctx.globalAlpha = a;
      ctx.strokeStyle = s.color;
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.arc(s.x, s.y, 12, -0.8 * s.dir, 0.8 * s.dir, s.dir < 0);
      ctx.stroke();
      ctx.restore();
    }

    for (const r of world.rings) {
      const a = r.life / r.total;
      ctx.save();
      ctx.globalAlpha = a;
      ctx.strokeStyle = r.color;
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.arc(r.x, r.y, r.r, 0, Math.PI * 2);
      ctx.stroke();
      ctx.restore();
      light(r.x, r.y, r.r * 1.2, 'rgba(255,160,80,' + (0.3 * a).toFixed(3) + ')');
    }

    for (const pt of world.particles) {
      const a = Math.max(0, pt.life / pt.total);
      ctx.save();
      if (pt.glow) ctx.globalCompositeOperation = 'lighter';
      ctx.globalAlpha = a;
      ctx.fillStyle = pt.color;
      ctx.fillRect(Math.round(pt.x), Math.round(pt.y), pt.size, pt.size);
      ctx.restore();
    }

    for (const t of world.texts) {
      const a = Math.min(1, t.life / t.total * 1.6);
      ctx.save();
      ctx.globalAlpha = a;
      text(t.text, t.x, t.y, {
        align: 'center',
        color: t.color,
        size: Math.round(7 * t.scale),
        bold: t.scale > 1.1
      });
      ctx.restore();
    }
  }

  /* Torchlight and the glow of anything hot, in one additive pass. */
  function drawLights(world, pal) {
    const level = world.level;
    const t = world.time || 0;
    for (const tor of level.torches) {
      if (tor.x < ox() - 80 || tor.x > ox() + VW + 80) continue;
      const flick = 46 + Math.sin(t * 9 + tor.seed) * 5 + Math.sin(t * 21 + tor.seed) * 3;
      light(tor.x, tor.y, flick, 'rgba(255,150,60,.30)');
    }
    void pal;
  }

  function drawVignette(pal) {
    const g = ctx.createRadialGradient(VW / 2, VH / 2, Math.min(VW, VH) * 0.35, VW / 2, VH / 2, Math.max(VW, VH) * 0.72);
    g.addColorStop(0, 'rgba(0,0,0,0)');
    g.addColorStop(1, 'rgba(0,0,0,.55)');
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, VW, VH);
    void pal;
  }

  const API = {
    init: init,
    resize: resize,
    view: view,
    context: function () { return ctx; },
    camera: camera,
    updateCamera: updateCamera,
    centreCamera: centreCamera,
    drawWorld: drawWorld,
    light: light,
    rect: rect,
    text: text,
    measure: measure,
    panel: panel,
    radial: radial,
    ox: ox,
    oy: oy,
    COLORS: CONTENT ? CONTENT.COLORS : null
  };

  global.CELLS_RENDER = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
