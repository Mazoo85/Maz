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
      does: [
        '{HERO} puts everything back where it lives before leaving the {PLACE}',
        '{HERO} does the last of it standing up',
        '{HERO} counts something under their breath',
        '{HERO} takes the long way round the {PLACE}',
        '{HERO} finishes a job nobody will notice was done',
        '{HERO} waits for a sound that does not come'
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
      does: [
        '{HERO} sees the {OBJ} before deciding to look at it',
        '{HERO} picks the {OBJ} up and puts it straight back down',
        '{HERO} reads the {OBJ} the way you read bad news',
        '{HERO} turns the {OBJ} over once',
        '{HERO} checks whether anyone else in the {PLACE} has seen it',
        '{HERO} stops in the middle of the {PLACE} and does not go on'
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
      does: [
        '{HERO} goes through a door that was not left open for them',
        '{HERO} puts the {OBJ} somewhere it cannot be taken from',
        '{HERO} moves like somebody who has stopped asking',
        '{HERO} does it before there is time to think better of it',
        '{HERO} takes the {OBJ} and does not write it down',
        '{HERO} crosses the {PLACE} without stopping'
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
      does: [
        '{OTHER} says the name of the {OBJ} out loud',
        '{OTHER} stands between {HERO} and the door without appearing to',
        '{OTHER} asks the one thing {HERO} has no answer ready for',
        '{HERO} works out that {OTHER} has been in the {PLACE} the whole time',
        '{OTHER} looks at the {OBJ} first and at {HERO} second',
        '{HERO} gives an answer that is true and is not the truth'
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
      does: [
        '{HERO} runs out of places to stand',
        '{HERO} says the thing that cannot be taken back',
        '{HERO} stops moving, because moving has stopped helping',
        '{OTHER} watches and does not step in',
        '{HERO} tries the door that was open an hour ago',
        '{HERO} asks for help out loud, to nobody'
      ],
      shots: ['HANDHELD — close, unsteady', 'LOW ANGLE — the room over them', 'CUTAWAY — {DETAIL_SHOT}']
    },
    {
      id: 'midpoint',
      name: 'False Victory',
      purpose: 'It works. That is the problem — winning here is what costs them later.',
      action: [
        'For about a minute, {HERO} has what they wanted, and it is easier than it should have been.',
        '{HERO} gets it. {DETAIL} Nobody in the {PLACE} tells them what it cost.',
        'The {OBJ} does exactly what {HERO} hoped it would do. That is the first thing that frightens them.',
        '{HERO} lets themselves believe it for the length of one held breath.',
        '{OTHER} congratulates {HERO}, and means it, and is wrong.'
      ],
      does: [
        '{HERO} gets exactly what they came for',
        '{HERO} lets the relief show for about a second',
        '{HERO} does the thing that was supposed to be impossible',
        '{OTHER} tells {HERO} that it is finished now',
        '{HERO} puts the {OBJ} down like it is theirs to put down',
        '{HERO} allows themselves to think about afterwards'
      ],
      shots: ['WIDE — {HERO} standing in the middle of it, winning',
              'CLOSE — a smile arriving late', 'INSERT — the {OBJ}, doing what it was asked']
    },
    {
      id: 'unravel',
      name: 'The Unravelling',
      purpose: 'The cost starts arriving, in instalments, and none of it can be sent back.',
      action: [
        'The first thing goes wrong quietly enough that {HERO} can pretend it has not.',
        '{DETAIL} Then the second thing, which cannot be pretended about.',
        '{HERO} counts what is left and does not like the number.',
        '{OTHER} stops helping. Nothing is said about it; the {PLACE} simply has one fewer ally in it.',
        'Everything {HERO} did to get here is now a reason somebody has to be somewhere else.'
      ],
      does: [
        '{HERO} finds out what the easy part cost',
        '{HERO} goes back over it and finds the place it went wrong',
        '{OTHER} stops coming to the {PLACE}',
        '{HERO} starts explaining before anyone has asked',
        '{HERO} does the sum twice and gets the same answer',
        '{HERO} keeps the {OBJ} closer than is sensible'
      ],
      shots: ['TRACKING — {HERO}, going the wrong way fast',
              'OTS — {OTHER} not meeting their eye', 'CUTAWAY — {DETAIL_SHOT}']
    },
    {
      id: 'low',
      name: 'All Is Lost',
      purpose: 'The bottom. Whatever the hero was protecting is gone, and they know whose fault it is.',
      action: [
        '{HERO} is alone in the {PLACE} with the thing they cannot undo.',
        '{SOUND_CUE} The {OBJ} is worth nothing now and {HERO} is still holding it.',
        'Nobody is coming, and this time {HERO} does not spend any of the night hoping they will.',
        '{DETAIL} {HERO} says the true thing to an empty room, which is the only place it can be said yet.',
        '{HERO} understands, finally, that the person who did this was them.'
      ],
      does: [
        '{HERO} sits down in the {PLACE} and stays there',
        '{HERO} holds the {OBJ} and cannot think what it was for',
        '{HERO} stops waiting for anyone',
        '{HERO} says the true thing where nobody can hear it',
        '{HERO} counts what is left and stops counting',
        '{HERO} works out whose fault this is'
      ],
      shots: ['HOLD — {HERO}, still, for longer than is comfortable',
              'LOW ANGLE — the {PLACE} over them', 'CLOSE — the {OBJ}, no longer worth anything']
    },
    {
      id: 'reckon',
      name: 'The Reckoning',
      purpose: 'The two of them, and the truth, said plainly for the first time.',
      action: [
        '{OTHER} comes back, which {HERO} had stopped expecting.',
        'They say it straight to each other, and neither of them enjoys it.',
        '{HERO} stops performing and tells {OTHER} what they actually want: to {WANT}.',
        '{DETAIL} The {OBJ} sits between them while the two of them finally talk about it.',
        '{OTHER} says the one sentence {HERO} has spent the whole film not saying.'
      ],
      does: [
        '{OTHER} comes back into the {PLACE} anyway',
        '{HERO} tells {OTHER} the part they left out',
        '{HERO} stops performing and says it',
        '{OTHER} says the sentence {HERO} has spent all night not saying',
        '{HERO} puts the {OBJ} on the table between them',
        '{HERO} asks {OTHER} a question they actually want answered'
      ],
      shots: ['TWO-SHOT — no gap left between them',
              'CLOSE — {HERO}, hearing it', 'CLOSE — {OTHER}, having said it']
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
        '{HERO} looks at {OTHER} and finally says it out loud: they want to {WANT}.'
      ],
      does: [
        '{HERO} takes the harder of the two',
        '{HERO} lets go of the {OBJ} on purpose',
        '{HERO} says out loud that they want to {WANT}',
        '{HERO} chooses, and does not explain it',
        '{HERO} does the small brave thing',
        '{HERO} stops trying to have it both ways'
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
      does: [
        '{HERO} comes back into the {PLACE} as somebody else',
        '{HERO} does the ordinary thing, differently',
        '{HERO} leaves the door open behind them',
        '{HERO} walks past the {OBJ} without slowing',
        '{HERO} puts the {PLACE} back the way it was',
        '{HERO} takes the long way out'
      ],
      shots: ['MATCHING WIDE — the opening frame, later', 'CLOSE — {HERO}, lighter or heavier', 'HOLD — the empty frame, then out']
    }
  ];

  /* What can follow any of them.
   *
   * Every beat has a handful of things somebody DOES, and these are the observations that come after.
   * Multiplied together they are the reason a beat can say seventy-two different things instead of
   * five, without five hundred lines of hand-written description.
   *
   * Two rules make the multiplication safe, and both are checked on every combination rather than
   * spot-checked. The clauses stop without punctuating and these carry their own; and NONE of these
   * names anybody. "...and does not look at REN" reads as nonsense after a clause that was already
   * about REN, and the only way to be sure it never happens is for the observation never to know who
   * is in the scene. They are about the room, the hour, and the manner of the thing.
   */
  var CLOSERS = [
    ', and does not hurry about it.',
    ', the way people do when the deciding is already done.',
    '. It takes a second longer than it should.',
    ', and the {PLACE} is quieter afterwards.',
    '. Nobody watching would think twice about it.',
    ', carefully, which is not the same thing as slowly.',
    ', and does not look back at it.',
    '. Whatever it costs is not due yet.',
    ', once, and that is enough.',
    '. The {PLACE} takes no position on it.',
    ', and the {PLACE} closes over it again.',
    '. That is a decision, whatever else it is.',
    '. Nothing about it is an accident.',
    ', and lets the {PLACE} have the last word.',
    '. It is not the first time.',
    ', and does not say why.',
    '. Afterwards there is a little more space in the {PLACE}.',
    ', and does not stop to see what it looks like.'
  ];

  /* Beat orders by target length. Short films earn their length; a 2-minute
   * film gets the spine only.
   *
   * `acts` is how many scenes fall in each of the three acts, and it lives on
   * the STRUCTURE rather than on the beat. That is not an arbitrary choice: a
   * beat does not have an act, a POSITION does. Festival's third shape runs
   * '...crisis, after, choice' — the same three beats as the first shape in a
   * different order — and marking each beat with an act number would have that
   * film going into act three and back out again. Counting scenes cannot. */
  var STRUCTURES = {
    micro: {
      label: '3 scenes',
      acts: [1, 1, 1],
      spines: [
        ['open', 'spark', 'choice'],
        ['open', 'crisis', 'after']
      ]
    },
    short: {
      label: '5 scenes',
      acts: [1, 3, 1],
      spines: [
        ['open', 'spark', 'crisis', 'choice', 'after'],
        ['open', 'push', 'turn', 'crisis', 'choice'],
        ['open', 'spark', 'turn', 'crisis', 'after']
      ]
    },
    festival: {
      label: '7 scenes',
      acts: [2, 3, 2],
      spines: [
        ['open', 'spark', 'push', 'turn', 'crisis', 'choice', 'after'],
        ['open', 'spark', 'turn', 'push', 'crisis', 'choice', 'after'],
        ['open', 'push', 'spark', 'turn', 'crisis', 'after', 'choice']
      ]
    },
    /* Eleven scenes, and the only length with room for the middle of a film to
     * actually be a middle. Everything shorter goes setup, trouble, ending, and
     * the trouble is one scene long — which is why every short film here has
     * the same shape however much its beats are shuffled. At eleven there is a
     * false victory that turns, a cost arriving in instalments, a bottom, and a
     * reckoning before the choice: an act two that goes somewhere rather than
     * being the gap between the other two. */
    feature: {
      label: '11 scenes',
      acts: [3, 5, 3],
      spines: [
        ['open', 'spark', 'push', 'turn', 'midpoint', 'unravel', 'crisis', 'low',
         'reckon', 'choice', 'after'],
        ['open', 'push', 'spark', 'turn', 'midpoint', 'crisis', 'unravel', 'low',
         'reckon', 'choice', 'after'],
        ['open', 'spark', 'turn', 'push', 'midpoint', 'unravel', 'low', 'crisis',
         'reckon', 'choice', 'after']
      ]
    }
  };

  /* Which act a scene at this index belongs to: 1, 2 or 3. */
  function actOf(structure, index) {
    var acts = (structure && structure.acts) || [1, 1, 1];
    var seen = 0;
    for (var a = 0; a < acts.length; a++) {
      seen += acts[a];
      if (index < seen) return a + 1;
    }
    return acts.length;
  }

  /* `beats` was the single shape each length used to have. Keep it pointing at
   * the first shape so anything still reading it sees a valid story. */
  Object.keys(STRUCTURES).forEach(function (key) {
    STRUCTURES[key].beats = STRUCTURES[key].spines[0];
  });

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

  /* Some images only work with a ceiling over them.
   *
   * A film used to have two places and they were nearly always interiors. It
   * now has three to five, and a third of its scenes are exteriors — so "Rain
   * finds the same crack in the sill it always finds" started landing in a
   * parking lot, and "the whole room reorganises itself" in an alley. Measured
   * over 400 default-length films: about one exterior scene in two carried a
   * line that needed a room around it.
   *
   * Each of those lines gets an outdoor twin here and the writer swaps it when
   * the scene is EXT. Keys are the raw strings, templates included, so the swap
   * happens before {HERO}/{OBJ} are substituted. Lines that only mention a room
   * as a simile — "The woods go quiet the way a room goes quiet" — are left
   * alone: they read correctly under an open sky, and rewriting them to satisfy
   * a keyword sweep would cost the image and buy nothing. */
  var OUTDOORS = {
    // Genre-neutral, so these two were the most frequent offenders by far.
    'The room does what rooms do once a decision has been made in them.':
      'The weather does what weather does once a decision has been made under it.',
    'The window has nothing to add.':
      'The sky has nothing to add.',
    // drama
    'Dust turns over in a bar of window light.':
      'Dust turns over in a bar of low sun.',
    'The radiator knocks twice and gives up.':
      'A gate knocks twice in the wind and gives up.',
    'Rain finds the same crack in the sill it always finds.':
      'Rain finds the same gutter it always finds.',
    'A photograph lies face down on the counter.':
      'A photograph lies face down on the step.',
    // thriller
    'Headlights sweep the ceiling and move on.':
      'Headlights sweep the wet road and move on.',
    'The room holds its breath.':
      'The street holds its breath.',
    'A phone screen lights the floor and dies.':
      'A phone screen lights a pair of hands and dies.',
    // horror
    'The dark past the doorway does not behave like dark.':
      'The dark past the treeline does not behave like dark.',
    'Something in the walls stops moving the moment it is heard.':
      'Something in the hedge stops moving the moment it is heard.',
    // comedy
    'Everyone in the room decides to look at something else.':
      'Everyone within earshot decides to look at something else.',
    // romance
    'The last of the light goes gold on the wall.':
      'The last of the light goes gold on the rooftops.',
    // mystery
    'The room has been tidied by someone in a hurry.':
      'The ground has been swept by someone in a hurry.',
    // fantasy
    'The wind moves through the room without moving the curtains.':
      'The wind moves through the yard without moving the washing.',
    // Beat action templates. Swapped before substitution, hence the braces.
    'A life, in the shorthand of a room: {DETAIL}':
      'A life, in the shorthand of what someone left outside: {DETAIL}',
    '{HERO} finds the {OBJ}, and the whole room reorganises itself around it.':
      '{HERO} finds the {OBJ}, and everything around it rearranges itself to match.',
    'The {OBJ} is on the floor between them and neither will be the one to pick it up.':
      'The {OBJ} is on the ground between them and neither will be the one to pick it up.',
    'No music. {HERO} chooses, and the room lets them.':
      'No music. {HERO} chooses, and nothing stops them.',
    // Sound cues. A kettle or a fridge is not audible from a parking lot.
    'a kettle building': 'an engine that will not start',
    'a door two rooms away': 'a door somewhere behind them',
    'a floorboard settling': 'gravel settling underfoot',
    'water in a pipe': 'water in a drain',
    'the fridge cutting out': 'a streetlight buzzing and stopping',
    'wings too large for the room': 'wings too large for the sky they are in',
    // Shot descriptions carry the same assumption.
    'WIDE — the room after': 'WIDE — the place after',
    'LOW ANGLE — the room over them': 'LOW ANGLE — the sky over them',
    'WIDE — the room, the decision made': 'WIDE — the open ground, the decision made'
  };

  /* Details, built from parts, for the same reason the beats are.
   *
   * Eighteen details per genre sounded like plenty until it was counted against a film that uses
   * them: an eleven-scene script writes fifty-four lines of description, so the pool is drained and
   * recycled twice over and two takes of the same idea come out sharing whole sentences. Measured
   * before this existed, two takes shared 40% of their description word for word — and when the
   * beats alone were made combinatorial it barely moved, because the repeated lines were nearly all
   * DETAILS rather than actions. That is the sort of thing only counting finds.
   *
   * One rule these have that the hand-written details do not: none of them may name anything that
   * exists only indoors. The indoor-to-outdoor rewrite (OUTDOORS, below) is a lookup table keyed on
   * the whole line, and an assembled line is not in it — so a detail about a window would follow a
   * character into the woods. These are about the light, the hour, the cold and the quiet, which are
   * everywhere. There is a check that says so.
   */
  var DETAIL_THINGS = [
    'The light',
    'The air',
    'The hour',
    'The quiet',
    'The space between them',
    'Everything not being looked at',
    'Whatever was happening before this',
    'The cold',
    'A sound that stopped a moment ago',
    'The dark at the edges',
    'What is left of the night',
    'Everything within reach'
  ];

  var DETAIL_DOES = [
    'has nothing to add.',
    'stays exactly as it is.',
    'takes no position on any of it.',
    'goes on without them.',
    'is doing none of the work here.',
    'gets no say in it.',
    'waits, and goes on waiting.',
    'means nothing, and is there anyway.',
    'changes by one degree and settles.',
    'is exactly what it was ten minutes ago.',
    'does not help.',
    'carries on being ordinary.'
  ];

  /* The outdoor twin of a line, or the line itself when it needs no swap. */
  function outdoors(text) {
    var swap = OUTDOORS[text];
    return swap === undefined ? text : swap;
  }

  Object.keys(GENRES).forEach(function (key) {
    // The hand-written details only. The assembled ones are NOT poured in here, and the reason is the
    // same one the beats taught: a pool over finished sentences cannot see that three of them share a
    // predicate, and a film that says "The light waits, and goes on waiting", "The hour waits, and
    // goes on waiting" and "The space between them waits, and goes on waiting" is worse than one
    // short of details. The writer draws the two halves from their own pools instead.
    GENRES[key].details = GENRES[key].details.concat(NEUTRAL_DETAILS);
    GENRES[key].sounds = GENRES[key].sounds.concat(NEUTRAL_SOUNDS);
  });

  var LEX = {
    GENRES: GENRES,
    NEUTRAL_DETAILS: NEUTRAL_DETAILS,
    DETAIL_THINGS: DETAIL_THINGS,
    DETAIL_DOES: DETAIL_DOES,
    OUTDOORS: OUTDOORS,
    outdoors: outdoors,
    NEUTRAL_SOUNDS: NEUTRAL_SOUNDS,
    PLACES: PLACES,
    ROLES: ROLES,
    OBJECTS: OBJECTS,
    NAMES: NAMES,
    FOILS: FOILS,
    WANTS: WANTS,
    BEATS: BEATS,
    CLOSERS: CLOSERS,
    actOf: actOf,
    STRUCTURES: STRUCTURES
  };

  if (typeof module === 'object' && module.exports) module.exports = LEX;
  root.FILM_LEXICON = LEX;
})(typeof globalThis !== 'undefined' ? globalThis : this);
