# Companion

**A game character with a brain you train.** Walk around a little meadow with
Pip, your companion. Type (or say) what you want, and Pip does it and talks
back. Teach it new words and it remembers them. Tell it about yourself and it
remembers that too.

Open `index.html` in any browser. No build step, no server, nothing to
install. Works with a keyboard on a computer or a finger on a phone, in light
or dark mode.

A standalone app: it is not part of MAZ ARCADE, and it is not built on the Maz
engine.

## Things to say

| Say | Pip… |
|---|---|
| `follow me` · `stay` · `come here` | follows, waits, or comes to you |
| `get the apple` · `bring me a gem` | runs off, picks it up and brings it back to your bag |
| `go to the pond` · `go left` | walks there |
| `jump` · `dance` · `sit` · `explore` · `take a nap` | does just that |
| `jump and then dance` | does several things in order |
| `hi` · `how are you?` · `tell me a joke` · `help` | chats |

## Training it

| Say | What it learns |
|---|---|
| `when I say heel, follow me` | a new word for a command (or several: `when I say party, jump and dance`) |
| `when I say hi there, say Howdy partner!` | a new reply |
| `zoom` → *"I don't know what that means"* → `it means jump` | the word it just missed |
| `my name is Cody` · `your name is Rex` | names |
| `remember that I like apples` · `what do you remember?` | facts |
| `forget heel` · `forget everything` | to unlearn |

It recognises close variations of what you taught it. After teaching `heel`,
saying `heel boy` gets a guess. Say **`good job`** and it learns that phrase
for real. Say **`wrong`** and it won't make that guess again.

Everything it learns is saved in your browser, so it's still there next time.
Open **What Pip has learned** to see it all, forget single things, or save the
brain to a file and load it on another device.

## Smart chat with Claude (optional)

Without an API key, Pip's brain runs entirely in your browser. It handles
every command and lesson, plus simple small talk. Add a Claude API key under
**Smart chat with Claude** and anything that isn't a command goes to Claude.
Then Pip can hold a real conversation: answer questions, tell stories, and
make plans. It stays in character, knows everything you taught it and what's
in the meadow, and can still act ("let's go see the pond!" really walks there).

Commands and lessons always stay on the built-in brain, so they're instant and
free even with smart chat on. Each smart-chat message costs a small amount on
your Anthropic account. The key is stored only in this browser and sent only
to Anthropic. If Claude can't be reached or the key is wrong, Pip answers on
its own and says why.

It uses the official Anthropic SDK (loaded from jsDelivr only once you turn
smart chat on) with the `claude-opus-5-5` model at low effort for quick,
chatty replies. Claude answers in a fixed JSON shape (`say` + `actions`), and
the game drops any action it can't really perform. Change `MODEL` in
`js/mind.js` to use a different model.

## Use the brain in another game

The brain isn't tied to Pip or the meadow. `js/brain.js` and `js/mind.js`
know nothing about this page. A game gives them a **character** and they do
the understanding, the training, the memory and the smart chat.
[`examples/robot.html`](examples/robot.html) is a second, complete game
(Bolt, a robot on a space station) in one short file. Copy it to start a new
one.

A game needs three things:

**1. A character.** Its name and personality, and every action its body can
do, with the words a player might use for each:

```js
var BOLT = {
  name: 'Bolt',
  personality: 'brave, loyal, and very literal, like a friendly robot',
  setting: 'a space station adventure',
  actions: [
    { type: 'guard', words: ['guard', 'protect', 'cover me'],
      replies: ['Shields up!'], does: 'guard you', help: 'stay next to the player' },
    { type: 'attack', words: ['attack', 'zap', 'shoot'], target: 'enemies',
      replies: ['Zapping the {target}!'], does: 'zap the {target}', help: 'zap an enemy' }
  ],
  reactions: { praised: 'guard', greeted: 'guard' }   // optional
};
var brain = CompanionBrain.create(savedBrain, { character: BOLT });
```

| Action field | Meaning |
|---|---|
| `type` | What your game's code receives: `{ type: 'attack', target: 'drone' }`. |
| `words` | What the player might say. When several actions' words appear in a line, the longest one wins, so "stop following" is *stay*, not *follow*. |
| `target` | *(optional)* Which list in the world the target comes from, e.g. `'enemies'`. |
| `also` | *(optional)* Targets that always exist, such as `['left', 'right']`. |
| `replies` | What it says when it starts. `{target}` is filled in. |
| `does` | How it describes the action: "When you say pew, I'll *zap the drone*." |
| `ability` | *(optional)* How "help" lists it. |
| `help` | How smart chat (Claude) is told to use it. |

`reactions` picks which action, if any, goes with being `praised`,
`scolded`, `greeted`, `loved`, `named`, `renamed`, saying `bye`, or being
`wiped` ("forget everything"). Leave it out and it just talks. A typo in a
character (no words, two actions with the same type) throws an error straight
away instead of failing quietly.

**2. The world, every time the player speaks.** One list for each `target`
your actions use, plus optional `notes` for smart chat:

```js
var r = brain.hear(text, { enemies: ['alien', 'drone'], notes: 'It is night.' });
// r.say     what to show or speak
// r.actions what to do, e.g. [{ type: 'attack', target: 'drone' }]
// r.kind    who answered: command, taught, teach, memory, feedback, chat, missing, unknown
save(JSON.stringify(brain.toJSON()));   // keep what it learned
```

**3. Make the actions happen.** That part is your game: walk, animate, zap.

**Smart chat** works the same in every game. When `r.kind` is `chat`,
`unknown` or `missing` and the player has a key, pass the line to Claude:

```js
var claude = await CompanionMind.connect(apiKey);       // once
var conversation = new CompanionMind.History();         // once (don't call it "history": browsers use that name)
var reply = await CompanionMind.ask(claude, brain, conversation, text, world);
conversation.add(text, reply.say);                      // after every line, whoever answered
```

Claude is told the character, its memories and the world, and only actions
the character really has (with targets that exist) come back.

A brain saved by one game loads into another. The player's name, facts and
taught replies carry over, and lessons for actions the new character doesn't
have are left out.

**Where the files go:** copy `js/brain.js` and `js/mind.js` into the new
game and add two `<script>` tags. There's no build step and nothing to
install. In this repo, that is also how a standalone game in its own
top-level folder gets them. Loading them straight from `companion/js/`
would fail `scripts/check-exchange.mjs` in CI unless declared in
`shared/exchange.json`, and that file only takes MAZ ARCADE projects. Pages
inside `companion/` itself, like the example, load them directly.

## Files

| File | What it is |
|---|---|
| `js/brain.js` | The trainable brain, for any character: understands commands, learns phrases and replies, remembers facts, takes feedback, saves to JSON. Pip is its default character. No DOM, so it is tested in plain Node. |
| `js/mind.js` | The optional Claude mind: what Claude is told (character, memories, the world), how its reply is checked before the game acts on it, and the connection to Claude. Tested in plain Node with a stand-in for Claude. |
| `examples/robot.html` | A second game using the same brain: Bolt, a robot sidekick. The starting point for a new game. |
| `js/app.js` | The page: the meadow, walking, Pip's body and animations, chat, voice, saving, and the call to Claude. |
| `css/style.css` | The look. |
| `tests/companion-logic.test.js` | Logic tests: `node companion/tests/companion-logic.test.js` |

CI: `.github/workflows/companion.yml` runs the logic tests on every change here.
