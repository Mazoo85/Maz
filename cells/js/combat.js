/* =============================================================================
 *  NEON CELLS  —  combat mathematics
 *
 *  All the numbers of a fight, with no canvas and no game loop in sight: how a
 *  scroll makes a weapon hit harder, when a hit crits, what bleeding costs per
 *  second, and what each mutation changes. Keeping it separate means the
 *  balance of the game can be tested directly (see cells/tests).
 *
 *  The three colours work like Dead Cells': a weapon is coloured, and only its
 *  own colour's scrolls scale it. Survival also carries your health, which is
 *  why a survival run can stand in the open and a brutality run cannot.
 * ========================================================================== */
(function (global) {
  'use strict';

  const CONTENT = global.CELLS_CONTENT || require('./content.js');

  /* Same indirection as entities.js: one seam for randomness, so a simulated
   * fight can be replayed exactly. */
  let rand = Math.random;

  function setRandom(fn) {
    rand = typeof fn === 'function' ? fn : Math.random;
  }

  const SCALE_PER_POINT = 0.15;   // each scroll is +15% for its colour
  const BASE_HEALTH = 60;
  const HEALTH_PER_SURVIVAL = 11;

  /* ------------------------------------------------------------- scaling */
  function scaleFor(color, stats) {
    const points = Math.max(1, (stats && stats[color]) || 1);
    return 1 + SCALE_PER_POINT * (points - 1);
  }

  function maxHealth(stats, mutations, metaHealth) {
    const survival = Math.max(1, (stats && stats.survival) || 1);
    let hp = BASE_HEALTH + HEALTH_PER_SURVIVAL * (survival - 1);
    if (hasMutation(mutations, 'soldier')) hp += 10;
    hp += metaHealth || 0;
    return Math.round(hp);
  }

  function hasMutation(mutations, id) {
    if (!mutations) return false;
    for (const m of mutations) {
      if ((m && m.id ? m.id : m) === id) return true;
    }
    return false;
  }

  /* ------------------------------------------------------------- weapons
   * A weapon instance is a definition plus a rolled affix, with its numbers
   * already folded together so the game loop never has to think about it.
   */
  function makeWeapon(def, affix) {
    const a = affix || CONTENT.AFFIX.none;
    const onHit = (def.onHit || []).concat(a.onHit || []);
    return {
      id: def.id,
      def: def,
      name: a.name ? a.name + ' ' + def.name : def.name,
      color: def.color,
      kind: def.kind,
      affix: a.id,
      dmg: def.dmg * (a.dmgMult || 1),
      rate: def.rate * (a.rateMult || 1),
      reach: def.reach || 0,
      arc: def.arc || 0,
      combo: def.combo || 1,
      knock: def.knock || 0,
      block: def.block || 0,
      reflect: def.reflect || 0,
      proj: def.proj || null,
      crit: def.crit || null,
      onHit: onHit,
      lifeOnHit: a.lifeOnHit || 0,
      desc: def.desc
    };
  }

  function rollWeapon(rng, def) {
    const affix = rng.weighted(CONTENT.AFFIXES, function (a) { return a.weight; });
    return makeWeapon(def, affix);
  }

  /* ------------------------------------------------------------- crits
   * ctx describes the moment of the hit:
   *   { behind, airborne, distance, target }
   */
  function critApplies(weapon, ctx) {
    const crit = weapon.crit;
    if (!crit) return false;
    const target = (ctx && ctx.target) || {};
    switch (crit.when) {
      case 'backstab':  return !!(ctx && ctx.behind);
      case 'airborne':  return !!(ctx && ctx.airborne);
      case 'close':     return !!(ctx && ctx.distance != null && ctx.distance < 40);
      case 'bleeding':  return hasStatus(target, 'bleed');
      case 'frozen':    return hasStatus(target, 'frozen');
      case 'rooted':    return hasStatus(target, 'rooted');
      case 'stunned':   return hasStatus(target, 'stun');
      default:          return false;
    }
  }

  /* ------------------------------------------------- the damage of one hit
   * player is { stats, mutations, hp, maxHp, buffs } — see entities.js.
   */
  function hitDamage(weapon, player, ctx) {
    const stats = player.stats;
    let dmg = weapon.dmg * scaleFor(weapon.color, stats);

    let crit = critApplies(weapon, ctx);
    if (!crit && player.critChance && rand() < player.critChance) crit = true;
    if (crit) dmg *= (weapon.crit && weapon.crit.mult) || 2.0;

    dmg *= playerDamageMultiplier(player);

    return { dmg: Math.max(1, Math.round(dmg)), crit: crit };
  }

  /* Every mutation and buff that changes how hard the player hits. */
  function playerDamageMultiplier(player) {
    let mult = 1;
    const muts = player.mutations;
    const now = player.time || 0;

    if (hasMutation(muts, 'vengeance') && player.hp <= player.maxHp / 3) mult *= 1.6;
    if (hasMutation(muts, 'combo') && now - (player.lastHitLanded || -99) < 3) mult *= 1.25;
    if (hasMutation(muts, 'tranquil') && now - (player.lastHurt || -99) > 6) mult *= 1.45;
    if (player.buffs && player.buffs.rage) mult *= 1.35;

    return mult;
  }

  /* Attack speed: lower is faster, so Frenzy divides. */
  function attackInterval(weapon, player) {
    let rate = weapon.rate;
    const stacks = (player && player.frenzy) || 0;
    if (stacks) rate /= 1 + 0.08 * stacks;
    return Math.max(0.07, rate);
  }

  /* --------------------------------------------------- damage taken by you */
  function incomingDamage(player, amount, kind) {
    let dmg = amount;
    if (hasMutation(player.mutations, 'armour')) dmg *= 0.85;
    if (hasMutation(player.mutations, 'soldier') && kind === 'trap') dmg *= 0.5;
    if (player.cursed) dmg = Math.max(dmg, 9999);   // a cursed run dies to one hit
    return Math.max(1, Math.round(dmg));
  }

  function healAmount(player, amount) {
    let heal = amount;
    if (hasMutation(player.mutations, 'gastronomy')) heal *= 1.4;
    return Math.round(heal);
  }

  /* ------------------------------------------------------------- statuses */
  function hasStatus(ent, id) {
    return !!(ent && ent.status && ent.status[id] && ent.status[id].t > 0);
  }

  function applyStatus(ent, id, dur, stacks) {
    const def = CONTENT.STATUSES[id];
    if (!def || !ent) return;
    if (ent.statusImmune) return;
    if (!ent.status) ent.status = {};
    const cur = ent.status[id];
    const life = dur || def.dur;
    if (cur && cur.t > 0) {
      cur.t = Math.max(cur.t, life);
      if (def.stacks) cur.stacks = Math.min(def.maxStacks || 5, cur.stacks + (stacks || 1));
    } else {
      ent.status[id] = { t: life, stacks: stacks || 1 };
    }
  }

  /* Runs the clock on every status on one entity and returns the damage it
   * did this frame. held is true while the entity cannot act. */
  function tickStatuses(ent, dt, opts) {
    if (!ent.status) return { damage: 0, held: false };
    const options = opts || {};
    let damage = 0;
    let held = false;

    for (const id of Object.keys(ent.status)) {
      const s = ent.status[id];
      if (!s || s.t <= 0) continue;
      const def = CONTENT.STATUSES[id];
      s.t -= dt;

      let dps = def.dps * (s.stacks || 1);
      if (options.openWounds && (id === 'bleed' || id === 'fire')) dps *= 1.5;
      damage += dps * dt;

      if (def.hold || def.slow === 0) held = true;
      if (s.t <= 0) delete ent.status[id];
    }

    return { damage: damage, held: held };
  }

  function statusSpeedMultiplier(ent) {
    if (hasStatus(ent, 'frozen') || hasStatus(ent, 'stun') || hasStatus(ent, 'rooted')) return 0;
    return 1;
  }

  /* ------------------------------------------------------- enemy scaling */
  /* Health climbs steadily; damage climbs a little faster than linearly. A run
   * that has been collecting scrolls all the way down gains damage *and* health
   * at once, so a purely linear ramp makes the last biomes easier than the
   * middle ones — which is the wrong shape for the end of a run. */
  function enemyScaling(depth, bossCells) {
    const steps = Math.max(0, depth - 1);
    const cells = bossCells || 0;
    return {
      hp: (1 + 0.26 * steps) * (1 + 0.35 * cells),
      dmg: (1 + 0.17 * steps + 0.02 * steps * steps) * (1 + 0.25 * cells)
    };
  }

  function enemyStats(def, depth, bossCells) {
    const scale = enemyScaling(depth, bossCells);
    return {
      hp: Math.round(def.hp * scale.hp),
      dmg: Math.round(def.dmg * scale.dmg)
    };
  }

  const API = {
    setRandom: setRandom,
    SCALE_PER_POINT: SCALE_PER_POINT,
    scaleFor: scaleFor,
    maxHealth: maxHealth,
    hasMutation: hasMutation,
    makeWeapon: makeWeapon,
    rollWeapon: rollWeapon,
    critApplies: critApplies,
    hitDamage: hitDamage,
    playerDamageMultiplier: playerDamageMultiplier,
    attackInterval: attackInterval,
    incomingDamage: incomingDamage,
    healAmount: healAmount,
    hasStatus: hasStatus,
    applyStatus: applyStatus,
    tickStatuses: tickStatuses,
    statusSpeedMultiplier: statusSpeedMultiplier,
    enemyScaling: enemyScaling,
    enemyStats: enemyStats
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  global.CELLS_COMBAT = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
