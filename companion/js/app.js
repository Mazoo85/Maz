/*
 * COMPANION — the page
 * --------------------
 * The meadow, the player, the companion's body, and the chat. The thinking
 * lives in brain.js (trainable, offline) and mind.js (Claude, optional); this
 * file turns their answers into things you can see.
 */
(function () {
  'use strict';

  var Brain = window.CompanionBrain;
  var Mind = window.CompanionMind;

  var BRAIN_KEY = 'companion.brain.v1';
  var API_KEY = 'companion.apikey';
  var VOICE_KEY = 'companion.voice';

  // The world is laid out in a fixed 960 x 600 space; the canvas scales it.
  var W = 960, H = 600;

  var PLACES = [
    { name: 'tree', x: 170, y: 150, emoji: '🌳', size: 92 },
    { name: 'pond', x: 780, y: 140, pond: true },
    { name: 'campfire', x: 480, y: 320, emoji: '🔥', size: 46 },
    { name: 'rock', x: 160, y: 470, emoji: '🪨', size: 58 },
    { name: 'garden', x: 790, y: 470, emoji: '🌻', size: 50 }
  ];

  // Where each kind of item grows, and how many there can be at once.
  var ITEM_KINDS = {
    apple: { emoji: '🍎', near: 'tree', max: 3 },
    gem: { emoji: '💎', near: 'rock', max: 2 },
    flower: { emoji: '🌸', near: 'garden', max: 3 },
    mushroom: { emoji: '🍄', near: null, max: 3 }
  };

  // ---- storage (never fatal: private windows can refuse it) -----------------

  function load(key) {
    try { return localStorage.getItem(key); } catch (e) { return null; }
  }
  function save(key, value) {
    try {
      if (value === null) localStorage.removeItem(key);
      else localStorage.setItem(key, value);
    } catch (e) { /* the game still works, it just won't remember */ }
  }

  var brain = Brain.create(load(BRAIN_KEY));
  var history = new Mind.History();
  function saveBrain() { save(BRAIN_KEY, JSON.stringify(brain.toJSON())); }

  // ---- the world -----------------------------------------------------------

  var player = { x: 480, y: 430, tx: null, ty: null, speed: 230, dir: 1 };
  var pip = {
    x: 430, y: 450, speed: 190, dir: 1,
    mode: 'follow',     // what it does when it has no task
    queue: [],          // actions still to do, in order
    task: null,         // what it is doing right now
    carrying: null,
    anim: null,         // { type: 'jump'|'dance', t, dur }
    wanderAt: 0,
    mood: 'happy'
  };
  var items = [];
  var bag = {};
  var bubble = { text: '', until: 0 };
  var thinking = false;

  function place(name) {
    for (var i = 0; i < PLACES.length; i++) if (PLACES[i].name === name) return PLACES[i];
    return null;
  }

  function spawn(kind) {
    var k = ITEM_KINDS[kind];
    var p = k.near ? place(k.near) : null;
    for (var tries = 0; tries < 30; tries++) {
      var x = p ? p.x + (Math.random() - 0.5) * 170 : 60 + Math.random() * (W - 120);
      var y = p ? p.y + 40 + Math.random() * 70 : 60 + Math.random() * (H - 120);
      x = clamp(x, 30, W - 30);
      y = clamp(y, 30, H - 30);
      if (!tooClose(x, y)) { items.push({ kind: kind, x: x, y: y, born: now() }); return; }
    }
  }

  function tooClose(x, y) {
    for (var i = 0; i < PLACES.length; i++) if (dist(x, y, PLACES[i].x, PLACES[i].y) < 55) return true;
    for (var j = 0; j < items.length; j++) if (dist(x, y, items[j].x, items[j].y) < 30) return true;
    return false;
  }

  var regrowAt = 0;
  function regrow(t) {
    if (t < regrowAt) return;
    regrowAt = t + 6;
    for (var kind in ITEM_KINDS) {
      var have = items.filter(function (it) { return it.kind === kind; }).length;
      if (have < ITEM_KINDS[kind].max) { spawn(kind); return; }
    }
  }

  for (var kind in ITEM_KINDS) for (var n = 0; n < ITEM_KINDS[kind].max - 1; n++) spawn(kind);

  // What the minds are told exists right now.
  function worldInfo() {
    return {
      items: items.map(function (it) { return it.kind; }),
      places: PLACES.map(function (p) { return p.name; }),
      notes: bagText() ? 'The player\'s bag: ' + bagText() + '.' : ''
    };
  }

  // ---- the companion's body ------------------------------------------------

  // A fresh order replaces whatever it was doing.
  function perform(actions) {
    if (!actions.length) return;
    if (pip.carrying) dropCarried();
    pip.queue = actions.slice();
    pip.task = null;
    pip.anim = null;
    if (pip.mode === 'sleep') pip.mode = 'stay';
  }

  function dropCarried() {
    items.push({ kind: pip.carrying, x: pip.x, y: pip.y + 10, born: now() });
    pip.carrying = null;
  }

  function nextTask() {
    var a = pip.queue.shift();
    if (!a) return;
    switch (a.type) {
      case 'follow': case 'stay': case 'sit': case 'sleep': case 'wander':
        pip.mode = a.type;
        pip.wanderAt = 0;
        break;
      case 'come':
        pip.task = { kind: 'walk', to: function () { return besidePlayer(); }, then: function () { pip.mode = 'stay'; } };
        break;
      case 'goto':
        var spot = targetSpot(a.target);
        if (spot) pip.task = { kind: 'walk', to: function () { return spot; }, then: function () { pip.mode = 'stay'; } };
        break;
      case 'fetch':
        var it = nearestItem(a.target);
        if (!it) { speak('I can\'t find a ' + a.target + ' any more!'); break; }
        var modeBefore = pip.mode === 'sleep' ? 'follow' : pip.mode;
        pip.task = {
          kind: 'walk', item: it,
          to: function () { return it; },
          then: function () {
            var i = items.indexOf(it);
            if (i === -1) { speak('Aw, it\'s gone!'); return; }
            items.splice(i, 1);
            pip.carrying = it.kind;
            pip.task = {
              kind: 'deliver',
              to: function () { return besidePlayer(); },
              then: function () {
                bag[pip.carrying] = (bag[pip.carrying] || 0) + 1;
                speak(pip.carrying === 'gem' ? 'Ooh, shiny! Here you go!' : 'Here you go! One ' + pip.carrying + '.');
                pip.carrying = null;
                pip.mode = modeBefore === 'stay' ? 'follow' : modeBefore;
                renderBag();
              }
            };
          }
        };
        break;
      case 'jump':
        pip.anim = { type: 'jump', t: 0, dur: 0.7 };
        break;
      case 'dance':
        pip.anim = { type: 'dance', t: 0, dur: 2.2 };
        break;
    }
  }

  function targetSpot(target) {
    var step = 220;
    if (target === 'left') return { x: clamp(pip.x - step, 30, W - 30), y: pip.y };
    if (target === 'right') return { x: clamp(pip.x + step, 30, W - 30), y: pip.y };
    if (target === 'up') return { x: pip.x, y: clamp(pip.y - step, 30, H - 30) };
    if (target === 'down') return { x: pip.x, y: clamp(pip.y + step, 30, H - 30) };
    var p = place(target);
    return p ? { x: p.x, y: p.y + (p.pond ? 70 : 50) } : null;
  }

  function besidePlayer() {
    return { x: player.x - 46 * player.dir, y: player.y + 6 };
  }

  function nearestItem(kind) {
    var best = null, bestD = Infinity;
    items.forEach(function (it) {
      if (it.kind !== kind) return;
      var d = dist(pip.x, pip.y, it.x, it.y);
      if (d < bestD) { bestD = d; best = it; }
    });
    return best;
  }

  // Step toward a point; true once it's there.
  function walkTo(body, x, y, dt, near) {
    var dx = x - body.x, dy = y - body.y;
    var d = Math.sqrt(dx * dx + dy * dy);
    if (d <= (near || 6)) return true;
    var step = Math.min(d, body.speed * dt);
    body.x += dx / d * step;
    body.y += dy / d * step;
    if (Math.abs(dx) > 1) body.dir = dx > 0 ? 1 : -1;
    body.moving = true;
    return false;
  }

  function updatePip(dt, t) {
    pip.moving = false;
    if (pip.anim) {
      pip.anim.t += dt;
      if (pip.anim.t >= pip.anim.dur) pip.anim = null;
      return;
    }
    if (!pip.task && pip.queue.length) nextTask();
    if (pip.anim) return;
    if (pip.task) {
      var to = pip.task.to();
      if (walkTo(pip, to.x, to.y, dt, 8)) {
        var done = pip.task;
        pip.task = null;
        if (done.then) done.then();
      }
      return;
    }
    if (pip.mode === 'follow') {
      if (dist(pip.x, pip.y, player.x, player.y) > 70) {
        var b = besidePlayer();
        walkTo(pip, b.x, b.y, dt, 10);
      }
    } else if (pip.mode === 'wander') {
      if (t > pip.wanderAt || !pip.wanderTo) {
        pip.wanderTo = { x: 60 + Math.random() * (W - 120), y: 60 + Math.random() * (H - 120) };
        pip.wanderAt = t + 3 + Math.random() * 3;
      }
      walkTo(pip, pip.wanderTo.x, pip.wanderTo.y, dt * 0.6, 10);
    }
  }

  // ---- the player ----------------------------------------------------------

  var keys = {};
  function typing() {
    var el = document.activeElement;
    return el && (el.tagName === 'INPUT' || el.tagName === 'TEXTAREA');
  }
  window.addEventListener('keydown', function (e) {
    if (typing()) return;
    keys[e.key.toLowerCase()] = true;
    if (/^arrow/.test(e.key.toLowerCase())) e.preventDefault();
  });
  window.addEventListener('keyup', function (e) { keys[e.key.toLowerCase()] = false; });
  window.addEventListener('blur', function () { keys = {}; });

  function updatePlayer(dt) {
    var vx = 0, vy = 0;
    if (keys.arrowleft || keys.a) vx -= 1;
    if (keys.arrowright || keys.d) vx += 1;
    if (keys.arrowup || keys.w) vy -= 1;
    if (keys.arrowdown || keys.s) vy += 1;
    player.moving = false;
    if (vx || vy) {
      player.tx = null;
      var len = Math.sqrt(vx * vx + vy * vy);
      player.x = clamp(player.x + vx / len * player.speed * dt, 24, W - 24);
      player.y = clamp(player.y + vy / len * player.speed * dt, 24, H - 24);
      if (vx) player.dir = vx > 0 ? 1 : -1;
      player.moving = true;
    } else if (player.tx !== null) {
      if (walkTo(player, player.tx, player.ty, dt, 4)) player.tx = null;
    }
  }

  // ---- drawing -------------------------------------------------------------

  var canvas = document.getElementById('world');
  var ctx = canvas.getContext('2d');
  var scale = 1;

  function resize() {
    var r = canvas.getBoundingClientRect();
    var dpr = Math.min(window.devicePixelRatio || 1, 2);
    canvas.width = Math.round(r.width * dpr);
    canvas.height = Math.round(r.width * (H / W) * dpr);
    scale = canvas.width / W;
  }
  window.addEventListener('resize', resize);

  canvas.addEventListener('pointerdown', function (e) {
    var r = canvas.getBoundingClientRect();
    player.tx = clamp((e.clientX - r.left) / r.width * W, 24, W - 24);
    player.ty = clamp((e.clientY - r.top) / r.height * H, 24, H - 24);
    if (document.activeElement && document.activeElement.blur) document.activeElement.blur();
  });

  var dark = window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)');

  function draw(t) {
    ctx.setTransform(scale, 0, 0, scale, 0, 0);
    var night = dark && dark.matches;
    var g = ctx.createLinearGradient(0, 0, 0, H);
    g.addColorStop(0, night ? '#22402c' : '#a8dc8a');
    g.addColorStop(1, night ? '#183022' : '#86c76a');
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, W, H);

    // A few tufts of grass, always in the same spots.
    ctx.fillStyle = night ? 'rgba(255,255,255,0.05)' : 'rgba(40,90,30,0.12)';
    for (var i = 0; i < 40; i++) {
      var gx = (i * 211) % W, gy = (i * 137) % H;
      ctx.fillRect(gx, gy, 3, 8);
      ctx.fillRect(gx + 5, gy + 2, 3, 6);
    }

    // Everything sorted by height on screen, so things lower down are in front.
    var things = [];
    PLACES.forEach(function (p) { things.push({ y: p.y, draw: function () { drawPlace(p, t, night); } }); });
    items.forEach(function (it) { things.push({ y: it.y, draw: function () { drawItem(it, t); } }); });
    things.push({ y: player.y, draw: function () { drawPlayer(t); } });
    things.push({ y: pip.y, draw: function () { drawPip(t); } });
    things.sort(function (a, b) { return a.y - b.y; });
    things.forEach(function (th) { th.draw(); });

    drawBubble(t);
  }

  function drawPlace(p, t, night) {
    if (p.pond) {
      ctx.fillStyle = night ? '#2b5d7a' : '#69b7e0';
      ctx.beginPath();
      ctx.ellipse(p.x, p.y, 110, 55, 0, 0, Math.PI * 2);
      ctx.fill();
      ctx.strokeStyle = 'rgba(255,255,255,0.35)';
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.ellipse(p.x - 20, p.y - 8, 40 + Math.sin(t * 2) * 4, 10, 0, 0, Math.PI * 2);
      ctx.stroke();
    } else {
      shadow(p.x, p.y + p.size * 0.42, p.size * 0.45);
      emoji(p.emoji, p.x, p.y, p.size * (p.name === 'campfire' ? 1 + Math.sin(t * 9) * 0.05 : 1));
    }
    label(p.name, p.x, p.y + (p.pond ? 72 : p.size * 0.62));
  }

  function drawItem(it, t) {
    var bob = Math.sin(t * 3 + it.x) * 2;
    var grow = Math.min(1, (now() - it.born) * 2);
    shadow(it.x, it.y + 12, 9);
    emoji(ITEM_KINDS[it.kind].emoji, it.x, it.y + bob, 28 * grow);
  }

  function drawPlayer(t) {
    var bounce = player.moving ? Math.abs(Math.sin(t * 12)) * 4 : 0;
    var x = player.x, y = player.y - bounce;
    shadow(player.x, player.y + 20, 15);
    ctx.fillStyle = '#3c6fd8';
    roundRect(x - 13, y - 6, 26, 26, 8);
    ctx.fill();
    ctx.fillStyle = '#f3c9a0';
    ctx.beginPath();
    ctx.arc(x, y - 16, 13, 0, Math.PI * 2);
    ctx.fill();
    ctx.fillStyle = '#5a3a22';
    ctx.beginPath();
    ctx.arc(x, y - 21, 13, Math.PI, Math.PI * 2);
    ctx.fill();
    ctx.fillStyle = '#222';
    ctx.beginPath();
    ctx.arc(x + 4 * player.dir, y - 15, 2, 0, Math.PI * 2);
    ctx.fill();
    label(brain.playerName || 'you', x, y + 34);
  }

  function drawPip(t) {
    var x = pip.x, y = pip.y, squash = 1, tilt = 0;
    if (pip.moving) y -= Math.abs(Math.sin(t * 11)) * 5;
    if (pip.anim && pip.anim.type === 'jump') {
      var k = pip.anim.t / pip.anim.dur;
      y -= Math.sin(k * Math.PI) * 46;
      squash = 1 + Math.sin(k * Math.PI) * 0.12;
    }
    if (pip.anim && pip.anim.type === 'dance') {
      tilt = Math.sin(pip.anim.t * 12) * 0.35;
      y -= Math.abs(Math.sin(pip.anim.t * 6)) * 14;
    }
    var sitting = !pip.task && !pip.anim && (pip.mode === 'sit' || pip.mode === 'sleep');
    if (sitting) squash = 0.82;

    shadow(pip.x, pip.y + 20, 17);
    ctx.save();
    ctx.translate(x, y + 18);
    ctx.rotate(tilt);
    ctx.scale(1 / squash, squash);

    // Body: a round orange blob with little feet.
    ctx.fillStyle = '#f2a65a';
    ctx.beginPath();
    ctx.ellipse(0, -20, 22, 21, 0, 0, Math.PI * 2);
    ctx.fill();
    ctx.fillStyle = '#d9772b';
    ctx.beginPath();
    ctx.ellipse(-10, -1, 7, 4, 0, 0, Math.PI * 2);
    ctx.ellipse(10, -1, 7, 4, 0, 0, Math.PI * 2);
    ctx.fill();
    // Ears.
    ctx.fillStyle = '#f2a65a';
    ctx.beginPath();
    ctx.moveTo(-16, -34); ctx.lineTo(-20, -48); ctx.lineTo(-6, -40); ctx.closePath();
    ctx.moveTo(16, -34); ctx.lineTo(20, -48); ctx.lineTo(6, -40); ctx.closePath();
    ctx.fill();

    // Face: eyes and mouth change with mood and sleep.
    var ex = 4 * pip.dir;
    ctx.strokeStyle = '#222';
    ctx.fillStyle = '#222';
    ctx.lineWidth = 2;
    if (pip.mode === 'sleep' && sitting) {
      ctx.beginPath();
      ctx.moveTo(-11 + ex, -24); ctx.lineTo(-4 + ex, -24);
      ctx.moveTo(4 + ex, -24); ctx.lineTo(11 + ex, -24);
      ctx.stroke();
    } else {
      var blink = (t % 4) < 0.12 ? 0.3 : 1;
      ctx.beginPath();
      ctx.ellipse(-7 + ex, -24, 3, 4 * blink, 0, 0, Math.PI * 2);
      ctx.ellipse(7 + ex, -24, 3, 4 * blink, 0, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.beginPath();
    if (pip.mood === 'sad') ctx.arc(ex, -10, 5, Math.PI * 1.15, Math.PI * 1.85);
    else if (pip.mood === 'confused') { ctx.moveTo(ex - 5, -13); ctx.lineTo(ex + 5, -15); }
    else ctx.arc(ex, -17, 6, Math.PI * 0.15, Math.PI * 0.85);
    ctx.stroke();
    ctx.restore();

    if (pip.carrying) emoji(ITEM_KINDS[pip.carrying].emoji, x, y - 42, 24);
    if (pip.mode === 'sleep' && sitting) {
      ctx.fillStyle = 'rgba(255,255,255,0.9)';
      ctx.font = 'bold 16px system-ui, sans-serif';
      ctx.fillText('z', x + 22, y - 30 - (t * 10 % 12));
      ctx.fillText('Z', x + 32, y - 44 - (t * 10 % 12));
    }
    if (thinking) emoji('💭', x + 26, y - 40, 24);
    label(brain.name, pip.x, pip.y + 34);
  }

  function drawBubble(t) {
    if (!bubble.text || t > bubble.until) return;
    ctx.font = '16px system-ui, sans-serif';
    var lines = wrap(bubble.text, 260);
    var lh = 20, pad = 10;
    var bw = 0;
    lines.forEach(function (l) { bw = Math.max(bw, ctx.measureText(l).width); });
    bw += pad * 2;
    var bh = lines.length * lh + pad * 2 - 4;
    var bx = clamp(pip.x - bw / 2, 8, W - bw - 8);
    var by = pip.y - 70 - bh;
    if (by < 8) by = pip.y + 44;
    var fade = Math.min(1, (bubble.until - t) * 2);
    ctx.globalAlpha = fade;
    ctx.fillStyle = 'rgba(255,255,255,0.96)';
    roundRect(bx, by, bw, bh, 10);
    ctx.fill();
    ctx.fillStyle = '#1f2a22';
    lines.forEach(function (l, i) { ctx.fillText(l, bx + pad, by + pad + 12 + i * lh); });
    ctx.globalAlpha = 1;
  }

  function wrap(text, max) {
    var ws = text.split(/\s+/), lines = [], line = '';
    ws.forEach(function (w) {
      var test = line ? line + ' ' + w : w;
      if (ctx.measureText(test).width > max && line) { lines.push(line); line = w; }
      else line = test;
    });
    if (line) lines.push(line);
    return lines.slice(0, 6);
  }

  function emoji(ch, x, y, size) {
    ctx.font = Math.round(size) + 'px "Apple Color Emoji","Segoe UI Emoji","Noto Color Emoji",sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(ch, x, y);
    ctx.textAlign = 'start';
    ctx.textBaseline = 'alphabetic';
  }

  function label(text, x, y) {
    ctx.font = '600 13px system-ui, sans-serif';
    ctx.textAlign = 'center';
    ctx.fillStyle = 'rgba(0,0,0,0.55)';
    ctx.fillText(text, x + 1, y + 1);
    ctx.fillStyle = '#fff';
    ctx.fillText(text, x, y);
    ctx.textAlign = 'start';
  }

  function shadow(x, y, r) {
    ctx.fillStyle = 'rgba(0,0,0,0.18)';
    ctx.beginPath();
    ctx.ellipse(x, y, r, r * 0.35, 0, 0, Math.PI * 2);
    ctx.fill();
  }

  function roundRect(x, y, w, h, r) {
    ctx.beginPath();
    ctx.moveTo(x + r, y);
    ctx.arcTo(x + w, y, x + w, y + h, r);
    ctx.arcTo(x + w, y + h, x, y + h, r);
    ctx.arcTo(x, y + h, x, y, r);
    ctx.arcTo(x, y, x + w, y, r);
    ctx.closePath();
  }

  // ---- the loop ------------------------------------------------------------

  var last = 0;
  function frame(ms) {
    var t = ms / 1000;
    var dt = Math.min(0.05, last ? t - last : 0);
    last = t;
    updatePlayer(dt);
    updatePip(dt, t);
    regrow(t);
    draw(t);
    requestAnimationFrame(frame);
  }

  function now() { return performance.now() / 1000; }

  // ---- chat ----------------------------------------------------------------

  var log = document.getElementById('log');
  var form = document.getElementById('sayForm');
  var input = document.getElementById('sayInput');

  function addLine(cls, text, tag) {
    var li = document.createElement('li');
    li.className = cls;
    li.textContent = text;
    if (tag) {
      var s = document.createElement('span');
      s.className = 'tag';
      s.textContent = tag;
      li.appendChild(s);
    }
    log.appendChild(li);
    while (log.children.length > 200) log.removeChild(log.firstChild);
    log.scrollTop = log.scrollHeight;
  }

  function speak(text, tag) {
    bubble.text = text;
    bubble.until = now() + Math.max(3, text.length / 14);
    addLine('pip', text, tag);
    if (voiceOn && window.speechSynthesis) {
      try {
        window.speechSynthesis.cancel();
        var u = new SpeechSynthesisUtterance(text.replace(/\(.*?\)/g, ''));
        u.pitch = 1.5;
        u.rate = 1.05;
        window.speechSynthesis.speak(u);
      } catch (e) { /* no voice on this device */ }
    }
  }

  var TAGS = {
    teach: 'learned something new',
    taught: 'used something you taught',
    memory: 'memory'
  };

  async function hear(text) {
    text = text.trim();
    if (!text || thinking) return;
    addLine('me', text);
    var world = worldInfo();
    var r = brain.hear(text, world);
    saveBrain();
    renderLearned();

    var useAI = apiKey && (r.kind === 'chat' || r.kind === 'unknown' || r.kind === 'missing');
    if (useAI) {
      thinking = true;
      try {
        var ai = await askClaude(text, world);
        r = { say: ai.say, actions: ai.actions, mood: 'happy', kind: 'ai' };
      } catch (e) {
        addLine('note', Mind.explain(e) + ' So I answered on my own.');
      } finally {
        thinking = false;
      }
    }

    pip.mood = r.mood || 'happy';
    speak(r.say, r.kind === 'ai' ? 'Claude' : r.guessed ? 'a guess — say "good job" or "wrong"' : TAGS[r.kind]);
    perform(r.actions || []);
    history.add(text, r.say);
  }

  form.addEventListener('submit', function (e) {
    e.preventDefault();
    var text = input.value;
    input.value = '';
    hear(text);
  });

  var TRY = ['follow me', 'stay', 'get the apple', 'go to the pond', 'jump and then dance',
    'when I say heel, follow me', 'my name is …', 'what do you remember?'];
  var chips = document.getElementById('chips');
  TRY.forEach(function (t) {
    var b = document.createElement('button');
    b.type = 'button';
    b.textContent = t;
    b.addEventListener('click', function () {
      if (t.indexOf('…') !== -1) { input.value = t.replace('…', ''); input.focus(); return; }
      hear(t);
    });
    chips.appendChild(b);
  });

  // ---- voice input ---------------------------------------------------------

  var Rec = window.SpeechRecognition || window.webkitSpeechRecognition;
  var micBtn = document.getElementById('micBtn');
  if (Rec) {
    micBtn.hidden = false;
    var rec = null;
    micBtn.addEventListener('click', function () {
      if (rec) { rec.stop(); return; }
      rec = new Rec();
      rec.lang = navigator.language || 'en-US';
      rec.interimResults = false;
      rec.onresult = function (e) { hear(e.results[0][0].transcript); };
      rec.onend = function () { rec = null; micBtn.classList.remove('on'); };
      rec.onerror = function () { addLine('note', 'I couldn\'t hear that. Check the microphone permission.'); };
      micBtn.classList.add('on');
      rec.start();
    });
  }

  // ---- Claude --------------------------------------------------------------

  var apiKey = load(API_KEY) || '';
  var voiceOn = load(VOICE_KEY) === '1';
  var client = null;

  async function askClaude(text, world) {
    if (!client) client = await Mind.connect(apiKey);
    var self = { mode: pip.task ? 'busy with a task' : pip.mode, carrying: pip.carrying };
    // Checked against the world as it is when the answer arrives, not when asked.
    var reply = await Mind.ask(client, brain, history, text, world, self);
    var now = worldInfo();
    reply.actions = reply.actions.filter(function (a) { return brain.canDo(a, now); });
    return reply;
  }

  var keyInput = document.getElementById('keyInput');
  var aiStatus = document.getElementById('aiStatus');
  function renderAI() {
    aiStatus.textContent = apiKey ? 'on' : 'off';
    aiStatus.classList.toggle('on', !!apiKey);
    keyInput.value = '';
    keyInput.placeholder = apiKey ? 'Key saved (' + apiKey.slice(0, 7) + '…)' : 'sk-ant-…';
  }
  document.getElementById('keySave').addEventListener('click', function () {
    var k = keyInput.value.trim();
    if (!k) { keyInput.focus(); return; }
    apiKey = k;
    client = null;
    save(API_KEY, k);
    renderAI();
    addLine('note', 'Smart chat is on. Talk to ' + brain.name + ' about anything!');
  });
  document.getElementById('keyClear').addEventListener('click', function () {
    apiKey = '';
    client = null;
    save(API_KEY, null);
    renderAI();
    addLine('note', 'Smart chat is off. ' + brain.name + ' still knows everything you taught.');
  });
  var voiceToggle = document.getElementById('voiceToggle');
  voiceToggle.checked = voiceOn;
  voiceToggle.addEventListener('change', function () {
    voiceOn = voiceToggle.checked;
    save(VOICE_KEY, voiceOn ? '1' : '0');
  });

  // ---- what it has learned -------------------------------------------------

  var learned = document.getElementById('learned');
  function renderLearned() {
    document.querySelectorAll('.cname').forEach(function (el) { el.textContent = brain.name; });
    learned.textContent = '';
    var keysList = Object.keys(brain.phrases);
    if (!keysList.length && !brain.facts.length && !brain.playerName) {
      var p = document.createElement('p');
      p.className = 'empty';
      p.textContent = 'Nothing yet. Try "when I say heel, follow me" or "remember that I like apples".';
      learned.appendChild(p);
      return;
    }
    var ul = document.createElement('ul');
    ul.className = 'learned-list';
    if (brain.playerName) ul.appendChild(row('Your name: ' + brain.playerName));
    keysList.forEach(function (k) {
      var ph = brain.phrases[k];
      var what = ph.reply ? 'say "' + ph.reply + '"' : brain.describe(ph.actions);
      ul.appendChild(row('"' + k + '" → ' + what, function () { delete brain.phrases[k]; }));
    });
    brain.facts.forEach(function (f, i) {
      ul.appendChild(row('Remembers: ' + f, function () { brain.facts.splice(i, 1); }));
    });
    learned.appendChild(ul);
  }

  function row(text, onForget) {
    var li = document.createElement('li');
    var span = document.createElement('span');
    span.textContent = text;
    li.appendChild(span);
    if (onForget) {
      var b = document.createElement('button');
      b.type = 'button';
      b.textContent = 'forget';
      b.addEventListener('click', function () { onForget(); saveBrain(); renderLearned(); });
      li.appendChild(b);
    }
    return li;
  }

  document.getElementById('exportBtn').addEventListener('click', function () {
    var blob = new Blob([JSON.stringify(brain.toJSON(), null, 2)], { type: 'application/json' });
    var a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = brain.name.toLowerCase().replace(/[^a-z0-9]+/g, '-') + '-brain.json';
    a.click();
    setTimeout(function () { URL.revokeObjectURL(a.href); }, 1000);
  });

  document.getElementById('importFile').addEventListener('change', function (e) {
    var f = e.target.files[0];
    if (!f) return;
    f.text().then(function (txt) {
      var fresh = Brain.create();
      if (!fresh.load(txt)) { addLine('note', 'That file isn\'t a companion brain.'); return; }
      brain = fresh;
      saveBrain();
      renderLearned();
      speak('Whoa... I remember things! Hi, I\'m ' + brain.name + '.', 'memory');
    });
    e.target.value = '';
  });

  document.getElementById('resetBtn').addEventListener('click', function () {
    if (!window.confirm('Forget everything ' + brain.name + ' has learned?')) return;
    brain = Brain.create();
    history = new Mind.History();
    saveBrain();
    renderLearned();
    speak('Hi! I\'m Pip. Who are you?');
  });

  // ---- helpers & start -----------------------------------------------------

  function dist(ax, ay, bx, by) { var dx = ax - bx, dy = ay - by; return Math.sqrt(dx * dx + dy * dy); }
  function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)); }

  function bagText() {
    var parts = Object.keys(bag).map(function (k) { return bag[k] + ' ' + k + (bag[k] > 1 ? 's' : ''); });
    return parts.join(', ');
  }
  function renderBag() {
    var parts = Object.keys(bag).map(function (k) { return ITEM_KINDS[k].emoji + ' ' + bag[k]; });
    document.getElementById('bag').textContent = 'Bag: ' + (parts.join('  ') || 'empty');
  }

  resize();
  renderAI();
  renderLearned();
  speak(brain.playerName ? 'Welcome back, ' + brain.playerName + '!' : 'Hi! I\'m ' + brain.name + '. Tell me what to do, or just chat!');
  requestAnimationFrame(frame);
})();
