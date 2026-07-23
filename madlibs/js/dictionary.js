/*
 * MadLibs Story Generator — Dictionary
 * ------------------------------------
 * Part-of-speech / theme categorized word lists. The generator draws random
 * words from these to fill the {placeholders} in story templates.
 *
 * MadLibs need grammatically-aware categories (a verb where a verb belongs,
 * a noun where a noun belongs) so that random picks still read naturally.
 *
 * To add words: just push more strings onto any array below. Every new word
 * multiplies the number of unique stories the app can produce.
 *
 * Exposed as: window.MADLIBS_DICT  (also module.exports for headless testing)
 */
(function (root) {
  'use strict';

  var DICT = {
    // --- People & beings -------------------------------------------------
    name: [
      'Aria', 'Kai', 'Rowan', 'Sable', 'Dax', 'Nova', 'Cyrus', 'Wren',
      'Milo', 'Freya', 'Jasper', 'Indira', 'Orion', 'Luna', 'Bram', 'Zoya',
      'Silas', 'Mira', 'Thorne', 'Elara', 'Ezra', 'Nadia', 'Flint', 'Cleo',
      'Ronan', 'Isolde', 'Quinn', 'Vera', 'Hollis', 'Juno', 'Percy', 'Anya',
      'Caspian', 'Delphine', 'Ludo', 'Marlowe', 'Ottavia', 'Rafferty',
      'Selene', 'Tobias', 'Winnie', 'Xander', 'Yara', 'Zephyr', 'Beckett',
      'Cordelia', 'Emrys', 'Fenn', 'Greta', 'Hugo'
    ],
    profession: [
      'blacksmith', 'astronaut', 'detective', 'librarian', 'chef', 'pilot',
      'archaeologist', 'nurse', 'sailor', 'clockmaker', 'botanist', 'thief',
      'diplomat', 'mechanic', 'surgeon', 'cartographer', 'beekeeper',
      'lighthouse keeper', 'street magician', 'bounty hunter', 'journalist',
      'locksmith', 'perfumer', 'undertaker', 'tattoo artist', 'smuggler',
      'watchmaker', 'gravedigger', 'fortune teller', 'toymaker', 'ferryman',
      'apothecary', 'stunt double', 'code-breaker', 'gardener', 'welder',
      'violinist', 'baker', 'ranger', 'archivist'
    ],
    creature: [
      'dragon', 'griffin', 'kraken', 'phoenix', 'werewolf', 'basilisk',
      'sphinx', 'wendigo', 'golem', 'chimera', 'leviathan', 'gargoyle',
      'banshee', 'minotaur', 'hydra', 'wyvern', 'poltergeist', 'shapeshifter',
      'sea serpent', 'frost giant', 'shadow wolf', 'thunderbird', 'revenant',
      'djinn', 'cyclops', 'harpy', 'ghoul', 'manticore', 'selkie', 'gremlin'
    ],
    villainTitle: [
      'Baron', 'Countess', 'Warlord', 'High Priestess', 'Overlord',
      'Duke', 'Sorceress', 'Inquisitor', 'Tyrant', 'Marchioness', 'Kingpin',
      'Archduke', 'Chancellor', 'Cult Leader', 'Pirate Queen', 'Necromancer',
      'Governor', 'Cardinal', 'Warden', 'Enchantress'
    ],
    animal: [
      'fox', 'raven', 'wolf', 'owl', 'lynx', 'stag', 'otter', 'falcon',
      'panther', 'hare', 'badger', 'heron', 'mongoose', 'jackal', 'crow',
      'stoat', 'ibis', 'lemur', 'weasel', 'magpie', 'coyote', 'osprey',
      'salamander', 'octopus', 'firefly', 'scorpion', 'moth', 'seal'
    ],

    // --- Descriptors -----------------------------------------------------
    adjective: [
      'ancient', 'forbidden', 'glittering', 'cursed', 'hollow', 'radiant',
      'restless', 'crooked', 'velvet', 'frozen', 'feral', 'gilded', 'weary',
      'luminous', 'ravenous', 'silent', 'shattered', 'reckless', 'molten',
      'hidden', 'wretched', 'brazen', 'fragile', 'thunderous', 'nameless',
      'crimson', 'ghostly', 'jagged', 'sacred', 'wild', 'somber', 'dazzling',
      'venomous', 'tangled', 'brittle', 'sunken', 'blistering', 'quiet',
      'unruly', 'clever', 'doomed', 'gleaming', 'furious', 'wandering',
      'stubborn', 'invisible', 'colossal', 'flickering', 'mournful', 'bold'
    ],
    emotion: [
      'grief', 'wonder', 'dread', 'longing', 'fury', 'hope', 'envy',
      'guilt', 'joy', 'despair', 'courage', 'shame', 'awe', 'loneliness',
      'defiance', 'tenderness', 'panic', 'serenity', 'jealousy', 'regret',
      'yearning', 'spite', 'relief', 'terror', 'pride', 'sorrow'
    ],
    color: [
      'crimson', 'emerald', 'obsidian', 'amber', 'violet', 'silver',
      'ivory', 'cobalt', 'scarlet', 'jade', 'ochre', 'indigo', 'gold',
      'ash-gray', 'sea-green', 'blood-orange', 'midnight-blue', 'bone-white',
      'copper', 'plum'
    ],
    numberWord: [
      'three', 'seven', 'twelve', 'thirteen', 'nine', 'a hundred',
      'a thousand', 'two', 'five', 'forty', 'a dozen', 'countless',
      'four', 'six', 'ten', 'twenty', 'a single', 'the last'
    ],

    // --- Actions ---------------------------------------------------------
    verb: [
      'steal', 'awaken', 'betray', 'summon', 'unearth', 'shatter', 'hunt',
      'protect', 'escape', 'rebuild', 'poison', 'ignite', 'forgive',
      'command', 'vanish', 'conquer', 'sabotage', 'inherit', 'confront',
      'abandon', 'restore', 'unravel', 'silence', 'bargain', 'defy',
      'smuggle', 'resurrect', 'outwit', 'banish', 'rescue'
    ],
    verbPast: [
      'stole', 'awakened', 'betrayed', 'summoned', 'unearthed', 'shattered',
      'hunted', 'vanished', 'conquered', 'inherited', 'abandoned', 'poisoned',
      'ignited', 'forgave', 'sabotaged', 'confronted', 'restored', 'defied',
      'smuggled', 'resurrected', 'outwitted', 'banished', 'rescued',
      'bargained', 'unraveled', 'silenced', 'escaped', 'commanded'
    ],
    verbIng: [
      'stealing', 'awakening', 'betraying', 'summoning', 'unearthing',
      'shattering', 'hunting', 'vanishing', 'conquering', 'inheriting',
      'abandoning', 'poisoning', 'igniting', 'forgiving', 'sabotaging',
      'confronting', 'restoring', 'defying', 'smuggling', 'resurrecting',
      'outwitting', 'banishing', 'rescuing', 'bargaining', 'unraveling',
      'silencing', 'escaping', 'commanding', 'searching', 'burning'
    ],
    adverb: [
      'secretly', 'recklessly', 'silently', 'desperately', 'ruthlessly',
      'gracefully', 'blindly', 'furiously', 'tenderly', 'relentlessly',
      'quietly', 'boldly', 'cruelly', 'hastily', 'fearlessly', 'bitterly',
      'gently', 'savagely', 'cautiously', 'wildly'
    ],

    // --- Things ----------------------------------------------------------
    noun: [
      'lantern', 'compass', 'crown', 'letter', 'mirror', 'key', 'clock',
      'map', 'locket', 'dagger', 'violin', 'diary', 'skull', 'telescope',
      'ring', 'mask', 'coin', 'feather', 'candle', 'chalice', 'contract',
      'photograph', 'music box', 'pocket watch', 'seed', 'bell', 'ledger',
      'blueprint', 'amulet', 'painting', 'typewriter', 'radio', 'engine',
      'flask', 'quill', 'hourglass', 'ticket', 'suitcase', 'anchor', 'crown'
    ],
    pluralNoun: [
      'secrets', 'ghosts', 'stars', 'debts', 'lies', 'wolves', 'ashes',
      'wounds', 'memories', 'shadows', 'promises', 'ruins', 'thieves',
      'storms', 'echoes', 'crows', 'embers', 'strangers', 'graves',
      'machines', 'letters', 'islands', 'rebels', 'nightmares', 'coins',
      'refugees', 'clockwork soldiers', 'omens', 'relics', 'whispers'
    ],
    object: [
      'artifact', 'device', 'manuscript', 'serum', 'blade', 'gemstone',
      'engine', 'formula', 'recording', 'heirloom', 'prototype', 'antidote',
      'ledger', 'signal', 'talisman', 'reactor', 'canvas', 'specimen',
      'coordinates', 'confession', 'contraption', 'vault', 'transmitter'
    ],
    weapon: [
      'longsword', 'crossbow', 'revolver', 'war hammer', 'poisoned dagger',
      'plasma rifle', 'scythe', 'flamethrower', 'grappling hook', 'spear',
      'throwing knife', 'battle-axe', 'rail gun', 'trident', 'garrote',
      'blowgun', 'chain whip', 'harpoon', 'saber', 'slingshot'
    ],
    magicItem: [
      'enchanted amulet', 'singing sword', 'map of the dead', 'phoenix feather',
      'bottled storm', 'crown of thorns', 'mirror of truth', 'seed of worlds',
      'lantern of souls', 'ring of silence', 'clock that runs backward',
      'cloak of shadows', 'vial of starlight', 'book of names', 'wishing coin',
      'compass to nowhere', 'flute of tides', 'mask of a thousand faces'
    ],
    vehicle: [
      'steam train', 'airship', 'submarine', 'motorcycle', 'sailboat',
      'rocket', 'hot-air balloon', 'stagecoach', 'gondola', 'hovercraft',
      'freight elevator', 'cargo ship', 'sled', 'biplane', 'monorail',
      'escape pod', 'clockwork carriage', 'dirigible', 'speedboat', 'tram'
    ],
    food: [
      'honey cake', 'black coffee', 'pomegranate', 'sourdough', 'venison',
      'peppermint tea', 'wild mushrooms', 'salt cod', 'plum wine', 'ration bar',
      'candied ginger', 'oysters', 'burnt toast', 'moon pears', 'chili stew',
      'dark chocolate', 'pickled eggs', 'saffron rice', 'blood orange', 'mead'
    ],
    bodyPart: [
      'hand', 'eye', 'heart', 'shoulder', 'jaw', 'spine', 'throat', 'wrist',
      'knuckles', 'ribs', 'palm', 'brow', 'temple', 'thumb', 'ankle', 'chest'
    ],

    // --- Places ----------------------------------------------------------
    place: [
      'Ravenspire', 'the Sunken City', 'New Meridian', 'the Ashen Coast',
      'Port Halcyon', 'the Whispering Vale', 'Blackpine', 'the Iron Marsh',
      'Vesper Station', 'the Glass Desert', 'Old Hollow', 'the Amber Isles',
      'Dredmoor', 'the Crimson Steppe', 'Saltmarsh', 'the Frostreach',
      'Thornhaven', 'the Cinder Wastes', 'Lower Gallows', 'the Drowned Quarter',
      'Kestrel Bay', 'the Hollow Mountains', 'Umberfall', 'the Silver Fen',
      'Nightmarket', 'the Broken Meridian', 'Cobbleworth', 'the Weeping Cliffs',
      'Gasworks Row', 'the Forgotten Terminus'
    ],
    bodyOfWater: [
      'the Black Sea', 'a frozen lake', 'the tidal caves', 'a hidden lagoon',
      'the storm channel', 'a bottomless well', 'the delta', 'a flooded quarry',
      'the strait', 'a mountain spring', 'the harbor', 'a sunken reservoir',
      'the fjord', 'a mangrove swamp', 'the geyser fields'
    ],
    timePeriod: [
      'winter', 'the harvest', 'a blood moon', 'the eclipse', 'midnight',
      'the last day of summer', 'the flood season', 'dawn', 'the plague years',
      'the festival', 'the long dark', 'the thaw', 'the coronation',
      'the storm season', 'the equinox', 'the final hour', 'the dry season'
    ],

    // --- Extra themed categories (used by the newer templates) -----------
    gadget: [
      'neural implant', 'holo-blade', 'cloaking rig', 'data-spike', 'drone swarm',
      'memory chip', 'plasma cutter', 'pulse pistol', 'grappler', 'optic scrambler',
      'signal jammer', 'exo-suit', 'nano-serum', 'ghost-key', 'railcannon',
      'brainjack', 'hover-board', 'stealth cloak', 'EMP charge', 'wrist-comp'
    ],
    syndicate: [
      'the Ashwood Family', 'the Crimson Syndicate', 'the Vesper Cartel',
      'the Hollow Crew', 'the Black Ledger', 'the Iron Circle', 'the Nightjar Ring',
      'the Salt Kings', 'the Glass Court', 'the Undertow', 'the Gilded Hand',
      'the Cobra Consortium', 'the Pale Mob', 'the Thorn Collective', 'the Ninth Street Boys'
    ],
    sport: [
      'boxing', 'rowing', 'motocross', 'fencing', 'sprinting', 'wrestling',
      'ice hockey', 'street racing', 'archery', 'surfing', 'climbing',
      'roller derby', 'chess boxing', 'dressage', 'kickboxing', 'marathon running',
      'downhill skiing', 'skateboarding', 'weightlifting', 'sailing'
    ],
    era: [
      'the Gilded Age', 'the Roaring Twenties', 'the age of sail', 'the space race',
      'the industrial boom', 'the great frontier', 'the jazz years', 'the plague era',
      'the age of steam', 'the reconstruction', 'the neon eighties', 'the cold war',
      'the age of explorers', 'the dust-bowl years', 'the golden age of piracy'
    ],
    instrument: [
      'violin', 'cello', 'saxophone', 'grand piano', 'trumpet', 'harp',
      'accordion', 'electric guitar', 'double bass', 'clarinet', 'banjo',
      'organ', 'hand drum', 'flute', 'synthesizer', 'mandolin'
    ],
    celestial: [
      'a red moon', 'a dying star', 'a passing comet', 'the twin suns',
      'a black hole', 'a shattered ring', 'the northern lights', 'a rogue planet',
      'a meteor shower', 'the pole star', 'a supernova', 'an eclipse',
      'a nebula', 'a wandering moon', 'the morning star'
    ],
    disaster: [
      'earthquake', 'wildfire', 'flood', 'blizzard', 'volcanic eruption',
      'hurricane', 'landslide', 'drought', 'tidal wave', 'sandstorm',
      'meteor strike', 'plague', 'blackout', 'famine', 'reactor meltdown'
    ],
    relationship: [
      'sibling', 'mentor', 'rival', 'partner', 'twin', 'cousin',
      'friend', 'companion', 'protégé', 'bodyguard', 'childhood friend',
      'estranged brother', 'estranged sister', 'old flame', 'business partner'
    ],
    landscape: [
      'salt flats', 'a pine forest', 'a canyon', 'the tundra', 'a coral reef',
      'a bamboo grove', 'the badlands', 'a glacier', 'a rice terrace', 'a moor',
      'the dunes', 'a redwood valley', 'a lava field', 'the wetlands', 'a limestone cave'
    ],
    beverage: [
      'whiskey', 'bitter coffee', 'moonshine', 'green tea', 'red wine',
      'cheap beer', 'spiced cider', 'cold vodka', 'herbal tonic', 'rum',
      'sparkling water', 'black tea', 'absinthe', 'buttermilk', 'champagne'
    ],
    title: [
      'Captain', 'Professor', 'Doctor', 'Sergeant', 'Commander', 'Sister',
      'Warden', 'Ambassador', 'Detective', 'Colonel', 'Reverend', 'Judge',
      'Foreman', 'Skipper', 'Marshal'
    ],
    substance: [
      'gold', 'silver', 'obsidian', 'amber', 'iron', 'quicksilver',
      'crystal', 'jade', 'ivory', 'bronze', 'salt', 'ash', 'glass',
      'moonstone', 'brass', 'coal', 'opal', 'marble'
    ]
  };

  // Guard: warn (in a test/node context) if any list is suspiciously short.
  DICT.__categories = Object.keys(DICT).filter(function (k) {
    return k.indexOf('__') !== 0;
  });

  root.MADLIBS_DICT = DICT;
  if (typeof module !== 'undefined' && module.exports) {
    module.exports = DICT;
  }
})(typeof window !== 'undefined' ? window : this);
