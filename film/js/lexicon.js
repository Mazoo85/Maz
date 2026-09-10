/*
 * SCRIPT FORGE — the lexicon.
 * ---------------------------
 * Every word this app can write comes from here: the genre keyword lists it
 * reads your idea with, the sensory details it dresses scenes in, and the
 * dialogue exchanges it stages between characters.
 *
 * Nothing in this file touches the DOM, so the tests can load it in Node.
 *
 * Placeholder slots used by action lines and dialogue:
 *   {HERO}   the protagonist's name          {OTHER}  the second character
 *   {OBJ}    the thing the story turns on    {PLACE}  the main location
 *   {WANT}   what the hero is after          {DETAIL} a genre sensory detail
 *
 * Exposed as window.FILM_LEXICON (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  /* ------------------------------------------------------------------ genres
   * `keywords` are matched against the idea you type — the genre with the most
   * hits wins. Everything else in the entry is the texture that genre writes
   * with once it has won.
   */
  var GENRES = {
    drama: {
      label: 'Drama',
      keywords: [
        'family', 'father', 'dad', 'mother', 'mom', 'brother', 'sister', 'son',
        'daughter', 'funeral', 'hospital', 'divorce', 'cancer', 'debt', 'home',
        'grief', 'apology', 'sober', 'rent', 'eviction', 'goodbye', 'memory',
        'forgive', 'estranged', 'inheritance', 'diagnosis', 'caregiver'
      ],
      time: 'DAY',
      details: [
        'Dust turns over in a bar of window light.',
        'A clock ticks somewhere out of frame.',
        'Coffee has gone cold in two cups.',
        'The radiator knocks twice and gives up.',
        'Rain finds the same crack in the sill it always finds.',
        'A photograph lies face down on the counter.'
      ],
      sounds: ['a kettle building', 'traffic three floors down', 'a screen door', 'a dog barking two yards over'],
      places: ['kitchen', 'porch', 'hospital', 'car'],
      objects: ['letter', 'photograph', 'set of keys', 'wedding ring', 'voicemail']
    },
    thriller: {
      label: 'Thriller',
      keywords: [
        'chase', 'kill', 'hunt', 'stalker', 'gun', 'murder', 'missing', 'escape',
        'trapped', 'cop', 'police', 'hostage', 'bomb', 'deadline', 'blackmail',
        'spy', 'witness', 'threat', 'ransom', 'assassin', 'follow', 'followed',
        'run', 'hide', 'survive', 'weapon', 'danger'
      ],
      time: 'NIGHT',
      details: [
        'Headlights sweep the ceiling and move on.',
        'The room holds its breath.',
        'Somewhere below, a door closes that should not have opened.',
        'A phone screen lights the floor and dies.',
        'Every sound in the building suddenly has a direction.',
        'The lock is old. The lock is the only thing between them.'
      ],
      sounds: ['an engine idling', 'footsteps on a stairwell', 'a phone buzzing face-down', 'a siren pulling away'],
      places: ['parking garage', 'motel room', 'stairwell', 'alley'],
      objects: ['burner phone', 'envelope of cash', 'car key', 'flash drive', 'gun']
    },
    horror: {
      label: 'Horror',
      keywords: [
        'ghost', 'haunted', 'demon', 'blood', 'monster', 'creature', 'basement',
        'scream', 'ritual', 'cursed', 'possessed', 'dead', 'grave', 'attic',
        'woods', 'nightmare', 'evil', 'shadow', 'corpse', 'ouija', 'exorcism',
        'thing', 'crawl', 'teeth'
      ],
      time: 'NIGHT',
      details: [
        'The dark past the doorway does not behave like dark.',
        'Something in the walls stops moving the moment it is heard.',
        'Breath fogs. The heat is on.',
        'The reflection lags by half a second.',
        'A smell like wet pennies rides the draft.',
        'The floorboards remember a weight that is not there.'
      ],
      sounds: ['a wet dragging sound', 'nails on plaster', 'breathing that is not theirs', 'a hymn played too slow'],
      places: ['basement', 'attic', 'woods', 'hallway'],
      objects: ['music box', 'old photograph', 'handful of teeth', 'video tape', 'doll']
    },
    comedy: {
      label: 'Comedy',
      keywords: [
        'funny', 'awkward', 'wedding', 'date', 'boss', 'prank', 'roommate',
        'karaoke', 'interview', 'dating', 'app', 'party', 'dog', 'cat', 'birthday',
        'hangover', 'wrong', 'accidentally', 'embarrassing', 'lie', 'pretend',
        'influencer', 'group chat'
      ],
      time: 'DAY',
      details: [
        'The silence goes on one full second too long.',
        'Everyone in the room decides to look at something else.',
        'A smoke alarm chirps for a battery nobody will replace.',
        'The playlist chooses violence.',
        'Someone has laminated something that did not need laminating.',
        'The chair makes a noise. It was the chair.'
      ],
      sounds: ['a blender at the worst moment', 'polite applause', 'a group chat going off', 'a microwave beeping'],
      places: ['office', 'kitchen', 'restaurant', 'apartment'],
      objects: ['phone', 'cake', 'name tag', 'ring box', 'group chat']
    },
    romance: {
      label: 'Romance',
      keywords: [
        'love', 'crush', 'kiss', 'ex', 'heartbreak', 'married', 'valentine',
        'anniversary', 'flirt', 'romantic', 'girlfriend', 'boyfriend', 'partner',
        'wedding', 'letter', 'dance', 'reunion', 'first', 'again'
      ],
      time: 'DUSK',
      details: [
        'Neither of them looks at the door.',
        'Streetlight makes the whole block look kinder than it is.',
        'Two coats on one hook.',
        'The song is not good. They stay for it anyway.',
        'A hand almost moves and does not.',
        'The last of the light goes gold on the wall.'
      ],
      sounds: ['a record ending', 'rain on an awning', 'a bar three doors down', 'a train slowing'],
      places: ['bar', 'rooftop', 'train station', 'kitchen'],
      objects: ['letter', 'mixtape', 'photograph', 'second key', 'plane ticket']
    },
    scifi: {
      label: 'Sci-Fi',
      keywords: [
        'robot', 'ai', 'space', 'alien', 'future', 'android', 'machine',
        'simulation', 'portal', 'mars', 'clone', 'signal', 'satellite', 'time',
        'travel', 'spaceship', 'planet', 'orbit', 'lab', 'experiment', 'upload',
        'memory', 'algorithm', 'drone', 'cyborg'
      ],
      time: 'NIGHT',
      details: [
        'A status light cycles through a colour that has no name.',
        'The air recycles with a sound like a held note.',
        'Numbers change on a screen nobody is reading.',
        'Everything here is clean in a way that took work.',
        'The machine waits with the patience of a thing that does not age.',
        'Somewhere a fan spins up, then thinks better of it.'
      ],
      sounds: ['a low electrical hum', 'a pressure door sealing', 'static resolving into speech', 'a heart monitor'],
      places: ['lab', 'server room', 'spaceship', 'apartment'],
      objects: ['transmitter', 'data drive', 'prototype', 'recording', 'sample']
    },
    mystery: {
      label: 'Mystery',
      keywords: [
        'detective', 'clue', 'secret', 'disappear', 'disappeared', 'body', 'case',
        'investigate', 'unsolved', 'suspect', 'alibi', 'evidence', 'lie', 'truth',
        'cold', 'files', 'interrogation', 'vanished', 'diary'
      ],
      time: 'NIGHT',
      details: [
        'The room has been tidied by someone in a hurry.',
        'Every object here is exactly one inch from where it should be.',
        'The date on the receipt does not work.',
        'A drawer has been emptied and closed politely.',
        'There is one cup too many in the sink.',
        'The dust says the frame was moved this week.'
      ],
      sounds: ['a tape recorder clicking', 'rain on a window', 'a landline ringing out', 'pages turning'],
      places: ['office', 'apartment', 'police station', 'diner'],
      objects: ['case file', 'photograph', 'diary', 'receipt', 'key with no label']
    },
    fantasy: {
      label: 'Fantasy',
      keywords: [
        'magic', 'witch', 'dragon', 'wizard', 'spell', 'fairy', 'curse', 'kingdom',
        'sword', 'prophecy', 'enchanted', 'quest', 'realm', 'sorcerer', 'rune',
        'creature', 'forest', 'crown'
      ],
      time: 'DUSK',
      details: [
        'The candle burns a colour candles do not burn.',
        'Frost crawls up the glass against the season.',
        'The woods go quiet the way a room goes quiet.',
        'Something old is awake and being polite about it.',
        'The chalk lines are still warm.',
        'The wind moves through the room without moving the curtains.'
      ],
      sounds: ['a bell with no rope', 'wings too large for the room', 'a language nobody living speaks', 'water where there is no water'],
      places: ['forest', 'library', 'cottage', 'ruins'],
      objects: ['locket', 'old book', 'ring', 'blade', 'jar of ash']
    },
    heist: {
      label: 'Heist',
      keywords: [
        'heist', 'steal', 'rob', 'robbery', 'bank', 'vault', 'crew', 'safe',
        'diamonds', 'con', 'thief', 'score', 'launder', 'forgery', 'inside', 'job'
      ],
      time: 'NIGHT',
      details: [
        'The plan is drawn on the back of something that will be burned.',
        'Two minutes on the clock and nobody is looking at the clock.',
        'The camera pans away on a nine-second cycle. It is second one.',
        'Gloves. Everyone remembered the gloves except one of them.',
        'The floor plan has a room on it that is not on the other floor plan.',
        'Someone has been counting under their breath since they walked in.'
      ],
      sounds: ['a tumbler falling', 'a radio check', 'an alarm arming', 'a van door'],
      places: ['warehouse', 'bank', 'parking garage', 'motel room'],
      objects: ['blueprint', 'duffel bag', 'key card', 'wire cutters', 'burner phone']
    },
    western: {
      label: 'Western',
      keywords: [
        'cowboy', 'sheriff', 'saloon', 'horse', 'desert', 'outlaw', 'frontier',
        'ranch', 'gunslinger', 'wagon', 'marshal', 'homestead', 'dust'
      ],
      time: 'DAY',
      details: [
        'Heat stands over the road like a second road.',
        'Dust takes its time coming back down.',
        'The porch boards have been walked into a curve.',
        'Nothing out here is in a hurry, including the trouble.',
        'A horse shifts its weight and settles.',
        'The shade is the only thing worth owning today.'
      ],
      sounds: ['a screen door on a spring', 'flies', 'a distant rifle', 'boot heels on boards'],
      places: ['saloon', 'porch', 'desert', 'barn'],
      objects: ['telegram', 'rifle', 'deed', 'canteen', 'tin star']
    }
  };

  /* ------------------------------------------------------------------ places
   * Every place a scene heading can be built from, with the INT./EXT. that
   * goes with it and how it should read in a slug line.
   */
  var PLACES = {
    kitchen:          { slug: 'KITCHEN', int: 'INT.' },
    apartment:        { slug: 'APARTMENT', int: 'INT.' },
    bedroom:          { slug: 'BEDROOM', int: 'INT.' },
    'living room':    { slug: 'LIVING ROOM', int: 'INT.' },
    bathroom:         { slug: 'BATHROOM', int: 'INT.' },
    hallway:          { slug: 'HALLWAY', int: 'INT.' },
    stairwell:        { slug: 'STAIRWELL', int: 'INT.' },
    basement:         { slug: 'BASEMENT', int: 'INT.' },
    attic:            { slug: 'ATTIC', int: 'INT.' },
    garage:           { slug: 'GARAGE', int: 'INT.' },
    porch:            { slug: 'FRONT PORCH', int: 'EXT.' },
    yard:             { slug: 'BACK YARD', int: 'EXT.' },
    house:            { slug: 'HOUSE', int: 'INT.' },
    cabin:            { slug: 'CABIN', int: 'INT.' },
    cottage:          { slug: 'COTTAGE', int: 'INT.' },
    motel:            { slug: 'MOTEL ROOM', int: 'INT.' },
    'motel room':     { slug: 'MOTEL ROOM', int: 'INT.' },
    hotel:            { slug: 'HOTEL ROOM', int: 'INT.' },
    diner:            { slug: 'DINER', int: 'INT.' },
    restaurant:       { slug: 'RESTAURANT', int: 'INT.' },
    cafe:             { slug: 'CAFÉ', int: 'INT.' },
    'coffee shop':    { slug: 'CAFÉ', int: 'INT.' },
    bar:              { slug: 'BAR', int: 'INT.' },
    saloon:           { slug: 'SALOON', int: 'INT.' },
    office:           { slug: 'OFFICE', int: 'INT.' },
    school:           { slug: 'SCHOOL — CLASSROOM', int: 'INT.' },
    classroom:        { slug: 'CLASSROOM', int: 'INT.' },
    library:          { slug: 'LIBRARY', int: 'INT.' },
    church:           { slug: 'CHURCH', int: 'INT.' },
    hospital:         { slug: 'HOSPITAL — ROOM 4', int: 'INT.' },
    clinic:           { slug: 'CLINIC', int: 'INT.' },
    lab:              { slug: 'LAB', int: 'INT.' },
    laboratory:       { slug: 'LAB', int: 'INT.' },
    'server room':    { slug: 'SERVER ROOM', int: 'INT.' },
    warehouse:        { slug: 'WAREHOUSE', int: 'INT.' },
    factory:          { slug: 'FACTORY FLOOR', int: 'INT.' },
    bank:             { slug: 'BANK — LOBBY', int: 'INT.' },
    'police station': { slug: 'POLICE STATION', int: 'INT.' },
    prison:           { slug: 'PRISON — VISITING ROOM', int: 'INT.' },
    'parking garage': { slug: 'PARKING GARAGE — LEVEL 3', int: 'INT.' },
    'parking lot':    { slug: 'PARKING LOT', int: 'EXT.' },
    car:              { slug: 'CAR — MOVING', int: 'INT.' },
    truck:            { slug: 'TRUCK — MOVING', int: 'INT.' },
    bus:              { slug: 'BUS', int: 'INT.' },
    train:            { slug: 'TRAIN CAR', int: 'INT.' },
    'train station':  { slug: 'TRAIN STATION', int: 'INT.' },
    subway:           { slug: 'SUBWAY PLATFORM', int: 'INT.' },
    airport:          { slug: 'AIRPORT — GATE 12', int: 'INT.' },
    spaceship:        { slug: 'SHIP — CREW DECK', int: 'INT.' },
    station:          { slug: 'STATION — CORRIDOR', int: 'INT.' },
    lighthouse:       { slug: 'LIGHTHOUSE — LAMP ROOM', int: 'INT.' },
    barn:             { slug: 'BARN', int: 'INT.' },
    farm:             { slug: 'FARM', int: 'EXT.' },
    ranch:            { slug: 'RANCH', int: 'EXT.' },
    woods:            { slug: 'WOODS', int: 'EXT.' },
    forest:           { slug: 'FOREST', int: 'EXT.' },
    beach:            { slug: 'BEACH', int: 'EXT.' },
    lake:             { slug: 'LAKESHORE', int: 'EXT.' },
    river:            { slug: 'RIVERBANK', int: 'EXT.' },
    desert:           { slug: 'DESERT ROAD', int: 'EXT.' },
    mountain:         { slug: 'MOUNTAIN PASS', int: 'EXT.' },
    field:            { slug: 'OPEN FIELD', int: 'EXT.' },
    park:             { slug: 'PARK', int: 'EXT.' },
    rooftop:          { slug: 'ROOFTOP', int: 'EXT.' },
    roof:             { slug: 'ROOFTOP', int: 'EXT.' },
    alley:            { slug: 'ALLEY', int: 'EXT.' },
    street:           { slug: 'STREET', int: 'EXT.' },
    city:             { slug: 'CITY STREET', int: 'EXT.' },
    town:             { slug: 'MAIN STREET', int: 'EXT.' },
    bridge:           { slug: 'BRIDGE', int: 'EXT.' },
    graveyard:        { slug: 'GRAVEYARD', int: 'EXT.' },
    cemetery:         { slug: 'CEMETERY', int: 'EXT.' },
    ruins:            { slug: 'RUINS', int: 'EXT.' },
    island:           { slug: 'SHORELINE', int: 'EXT.' },
    space:            { slug: 'SPACE', int: 'EXT.' },
    gym:              { slug: 'GYM', int: 'INT.' },
    store:            { slug: 'CORNER STORE', int: 'INT.' },
    shop:             { slug: 'SHOP', int: 'INT.' },
    laundromat:       { slug: 'LAUNDROMAT', int: 'INT.' },
    'gas station':    { slug: 'GAS STATION', int: 'EXT.' },
    'waiting room':   { slug: 'WAITING ROOM', int: 'INT.' },
    elevator:         { slug: 'ELEVATOR', int: 'INT.' },
    boat:             { slug: 'BOAT — DECK', int: 'EXT.' },
    dock:             { slug: 'DOCK', int: 'EXT.' },
    theater:          { slug: 'THEATER', int: 'INT.' },
    studio:           { slug: 'STUDIO', int: 'INT.' }
  };

  /* ------------------------------------------------------------------- roles
   * Occupation and relationship words. Finding one in your idea tells the app
   * who the story is about, where they would plausibly be, and what is in
   * their hands.
   */
  var ROLES = {
    'lighthouse keeper': { place: 'lighthouse', object: 'radio' },
    'police officer':    { place: 'police station', object: 'badge' },
    'social worker':     { place: 'office', object: 'case file' },
    'flight attendant':  { place: 'airport', object: 'boarding pass' },
    'night nurse':       { place: 'hospital', object: 'chart' },
    'security guard':    { place: 'warehouse', object: 'flashlight' },
    'delivery driver':   { place: 'car', object: 'package' },
    'bus driver':        { place: 'bus', object: 'transfer slip' },
    'taxi driver':       { place: 'car', object: 'fare receipt' },
    detective:  { place: 'police station', object: 'case file' },
    nurse:      { place: 'hospital', object: 'chart' },
    doctor:     { place: 'hospital', object: 'test result' },
    teacher:    { place: 'classroom', object: 'stack of essays' },
    student:    { place: 'school', object: 'acceptance letter' },
    barista:    { place: 'cafe', object: 'paper cup' },
    waitress:   { place: 'diner', object: 'order pad' },
    waiter:     { place: 'restaurant', object: 'order pad' },
    bartender:  { place: 'bar', object: 'bar rag' },
    janitor:    { place: 'school', object: 'ring of keys' },
    mechanic:   { place: 'garage', object: 'wrench' },
    farmer:     { place: 'farm', object: 'foreclosure notice' },
    fisherman:  { place: 'dock', object: 'net' },
    soldier:    { place: 'desert', object: 'letter from home' },
    priest:     { place: 'church', object: 'rosary' },
    lawyer:     { place: 'office', object: 'contract' },
    astronaut:  { place: 'spaceship', object: 'recording' },
    scientist:  { place: 'lab', object: 'sample' },
    engineer:   { place: 'lab', object: 'prototype' },
    programmer: { place: 'server room', object: 'laptop' },
    hacker:     { place: 'apartment', object: 'flash drive' },
    thief:      { place: 'parking garage', object: 'duffel bag' },
    burglar:    { place: 'house', object: 'crowbar' },
    sheriff:    { place: 'saloon', object: 'tin star' },
    cowboy:     { place: 'ranch', object: 'canteen' },
    witch:      { place: 'cottage', object: 'jar of ash' },
    wizard:     { place: 'library', object: 'old book' },
    knight:     { place: 'ruins', object: 'blade' },
    writer:     { place: 'apartment', object: 'manuscript' },
    painter:    { place: 'studio', object: 'unfinished canvas' },
    musician:   { place: 'bar', object: 'guitar case' },
    actor:      { place: 'theater', object: 'audition sides' },
    photographer: { place: 'apartment', object: 'roll of film' },
    journalist: { place: 'office', object: 'notebook' },
    boxer:      { place: 'gym', object: 'hand wraps' },
    dancer:     { place: 'studio', object: 'pair of shoes' },
    chef:       { place: 'kitchen', object: 'knife roll' },
    babysitter: { place: 'house', object: 'baby monitor' },
    kid:        { place: 'yard', object: 'walkie-talkie' },
    boy:        { place: 'yard', object: 'walkie-talkie' },
    girl:       { place: 'yard', object: 'walkie-talkie' },
    teenager:   { place: 'bedroom', object: 'phone' },
    widow:      { place: 'kitchen', object: 'wedding ring' },
    widower:    { place: 'kitchen', object: 'wedding ring' },
    mother:     { place: 'kitchen', object: 'photograph' },
    mom:        { place: 'kitchen', object: 'photograph' },
    father:     { place: 'garage', object: 'set of keys' },
    dad:        { place: 'garage', object: 'set of keys' },
    daughter:   { place: 'bedroom', object: 'letter' },
    son:        { place: 'porch', object: 'letter' },
    sister:     { place: 'car', object: 'voicemail' },
    brother:    { place: 'car', object: 'voicemail' },
    grandmother:{ place: 'kitchen', object: 'recipe card' },
    grandfather:{ place: 'porch', object: 'pocket watch' },
    twin:       { place: 'bedroom', object: 'photograph' },
    neighbour:  { place: 'porch', object: 'casserole dish' },
    neighbor:   { place: 'porch', object: 'casserole dish' },
    stranger:   { place: 'diner', object: 'envelope' },
    robot:      { place: 'lab', object: 'power cell' },
    android:    { place: 'lab', object: 'memory core' },
    ghost:      { place: 'house', object: 'music box' },
    alien:      { place: 'field', object: 'transmitter' },
    veteran:    { place: 'porch', object: 'folded flag' },
    addict:     { place: 'motel room', object: 'chip' },
    'ex-con':   { place: 'bus', object: 'release papers' },
    driver:     { place: 'car', object: 'road map' },
    guard:      { place: 'prison', object: 'ring of keys' },
    clerk:      { place: 'store', object: 'lottery ticket' },
    manager:    { place: 'office', object: 'termination letter' },
    intern:     { place: 'office', object: 'name tag' },
    'best friend': { place: 'apartment', object: 'second key' },
    woman:      { place: 'apartment', object: 'letter' },
    man:        { place: 'apartment', object: 'letter' },
    guy:        { place: 'bar', object: 'phone' },
    person:     { place: 'apartment', object: 'letter' },
    friend:     { place: 'apartment', object: 'second key' },
    boss:       { place: 'office', object: 'termination letter' },
    partner:    { place: 'kitchen', object: 'second key' },
    couple:     { place: 'car', object: 'plane ticket' },
    roommate:   { place: 'apartment', object: 'unpaid bill' },
    'old man':  { place: 'porch', object: 'pocket watch' },
    'old woman':{ place: 'kitchen', object: 'recipe card' }
  };

  /* Objects the parser will recognise on their own, without a trigger verb. */
  var OBJECTS = [
    'radio', 'letter', 'photograph', 'photo', 'phone', 'tape', 'video tape',
    'diary', 'journal', 'map', 'key', 'keys', 'ring', 'necklace', 'locket',
    'box', 'suitcase', 'briefcase', 'envelope', 'package', 'gun', 'knife',
    'camera', 'painting', 'book', 'notebook', 'recording', 'message', 'note',
    'ticket', 'watch', 'mirror', 'doll', 'music box', 'machine', 'computer',
    'laptop', 'drive', 'flash drive', 'car', 'dog', 'cat', 'baby', 'money',
    'cash', 'deed', 'will', 'contract', 'telegram', 'postcard', 'bottle',
    'guitar', 'coin', 'stone', 'seed', 'egg', 'door', 'suit', 'mask', 'sword',
    'walkie-talkie', 'answering machine', 'typewriter', 'projector', 'telescope',
    'teddy bear', 'urn', 'casserole', 'lottery ticket', 'sonogram', 'chessboard'
  ];

  /* Given names for characters your idea did not name. Neutral by design —
   * the script says nothing about anyone's gender unless you do. */
  var NAMES = [
    'RILEY', 'MO', 'SAM', 'JUNE', 'ELLIS', 'KAI', 'NOOR', 'DREW', 'ALEX',
    'TAM', 'VIC', 'LOU', 'REN', 'ARI', 'JOSS', 'MARLOW', 'QUINN', 'BEX',
    'ODIE', 'SHAY', 'CASS', 'NIKO', 'PIP', 'EMORY', 'HOLLIS', 'WREN'
  ];

  /* Second-character relationships, used when your idea names only one person. */
  var FOILS = [
    { name: null, role: 'sister' }, { name: null, role: 'old friend' },
    { name: null, role: 'neighbour' }, { name: null, role: 'stranger' },
    { name: null, role: 'boss' }, { name: null, role: 'brother' },
    { name: null, role: 'daughter' }, { name: null, role: 'partner' }
  ];

  /* What the hero is after. Trigger word in the idea → the want it implies. */
  var WANTS = [
    { keys: ['escape', 'run', 'leave', 'flee', 'quit'], want: 'get out before it is too late' },
    { keys: ['find', 'search', 'looking', 'missing', 'lost'], want: 'find what went missing' },
    { keys: ['save', 'rescue', 'protect'], want: 'get them out alive' },
    { keys: ['tell', 'confess', 'admit', 'truth', 'secret'], want: 'say the thing out loud' },
    { keys: ['steal', 'rob', 'take', 'heist'], want: 'take it and get clear' },
    { keys: ['prove', 'evidence', 'clear'], want: 'prove it before anyone believes otherwise' },
    { keys: ['forgive', 'apologize', 'apologise', 'sorry'], want: 'be forgiven' },
    { keys: ['win', 'beat', 'compete', 'race'], want: 'win it once, properly' },
    { keys: ['remember', 'memory', 'forget'], want: 'hold on to what is left' },
    { keys: ['home', 'return', 'back'], want: 'get home' },
    { keys: ['love', 'kiss', 'date'], want: 'be honest about how they feel' },
    { keys: ['stop', 'prevent'], want: 'stop it happening again' },
    { keys: ['goodbye', 'dying', 'dies', 'funeral', 'death', 'buried'], want: 'say goodbye properly' },
    { keys: ['debt', 'rent', 'owe', 'broke', 'paid'], want: 'make the money work, just this once' },
    { keys: ['answer', 'why', 'question'], want: 'get an answer they can live with' }
  ];

  /* ------------------------------------------------------------------- beats
   * The spine of a short film. Each beat knows what it is for, so the action
   * and dialogue written into it can actually do that job.
   */
  var BEATS = [
    {
      id: 'open',
      name: 'Ordinary',
      purpose: 'Show the hero in their world, and the small ache they live with.',
      action: [
        '{HERO} does the thing they do every day, and does it well enough that nobody would ask.',
        '{DETAIL} {HERO} moves through the {PLACE} without needing to look where they are going.',
        '{HERO} stops. Listens to {SOUND}. Goes back to it.',
        'A life, in the shorthand of a room: {DETAIL}',
        '{HERO} checks the time twice inside a minute, for no reason either of them could name.'
      ],
      shots: ['WIDE — the {PLACE}, and one small figure in it', 'CLOSE — hands, working', 'INSERT — {DETAIL_SHOT}']
    },
    {
      id: 'spark',
      name: 'Disruption',
      purpose: 'The thing that will not let the ordinary stay ordinary arrives.',
      action: [
        'The {OBJ} is there. It was not there a minute ago.',
        '{HERO} finds the {OBJ}, and the whole room reorganises itself around it.',
        '{SOUND_CUE} {HERO} turns. The {OBJ} is waiting to be picked up.',
        '{HERO} picks up the {OBJ} the way you pick up something that might still be warm.',
        'Everything about the {OBJ} says it was meant to be found. That is the part that lands wrong.'
      ],
      shots: ['PUSH IN — {HERO} clocking the {OBJ}', 'INSERT — the {OBJ}, held', 'REACTION — the decision arriving']
    },
    {
      id: 'push',
      name: 'The Push',
      purpose: 'The hero acts on it. First cost, first thrill.',
      action: [
        '{HERO} does the thing they have been telling themselves they would never do.',
        'A door that stays shut for most people opens for {HERO} {TONIGHT}.',
        '{HERO} moves fast now, and the {PLACE} feels smaller for it.',
        '{DETAIL} {HERO} does not stop to think about it, which is the point.',
        'The {OBJ} goes into a pocket. That is a choice, and both of them know it.'
      ],
      shots: ['TRACKING — behind {HERO}, moving', 'CLOSE — a hand, deciding', 'WIDE — the room after']
    },
    {
      id: 'turn',
      name: 'Complication',
      purpose: 'Someone else wants something else. The ground tilts.',
      action: [
        '{OTHER} is standing where nobody was standing.',
        '{OTHER} has been here longer than {HERO} has, and has been waiting to say so.',
        'Whatever {HERO} expected, it was not {OTHER}, and not this.',
        '{DETAIL} The conversation that follows will cost one of them something.',
        '{OTHER} looks at the {OBJ} first, and at {HERO} second. That order matters.'
      ],
      shots: ['OTS — {OTHER} over {HERO}’s shoulder', 'TWO-SHOT — the gap between them', 'CLOSE — {OTHER}, not blinking']
    },
    {
      id: 'crisis',
      name: 'Crisis',
      purpose: 'The worst version of the night. The cost is named out loud.',
      action: [
        'It goes wrong in the ordinary way things go wrong: quickly, and then all at once.',
        '{HERO} is out of moves and the {PLACE} knows it.',
        '{SOUND_CUE} Nobody comes. Nobody was ever going to come.',
        '{DETAIL} {HERO} sits down in the middle of it because standing has stopped working.',
        'The {OBJ} is on the floor between them and neither will be the one to pick it up.'
      ],
      shots: ['HANDHELD — close, unsteady', 'LOW ANGLE — the room over them', 'CUTAWAY — {DETAIL_SHOT}']
    },
    {
      id: 'choice',
      name: 'The Choice',
      purpose: 'The hero chooses, and the choice is the whole film.',
      action: [
        '{HERO} makes the call. It is the harder one.',
        'A long beat. Then {HERO} does the small brave thing instead of the big safe one.',
        '{HERO} puts the {OBJ} down, and it is the first honest thing in the film.',
        'No music. {HERO} chooses, and the room lets them.',
        '{HERO} looks at {OTHER} and finally tells the truth about {WANT}.'
      ],
      shots: ['CLOSE — {HERO}, the moment before', 'SLOW PUSH — into the choice', 'WIDE — the room, the decision made']
    },
    {
      id: 'after',
      name: 'After',
      purpose: 'The new ordinary — the first image again, changed by everything.',
      action: [
        'The same {PLACE}, the same light, a different person standing in it.',
        '{HERO} goes back to the thing they do every day. It is not the same thing now.',
        '{DETAIL} Nothing here has changed except the one thing that has.',
        '{HERO} lets the door stay open behind them.',
        'The {OBJ} sits where it will sit from now on, and {HERO} walks past it.'
      ],
      shots: ['MATCHING WIDE — the opening frame, later', 'CLOSE — {HERO}, lighter or heavier', 'HOLD — the empty frame, then out']
    }
  ];

  /* Beat orders by target length. Short films earn their length; a 2-minute
   * film gets the spine only. */
  var STRUCTURES = {
    micro:  { label: '3 scenes', beats: ['open', 'spark', 'choice'] },
    short:  { label: '5 scenes', beats: ['open', 'spark', 'push', 'turn', 'choice'] },
    festival: { label: '7 scenes', beats: ['open', 'spark', 'push', 'turn', 'crisis', 'choice', 'after'] }
  };

  /* Genre-neutral images. Mixed into every genre's own details so a single
   * script has enough of them never to repeat itself. */
  var NEUTRAL_DETAILS = [
    'Nobody says anything for a moment longer than is comfortable.',
    'A light somewhere decides to be a different light.',
    'The room does what rooms do once a decision has been made in them.',
    'Two people, one door, and all the air in between.',
    'The window has nothing to add.',
    'Outside, the ordinary world carries on being ordinary.',
    'A hand goes to a pocket and comes back empty.',
    'Neither of them moves toward the door.',
    'A breath, taken and not used.',
    'The light drops one notch, the way light does.',
    'Nothing in the frame is where it was ten minutes ago.',
    'Somebody should say something. Nobody does.'
  ];

  /* Same reasoning as the details: a genre's own four sounds get used up long
   * before a five-scene film is over, and then you hear the same hymn twice in
   * a minute. */
  var NEUTRAL_SOUNDS = [
    'a door two rooms away', 'a floorboard settling', 'wind finding a gap',
    'a clock with a loud second hand', 'water in a pipe', 'someone else\'s music',
    'a car that does not stop', 'the fridge cutting out'
  ];

  Object.keys(GENRES).forEach(function (key) {
    GENRES[key].details = GENRES[key].details.concat(NEUTRAL_DETAILS);
    GENRES[key].sounds = GENRES[key].sounds.concat(NEUTRAL_SOUNDS);
  });

  var LEX = {
    GENRES: GENRES,
    NEUTRAL_DETAILS: NEUTRAL_DETAILS,
    NEUTRAL_SOUNDS: NEUTRAL_SOUNDS,
    PLACES: PLACES,
    ROLES: ROLES,
    OBJECTS: OBJECTS,
    NAMES: NAMES,
    FOILS: FOILS,
    WANTS: WANTS,
    BEATS: BEATS,
    STRUCTURES: STRUCTURES
  };

  if (typeof module === 'object' && module.exports) module.exports = LEX;
  root.FILM_LEXICON = LEX;
})(typeof globalThis !== 'undefined' ? globalThis : this);
