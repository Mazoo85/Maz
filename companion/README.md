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

## Files

| File | What it is |
|---|---|
| `js/brain.js` | The trainable brain: understands commands, learns phrases and replies, remembers facts, takes feedback, saves to JSON. No DOM, so it is tested in plain Node. |
| `js/mind.js` | The optional Claude mind: what Claude is told (personality, memories, the world) and how its reply is checked before the game acts on it. No DOM and no network, so it is tested in plain Node. |
| `js/app.js` | The page: the meadow, walking, Pip's body and animations, chat, voice, saving, and the call to Claude. |
| `css/style.css` | The look. |
| `tests/companion-logic.test.js` | Logic tests: `node companion/tests/companion-logic.test.js` |

CI: `.github/workflows/companion.yml` runs the logic tests on every change here.
