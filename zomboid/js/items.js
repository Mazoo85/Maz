/* =============================================================================
 *  ZOMBOID: ANCHORAGE  —  ITEMS & LOOT TABLES
 * ========================================================================== */
(function (global) {
  'use strict';

  // type: weapon | food | drink | med | misc
  const ITEMS = {
    fists:        { name: 'Bare Fists',        type: 'weapon', dmg: 8,  range: 1.3, speed: 0.35, durab: Infinity, icon: '👊' },
    branch:       { name: 'Birch Branch',      type: 'weapon', dmg: 14, range: 1.5, speed: 0.45, durab: 14, icon: '🌿' },
    bat:          { name: 'Louisville Bat',    type: 'weapon', dmg: 26, range: 1.6, speed: 0.5,  durab: 40, icon: '🏏' },
    crowbar:      { name: 'Crowbar',           type: 'weapon', dmg: 30, range: 1.5, speed: 0.55, durab: 80, icon: '🔧' },
    axe:          { name: 'Fire Axe',          type: 'weapon', dmg: 48, range: 1.7, speed: 0.7,  durab: 60, icon: '🪓' },
    machete:      { name: 'Machete',           type: 'weapon', dmg: 40, range: 1.6, speed: 0.5,  durab: 70, icon: '🔪' },
    pistol:       { name: 'M9 Pistol',         type: 'weapon', dmg: 70, range: 7.0, speed: 0.4,  durab: 9999, ranged: true, ammo: 'ammo9', icon: '🔫' },
    shotgun:      { name: 'Mossberg 500',      type: 'weapon', dmg: 120,range: 5.0, speed: 0.9,  durab: 9999, ranged: true, ammo: 'shells', icon: '💥' },
    ammo9:        { name: '9mm Rounds',        type: 'misc',   icon: '🔩', stack: 60 },
    shells:       { name: '12ga Shells',       type: 'misc',   icon: '🟥', stack: 24 },

    cannedsalmon: { name: 'Canned Salmon',     type: 'food', hunger: 45, icon: '🐟' },
    granola:      { name: 'Granola Bar',       type: 'food', hunger: 20, icon: '🍫' },
    chips:        { name: 'Bag of Chips',      type: 'food', hunger: 18, icon: '🥔' },
    moosejerky:   { name: 'Moose Jerky',       type: 'food', hunger: 30, icon: '🥩' },
    cannedbeans:  { name: 'Canned Beans',      type: 'food', hunger: 38, icon: '🥫' },
    pizza:        { name: "Moose's Tooth Slice",type:'food', hunger: 35, mood: 8, icon: '🍕' },

    water:        { name: 'Bottled Water',     type: 'drink', thirst: 40, icon: '💧' },
    soda:         { name: 'Can of Soda',       type: 'drink', thirst: 28, mood: 4, icon: '🥤' },
    coffee:       { name: 'Kaladi Coffee',     type: 'drink', thirst: 20, fatigue: -25, icon: '☕' },
    energy:       { name: 'Energy Drink',      type: 'drink', thirst: 18, fatigue: -35, icon: '⚡' },

    bandage:      { name: 'Bandage',           type: 'med', heal: 18, icon: '🩹' },
    pills:        { name: 'Painkillers',       type: 'med', heal: 30, icon: '💊' },
    firstaid:     { name: 'First Aid Kit',     type: 'med', heal: 55, icon: '🧰' },
    antibiotics:  { name: 'Antibiotics',       type: 'med', heal: 25, cureInfection: true, icon: '💉' },

    parka:        { name: 'Down Parka',        type: 'misc', icon: '🧥', warm: true },
    flashlight:   { name: 'Flashlight',        type: 'misc', icon: '🔦' },
    map:          { name: 'Anchorage Map',     type: 'misc', icon: '🗺️' },
  };

  // Loot tables by building richness (0..3). Each entry [itemId, weight].
  const LOOT_TABLES = {
    grocery:  [['cannedsalmon',6],['cannedbeans',6],['chips',5],['water',6],['soda',5],['granola',5],['pizza',2],['moosejerky',3]],
    food:     [['pizza',6],['soda',5],['water',5],['chips',4],['coffee',4],['granola',3]],
    bar:      [['soda',5],['water',4],['energy',4],['chips',3],['bat',2]],
    gas:      [['soda',5],['chips',5],['water',4],['granola',4],['ammo9',2],['flashlight',2]],
    hospital: [['firstaid',6],['bandage',7],['pills',6],['antibiotics',4],['water',3]],
    police:   [['pistol',4],['ammo9',6],['shotgun',2],['shells',4],['bandage',3],['crowbar',3]],
    hardware: [['axe',4],['crowbar',5],['machete',3],['bat',3],['flashlight',4],['branch',3]],
    outdoor:  [['machete',3],['moosejerky',4],['parka',4],['water',4],['shotgun',2],['shells',4],['map',3],['granola',3]],
    mall:     [['chips',4],['soda',4],['bat',3],['bandage',3],['parka',3],['granola',3],['flashlight',2]],
    hotel:    [['water',5],['granola',4],['bandage',3],['pills',3],['chips',3]],
    default:  [['water',4],['granola',4],['chips',4],['branch',3],['bandage',2],['soda',3]],
  };

  function rollLoot(kindRichness, kindName) {
    const table = LOOT_TABLES[kindName] || LOOT_TABLES.default;
    const total = table.reduce((s, e) => s + e[1], 0);
    const drops = [];
    const n = 1 + kindRichness;          // 1..4 items
    for (let i = 0; i < n; i++) {
      let r = Math.random() * total;
      for (const [id, w] of table) {
        r -= w;
        if (r <= 0) {
          const base = ITEMS[id];
          let qty = 1;
          if (base.stack) qty = Math.ceil(Math.random() * base.stack * 0.5) + 4;
          drops.push({ id, qty });
          break;
        }
      }
    }
    return drops;
  }

  global.ITEMS = ITEMS;
  global.LOOT = { rollLoot, tableFor: (k) => LOOT_TABLES[k] || LOOT_TABLES.default };
})(typeof window !== 'undefined' ? window : this);
