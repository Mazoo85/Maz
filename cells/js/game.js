/* =============================================================================
 *  NEON CELLS  —  the game
 *
 *  A roguelite action-platformer in the shape of Dead Cells: you fight down
 *  through procedurally built biomes, the scrolls you find decide what kind of
 *  fighter this run is, and when you die you start again at the top — keeping
 *  only what you spent cells on at a Collector.
 *
 *  Controls (keyboard)        Controls (touch)
 *    A / D or arrows  move      left thumb drags to move
 *    Space / W        jump      right thumb: the labelled buttons
 *    Shift            roll (invulnerable — roll through enemies to backstab)
 *    J / K            left and right hand weapons
 *    U / I            the two skills
 *    Q                health flask
 *    E                interact: doors, chests, scrolls, shops, the Collector
 *    Esc              pause      M  mute
 * ========================================================================== */
(function (global) {
  'use strict';

  const RNG = global.CELLS_RNG;
  const CONTENT = global.CELLS_CONTENT;
  const LG = global.CELLS_LEVELGEN;
  const CB = global.CELLS_COMBAT;
  const EN = global.CELLS_ENTITIES;
  const R = global.CELLS_RENDER;
  const META = global.CELLS_META;
  const AUDIO = global.CELLS_AUDIO;

  const STEP = 1 / 60;
  const STATE = {
    TITLE: 'TITLE', INTRO: 'INTRO', PLAY: 'PLAY', PAUSE: 'PAUSE',
    SCROLL: 'SCROLL', MUTATE: 'MUTATE', SHOP: 'SHOP', COLLECTOR: 'COLLECTOR',
    DEAD: 'DEAD', VICTORY: 'VICTORY', HELP: 'HELP'
  };

  let canvas = null;
  let state = STATE.TITLE;
  let lastTime = 0;
  let acc = 0;
  let world = null;
  let run = null;
  let rng = null;
  let meta = null;
  let introT = 0;
  let choices = null;        // whatever the current menu is offering
  let menuIndex = 0;
  let shopStock = null;
  let nearby = null;         // the object the player could interact with
  let visited = null;        // rooms seen, for the map
  let toast = null;
  let titleT = 0;
  let stepsRan = 0;          // world steps in the last frame
  let lastShake = 0;         // for deciding when to rumble a controller
  let heldPressT = 0;        // how long an unconsumed press has been waiting

  /* =========================================================================
   *  INPUT
   * ====================================================================== */
  const keys = Object.create(null);
  const pressed = Object.create(null);
  const touch = {
    active: false,
    move: { id: null, x0: 0, dir: 0, down: false },
    buttons: []
  };

  /* A controller's buttons are fed in as codes of their own (PadA, PadLB, …)
   * alongside the keyboard's. Everything downstream — playing, menus, and the
   * latching that stops a press being swallowed on a frame with no world step —
   * then works for a pad without knowing a pad exists. */
  const KEYMAP = {
    left: ['ArrowLeft', 'KeyA', 'PadLeft'],
    right: ['ArrowRight', 'KeyD', 'PadRight'],
    up: ['ArrowUp', 'KeyW', 'PadUp'],
    down: ['ArrowDown', 'KeyS', 'PadDown'],
    jump: ['Space', 'KeyW', 'ArrowUp', 'PadA'],
    roll: ['ShiftLeft', 'ShiftRight', 'KeyL', 'PadB'],
    atk1: ['KeyJ', 'KeyZ', 'PadX'],
    atk2: ['KeyK', 'KeyX', 'PadY'],
    skill1: ['KeyU', 'KeyC', 'Digit1', 'PadLB'],
    skill2: ['KeyI', 'KeyV', 'Digit2', 'PadRB'],
    flask: ['KeyQ', 'KeyH', 'PadRT'],
    interact: ['KeyE', 'Enter', 'PadLT'],
    pause: ['Escape', 'KeyP', 'PadStart'],
    mute: ['KeyM', 'PadBack']
  };

  /* Standard-mapping button numbers → the codes above. Directions are handled
   * separately because the d-pad and the left stick both feed them. */
  const PAD_BUTTONS = {
    0: 'PadA', 1: 'PadB', 2: 'PadX', 3: 'PadY',
    4: 'PadLB', 5: 'PadRB', 6: 'PadLT', 7: 'PadRT',
    8: 'PadBack', 9: 'PadStart'
  };
  const STICK_DEADZONE = 0.45;

  let padIndex = null;
  let padConnected = false;
  let lastDevice = 'key';       // key | pad | touch — decides the button hints
  let calmMotion = false;       // set at boot from prefers-reduced-motion

  function currentPad() {
    if (!global.navigator || !navigator.getGamepads) return null;
    let list = null;
    try {
      list = navigator.getGamepads();
    } catch (e) {
      return null;
    }
    if (!list) return null;
    if (padIndex != null && list[padIndex] && list[padIndex].connected) return list[padIndex];
    for (let i = 0; i < list.length; i++) {
      if (list[i] && list[i].connected) {
        padIndex = i;
        return list[i];
      }
    }
    padIndex = null;
    return null;
  }

  /* Called once a frame, before anything reads the input. */
  function pollPad() {
    const gp = currentPad();
    if (!gp) {
      if (padConnected) {
        padConnected = false;
        for (const code of Object.values(PAD_BUTTONS)) keys[code] = false;
        for (const code of ['PadLeft', 'PadRight', 'PadUp', 'PadDown']) keys[code] = false;
      }
      return;
    }
    padConnected = true;

    let touched = false;

    function set(code, down) {
      if (down && !keys[code]) pressed[code] = true;
      keys[code] = down;
      if (down) touched = true;
    }

    const buttons = gp.buttons || [];
    function held(i) {
      const b = buttons[i];
      if (!b) return false;
      return typeof b === 'object' ? b.pressed || b.value > 0.45 : b > 0.45;
    }

    for (const index of Object.keys(PAD_BUTTONS)) set(PAD_BUTTONS[index], held(Number(index)));

    /* d-pad or left stick, whichever the player reaches for */
    const axes = gp.axes || [];
    const ax = axes[0] || 0;
    const ay = axes[1] || 0;
    set('PadLeft', held(14) || ax < -STICK_DEADZONE);
    set('PadRight', held(15) || ax > STICK_DEADZONE);
    set('PadUp', held(12) || ay < -STICK_DEADZONE);
    set('PadDown', held(13) || ay > STICK_DEADZONE);

    if (touched) lastDevice = 'pad';
  }

  /* Physical feedback for the same moments that shake the screen. */
  function rumble(strength, ms) {
    if (calmMotion) return;
    const gp = currentPad();
    if (!gp) return;
    const actuator = gp.vibrationActuator;
    if (!actuator || !actuator.playEffect) return;
    try {
      actuator.playEffect('dual-rumble', {
        startDelay: 0,
        duration: Math.max(40, Math.min(400, ms)),
        weakMagnitude: Math.max(0, Math.min(1, strength * 0.75)),
        strongMagnitude: Math.max(0, Math.min(1, strength))
      });
    } catch (e) {
      /* a controller that cannot rumble is not an error */
    }
  }

  /* What to print on a button hint, for whichever thing they last touched. */
  function hint(action) {
    const pad = lastDevice === 'pad';
    switch (action) {
      case 'atk1':     return pad ? 'X' : 'J';
      case 'atk2':     return pad ? 'Y' : 'K';
      case 'skill1':   return pad ? 'LB' : 'U';
      case 'skill2':   return pad ? 'RB' : 'I';
      case 'interact': return pad ? 'LT' : 'E';
      case 'flask':    return pad ? 'RT' : 'Q';
      case 'confirm':  return pad ? 'A' : 'SPACE';
      default:         return '';
    }
  }

  function keyDown(name) {
    for (const code of KEYMAP[name]) if (keys[code]) return true;
    return false;
  }

  function keyPressed(name) {
    for (const code of KEYMAP[name]) if (pressed[code]) return true;
    return false;
  }

  function buttonDown(name) {
    for (const b of touch.buttons) if (b.name === name && b.down) return true;
    return false;
  }

  function buttonPressed(name) {
    for (const b of touch.buttons) if (b.name === name && b.pressedNow) return true;
    return false;
  }

  /* One object per frame describing what the player is asking for, from
   * whichever input device they happen to be using. */
  function readInput() {
    const moveDir = touch.move.dir;
    return {
      left: keyDown('left') || moveDir < 0,
      right: keyDown('right') || moveDir > 0,
      down: keyDown('down') || touch.move.down,
      jump: keyPressed('jump') || buttonPressed('jump'),
      jumpHeld: keyDown('jump') || buttonDown('jump'),
      roll: keyPressed('roll') || buttonPressed('roll'),
      atk1: keyPressed('atk1') || buttonPressed('atk1'),
      atk2: keyPressed('atk2') || buttonPressed('atk2'),
      atk1Held: keyDown('atk1') || buttonDown('atk1'),
      atk2Held: keyDown('atk2') || buttonDown('atk2'),
      skill1: keyPressed('skill1') || buttonPressed('skill1'),
      skill2: keyPressed('skill2') || buttonPressed('skill2'),
      flask: keyPressed('flask') || buttonPressed('flask'),
      interact: keyPressed('interact') || buttonPressed('interact'),
      pause: keyPressed('pause') || buttonPressed('pause')
    };
  }

  function clearPressed() {
    for (const k of Object.keys(pressed)) pressed[k] = false;
    for (const b of touch.buttons) b.pressedNow = false;
  }

  function bindInput() {
    global.addEventListener('keydown', function (e) {
      if (e.repeat) {
        keys[e.code] = true;
        return;
      }
      keys[e.code] = true;
      pressed[e.code] = true;
      lastDevice = 'key';
      if (['Space', 'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'Tab'].indexOf(e.code) !== -1) e.preventDefault();
      AUDIO.resume();
    });
    global.addEventListener('keyup', function (e) {
      keys[e.code] = false;
    });
    global.addEventListener('blur', function () {
      for (const k of Object.keys(keys)) keys[k] = false;
    });

    global.addEventListener('gamepadconnected', function (e) {
      padIndex = e.gamepad ? e.gamepad.index : padIndex;
      lastDevice = 'pad';
      AUDIO.resume();
      toastMessage('Controller connected — A jumps, X and Y swing, B rolls.');
    });
    global.addEventListener('gamepaddisconnected', function () {
      padIndex = null;
      padConnected = false;
      lastDevice = 'key';
      toastMessage('Controller disconnected.');
    });

    /* ---- touch: a move zone on the left, buttons on the right */
    canvas.addEventListener('touchstart', handleTouch, { passive: false });
    canvas.addEventListener('touchmove', handleTouch, { passive: false });
    canvas.addEventListener('touchend', handleTouch, { passive: false });
    canvas.addEventListener('touchcancel', handleTouch, { passive: false });

    /* mouse as a second pair of hands on the desktop */
    canvas.addEventListener('mousedown', function (e) {
      AUDIO.resume();
      const v = R.view();
      const x = (e.clientX / global.innerWidth) * v.w;
      const y = (e.clientY / global.innerHeight) * v.h;
      if (state !== STATE.PLAY) {
        menuClick(x, y);
        return;
      }
      if (e.button === 0) {
        pressed.KeyJ = true;
        keys.KeyJ = true;
      }
      if (e.button === 2) {
        pressed.KeyK = true;
        keys.KeyK = true;
      }
    });
    canvas.addEventListener('mouseup', function (e) {
      if (e.button === 0) keys.KeyJ = false;
      if (e.button === 2) keys.KeyK = false;
    });
    canvas.addEventListener('mouseleave', function () {
      keys.KeyJ = false;
      keys.KeyK = false;
    });
    canvas.addEventListener('contextmenu', function (e) { e.preventDefault(); });
  }

  /* A compact pad in the bottom-right corner: the two attacks and jump under
   * the thumb, the rest arced away to the left. It has to stay clear of the
   * weapon panels in the opposite corner and of the minimap up top, which is
   * why everything is measured from the corner rather than from the middle. */
  function layoutTouchButtons() {
    const v = R.view();
    const r = Math.max(13, Math.min(22, Math.round(v.h * 0.09)));
    const g = r * 2.1;
    const bx = v.w - r - 10;
    const by = v.h - r - 10;

    touch.buttons = [
      { name: 'atk1', label: 'ATK', x: bx, y: by, r: r * 1.12 },
      { name: 'jump', label: 'JMP', x: bx - g * 0.95, y: by - g * 0.32, r: r * 1.12 },
      { name: 'atk2', label: 'ATK2', x: bx - g * 0.2, y: by - g * 1.0, r: r * 0.95 },
      { name: 'roll', label: 'ROLL', x: bx - g * 1.2, y: by - g * 1.22, r: r * 0.95 },
      { name: 'skill1', label: 'S1', x: bx - g * 1.95, y: by - g * 0.35, r: r * 0.8 },
      { name: 'skill2', label: 'S2', x: bx - g * 2.1, y: by - g * 1.3, r: r * 0.8 },
      { name: 'flask', label: 'HEAL', x: bx - g * 2.8, y: by - g * 0.85, r: r * 0.8 },
      { name: 'interact', label: 'USE', x: bx - g * 2.75, y: by - g * 1.85, r: r * 0.8 },
      /* Esc and Start are not available to a thumb, so pause gets a corner. */
      { name: 'pause', label: 'II', x: v.w - r * 0.6 - 8, y: r * 0.6 + 8, r: r * 0.6 }
    ].map(function (b) {
      b.down = false;
      b.pressedNow = false;
      return b;
    });
  }

  function handleTouch(e) {
    e.preventDefault();
    AUDIO.resume();
    touch.active = true;
    lastDevice = 'touch';
    const v = R.view();
    const list = e.touches;

    const wasDown = touch.buttons.map(function (b) { return b.down; });
    for (const b of touch.buttons) b.down = false;
    touch.move.dir = 0;
    touch.move.down = false;
    let moveTouch = null;

    for (let i = 0; i < list.length; i++) {
      const t = list[i];
      const x = (t.clientX / global.innerWidth) * v.w;
      const y = (t.clientY / global.innerHeight) * v.h;

      /* The name card at the start of a biome is drawn over a live world, so the
       * thumb buttons have to work under it exactly as they do in play. */
      if (state !== STATE.PLAY && state !== STATE.INTRO) {
        /* the pause button keeps working while paused; everything else on a
         * menu screen is handled as a tap on the menu itself */
        const resume = touch.buttons.filter(function (b) { return b.name === 'pause'; })[0];
        if (state === STATE.PAUSE && resume && Math.hypot(x - resume.x, y - resume.y) <= resume.r * 1.4) {
          if (e.type === 'touchstart') resume.pressedNow = true;
          resume.down = true;
          continue;
        }
        if (e.type === 'touchstart') menuClick(x, y);
        continue;
      }

      let onButton = false;
      touch.buttons.forEach(function (b, idx) {
        if (Math.hypot(x - b.x, y - b.y) <= b.r * 1.25) {
          if (!wasDown[idx]) b.pressedNow = true;
          b.down = true;
          onButton = true;
        }
      });
      if (onButton) continue;

      if (x < v.w * 0.55 && y > v.h * 0.3) moveTouch = { x: x, y: y, id: t.identifier };
    }

    if (moveTouch) {
      if (touch.move.id !== moveTouch.id) {
        touch.move.id = moveTouch.id;
        touch.move.x0 = moveTouch.x;
        touch.move.y0 = moveTouch.y;
      }
      const dx = moveTouch.x - touch.move.x0;
      const dy = moveTouch.y - touch.move.y0;
      if (Math.abs(dx) > 5) touch.move.dir = Math.sign(dx);
      if (dy > 16) touch.move.down = true;
      touch.move.cur = moveTouch;
    } else {
      touch.move.id = null;
      touch.move.cur = null;
    }
  }

  /* =========================================================================
   *  A RUN
   * ====================================================================== */
  function weaponPool() {
    return CONTENT.WEAPONS.filter(function (w) {
      return w.tier === 0 || meta.unlocked.indexOf(w.id) !== -1;
    });
  }

  function skillPool() {
    return CONTENT.SKILLS.filter(function (s) {
      return s.tier === 0 || meta.unlocked.indexOf(s.id) !== -1;
    });
  }

  function startRun() {
    meta = META.load();
    const seed = (Date.now() ^ Math.floor(Math.random() * 0xfffff)) >>> 0;
    rng = RNG.Rng(seed);

    run = {
      seed: seed,
      biomeIndex: 0,
      stats: { brutality: 1, tactics: 1, survival: 1 },
      mutations: [],
      weapons: [CB.rollWeapon(rng, CONTENT.WEAPON.rusty_sword), null],
      skills: [null, null],
      flasks: 2,
      maxFlasks: 2,
      gold: 0,
      cells: 0,
      keys: 0,
      hp: null,
      time: 0,
      kills: 0,
      bossCells: meta.bossCells || 0,
      spent: 0
    };
    AUDIO.resume();
    enterBiome(0);
  }

  function enterBiome(index) {
    run.biomeIndex = index;
    const biome = CONTENT.BIOMES[index];
    const level = LG.generate(biome.id, (run.seed + index * 1013) >>> 0, { bossCells: run.bossCells });

    world = {
      level: level,
      player: null,
      enemies: [],
      boss: null,
      projectiles: [],
      drops: [],
      particles: [],
      rings: [],
      slashes: [],
      texts: [],
      turrets: [],
      traps: [],
      orbs: [],
      time: 0,
      runTime: 0,
      shake: 0,
      freeze: 0,
      kills: 0,
      bossCells: run.bossCells,
      events: []
    };

    world.player = EN.makePlayer(level, {
      stats: run.stats,
      mutations: run.mutations,
      weapons: run.weapons,
      skills: run.skills,
      flasks: run.flasks,
      gold: run.gold,
      cells: run.cells,
      keys: run.keys,
      /* never carry zero health into a new biome: a run that is still going is
       * a run with at least one point of health left */
      hp: run.hp != null ? Math.max(1, run.hp) : null
    });

    for (const spec of level.enemies) world.enemies.push(EN.makeEnemy(spec));
    if (level.boss) {
      world.boss = EN.makeBoss(world, level);
      AUDIO.play('roar');
    }

    visited = new Set();
    R.centreCamera(world);
    AUDIO.startMusic(level.boss ? 5 : index);
    AUDIO.setIntensity(level.boss ? 1 : 0.2);
    introT = 2.2;
    state = STATE.INTRO;
  }

  /* Carry everything that belongs to the player, not the level, forward. */
  function syncRunFromWorld() {
    const p = world.player;
    run.hp = p.hp;
    run.gold = p.gold;
    run.cells = p.cells;
    run.keys = p.keys;
    run.flasks = p.flasks;
    run.weapons = p.weapons;
    run.skills = p.skills;
    run.kills = (run.kills || 0) + 0;
  }

  function nextBiome() {
    syncRunFromWorld();
    run.kills += world.kills || 0;
    run.time += world.runTime;

    if (run.biomeIndex + 1 >= CONTENT.BIOMES.length) {
      winRun();
      return;
    }

    /* a breather between levels: flasks refilled, a little health back */
    run.flasks = run.maxFlasks;
    const heal = Math.round(CB.maxHealth(run.stats, run.mutations) * 0.25);
    run.hp = Math.min(CB.maxHealth(run.stats, run.mutations), (run.hp || 0) + heal);

    offerMutation();
  }

  function offerMutation() {
    const taken = run.mutations.map(function (m) { return m.id; });
    const pool = CONTENT.MUTATIONS.filter(function (m) { return taken.indexOf(m.id) === -1; });
    if (!pool.length) {
      enterBiome(run.biomeIndex + 1);
      return;
    }
    choices = rng.sample(pool, Math.min(3, pool.length));
    menuIndex = 0;
    state = STATE.MUTATE;
    AUDIO.play('levelup');
  }

  function takeMutation(mutation) {
    run.mutations.push(mutation);
    /* mutations that change the body take effect at once */
    const maxHp = CB.maxHealth(run.stats, run.mutations);
    run.hp = Math.min(maxHp, (run.hp || maxHp) + (mutation.id === 'soldier' ? 10 : 0));
    enterBiome(run.biomeIndex + 1);
  }

  function winRun() {
    syncRunFromWorld();
    const banked = META.bankCells(run.cells);
    meta = META.recordRun({
      won: true,
      kills: run.kills,
      depth: CONTENT.BIOMES.length,
      biome: CONTENT.BIOMES[CONTENT.BIOMES.length - 1].name,
      time: run.time,
      bossCells: run.bossCells
    });
    run.banked = banked;
    AUDIO.stopMusic();
    AUDIO.play('win');
    state = STATE.VICTORY;
  }

  function endRun() {
    syncRunFromWorld();
    run.kills += world.kills || 0;
    run.time += world.runTime;
    /* Half of what you are carrying reaches the Collector; the rest is lost
     * with you. Spending cells mid-run at a Collector is how you keep it all. */
    run.banked = Math.floor(run.cells / 2);
    META.bankCells(run.banked);
    meta = META.recordRun({
      won: false,
      kills: run.kills,
      depth: run.biomeIndex + 1,
      biome: CONTENT.BIOMES[run.biomeIndex].name,
      time: run.time
    });
    AUDIO.stopMusic();
    state = STATE.DEAD;
  }

  /* =========================================================================
   *  LOOT
   * ====================================================================== */
  function rollWeaponDrop() {
    return CB.rollWeapon(rng, rng.pick(weaponPool()));
  }

  function rollSkillDrop() {
    return rng.pick(skillPool());
  }

  function giveWeapon(weapon) {
    const p = world.player;
    const slot = p.weapons[1] ? (p.weapons[0] && p.weapons[1] ? pickWorstSlot(weapon) : 1) : 1;
    const dropped = p.weapons[slot];
    p.weapons[slot] = weapon;
    EN.floatText(world, p.x, p.y - 34, weapon.name.toUpperCase(), CONTENT.COLORS[weapon.color].hex);
    if (dropped) toastMessage('Swapped out ' + dropped.name);
    AUDIO.play('pickup');
  }

  /* Replace whichever hand is worth less to this run, measured in the damage
   * it would actually do with the scrolls you have. */
  function pickWorstSlot(candidate) {
    const p = world.player;
    const value = function (w) {
      if (!w) return -1;
      return (w.dmg / Math.max(0.1, w.rate)) * CB.scaleFor(w.color, p.stats);
    };
    const a = value(p.weapons[0]);
    const b = value(p.weapons[1]);
    const c = value(candidate);
    if (c < Math.min(a, b)) return a < b ? 0 : 1;   // still swap the weakest
    return a < b ? 0 : 1;
  }

  function giveSkill(skill) {
    const p = world.player;
    const slot = p.skills[0] ? (p.skills[1] ? rng.int(0, 1) : 1) : 0;
    p.skills[slot] = skill;
    p.skillCd[slot] = 0;
    EN.floatText(world, p.x, p.y - 34, skill.name.toUpperCase(), CONTENT.COLORS[skill.color].hex);
    AUDIO.play('pickup');
  }

  function toastMessage(msg) {
    toast = { msg: msg, life: 2.6 };
  }

  /* =========================================================================
   *  INTERACTION
   * ====================================================================== */
  function findNearby() {
    const p = world.player;
    const level = world.level;
    let best = null;
    let bestD = 30;

    if (level.exit) {
      const d = Math.hypot(level.exit.x - p.x, (level.exit.y - 16) - (p.y - 10));
      if (d < bestD) {
        best = { type: 'exit', pos: { x: level.exit.x, y: level.exit.y }, locked: level.exit.locked };
        bestD = d;
      }
    }
    for (const o of level.objects) {
      if (o.taken || o.opened) continue;
      if (o.type === 'collector' && o.used) continue;
      const d = Math.hypot(o.pos.x - p.x, o.pos.y - p.y);
      if (d < bestD) {
        best = o;
        bestD = d;
      }
    }
    return best;
  }

  function interact() {
    const o = nearby;
    if (!o) return;
    const p = world.player;

    switch (o.type) {
      case 'exit':
        if (o.locked) {
          AUDIO.play('locked');
          toastMessage('The way out is sealed until the boss falls.');
          return;
        }
        AUDIO.play('door');
        nextBiome();
        return;

      case 'chest':
      case 'timed_chest': {
        if (o.type === 'timed_chest' && world.runTime > (world.level.timeLimit || 0)) {
          AUDIO.play('locked');
          toastMessage('Too slow — this one is locked for good.');
          return;
        }
        o.opened = true;
        AUDIO.play('door');
        const roll = rng.next();
        if (roll < 0.45) giveWeapon(rollWeaponDrop());
        else if (roll < 0.7) giveSkill(rollSkillDrop());
        else if (roll < 0.9) {
          const gold = rng.int(20, 60) + 10 * world.level.depth;
          p.gold += gold;
          EN.floatText(world, p.x, p.y - 30, '+' + gold + ' GOLD', '#ffd34a');
        } else {
          p.flasks = Math.min(run.maxFlasks, p.flasks + 1);
          EN.floatText(world, p.x, p.y - 30, '+1 FLASK', '#2fe6c8');
        }
        if (o.type === 'timed_chest') {
          const bonus = rng.int(40, 90);
          p.gold += bonus;
          EN.floatText(world, p.x, p.y - 42, 'QUICK! +' + bonus + ' GOLD', '#ffe600');
        }
        return;
      }

      case 'cursed_chest':
        o.opened = true;
        p.cursed = true;
        p.curseKills = 0;
        giveWeapon(rollWeaponDrop());
        toastMessage('CURSED — one hit kills you until you have killed 10.');
        AUDIO.play('locked');
        return;

      case 'vault':
        if (o.locked) {
          if (p.keys <= 0) {
            AUDIO.play('locked');
            toastMessage('Locked. An elite somewhere in here is carrying the key.');
            return;
          }
          p.keys--;
          o.locked = false;
        }
        o.opened = true;
        AUDIO.play('door');
        giveWeapon(rollWeaponDrop());
        giveSkill(rollSkillDrop());
        p.gold += rng.int(60, 140);
        return;

      case 'scroll':
        o.taken = true;
        choices = CONTENT.COLOR_IDS.map(function (id) { return CONTENT.COLORS[id]; });
        menuIndex = 0;
        state = STATE.SCROLL;
        world.pendingScroll = o.dual;
        AUDIO.play('scroll');
        return;

      case 'food':
        o.taken = true;
        EN.healPlayer(world, CB.healAmount(p, Math.round(p.maxHp * 0.3)));
        AUDIO.play('pickup');
        return;

      case 'shop':
        if (!o.stock) o.stock = makeShopStock();
        shopStock = o.stock;
        menuIndex = 0;
        state = STATE.SHOP;
        return;

      case 'collector':
        menuIndex = 0;
        state = STATE.COLLECTOR;
        return;
    }
  }

  function takeScroll(colorId) {
    const p = world.player;
    run.stats[colorId] = (run.stats[colorId] || 1) + 1;
    if (world.pendingScroll) {
      const others = CONTENT.COLOR_IDS.filter(function (c) { return c !== colorId; });
      const extra = rng.pick(others);
      run.stats[extra] = (run.stats[extra] || 1) + 1;
      toastMessage('+1 ' + CONTENT.COLORS[colorId].name + ', +1 ' + CONTENT.COLORS[extra].name);
    }
    p.stats = run.stats;
    const before = p.maxHp;
    p.maxHp = CB.maxHealth(run.stats, run.mutations);
    p.hp += Math.max(0, p.maxHp - before);
    EN.floatText(world, p.x, p.y - 36, '+1 ' + CONTENT.COLORS[colorId].short, CONTENT.COLORS[colorId].hex);
    AUDIO.play('levelup');
    world.pendingScroll = false;
    state = STATE.PLAY;
  }

  function makeShopStock() {
    const depth = world.level.depth;
    const stock = [];
    for (let i = 0; i < 2; i++) {
      const w = rollWeaponDrop();
      stock.push({ kind: 'weapon', item: w, name: w.name, color: w.color, price: 45 + 12 * depth + rng.int(0, 30), desc: w.desc });
    }
    const s = rollSkillDrop();
    stock.push({ kind: 'skill', item: s, name: s.name, color: s.color, price: 55 + 12 * depth, desc: s.desc });
    stock.push({ kind: 'flask', name: 'Extra Flask Charge', color: 'survival', price: 70, desc: 'One more drink from the flask, for the rest of the run.' });
    stock.push({ kind: 'heal', name: 'Full Meal', color: 'survival', price: 40, desc: 'Heals you to full, right now.' });
    return stock;
  }

  function buy(entry) {
    const p = world.player;
    if (entry.sold) return;
    if (p.gold < entry.price) {
      AUDIO.play('locked');
      toastMessage('Not enough gold.');
      return;
    }
    p.gold -= entry.price;
    entry.sold = true;
    AUDIO.play('buy');

    if (entry.kind === 'weapon') giveWeapon(entry.item);
    else if (entry.kind === 'skill') giveSkill(entry.item);
    else if (entry.kind === 'flask') {
      run.maxFlasks++;
      p.flasks++;
    } else if (entry.kind === 'heal') EN.healPlayer(world, p.maxHp);
  }

  function buyUnlock(entry) {
    const p = world.player;
    if (meta.unlocked.indexOf(entry.id) !== -1) return;
    if (p.cells < entry.cost) {
      AUDIO.play('locked');
      toastMessage('Not enough cells.');
      return;
    }
    /* spend from the run, and record it as permanently unlocked */
    p.cells -= entry.cost;
    run.spent += entry.cost;
    META.bankCells(entry.cost);
    META.unlock(entry.id, entry.cost);
    meta = META.load();
    AUDIO.play('levelup');
    toastMessage(entry.name + ' unlocked — it can drop in every run from now on.');
  }

  /* =========================================================================
   *  THE LOOP
   * ====================================================================== */
  function frame(now) {
    global.requestAnimationFrame(frame);
    const dt = Math.min(0.05, (now - lastTime) / 1000 || 0);
    lastTime = now;

    update(dt);
    draw();

    /* Only throw away a button press once something has actually read it. A
     * frame can advance no world steps at all — hit-stop holds the simulation
     * still, and on a 120Hz display most frames have no step due yet — and a
     * press dropped on such a frame is an attack the player never got. The
     * time limit stops one sticking around long enough to fire twice. */
    const consumed = (state !== STATE.PLAY && state !== STATE.INTRO) || stepsRan > 0;
    if (consumed) {
      clearPressed();
      heldPressT = 0;
    } else {
      heldPressT += dt;
      if (heldPressT > 0.25) {
        clearPressed();
        heldPressT = 0;
      }
    }
  }

  function update(dt) {
    pollPad();
    const input = readInput();

    if (keyPressed('mute')) AUDIO.toggleMute();

    switch (state) {
      case STATE.TITLE:
        titleT += dt;
        menuNav();
        break;

      case STATE.INTRO:
        introT -= dt;
        stepWorld(dt, { left: false, right: false });
        if (introT <= 0 || input.interact || input.jump) state = STATE.PLAY;
        break;

      case STATE.PLAY: {
        if (input.pause) {
          state = STATE.PAUSE;
          menuIndex = 0;
          break;
        }
        if (input.interact && nearby) {
          interact();
          if (state !== STATE.PLAY) break;   // a shop or a scroll took over
        }
        stepWorld(dt, input);
        break;
      }

      case STATE.PAUSE:
        if (input.pause) state = STATE.PLAY;
        else menuNav();
        break;

      case STATE.SCROLL:
      case STATE.MUTATE:
        menuNav();
        break;

      case STATE.SHOP:
      case STATE.COLLECTOR:
        if (input.pause || pressed.PadB) state = STATE.PLAY;
        else menuNav();
        break;

      case STATE.DEAD:
      case STATE.VICTORY:
        if (input.interact || input.jump || input.atk1) state = STATE.TITLE;
        break;

      case STATE.HELP:
        if (input.interact || input.jump || keyPressed('pause')) state = STATE.TITLE;
        break;
    }

    if (toast) {
      toast.life -= dt;
      if (toast.life <= 0) toast = null;
    }
  }

  /* One fixed-timestep slice of the world. */
  function stepWorld(dt, input) {
    acc += dt;
    let steps = 0;
    stepsRan = 0;
    while (acc >= STEP && steps < 5) {
      acc -= STEP;
      steps++;

      /* hit-stop: the world holds still for a few frames on a solid hit */
      if (world.freeze > 0) {
        world.freeze -= STEP;
        continue;
      }
      stepsRan++;

      world.time += STEP;
      world.runTime += STEP;

      EN.updatePlayer(world, input, STEP);
      EN.updateEnemies(world, STEP);
      EN.updateProjectiles(world, STEP);
      EN.updateDrops(world, STEP);
      EN.updateEffects(world, STEP);

      /* Anything that jolts the camera jolts the controller too. Reading it off
       * the shake means every source — a hit taken, a crit landed, a boss slam
       * — is covered without the entity code knowing about controllers. */
      if (world.shake > lastShake + 1.5) {
        rumble(Math.max(0.2, Math.min(1, world.shake / 11)), 70 + world.shake * 7);
      }
      lastShake = world.shake;

      world.shake = Math.max(0, world.shake - STEP * 22);
      handleEvents();
      trackRoom();
      if (state !== STATE.PLAY && state !== STATE.INTRO) break;
    }

    R.updateCamera(world, dt);
    nearby = world.player.dead ? null : findNearby();

    /* the music leans in when you are in danger */
    const p = world.player;
    const hurtness = 1 - p.hp / p.maxHp;
    const near = EN.nearestEnemy(world, p.x, p.y - 10, 180) ? 0.5 : 0;
    AUDIO.setIntensity(Math.max(near, hurtness * 0.9, world.boss && !world.boss.dead ? 0.8 : 0));
  }

  function handleEvents() {
    while (world.events.length) {
      const ev = world.events.shift();
      if (ev.type === 'player_died') {
        endRun();
        return;
      }
      if (ev.type === 'boss_killed') {
        world.level.exit.locked = false;
        toastMessage('The way on is open.');
      }
    }
  }

  function trackRoom() {
    const p = world.player;
    for (const room of world.level.rooms) {
      const x0 = room.x0 * LG.TILE;
      const x1 = room.x1 * LG.TILE;
      const y0 = room.y0 * LG.TILE;
      const y1 = (room.floorY + 1) * LG.TILE;
      if (p.x >= x0 && p.x <= x1 && p.y >= y0 && p.y <= y1) visited.add(room);
    }
  }

  /* =========================================================================
   *  MENUS
   *
   *  One list of items per menu state, one place that acts on a choice, and a
   *  set of rectangles recorded while drawing so a tap lands on the same thing
   *  the keyboard would have chosen.
   * ====================================================================== */
  let menuRects = [];

  function menuItems() {
    switch (state) {
      case STATE.SCROLL:
      case STATE.MUTATE:
        return (choices || []).map(function (c) { return c.name; });
      case STATE.PAUSE:
        return ['Resume', 'Restart run', AUDIO.isMuted() ? 'Unmute' : 'Mute', 'Quit to title'];
      case STATE.SHOP:
        return (shopStock || []).map(function (s) { return s.name; }).concat(['Leave']);
      case STATE.COLLECTOR:
        return CONTENT.unlockables().map(function (u) { return u.name; }).concat(['Leave']);
      case STATE.TITLE:
        return ['New run', 'Controls'];
      default:
        return [];
    }
  }

  function activateMenu(i) {
    switch (state) {
      case STATE.TITLE:
        if (i === 0) startRun();
        else state = STATE.HELP;
        return;

      case STATE.PAUSE:
        if (i === 0) state = STATE.PLAY;
        else if (i === 1) startRun();
        else if (i === 2) AUDIO.toggleMute();
        else {
          AUDIO.stopMusic();
          state = STATE.TITLE;
        }
        return;

      case STATE.SCROLL:
        takeScroll(choices[i].id);
        return;

      case STATE.MUTATE:
        takeMutation(choices[i]);
        return;

      case STATE.SHOP:
        if (i >= shopStock.length) state = STATE.PLAY;
        else buy(shopStock[i]);
        return;

      case STATE.COLLECTOR: {
        const list = CONTENT.unlockables();
        if (i >= list.length) state = STATE.PLAY;
        else buyUnlock(list[i]);
        return;
      }

      case STATE.DEAD:
      case STATE.VICTORY:
      case STATE.HELP:
        state = STATE.TITLE;
        return;
    }
  }

  function menuNav() {
    const items = menuItems();
    if (!items.length) return;
    if (keyPressed('left') || keyPressed('up')) {
      menuIndex = (menuIndex + items.length - 1) % items.length;
      AUDIO.play('swing');
    }
    if (keyPressed('right') || keyPressed('down')) {
      menuIndex = (menuIndex + 1) % items.length;
      AUDIO.play('swing');
    }
    for (let i = 1; i <= Math.min(9, items.length); i++) {
      if (pressed['Digit' + i]) {
        menuIndex = i - 1;
        activateMenu(i - 1);
        return;
      }
    }
    if (keyPressed('jump') || keyPressed('atk1') || pressed.Enter || keyPressed('interact')) {
      activateMenu(menuIndex);
    }
  }

  function menuClick(x, y) {
    for (const r of menuRects) {
      if (x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h) {
        menuIndex = r.index;
        activateMenu(r.index);
        return;
      }
    }
    if (state === STATE.TITLE) startRun();
    else if (state === STATE.DEAD || state === STATE.VICTORY || state === STATE.HELP) state = STATE.TITLE;
  }

  /* =========================================================================
   *  DRAWING
   * ====================================================================== */
  function draw() {
    const ctx = R.context();
    const v = R.view();
    menuRects = [];

    /* Defend the screens against being entered without their data: a menu with
     * nothing to offer is a bug, but it must never be a crash mid-frame. */
    if ((state === STATE.SCROLL || state === STATE.MUTATE) && (!choices || !choices.length)) state = STATE.PLAY;
    if (state === STATE.SHOP && (!shopStock || !shopStock.length)) state = STATE.PLAY;
    if ((state !== STATE.TITLE && state !== STATE.HELP && state !== STATE.VICTORY) && !world) state = STATE.TITLE;

    switch (state) {
      case STATE.TITLE:
        drawTitle(ctx, v);
        return;

      case STATE.HELP:
        drawTitleBackdrop(ctx, v);
        drawHelp(ctx, v);
        return;

      case STATE.INTRO:
        R.drawWorld(world);
        drawHud(ctx, v);
        drawIntroCard(ctx, v);
        return;

      case STATE.PLAY:
        R.drawWorld(world);
        drawHud(ctx, v);
        drawPrompt(ctx, v);
        if (touch.active) drawTouchControls(ctx);
        return;

      case STATE.PAUSE:
        R.drawWorld(world);
        drawHud(ctx, v);
        dim(ctx, v, 0.72);
        drawList(ctx, v, 'PAUSED', menuItems(), null);
        if (touch.active) drawTouchControls(ctx, ['pause']);
        return;

      case STATE.SCROLL:
        R.drawWorld(world);
        drawHud(ctx, v);
        dim(ctx, v, 0.74);
        drawScrollChoice(ctx, v);
        return;

      case STATE.MUTATE:
        if (world) R.drawWorld(world);
        dim(ctx, v, 0.82);
        drawMutationChoice(ctx, v);
        return;

      case STATE.SHOP:
        R.drawWorld(world);
        drawHud(ctx, v);
        dim(ctx, v, 0.78);
        drawShop(ctx, v);
        return;

      case STATE.COLLECTOR:
        R.drawWorld(world);
        drawHud(ctx, v);
        dim(ctx, v, 0.82);
        drawCollector(ctx, v);
        return;

      case STATE.DEAD:
        R.drawWorld(world);
        dim(ctx, v, 0.76);
        drawDeath(ctx, v);
        return;

      case STATE.VICTORY:
        drawTitleBackdrop(ctx, v);
        drawVictory(ctx, v);
        return;
    }
  }

  function dim(ctx, v, alpha) {
    ctx.fillStyle = 'rgba(4,3,8,' + alpha + ')';
    ctx.fillRect(0, 0, v.w, v.h);
  }

  /* ---------------------------------------------------------------- the HUD */
  function drawHud(ctx, v) {
    const p = world.player;
    const level = world.level;

    /* Everything down the left starts below HUD_TOP: the shared MAZ nav pill
     * floats over the top-left corner, and the health bar must not end up
     * underneath it. */
    const HUD_TOP = 26;

    /* health */
    const barW = Math.min(138, Math.round(v.w * 0.32));
    R.rect(8, HUD_TOP, barW, 10, 'rgba(0,0,0,.66)');
    const frac = Math.max(0, p.hp / p.maxHp);
    const g = ctx.createLinearGradient(8, 0, 8 + barW, 0);
    g.addColorStop(0, '#ff3b5c');
    g.addColorStop(1, '#ff8a5c');
    ctx.fillStyle = g;
    ctx.fillRect(9, HUD_TOP + 1, Math.round((barW - 2) * frac), 8);
    ctx.strokeStyle = 'rgba(255,255,255,.3)';
    ctx.strokeRect(8.5, HUD_TOP + 0.5, barW - 1, 9);
    for (let hp = 40; hp < p.maxHp; hp += 40) {
      const x = 9 + (barW - 2) * (hp / p.maxHp);
      R.rect(x, HUD_TOP + 1, 1, 8, 'rgba(0,0,0,.45)');
    }
    R.text(Math.ceil(p.hp) + ' / ' + p.maxHp, 8 + barW / 2, HUD_TOP + 8, { align: 'center', size: 7 });

    /* flasks */
    const row2 = HUD_TOP + 13;
    for (let i = 0; i < run.maxFlasks; i++) {
      const x = 8 + i * 9;
      R.rect(x, row2, 7, 9, i < p.flasks ? '#2fe6c8' : 'rgba(47,230,200,.2)');
      R.rect(x + 2, row2 - 2, 3, 2, i < p.flasks ? '#9ffff0' : 'rgba(47,230,200,.2)');
    }

    /* the three colours */
    let sx = 8 + run.maxFlasks * 9 + 6;
    for (const id of CONTENT.COLOR_IDS) {
      const c = CONTENT.COLORS[id];
      R.rect(sx, row2, 16, 9, 'rgba(0,0,0,.5)');
      R.rect(sx, row2, 2, 9, c.hex);
      R.text(String(run.stats[id]), sx + 10, row2 + 7, { align: 'center', size: 7, color: c.hex });
      sx += 18;
    }

    /* statuses on the player */
    let stx = sx + 4;
    for (const id of Object.keys(p.status || {})) {
      const s = p.status[id];
      if (!s || s.t <= 0) continue;
      const def = CONTENT.STATUSES[id];
      R.rect(stx, row2, 8, 9, def.hex);
      stx += 10;
    }

    /* cells, gold, keys — shifted in to clear the touch pause button */
    const right = v.w - 8 - (touch.active ? 24 : 0);
    R.text(String(p.cells), right, 16, { align: 'right', size: 9, color: '#9ffff0', bold: true });
    R.text('CELLS', right - R.measure(String(p.cells), 9, true) - 4, 16, { align: 'right', size: 7, color: 'rgba(159,255,240,.6)' });
    R.text(String(p.gold), right, 27, { align: 'right', size: 9, color: '#ffd34a', bold: true });
    R.text('GOLD', right - R.measure(String(p.gold), 9, true) - 4, 27, { align: 'right', size: 7, color: 'rgba(255,211,74,.6)' });
    if (p.keys > 0) R.text('KEY x' + p.keys, right, 38, { align: 'right', size: 7, color: '#ffe600' });

    /* where you are, and the clock the timed vault cares about */
    const biome = level.biome;
    R.text(biome.name.toUpperCase(), v.w / 2, 14, { align: 'center', size: 8, color: '#fff', bold: true });
    const left = Math.max(0, (level.timeLimit || 0) - world.runTime);
    R.text(
      (level.isArena ? '' : left > 0 ? 'VAULT ' + Math.ceil(left) + 's  ·  ' : 'VAULT CLOSED  ·  ') + clock(world.runTime),
      v.w / 2, 24,
      { align: 'center', size: 7, color: left > 0 ? 'rgba(255,230,0,.8)' : 'rgba(255,255,255,.45)' }
    );

    if (p.cursed) {
      R.text('CURSED — ' + (10 - p.curseKills) + ' KILLS TO LIFT', v.w / 2, 36, { align: 'center', size: 7, color: '#d06bff', bold: true });
    }

    /* Weapons and skills are drawn slightly see-through: at this zoom the
     * player can be standing right behind them. */
    ctx.save();
    ctx.globalAlpha = 0.82;

    /* weapons, bottom left */
    for (let i = 0; i < 2; i++) {
      const w = p.weapons[i];
      const x = 8;
      const y = v.h - 40 + i * 18;
      R.panel(x, y, 112, 16, w ? CONTENT.COLORS[w.color].hex : 'rgba(255,255,255,.12)');
      R.text(hint(i === 0 ? 'atk1' : 'atk2'), x + 6, y + 11, { size: 8, color: '#fff', bold: true });
      if (w) {
        R.rect(x + 14, y + 2, 2, 12, CONTENT.COLORS[w.color].hex);
        R.text(trim(w.name, 15), x + 19, y + 11, { size: 7, color: '#e8e2f2' });
      } else {
        R.text('empty hand', x + 19, y + 11, { size: 7, color: 'rgba(255,255,255,.35)' });
      }
    }

    /* Skills sit bottom right on a keyboard, but that corner belongs to the
     * thumb buttons on a touch screen, so they move up under the health bar. */
    for (let i = 0; i < 2; i++) {
      const s = p.skills[i];
      const w = 86;
      const x = touch.active ? 8 : v.w - w - 8;
      const y = touch.active ? HUD_TOP + 26 + i * 18 : v.h - 40 + i * 18;
      R.panel(x, y, w, 16, s ? CONTENT.COLORS[s.color].hex : 'rgba(255,255,255,.12)');
      R.text(hint(i === 0 ? 'skill1' : 'skill2'), x + 6, y + 11, { size: 8, color: '#fff', bold: true });
      if (s) {
        R.text(trim(s.name, 11), x + 16, y + 11, { size: 7, color: '#e8e2f2' });
        if (p.skillCd[i] > 0) {
          ctx.fillStyle = 'rgba(0,0,0,.6)';
          ctx.fillRect(x + 1, y + 1, (w - 2) * (p.skillCd[i] / s.cd), 14);
          R.text(Math.ceil(p.skillCd[i]) + 's', x + w - 6, y + 11, { align: 'right', size: 7, color: '#fff' });
        }
      } else {
        R.text('no skill', x + 16, y + 11, { size: 7, color: 'rgba(255,255,255,.35)' });
      }
    }

    ctx.restore();

    drawMinimap(ctx, v);

    /* the boss's health, across the bottom */
    if (world.boss && !world.boss.dead) {
      const bw = Math.min(210, v.w - 120);
      const bx = (v.w - bw) / 2;
      const by = 52;
      R.rect(bx, by, bw, 8, 'rgba(0,0,0,.7)');
      R.rect(bx + 1, by + 1, (bw - 2) * Math.max(0, world.boss.hp / world.boss.maxHp), 6, world.boss.def.hex);
      ctx.strokeStyle = 'rgba(255,255,255,.35)';
      ctx.strokeRect(bx + 0.5, by + 0.5, bw - 1, 7);
      R.text(world.boss.def.intro + (world.boss.phase === 2 ? '  ·  ENRAGED' : ''), v.w / 2, by + 16, {
        align: 'center', size: 7, color: world.boss.phase === 2 ? '#ff3b5c' : '#fff', bold: true
      });
    }

    if (toast) {
      const a = Math.min(1, toast.life);
      ctx.save();
      ctx.globalAlpha = a;
      const tw = R.measure(toast.msg, 7) + 14;
      R.panel((v.w - tw) / 2, v.h - 74, tw, 14, 'rgba(255,255,255,.25)');
      R.text(toast.msg, v.w / 2, v.h - 64, { align: 'center', size: 7, color: '#fff' });
      ctx.restore();
    }

    if (AUDIO.isMuted()) R.text('MUTED (M)', v.w - 8, v.h - 46, { align: 'right', size: 7, color: 'rgba(255,255,255,.4)' });
  }

  /* A small plan of the level: rooms you have been in, and the way out. */
  function drawMinimap(ctx, v) {
    const level = world.level;
    if (!level.rooms || level.isArena) return;
    const cell = 7;
    let cols = 0;
    let rows = 0;
    for (const room of level.rooms) {
      cols = Math.max(cols, room.cx + 1);
      rows = Math.max(rows, room.cy + 1);
    }
    const w = cols * cell + 4;
    const h = rows * cell + 4;
    const x = v.w - w - 8;
    const y = 44;
    R.panel(x, y, w, h, 'rgba(255,255,255,.15)');

    const p = world.player;
    for (const room of level.rooms) {
      const rx = x + 2 + room.cx * cell;
      const ry = y + 2 + room.cy * cell;
      const seen = visited.has(room);
      const here = p.x >= room.x0 * LG.TILE && p.x <= room.x1 * LG.TILE &&
                   p.y >= room.y0 * LG.TILE && p.y <= (room.floorY + 1) * LG.TILE;
      let color = 'rgba(255,255,255,.10)';
      if (seen) color = 'rgba(255,255,255,.38)';
      if (room === level.exitRoom && seen) color = level.biome.palette.moss;
      if (here) color = '#fff';
      R.rect(rx, ry, cell - 2, cell - 2, color);
    }
  }

  function clock(t) {
    const m = Math.floor(t / 60);
    const s = Math.floor(t % 60);
    return m + ':' + (s < 10 ? '0' : '') + s;
  }

  function trim(str, n) {
    return str.length > n ? str.slice(0, n - 1) + '.' : str;
  }

  /* What pressing E would do right now, floating over the thing it would do it to. */
  function drawPrompt(ctx, v) {
    if (!nearby) return;
    const label = {
      exit: nearby.locked ? 'SEALED' : 'ENTER',
      chest: 'OPEN',
      timed_chest: 'OPEN — QUICK',
      cursed_chest: 'OPEN (CURSED)',
      vault: 'UNLOCK',
      scroll: 'READ SCROLL',
      food: 'EAT',
      shop: 'TRADE',
      collector: 'SPEND CELLS'
    }[nearby.type] || 'USE';

    const x = nearby.pos.x - R.ox();
    const y = nearby.pos.y - R.oy() - 46;
    if (x < 0 || x > v.w) return;
    const key = hint('interact');
    const w = R.measure(label, 7) + 18 + R.measure(key, 8, true);
    R.panel(x - w / 2, y, w, 13, 'rgba(255,255,255,.3)');
    R.text(key, x - w / 2 + 5, y + 9, { size: 8, bold: true, color: '#ffe600' });
    R.text(label, x + 4, y + 9, { align: 'center', size: 7, color: '#fff' });
  }

  function drawTouchControls(ctx, only) {
    ctx.save();
    for (const b of touch.buttons) {
      if (only && only.indexOf(b.name) === -1) continue;
      ctx.globalAlpha = b.down ? 0.5 : 0.22;
      ctx.fillStyle = '#ffffff';
      ctx.beginPath();
      ctx.arc(b.x, b.y, b.r, 0, Math.PI * 2);
      ctx.fill();
      ctx.globalAlpha = b.down ? 1 : 0.75;
      R.text(b.label, b.x, b.y + 3, { align: 'center', size: 7, color: '#0b0714', bold: true, shadow: false });
    }
    if (touch.move.cur) {
      ctx.globalAlpha = 0.25;
      ctx.fillStyle = '#fff';
      ctx.beginPath();
      ctx.arc(touch.move.x0, touch.move.y0, 22, 0, Math.PI * 2);
      ctx.fill();
      ctx.globalAlpha = 0.6;
      ctx.beginPath();
      ctx.arc(touch.move.cur.x, touch.move.cur.y, 10, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.restore();
  }

  /* ------------------------------------------------------------ name cards */
  function drawIntroCard(ctx, v) {
    const a = Math.min(1, introT / 0.5);
    const biome = world.level.biome;
    ctx.save();
    ctx.globalAlpha = a;
    ctx.fillStyle = 'rgba(4,3,8,.55)';
    ctx.fillRect(0, v.h / 2 - 34, v.w, 68);
    R.text(biome.name.toUpperCase(), v.w / 2, v.h / 2 - 6, { align: 'center', size: 16, bold: true, color: '#fff' });
    R.text(biome.tag, v.w / 2, v.h / 2 + 8, { align: 'center', size: 8, color: 'rgba(255,255,255,.7)' });
    R.text('BIOME ' + (run.biomeIndex + 1) + ' / ' + CONTENT.BIOMES.length, v.w / 2, v.h / 2 + 22, {
      align: 'center', size: 7, color: 'rgba(255,255,255,.45)'
    });
    ctx.restore();
  }

  /* --------------------------------------------------------------- choices */
  function cardRow(ctx, v, title, subtitle, items, render) {
    R.text(title, v.w / 2, Math.max(34, v.h * 0.2), { align: 'center', size: 14, bold: true, color: '#fff' });
    if (subtitle) {
      R.text(subtitle, v.w / 2, Math.max(48, v.h * 0.2 + 14), { align: 'center', size: 7, color: 'rgba(255,255,255,.6)' });
    }

    const n = items.length;
    const cw = Math.min(128, Math.floor((v.w - 24) / n) - 6);
    const ch = Math.min(104, Math.round(v.h * 0.42));
    const total = n * cw + (n - 1) * 8;
    const x0 = (v.w - total) / 2;
    const y = v.h / 2 - ch / 4;

    for (let i = 0; i < n; i++) {
      const x = x0 + i * (cw + 8);
      const chosen = i === menuIndex;
      const accent = render(items[i], 'accent');
      R.panel(x, y, cw, ch, chosen ? accent : 'rgba(255,255,255,.16)');
      if (chosen) {
        ctx.save();
        ctx.globalAlpha = 0.1;
        ctx.fillStyle = accent;
        ctx.fillRect(x + 1, y + 1, cw - 2, ch - 2);
        ctx.restore();
      }
      R.text(String(i + 1), x + 6, y + 12, { size: 8, color: accent, bold: true });
      render(items[i], 'body', x, y, cw, ch, accent);
      menuRects.push({ x: x, y: y, w: cw, h: ch, index: i });
    }

    R.text('← → choose   ·   ' + hint('confirm') + ' or tap to take', v.w / 2, y + ch + 16, {
      align: 'center', size: 7, color: 'rgba(255,255,255,.55)'
    });
  }

  function wrapText(str, width, size) {
    const words = String(str).split(' ');
    const lines = [];
    let line = '';
    for (const word of words) {
      const next = line ? line + ' ' + word : word;
      if (R.measure(next, size) > width && line) {
        lines.push(line);
        line = word;
      } else {
        line = next;
      }
    }
    if (line) lines.push(line);
    return lines;
  }

  function drawScrollChoice(ctx, v) {
    cardRow(ctx, v, 'POWER SCROLL', world.pendingScroll ? 'A dual scroll — your pick, plus one more at random' : 'Choose what this run becomes', choices, function (c, what, x, y, cw) {
      if (what === 'accent') return c.hex;
      R.text(c.name.toUpperCase(), x + cw / 2, y + 30, { align: 'center', size: 9, bold: true, color: c.hex });
      R.text('+1', x + cw / 2, y + 46, { align: 'center', size: 12, bold: true, color: '#fff' });
      const blurb = {
        brutality: 'Close-quarters damage. Hit hard, stay moving.',
        tactics: 'Ranged and skill damage. Kill it before it arrives.',
        survival: 'Heavy weapons and health. Stand your ground.'
      }[c.id];
      const lines = wrapText(blurb, cw - 14, 7);
      lines.forEach(function (line, i) {
        R.text(line, x + cw / 2, y + 62 + i * 9, { align: 'center', size: 7, color: 'rgba(255,255,255,.7)' });
      });
      R.text('now ' + run.stats[c.id], x + cw / 2, y + 96, { align: 'center', size: 7, color: 'rgba(255,255,255,.45)' });
      return null;
    });
  }

  function drawMutationChoice(ctx, v) {
    cardRow(ctx, v, 'MUTATION', 'Biome cleared — take one with you', choices, function (m, what, x, y, cw) {
      const accent = CONTENT.COLORS[m.color].hex;
      if (what === 'accent') return accent;
      R.text(m.name.toUpperCase(), x + cw / 2, y + 26, { align: 'center', size: 8, bold: true, color: accent });
      const lines = wrapText(m.desc, cw - 14, 7);
      lines.forEach(function (line, i) {
        R.text(line, x + cw / 2, y + 44 + i * 9, { align: 'center', size: 7, color: 'rgba(255,255,255,.78)' });
      });
      return null;
    });
  }

  /* ----------------------------------------------------------- list screens */
  function drawList(ctx, v, title, items, detail) {
    const w = Math.min(260, v.w - 40);
    const rowH = 15;
    const h = items.length * rowH + 44;
    const x = (v.w - w) / 2;
    const y = Math.max(14, (v.h - h) / 2);

    R.panel(x, y, w, h, 'rgba(47,230,200,.5)');
    R.text(title, x + w / 2, y + 18, { align: 'center', size: 11, bold: true, color: '#fff' });

    items.forEach(function (item, i) {
      const ry = y + 28 + i * rowH;
      const chosen = i === menuIndex;
      if (chosen) R.rect(x + 4, ry, w - 8, rowH - 2, 'rgba(47,230,200,.18)');
      R.text((chosen ? '> ' : '  ') + item, x + 12, ry + 10, { size: 8, color: chosen ? '#fff' : 'rgba(255,255,255,.7)' });
      menuRects.push({ x: x + 4, y: ry, w: w - 8, h: rowH - 2, index: i });
    });

    if (detail) R.text(detail, x + w / 2, y + h - 8, { align: 'center', size: 7, color: 'rgba(255,255,255,.5)' });
  }

  function drawShop(ctx, v) {
    const p = world.player;
    const w = Math.min(300, v.w - 24);
    const rowH = 20;
    const items = shopStock;
    const h = items.length * rowH + 62;
    const x = (v.w - w) / 2;
    const y = Math.max(10, (v.h - h) / 2);

    R.panel(x, y, w, h, 'rgba(255,211,74,.55)');
    R.text('SHOP', x + w / 2, y + 18, { align: 'center', size: 12, bold: true, color: '#ffd34a' });
    R.text('You have ' + p.gold + ' gold', x + w / 2, y + 29, { align: 'center', size: 7, color: 'rgba(255,255,255,.65)' });

    items.forEach(function (entry, i) {
      const ry = y + 36 + i * rowH;
      const chosen = i === menuIndex;
      const accent = CONTENT.COLORS[entry.color].hex;
      if (chosen) R.rect(x + 4, ry, w - 8, rowH - 2, 'rgba(255,211,74,.14)');
      R.rect(x + 6, ry + 2, 2, rowH - 6, accent);
      R.text(entry.sold ? 'SOLD — ' + entry.name : entry.name, x + 12, ry + 9, {
        size: 7, color: entry.sold ? 'rgba(255,255,255,.35)' : chosen ? '#fff' : 'rgba(255,255,255,.8)'
      });
      R.text(trim(entry.desc || '', 46), x + 12, ry + 17, { size: 7, color: 'rgba(255,255,255,.45)' });
      R.text(entry.sold ? '' : entry.price + 'g', x + w - 8, ry + 12, {
        align: 'right', size: 8, bold: true, color: p.gold >= entry.price ? '#ffd34a' : '#ff6b8a'
      });
      menuRects.push({ x: x + 4, y: ry, w: w - 8, h: rowH - 2, index: i });
    });

    const ly = y + 36 + items.length * rowH;
    const chosenLeave = menuIndex === items.length;
    R.text((chosenLeave ? '> ' : '  ') + 'Leave  (' + hint('interact') + ')', x + w / 2, ly + 10, {
      align: 'center', size: 8, color: chosenLeave ? '#fff' : 'rgba(255,255,255,.6)'
    });
    menuRects.push({ x: x + 4, y: ly, w: w - 8, h: 14, index: items.length });
  }

  function drawCollector(ctx, v) {
    const p = world.player;
    const list = CONTENT.unlockables();
    const w = Math.min(320, v.w - 20);
    const rowH = 14;
    const h = list.length * rowH + 72;
    const x = (v.w - w) / 2;
    const y = Math.max(6, (v.h - h) / 2);

    R.panel(x, y, w, h, 'rgba(47,230,200,.55)');
    R.text('THE COLLECTOR', x + w / 2, y + 18, { align: 'center', size: 12, bold: true, color: '#2fe6c8' });
    R.text('Blueprints bought here stay bought — in this run and every run after.', x + w / 2, y + 29, {
      align: 'center', size: 7, color: 'rgba(255,255,255,.6)'
    });
    R.text(p.cells + ' cells carried', x + w / 2, y + 39, { align: 'center', size: 8, bold: true, color: '#9ffff0' });

    list.forEach(function (entry, i) {
      const ry = y + 46 + i * rowH;
      const chosen = i === menuIndex;
      const owned = meta.unlocked.indexOf(entry.id) !== -1;
      const accent = CONTENT.COLORS[entry.color].hex;
      if (chosen) R.rect(x + 4, ry, w - 8, rowH - 2, 'rgba(47,230,200,.14)');
      R.rect(x + 6, ry + 2, 2, rowH - 6, accent);
      R.text((entry.kind === 'skill' ? 'skill  ' : 'weapon ') + entry.name, x + 12, ry + 9, {
        size: 7, color: owned ? 'rgba(255,255,255,.4)' : chosen ? '#fff' : 'rgba(255,255,255,.8)'
      });
      R.text(owned ? 'UNLOCKED' : entry.cost + ' cells', x + w - 8, ry + 9, {
        align: 'right', size: 7, bold: !owned,
        color: owned ? '#2fe6c8' : p.cells >= entry.cost ? '#9ffff0' : '#ff6b8a'
      });
      menuRects.push({ x: x + 4, y: ry, w: w - 8, h: rowH - 2, index: i });
    });

    const ly = y + 46 + list.length * rowH;
    const chosenLeave = menuIndex === list.length;
    R.text((chosenLeave ? '> ' : '  ') + 'Leave  (' + hint('interact') + ')', x + w / 2, ly + 12, {
      align: 'center', size: 8, color: chosenLeave ? '#fff' : 'rgba(255,255,255,.6)'
    });
    menuRects.push({ x: x + 4, y: ly, w: w - 8, h: 14, index: list.length });
  }

  /* ----------------------------------------------------------- end screens */
  function drawDeath(ctx, v) {
    R.text('YOU DIED', v.w / 2, v.h / 2 - 40, { align: 'center', size: 22, bold: true, color: '#ff3b5c' });
    R.text(
      'killed in ' + CONTENT.BIOMES[run.biomeIndex].name,
      v.w / 2, v.h / 2 - 22, { align: 'center', size: 8, color: 'rgba(255,255,255,.75)' }
    );

    const lines = [
      'biome reached   ' + (run.biomeIndex + 1) + ' / ' + CONTENT.BIOMES.length,
      'kills           ' + run.kills,
      'time            ' + clock(run.time),
      'cells banked    ' + (run.banked || 0) + '  (half of what you carried)',
      'spent on        ' + (run.spent || 0) + ' cells of blueprints'
    ];
    lines.forEach(function (line, i) {
      R.text(line, v.w / 2, v.h / 2 + i * 11, { align: 'center', size: 8, color: 'rgba(255,255,255,.8)' });
    });

    R.text(hint('confirm') + ' or tap — back to the top', v.w / 2, v.h / 2 + lines.length * 11 + 16, {
      align: 'center', size: 8, color: '#2fe6c8'
    });
  }

  function drawVictory(ctx, v) {
    R.text('THE THRONE IS EMPTY', v.w / 2, v.h * 0.3, { align: 'center', size: 18, bold: true, color: '#ffe600' });
    R.text('You beat the run. A Boss Cell has been added — the next one is harder.',
      v.w / 2, v.h * 0.3 + 18, { align: 'center', size: 8, color: 'rgba(255,255,255,.8)' });

    const lines = [
      'time            ' + clock(run.time),
      'kills           ' + run.kills,
      'boss cells      ' + meta.bossCells,
      'cells banked    ' + (run.banked || 0)
    ];
    lines.forEach(function (line, i) {
      R.text(line, v.w / 2, v.h * 0.5 + i * 11, { align: 'center', size: 8, color: 'rgba(255,255,255,.85)' });
    });
    R.text(hint('confirm') + ' or tap — go again', v.w / 2, v.h * 0.5 + lines.length * 11 + 18, {
      align: 'center', size: 8, color: '#2fe6c8'
    });
  }

  /* ------------------------------------------------------------ title page */
  function drawTitleBackdrop(ctx, v) {
    const g = ctx.createLinearGradient(0, 0, 0, v.h);
    g.addColorStop(0, '#0b0714');
    g.addColorStop(1, '#1a1026');
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, v.w, v.h);

    /* a silhouette skyline and drifting embers, so the menu is not a blank wall */
    ctx.fillStyle = 'rgba(0,0,0,.4)';
    for (let i = 0; i < 14; i++) {
      const n = ((i * 2654435761) % 1000) / 1000;
      const w = 22 + n * 34;
      const h = 40 + n * 90;
      ctx.fillRect(i * 42 - 10, v.h - h, w, h);
    }
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    for (let i = 0; i < 26; i++) {
      const n = ((i * 40503) % 997) / 997;
      const x = (n * v.w + titleT * (8 + n * 14)) % v.w;
      const y = v.h - ((titleT * (12 + n * 20) + n * v.h) % v.h);
      ctx.fillStyle = 'rgba(47,230,200,.5)';
      ctx.fillRect(Math.round(x), Math.round(y), 1, 2);
    }
    ctx.restore();
  }

  function drawTitle(ctx, v) {
    drawTitleBackdrop(ctx, v);

    const cy = Math.max(44, v.h * 0.26);
    R.text('NEON', v.w / 2 - 2, cy, { align: 'center', size: Math.round(Math.min(34, v.w / 12)), bold: true, color: '#ff3b5c' });
    R.text('CELLS', v.w / 2 + 2, cy + Math.min(30, v.w / 14), {
      align: 'center', size: Math.round(Math.min(34, v.w / 12)), bold: true, color: '#2fe6c8'
    });
    R.text('a roguelite in the shape of Dead Cells', v.w / 2, cy + Math.min(48, v.w / 9), {
      align: 'center', size: 8, color: 'rgba(255,255,255,.65)'
    });
    if (padConnected) {
      R.text('controller ready — press A', v.w / 2, cy + Math.min(60, v.w / 9) + 10, {
        align: 'center', size: 7, color: '#2fe6c8'
      });
    }

    const items = menuItems();
    const y0 = Math.min(v.h - 66, cy + Math.min(70, v.w / 7));
    items.forEach(function (item, i) {
      const w = 120;
      const x = (v.w - w) / 2;
      const y = y0 + i * 20;
      const chosen = i === menuIndex;
      R.panel(x, y, w, 17, chosen ? '#2fe6c8' : 'rgba(255,255,255,.18)');
      R.text(item.toUpperCase(), v.w / 2, y + 12, {
        align: 'center', size: 8, bold: true, color: chosen ? '#fff' : 'rgba(255,255,255,.7)'
      });
      menuRects.push({ x: x, y: y, w: w, h: 17, index: i });
    });

    const stats = [
      'runs ' + meta.runs + '   ·   wins ' + meta.wins + '   ·   deepest ' + (meta.bestDepth || 0) + ' / ' + CONTENT.BIOMES.length,
      'cells banked ' + meta.cells + '   ·   blueprints ' + meta.unlocked.length + ' / ' + CONTENT.unlockables().length +
        (meta.bossCells ? '   ·   boss cells ' + meta.bossCells : '')
    ];
    stats.forEach(function (line, i) {
      R.text(line, v.w / 2, v.h - 26 + i * 10, { align: 'center', size: 7, color: 'rgba(255,255,255,.5)' });
    });
  }

  function drawHelp(ctx, v) {
    const w = Math.min(374, v.w - 12);
    const x = (v.w - w) / 2;

    /* action, keyboard, controller, touch */
    const rows = [
      ['move', 'A / D  ·  arrows', 'stick / d-pad', 'left thumb'],
      ['jump', 'SPACE  ·  W', 'A', 'JMP'],
      ['roll', 'SHIFT', 'B', 'ROLL'],
      ['', 'invulnerable while rolling — roll through an enemy to hit its back', '', ''],
      ['attack', 'J  ·  K', 'X  ·  Y', 'ATK · ATK2'],
      ['skills', 'U  ·  I', 'LB  ·  RB', 'S1 · S2'],
      ['heal', 'Q', 'RT', 'HEAL'],
      ['use', 'E', 'LT', 'USE'],
      ['', 'doors, chests, scrolls, shops and the Collector', '', ''],
      ['drop', 'hold DOWN and jump to fall through a platform', '', ''],
      ['pause', 'ESC', 'START', ''],
      ['mute', 'M', 'BACK', '']
    ];
    const h = rows.length * 13 + 62;
    const y = Math.max(4, (v.h - h) / 2);

    const colKey = x + 56;
    const colPad = x + w - 136;
    const colTouch = x + w - 10;

    R.panel(x, y, w, h, 'rgba(47,230,200,.5)');
    R.text('HOW TO PLAY', x + w / 2, y + 17, { align: 'center', size: 11, bold: true, color: '#fff' });
    R.text('Find the exit, fight what is in the way, read every scroll you find.',
      x + w / 2, y + 28, { align: 'center', size: 7, color: 'rgba(255,255,255,.6)' });

    R.text('keyboard', colKey, y + 40, { size: 7, color: 'rgba(255,255,255,.4)' });
    R.text('controller', colPad, y + 40, { size: 7, color: padConnected ? '#2fe6c8' : 'rgba(255,255,255,.4)' });
    R.text('touch', colTouch, y + 40, { align: 'right', size: 7, color: 'rgba(255,255,255,.4)' });

    rows.forEach(function (row, i) {
      const ry = y + 52 + i * 13;
      if (!row[0]) {
        /* a note, not a binding: let it run the width of the panel */
        R.text(row[1], x + 10, ry, { size: 7, color: 'rgba(255,255,255,.5)' });
        return;
      }
      R.text(row[0], x + 10, ry, { size: 7, color: '#2fe6c8', bold: true });
      R.text(row[1], colKey, ry, { size: 7, color: 'rgba(255,255,255,.82)' });
      if (row[2]) R.text(row[2], colPad, ry, { size: 7, color: padConnected ? '#9ffff0' : 'rgba(255,255,255,.6)' });
      if (row[3]) R.text(row[3], colTouch, ry, { align: 'right', size: 7, color: 'rgba(255,255,255,.45)' });
    });

    R.text(hint('confirm') + ' or tap to go back', x + w / 2, y + h - 8, {
      align: 'center', size: 7, color: 'rgba(255,255,255,.5)'
    });
  }

  /* =========================================================================
   *  BOOT
   * ====================================================================== */
  function boot() {
    canvas = document.getElementById('game');
    calmMotion = !!(global.matchMedia && global.matchMedia('(prefers-reduced-motion: reduce)').matches);
    R.init(canvas);
    layoutTouchButtons();
    meta = META.load();
    bindInput();

    global.addEventListener('resize', function () {
      R.resize();
      layoutTouchButtons();
    });
    global.addEventListener('orientationchange', function () {
      global.setTimeout(function () {
        R.resize();
        layoutTouchButtons();
      }, 150);
    });

    const splash = document.getElementById('loading');
    if (splash) splash.style.display = 'none';

    lastTime = global.performance ? global.performance.now() : Date.now();
    global.requestAnimationFrame(frame);
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', boot);
  } else {
    boot();
  }

  /* The test harness drives these; nothing else should need them. */
  global.NEON_CELLS = {
    STATE: STATE,
    state: function () { return state; },
    setState: function (s) { state = s; },
    world: function () { return world; },
    run: function () { return run; },
    startRun: startRun,
    enterBiome: enterBiome,
    nextBiome: nextBiome,
    interactNow: interact,
    nearby: function () { return nearby; },
    press: function (code) { keys[code] = true; pressed[code] = true; },
    hint: hint,
    touchButtons: function () { return touch.buttons; },
    padConnected: function () { return padConnected; },
    release: function (code) { keys[code] = false; }
  };
})(typeof globalThis !== 'undefined' ? globalThis : this);
