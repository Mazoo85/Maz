/*
 * CODA PICS — the lexicon.
 * ------------------------
 * The only place that knows what a word means. Every entry maps plain English
 * a person would actually type ("a red dragon over snowy mountains at sunset,
 * neon") onto the handful of decisions the painter needs: what to draw, where
 * it stands, what hour it is, what the weather is doing, and how the finished
 * picture should be treated.
 *
 * Data only — no drawing, no randomness, no DOM. js/prompt.js reads this to
 * build a scene; js/paint.js draws it. Keeping it inert is what lets the tests
 * check the whole vocabulary in Node, with no browser.
 *
 * Exposed as window.CodaLexicon (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  /* ------------------------------------------------------------- subjects
   * `draw` names a routine in js/subjects.js — several subjects share one
   * (a fox and a wolf are the same animal with different ears), which is why
   * the id and the routine are separate fields.
   *
   * `scene` is where this subject stands when the prompt does not say. A whale
   * with no setting belongs in the sea; a cactus does not.
   */
  var SUBJECTS = [
    /* --- creatures --- */
    { id: 'dragon',    label: 'a dragon',        draw: 'dragon',    scene: 'mountains', words: ['dragon', 'wyvern', 'drake', 'wyrm'] },
    { id: 'whale',     label: 'a whale',         draw: 'whale',     scene: 'ocean',     words: ['whale', 'humpback', 'orca', 'leviathan'] },
    { id: 'jellyfish', label: 'a jellyfish',     draw: 'jellyfish', scene: 'ocean',     words: ['jellyfish', 'jelly', 'medusa'] },
    { id: 'fish',      label: 'a great fish',    draw: 'fish',      scene: 'lake',      words: ['fish', 'koi', 'carp', 'salmon', 'trout'] },
    { id: 'serpent',   label: 'a serpent',       draw: 'serpent',   scene: 'desert',    words: ['snake', 'serpent', 'python', 'cobra', 'viper'] },
    { id: 'butterfly', label: 'a butterfly',     draw: 'butterfly', scene: 'meadow',    words: ['butterfly', 'moth', 'butterflies'] },
    { id: 'crab',      label: 'a crab',          draw: 'crab',      scene: 'shore',     words: ['crab', 'lobster', 'crustacean'] },
    { id: 'wolf',      label: 'a wolf',          draw: 'quadruped', form: 'wolf',   scene: 'forest',    words: ['wolf', 'wolves', 'direwolf', 'husky'] },
    { id: 'fox',       label: 'a fox',           draw: 'quadruped', form: 'fox',    scene: 'forest',    words: ['fox', 'vixen', 'kitsune'] },
    { id: 'cat',       label: 'a cat',           draw: 'quadruped', form: 'cat',    scene: 'city',      words: ['cat', 'kitten', 'kitty', 'tabby', 'feline'] },
    { id: 'dog',       label: 'a dog',           draw: 'quadruped', form: 'dog',    scene: 'meadow',    words: ['dog', 'puppy', 'hound', 'retriever', 'terrier'] },
    { id: 'deer',      label: 'a stag',          draw: 'quadruped', form: 'deer',   scene: 'forest',    words: ['deer', 'stag', 'elk', 'reindeer', 'caribou', 'moose'] },
    { id: 'horse',     label: 'a horse',         draw: 'quadruped', form: 'horse',  scene: 'plains',    words: ['horse', 'stallion', 'mare', 'pony', 'unicorn'] },
    { id: 'bear',      label: 'a bear',          draw: 'quadruped', form: 'bear',   scene: 'forest',    words: ['bear', 'grizzly', 'kodiak'] },
    { id: 'lion',      label: 'a lion',          draw: 'quadruped', form: 'lion',   scene: 'plains',    words: ['lion', 'lioness', 'tiger', 'panther', 'leopard', 'jaguar'] },
    { id: 'rabbit',    label: 'a rabbit',        draw: 'quadruped', form: 'rabbit', scene: 'meadow',    words: ['rabbit', 'hare', 'bunny'] },
    { id: 'bird',      label: 'a bird',          draw: 'bird',      form: 'bird',   scene: 'mountains', words: ['bird', 'birds', 'eagle', 'hawk', 'falcon', 'raven', 'crow', 'gull', 'heron', 'crane'] },
    { id: 'owl',       label: 'an owl',          draw: 'bird',      form: 'owl',    scene: 'forest',    words: ['owl', 'barn owl'] },
    { id: 'phoenix',   label: 'a phoenix',       draw: 'bird',      form: 'phoenix', scene: 'volcano',  words: ['phoenix', 'firebird'] },

    /* --- people --- */
    { id: 'figure',    label: 'a lone traveller', draw: 'humanoid', form: 'figure',    scene: 'plains',    words: ['traveller', 'traveler', 'wanderer', 'person', 'girl', 'boy', 'woman', 'man', 'child', 'figure', 'silhouette', 'hiker', 'walker'] },
    { id: 'astronaut', label: 'an astronaut',     draw: 'humanoid', form: 'astronaut', scene: 'space',     words: ['astronaut', 'cosmonaut', 'spaceman', 'spacewalker'] },
    { id: 'robot',     label: 'a robot',          draw: 'humanoid', form: 'robot',     scene: 'city',      words: ['robot', 'android', 'mech', 'droid', 'automaton', 'cyborg'] },
    { id: 'wizard',    label: 'a wizard',         draw: 'humanoid', form: 'wizard',    scene: 'mountains', words: ['wizard', 'sorcerer', 'mage', 'witch', 'warlock', 'sage'] },
    { id: 'knight',    label: 'a knight',         draw: 'humanoid', form: 'knight',    scene: 'ruins',     words: ['knight', 'warrior', 'samurai', 'paladin', 'soldier', 'swordsman'] },
    { id: 'ghost',     label: 'a ghost',          draw: 'humanoid', form: 'ghost',     scene: 'ruins',     words: ['ghost', 'spirit', 'phantom', 'wraith', 'spectre', 'specter'] },
    { id: 'diver',     label: 'a diver',          draw: 'humanoid', form: 'diver',     scene: 'ocean',     words: ['diver', 'swimmer', 'mermaid'] },

    /* --- buildings & places --- */
    { id: 'castle',     label: 'a castle',        draw: 'castle',     scene: 'mountains', words: ['castle', 'fortress', 'citadel', 'keep', 'palace', 'stronghold'] },
    { id: 'tower',      label: 'a tower',         draw: 'tower',      scene: 'plains',    words: ['tower', 'spire', 'obelisk', 'monolith', 'skyscraper'] },
    { id: 'lighthouse', label: 'a lighthouse',    draw: 'lighthouse', scene: 'shore',     words: ['lighthouse', 'beacon'] },
    { id: 'cabin',      label: 'a cabin',         draw: 'cabin',      scene: 'snow',      words: ['cabin', 'cottage', 'hut', 'shack', 'house', 'home', 'farmhouse', 'chalet'] },
    { id: 'temple',     label: 'a temple',        draw: 'temple',     scene: 'jungle',    words: ['temple', 'shrine', 'monastery', 'cathedral', 'church', 'sanctuary'] },
    { id: 'torii',      label: 'a torii gate',    draw: 'torii',      scene: 'lake',      words: ['torii', 'gate', 'archway', 'arch'] },
    { id: 'pyramid',    label: 'a pyramid',       draw: 'pyramid',    scene: 'desert',    words: ['pyramid', 'ziggurat', 'sphinx'] },
    { id: 'windmill',   label: 'a windmill',      draw: 'windmill',   scene: 'meadow',    words: ['windmill', 'mill', 'turbine'] },
    { id: 'bridge',     label: 'a bridge',        draw: 'bridge',     scene: 'canyon',    words: ['bridge', 'viaduct', 'aqueduct'] },
    { id: 'city',       label: 'a skyline',       draw: 'city',       scene: 'city',      words: ['city', 'skyline', 'metropolis', 'downtown', 'town', 'megacity'] },
    { id: 'ruins',      label: 'broken ruins',    draw: 'ruins',      scene: 'ruins',     words: ['ruin', 'ruins', 'rubble', 'wreckage'] },

    /* --- machines & things --- */
    { id: 'ship',     label: 'a tall ship',       draw: 'ship',     scene: 'ocean',   words: ['ship', 'boat', 'sailboat', 'schooner', 'galleon', 'vessel', 'yacht'] },
    { id: 'rocket',   label: 'a rocket',          draw: 'rocket',   scene: 'plains',  words: ['rocket', 'spaceship', 'shuttle', 'starship', 'spacecraft'] },
    { id: 'balloon',  label: 'a hot air balloon', draw: 'balloon',  scene: 'canyon',  words: ['balloon', 'hot air balloon', 'airship', 'zeppelin', 'blimp', 'dirigible'] },
    { id: 'ufo',      label: 'a flying saucer',   draw: 'ufo',      scene: 'desert',  words: ['ufo', 'saucer', 'alien ship', 'mothership'] },
    { id: 'train',    label: 'a train',           draw: 'train',    scene: 'plains',  words: ['train', 'locomotive', 'railway', 'railroad'] },
    { id: 'car',      label: 'a car',             draw: 'car',      scene: 'road',    words: ['car', 'truck', 'van', 'automobile', 'racer', 'roadster'] },
    { id: 'portal',   label: 'a portal',          draw: 'portal',   scene: 'ruins',   words: ['portal', 'rift', 'doorway', 'gateway', 'wormhole'] },
    { id: 'sword',    label: 'a sword in stone',  draw: 'sword',    scene: 'ruins',   words: ['sword', 'blade', 'katana', 'excalibur', 'dagger'] },
    { id: 'campfire', label: 'a campfire',        draw: 'campfire', scene: 'forest',  words: ['campfire', 'bonfire', 'fire', 'campsite', 'tent', 'camp'] },
    { id: 'crystal',  label: 'a crystal',         draw: 'crystal',  scene: 'cave',    words: ['crystal', 'gem', 'geode', 'diamond', 'quartz', 'shard'] },
    { id: 'skull',    label: 'a skull',           draw: 'skull',    scene: 'desert',  words: ['skull', 'bones', 'skeleton'] },

    /* --- plants & nature --- */
    { id: 'tree',       label: 'a great tree',    draw: 'tree',      form: 'oak',     scene: 'meadow',   words: ['tree', 'oak', 'willow', 'maple', 'sakura', 'cherry blossom', 'blossom'] },
    { id: 'pine',       label: 'pines',           draw: 'tree',      form: 'pine',    scene: 'forest',   words: ['pine', 'fir', 'spruce', 'conifer', 'evergreen'] },
    { id: 'palm',       label: 'a palm',          draw: 'tree',      form: 'palm',    scene: 'island',   words: ['palm', 'coconut'] },
    { id: 'cactus',     label: 'a cactus',        draw: 'cactus',    scene: 'desert',  words: ['cactus', 'saguaro', 'succulent'] },
    { id: 'flower',     label: 'a flower',        draw: 'flower',    scene: 'meadow',  words: ['flower', 'rose', 'sunflower', 'tulip', 'lotus', 'poppy', 'daisy'] },
    { id: 'mushroom',   label: 'mushrooms',       draw: 'mushroom',  scene: 'forest',  words: ['mushroom', 'mushrooms', 'toadstool', 'fungus'] },
    { id: 'waterfall',  label: 'a waterfall',     draw: 'waterfall', scene: 'jungle',  words: ['waterfall', 'falls', 'cascade'] },
    { id: 'mountainTop', label: 'a great peak',   draw: 'peak',      scene: 'mountains', words: ['peak', 'summit', 'mount', 'everest', 'fuji', 'matterhorn'] },
    { id: 'island',     label: 'a floating isle', draw: 'island',    scene: 'sky',     words: ['island', 'isle', 'atoll', 'archipelago'] },
    { id: 'planet',     label: 'a ringed planet', draw: 'planet',    scene: 'space',   words: ['planet', 'saturn', 'jupiter', 'world', 'globe'] },
    { id: 'moon',       label: 'a huge moon',     draw: 'bigmoon',   scene: 'plains',  words: ['moon', 'luna', 'crescent'] },
    { id: 'eye',        label: 'a watching eye',  draw: 'eye',       scene: 'space',   words: ['eye', 'iris', 'pupil'] },

    /* --- more of what people ask for, on routines that already exist -------
     * A sheep and a wolf are the same four-legged builder with different ears
     * and tail; a swan and an owl the same bird. Adding the word is the whole
     * job, and the lexicon is where that stays honest. */
    { id: 'sheep',    label: 'a sheep',        draw: 'quadruped', form: 'rabbit', scene: 'meadow',    words: ['sheep', 'lamb', 'ram', 'goat'] },
    { id: 'cow',      label: 'a cow',          draw: 'quadruped', form: 'bear',   scene: 'meadow',    words: ['cow', 'bull', 'cattle', 'ox', 'buffalo', 'bison'] },
    { id: 'camel',    label: 'a camel',        draw: 'quadruped', form: 'horse',  scene: 'desert',    words: ['camel', 'dromedary'] },
    { id: 'elephant', label: 'an elephant',    draw: 'quadruped', form: 'bear',   scene: 'plains',    words: ['elephant', 'mammoth'] },
    { id: 'swan',     label: 'a swan',         draw: 'bird',      form: 'bird',   scene: 'lake',      words: ['swan', 'goose', 'duck', 'flamingo', 'stork'] },
    { id: 'bat',      label: 'a bat',          draw: 'bird',      form: 'bird',   scene: 'cave',      words: ['bat', 'bats'] },
    { id: 'monk',     label: 'a monk',         draw: 'humanoid',  form: 'wizard', scene: 'mountains', words: ['monk', 'priest', 'pilgrim', 'hermit'] },
    { id: 'pirate',   label: 'a pirate',       draw: 'humanoid',  form: 'knight', scene: 'shore',     words: ['pirate', 'captain', 'buccaneer'] },
    { id: 'farmer',   label: 'a farmer',       draw: 'humanoid',  form: 'figure', scene: 'meadow',    words: ['farmer', 'shepherd', 'gardener'] },
    { id: 'barn',     label: 'a barn',         draw: 'cabin',     scene: 'meadow',  words: ['barn', 'farmhouse', 'farm', 'stable'] },
    { id: 'obelisk',  label: 'an obelisk',     draw: 'tower',     scene: 'desert',  words: ['obelisk', 'pillar', 'column', 'standing stone'] },
    { id: 'raft',     label: 'a small boat',   draw: 'ship',      scene: 'lake',    words: ['raft', 'canoe', 'kayak', 'rowboat', 'dinghy'] },
    { id: 'satellite', label: 'a satellite',   draw: 'ufo',       scene: 'space',   words: ['satellite', 'probe', 'station'] },
    { id: 'comet',    label: 'a comet',        draw: 'bigmoon',   scene: 'space',   words: ['comet', 'asteroid', 'meteor'] },
    { id: 'totem',    label: 'a totem',        draw: 'crystal',   scene: 'forest',  words: ['totem', 'monolith stone', 'idol', 'statue'] }
  ];

  /* ---------------------------------------------------------------- scenes
   * Where the picture happens. `horizon` is how far down the canvas the land
   * meets the sky (0 = top), and is a starting point the painter jitters.
   * `prep` is only for the sentence the app reads back ("on the shore",
   * not "in the shore"); it defaults to "in".
   */
  var SCENES = [
    { id: 'mountains', label: 'the mountains', horizon: 0.62, words: ['mountain', 'mountains', 'alps', 'range', 'himalaya', 'peaks', 'highlands', 'cliff', 'cliffs'] },
    { id: 'forest',    label: 'a forest',    horizon: 0.66, words: ['forest', 'woods', 'woodland', 'jungle canopy', 'trees', 'grove', 'thicket'] },
    { id: 'jungle',    label: 'a jungle',    horizon: 0.68, words: ['jungle', 'rainforest', 'amazon', 'tropics'] },
    { id: 'ocean',     label: 'the open sea', horizon: 0.58, words: ['ocean', 'sea', 'waves', 'underwater', 'deep', 'tide', 'atlantic', 'pacific'] },
    { id: 'shore',     label: 'the shore',    prep: 'on', horizon: 0.60, words: ['shore', 'coast', 'beach', 'cove', 'bay', 'harbour', 'harbor', 'sand'] },
    { id: 'lake',      label: 'a still lake', horizon: 0.60, words: ['lake', 'pond', 'lagoon', 'reflection', 'fjord', 'loch'] },
    { id: 'desert',    label: 'a desert',    horizon: 0.70, words: ['desert', 'dune', 'dunes', 'sahara', 'wasteland', 'badlands', 'mesa'] },
    { id: 'city',      label: 'a city',      horizon: 0.72, words: ['street', 'alley', 'urban', 'neon city', 'rooftops', 'tokyo', 'newyork'] },
    { id: 'space',     label: 'deep space',  horizon: 1.20, words: ['space', 'galaxy', 'nebula', 'cosmos', 'orbit', 'stars', 'universe', 'void'] },
    { id: 'snow',      label: 'the snow',    horizon: 0.66, words: ['snow', 'snowy', 'arctic', 'tundra', 'glacier', 'ice', 'frozen', 'winter', 'blizzard'] },
    { id: 'canyon',    label: 'a canyon',    horizon: 0.64, words: ['canyon', 'gorge', 'ravine', 'valley', 'chasm'] },
    { id: 'swamp',     label: 'a swamp',     horizon: 0.68, words: ['swamp', 'marsh', 'bog', 'bayou', 'wetland', 'mire'] },
    { id: 'meadow',    label: 'a meadow',    horizon: 0.70, words: ['meadow', 'field', 'fields', 'grass', 'garden', 'pasture', 'countryside'] },
    { id: 'plains',    label: 'open plains', horizon: 0.72, words: ['plain', 'plains', 'prairie', 'steppe', 'savanna', 'hills', 'moor'] },
    { id: 'volcano',   label: 'a volcano',   horizon: 0.64, words: ['volcano', 'lava', 'magma', 'ash', 'eruption', 'inferno'] },
    { id: 'ruins',     label: 'old ruins',   horizon: 0.68, words: ['ruins', 'ancient', 'abandoned', 'derelict', 'forgotten', 'overgrown'] },
    { id: 'cave',      label: 'a cavern',    horizon: 0.74, words: ['cave', 'cavern', 'grotto', 'underground', 'tunnel', 'mine'] },
    { id: 'island',    label: 'an island',   horizon: 0.62, words: ['tropical', 'paradise', 'reef', 'caribbean'] },
    { id: 'road',      label: 'an empty road', prep: 'on', horizon: 0.68, words: ['road', 'highway', 'freeway', 'route', 'asphalt'] },
    { id: 'sky',       label: 'the open sky', horizon: 0.86, words: ['sky', 'clouds above', 'above the clouds', 'heavens', 'skies'] }
  ];

  /* ------------------------------------------------------------------ hour */
  var TIMES = [
    { id: 'dawn',  label: 'at dawn',     words: ['dawn', 'sunrise', 'morning', 'daybreak', 'first light', 'early'] },
    { id: 'day',   label: 'in daylight', words: ['day', 'daylight', 'noon', 'midday', 'afternoon', 'sunny', 'bright'] },
    { id: 'dusk',  label: 'at sunset',   words: ['sunset', 'dusk', 'evening', 'twilight', 'golden hour', 'sundown'] },
    { id: 'night', label: 'at night',    words: ['night', 'midnight', 'moonlight', 'nocturnal', 'dark', 'starlit', 'starry'] }
  ];

  /* --------------------------------------------------------------- weather */
  var WEATHER = [
    { id: 'clear',  label: 'clear',        words: ['clear', 'cloudless'] },
    { id: 'clouds', label: 'clouded',      words: ['cloud', 'clouds', 'cloudy', 'overcast'] },
    { id: 'rain',   label: 'in the rain',  words: ['rain', 'rainy', 'drizzle', 'downpour', 'monsoon', 'wet'] },
    { id: 'snowfall', label: 'in falling snow', words: ['snowfall', 'snowing', 'flurry', 'snowflakes'] },
    { id: 'fog',    label: 'in fog',       words: ['fog', 'foggy', 'mist', 'misty', 'haze', 'hazy', 'smoke'] },
    { id: 'storm',  label: 'in a storm',   words: ['storm', 'stormy', 'lightning', 'thunder', 'tempest', 'hurricane'] },
    { id: 'aurora', label: 'under an aurora', words: ['aurora', 'northern lights', 'borealis'] }
  ];

  /* ---------------------------------------------------------------- styles
   * How the finished picture is treated. Each id has a matching pass in
   * js/finish.js — the tests check that the two lists never drift apart.
   */
  var STYLES = [
    { id: 'neon',       label: 'Neon',          words: ['neon', 'synthwave', 'vaporwave', 'cyberpunk', 'retrowave', 'outrun', 'glow', 'glowing'] },
    { id: 'pixel',      label: 'Pixel art',     words: ['pixel', 'pixel art', '8 bit', '8bit', 'sprite', 'minecraft'] },
    { id: 'retro16',    label: '16-bit arcade', words: ['16 bit', '16bit', 'arcade', 'sega', 'retro game', 'megadrive', 'snes'] },
    { id: 'watercolour', label: 'Watercolour',  words: ['watercolour', 'watercolor', 'wash', 'aquarelle', 'soft paint'] },
    { id: 'noir',       label: 'Film noir',     words: ['noir', 'black and white', 'monochrome', 'grayscale', 'greyscale', 'charcoal'] },
    { id: 'comic',      label: 'Comic book',    words: ['comic', 'cartoon', 'manga', 'anime', 'graphic novel', 'halftone', 'pop art'] },
    { id: 'blueprint',  label: 'Blueprint',     words: ['blueprint', 'schematic', 'technical drawing', 'wireframe', 'draft'] },
    { id: 'storybook',  label: 'Storybook',     words: ['storybook', 'childrens book', 'fairytale', 'whimsical', 'pastel', 'cosy', 'cozy'] },
    { id: 'ukiyo',      label: 'Woodblock',     words: ['ukiyo', 'ukiyo-e', 'woodblock', 'japanese print', 'hokusai', 'linocut'] },
    { id: 'poster',     label: 'Poster',        words: ['poster', 'flat', 'minimal', 'minimalist', 'vector', 'travel poster', 'silhouette'] },
    { id: 'lowpoly',    label: 'Low poly',      words: ['low poly', 'lowpoly', 'polygon', 'faceted', 'geometric', '3d render'] },
    { id: 'glass',      label: 'Stained glass', words: ['stained glass', 'mosaic', 'glass', 'tiffany'] },
    { id: 'oil',        label: 'Oil painting',  words: ['oil', 'oil painting', 'impressionist', 'painterly', 'van gogh', 'brush'] },
    { id: 'photo',      label: 'Soft realism',  words: ['photo', 'photograph', 'photorealistic', 'realistic', 'cinematic', 'film still', 'hyperreal'] }
  ];

  /* -------------------------------------------------------------- palettes
   * A palette bends every colour the painter mixes. `hue` is degrees of shift,
   * `sat` and `warm` are multipliers. Absent = the scene's natural colours.
   */
  var PALETTES = [
    { id: 'crimson',  label: 'crimson',   hue: -20, sat: 1.25, warm: 1.20, words: ['red', 'crimson', 'scarlet', 'blood', 'ruby'] },
    { id: 'ember',    label: 'ember',     hue: -8,  sat: 1.20, warm: 1.30, words: ['orange', 'amber', 'ember', 'copper', 'rust', 'autumn'] },
    { id: 'gold',     label: 'gold',      hue: 8,   sat: 1.15, warm: 1.25, words: ['gold', 'golden', 'yellow', 'brass', 'honey', 'sand'] },
    { id: 'emerald',  label: 'emerald',   hue: 95,  sat: 1.20, warm: 0.92, words: ['green', 'emerald', 'jade', 'moss', 'verdant', 'lime'] },
    { id: 'teal',     label: 'teal',      hue: 150, sat: 1.15, warm: 0.88, words: ['teal', 'turquoise', 'aqua', 'cyan', 'seafoam'] },
    { id: 'cobalt',   label: 'cobalt',    hue: 205, sat: 1.20, warm: 0.82, words: ['blue', 'cobalt', 'sapphire', 'navy', 'azure', 'indigo'] },
    { id: 'violet',   label: 'violet',    hue: 265, sat: 1.20, warm: 0.95, words: ['purple', 'violet', 'lavender', 'amethyst', 'magenta', 'lilac'] },
    { id: 'rose',     label: 'rose',      hue: 330, sat: 1.10, warm: 1.10, words: ['pink', 'rose', 'blush', 'coral', 'peach'] },
    { id: 'ice',      label: 'ice',       hue: 195, sat: 0.75, warm: 0.80, words: ['white', 'ice', 'icy', 'pale', 'silver', 'frost', 'pearl'] },
    { id: 'ink',      label: 'ink',       hue: 220, sat: 0.30, warm: 0.85, words: ['black', 'ink', 'shadow', 'obsidian', 'onyx'] }
  ];

  /* ------------------------------------------------------------------ mood
   * A single 0..1 dial. Low is calm and open; high is dramatic and contrasty.
   */
  var MOODS = [
    { id: 'calm',      mood: 0.15, words: ['calm', 'peaceful', 'quiet', 'serene', 'gentle', 'still', 'soft', 'tranquil', 'sleepy'] },
    { id: 'warm',      mood: 0.32, words: ['happy', 'joyful', 'warm', 'cheerful', 'bright', 'hopeful', 'sweet'] },
    { id: 'lonely',    mood: 0.52, words: ['lonely', 'alone', 'melancholy', 'sad', 'empty', 'quietly', 'nostalgic', 'wistful'] },
    { id: 'epic',      mood: 0.75, words: ['epic', 'massive', 'colossal', 'mighty', 'legendary', 'heroic', 'vast', 'grand'] },
    { id: 'ominous',   label: 'ominous', mood: 0.88, words: ['dark', 'ominous', 'haunted', 'cursed', 'eerie', 'sinister', 'apocalyptic', 'doom', 'nightmare', 'horror', 'burning', 'war'] }
  ];

  /* --------------------------------------------------------------- sizeing
   * Words that make the subject bigger or smaller than its default.
   */
  var SCALE_WORDS = [
    { factor: 1.55, words: ['giant', 'huge', 'colossal', 'enormous', 'massive', 'towering', 'gigantic', 'titanic'] },
    { factor: 1.25, words: ['big', 'large', 'great', 'tall'] },
    { factor: 0.72, words: ['small', 'little', 'tiny', 'distant', 'far', 'faraway'] },
    { factor: 0.55, words: ['miniature', 'microscopic', 'speck'] }
  ];

  /* Words that add more than one of the subject. */
  var COUNT_WORDS = [
    { count: 3, words: ['many', 'several', 'group', 'flock', 'herd', 'pack', 'crowd', 'swarm', 'three'] },
    { count: 2, words: ['two', 'pair', 'couple', 'both'] },
    { count: 1, words: ['one', 'lone', 'single', 'solitary', 'a'] }
  ];

  /* ---------------------------------------------------------- relations
   * Where one thing sits relative to another. People write these constantly
   * ("a cat under a tree") and a painter that ignores them paints the wrong
   * picture with all the right things in it.
   */
  var RELATIONS = [
    { id: 'under',  label: 'under',      words: ['under', 'underneath', 'beneath', 'below'] },
    { id: 'above',  label: 'above',      words: ['above', 'over', 'on top of', 'atop', 'on'] },
    { id: 'behind', label: 'behind',     words: ['behind', 'past', 'beyond'] },
    { id: 'beside', label: 'beside',     words: ['beside', 'next to', 'alongside', 'by', 'with', 'and'] },
    { id: 'front',  label: 'in front of', words: ['in front of', 'before'] }
  ];

  /* Words that mean nothing to a painter. Kept so the app can tell the
   * difference between "a word I do not know" and "a word nobody draws". */
  var FILLER = [
    'a', 'an', 'the', 'of', 'in', 'on', 'at', 'to', 'is', 'it', 'its', 'as',
    'and', 'or', 'but', 'with', 'from', 'into', 'onto', 'for', 'by', 'very',
    'some', 'that', 'this', 'there', 'here', 'looking', 'style', 'art', 'image',
    'picture', 'draw', 'drawing', 'make', 'me', 'please', 'show', 'want', 'like',
    'really', 'quite', 'sort', 'kind', 'scene', 'background', 'foreground'
  ];

  var LEXICON = {
    SUBJECTS: SUBJECTS,
    SCENES: SCENES,
    TIMES: TIMES,
    WEATHER: WEATHER,
    STYLES: STYLES,
    PALETTES: PALETTES,
    MOODS: MOODS,
    SCALE_WORDS: SCALE_WORDS,
    COUNT_WORDS: COUNT_WORDS,
    RELATIONS: RELATIONS,
    FILLER: FILLER
  };

  root.CodaLexicon = LEXICON;
  if (typeof module !== 'undefined' && module.exports) module.exports = LEXICON;
})(typeof window !== 'undefined' ? window : this);
