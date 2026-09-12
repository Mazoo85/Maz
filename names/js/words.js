/*
 * NAME FORGE — the word bank
 * --------------------------
 * Two flat lists: 1000 adjectives and 1000 nouns. That is the whole raw
 * material of the app — every name it makes is one word from each list, in
 * whichever order the dice land.
 *
 * The lists are grouped under comments purely so a human can read and extend
 * them; the generator sees one long array each. Rules the tests enforce
 * (names/tests/names-logic.test.js):
 *
 *   - exactly 1000 entries in each list;
 *   - no duplicates inside a list;
 *   - every word is plain lowercase a-z (the app capitalises as needed), so
 *     no spaces, hyphens, accents or proper nouns.
 *
 * A word may appear in both lists (rose the colour, rose the flower) — the
 * generator simply never pairs a word with itself.
 *
 * Exposed as: window.NAME_WORDS (also module.exports for headless testing).
 */
(function (root) {
  'use strict';

  var ADJECTIVES = [
    /* --- colour & hue ------------------------------------------------- */
    'amber', 'ashen', 'auburn', 'azure', 'beige', 'blond', 'bronze',
    'cardinal', 'carmine', 'cerulean', 'charcoal', 'cherry', 'chestnut',
    'chrome', 'cobalt', 'copper', 'coral', 'cream', 'crimson', 'cyan',
    'dappled', 'ebony', 'emerald', 'fawn', 'flaxen', 'garnet', 'gilded',
    'ginger', 'gold', 'golden', 'grey', 'hazel', 'honeyed', 'indigo', 'inky',
    'ivory', 'jade', 'jet', 'khaki', 'lavender', 'lilac', 'lime', 'magenta',
    'maroon', 'mauve', 'milky', 'mint', 'mottled', 'navy', 'ochre', 'olive',
    'onyx', 'opal', 'orange', 'puce', 'pale', 'pastel', 'peach', 'pearly',
    'pewter', 'pied', 'pink', 'plum', 'purple', 'rosy', 'ruby', 'russet',
    'rust', 'saffron', 'sage', 'seafoam', 'sapphire', 'scarlet', 'sepia',
    'silver', 'slate', 'snowy', 'sooty', 'speckled', 'spotted', 'striped',
    'tan', 'tawny', 'teal', 'tinted', 'topaz', 'turquoise', 'umber',
    'verdant', 'vermilion', 'violet', 'viridian', 'white', 'wine', 'yellow',
    'zinc', 'brindled', 'burgundy', 'roan', 'sallow',

    /* --- light, fire & heat ------------------------------------------- */
    'ablaze', 'aglow', 'alight', 'beaming', 'blazing', 'blinding', 'bright',
    'brilliant', 'burning', 'candlelit', 'charred', 'cindered', 'dark',
    'darkened', 'dawning', 'dazzling', 'dim', 'dusky', 'eclipsed', 'ember',
    'fading', 'fiery', 'flaming', 'flaring', 'flickering', 'gleaming',
    'glimmering', 'glinting', 'glistening', 'glittering', 'glowing',
    'guttering', 'halogen', 'incandescent', 'kindled', 'lambent', 'lantern',
    'lightless', 'lucent', 'luminous', 'lunar', 'midnight', 'molten',
    'moonlit', 'murky', 'neon', 'obscured', 'opaque', 'phosphor', 'prismatic',
    'radiant', 'reflective', 'scorched', 'scorching', 'searing', 'shadowed',
    'shadowy', 'shimmering', 'shining', 'sizzling', 'smoky', 'smouldering',
    'solar', 'sparkling', 'starlit', 'stellar', 'sunlit', 'sunny', 'torchlit',
    'translucent', 'twilit', 'ultraviolet', 'umbral', 'unlit', 'veiled',
    'vivid', 'volcanic', 'wan', 'waning', 'waxing', 'aflame', 'backlit',
    'bleached', 'firelit', 'gloaming', 'glaring', 'lamplit', 'sunburnt',
    'gilt', 'balmy', 'blistering', 'boiling', 'feverish', 'scalding',
    'simmering', 'steaming', 'sweltering', 'torrid', 'sultry',

    /* --- material & make ---------------------------------------------- */
    'adamant', 'alabaster', 'alloyed', 'basalt', 'bejewelled', 'brass',
    'bricked', 'calcite', 'carbon', 'ceramic', 'chalky', 'china', 'clay',
    'concrete', 'cotton', 'crystal', 'crystalline', 'diamond', 'earthen',
    'enamelled', 'ferrous', 'flint', 'flinty', 'glass', 'glassy', 'granite',
    'gravelled', 'gypsum', 'iron', 'ironclad', 'jasper', 'lacquered',
    'leaden', 'leather', 'limestone', 'linen', 'marble', 'metallic',
    'mineral', 'obsidian', 'oaken', 'papery', 'pebbled', 'plaster', 'plastic',
    'platinum', 'porcelain', 'quartz', 'resin', 'rubber', 'sandstone',
    'satin', 'sequined', 'shale', 'silken', 'silicon', 'soapstone', 'steel',
    'stone', 'stony', 'tin', 'titanium', 'velvet', 'vinyl', 'wicker',
    'wooden', 'woollen', 'wrought', 'zircon', 'brocade', 'beaded', 'bone',
    'brazen', 'canvas', 'cast', 'chitin', 'cobbled', 'corded', 'denim',
    'driftwood', 'ebon', 'fossil', 'hempen', 'jadeite', 'latticed', 'mossy',
    'nickel', 'opaline', 'parchment', 'plated', 'quilted', 'rattan',
    'rosewood', 'soldered', 'stained', 'teak', 'burnished', 'woven', 'forged',
    'hammered', 'riveted',

    /* --- texture, shape & size ---------------------------------------- */
    'angular', 'barbed', 'blunt', 'bristled', 'broad', 'bulbous', 'bumpy',
    'chiselled', 'coarse', 'colossal', 'compact', 'corrugated', 'craggy',
    'crooked', 'curved', 'cavernous', 'dainty', 'dense', 'dimpled', 'domed',
    'downy', 'elongated', 'enormous', 'fathomless', 'feathered', 'fine',
    'flaky', 'flat', 'fluffy', 'furrowed', 'fuzzy', 'gaunt', 'giant',
    'gigantic', 'glossy', 'gnarled', 'grainy', 'grand', 'grooved', 'hairy',
    'hefty', 'hollow', 'hulking', 'huge', 'immense', 'jagged', 'knobbly',
    'knotted', 'lanky', 'lean', 'little', 'lofty', 'long', 'lumpy', 'massive',
    'matted', 'meagre', 'miniature', 'minute', 'narrow', 'notched', 'oblong',
    'petite', 'plump', 'pointed', 'polished', 'porous', 'prickly', 'ragged',
    'rippled', 'rough', 'rounded', 'rugged', 'scaly', 'serrated', 'sharp',
    'sheer', 'short', 'silky', 'sinuous', 'slender', 'slim', 'smooth',
    'spiky', 'spindly', 'spiral', 'squat', 'stout', 'stubby', 'supple',
    'svelte', 'tapered', 'thick', 'thin', 'tiny', 'towering', 'twisted',
    'vast', 'wispy', 'crested', 'winged',

    /* --- motion & speed ----------------------------------------------- */
    'agile', 'airborne', 'ambling', 'arcing', 'bobbing', 'bolting',
    'bounding', 'brisk', 'cantering', 'careening', 'cascading', 'charging',
    'circling', 'climbing', 'coasting', 'colliding', 'creeping', 'cruising',
    'darting', 'dashing', 'diving', 'drifting', 'driving', 'drooping',
    'falling', 'fleeing', 'fleet', 'flitting', 'floating', 'flowing',
    'fluttering', 'flying', 'galloping', 'gliding', 'gyrating', 'headlong',
    'hovering', 'hurried', 'hurtling', 'idling', 'jogging', 'jolting',
    'juddering', 'jumping', 'leaping', 'lively', 'loping', 'lumbering',
    'lurching', 'marching', 'meandering', 'migrating', 'nimble', 'nomadic',
    'orbiting', 'pacing', 'plodding', 'plummeting', 'pouncing', 'prowling',
    'quick', 'racing', 'rambling', 'ranging', 'reeling', 'revolving',
    'rising', 'roaming', 'rocketing', 'rolling', 'roving', 'rushing',
    'sailing', 'sauntering', 'scampering', 'scrambling', 'scurrying',
    'shuffling', 'skidding', 'skimming', 'slinking', 'slithering', 'snaking',
    'soaring', 'speeding', 'spinning', 'sprinting', 'stalking', 'staggering',
    'stampeding', 'streaking', 'strolling', 'swerving', 'swift', 'swirling',
    'tumbling', 'vaulting', 'wandering', 'whirling', 'zigzagging',

    /* --- temper & mood ------------------------------------------------ */
    'affable', 'amiable', 'angry', 'anxious', 'ardent', 'bashful', 'bitter',
    'blissful', 'blithe', 'boisterous', 'bold', 'brave', 'breezy', 'brooding',
    'buoyant', 'calm', 'carefree', 'cheerful', 'cheery', 'chipper',
    'composed', 'content', 'crabby', 'cranky', 'cuddly', 'daring',
    'dauntless', 'dejected', 'doleful', 'dour', 'dreamy', 'eager', 'earnest',
    'ecstatic', 'elated', 'fearless', 'feisty', 'ferocious', 'fierce',
    'fretful', 'frisky', 'furious', 'gallant', 'genial', 'gentle', 'giddy',
    'glad', 'gleeful', 'gloomy', 'grim', 'grouchy', 'grumpy', 'happy',
    'hearty', 'heroic', 'hopeful', 'hotheaded', 'hushed', 'impish', 'irate',
    'jolly', 'jovial', 'joyful', 'jubilant', 'keen', 'kindly', 'languid',
    'lonesome', 'loyal', 'mellow', 'merry', 'mild', 'mirthful', 'moody',
    'mournful', 'nervous', 'noble', 'peaceful', 'pensive', 'placid', 'plucky',
    'proud', 'quiet', 'restless', 'rowdy', 'sedate', 'serene', 'solemn',
    'spirited', 'stern', 'stoic', 'sulky', 'sullen', 'tender', 'timid',
    'valiant', 'wistful', 'zealous', 'zany',

    /* --- mind & manner ------------------------------------------------ */
    'able', 'absurd', 'adept', 'adroit', 'artful', 'astute', 'awkward',
    'baffling', 'bizarre', 'bookish', 'brainy', 'bumbling', 'canny',
    'capable', 'careless', 'clever', 'clumsy', 'cunning', 'curious',
    'cryptic', 'dapper', 'deft', 'devious', 'diligent', 'dignified',
    'discreet', 'dodgy', 'dutiful', 'eccentric', 'educated', 'elegant',
    'eloquent', 'enigmatic', 'erudite', 'exacting', 'fancy', 'fastidious',
    'feckless', 'foolish', 'formal', 'frank', 'gaudy', 'genteel', 'graceful',
    'grandiose', 'gruff', 'guileless', 'handy', 'haughty', 'humble', 'idle',
    'ingenious', 'inquisitive', 'inventive', 'judicious', 'knavish',
    'learned', 'logical', 'lucid', 'manic', 'masterful', 'meticulous',
    'mindful', 'mischievous', 'modest', 'mysterious', 'nonchalant', 'obscure',
    'odd', 'opinionated', 'outlandish', 'particular', 'pedantic', 'peculiar',
    'perceptive', 'philosophical', 'playful', 'poetic', 'polite', 'pompous',
    'practical', 'pragmatic', 'precise', 'prudent', 'puzzling', 'quaint',
    'quirky', 'rakish', 'rational', 'refined', 'resourceful', 'sagacious',
    'savvy', 'scholarly', 'scrappy', 'shrewd', 'sly', 'sophisticated',
    'stately', 'studious', 'witty',

    /* --- weather, water & cold ---------------------------------------- */
    'arctic', 'autumnal', 'blustery', 'boreal', 'briny', 'chill', 'chilly',
    'clammy', 'cloudy', 'coastal', 'cold', 'crisp', 'cyclonic', 'damp',
    'dewy', 'drizzly', 'drenched', 'dripping', 'dry', 'dusty', 'foamy',
    'foggy', 'freezing', 'frigid', 'frosty', 'frozen', 'glacial', 'gusty',
    'hailing', 'hazy', 'humid', 'icebound', 'iced', 'icy', 'leeward',
    'marine', 'maritime', 'misty', 'monsoonal', 'muggy', 'nautical',
    'northern', 'oceanic', 'overcast', 'parched', 'polar', 'rainswept',
    'rainy', 'riparian', 'riverine', 'salty', 'sandy', 'seaborne', 'seaside',
    'sleety', 'slushy', 'snowbound', 'snowcapped', 'soaked', 'sodden',
    'southern', 'splashing', 'squally', 'stormy', 'subzero', 'swampy',
    'tempestuous', 'thawing', 'thundering', 'tidal', 'torrential', 'tropical',
    'turbulent', 'undersea', 'vaporous', 'wavy', 'weathered', 'wet',
    'windblown', 'windswept', 'windy', 'wintry', 'alpine', 'antarctic',
    'aquatic', 'bracing', 'brackish', 'equinoctial', 'estival', 'hibernal',
    'vernal', 'summery', 'sunless', 'flooded', 'marshy', 'reedy', 'silted',
    'sunbaked', 'windward',

    /* --- age, fate, myth & the uncanny -------------------------------- */
    'accursed', 'ageless', 'ancestral', 'ancient', 'antediluvian', 'antique',
    'arcane', 'astral', 'auspicious', 'banished', 'bewitched', 'blessed',
    'bygone', 'celestial', 'charmed', 'chosen', 'cosmic', 'crowned', 'cursed',
    'damned', 'deathless', 'demonic', 'destined', 'divine', 'doomed',
    'draconic', 'dreadful', 'dread', 'eerie', 'eldritch', 'elder',
    'elemental', 'elfin', 'enchanted', 'epic', 'ethereal', 'everlasting',
    'exiled', 'fabled', 'fated', 'fey', 'forbidden', 'forgotten', 'forsaken',
    'ghastly', 'ghostly', 'ghoulish', 'gothic', 'hallowed', 'haunted',
    'heathen', 'hexed', 'holy', 'immemorial', 'immortal', 'infernal',
    'inscrutable', 'invisible', 'legendary', 'lost', 'medieval', 'mythic',
    'necrotic', 'nocturnal', 'occult', 'olden', 'omened', 'oracular',
    'otherworldly', 'pagan', 'phantom', 'prehistoric', 'primal', 'primeval',
    'prophetic', 'runic', 'sacred', 'sacrificial', 'seraphic', 'shrouded',
    'sibylline', 'sorcerous', 'spectral', 'spellbound', 'storied', 'sunken',
    'timeless', 'titanic', 'transcendent', 'uncanny', 'undying', 'unearthly',
    'unhallowed', 'unseen', 'vampiric', 'venerable', 'vintage', 'wizened',
    'wraithlike', 'witching',

    /* --- wild, rare & flavourful -------------------------------------- */
    'abundant', 'adventurous', 'audacious', 'barefoot', 'bearded',
    'boundless', 'brash', 'bristling', 'buttered', 'candied', 'caramel',
    'cinnamon', 'citrus', 'clandestine', 'crumbling', 'delicious', 'electric',
    'endless', 'errant', 'exotic', 'famous', 'fearsome', 'feral', 'festive',
    'fortunate', 'free', 'freewheeling', 'fugitive', 'gallivanting',
    'glorious', 'hidden', 'hungry', 'illicit', 'imperial', 'infamous',
    'intrepid', 'itinerant', 'kingly', 'lawless', 'lonely', 'lucky',
    'luscious', 'magnificent', 'majestic', 'marooned', 'masked', 'maverick',
    'mighty', 'mutinous', 'naughty', 'nameless', 'notorious', 'outlaw',
    'outrageous', 'peppered', 'peerless', 'perilous', 'pickled', 'piratical',
    'plundered', 'poisoned', 'potent', 'precious', 'prized', 'quixotic',
    'rambunctious', 'ravenous', 'rebel', 'reckless', 'regal', 'renegade',
    'restive', 'riotous', 'roguish', 'royal', 'ruthless', 'salted', 'savage',
    'scandalous', 'scarce', 'secret', 'smuggled', 'solitary', 'sovereign',
    'splendid', 'stolen', 'sublime', 'sugared', 'sumptuous', 'supreme',
    'tempting', 'thieving', 'thrifty', 'triumphant', 'unbroken', 'unruly',
    'untamed', 'valorous', 'vagabond', 'wayward'
  ];

  var NOUNS = [
    /* --- beasts ------------------------------------------------------- */
    'antelope', 'ape', 'armadillo', 'baboon', 'badger', 'bear', 'beaver',
    'bison', 'boar', 'buffalo', 'bull', 'camel', 'capybara', 'caribou',
    'cheetah', 'chipmunk', 'cougar', 'coyote', 'deer', 'dingo', 'elephant',
    'elk', 'ermine', 'ferret', 'fox', 'gazelle', 'gibbon', 'giraffe',
    'gopher', 'goat', 'gorilla', 'grizzly', 'hare', 'hedgehog', 'hippo',
    'horse', 'hound', 'hyena', 'ibex', 'impala', 'jackal', 'jaguar',
    'kangaroo', 'koala', 'lemur', 'leopard', 'lion', 'llama', 'lynx',
    'mammoth', 'marmot', 'meerkat', 'mink', 'mole', 'mongoose', 'monkey',
    'moose', 'muskox', 'ocelot', 'okapi', 'opossum', 'otter', 'panda',
    'panther', 'pangolin', 'peccary', 'platypus', 'polecat', 'porcupine',
    'puma', 'rabbit', 'raccoon', 'reindeer', 'rhino', 'sable', 'serval',
    'shrew', 'skunk', 'sloth', 'squirrel', 'stallion', 'stoat', 'tapir',
    'tiger', 'wolf', 'zebra', 'weasel', 'wombat', 'yak', 'bobcat', 'buck',
    'stag', 'mustang', 'bat',

    /* --- birds -------------------------------------------------------- */
    'albatross', 'avocet', 'bittern', 'blackbird', 'bluebird', 'bunting',
    'buzzard', 'chickadee', 'cockatoo', 'condor', 'cormorant', 'crane',
    'crow', 'cuckoo', 'curlew', 'dove', 'duck', 'eagle', 'egret', 'emu',
    'falcon', 'finch', 'flamingo', 'gannet', 'goldfinch', 'goose', 'goshawk',
    'grebe', 'grouse', 'gull', 'harrier', 'hawk', 'heron', 'hoopoe',
    'hornbill', 'hummingbird', 'ibis', 'jackdaw', 'jay', 'kestrel',
    'kingfisher', 'kite', 'kiwi', 'kookaburra', 'lapwing', 'lark', 'loon',
    'macaw', 'magpie', 'mallard', 'martin', 'merlin', 'nightingale',
    'nightjar', 'nuthatch', 'oriole', 'osprey', 'ostrich', 'owl',
    'oystercatcher', 'parakeet', 'parrot', 'partridge', 'peacock', 'pelican',
    'penguin', 'peregrine', 'petrel', 'pheasant', 'plover', 'puffin', 'quail',
    'raven', 'redstart', 'roadrunner', 'robin', 'rook', 'sandpiper', 'shrike',
    'skylark', 'sparrow', 'starling', 'stork', 'swallow', 'swan', 'tanager',
    'tern', 'thrush', 'toucan', 'vulture', 'wagtail', 'warbler', 'waxwing',
    'woodpecker', 'wren',

    /* --- sea life & small creatures ----------------------------------- */
    'abalone', 'adder', 'anemone', 'angelfish', 'barnacle', 'barracuda',
    'beetle', 'boa', 'butterfly', 'caterpillar', 'catfish', 'chameleon',
    'cicada', 'cobra', 'crab', 'crayfish', 'cricket', 'crocodile',
    'cuttlefish', 'damselfly', 'dolphin', 'dragonfly', 'eel', 'firefly',
    'flounder', 'frog', 'gecko', 'glowworm', 'grasshopper', 'grouper',
    'hornet', 'iguana', 'jellyfish', 'krill', 'ladybird', 'lamprey', 'limpet',
    'lizard', 'lobster', 'locust', 'mackerel', 'manta', 'mantis', 'marlin',
    'mayfly', 'minnow', 'moth', 'mussel', 'narwhal', 'nautilus', 'newt',
    'octopus', 'orca', 'oyster', 'pike', 'piranha', 'plankton', 'python',
    'ray', 'salamander', 'salmon', 'scallop', 'scorpion', 'seahorse', 'seal',
    'shark', 'snail', 'snake', 'spider', 'sponge', 'squid', 'starfish',
    'stingray', 'sturgeon', 'swordfish', 'tadpole', 'tarantula', 'termite',
    'toad', 'tortoise', 'trout', 'tuna', 'turtle', 'urchin', 'viper',
    'walrus', 'wasp', 'whale', 'whelk',

    /* --- trees, plants & flowers -------------------------------------- */
    'acacia', 'acorn', 'alder', 'amaranth', 'ash', 'aspen', 'apple', 'aster',
    'azalea', 'bamboo', 'banyan', 'baobab', 'bayberry', 'beech', 'begonia',
    'birch', 'bluebell', 'bracken', 'bramble', 'briar', 'buttercup', 'cactus',
    'camellia', 'carnation', 'cattail', 'cedar', 'chrysanthemum', 'clover',
    'columbine', 'cornflower', 'crocus', 'cypress', 'daffodil', 'dahlia',
    'daisy', 'dandelion', 'elderberry', 'elm', 'fern', 'fir', 'foxglove',
    'gardenia', 'ginkgo', 'ginseng', 'gorse', 'hawthorn', 'heather',
    'hemlock', 'hibiscus', 'holly', 'honeysuckle', 'hyacinth', 'iris', 'ivy',
    'jasmine', 'juniper', 'larch', 'laurel', 'lichen', 'lily', 'lotus',
    'lupine', 'magnolia', 'mandrake', 'mangrove', 'maple', 'marigold',
    'mistletoe', 'moss', 'mushroom', 'myrtle', 'nettle', 'oak', 'orchid',
    'palm', 'pansy', 'papyrus', 'peony', 'pine', 'poppy', 'primrose',
    'redwood', 'reed', 'rose', 'sequoia', 'shamrock', 'snapdragon', 'sorrel',
    'spruce', 'sunflower', 'sycamore', 'thistle', 'thorn', 'toadstool',
    'tulip', 'vine', 'willow', 'wisteria', 'wormwood', 'yarrow', 'yew',
    'zinnia',

    /* --- land & sky --------------------------------------------------- */
    'basin', 'bay', 'beach', 'bluff', 'boulder', 'brook', 'butte', 'canyon',
    'cape', 'cavern', 'cave', 'chasm', 'cliff', 'cloud', 'comet', 'cove',
    'crag', 'crater', 'creek', 'crest', 'delta', 'desert', 'dune', 'dusk',
    'eclipse', 'estuary', 'fjord', 'flood', 'forest', 'fountain', 'frost',
    'galaxy', 'geyser', 'glacier', 'glade', 'gorge', 'grotto', 'gulf',
    'harbour', 'haze', 'headland', 'highland', 'iceberg', 'inlet', 'island',
    'isle', 'jungle', 'lagoon', 'lake', 'landslide', 'lightning', 'marsh',
    'meadow', 'mesa', 'meteor', 'mire', 'mist', 'moon', 'moor', 'mountain',
    'nebula', 'oasis', 'ocean', 'orbit', 'peak', 'pinnacle', 'planet',
    'plateau', 'pond', 'prairie', 'quarry', 'quicksand', 'rainbow', 'rapids',
    'ravine', 'reef', 'ridge', 'rift', 'river', 'savanna', 'sea', 'shore',
    'sky', 'star', 'steppe', 'storm', 'stream', 'summit', 'sun', 'sunrise',
    'sunset', 'swamp', 'thunder', 'tide', 'tundra', 'valley', 'volcano',
    'waterfall', 'wave', 'wilderness', 'wind',

    /* --- stone, metal & treasure -------------------------------------- */
    'agate', 'amethyst', 'amulet', 'bauble', 'beryl', 'chalice', 'cinnabar',
    'citrine', 'crown', 'diadem', 'doubloon', 'filigree', 'geode', 'hoard',
    'jewel', 'lapis', 'lodestone', 'malachite', 'medallion', 'mercury',
    'moonstone', 'nugget', 'obelisk', 'pearl', 'peridot', 'prism', 'pyrite',
    'relic', 'regalia', 'ring', 'scepter', 'sovereign', 'sunstone',
    'talisman', 'tanzanite', 'tiara', 'torc', 'treasure', 'trinket', 'locket',
    'brooch', 'ruby', 'sapphire', 'emerald', 'diamond', 'topaz', 'opal',
    'onyx', 'jade', 'amber', 'ivory', 'quartz', 'granite', 'marble',
    'obsidian', 'flint', 'slate', 'jasper', 'garnet', 'turquoise', 'silver',
    'gold', 'copper', 'bronze', 'brass', 'iron', 'steel', 'pewter',
    'platinum', 'zircon',

    /* --- tools, weapons & machines ------------------------------------ */
    'anchor', 'anvil', 'arrow', 'axe', 'ballista', 'bellows', 'blade', 'bolt',
    'bow', 'bullet', 'cannon', 'catapult', 'chain', 'chisel', 'cleaver',
    'clockwork', 'compass', 'crank', 'crossbow', 'cutlass', 'dagger', 'dart',
    'drill', 'dynamo', 'engine', 'flail', 'forge', 'gauntlet', 'gear',
    'glaive', 'grinder', 'halberd', 'hammer', 'harpoon', 'hatchet', 'helm',
    'javelin', 'katana', 'knife', 'key', 'ladder', 'lance', 'lantern',
    'lasso', 'lathe', 'lever', 'longbow', 'mace', 'machete', 'mallet', 'mast',
    'mortar', 'musket', 'needle', 'oar', 'piston', 'pistol', 'pitchfork',
    'plough', 'pulley', 'pump', 'quiver', 'rapier', 'ratchet', 'revolver',
    'rifle', 'rivet', 'rocket', 'rudder', 'saber', 'scalpel', 'scythe',
    'shield', 'shovel', 'sickle', 'sledge', 'spanner', 'spear', 'spike',
    'spindle', 'spyglass', 'staff', 'stiletto', 'tinderbox', 'tongs', 'torch',
    'trident', 'trigger', 'turbine', 'wand', 'wedge', 'whetstone', 'whip',
    'winch', 'wrench', 'yoke',

    /* --- places & structures ------------------------------------------ */
    'abbey', 'alcove', 'alley', 'aqueduct', 'arcade', 'arch', 'armoury',
    'arena', 'atrium', 'bailey', 'balcony', 'barracks', 'bastion', 'bazaar',
    'beacon', 'belfry', 'bridge', 'bunker', 'buttress', 'cabin', 'castle',
    'catacomb', 'cathedral', 'causeway', 'chamber', 'chapel', 'citadel',
    'cloister', 'colonnade', 'corridor', 'cottage', 'courtyard', 'crossroads',
    'crypt', 'dome', 'drawbridge', 'dungeon', 'estate', 'factory', 'fort',
    'fortress', 'foundry', 'gallery', 'garden', 'garret', 'gate', 'hall',
    'gatehouse', 'granary', 'greenhouse', 'hamlet', 'hearth', 'hive',
    'hostel', 'keep', 'kiln', 'laboratory', 'labyrinth', 'lighthouse',
    'lodge', 'loft', 'manor', 'market', 'maze', 'mill', 'mine', 'minaret',
    'monastery', 'moat', 'monument', 'museum', 'nest', 'observatory',
    'orchard', 'outpost', 'palace', 'pagoda', 'parapet', 'parlour',
    'pavilion', 'pier', 'pillar', 'plaza', 'port', 'portal', 'quay',
    'rampart', 'refinery', 'sanctuary', 'shipyard', 'shrine', 'silo', 'spire',
    'stable', 'stairway', 'station', 'stockade', 'tavern', 'temple',
    'terrace', 'theatre', 'tower', 'tunnel', 'turret', 'vault', 'veranda',
    'viaduct', 'village', 'wharf', 'windmill', 'workshop', 'ziggurat',

    /* --- myth, magic & the uncanny ------------------------------------ */
    'alchemist', 'angel', 'apparition', 'augur', 'banshee', 'basilisk',
    'behemoth', 'cauldron', 'centaur', 'chimera', 'cipher', 'colossus',
    'coven', 'curse', 'cyclops', 'daemon', 'deity', 'djinn', 'doppelganger',
    'dragon', 'dryad', 'druid', 'effigy', 'elf', 'enchanter', 'fable',
    'fairy', 'fate', 'gargoyle', 'ghost', 'ghoul', 'giant', 'gnome', 'goblin',
    'golem', 'gorgon', 'grail', 'gremlin', 'griffin', 'grimoire', 'harpy',
    'haunt', 'hex', 'hydra', 'idol', 'imp', 'incantation', 'jinx', 'kelpie',
    'kraken', 'leviathan', 'lich', 'mage', 'magus', 'manticore', 'mermaid',
    'minotaur', 'mirage', 'myth', 'naiad', 'necromancer', 'nymph', 'ogre',
    'omen', 'oracle', 'phantom', 'phoenix', 'pixie', 'poltergeist', 'portent',
    'prophecy', 'revenant', 'rite', 'rune', 'satyr', 'seance', 'seer',
    'selkie', 'shade', 'shaman', 'siren', 'sorcerer', 'specter', 'sphinx',
    'spell', 'spirit', 'sprite', 'titan', 'totem', 'troll', 'unicorn',
    'valkyrie', 'vampire', 'vision', 'warlock', 'werewolf', 'witch', 'wizard',
    'wraith', 'wyrm', 'wyvern', 'yeti', 'zombie',

    /* --- things, music & time ----------------------------------------- */
    'accordion', 'anthem', 'aria', 'badge', 'ballad', 'balloon', 'banjo',
    'banner', 'bassoon', 'bell', 'blanket', 'book', 'boot', 'bottle',
    'bracelet', 'brush', 'bucket', 'bugle', 'candle', 'carousel', 'carpet',
    'cart', 'chalk', 'chime', 'chorus', 'cloak', 'clock', 'comb', 'cord',
    'cradle', 'crate', 'curtain', 'cymbal', 'dial', 'diary', 'dice', 'doll',
    'drum', 'echo', 'envelope', 'fan', 'feather', 'fiddle', 'flag', 'flask',
    'flute', 'gong', 'guitar', 'harp', 'hourglass', 'inkwell', 'journal',
    'kaleidoscope', 'kettle', 'lamp', 'ledger', 'lens', 'letter', 'lullaby',
    'lute', 'lyre', 'mandolin', 'mask', 'medal', 'melody', 'metronome',
    'mirror', 'monocle', 'note', 'oboe', 'ocarina', 'organ', 'pendulum',
    'pennant', 'piano', 'pocket', 'postcard', 'pouch', 'puzzle', 'quill',
    'quilt', 'radio', 'rattle', 'record', 'ribbon', 'sash', 'satchel',
    'saxophone', 'scarf', 'scroll', 'sextant', 'shawl', 'sonata', 'song',
    'spectacles', 'stamp', 'suitcase', 'symphony', 'tambourine', 'tapestry',
    'teapot', 'thimble', 'ticket', 'trumpet', 'tuba', 'typewriter', 'ukulele',
    'umbrella', 'urn', 'vase', 'violin', 'waltz', 'watch', 'whistle',
    'xylophone', 'yarn', 'zither', 'aeon', 'autumn', 'century', 'dawn',
    'decade', 'epoch', 'equinox', 'era', 'evening', 'hour', 'midnight',
    'morning', 'night', 'noon', 'season', 'solstice', 'summer', 'twilight',
    'winter', 'eternity', 'millennium'
  ];

  var WORDS = { adjectives: ADJECTIVES, nouns: NOUNS };

  root.NAME_WORDS = WORDS;
  if (typeof module !== 'undefined' && module.exports) {
    module.exports = WORDS;
  }
})(typeof window !== 'undefined' ? window : this);
