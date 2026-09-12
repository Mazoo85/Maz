/* =============================================================================
 *  NEON CELLS  —  content tables
 *
 *  Everything the game is *made of*, with no logic in it: the three stat
 *  colours, the weapons and how they swing, the skills, the mutations, the
 *  enemy roster and the biomes they live in.
 *
 *  Keeping it all data means the balance of the game can be read in one file,
 *  and the tests can check the whole roster for coherence (every weapon has a
 *  colour that exists, every biome spawns enemies that exist, and so on)
 *  without booting a browser.
 * ========================================================================== */
(function (global) {
  'use strict';

  /* ------------------------------------------------------------- the colours
   * Dead Cells' three scroll colours. Each one scales the weapons and skills
   * that belong to it, and survival also carries your health.
   */
  const COLORS = {
    brutality: { id: 'brutality', name: 'Brutality', short: 'BRU', hex: '#ff3b5c', glow: 'rgba(255,59,92,.55)' },
    tactics:   { id: 'tactics',   name: 'Tactics',   short: 'TAC', hex: '#b46bff', glow: 'rgba(180,107,255,.55)' },
    survival:  { id: 'survival',  name: 'Survival',  short: 'SUR', hex: '#2fe6c8', glow: 'rgba(47,230,200,.55)' }
  };
  const COLOR_IDS = ['brutality', 'tactics', 'survival'];

  /* --------------------------------------------------------------- statuses
   * dps      damage per second while it lasts
   * slow     movement multiplier
   * hold     the victim cannot act at all
   */
  const STATUSES = {
    bleed:  { name: 'Bleeding', dps: 9,  dur: 3.0, hex: '#ff3b5c', stacks: true },
    fire:   { name: 'Burning',  dps: 14, dur: 2.2, hex: '#ff8a1e', stacks: false, spreads: true },
    poison: { name: 'Poisoned', dps: 7,  dur: 4.0, hex: '#8ddb3a', stacks: true },
    frozen: { name: 'Frozen',   dps: 0,  dur: 2.0, hex: '#7fd8ff', stacks: false, hold: true },
    rooted: { name: 'Rooted',   dps: 0,  dur: 2.4, hex: '#c9a227', stacks: false, slow: 0 },
    stun:   { name: 'Stunned',  dps: 0,  dur: 0.9, hex: '#ffe600', stacks: false, hold: true }
  };

  /* ---------------------------------------------------------------- weapons
   * kind tells the combat code how to swing it:
   *   swing   an arc in front of you, chained into a combo
   *   thrust  a long thin poke, reaches past the arc weapons
   *   slam    slow, wide, staggering
   *   shoot   fires a projectile (ranged)
   *   shield  holds a block / parry window instead of attacking
   *
   * crit is the Dead Cells hook that gives every weapon a *way to play it*:
   *   backstab  hitting an enemy in its back
   *   frozen / rooted / stunned / bleeding   hitting one in that state
   *   airborne  hitting while you are off the ground
   *
   * tier 0 weapons are in the starting pool; tier 1+ must be unlocked with
   * cells at the Collector first (see meta.js).
   */
  const WEAPONS = [
    {
      id: 'rusty_sword', name: 'Rusty Sword', color: 'brutality', kind: 'swing', tier: 0,
      dmg: 12, rate: 0.32, reach: 28, arc: 1.45, combo: 3, knock: 130, cost: 0,
      desc: 'Three-hit combo. Nothing fancy, always works.'
    },
    {
      id: 'blood_sword', name: 'Blood Sword', color: 'brutality', kind: 'swing', tier: 1,
      dmg: 10, rate: 0.30, reach: 28, arc: 1.45, combo: 3, knock: 120, cost: 35,
      onHit: [{ status: 'bleed', dur: 3.0 }], crit: { when: 'bleeding', mult: 2.0 },
      desc: 'Makes them bleed, then hits twice as hard while they do.'
    },
    {
      id: 'war_hammer', name: 'War Hammer', color: 'brutality', kind: 'slam', tier: 0,
      dmg: 30, rate: 0.78, reach: 34, arc: 1.9, combo: 1, knock: 330, cost: 0,
      onHit: [{ status: 'stun', dur: 0.9 }], crit: { when: 'stunned', mult: 1.8 },
      desc: 'Slow and staggering. Crits anything already reeling.'
    },
    {
      id: 'twin_daggers', name: 'Twin Daggers', color: 'brutality', kind: 'swing', tier: 1,
      dmg: 7, rate: 0.17, reach: 22, arc: 1.2, combo: 4, knock: 60, cost: 40,
      crit: { when: 'backstab', mult: 3.0 },
      desc: 'Very fast. Triple damage from behind — roll through them first.'
    },
    {
      id: 'flint', name: 'Flint', color: 'brutality', kind: 'swing', tier: 1,
      dmg: 13, rate: 0.38, reach: 27, arc: 1.5, combo: 2, knock: 140, cost: 45,
      onHit: [{ status: 'fire', dur: 2.2 }],
      desc: 'Sets them alight, and fire spreads to whatever they touch.'
    },
    {
      id: 'spear', name: 'Spite Spear', color: 'tactics', kind: 'thrust', tier: 0,
      dmg: 15, rate: 0.42, reach: 52, arc: 0.55, combo: 2, knock: 110, cost: 0,
      desc: 'Outranges everything that has to walk up to you.'
    },
    {
      id: 'bow', name: 'Infantry Bow', color: 'tactics', kind: 'shoot', tier: 0,
      dmg: 11, rate: 0.40, knock: 70, cost: 0,
      proj: { kind: 'arrow', speed: 540, life: 1.2, pierce: 0 },
      crit: { when: 'close', mult: 2.0 },
      desc: 'Crits at point-blank range. Shoot them in the face.'
    },
    {
      id: 'throwing_knives', name: 'Throwing Knives', color: 'tactics', kind: 'shoot', tier: 1,
      dmg: 6, rate: 0.22, knock: 40, cost: 40,
      proj: { kind: 'knife', speed: 620, life: 0.9, pierce: 0, count: 2, spread: 0.10 },
      onHit: [{ status: 'bleed', dur: 2.0 }],
      desc: 'Two knives a throw, and they bleed.'
    },
    {
      id: 'lightning_bolt', name: 'Lightning Bolt', color: 'tactics', kind: 'shoot', tier: 1,
      dmg: 14, rate: 0.55, knock: 30, cost: 55,
      proj: { kind: 'bolt', speed: 900, life: 0.5, pierce: 3 },
      crit: { when: 'frozen', mult: 2.4 },
      desc: 'Passes through a whole corridor of them. Crits the frozen.'
    },
    {
      id: 'shield', name: 'Rampart', color: 'survival', kind: 'shield', tier: 0,
      dmg: 10, rate: 0.5, reach: 24, block: 0.45, reflect: 1.6, knock: 240, cost: 0,
      desc: 'Hold to block. A parry in the first moments throws it back harder.'
    },
    {
      id: 'ice_shield', name: 'Ice Shield', color: 'survival', kind: 'shield', tier: 1,
      dmg: 8, rate: 0.5, reach: 24, block: 0.45, reflect: 1.2, knock: 200, cost: 50,
      onHit: [{ status: 'frozen', dur: 1.6 }], cost_note: 'parry freezes',
      desc: 'A parry freezes whatever hit you solid.'
    },
    {
      id: 'greatsword', name: 'Nutcracker', color: 'survival', kind: 'slam', tier: 1,
      dmg: 26, rate: 0.62, reach: 38, arc: 2.1, combo: 1, knock: 280, cost: 45,
      crit: { when: 'airborne', mult: 1.9 },
      desc: 'Huge arc. Crits when you swing it out of a jump.'
    },
    {
      id: 'whip', name: 'Balanced Blade', color: 'survival', kind: 'thrust', tier: 1,
      dmg: 18, rate: 0.48, reach: 60, arc: 0.4, combo: 1, knock: 150, cost: 50,
      crit: { when: 'rooted', mult: 2.2 },
      desc: 'Enormous reach. Crits anything held in place.'
    }
  ];

  /* ----------------------------------------------------------------- affixes
   * Rolled onto a weapon when it drops, so no two runs hand you the same
   * Rusty Sword. Exactly how Dead Cells makes a starting weapon interesting.
   */
  const AFFIXES = [
    { id: 'none',     name: '',            weight: 34, apply: function () {} },
    { id: 'bleeding', name: 'Bleeding',    weight: 12, onHit: [{ status: 'bleed', dur: 2.5 }] },
    { id: 'searing',  name: 'Searing',     weight: 10, onHit: [{ status: 'fire', dur: 1.8 }] },
    { id: 'venomous', name: 'Venomous',    weight: 10, onHit: [{ status: 'poison', dur: 3.5 }] },
    { id: 'heavy',    name: 'Heavy',       weight: 10, dmgMult: 1.22, rateMult: 1.15 },
    { id: 'swift',    name: 'Swift',       weight: 10, rateMult: 0.80 },
    { id: 'vampiric', name: 'Vampiric',    weight: 7,  lifeOnHit: 1 },
    { id: 'cold',     name: 'Cold-Forged', weight: 7,  onHit: [{ status: 'frozen', dur: 0.9, chance: 0.25 }] }
  ];

  /* ------------------------------------------------------------------ skills */
  const SKILLS = [
    {
      id: 'grenade', name: 'Fire Grenade', color: 'tactics', kind: 'throw', tier: 0,
      cd: 6, dmg: 34, radius: 46, cost: 0, status: { status: 'fire', dur: 2.4 },
      desc: 'Lobbed. Explodes in flame.'
    },
    {
      id: 'frost_blast', name: 'Frost Blast', color: 'tactics', kind: 'blast', tier: 0,
      cd: 9, dmg: 10, radius: 74, cost: 0, status: { status: 'frozen', dur: 2.2 },
      desc: 'Freezes everything around you. Then hit them.'
    },
    {
      id: 'turret', name: 'Crossbow Turret', color: 'tactics', kind: 'turret', tier: 1,
      cd: 15, dmg: 9, life: 16, rate: 1.0, cost: 45,
      desc: 'Bolt it to the floor and let it work.'
    },
    {
      id: 'shockwave', name: 'Shockwave', color: 'brutality', kind: 'blast', tier: 0,
      cd: 8, dmg: 16, radius: 66, knock: 420, cost: 0, status: { status: 'stun', dur: 1.0 },
      desc: 'Throws everything off you, stunned.'
    },
    {
      id: 'wolf_trap', name: 'Wolf Trap', color: 'survival', kind: 'place', tier: 1,
      cd: 7, dmg: 22, life: 20, cost: 40, status: { status: 'rooted', dur: 2.6 },
      desc: 'Dropped at your feet. Holds the first thing that walks in.'
    },
    {
      id: 'tonic', name: 'Vampirism Tonic', color: 'survival', kind: 'buff', tier: 1,
      cd: 24, dur: 7, leech: 3, cost: 55,
      desc: 'For seven seconds, every hit you land heals you.'
    },
    {
      id: 'cleaver', name: 'Death Orb', color: 'brutality', kind: 'orb', tier: 1,
      cd: 12, dmg: 12, life: 6, cost: 50,
      desc: 'A spinning orb that grinds through anything in its path.'
    }
  ];

  /* --------------------------------------------------------------- mutations
   * Picked one-of-three at the end of every level. Each one is read by name in
   * the combat code — see applyMutations in combat.js.
   */
  const MUTATIONS = [
    { id: 'vengeance',  name: 'Vengeance',        color: 'brutality', desc: '+60% damage while below a third of your health.' },
    { id: 'combo',      name: 'Combo',            color: 'brutality', desc: '+25% damage for 3s after you hit something.' },
    { id: 'frenzy',     name: 'Frenzy',           color: 'brutality', desc: 'Each kill gives +8% attack speed for 5s, stacking.' },
    { id: 'predator',   name: 'Predator',         color: 'tactics',   desc: 'A kill grants +25% movement speed for 4s.' },
    { id: 'scheme',     name: 'Scheme',           color: 'tactics',   desc: 'Landing a hit takes 0.6s off your skill cooldowns.' },
    { id: 'tranquil',   name: 'Tranquillity',     color: 'tactics',   desc: '+45% damage if nothing has hurt you for 6s.' },
    { id: 'recovery',   name: 'Recovery',         color: 'survival',  desc: 'Hit something within 3s of being hurt and take a third of it back.' },
    { id: 'gastronomy', name: 'Gastronomy',       color: 'survival',  desc: 'Food and flasks heal 40% more.' },
    { id: 'armour',     name: 'Armadillopack',    color: 'survival',  desc: 'Take 15% less damage, and rolling heals 1.' },
    { id: 'soldier',    name: "Soldier's Resistance", color: 'survival', desc: 'Traps and spikes do half damage; +10 maximum health.' },
    { id: 'acrobat',    name: 'Acrobatipack',     color: 'tactics',   desc: 'A second jump in mid-air.' },
    { id: 'open_wounds', name: 'Open Wounds',     color: 'brutality', desc: 'Bleeding and burning tick 50% harder.' }
  ];

  /* ---------------------------------------------------------------- enemies
   * ai names the behaviour in entities.js. hp/dmg are the biome-1 values;
   * levelgen scales them with depth and with how many Boss Cells you carry.
   */
  const ENEMIES = [
    { id: 'zombie',   name: 'Shambler',      ai: 'walker',   hp: 26,  dmg: 8,  speed: 44,  w: 13, h: 22, cells: 1, gold: 5,  reach: 18, windup: 0.42, hex: '#6fbf5a' },
    { id: 'runner',   name: 'Rabid',         ai: 'walker',   hp: 20,  dmg: 9,  speed: 104, w: 12, h: 19, cells: 1, gold: 6,  reach: 16, windup: 0.24, hex: '#d4e84a', leaps: true },
    { id: 'archer',   name: 'Bone Archer',   ai: 'archer',   hp: 24,  dmg: 8,  speed: 40,  w: 12, h: 22, cells: 2, gold: 8,  range: 200, windup: 0.7, hex: '#cfc7a8' },
    { id: 'bat',      name: 'Cave Bat',      ai: 'flyer',    hp: 16,  dmg: 7,  speed: 92,  w: 13, h: 11, cells: 1, gold: 4,  reach: 14, windup: 0.2,  hex: '#9d6bff' },
    { id: 'shielder', name: 'Shieldbearer',  ai: 'shielder', hp: 44,  dmg: 11, speed: 38,  w: 15, h: 23, cells: 3, gold: 11, reach: 20, windup: 0.5,  hex: '#8fa7c4' },
    { id: 'bomber',   name: 'Grenadier',     ai: 'bomber',   hp: 28,  dmg: 14, speed: 46,  w: 14, h: 21, cells: 3, gold: 10, range: 190, windup: 0.85, hex: '#ff8a1e' },
    { id: 'caster',   name: 'Inquisitor',    ai: 'caster',   hp: 34,  dmg: 12, speed: 52,  w: 13, h: 24, cells: 4, gold: 14, range: 230, windup: 0.9,  hex: '#ff4fd8' },
    { id: 'slammer',  name: 'Protector',     ai: 'slammer',  hp: 70,  dmg: 17, speed: 36,  w: 20, h: 27, cells: 5, gold: 18, reach: 28, windup: 0.75, hex: '#e05c3a' },
    { id: 'spitter',  name: 'Sewer Spitter', ai: 'archer',   hp: 30,  dmg: 10, speed: 34,  w: 14, h: 20, cells: 2, gold: 9,  range: 170, windup: 0.6, hex: '#8ddb3a', poisonShot: true }
  ];

  /* ------------------------------------------------------------------ bosses */
  const BOSSES = [
    {
      id: 'warden', name: 'The Warden', hp: 560, dmg: 18, w: 30, h: 40, cells: 22, gold: 90,
      hex: '#ff3b5c', speed: 70,
      moves: ['charge', 'slam', 'summon'],
      intro: 'THE WARDEN', subtitle: 'keeper of the black bridge'
    },
    {
      id: 'throne', name: 'Hand of the Throne', hp: 980, dmg: 22, w: 28, h: 42, cells: 40, gold: 160,
      hex: '#ffe600', speed: 96,
      moves: ['dash', 'fan', 'slam', 'summon', 'rain'],
      intro: 'HAND OF THE THRONE', subtitle: 'the last door'
    }
  ];

  /* ------------------------------------------------------------------ biomes
   * The run, in order. cols/rows is the room grid the generator fills.
   */
  const BIOMES = [
    {
      id: 'quarters', name: "Prisoners' Quarters", tag: 'where it always starts',
      cols: 3, rows: 2, pool: ['zombie', 'zombie', 'zombie', 'runner'], density: 0.8,
      scrolls: 2, chests: 2, shop: false, spikes: 0.03, depth: 1,
      palette: { sky1: '#120a1c', sky2: '#221436', rock: '#2e2342', rockLit: '#453357', edge: '#6a4f86', moss: '#2fe6c8', torch: '#ff8a1e', mote: 'rgba(47,230,200,.5)' }
    },
    {
      id: 'promenade', name: 'Promenade of the Condemned', tag: 'open sky, bad company',
      cols: 4, rows: 2, pool: ['zombie', 'runner', 'archer', 'bat'], density: 1.0,
      scrolls: 3, chests: 2, shop: true, spikes: 0.05, depth: 2,
      palette: { sky1: '#0a1624', sky2: '#123049', rock: '#233a4a', rockLit: '#31526a', edge: '#4f86a8', moss: '#8ddb3a', torch: '#ffd34a', mote: 'rgba(141,219,58,.4)' }
    },
    {
      id: 'sewers', name: 'Toxic Sewers', tag: 'mind the spikes',
      cols: 4, rows: 3, pool: ['spitter', 'runner', 'bat', 'shielder'], density: 1.1,
      scrolls: 3, chests: 3, shop: true, spikes: 0.10, depth: 3,
      palette: { sky1: '#07140c', sky2: '#0e2417', rock: '#1c3326', rockLit: '#2a4a35', edge: '#55874f', moss: '#b6ff5a', torch: '#9dff4a', mote: 'rgba(182,255,90,.45)' }
    },
    {
      id: 'ossuary', name: 'Ossuary', tag: 'bones all the way down',
      cols: 4, rows: 3, pool: ['archer', 'bat', 'caster', 'shielder', 'zombie'], density: 1.2,
      scrolls: 3, chests: 3, shop: true, spikes: 0.07, depth: 4,
      palette: { sky1: '#140f18', sky2: '#241c2c', rock: '#3a3140', rockLit: '#524657', edge: '#8e7f99', moss: '#cfc7a8', torch: '#ff4fd8', mote: 'rgba(207,199,168,.4)' }
    },
    {
      id: 'ramparts', name: 'Ramparts', tag: 'they have the high ground',
      cols: 5, rows: 3, pool: ['archer', 'bomber', 'shielder', 'slammer', 'caster'], density: 1.25,
      scrolls: 4, chests: 3, shop: true, spikes: 0.06, depth: 5,
      palette: { sky1: '#1a0d12', sky2: '#2e1620', rock: '#3c2630', rockLit: '#563743', edge: '#96596e', moss: '#ff8aa8', torch: '#ff8a1e', mote: 'rgba(255,138,168,.35)' }
    },
    {
      id: 'bridge', name: 'Black Bridge', tag: 'the warden is waiting',
      boss: 'warden', depth: 6,
      palette: { sky1: '#0a0a12', sky2: '#1c1424', rock: '#2a2232', rockLit: '#3d3147', edge: '#7a5f8e', moss: '#ff3b5c', torch: '#ff3b5c', mote: 'rgba(255,59,92,.4)' }
    },
    {
      id: 'keep', name: 'Stilt Village', tag: 'one last climb',
      cols: 5, rows: 3, pool: ['slammer', 'caster', 'bomber', 'shielder', 'runner'], density: 1.35,
      scrolls: 4, chests: 3, shop: true, spikes: 0.08, depth: 7,
      palette: { sky1: '#150a08', sky2: '#2a1410', rock: '#3a251c', rockLit: '#543627', edge: '#9c6a3c', moss: '#ffb300', torch: '#ffd34a', mote: 'rgba(255,179,0,.35)' }
    },
    {
      id: 'throne', name: 'Throne Room', tag: 'no more doors',
      boss: 'throne', depth: 8,
      palette: { sky1: '#12100a', sky2: '#2a2410', rock: '#36301c', rockLit: '#4e462a', edge: '#a3924a', moss: '#ffe600', torch: '#ffe600', mote: 'rgba(255,230,0,.4)' }
    }
  ];

  /* --------------------------------------------------------------- lookups */
  function byId(list) {
    const map = Object.create(null);
    for (const item of list) map[item.id] = item;
    return map;
  }

  const API = {
    COLORS: COLORS,
    COLOR_IDS: COLOR_IDS,
    STATUSES: STATUSES,
    WEAPONS: WEAPONS,
    WEAPON: byId(WEAPONS),
    AFFIXES: AFFIXES,
    AFFIX: byId(AFFIXES),
    SKILLS: SKILLS,
    SKILL: byId(SKILLS),
    MUTATIONS: MUTATIONS,
    MUTATION: byId(MUTATIONS),
    ENEMIES: ENEMIES,
    ENEMY: byId(ENEMIES),
    BOSSES: BOSSES,
    BOSS: byId(BOSSES),
    BIOMES: BIOMES,
    BIOME: byId(BIOMES),

    /* Everything that can be unlocked with cells between runs. */
    unlockables: function () {
      const out = [];
      for (const w of WEAPONS) if (w.tier > 0) out.push({ kind: 'weapon', id: w.id, name: w.name, color: w.color, cost: w.cost, desc: w.desc });
      for (const s of SKILLS) if (s.tier > 0) out.push({ kind: 'skill', id: s.id, name: s.name, color: s.color, cost: s.cost, desc: s.desc });
      return out;
    }
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  global.CELLS_CONTENT = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
