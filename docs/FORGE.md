# The Forge

A loop that wakes up every night, reads the state of this repo, does **one**
useful thing, and writes down what happened.

Full design: [`docs/superpowers/specs/2026-09-10-forge-design.md`](superpowers/specs/2026-09-10-forge-design.md)

## How to stop it

Read this first, because it matters more than anything else here.

- **Stop tonight's run:** disable the scheduled Routine (see *Scheduling* below).
  Nothing else is needed — the Forge holds no state between runs.
- **Undo a night:** every live run works on its own `forge/YYYY-MM-DD-<slug>`
  branch, cut fresh from the base branch (`base_branch` in `forge.json` —
  `main` by default; see *The leash* below), and opens a *draft* pull
  request. Close the PR and delete the branch — nothing else was touched,
  and no other night's branch carries those commits. Nothing the Forge does
  is ever merged without you — it has no ability to merge anything itself.
- **Stop it touching a directory:** add the path to `no_touch` in `forge.json`.
- **Stop it entirely and permanently:** delete `forge.json`. Without a config
  file the defaults still apply (they are the same safe zones and limits), so
  to actually stop it you must also disable the Routine.

The Forge never pushes to its base branch — `main` by default, whatever
`base_branch` says otherwise — never merges anything, and can never modify
`forge/` or `.github/workflows/` — those two paths are hard-coded into the
program itself, welded on underneath whatever `forge.json` says, so deleting
or editing the config cannot remove them.

## What it will actually do tonight

As configured today, a run has **nothing it can act on**. A real dry run
against this repo considered 97 candidates and skipped every single one:

- 93 came from the roadmap and 3 from codebase memory — neither kind carries
  a file path, and the Forge refuses to touch anything it cannot name a path
  for. That's by design, not a bug: an idea with no known file is not
  something the leash can safety-check.
- 1 came from a red CI check, and it pointed at `engine/` — the C++ engine
  core, which is deliberately human-only and outside every safe zone.
- The `TODO`/`FIXME`/`HACK` scanner found zero matches in tracked source.

So a night keeps recording "nothing worth doing" (`no_task`, or
`considered: 97 / outside_zone: 97` if you run `forge sense` and `forge
decide` yourself) until you give it something it can place. That is the
conservative design working as intended, not something to fix in a hurry.

## How to give it a job

Write one line in **Phase 14** of [`docs/ROADMAP.md`](ROADMAP.md) and put the
file it should change in backticks:

    - [ ] Add a volume slider to `music/js/player.js`

That is the whole interface. The next run scores it like any other candidate,
places it by its path, and — on a live run — hands it to Crew and opens a
draft pull request you can merge or close.

Four things make this safe to do casually:

- **The file has to exist**, spelled the way the repo spells it. A name the
  Forge cannot find is read as ordinary prose and the item is skipped. Spell
  it right: a typo that lands on nothing costs you a quiet night, but a typo
  that lands on a *different* real file in a safe zone is work the Forge will
  happily do. (Matching is case-sensitive on Linux, where it runs; a macOS or
  Windows checkout is laxer, the same caveat `zones.py` documents.)
- **A symlink is judged by where it points**, not by the name you wrote, so a
  safe-zone name cannot smuggle in a file from outside the repo.
- **Naming a file is not permission to change it.** `safe_zones` still
  decides, and `forge/` and `.github/workflows/` are refused outright. An
  item pointing at `engine/` is skipped every night, however you word it.
- **Examples inside a fenced code block are ignored**, so the roadmap can
  document its own conventions without commissioning them. Fence handling
  follows CommonMark rather than counting ``` lines, so a nested block or a
  line-opening inline span cannot reach inside an example — and the examples
  name files that do not exist, so they are inert regardless.
- **`docs/ROADMAP.md` and this file are in `no_touch`.** The intake and the
  manual are the two files the Forge must not be able to rewrite.
- **Everything still lands as a draft PR.** One job a night, nothing merged
  for you.

Verified through SENSE and DECIDE against this repo: a Phase 14 item naming
`music/js/theory.js` is picked, placed in zone `music/`, and scored 10.5
against a floor of 4.0. Removing the line puts the night straight back to
`considered: 97 / outside_zone: 97`. The Crew hand-off and the draft PR that
follow are covered by the test suite, not by that live run.

The older, blunter lever still exists — widening `safe_zones` in `forge.json`
so whole directories become reachable (see *Rollout* below). Prefer naming
files in the roadmap: it is reversible by deleting one line, and it does not
widen what the Forge may touch — only what it may be pointed at inside the
zones it already had.

## The cycle

```
SENSE -> DECIDE -> DO -> VERIFY -> LEARN -> (tomorrow)
```

| Step | What it does |
|------|--------------|
| **SENSE** | Reads unchecked roadmap boxes, red CI runs, `TODO` markers, and codebase-memory into `forge/state/pulse.json` |
| **DECIDE** | Scores every candidate, applies three strikes / variety / floor, writes the pick and the reasoning to `forge/state/tonight.json` |
| **DO** | Refuses to start if the working tree has uncommitted changes anywhere but the Forge's own bookkeeping (see *Outcomes* below); otherwise creates a branch and hands the single task to Maz Crew |
| **VERIFY** | Runs the union of the checks for every zone the *changed files* actually touch (not just the zone DECIDE chose — see the exchange-checks section below); green pushes the branch and opens a draft PR, red abandons the branch — either way, the tree is back on the base branch when the run ends |
| **LEARN** | Appends one line to `forge/ledger/YYYY-MM.jsonl` |

## GITHUB_TOKEN

Set this (or `GH_TOKEN`) as an environment variable wherever the Forge runs,
before you rely on anything below. Without it, three things quietly stop
working — none of them loudly:

- **SENSE** can't see which GitHub Actions workflow is currently red on
  `main`, `master`, or the configured base branch — that signal source is
  just silently empty every night, as if everything were green.
- **VERIFY**, on a `--live` run, can't open the draft pull request after
  checks pass — the branch still gets pushed, but no PR appears. The night
  records `pr_failed`, not `pr_opened`.
- **`forge followup`** can't ask GitHub whether a PR was merged, so it does
  nothing, every time, without saying why.

Nothing crashes: a missing token is built to look exactly like GitHub being
briefly unreachable, so the Forge degrades instead of breaking. **If the
ledger keeps recording `pr_failed`, or draft PRs simply never show up, check
this first** before suspecting anything else. Set `GITHUB_TOKEN` in whatever
environment runs the Forge — the Routine, or the GitHub Actions secret if
you move to that later — and never write the token's actual value into
`forge.json` or commit it anywhere in this repo.

## Commands

```bash
cd forge && pip install -e .

forge init                  # write a starter forge.json
forge config                # show the leash currently in force
forge sense                 # gather signals into forge/state/pulse.json
forge decide                # score the latest pulse into forge/state/tonight.json
forge run --dry-run         # sense + decide + ledger line, change nothing (the default)
forge run --live            # the full cycle: hands the pick to Crew, may open a draft PR
forge ledger                # read the record back (add --limit N for more than 10 rows)
forge followup              # backfill merged / human_edits for PRs opened on earlier nights
```

`forge run` with neither flag is a dry run — that is the default, not an
oversight, and is what week one should run every night.

## The leash

`forge.json` at the repo root. Every field is optional; defaults apply
otherwise, and `forge init` writes a starter file with these same defaults.

| Field | Default | Meaning |
|-------|---------|---------|
| `base_branch` | `main` | What every PR is opened against, and what the tree is returned to when a run ends — see the callout below |
| `budget_usd` | `5.0` | Spend cap for one run — see the caveat below, it does not bind today |
| `max_files_touched` | `12` | A run that changes more than this is abandoned |
| `safe_zones` | `docs/`, `madlibs/`, `music/`, `shooter/`, `scraper/`, `crew/tests/` | Where it may work |
| `no_touch` | `forge/`, `.github/workflows/` | Where it may never work — these two are welded on in code and cannot be removed by editing `forge.json` |
| `score_floor` | `4.0` | Below this, the night records "nothing worth doing" |
| `strike_limit` | `3` | Failures on the same task before it is quarantined to `forge/stuck.md` |
| `crew_timeout_min` | `45` | Wall-clock cap on the Crew subprocess |
| `weights` | see `forge/forge/config.py` | The scoring numbers |

**Get `base_branch` right, or the Forge's work goes to the wrong place.**
Every PR the Forge opens declares this branch as its `base`, and the working
tree is checked out back to it both when a run fails and after a run
succeeds — so if it names the wrong branch, every PR is opened against the
wrong line of the project, and every successful night silently moves the
working tree there too, before tomorrow's branch is even cut. `main` is the
right default for most repositories, which is why it's the default — but
this repo's actual default branch is not `main` (`main` here is a separate,
unrelated line of the project), so **this repo's `forge.json` sets
`base_branch` explicitly**. If you copy the Forge into another repository,
check what that repo's default branch actually is before trusting the
default.

**The budget cap does not bind in production.** Crew doesn't currently report
its cost in any machine-readable way, so a real run always records
`cost_usd: null` ("not measured"), and a value that isn't there can't be
compared against a cap. `budget_usd` is fully enforced under the test suite
(which injects a fake Crew that does report a cost) and will start
enforcing for real the day Crew gains a `--json`-style cost report — but as
shipped, do not read it as a live spending guarantee. `max_files_touched`,
by contrast, is checked against the real diff on every live run and does
bind today.

**A safe zone only means something if a check backs it up.** Every safe zone
now runs at least the exchange gate (`node scripts/check-exchange.mjs`, see
below) — `docs/`, `madlibs/`, and `shooter/` included — so no zone is ever
verified by literally nothing. But `docs/` still has no project of its own,
and `madlibs/` and `shooter/` have no test command of their own in
`PROJECT_CHECKS` today: their own code is exercised only when a declared
consumer's checks pick it up (see the exchange-checks section below), which
is nothing if nobody currently consumes them. `music/`, `scraper/`, and
`crew/tests/` are the three zones with a real own-zone command that runs
regardless. A zone in `safe_zones` with **no entry at all** in
`forge/forge/checks.py`'s `ZONE_PROJECT` is a different, worse problem than
"nothing to run": VERIFY now fails closed for it (`verify_failed`, checks
never even attempted) rather than silently recording green. Before widening
`safe_zones` to a new directory (see *Rollout* below), give it an entry in
`ZONE_PROJECT` — and a real check command in `PROJECT_CHECKS` if it has
code of its own worth testing — or the run either fails every night or the
work landing there is unverified while looking exactly like everything else
that isn't.

**A zone's checks now include everything downstream of it, and a consumer
with no checks of its own now fails closed the same way an unmapped zone
does.** `shared/exchange.json` records which projects use each other —
today only that SCRIPT FORGE loads five of SONG FORGE's files. So a change
in `music/` runs `film/`'s tests as well as music's own, and `checks: green`
means both passed — because `film` is registered in `forge/forge/checks.py`'s
`PROJECT_CHECKS`. `all_commands` looks each consumer up in that same map,
and a project named in `consumes` with **no** entry there now raises
`checks.UnmappedConsumerError` rather than silently contributing zero
commands: VERIFY reports `verify_failed` with `checks: red`, the same as an
unmapped zone (see above), instead of `checks: green` claiming to have
verified a downstream project it never ran a single command against. Give a
new consumer a `PROJECT_CHECKS` entry *before* declaring it in
`shared/exchange.json`, or the first night touching what it consumes fails
closed rather than recording green over an empty check list.
Before this exchange check existed at all, the Forge could break SCRIPT
FORGE, record green, and open a pull request describing verified work; CI on
that pull request caught the break, but the ledger — the permanent record of
the loop's judgement — carried a false claim.

**What "`film`'s tests passed" actually covers, and what it doesn't.**
`shared/exchange.json` declares `music/composer` as five files —
`theory.js`, `genres.js`, `synth.js`, `composer.js`, `engine.js` — and
`film/index.html` loads all five, in that order. Three things now stand
between a broken one of those files and a false `checks: green`:

- `scripts/check-exchange.mjs` parses every published file as JavaScript
  (`new vm.Script(...)`) and fails naming the file if it isn't — this alone
  catches a file replaced with garbage, or a stray syntax error, regardless
  of whether anything ever loads it in a sandbox.
- `film/tests/film-logic.test.js` loads all five files into its vm sandbox
  (not just the three — `theory.js`, `genres.js`, `composer.js` — its other
  assertions happen to call into) and asserts each one actually defines its
  global. `synth.js` and `engine.js` are Web Audio code, but neither one
  touches `AudioContext` at load time — only lazily, inside functions this
  first pass never calls — so both load in plain Node with no browser and no
  stub. This proves both files are syntactically valid, and that loading
  them, in the real order SCRIPT FORGE loads them in, does not throw — not
  the same claim as side-effect-free, which nothing here checks.
- The same suite goes past the five bare globals to the specific members
  code actually calls: `Composer.compose`, `Engine.Player`,
  `Engine.Player.prototype.load` and `Genres.GENRES` (what
  `film/js/film-audio.js` itself reaches for), plus `Theory.midiToFreq`,
  `Genres.PRESETS`, and `Synth.playNote` / `playDrum` / `softClipCurve` /
  `reverbImpulse` / `vinylBuffer` (what `engine.js` reaches for internally
  to build its mixer graph and schedule notes) — asserting each is present
  with the right type, so a global staying truthy while one of its own
  exports is renamed or deleted no longer passes. One more test composes a
  real song with `Composer.compose` and loads it into a real
  `Engine.Player` — the same two calls `Score.prototype.startScore` makes —
  and asserts that chain runs without throwing. This list was produced by
  reading `film/js/film-audio.js` and `engine.js`, not derived
  structurally: a call added later to either file is covered here only once
  someone reads it in and extends the assertions the same way.

None of this runs the audio graph `synth.js` and `engine.js` build, proves a
bar of music actually sounds right, or catches a member of the right *type*
that does the wrong thing — `Theory.midiToFreq` returning the wrong number
for a given pitch is still a function, and passes every assertion above.
Only a real `AudioContext`, which Node does not have, can settle that. The
one check that does exercise the actual audio graph is
`film/tests/film-browser.test.js`, run against a real Chromium in
`.github/workflows/site-ci.yml`'s browser job, on every pull request —
**not** by the Forge's own nightly `PROJECT_CHECKS` entry for `film`, which
is the plain `node film/tests/film-logic.test.js` above. So a `music/`-only
night's `checks: green` now guarantees the published files parse, load
without throwing in the declared order, and that the specific surface
`film`'s own code and `engine.js`'s internals reach for still exists with
the right shape and can be driven end to end without throwing; it does not
guarantee that surface computes the right values, or that the resulting
audio is correct — that half of the guarantee still lives entirely in CI,
after the PR is already open, exactly like the gap the *Two separate
failure paths* paragraph below describes for a broken declaration.

Two separate failure paths follow from a broken `shared/exchange.json`, and
they cost differently. If it is already missing or malformed when DECIDE
runs, DECIDE treats it as an early exit: every candidate is skipped
(`config_error`), nothing is picked, and the night records `no_task` —
cheap, because nothing ran. But if the declaration is intact at DECIDE and
breaks afterward — during DO, while the coding agent is still working — a
candidate has already been picked and the agent has already run by the time
VERIFY tries to read it. `run_checks_for_files` — the function VERIFY
actually calls on a live run — then fails closed the same way DECIDE does,
but the night records `verify_failed` with `checks: red` and a
quarantine strike against that candidate, after real cost was spent. Either
way nothing is falsely reported green; a `verify_failed` entry with no test
output beyond "cannot tell what this change could break" is this condition,
not a real test failure.

Adding a cross-project `<script>` tag without declaring it in
`shared/exchange.json` fails CI (`scripts/check-exchange.mjs`), so the
declaration cannot quietly rot.

## Outcomes

Every run — dry or live — writes exactly one of these to the ledger:

| Outcome | Meaning |
|---|---|
| `dry_run` | `forge run --dry-run`: sensed, decided, recorded, changed nothing |
| `no_task` | Nothing cleared the score floor; the night is a deliberate no-op |
| `pr_opened` | Checks were green, the branch pushed, and GitHub actually handed back a PR number — this is the only outcome that guarantees a PR exists to look at |
| `pr_failed` | Checks were green and the branch reached the remote, but the PR call itself came back empty — almost always a missing `GITHUB_TOKEN` (see above). The branch is left pushed with no PR |
| `push_failed` | Checks were green but the branch could not be pushed to the remote |
| `verify_failed` | Crew's changes failed the checks for the zones the changed files actually touch (see VERIFY above, not the zone DECIDE chose); branch abandoned |
| `crew_failed` | Crew crashed, timed out, touched a no-touch or out-of-zone path, or the working tree had uncommitted changes (other than the Forge's own bookkeeping) when the run tried to start |
| `budget_exceeded` | Crew reported a cost over `budget_usd` (only possible with a cost-reporting runner — see above) |

A live run refuses to start if the working tree has uncommitted changes —
checked before anything is created, so a human's in-progress edits are never
mistaken for the Forge's own and never end up on a Forge branch. The one
exception is the Forge's own bookkeeping left over from the *previous* run —
the ledger line, `forge/stuck.md`, its scratch state, and the
codebase-memory note — which never blocks the next run from starting.
Anything else left uncommitted, from you or anything else, still stops it
cold.

Every run also leaves the tree back on the base branch (`base_branch` in
`forge.json`, `main` by default) when it finishes, whether it opened a PR
or abandoned the branch, so tomorrow's run always branches from the same
known point rather than from tonight's own branch.

## The ledger

`forge/ledger/YYYY-MM.jsonl` — one JSON line per run, committed to git, never
deleted. It is the most important thing the Forge produces: everything else
here could be rebuilt, and six months of records could not.

Two fields start out `null` and are filled in later, by running
`forge followup`, once you have looked at a PR the Forge opened:

- **`merged`** — did you take the work (`true`/`false`)
- **`human_edits`** — how many lines you changed before merging (`0` for
  merged-as-is; `null` if GitHub couldn't tell us)

Green checks only prove nothing broke. *Merged with zero edits* is the only
evidence that the work was actually good, and it is what a future learned
scorer will train on.

## Scheduling

The brain lives in this repo; the alarm clock is deliberately external, so it
can be swapped without touching any code.

**Now — a Claude Code Routine.** Create a scheduled trigger that wakes a fresh
session nightly with a prompt like:

> Run the Forge for tonight: `cd forge && python -m forge.cli run --dry-run --root ..`
> then `python -m forge.cli followup --root ..`. Commit any ledger change on a
> branch and open a draft PR if the run opened one. Do not merge anything.

Start with `--dry-run` and only switch the Routine's prompt to `--live` once
you've read a couple of weeks of ledger entries and are comfortable with what
it's picking. Whatever runs the Routine needs `GITHUB_TOKEN` set in its own
environment too (see *GITHUB_TOKEN* above) — a Routine session doesn't get
one for free the way a GitHub Actions run does.

**Later, optionally — GitHub Actions.** Add `.github/workflows/forge.yml` on a
cron doing the same thing, with an `ANTHROPIC_API_KEY` repo secret for Crew.
Nothing in `forge/` knows how it was invoked, so this is a drop-in swap.

## Rollout

| | What runs | What you do |
|---|---|---|
| **Week 1** | `forge run --dry-run` nightly | Read `forge ledger` over coffee. As configured today expect `no_task` every night (see *What it will actually do tonight*) — that's the leash working. To watch it act, add one Phase 14 roadmap line naming a real file in a safe zone (see *How to give it a job*) and read the ledger the next morning |
| **Week 2** | `forge run --live`, safe zones only | Review the draft PRs |
| **Week 3+** | Widen `safe_zones` as the ledger justifies — and give each new zone an entry in `checks.py`'s `ZONE_PROJECT` (plus a real check command in `PROJECT_CHECKS` if it has code of its own worth testing) first, or the run fails closed the first live night and the work landing there is unverified even after that's fixed (see *The leash* above) | Then start P2, the Exchange |
