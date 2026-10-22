/*
 * COMPANION — logic tests.
 *
 *   node companion/tests/companion-logic.test.js
 *
 * No dependencies, no browser and no network: the brain and the mind are
 * plain modules. What these prove is the promise the game makes: everyday
 * commands work out of the box, anything you teach it sticks (and survives a
 * save and reload), "good job" and "wrong" really train it, and whatever Claude
 * says, the game only ever gets actions it can actually perform.
 */
'use strict';

const path = require('path');
const B = require(path.join(__dirname, '..', 'js', 'brain.js'));
const M = require(path.join(__dirname, '..', 'js', 'mind.js'));

let passed = 0;
const failures = [];

function test(name, fn) {
  try {
    fn();
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
  b.load({ version: 1, phrases: { evil: { actions: [{ type: 'explode' }] }, ok: { actions: [{ type: 'jump' }] } } });
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
  }), WORLD);
  eq(r.say, 'Sure!');
  eq(r.actions, [{ type: 'fetch', target: 'apple' }, { type: 'goto', target: 'pond' }, { type: 'dance' }]);
});

test('a refusal or broken reply still leaves the companion something to say', () => {
  eq(M.read(reply({ say: 'x', actions: [] }, { stop_reason: 'refusal' }), WORLD).ok, false);
  const broken = M.read({ stop_reason: 'end_turn', content: [{ type: 'text', text: 'Hello there' }] }, WORLD);
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
  ok(/Cody/.test(last) && /"heel" = follow you/.test(last) && /apple \(2\)/.test(last) && /tell me a story/.test(last), last);
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

test('the reply schema lists exactly the actions the brain knows', () => {
  eq(M.REPLY_SCHEMA.properties.actions.items.properties.type.enum, B.ACTION_TYPES);
});

// ---- report -----------------------------------------------------------------

if (failures.length) {
  console.error(failures.length + ' failed, ' + passed + ' passed:\n  ' + failures.join('\n  '));
  process.exit(1);
}
console.log('companion logic: ' + passed + ' passed');
