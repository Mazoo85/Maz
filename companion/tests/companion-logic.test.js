/*
 * COMPANION — logic tests.
 *
 *   node companion/tests/companion-logic.test.js
 *
 * No dependencies, no browser and no network: the brain and the mind are
 * plain modules. What these prove is the promise the game makes: everyday
 * commands work out of the box, anything you teach it sticks (and survives a
 * save and reload), "good job" and "wrong" really train it, whatever Claude
 * says, the game only ever gets actions it can actually perform, and all of
 * it works the same for another game's character, not just Pip.
 */
'use strict';

const path = require('path');
const B = require(path.join(__dirname, '..', 'js', 'brain.js'));
const M = require(path.join(__dirname, '..', 'js', 'mind.js'));

let passed = 0;
const failures = [];

const pending = [];

function test(name, fn) {
  try {
    const r = fn();
    if (r && r.then) {
      pending.push(r.then(() => { passed++; }, e => { failures.push(name + '\n    ' + e.message); }));
      return;
    }
    passed++;
  } catch (e) {
    failures.push(name + '\n    ' + e.message);
  }
}

function eq(actual, expected, what) {
  const a = JSON.stringify(actual), b = JSON.stringify(expected);
  if (a !== b) throw new Error((what || 'value') + ': expected ' + b + ', got ' + a);
}

function ok(cond, what) {
  if (!cond) throw new Error(what || 'expected true');
}

const WORLD = { items: ['apple', 'apple', 'gem', 'mushroom'], places: ['tree', 'pond', 'campfire'] };
const fresh = () => B.create(null, { rand: () => 0 });

// ---- built-in commands ------------------------------------------------------

test('everyday commands work out of the box', () => {
  const cases = {
    'follow me': [{ type: 'follow' }],
    'stay': [{ type: 'stay' }],
    'wait here': [{ type: 'stay' }],
    'stop following': [{ type: 'stay' }],
    'stay close': [{ type: 'follow' }],
    'come here': [{ type: 'come' }],
    'come with me': [{ type: 'follow' }],
    'jump': [{ type: 'jump' }],
    'can you please dance for me': [{ type: 'dance' }],
    'sit down': [{ type: 'sit' }],
    'go explore': [{ type: 'wander' }],
    'take a nap': [{ type: 'sleep' }]
  };
  for (const line in cases) eq(fresh().hear(line, WORLD).actions, cases[line], line);
});

test('fetch and go-to find their target, plurals included', () => {
  eq(fresh().hear('go get the apple', WORLD).actions, [{ type: 'fetch', target: 'apple' }]);
  eq(fresh().hear('bring me some mushrooms', WORLD).actions, [{ type: 'fetch', target: 'mushroom' }]);
  eq(fresh().hear('go to the pond', WORLD).actions, [{ type: 'goto', target: 'pond' }]);
  eq(fresh().hear('walk left', WORLD).actions, [{ type: 'goto', target: 'left' }]);
});

test('"get over here" is a come, not a fetch', () => {
  eq(fresh().hear('get over here', WORLD).actions, [{ type: 'come' }]);
});

test('several commands in one line run in order', () => {
  eq(fresh().hear('jump and then dance, then follow me', WORLD).actions,
    [{ type: 'jump' }, { type: 'dance' }, { type: 'follow' }]);
});

test('asking for something that is not there says so', () => {
  const r = fresh().hear('fetch the banana', WORLD);
  eq(r.actions, []);
  eq(r.kind, 'missing');
  ok(/apple/.test(r.say), 'lists what it can see');
});

test('the companion\'s own name is ignored in commands', () => {
  eq(fresh().hear('Pip, sit!', WORLD).actions, [{ type: 'sit' }]);
});

// ---- teaching ---------------------------------------------------------------

test('"when I say X, Y" teaches a new command phrase', () => {
  const b = fresh();
  const r = b.hear('When I say heel, follow me', WORLD);
  eq(r.kind, 'teach');
  eq(b.hear('heel', WORLD).actions, [{ type: 'follow' }]);
});

test('a taught phrase can be several actions', () => {
  const b = fresh();
  b.hear('when I say party time, jump and dance', WORLD);
  eq(b.hear('party time', WORLD).actions, [{ type: 'jump' }, { type: 'dance' }]);
});

test('"X means Y" teaches too, and taught phrases beat the built-ins', () => {
  const b = fresh();
  b.hear('jump means dance', WORLD);
  eq(b.hear('jump', WORLD).actions, [{ type: 'dance' }]);
});

test('teaching a reply keeps the capitals it was typed with', () => {
  const b = fresh();
  b.hear('when I say hi there, say "Howdy Partner!"', WORLD);
  const r = b.hear('hi there', WORLD);
  eq(r.say, 'Howdy Partner!');
  eq(r.actions, []);
});

test('after "I don\'t understand", "it means ..." teaches the missed phrase', () => {
  const b = fresh();
  const lost = b.hear('zoom zoom', WORLD);
  eq(lost.understood, false);
  b.hear('it means jump', WORLD);
  eq(b.hear('zoom zoom', WORLD).actions, [{ type: 'jump' }]);
});

test('after "I don\'t understand", "say ..." teaches a reply', () => {
  const b = fresh();
  b.hear('blorp', WORLD);
  b.hear('say blorp to you too', WORLD);
  eq(b.hear('blorp', WORLD).say, 'blorp to you too');
});

test('teaching something it cannot do is refused, and the lesson stays open', () => {
  const b = fresh();
  b.hear('zoom', WORLD);
  const r = b.hear('it means fly to the moon', WORLD);
  eq(r.mood, 'confused');
  b.hear('it means jump', WORLD);
  eq(b.hear('zoom', WORLD).actions, [{ type: 'jump' }]);
});

test('a taught fetch needs the item to be there when it is used', () => {
  const b = fresh();
  b.hear('when I say snack, get the apple', WORLD);
  eq(b.hear('snack', WORLD).actions, [{ type: 'fetch', target: 'apple' }]);
  eq(b.hear('snack', { items: [], places: [] }).actions, []);
});

// ---- training by feedback ----------------------------------------------------

test('a close phrase is guessed, and "good job" makes it official', () => {
  const b = fresh();
  b.hear('when I say heel, follow me', WORLD);
  const g = b.hear('heel boy', WORLD);
  eq(g.guessed, true);
  eq(g.actions, [{ type: 'follow' }]);
  b.hear('good job', WORLD);
  const again = b.hear('heel boy', WORLD);
  eq(again.guessed, false);
  eq(again.actions, [{ type: 'follow' }]);
});

test('"wrong" after a guess stops that guess but keeps the lesson', () => {
  const b = fresh();
  b.hear('when I say heel, follow me', WORLD);
  eq(b.hear('heeel', WORLD).guessed, true);
  b.hear('wrong', WORLD);
  eq(b.hear('heeel', WORLD).understood, false);
  eq(b.hear('heel', WORLD).actions, [{ type: 'follow' }]);
});

test('similarity: same words score 1, unrelated words 0, typos count', () => {
  eq(B.similarity('heel', 'heel'), 1);
  eq(B.similarity('apple pie', 'blue sky'), 0);
  ok(B.similarity('folow me', 'follow me') >= 0.99, 'typo');
});

// ---- memory -----------------------------------------------------------------

test('it remembers names and facts', () => {
  const b = fresh();
  b.hear('My name is Cody', WORLD);
  b.hear('your name is Rex', WORLD);
  b.hear('remember that I like apples', WORLD);
  eq(b.playerName, 'Cody');
  eq(b.name, 'Rex');
  eq(b.facts, ['I like apples']);
  ok(/Cody/.test(b.hear('what is my name', WORLD).say + b.hear('who am i', WORLD).say));
  ok(/I like apples/.test(b.hear('what do you remember', WORLD).say));
  eq(b.hear('Rex, jump', WORLD).actions, [{ type: 'jump' }]);
});

test('"forget X" removes one lesson; "forget everything" wipes it', () => {
  const b = fresh();
  b.hear('when I say heel, follow me', WORLD);
  b.hear('my name is Cody', WORLD);
  b.hear('forget heel', WORLD);
  eq(b.hear('heel', WORLD).understood, false);
  b.hear('forget everything', WORLD);
  eq(b.playerName, '');
});

// ---- saving -----------------------------------------------------------------

test('everything it learned survives a save and reload', () => {
  const b = fresh();
  b.hear('my name is Cody', WORLD);
  b.hear('when I say heel, follow me', WORLD);
  b.hear('when I say hi there, say Howdy', WORLD);
  b.hear('remember that I like apples', WORLD);
  b.hear('heeel', WORLD);
  b.hear('wrong', WORLD);
  const back = B.create(JSON.stringify(b.toJSON()));
  eq(back.toJSON(), b.toJSON());
  eq(back.hear('heel', WORLD).actions, [{ type: 'follow' }]);
  eq(back.hear('heeel', WORLD).understood, false);
});

test('a bad or foreign save file is refused, not half-loaded', () => {
  const b = fresh();
  eq(b.load('not json'), false);
  eq(b.load({ version: 99 }), false);
  b.load({ version: 1, phrases: { evil: { actions: [{ type: 'explode' }] }, empty: { actions: [] }, ok: { actions: [{ type: 'jump' }] } } });
  eq(Object.keys(b.phrases), ['ok']);
});

// ---- the Claude mind --------------------------------------------------------

function reply(obj, extra) {
  return Object.assign({ stop_reason: 'end_turn', content: [{ type: 'text', text: JSON.stringify(obj) }] }, extra);
}

test('Claude\'s actions are kept only if the game can do them', () => {
  const r = M.read(reply({
    say: 'Sure!',
    actions: [
      { type: 'fetch', target: 'apple' },
      { type: 'fetch', target: 'unicorn' },
      { type: 'goto', target: 'pond' },
      { type: 'goto', target: 'moon' },
      { type: 'fly', target: '' },
      { type: 'dance', target: 'ignored' }
    ]
  }), fresh(), WORLD);
  eq(r.say, 'Sure!');
  eq(r.actions, [{ type: 'fetch', target: 'apple' }, { type: 'goto', target: 'pond' }, { type: 'dance' }]);
});

test('a refusal or broken reply still leaves the companion something to say', () => {
  eq(M.read(reply({ say: 'x', actions: [] }, { stop_reason: 'refusal' }), fresh(), WORLD).ok, false);
  const broken = M.read({ stop_reason: 'end_turn', content: [{ type: 'text', text: 'Hello there' }] }, fresh(), WORLD);
  eq(broken.say, 'Hello there');
  eq(broken.actions, []);
});

test('the request carries the brain\'s memory and what is in the world', () => {
  const b = fresh();
  b.hear('my name is Cody', WORLD);
  b.hear('when I say heel, follow me', WORLD);
  const h = new M.History();
  h.add('hello', 'Hi Cody!');
  const req = M.request(b, h, 'tell me a story', WORLD, { mode: 'follow' });
  eq(req.model, 'claude-opus-5-5');
  eq(req.messages.length, 3);
  eq(req.messages.map(m => m.role), ['user', 'assistant', 'user']);
  const last = req.messages[2].content;
  ok(/Cody/.test(last) && /"heel" = follow you/.test(last) && /Items around you: apple \(2\)/.test(last) && /tell me a story/.test(last), last);
  eq(req.output_config.format.type, 'json_schema');
  eq(req.fallbacks, 'default');
  ok(!/Cody/.test(req.system), 'the standing instructions do not change from turn to turn');
});

test('history stays short and always starts with the player', () => {
  const h = new M.History();
  for (let i = 0; i < 40; i++) h.add('line ' + i, 'reply ' + i);
  ok(h.turns.length <= 30, 'trimmed');
  eq(h.turns[0].role, 'user');
});

test('the reply schema lists exactly the actions the character has', () => {
  eq(M.schema(fresh()).properties.actions.items.properties.type.enum,
    ['follow', 'stay', 'come', 'goto', 'fetch', 'jump', 'dance', 'sit', 'wander', 'sleep']);
});

test('ask() sends the request and reads the reply', async () => {
  let sent = null;
  const claude = { beta: { messages: { create: async body => { sent = body; return reply({ say: 'Hi!', actions: [{ type: 'jump', target: '' }] }); } } } };
  const r = await M.ask(claude, fresh(), new M.History(), 'hello', WORLD, {});
  eq(r, { say: 'Hi!', actions: [{ type: 'jump' }], ok: true });
  eq(sent.model, M.MODEL);
});

// ---- another game's character -------------------------------------------------

// A robot guard for a sci-fi game: different name, actions, words and targets.
const BOLT = {
  name: 'Bolt',
  personality: 'brave, loyal, and very literal, like a friendly robot',
  setting: 'a space station adventure',
  actions: [
    { type: 'guard', words: ['guard', 'protect', 'watch my back', 'cover me'], replies: ['Shields up!'], does: 'guard you', help: 'stay next to the player and watch for danger' },
    { type: 'patrol', words: ['patrol', 'keep watch', 'scout'], replies: ['Patrolling.'], does: 'patrol', help: 'walk around the area' },
    { type: 'attack', words: ['attack', 'zap', 'shoot', 'blast', 'take out'], target: 'enemies', replies: ['Zapping the {target}!'], does: 'zap the {target}', help: 'zap an enemy' },
    { type: 'open', words: ['open', 'unlock', 'hack'], target: 'doors', replies: ['Opening the {target}.'], does: 'open the {target}', help: 'open a door' },
    { type: 'beep', words: ['beep', 'make a noise', 'say something robot'], replies: ['Beep boop!'], does: 'beep', help: 'beep happily' }
  ],
  reactions: { praised: 'beep', greeted: 'beep' }
};
const STATION = { enemies: ['alien', 'drone', 'drone'], doors: ['airlock', 'red door'] };
const bolt = () => B.create(null, { character: BOLT, rand: () => 0 });

test('another character understands its own commands and targets', () => {
  eq(bolt().hear('Bolt, cover me!', STATION).actions, [{ type: 'guard' }]);
  eq(bolt().hear('zap the drones', STATION).actions, [{ type: 'attack', target: 'drone' }]);
  eq(bolt().hear('please open the red door', STATION).actions, [{ type: 'open', target: 'red door' }]);
  eq(bolt().hear('open the red door', STATION).say, 'Opening the red door.');
  eq(bolt().hear('scout and then beep', STATION).actions, [{ type: 'patrol' }, { type: 'beep' }]);
});

test('another character knows nothing of Pip\'s actions', () => {
  eq(bolt().hear('follow me', STATION).understood, false);
  eq(bolt().hear('hi', STATION).actions, [{ type: 'beep' }]);
  eq(bolt().hear('thanks', STATION).actions, []);
  ok(/zap the enemies|zap things/.test(bolt().hear('help', STATION).say) || /guard you/.test(bolt().hear('help', STATION).say));
});

test('another character can be trained the same way', () => {
  const b = bolt();
  ok(/it means guard/.test(b.hear('lasers', STATION).say), 'suggests one of its own actions');
  b.hear('it means attack the alien', STATION);
  eq(b.hear('lasers', STATION).actions, [{ type: 'attack', target: 'alien' }]);
  b.hear('when I say pew pew, zap the drone and then beep', STATION);
  eq(b.hear('pew pew', STATION).actions, [{ type: 'attack', target: 'drone' }, { type: 'beep' }]);
  eq(b.hear('zap the dragon', STATION).kind, 'missing');
});

test('a brain saved by one game loads into another, keeping only what fits', () => {
  const pip = fresh();
  pip.hear('my name is Cody', WORLD);
  pip.hear('when I say heel, follow me', WORLD);
  pip.hear('when I say hi there, say Howdy', WORLD);
  const b = B.create(pip.toJSON(), { character: BOLT });
  eq(b.playerName, 'Cody');
  eq(Object.keys(b.phrases), ['hi there']);
});

test('Claude is told about the other character, and kept to its actions', () => {
  const b = bolt();
  const req = M.request(b, new M.History(), 'what do you see?', STATION, {});
  ok(/Bolt/.test(req.system) && /space station/.test(req.system) && /attack \(target = one of the enemies\)/.test(req.system), req.system);
  ok(/Enemies around you: alien, drone \(2\)/.test(req.messages[0].content) && /Doors around you/.test(req.messages[0].content), req.messages[0].content);
  eq(M.schema(b).properties.actions.items.properties.type.enum, ['guard', 'patrol', 'attack', 'open', 'beep']);
  const r = M.read(reply({ say: 'Zap!', actions: [{ type: 'attack', target: 'drone' }, { type: 'attack', target: 'dragon' }, { type: 'follow', target: '' }] }), b, STATION);
  eq(r.actions, [{ type: 'attack', target: 'drone' }]);
});

test('a broken character is caught at once, with a clear message', () => {
  const bad = (spec, msg) => {
    try { B.create(null, { character: spec }); } catch (e) { ok(e.message.indexOf(msg) !== -1, e.message); return; }
    throw new Error('expected an error about ' + msg);
  };
  bad({ actions: [] }, 'list of actions');
  bad({ actions: [{ type: 'jump' }] }, 'needs some words');
  bad({ actions: [{ type: 'jump', words: ['jump'] }, { type: 'jump', words: ['hop'] }] }, 'both called "jump"');
});

// ---- report -----------------------------------------------------------------

Promise.all(pending).then(() => {
  if (failures.length) {
    console.error(failures.length + ' failed, ' + passed + ' passed:\n  ' + failures.join('\n  '));
    process.exit(1);
  }
  console.log('companion logic: ' + passed + ' passed');
});
