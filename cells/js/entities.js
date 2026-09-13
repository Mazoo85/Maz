/* =============================================================================
 *  NEON CELLS  —  bodies, movement and fighting
 *
 *  Physics first: an axis-separated AABB against the tile grid, with one-way
 *  platforms you can jump up through and drop down out of. Then the player's
 *  moveset — run, jump, roll with invulnerability frames, a weapon in each
 *  hand, two skills, a parry — and then the enemies, each with a telegraphed
 *  wind-up so every hit you take is a hit you could have read.
 * ========================================================================== */
(function (global) {
  'use strict';

  const LG = global.CELLS_LEVELGEN || require('./levelgen.js');
  const CB = global.CELLS_COMBAT || require('./combat.js');
  const CONTENT = global.CELLS_CONTENT || require('./content.js');
  const AUDIO = global.CELLS_AUDIO || null;

  const T = LG.T;
  const TILE = LG.TILE;

  /* All randomness in here goes through one function so a test can replace it
   * with a seeded one and replay a fight exactly. In the game it is
   * Math.random, as it always was. */
  let rand = Math.random;

  function setRandom(fn) {
    rand = typeof fn === 'function' ? fn : Math.random;
  }

  const GRAVITY = 1500;
  const MAX_FALL = 780;

  /* The player's numbers. Jump height works out at ~4.8 tiles, comfortably
   * above the 4 the level generator assumes, so anything it calls reachable
   * really is reachable. */
  const P = {
    w: 11, h: 21,
    speed: 276,
    accel: 2400,
    friction: 2000,
    jump: 480,
    airControl: 0.82,
    rollSpeed: 430,
    rollTime: 0.30,
    rollInvuln: 0.26,
    rollCd: 0.42,
    coyote: 0.10,
    buffer: 0.12
  };

  function sfx(name) {
    const a = global.CELLS_AUDIO || AUDIO;
    if (a) a.play(name);
  }

  /* ------------------------------------------------------------- collision */
  function tileVal(level, tx, ty) {
    return LG.at(level, tx, ty);
  }

  /* Does a body of this size at this position overlap anything that stops it?
   * prevBottom is where its feet were a moment ago, which is how a one-way
   * platform knows whether the body is landing on it or rising through it. */
  function blocked(level, x, y, w, h, ignorePlatforms, prevBottom) {
    const left = Math.floor((x - w / 2) / TILE);
    const right = Math.floor((x + w / 2 - 0.01) / TILE);
    const top = Math.floor((y - h) / TILE);
    const bottom = Math.floor((y - 0.01) / TILE);

    for (let ty = top; ty <= bottom; ty++) {
      for (let tx = left; tx <= right; tx++) {
        const v = tileVal(level, tx, ty);
        if (v === T.SOLID || v === T.SPIKE) return true;
        if (v === T.PLATFORM && !ignorePlatforms) {
          const surface = ty * TILE;
          if (prevBottom != null && prevBottom <= surface + 1.0 && y > surface) return true;
        }
      }
    }
    return false;
  }

  function moveBody(level, b, dt) {
    const dx = b.vx * dt;
    const dy = b.vy * dt;
    const steps = Math.max(1, Math.ceil(Math.max(Math.abs(dx), Math.abs(dy)) / 5));
    const sx = dx / steps;
    const sy = dy / steps;

    b.onGround = false;
    b.hitWall = false;
    b.hitCeiling = false;

    for (let i = 0; i < steps; i++) {
      if (sx !== 0) {
        const prevX = b.x;
        b.x += sx;
        if (blocked(level, b.x, b.y, b.w, b.h, true, null)) {
          b.x = prevX;
          b.vx = 0;
          b.hitWall = true;
        }
      }
      if (sy !== 0) {
        const prevY = b.y;
        b.y += sy;
        const ignore = b.vy < 0 || (b.dropThrough || 0) > 0 || b.flies;
        if (blocked(level, b.x, b.y, b.w, b.h, ignore, prevY)) {
          b.y = prevY;
          if (sy > 0) b.onGround = true;
          else b.hitCeiling = true;
          b.vy = 0;
          break;
        }
      }
    }

    /* keep bodies inside the world */
    b.x = Math.max(4, Math.min(level.w * TILE - 4, b.x));
    if (b.y > level.h * TILE + 200) b.y = level.h * TILE + 200;
  }

  /* Is the body standing on (or in) spikes? */
  function touchingSpikes(level, b) {
    const left = Math.floor((b.x - b.w / 2) / TILE);
    const right = Math.floor((b.x + b.w / 2 - 0.01) / TILE);
    const row = Math.floor((b.y + 1) / TILE);
    for (let tx = left; tx <= right; tx++) {
      if (tileVal(level, tx, row) === T.SPIKE) return true;
    }
    return false;
  }

  /* ------------------------------------------------------------- the player */
  function makePlayer(level, loadout) {
    const stats = loadout.stats || { brutality: 1, tactics: 1, survival: 1 };
    const player = {
      kind: 'player',
      x: level.spawn.x, y: level.spawn.y,
      vx: 0, vy: 0, w: P.w, h: P.h,
      facing: 1,
      onGround: false,
      stats: stats,
      mutations: loadout.mutations || [],
      weapons: loadout.weapons,
      skills: loadout.skills,
      skillCd: [0, 0],
      flasks: loadout.flasks != null ? loadout.flasks : 2,
      gold: loadout.gold || 0,
      cells: loadout.cells || 0,
      keys: loadout.keys || 0,
      maxHp: 0, hp: 0,
      status: {},
      time: 0,
      attackTimer: 0,
      swing: null,
      comboStep: 0,
      comboTimer: 0,
      activeSlot: 0,
      blocking: false,
      blockHeld: 0,
      rollTimer: 0,
      rollCd: 0,
      invuln: 0,
      coyote: 0,
      jumpBuffer: 0,
      airJumps: 0,
      dropThrough: 0,
      lastHurt: -99,
      lastHitLanded: -99,
      frenzy: 0,
      frenzyTimer: 0,
      speedBuff: 0,
      buffs: {},
      leech: 0,
      cursed: false,
      curseKills: 0,
      spikeCd: 0,
      hitFlash: 0,
      anim: 0,
      dead: false
    };
    player.maxHp = CB.maxHealth(stats, player.mutations, loadout.metaHealth || 0);
    player.hp = loadout.hp != null ? Math.min(loadout.hp, player.maxHp) : player.maxHp;
    return player;
  }

  function updatePlayer(world, input, dt) {
    const p = world.player;
    const level = world.level;
    p.time += dt;
    p.anim += dt;

    /* --- timers */
    p.attackTimer = Math.max(0, p.attackTimer - dt);
    p.rollTimer = Math.max(0, p.rollTimer - dt);
    p.rollCd = Math.max(0, p.rollCd - dt);
    p.invuln = Math.max(0, p.invuln - dt);
    p.dropThrough = Math.max(0, p.dropThrough - dt);
    p.comboTimer = Math.max(0, p.comboTimer - dt);
    p.hitFlash = Math.max(0, p.hitFlash - dt);
    p.spikeCd = Math.max(0, p.spikeCd - dt);
    if (p.comboTimer <= 0) p.comboStep = 0;
    for (let i = 0; i < 2; i++) p.skillCd[i] = Math.max(0, p.skillCd[i] - dt);

    if (p.frenzyTimer > 0) {
      p.frenzyTimer -= dt;
      if (p.frenzyTimer <= 0) p.frenzy = 0;
    }
    if (p.speedBuff > 0) p.speedBuff -= dt;
    if (p.buffs.leechT > 0) {
      p.buffs.leechT -= dt;
      if (p.buffs.leechT <= 0) p.leech = 0;
    }

    /* --- statuses the player is suffering */
    const tickOpts = { openWounds: CB.hasMutation(p.mutations, 'open_wounds') };
    const st = CB.tickStatuses(p, dt, tickOpts);
    if (st.damage > 0) hurtPlayer(world, st.damage, 'status', null, true);
    const held = st.held;

    /* --- roll: i-frames, and it carries you through enemies */
    if (input.roll && p.rollCd <= 0 && p.rollTimer <= 0 && !held) {
      p.rollTimer = P.rollTime;
      p.rollCd = P.rollCd;
      p.invuln = Math.max(p.invuln, P.rollInvuln);
      p.swing = null;
      if (input.left || input.right) p.facing = input.left ? -1 : 1;
      p.vx = p.facing * P.rollSpeed;
      sfx('roll');
      if (CB.hasMutation(p.mutations, 'armour')) healPlayer(world, 1);
    }

    const rolling = p.rollTimer > 0;
    const attacking = p.swing && p.swing.t > 0;

    /* --- walking */
    let speed = P.speed;
    if (p.speedBuff > 0) speed *= 1.25;
    if (attacking && p.onGround) speed *= 0.55;
    if (p.blocking) speed *= 0.5;
    speed *= CB.statusSpeedMultiplier(p);

    let want = 0;
    if (!held && !rolling) {
      if (input.left) want -= 1;
      if (input.right) want += 1;
      if (want !== 0 && !attacking) p.facing = want;
    }

    if (rolling) {
      p.vx = p.facing * P.rollSpeed;
    } else {
      const control = p.onGround ? 1 : P.airControl;
      if (want !== 0) {
        p.vx += want * P.accel * control * dt;
        p.vx = Math.max(-speed, Math.min(speed, p.vx));
      } else {
        const drag = P.friction * (p.onGround ? 1 : 0.35) * dt;
        if (Math.abs(p.vx) <= drag) p.vx = 0;
        else p.vx -= Math.sign(p.vx) * drag;
      }
    }

    /* --- jumping, with coyote time and a buffered press */
    if (p.onGround) {
      p.coyote = P.coyote;
      p.airJumps = CB.hasMutation(p.mutations, 'acrobat') ? 1 : 0;
    } else {
      p.coyote = Math.max(0, p.coyote - dt);
    }
    if (input.jump) p.jumpBuffer = P.buffer;
    else p.jumpBuffer = Math.max(0, p.jumpBuffer - dt);

    const wantsDown = input.down;
    if (p.jumpBuffer > 0 && !held) {
      const onPlatform = standingOnPlatform(level, p);
      if (wantsDown && onPlatform) {
        p.dropThrough = 0.22;
        p.y += 2;
        p.jumpBuffer = 0;
      } else if (p.coyote > 0) {
        p.vy = -P.jump;
        p.coyote = 0;
        p.jumpBuffer = 0;
        sfx('jump');
      } else if (p.airJumps > 0) {
        p.vy = -P.jump * 0.94;
        p.airJumps--;
        p.jumpBuffer = 0;
        sfx('jump');
        puff(world, p.x, p.y, '#cfe9ff', 8);
      }
    }
    /* releasing jump early cuts the arc short — a platformer has to do this */
    if (!input.jumpHeld && p.vy < -140) p.vy *= 0.86;

    /* --- gravity */
    p.vy = Math.min(MAX_FALL, p.vy + GRAVITY * dt);

    const wasAir = !p.onGround;
    moveBody(level, p, dt);
    if (wasAir && p.onGround && p.vy === 0) sfx('land');

    /* --- spikes */
    if (touchingSpikes(level, p) && p.spikeCd <= 0) {
      p.spikeCd = 0.7;
      hurtPlayer(world, 8 + 3 * (level.depth || 1), 'trap', null);
      p.vy = -280;
    }

    /* --- weapons */
    if (!rolling && !held) {
      handleAttacks(world, input, dt);
    } else {
      p.blocking = false;
    }
    advanceSwing(world, dt);

    /* --- skills */
    for (let i = 0; i < 2; i++) {
      const pressed = i === 0 ? input.skill1 : input.skill2;
      if (pressed && p.skills[i] && p.skillCd[i] <= 0 && !held) useSkill(world, i);
    }

    /* --- flask */
    if (input.flask && p.flasks > 0 && p.hp < p.maxHp) {
      p.flasks--;
      healPlayer(world, CB.healAmount(p, Math.round(p.maxHp * 0.5)));
      sfx('pickup');
      floatText(world, p.x, p.y - 24, 'HEALED', '#2fe6c8');
    }

    if (p.hp <= 0 && !p.dead) killPlayer(world);
  }

  function standingOnPlatform(level, b) {
    if (!b.onGround) return false;
    const row = Math.floor((b.y + 1) / TILE);
    const left = Math.floor((b.x - b.w / 2) / TILE);
    const right = Math.floor((b.x + b.w / 2 - 0.01) / TILE);
    for (let tx = left; tx <= right; tx++) {
      if (tileVal(level, tx, row) === T.PLATFORM) return true;
    }
    return false;
  }

  /* --------------------------------------------------------- swinging things */
  function handleAttacks(world, input, dt) {
    const p = world.player;
    const pressed = [input.atk1, input.atk2];
    const held = [input.atk1Held, input.atk2Held];

    p.blocking = false;
    for (let slot = 0; slot < 2; slot++) {
      const weapon = p.weapons[slot];
      if (!weapon) continue;

      if (weapon.kind === 'shield') {
        if (held[slot]) {
          if (!p.blocking) {
            if (p.blockHeld <= 0) p.blockHeld = 0.0001;
            p.blocking = true;
            p.blockSlot = slot;
          }
          p.blockHeld += dt;
        }
        continue;
      }

      if (pressed[slot] && p.attackTimer <= 0) startAttack(world, slot);
    }
    if (!p.blocking) p.blockHeld = 0;
  }

  function startAttack(world, slot) {
    const p = world.player;
    const weapon = p.weapons[slot];
    if (!weapon) return;

    p.activeSlot = slot;
    p.attackTimer = CB.attackInterval(weapon, p);
    p.comboStep = (p.comboStep % Math.max(1, weapon.combo)) + 1;
    p.comboTimer = 0.9;

    if (weapon.kind === 'shoot') {
      fireWeapon(world, weapon);
      sfx(weapon.proj && weapon.proj.kind === 'bolt' ? 'bolt' : 'bow');
      p.swing = { t: 0.12, total: 0.12, kind: 'shoot', weapon: weapon, dir: p.facing, hits: null };
      return;
    }

    const slam = weapon.kind === 'slam';
    p.swing = {
      t: slam ? 0.22 : 0.16,
      total: slam ? 0.22 : 0.16,
      kind: weapon.kind,
      weapon: weapon,
      dir: p.facing,
      step: p.comboStep,
      hits: new Set(),
      airborne: !p.onGround
    };
    sfx(slam ? 'heavy' : 'swing');
    if (slam) world.shake = Math.max(world.shake, 2);
  }

  /* A melee swing is a short-lived sector in front of the player. */
  function advanceSwing(world, dt) {
    const p = world.player;
    if (!p.swing) return;
    p.swing.t -= dt;

    const sw = p.swing;
    if (sw.kind !== 'shoot' && sw.hits) {
      const weapon = sw.weapon;
      const reach = weapon.reach + (sw.step > 2 ? 5 : 0);
      const arc = weapon.arc || 1.2;
      const ox = p.x + sw.dir * 4;
      const oy = p.y - p.h * 0.55;

      for (const e of world.enemies) {
        if (e.dead || sw.hits.has(e)) continue;
        const dx = e.x - ox;
        const dy = (e.y - e.h / 2) - oy;
        const dist = Math.hypot(dx, dy) - e.w * 0.4;
        if (dist > reach) continue;
        const ang = Math.atan2(dy, dx * sw.dir);
        if (Math.abs(ang) > arc / 2) continue;

        sw.hits.add(e);
        const toPlayer = Math.sign(p.x - e.x) || 1;
        hitEnemy(world, e, weapon, {
          behind: e.facing !== toPlayer,
          airborne: sw.airborne,
          distance: dist,
          knockDir: sw.dir
        });
      }
      if (world.boss && !world.boss.dead && !sw.hits.has(world.boss)) {
        const b = world.boss;
        const dx = b.x - ox;
        const dy = (b.y - b.h / 2) - oy;
        const dist = Math.hypot(dx, dy) - b.w * 0.4;
        if (dist <= reach && Math.abs(Math.atan2(dy, dx * sw.dir)) <= arc / 2) {
          sw.hits.add(b);
          hitEnemy(world, b, weapon, { airborne: sw.airborne, distance: dist, knockDir: sw.dir });
        }
      }
    }

    if (p.swing.t <= 0) p.swing = null;
  }

  /* Where a shot should go. There is no aiming stick — you fire where you face
   * — so a shot leans onto whatever is in front of you within a narrow cone.
   * Without this, a horizontal arrow physically cannot touch a bat hovering at
   * head height, and a ranged run has no answer to half the roster. */
  function aimAngle(world, ox, oy, facing, range, cone) {
    const targets = world.enemies.concat(world.boss && !world.boss.dead ? [world.boss] : []);
    let best = null;
    let bestDist = range;

    for (const e of targets) {
      if (e.dead) continue;
      const dx = e.x - ox;
      const dy = (e.y - e.h / 2) - oy;
      if (Math.sign(dx) !== facing && Math.abs(dx) > 8) continue;  // behind you
      const dist = Math.hypot(dx, dy);
      if (dist > bestDist) continue;
      if (Math.abs(Math.atan2(dy, dx * facing)) > cone) continue;  // outside the cone
      bestDist = dist;
      best = { dx: dx, dy: dy };
    }

    if (!best) return facing > 0 ? 0 : Math.PI;
    return Math.atan2(best.dy, best.dx);
  }

  function fireWeapon(world, weapon) {
    const p = world.player;
    const proj = weapon.proj;
    const count = proj.count || 1;
    const ox = p.x + p.facing * 8;
    const oy = p.y - p.h * 0.55;
    const aim = aimAngle(world, ox, oy, p.facing, 280, 0.5);

    for (let i = 0; i < count; i++) {
      const spread = (proj.spread || 0) * (count > 1 ? i - (count - 1) / 2 : 0);
      const angle = aim + spread * p.facing;
      spawnProjectile(world, {
        from: 'player',
        kind: proj.kind,
        x: ox,
        y: oy,
        vx: Math.cos(angle) * proj.speed,
        vy: Math.sin(angle) * proj.speed,
        life: proj.life,
        pierce: proj.pierce || 0,
        weapon: weapon
      });
    }
  }

  /* ------------------------------------------------------------- the skills */
  function useSkill(world, index) {
    const p = world.player;
    const skill = p.skills[index];
    if (!skill) return;
    p.skillCd[index] = skill.cd;

    switch (skill.kind) {
      case 'throw':
        spawnProjectile(world, {
          from: 'player', kind: 'grenade', x: p.x + p.facing * 8, y: p.y - p.h * 0.6,
          vx: p.facing * 300, vy: -250, life: 3, gravity: true, skill: skill
        });
        break;

      case 'blast': {
        blast(world, p.x, p.y - p.h / 2, skill.radius, skill, 'player');
        sfx(skill.status && skill.status.status === 'frozen' ? 'freeze' : 'explode');
        world.shake = Math.max(world.shake, 4);
        break;
      }

      case 'turret':
        world.turrets.push({
          x: p.x, y: p.y, w: 12, h: 14, vx: 0, vy: 0,
          life: skill.life, cd: 0, skill: skill, onGround: false
        });
        sfx('buy');
        break;

      case 'place':
        world.traps.push({ x: p.x, y: p.y - 2, w: 16, h: 6, life: skill.life, skill: skill, armed: true });
        sfx('locked');
        break;

      case 'orb':
        world.orbs.push({
          x: p.x, y: p.y - p.h / 2, vx: p.facing * 190, vy: 0,
          life: skill.life, skill: skill, spin: 0, hitCd: new Map()
        });
        sfx('bolt');
        break;

      case 'buff':
        p.leech = skill.leech;
        p.buffs.leechT = skill.dur;
        floatText(world, p.x, p.y - 26, 'VAMPIRISM', '#ff3b5c');
        sfx('levelup');
        break;
    }
  }

  /* An area hit: used by skills, grenades and boss slams alike. */
  function blast(world, x, y, radius, source, from) {
    const p = world.player;
    ring(world, x, y, radius, source.status && source.status.status === 'frozen' ? '#7fd8ff' : '#ff8a1e');

    if (from === 'player') {
      const targets = world.enemies.concat(world.boss && !world.boss.dead ? [world.boss] : []);
      for (const e of targets) {
        if (e.dead) continue;
        const d = Math.hypot(e.x - x, (e.y - e.h / 2) - y);
        if (d > radius + e.w / 2) continue;
        const dmg = Math.round(source.dmg * CB.scaleFor(source.color || 'tactics', p.stats) * CB.playerDamageMultiplier(p));
        damageEnemy(world, e, dmg, { crit: false, knock: source.knock || 160, knockDir: Math.sign(e.x - x) || 1, aoe: true });
        if (source.status) CB.applyStatus(e, source.status.status, source.status.dur);
      }
    } else {
      const d = Math.hypot(p.x - x, (p.y - p.h / 2) - y);
      if (d <= radius + p.w / 2) {
        hurtPlayer(world, source.dmg, 'blast', { x: x, y: y });
        if (source.status) CB.applyStatus(p, source.status.status, source.status.dur);
      }
    }
  }

  /* --------------------------------------------------------- taking damage */
  function hurtPlayer(world, amount, kind, source, silent) {
    const p = world.player;
    if (p.dead) return;
    if (p.invuln > 0 && kind !== 'status') return;

    /* Bleeding, burning and poison arrive a fraction of a point at a time, sixty
     * times a second. They must not go through the rounding below — rounding a
     * 0.08 tick up to a whole point turns a 5-per-second poison into 60, which
     * is the difference between a status and a death sentence. */
    if (kind === 'status') {
      p.hp -= amount;
      p.lastHurt = p.time;
      p.recoveryDebt = (p.recoveryDebt || 0) + amount;
      p.recoveryUntil = p.time + 3;
      p.hitFlash = Math.max(p.hitFlash, 0.05);
      if (p.hp <= 0) killPlayer(world);
      return;
    }

    /* A shield raised into the blow: the first moments are a parry. */
    if (p.blocking && kind !== 'status' && source) {
      const shield = p.weapons[p.blockSlot];
      const facingIt = Math.sign(source.x - p.x) === p.facing || source.x === p.x;
      if (shield && facingIt) {
        const parry = p.blockHeld <= (shield.block || 0.4);
        sfx(parry ? 'parry' : 'block');
        if (parry) {
          p.invuln = Math.max(p.invuln, 0.25);
          world.shake = Math.max(world.shake, 5);
          floatText(world, p.x, p.y - 28, 'PARRY', '#7fd8ff');
          /* throw it back, harder */
          const targets = world.enemies.concat(world.boss && !world.boss.dead ? [world.boss] : []);
          for (const e of targets) {
            if (e.dead) continue;
            if (Math.hypot(e.x - p.x, e.y - p.y) > 52) continue;
            const dmg = Math.round(shield.dmg * (shield.reflect || 1.5) * CB.scaleFor(shield.color, p.stats));
            damageEnemy(world, e, dmg, { crit: true, knock: shield.knock, knockDir: p.facing });
            for (const s of shield.onHit || []) CB.applyStatus(e, s.status, s.dur);
          }
          return;
        }
        amount *= 0.35;
      }
    }

    const dealt = CB.incomingDamage(p, amount, kind);
    p.hp -= dealt;
    p.lastHurt = p.time;
    p.hitFlash = 0.2;
    p.recoveryDebt = (p.recoveryDebt || 0) + dealt;
    p.recoveryUntil = p.time + 3;

    if (!silent) {
      p.invuln = Math.max(p.invuln, 0.45);
      sfx('hurt');
      world.shake = Math.max(world.shake, Math.min(9, 3 + dealt * 0.18));
      world.freeze = Math.max(world.freeze, 0.05);
      blood(world, p.x, p.y - p.h / 2, 10, '#ff3b5c');
      if (source) {
        const dir = Math.sign(p.x - source.x) || 1;
        p.vx = dir * 170;
        p.vy = Math.min(p.vy, -150);
      }
    }
    floatText(world, p.x, p.y - p.h - 4, '-' + dealt, '#ff6b8a');

    if (p.hp <= 0) killPlayer(world);
  }

  function healPlayer(world, amount) {
    const p = world.player;
    if (p.dead) return;
    const before = p.hp;
    p.hp = Math.min(p.maxHp, p.hp + amount);
    if (p.hp > before) floatText(world, p.x, p.y - p.h - 6, '+' + (p.hp - before), '#2fe6c8');
  }

  function killPlayer(world) {
    const p = world.player;
    p.dead = true;
    p.hp = 0;
    sfx('death');
    world.shake = 14;
    blood(world, p.x, p.y - p.h / 2, 40, '#ff3b5c');
    world.events.push({ type: 'player_died' });
  }

  /* =========================================================================
   *  ENEMIES
   *
   *  Every attack is telegraphed: an enemy leans into a wind-up you can see
   *  before the hit lands, so the answer is always to roll, parry or leave.
   * ====================================================================== */
  function makeEnemy(spec) {
    const def = CONTENT.ENEMY[spec.id];
    const scale = spec.elite ? 1.25 : 1;
    return {
      kind: 'enemy',
      id: spec.id,
      def: def,
      ai: def.ai,
      x: spec.pos.x, y: spec.pos.y,
      vx: 0, vy: 0,
      w: def.w * scale, h: def.h * scale,
      hp: spec.hp, maxHp: spec.hp,
      dmg: spec.dmg,
      speed: def.speed,
      facing: -1,
      elite: !!spec.elite,
      hasKey: !!spec.hasKey,
      flies: def.ai === 'flyer',
      state: 'idle',
      timer: 0,
      attackCd: 0,
      anim: rand() * 6.28,
      status: {},
      aggro: false,
      hurtT: 0,
      dead: false,
      cells: def.cells * (spec.elite ? 4 : 1),
      gold: def.gold * (spec.elite ? 3 : 1),
      home: { x: spec.pos.x, y: spec.pos.y },
      patrol: rand() < 0.5 ? -1 : 1,
      onGround: false,
      /* A shieldbearer's shield is a health pool of its own: keep hitting it and
       * it breaks, which staggers the enemy and leaves it open. Rolling behind
       * is still the quick answer, but a ranged run now has one too. */
      shieldHp: def.ai === 'shielder' ? Math.round(spec.hp * 0.55) : 0,
      shieldBroken: def.ai !== 'shielder'
    };
  }

  function makeBoss(world, level) {
    const def = CONTENT.BOSS[level.boss.id];
    return {
      kind: 'boss',
      id: def.id,
      def: def,
      ai: 'boss',
      x: level.boss.pos.x, y: level.boss.pos.y,
      vx: 0, vy: 0,
      w: def.w, h: def.h,
      hp: level.boss.hp, maxHp: level.boss.hp,
      dmg: level.boss.dmg,
      speed: def.speed,
      facing: -1,
      phase: 1,
      state: 'intro',
      timer: 2.0,
      move: null,
      moveT: 0,
      anim: 0,
      status: {},
      statusImmune: false,
      hurtT: 0,
      dead: false,
      cells: def.cells,
      gold: def.gold,
      onGround: false,
      summoned: 0
    };
  }

  /* Line of sight is deliberately crude — a wall between you and an enemy
   * keeps it asleep, which is what lets you pick fights one room at a time. */
  function canSee(level, a, b) {
    const steps = Math.ceil(Math.hypot(b.x - a.x, b.y - a.y) / TILE);
    for (let i = 1; i < steps; i++) {
      const t = i / steps;
      const x = Math.floor((a.x + (b.x - a.x) * t) / TILE);
      const y = Math.floor(((a.y - a.h / 2) + ((b.y - b.h / 2) - (a.y - a.h / 2)) * t) / TILE);
      if (LG.at(level, x, y) === T.SOLID) return false;
    }
    return true;
  }

  function floorAhead(level, e, dir) {
    const tx = Math.floor((e.x + dir * (e.w / 2 + 3)) / TILE);
    const ty = Math.floor((e.y + 2) / TILE);
    return LG.isFloor(LG.at(level, tx, ty)) && LG.at(level, tx, ty - 1) !== T.SOLID;
  }

  function wallAhead(level, e, dir) {
    const tx = Math.floor((e.x + dir * (e.w / 2 + 3)) / TILE);
    const ty = Math.floor((e.y - e.h / 2) / TILE);
    return LG.at(level, tx, ty) === T.SOLID;
  }

  function updateEnemies(world, dt) {
    const p = world.player;
    const level = world.level;
    const all = world.enemies;

    for (let i = all.length - 1; i >= 0; i--) {
      const e = all[i];
      if (e.dead) {
        e.fade = (e.fade || 0) + dt;
        if (e.fade > 1.2) all.splice(i, 1);
        continue;
      }
      updateOneEnemy(world, e, dt);
    }

    if (world.boss && !world.boss.dead) updateBoss(world, world.boss, dt);

    /* turrets, traps and orbs are the player's side of the same machinery */
    updateAllies(world, dt);
    void p;
    void level;
  }

  function updateOneEnemy(world, e, dt) {
    const p = world.player;
    const level = world.level;
    e.anim += dt;
    e.hurtT = Math.max(0, e.hurtT - dt);
    e.attackCd = Math.max(0, e.attackCd - dt);

    const st = CB.tickStatuses(e, dt, { openWounds: CB.hasMutation(p.mutations, 'open_wounds') });
    if (st.damage > 0) damageEnemy(world, e, st.damage, { silent: true, fromStatus: true });
    if (e.dead) return;

    const dx = p.x - e.x;
    const dy = (p.y - p.h / 2) - (e.y - e.h / 2);
    const dist = Math.hypot(dx, dy);
    const dir = Math.sign(dx) || 1;

    if (!e.aggro && !p.dead && dist < 230 && Math.abs(dy) < 70 && canSee(level, e, p)) e.aggro = true;
    if (st.held) {
      e.vx = 0;
      if (!e.flies) e.vy = Math.min(MAX_FALL, e.vy + GRAVITY * dt);
      moveBody(level, e, dt);
      return;
    }

    const def = e.def;
    switch (e.ai) {
      case 'walker':   aiWalker(world, e, dt, dist, dir, dy); break;
      case 'archer':   aiRanged(world, e, dt, dist, dir, dy, 'arrow'); break;
      case 'bomber':   aiRanged(world, e, dt, dist, dir, dy, 'bomb'); break;
      case 'caster':   aiCaster(world, e, dt, dist, dir, dy); break;
      case 'flyer':    aiFlyer(world, e, dt, dist, dir, dy); break;
      case 'shielder': aiWalker(world, e, dt, dist, dir, dy); break;
      case 'slammer':  aiSlammer(world, e, dt, dist, dir, dy); break;
      default:         aiWalker(world, e, dt, dist, dir, dy); break;
    }
    void def;

    if (!e.flies) e.vy = Math.min(MAX_FALL, e.vy + GRAVITY * dt);
    moveBody(level, e, dt);

    if (touchingSpikes(level, e) && !e.flies) {
      e.spikeCd = (e.spikeCd || 0) - dt;
      if (e.spikeCd <= 0) {
        e.spikeCd = 0.8;
        damageEnemy(world, e, 10, { knock: 0 });
      }
    }
  }

  /* A shared wind-up → strike → recover cycle, so every melee enemy reads the
   * same way to the player even though they move differently. */
  function meleeCycle(world, e, dt, inRange, dir) {
    const p = world.player;
    if (e.state === 'windup') {
      e.timer -= dt;
      e.vx *= 0.8;
      if (e.timer <= 0) {
        e.state = 'strike';
        e.timer = 0.14;
        const reach = (e.def.reach || 18) + e.w / 2;
        if (Math.hypot(p.x - e.x, (p.y - p.h / 2) - (e.y - e.h / 2)) < reach + 8) {
          hurtPlayer(world, e.dmg, 'melee', e);
        }
        slashEffect(world, e.x + e.facing * 12, e.y - e.h / 2, e.facing, e.elite ? '#ff4fd8' : '#ffd9a0');
      }
      return true;
    }
    if (e.state === 'strike') {
      e.timer -= dt;
      e.vx *= 0.7;
      if (e.timer <= 0) {
        e.state = 'chase';
        e.attackCd = 0.55 + rand() * 0.3;
      }
      return true;
    }
    if (inRange && e.attackCd <= 0) {
      e.state = 'windup';
      e.timer = e.def.windup || 0.4;
      e.facing = dir;
      return true;
    }
    return false;
  }

  function aiWalker(world, e, dt, dist, dir, dy) {
    const level = world.level;
    const p = world.player;
    const reach = (e.def.reach || 18) + e.w / 2;
    const inRange = dist < reach + 6 && Math.abs(dy) < 26;

    if (meleeCycle(world, e, dt, inRange, dir)) return;

    if (!e.aggro || p.dead) {
      /* patrol: turn at walls, and never walk off a ledge while idle */
      if (wallAhead(level, e, e.patrol) || !floorAhead(level, e, e.patrol)) e.patrol *= -1;
      e.facing = e.patrol;
      e.vx = e.patrol * e.speed * 0.45;
      return;
    }

    e.state = 'chase';
    e.facing = dir;
    e.vx = dir * e.speed;

    /* hop up small ledges, and leap at you if it is the leaping kind */
    if (e.onGround) {
      if (wallAhead(level, e, dir) && LG.at(level, Math.floor((e.x + dir * 10) / TILE), Math.floor((e.y - e.h - 4) / TILE)) !== T.SOLID) {
        e.vy = -330;
      } else if (dy < -30 && Math.abs(dx(p, e)) < 60) {
        e.vy = -360;
      } else if (e.def.leaps && dist > 44 && dist < 110 && rand() < 0.03) {
        e.vy = -330;
        e.vx = dir * e.speed * 1.5;
      }
    }
  }

  function dx(a, b) {
    return a.x - b.x;
  }

  function aiRanged(world, e, dt, dist, dir, dy, shot) {
    const p = world.player;
    const level = world.level;
    const range = e.def.range || 190;

    if (e.state === 'windup') {
      e.timer -= dt;
      e.vx *= 0.7;
      e.facing = dir;
      if (e.timer <= 0) {
        e.state = 'idle';
        e.attackCd = 1.5 + rand() * 0.8;
        if (shot === 'bomb') {
          const t = Math.max(0.45, Math.min(1.2, dist / 260));
          spawnProjectile(world, {
            from: 'enemy', kind: 'bomb', x: e.x + dir * 8, y: e.y - e.h * 0.8,
            vx: (p.x - e.x) / t, vy: (p.y - e.h - e.y) / t - 0.5 * GRAVITY * 0.75 * t,
            life: t + 0.9, gravity: true, dmg: e.dmg, radius: 40
          });
        } else {
          const ang = Math.atan2((p.y - p.h * 0.5) - (e.y - e.h * 0.6), p.x - e.x);
          spawnProjectile(world, {
            from: 'enemy', kind: e.def.poisonShot ? 'spit' : 'arrow',
            x: e.x + dir * 8, y: e.y - e.h * 0.6,
            vx: Math.cos(ang) * 300, vy: Math.sin(ang) * 300,
            life: 2.2, dmg: e.dmg,
            status: e.def.poisonShot ? { status: 'poison', dur: 3.0 } : null
          });
        }
        sfx(shot === 'bomb' ? 'bolt' : 'bow');
      }
      return;
    }

    if (!e.aggro || p.dead) {
      if (wallAhead(level, e, e.patrol) || !floorAhead(level, e, e.patrol)) e.patrol *= -1;
      e.facing = e.patrol;
      e.vx = e.patrol * e.speed * 0.4;
      return;
    }

    e.facing = dir;
    /* hold the middle distance: back off when crowded, close when too far */
    if (dist < 80) e.vx = -dir * e.speed;
    else if (dist > range) e.vx = dir * e.speed;
    else {
      e.vx *= 0.8;
      if (e.attackCd <= 0 && Math.abs(dy) < 90 && canSee(level, e, p)) {
        e.state = 'windup';
        e.timer = e.def.windup || 0.7;
      }
    }
    if (!floorAhead(level, e, Math.sign(e.vx) || 1)) e.vx = 0;
  }

  function aiCaster(world, e, dt, dist, dir, dy) {
    const p = world.player;
    const level = world.level;

    if (e.state === 'blink') {
      e.timer -= dt;
      if (e.timer <= 0) {
        e.state = 'idle';
        e.attackCd = 0.8;
        const away = -dir * (90 + rand() * 50);
        const tx = e.x + away;
        if (!blocked(level, tx, e.y, e.w, e.h, true, null)) {
          puff(world, e.x, e.y - e.h / 2, '#ff4fd8', 14);
          e.x = tx;
          puff(world, e.x, e.y - e.h / 2, '#ff4fd8', 14);
        }
      }
      return;
    }

    if (e.state === 'windup') {
      e.timer -= dt;
      e.vx = 0;
      e.facing = dir;
      if (e.timer <= 0) {
        e.state = 'idle';
        e.attackCd = 2.0 + rand();
        const ang = Math.atan2((p.y - p.h * 0.5) - (e.y - e.h * 0.6), p.x - e.x);
        spawnProjectile(world, {
          from: 'enemy', kind: 'orb', x: e.x + dir * 8, y: e.y - e.h * 0.6,
          vx: Math.cos(ang) * 150, vy: Math.sin(ang) * 150,
          life: 3.2, dmg: e.dmg, homing: 1.6
        });
        sfx('bolt');
      }
      return;
    }

    if (!e.aggro || p.dead) {
      e.vx = 0;
      return;
    }

    e.facing = dir;
    if (dist < 70 && e.attackCd <= 0) {
      e.state = 'blink';
      e.timer = 0.25;
      return;
    }
    if (dist > (e.def.range || 220)) e.vx = dir * e.speed;
    else {
      e.vx *= 0.85;
      if (e.attackCd <= 0 && Math.abs(dy) < 110) {
        e.state = 'windup';
        e.timer = e.def.windup || 0.9;
      }
    }
  }

  /* Bats hover, pull back, then dive. The pull-back is the tell — without one
   * a flyer is just unavoidable contact damage, and the rule here is that every
   * hit you take is a hit you could have read. */
  function aiFlyer(world, e, dt, dist, dir, dy) {
    const p = world.player;
    const dive = e.speed * 2.8;

    if (e.state === 'windup') {
      e.timer -= dt;
      /* drift back and up, winding the dive */
      e.vx += (-Math.sign(p.x - e.x) * e.speed * 0.5 - e.vx) * 4 * dt;
      e.vy += (-30 - e.vy) * 4 * dt;
      if (e.timer <= 0) {
        const ang = Math.atan2((p.y - p.h * 0.5) - (e.y - e.h / 2), p.x - e.x);
        e.vx = Math.cos(ang) * dive;
        e.vy = Math.sin(ang) * dive;
        e.state = 'dive';
        e.timer = 0.42;
        e.diveHit = false;
      }
      return;
    }

    if (e.state === 'dive') {
      e.timer -= dt;
      if (!e.diveHit && hitBox(e, p, 2)) {
        e.diveHit = true;
        hurtPlayer(world, e.dmg, 'melee', e);
        e.vx = -Math.sign(p.x - e.x) * 200;
        e.vy = -110;
      }
      if (e.timer <= 0 || e.hitWall) {
        e.state = 'hover';
        e.attackCd = 1.1 + rand() * 0.6;
      }
      return;
    }

    if (!e.aggro || p.dead) {
      e.vx = Math.cos(e.anim * 1.4) * e.speed * 0.5;
      e.vy = Math.sin(e.anim * 2.2) * 40;
      e.facing = Math.sign(e.vx) || e.facing;
      return;
    }

    /* hover at about head height, close enough to threaten */
    const targetY = (p.y - p.h / 2) - 6 + Math.sin(e.anim * 3) * 8;
    const keep = 34;                       // stand off a little between dives
    const wantX = p.x - Math.sign(p.x - e.x) * keep;
    e.vx += (Math.sign(wantX - e.x) * e.speed * 0.8 - e.vx) * 3 * dt;
    e.vy += (Math.sign(targetY - (e.y - e.h / 2)) * e.speed * 0.7 - e.vy) * 3 * dt;
    e.facing = Math.sign(p.x - e.x) || e.facing;
    e.state = 'hover';

    if (dist < 76 && e.attackCd <= 0) {
      e.state = 'windup';
      e.timer = 0.34;
    }
    void dy;
    void dir;
  }

  function aiSlammer(world, e, dt, dist, dir, dy) {
    const p = world.player;
    const level = world.level;

    if (e.state === 'windup') {
      e.timer -= dt;
      e.vx *= 0.6;
      if (e.timer <= 0) {
        e.state = 'strike';
        e.timer = 0.2;
        blast(world, e.x + e.facing * 14, e.y - 6, 44, { dmg: e.dmg, knock: 320 }, 'enemy');
        world.shake = Math.max(world.shake, 6);
        sfx('explode');
      }
      return;
    }
    if (e.state === 'strike') {
      e.timer -= dt;
      e.vx = 0;
      if (e.timer <= 0) {
        e.state = 'chase';
        e.attackCd = 1.4;
      }
      return;
    }

    if (!e.aggro || p.dead) {
      if (wallAhead(level, e, e.patrol) || !floorAhead(level, e, e.patrol)) e.patrol *= -1;
      e.facing = e.patrol;
      e.vx = e.patrol * e.speed * 0.4;
      return;
    }

    e.facing = dir;
    e.vx = dir * e.speed;
    if (dist < (e.def.reach || 28) + e.w / 2 && Math.abs(dy) < 34 && e.attackCd <= 0) {
      e.state = 'windup';
      e.timer = e.def.windup || 0.75;
    }
  }

  /* =========================================================================
   *  THE BOSSES
   * ====================================================================== */
  function updateBoss(world, b, dt) {
    const p = world.player;
    const level = world.level;
    b.anim += dt;
    b.hurtT = Math.max(0, b.hurtT - dt);

    const st = CB.tickStatuses(b, dt, {});
    if (st.damage > 0) damageEnemy(world, b, st.damage, { silent: true, fromStatus: true });
    if (b.dead) return;

    if (b.phase === 1 && b.hp < b.maxHp * 0.5) {
      b.phase = 2;
      b.speed *= 1.35;
      world.shake = 10;
      sfx('roar');
      floatText(world, b.x, b.y - b.h - 10, 'ENRAGED', '#ff3b5c');
      ring(world, b.x, b.y - b.h / 2, 90, '#ff3b5c');
    }

    const dist = Math.hypot(p.x - b.x, p.y - b.y);
    const dir = Math.sign(p.x - b.x) || 1;

    switch (b.state) {
      case 'intro':
        b.timer -= dt;
        b.vx = 0;
        if (b.timer <= 0) b.state = 'idle';
        break;

      case 'idle': {
        b.timer -= dt;
        b.facing = dir;
        b.vx = dist > 60 ? dir * b.speed * 0.5 : 0;
        if (b.timer <= 0) {
          const moves = b.def.moves.slice();
          if (b.phase < 2) {
            const drop = moves.indexOf('rain');
            if (drop >= 0) moves.splice(drop, 1);
          }
          b.move = moves[Math.floor(rand() * moves.length)];
          b.state = 'telegraph';
          b.timer = b.move === 'slam' ? 0.5 : 0.62;
        }
        break;
      }

      case 'telegraph':
        b.timer -= dt;
        b.vx *= 0.85;
        b.facing = dir;
        if (b.timer <= 0) startBossMove(world, b, dir);
        break;

      case 'charge':
        b.timer -= dt;
        b.vx = b.facing * b.speed * 3.2;
        if (hitBox(b, p, 10)) {
          hurtPlayer(world, b.dmg, 'melee', b);
          b.timer = Math.min(b.timer, 0.1);
        }
        if (b.hitWall || b.timer <= 0) {
          if (b.hitWall) {
            /* it has run itself into the wall: this is your window */
            world.shake = 8;
            puff(world, b.x + b.facing * 14, b.y - 10, '#caa', 16);
            stagger(world, b, 1.7);
          } else {
            b.state = 'idle';
            b.timer = 0.9 - 0.3 * (b.phase - 1);
          }
        }
        break;

      case 'slam':
        b.timer -= dt;
        if (b.onGround && b.vy === 0 && b.timer < 0.6) {
          blast(world, b.x, b.y - 8, 70, { dmg: b.dmg, knock: 420 }, 'enemy');
          shockwave(world, b.x, b.y, -1);
          shockwave(world, b.x, b.y, 1);
          world.shake = 10;
          sfx('explode');
          /* the weight of the slam leaves it planted for a beat */
          stagger(world, b, 1.2);
        } else if (b.timer <= 0) {
          b.state = 'idle';
          b.timer = 0.8;
        }
        break;

      case 'fan':
        b.timer -= dt;
        b.vx = 0;
        if (b.timer <= 0) {
          b.state = 'idle';
          b.timer = 1.1;
        }
        break;

      case 'dash':
        b.timer -= dt;
        b.vx = b.facing * b.speed * 4.0;
        if (hitBox(b, p, 8)) hurtPlayer(world, b.dmg * 0.8, 'melee', b);
        if (b.hitWall) {
          puff(world, b.x + b.facing * 12, b.y - 12, '#ffe600', 12);
          stagger(world, b, 1.4);       // same rule as a charge: a wall is a mistake
        } else if (b.timer <= 0) {
          b.state = 'idle';
          b.timer = 0.7;
        }
        break;

      case 'rain':
        b.timer -= dt;
        b.vx = 0;
        b.rainT = (b.rainT || 0) - dt;
        if (b.rainT <= 0) {
          b.rainT = 0.22;
          const x = 40 + rand() * (level.w * TILE - 80);
          spawnProjectile(world, {
            from: 'enemy', kind: 'shard', x: x, y: 40,
            vx: 0, vy: 230, life: 4, dmg: Math.round(b.dmg * 0.6), gravity: true
          });
        }
        if (b.timer <= 0) {
          b.state = 'idle';
          b.timer = 1.0;
        }
        break;

      case 'summon':
        b.timer -= dt;
        b.vx = 0;
        if (b.timer <= 0) {
          b.state = 'idle';
          b.timer = 1.0;
        }
        break;

      /* Open. This is what a boss fight is for: read the move, survive it, and
       * take the moment it costs them. */
      case 'stagger':
        b.timer -= dt;
        b.vx *= 0.85;
        b.vulnerable = true;
        if (b.timer <= 0) {
          b.vulnerable = false;
          b.state = 'idle';
          b.timer = 0.5;
        }
        break;
    }

    b.vy = Math.min(MAX_FALL, b.vy + GRAVITY * dt);
    moveBody(level, b, dt);
  }

  function stagger(world, b, seconds) {
    b.state = 'stagger';
    b.timer = seconds;
    b.vulnerable = true;
    b.vx *= 0.3;
    floatText(world, b.x, b.y - b.h - 12, 'STAGGERED', '#ffe600', 1.15);
    sfx('parry');
  }

  function startBossMove(world, b, dir) {
    const p = world.player;
    b.facing = dir;

    switch (b.move) {
      case 'charge':
        b.state = 'charge';
        b.timer = 1.0;
        sfx('roar');
        break;

      case 'slam':
        b.state = 'slam';
        b.timer = 1.4;
        b.vy = -520;
        b.vx = Math.sign(p.x - b.x) * 150;
        break;

      case 'fan': {
        b.state = 'fan';
        b.timer = 0.6;
        const shots = b.phase === 2 ? 7 : 5;
        for (let i = 0; i < shots; i++) {
          const ang = Math.atan2((p.y - p.h / 2) - (b.y - b.h / 2), p.x - b.x) + (i - (shots - 1) / 2) * 0.22;
          spawnProjectile(world, {
            from: 'enemy', kind: 'orb', x: b.x, y: b.y - b.h * 0.6,
            vx: Math.cos(ang) * 240, vy: Math.sin(ang) * 240,
            life: 2.6, dmg: Math.round(b.dmg * 0.7)
          });
        }
        sfx('bolt');
        break;
      }

      case 'dash':
        b.state = 'dash';
        b.timer = 0.42;
        puff(world, b.x, b.y - b.h / 2, '#ffe600', 12);
        break;

      case 'rain':
        b.state = 'rain';
        b.timer = 2.6;
        sfx('roar');
        break;

      case 'summon': {
        b.state = 'summon';
        b.timer = 0.8;
        const pool = b.id === 'warden' ? ['zombie', 'runner'] : ['shielder', 'caster', 'runner'];
        const n = b.phase === 2 ? 3 : 2;
        for (let i = 0; i < n; i++) {
          const id = pool[Math.floor(rand() * pool.length)];
          const def = CONTENT.ENEMY[id];
          const stats = CB.enemyStats(def, world.level.depth, world.bossCells || 0);
          const spawnX = b.x + (i - n / 2) * 34;
          const enemy = makeEnemy({ id: id, pos: { x: spawnX, y: b.y }, hp: stats.hp, dmg: stats.dmg });
          enemy.aggro = true;
          world.enemies.push(enemy);
          puff(world, spawnX, b.y - 10, '#ff3b5c', 14);
        }
        sfx('explode');
        break;
      }

      default:
        b.state = 'idle';
        b.timer = 0.8;
    }
  }

  /* A wave that travels along the ground — the tell for "jump now". */
  function shockwave(world, x, y, dir) {
    spawnProjectile(world, {
      from: 'enemy', kind: 'wave', x: x + dir * 16, y: y - 6,
      vx: dir * 260, vy: 0, life: 1.6, dmg: 12, ignoreWalls: false
    });
  }

  function hitBox(a, b, pad) {
    return (
      Math.abs(a.x - b.x) < (a.w + b.w) / 2 + (pad || 0) &&
      Math.abs((a.y - a.h / 2) - (b.y - b.h / 2)) < (a.h + b.h) / 2 + (pad || 0)
    );
  }

  /* =========================================================================
   *  DAMAGE TO ENEMIES
   * ====================================================================== */
  function hitEnemy(world, e, weapon, ctx) {
    const p = world.player;
    const res = CB.hitDamage(weapon, p, Object.assign({ target: e }, ctx || {}));
    damageEnemy(world, e, res.dmg, {
      crit: res.crit,
      knock: weapon.knock,
      knockDir: (ctx && ctx.knockDir) || Math.sign(e.x - p.x) || 1,
      behind: ctx && ctx.behind
    });
    for (const s of weapon.onHit || []) {
      if (s.chance && rand() > s.chance) continue;
      CB.applyStatus(e, s.status, s.dur);
    }
    if (weapon.lifeOnHit) healPlayer(world, weapon.lifeOnHit);
    if (p.leech) healPlayer(world, p.leech);
    onPlayerLandedHit(world);
  }

  /* Everything that keys off "you connected": mutations, and the recovery of
   * health you just lost. */
  function onPlayerLandedHit(world) {
    const p = world.player;
    p.lastHitLanded = p.time;
    if (CB.hasMutation(p.mutations, 'scheme')) {
      for (let i = 0; i < 2; i++) p.skillCd[i] = Math.max(0, p.skillCd[i] - 0.6);
    }
    if (CB.hasMutation(p.mutations, 'recovery') && p.recoveryDebt > 0 && p.time < (p.recoveryUntil || 0)) {
      healPlayer(world, Math.max(1, Math.round(p.recoveryDebt / 3)));
      p.recoveryDebt = 0;
    }
  }

  function damageEnemy(world, e, dmg, opts) {
    const options = opts || {};
    if (e.dead) return;

    /* A shieldbearer's shield is the puzzle: go round it. knockDir points away
     * from whoever landed the hit, so the attacker stands on -knockDir; if that
     * is the side the enemy is facing, the shield is in the way. */
    let amount = dmg;
    let blockedHit = false;
    if (e.ai === 'shielder' && !e.shieldBroken && !options.fromStatus && !options.behind && !options.aoe) {
      const attackerSide = -(options.knockDir || 1);
      if (attackerSide === e.facing) {
        e.shieldHp -= dmg;
        if (e.shieldHp <= 0) {
          /* the shield goes, and the enemy reels */
          e.shieldBroken = true;
          CB.applyStatus(e, 'stun', 1.4);
          floatText(world, e.x, e.y - e.h - 10, 'SHIELD BROKEN', '#ffe600', 1.2);
          puff(world, e.x + e.facing * 8, e.y - e.h / 2, '#cfd6e4', 14);
          sfx('parry');
        } else {
          amount = Math.max(1, Math.round(amount * 0.2));
          blockedHit = true;
        }
      }
    }

    /* a staggered enemy is wide open */
    if (e.vulnerable && !options.fromStatus) amount = Math.round(amount * 1.6);

    e.hp -= amount;
    e.hurtT = 0.12;

    if (!options.silent) {
      sfx(blockedHit ? 'block' : options.crit ? 'crit' : 'hit');
      world.freeze = Math.max(world.freeze, options.crit ? 0.07 : 0.035);
      world.shake = Math.max(world.shake, options.crit ? 5 : 2);
      blood(world, e.x, e.y - e.h / 2, options.crit ? 14 : 7, blockedHit ? '#8fa7c4' : '#ff3b5c');
      floatText(
        world, e.x, e.y - e.h - 2,
        (blockedHit ? 'BLOCKED ' : '') + Math.round(amount),
        options.crit ? '#ffe600' : blockedHit ? '#8fa7c4' : '#ffffff',
        options.crit ? 1.35 : 1
      );
    }

    if (options.knock && e.kind !== 'boss' && !blockedHit) {
      const k = options.knock / (e.elite ? 2.2 : 1);
      e.vx = (options.knockDir || 1) * k;
      if (!e.flies) e.vy = Math.min(e.vy, -k * 0.32);
    }

    if (e.hp <= 0) killEnemy(world, e);
  }

  function killEnemy(world, e) {
    const p = world.player;
    e.dead = true;
    e.fade = 0;
    e.vx = 0;
    blood(world, e.x, e.y - e.h / 2, 18, '#ff3b5c');

    /* cells and gold, scattered */
    const cells = e.cells;
    for (let i = 0; i < cells; i++) spawnDrop(world, e.x, e.y - e.h / 2, 'cell', 1);
    if (e.gold) spawnDrop(world, e.x, e.y - e.h / 2, 'gold', e.gold);
    if (rand() < 0.06) spawnDrop(world, e.x, e.y - e.h / 2, 'heart', Math.round(p.maxHp * 0.12));
    if (e.hasKey) spawnDrop(world, e.x, e.y - e.h / 2, 'key', 1);

    if (CB.hasMutation(p.mutations, 'frenzy')) {
      p.frenzy = Math.min(6, p.frenzy + 1);
      p.frenzyTimer = 5;
    }
    if (CB.hasMutation(p.mutations, 'predator')) p.speedBuff = 4;

    if (p.cursed) {
      p.curseKills++;
      if (p.curseKills >= 10) {
        p.cursed = false;
        floatText(world, p.x, p.y - 30, 'CURSE LIFTED', '#2fe6c8');
        sfx('levelup');
      }
    }

    world.kills = (world.kills || 0) + 1;
    if (e.kind === 'boss') {
      world.events.push({ type: 'boss_killed' });
      world.shake = 16;
      sfx('win');
      for (let i = 0; i < 40; i++) spawnDrop(world, e.x + (rand() - 0.5) * 40, e.y - 20, 'cell', 1);
    } else {
      world.events.push({ type: 'enemy_killed', id: e.id });
    }
  }

  /* =========================================================================
   *  PROJECTILES
   * ====================================================================== */
  function spawnProjectile(world, opts) {
    world.projectiles.push({
      from: opts.from,
      kind: opts.kind,
      x: opts.x, y: opts.y,
      vx: opts.vx, vy: opts.vy,
      life: opts.life != null ? opts.life : 2,
      pierce: opts.pierce || 0,
      weapon: opts.weapon || null,
      skill: opts.skill || null,
      dmg: opts.dmg || 0,
      radius: opts.radius || 0,
      gravity: !!opts.gravity,
      homing: opts.homing || 0,
      status: opts.status || null,
      hit: new Set(),
      anim: 0
    });
  }

  function updateProjectiles(world, dt) {
    const p = world.player;
    const level = world.level;

    for (let i = world.projectiles.length - 1; i >= 0; i--) {
      const pr = world.projectiles[i];
      pr.life -= dt;
      pr.anim += dt;

      if (pr.gravity) pr.vy += GRAVITY * 0.75 * dt;
      if (pr.homing && !p.dead) {
        const ang = Math.atan2((p.y - p.h / 2) - pr.y, p.x - pr.x);
        const speed = Math.hypot(pr.vx, pr.vy);
        const cur = Math.atan2(pr.vy, pr.vx);
        let diff = ang - cur;
        while (diff > Math.PI) diff -= Math.PI * 2;
        while (diff < -Math.PI) diff += Math.PI * 2;
        const next = cur + Math.sign(diff) * Math.min(Math.abs(diff), pr.homing * dt);
        pr.vx = Math.cos(next) * speed;
        pr.vy = Math.sin(next) * speed;
      }

      const steps = Math.max(1, Math.ceil(Math.hypot(pr.vx, pr.vy) * dt / 6));
      let done = false;
      for (let s = 0; s < steps && !done; s++) {
        pr.x += pr.vx * dt / steps;
        pr.y += pr.vy * dt / steps;

        const tx = Math.floor(pr.x / TILE);
        const ty = Math.floor(pr.y / TILE);
        const tile = LG.at(level, tx, ty);
        if (tile === T.SOLID || tile === T.SPIKE) {
          impact(world, pr);
          world.projectiles.splice(i, 1);
          done = true;
          break;
        }

        if (pr.from === 'player') {
          const targets = world.enemies.concat(world.boss && !world.boss.dead ? [world.boss] : []);
          for (const e of targets) {
            if (e.dead || pr.hit.has(e)) continue;
            if (Math.abs(pr.x - e.x) > e.w / 2 + 4 || Math.abs(pr.y - (e.y - e.h / 2)) > e.h / 2 + 4) continue;
            pr.hit.add(e);
            if (pr.weapon) {
              hitEnemy(world, e, pr.weapon, { distance: Math.hypot(pr.x - p.x, pr.y - p.y), knockDir: Math.sign(pr.vx) || 1 });
            } else if (pr.skill) {
              impact(world, pr);
              world.projectiles.splice(i, 1);
              done = true;
              break;
            }
            if (pr.pierce > 0) pr.pierce--;
            else {
              impact(world, pr);
              world.projectiles.splice(i, 1);
              done = true;
              break;
            }
          }
        } else if (!p.dead) {
          if (Math.abs(pr.x - p.x) < p.w / 2 + 4 && Math.abs(pr.y - (p.y - p.h / 2)) < p.h / 2 + 4) {
            if (pr.radius) {
              impact(world, pr);
            } else {
              hurtPlayer(world, pr.dmg, 'ranged', { x: pr.x, y: pr.y });
              if (pr.status) CB.applyStatus(p, pr.status.status, pr.status.dur);
            }
            world.projectiles.splice(i, 1);
            done = true;
            break;
          }
        }
      }
      if (done) continue;

      if (pr.life <= 0) {
        impact(world, pr);
        world.projectiles.splice(i, 1);
      }
    }
  }

  function impact(world, pr) {
    if (pr.skill) {
      blast(world, pr.x, pr.y, pr.skill.radius || 40, pr.skill, 'player');
      sfx('explode');
      world.shake = Math.max(world.shake, 5);
      return;
    }
    if (pr.radius) {
      blast(world, pr.x, pr.y, pr.radius, { dmg: pr.dmg, knock: 260 }, pr.from === 'player' ? 'player' : 'enemy');
      sfx('explode');
      world.shake = Math.max(world.shake, 4);
      return;
    }
    puff(world, pr.x, pr.y, pr.kind === 'spit' ? '#8ddb3a' : '#ffd9a0', 5);
  }

  /* ------------------------------------------------- turrets, traps, orbs */
  function updateAllies(world, dt) {
    const level = world.level;
    const p = world.player;

    for (let i = world.turrets.length - 1; i >= 0; i--) {
      const t = world.turrets[i];
      t.life -= dt;
      t.cd -= dt;
      t.vy = Math.min(MAX_FALL, t.vy + GRAVITY * dt);
      moveBody(level, t, dt);
      if (t.cd <= 0) {
        const target = nearestEnemy(world, t.x, t.y, 220);
        if (target) {
          t.cd = t.skill.rate;
          const ang = Math.atan2((target.y - target.h / 2) - t.y, target.x - t.x);
          spawnProjectile(world, {
            from: 'player', kind: 'bolt', x: t.x, y: t.y - 6,
            vx: Math.cos(ang) * 440, vy: Math.sin(ang) * 440, life: 1.4,
            weapon: CB.makeWeapon({ id: 'turret', name: 'Turret', color: t.skill.color, kind: 'shoot', dmg: t.skill.dmg, rate: 1, knock: 40 }, null)
          });
          sfx('bow');
        }
      }
      if (t.life <= 0) {
        puff(world, t.x, t.y - 6, '#b46bff', 10);
        world.turrets.splice(i, 1);
      }
    }

    for (let i = world.traps.length - 1; i >= 0; i--) {
      const tr = world.traps[i];
      tr.life -= dt;
      if (tr.armed) {
        const target = nearestEnemy(world, tr.x, tr.y, 26);
        if (target) {
          tr.armed = false;
          tr.life = Math.min(tr.life, 0.4);
          damageEnemy(world, target, Math.round(tr.skill.dmg * CB.scaleFor(tr.skill.color, p.stats)), { knock: 0 });
          if (tr.skill.status) CB.applyStatus(target, tr.skill.status.status, tr.skill.status.dur);
          sfx('hit');
        }
      }
      if (tr.life <= 0) world.traps.splice(i, 1);
    }

    for (let i = world.orbs.length - 1; i >= 0; i--) {
      const o = world.orbs[i];
      o.life -= dt;
      o.spin += dt * 12;
      o.x += o.vx * dt;
      if (blocked(level, o.x, o.y + 8, 10, 10, true, null)) o.vx *= -1;
      for (const e of world.enemies.concat(world.boss && !world.boss.dead ? [world.boss] : [])) {
        if (e.dead) continue;
        if (Math.abs(e.x - o.x) > e.w / 2 + 10 || Math.abs((e.y - e.h / 2) - o.y) > e.h / 2 + 10) continue;
        const last = o.hitCd.get(e) || 0;
        if (world.time - last < 0.3) continue;
        o.hitCd.set(e, world.time);
        damageEnemy(world, e, Math.round(o.skill.dmg * CB.scaleFor(o.skill.color, p.stats)), { knock: 80, knockDir: Math.sign(o.vx) });
      }
      if (o.life <= 0) world.orbs.splice(i, 1);
    }
  }

  function nearestEnemy(world, x, y, range) {
    let best = null;
    let bestD = range;
    for (const e of world.enemies) {
      if (e.dead) continue;
      const d = Math.hypot(e.x - x, (e.y - e.h / 2) - y);
      if (d < bestD) { bestD = d; best = e; }
    }
    if (world.boss && !world.boss.dead) {
      const d = Math.hypot(world.boss.x - x, world.boss.y - y);
      if (d < bestD) best = world.boss;
    }
    return best;
  }

  /* =========================================================================
   *  DROPS — cells, gold, health, keys
   * ====================================================================== */
  function spawnDrop(world, x, y, type, value) {
    world.drops.push({
      type: type, value: value,
      x: x, y: y,
      vx: (rand() - 0.5) * 140,
      vy: -120 - rand() * 120,
      w: 6, h: 6,
      life: 26,
      settle: 0.25,
      anim: rand() * 6.28
    });
  }

  function updateDrops(world, dt) {
    const p = world.player;
    const level = world.level;

    for (let i = world.drops.length - 1; i >= 0; i--) {
      const d = world.drops[i];
      d.life -= dt;
      d.anim += dt;
      d.settle -= dt;

      const dist = Math.hypot(p.x - d.x, (p.y - p.h / 2) - d.y);
      if (d.settle <= 0 && dist < 110 && !p.dead) {
        /* cells chase you down, the way they do in Dead Cells */
        const ang = Math.atan2((p.y - p.h / 2) - d.y, p.x - d.x);
        const pull = d.type === 'cell' ? 900 : 700;
        d.vx += Math.cos(ang) * pull * dt;
        d.vy += Math.sin(ang) * pull * dt;
        d.vx *= 0.94;
        d.vy *= 0.94;
      } else {
        d.vy += GRAVITY * 0.7 * dt;
        d.vx *= 0.99;
      }

      const body = { x: d.x, y: d.y + 3, w: 5, h: 5, vx: d.vx, vy: d.vy, onGround: false };
      moveBody(level, body, dt);
      d.x = body.x;
      d.y = body.y - 3;
      d.vx = body.vx;
      d.vy = body.vy;
      if (body.onGround) d.vx *= 0.8;

      if (dist < 14 && !p.dead) {
        collect(world, d);
        world.drops.splice(i, 1);
        continue;
      }
      if (d.life <= 0) world.drops.splice(i, 1);
    }
  }

  function collect(world, d) {
    const p = world.player;
    switch (d.type) {
      case 'cell':
        p.cells += d.value;
        sfx('cell');
        break;
      case 'gold':
        p.gold += d.value;
        sfx('gold');
        break;
      case 'heart':
        healPlayer(world, CB.healAmount(p, d.value));
        sfx('pickup');
        break;
      case 'key':
        p.keys += 1;
        floatText(world, p.x, p.y - 30, 'KEY', '#ffe600');
        sfx('pickup');
        break;
    }
  }

  /* =========================================================================
   *  EFFECTS — the juice
   * ====================================================================== */
  function particle(world, opts) {
    world.particles.push({
      x: opts.x, y: opts.y,
      vx: opts.vx, vy: opts.vy,
      life: opts.life, total: opts.life,
      color: opts.color,
      size: opts.size || 2,
      gravity: opts.gravity != null ? opts.gravity : 1,
      glow: !!opts.glow
    });
  }

  function blood(world, x, y, n, color) {
    for (let i = 0; i < n; i++) {
      const a = rand() * Math.PI * 2;
      const s = 40 + rand() * 190;
      particle(world, {
        x: x, y: y, vx: Math.cos(a) * s, vy: Math.sin(a) * s - 40,
        life: 0.4 + rand() * 0.5, color: color, size: 1 + rand() * 2.2
      });
    }
  }

  function puff(world, x, y, color, n) {
    for (let i = 0; i < n; i++) {
      const a = rand() * Math.PI * 2;
      const s = 20 + rand() * 90;
      particle(world, {
        x: x, y: y, vx: Math.cos(a) * s, vy: Math.sin(a) * s,
        life: 0.3 + rand() * 0.4, color: color, size: 1 + rand() * 2,
        gravity: 0.1, glow: true
      });
    }
  }

  function ring(world, x, y, radius, color) {
    world.rings.push({ x: x, y: y, r: 6, max: radius, life: 0.34, total: 0.34, color: color });
  }

  function slashEffect(world, x, y, dir, color) {
    world.slashes.push({ x: x, y: y, dir: dir, life: 0.16, total: 0.16, color: color });
  }

  function floatText(world, x, y, text, color, scale) {
    world.texts.push({
      x: x + (rand() - 0.5) * 8, y: y,
      text: String(text), color: color || '#fff',
      life: 0.8, total: 0.8, scale: scale || 1,
      vy: -34
    });
  }

  function updateEffects(world, dt) {
    for (let i = world.particles.length - 1; i >= 0; i--) {
      const pt = world.particles[i];
      pt.life -= dt;
      pt.vy += GRAVITY * 0.55 * pt.gravity * dt;
      pt.x += pt.vx * dt;
      pt.y += pt.vy * dt;
      pt.vx *= 0.98;
      if (pt.life <= 0) world.particles.splice(i, 1);
    }
    for (let i = world.rings.length - 1; i >= 0; i--) {
      const r = world.rings[i];
      r.life -= dt;
      r.r = r.max * (1 - r.life / r.total);
      if (r.life <= 0) world.rings.splice(i, 1);
    }
    for (let i = world.slashes.length - 1; i >= 0; i--) {
      const s = world.slashes[i];
      s.life -= dt;
      if (s.life <= 0) world.slashes.splice(i, 1);
    }
    for (let i = world.texts.length - 1; i >= 0; i--) {
      const t = world.texts[i];
      t.life -= dt;
      t.y += t.vy * dt;
      t.vy *= 0.92;
      if (t.life <= 0) world.texts.splice(i, 1);
    }
  }

  const API = {
    setRandom: setRandom,
    P: P,
    GRAVITY: GRAVITY,
    blocked: blocked,
    moveBody: moveBody,
    touchingSpikes: touchingSpikes,
    makePlayer: makePlayer,
    updatePlayer: updatePlayer,
    hurtPlayer: hurtPlayer,
    healPlayer: healPlayer,
    useSkill: useSkill,
    blast: blast,
    standingOnPlatform: standingOnPlatform,
    aimAngle: aimAngle,

    makeEnemy: makeEnemy,
    makeBoss: makeBoss,
    updateEnemies: updateEnemies,
    updateProjectiles: updateProjectiles,
    updateDrops: updateDrops,
    updateEffects: updateEffects,
    spawnProjectile: spawnProjectile,
    spawnDrop: spawnDrop,
    damageEnemy: damageEnemy,
    hitEnemy: hitEnemy,
    killEnemy: killEnemy,
    nearestEnemy: nearestEnemy,
    floatText: floatText,
    blood: blood,
    puff: puff,
    ring: ring,
    canSee: canSee
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  global.CELLS_ENTITIES = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
