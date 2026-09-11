# The Forge

A loop that wakes up every night, reads the state of this repo, does **one**
useful thing, and writes down what happened.

Full design: [`docs/superpowers/specs/2026-09-10-forge-design.md`](superpowers/specs/2026-09-10-forge-design.md)

## How to stop it

Read this first, because it matters more than anything else here.

- **Stop tonight's run:** disable the scheduled Routine (see *Scheduling* below).
  Nothing else is needed — the Forge holds no state between runs.
- **Undo a night:** every live run works on its own `forge/YYYY-MM-DD-<slug>`
  branch, cut fresh from `main`, and opens a *draft* pull request. Close the
  PR and delete the branch — nothing else was touched, and no other night's
  branch carries those commits. Nothing the Forge does is ever merged
  without you — it has no ability to merge anything itself.
- **Stop it touching a directory:** add the path to `no_touch` in `forge.json`.
- **Stop it entirely and permanently:** delete `forge.json`. Without a config
  file the defaults still apply (they are the same safe zones and limits), so
  to actually stop it you must also disable the Routine.

The Forge never pushes to `main`, never merges anything, and can never modify
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

So a night will keep recording "nothing worth doing" (`no_task`, or
`considered: 97 / outside_zone: 97` if you run `forge sense` and `forge
decide` yourself) until one of two things changes: either the roadmap or
memory signals start carrying real file paths, or `safe_zones` in
`forge.json` is widened to cover code the Forge can actually reach (see
*Rollout* below). This is the conservative design working as intended, not
something to fix in a hurry — but don't expect to wake up to draft PRs until
you've made one of those changes.

## The cycle

```
SENSE -> DECIDE -> DO -> VERIFY -> LEARN -> (tomorrow)
```

| Step | What it does |
|------|--------------|
| **SENSE** | Reads unchecked roadmap boxes, red CI runs, `TODO` markers, and codebase-memory into `forge/state/pulse.json` |
| **DECIDE** | Scores every candidate, applies three strikes / variety / floor, writes the pick and the reasoning to `forge/state/tonight.json` |
| **DO** | Refuses to start if the working tree has uncommitted changes anywhere but the Forge's own bookkeeping (see *Outcomes* below); otherwise creates a branch and hands the single task to Maz Crew |
| **VERIFY** | Runs the zone's checks; green pushes the branch and opens a draft PR, red abandons the branch — either way, the tree is back on `main` when the run ends |
| **LEARN** | Appends one line to `forge/ledger/YYYY-MM.jsonl` |

## GITHUB_TOKEN

Set this (or `GH_TOKEN`) as an environment variable wherever the Forge runs,
before you rely on anything below. Without it, three things quietly stop
working — none of them loudly:

- **SENSE** can't see which GitHub Actions workflow is currently red on
  `main` — that signal source is just silently empty every night, as if
  everything were green.
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
| `budget_usd` | `5.0` | Spend cap for one run — see the caveat below, it does not bind today |
| `max_files_touched` | `12` | A run that changes more than this is abandoned |
| `safe_zones` | `docs/`, `madlibs/`, `music/`, `shooter/`, `scraper/`, `crew/tests/` | Where it may work |
| `no_touch` | `forge/`, `.github/workflows/` | Where it may never work — these two are welded on in code and cannot be removed by editing `forge.json` |
| `score_floor` | `4.0` | Below this, the night records "nothing worth doing" |
| `strike_limit` | `3` | Failures on the same task before it is quarantined to `forge/stuck.md` |
| `crew_timeout_min` | `45` | Wall-clock cap on the Crew subprocess |
| `weights` | see `forge/forge/config.py` | The scoring numbers |

**The budget cap does not bind in production.** Crew doesn't currently report
its cost in any machine-readable way, so a real run always records
`cost_usd: null` ("not measured"), and a value that isn't there can't be
compared against a cap. `budget_usd` is fully enforced under the test suite
(which injects a fake Crew that does report a cost) and will start
enforcing for real the day Crew gains a `--json`-style cost report — but as
shipped, do not read it as a live spending guarantee. `max_files_touched`,
by contrast, is checked against the real diff on every live run and does
bind today.

**A safe zone only means something if a check backs it up.** `docs/`,
`madlibs/`, and `shooter/` have nothing to run — there's nothing to test,
and CI on the pull request is the real gate, which is fine and deliberate.
But `music/`, `scraper/`, and `crew/tests/` are recorded green *because* a
real command ran and passed. A zone with no matching command in
`forge/forge/checks.py` passes its checks trivially — the run still gets
recorded as `checks: green`, having verified nothing at all. Before widening
`safe_zones` to a new directory (see *Rollout* below), give it a real check
command first, or the work landing there is unverified while looking
exactly like everything else that isn't.

## Outcomes

Every run — dry or live — writes exactly one of these to the ledger:

| Outcome | Meaning |
|---|---|
| `dry_run` | `forge run --dry-run`: sensed, decided, recorded, changed nothing |
| `no_task` | Nothing cleared the score floor; the night is a deliberate no-op |
| `pr_opened` | Checks were green, the branch pushed, and GitHub actually handed back a PR number — this is the only outcome that guarantees a PR exists to look at |
| `pr_failed` | Checks were green and the branch reached the remote, but the PR call itself came back empty — almost always a missing `GITHUB_TOKEN` (see above). The branch is left pushed with no PR |
| `push_failed` | Checks were green but the branch could not be pushed to the remote |
| `verify_failed` | Crew's changes failed the zone's checks; branch abandoned |
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

Every run also leaves the tree back on `main` when it finishes, whether it
opened a PR or abandoned the branch, so tomorrow's run always branches from
the same known point rather than from tonight's own branch.

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
| **Week 1** | `forge run --dry-run` nightly | Read `forge ledger` over coffee. As configured today expect `no_task` every night (see *What it will actually do tonight*) — that's the leash working. If you want to see it act sooner, either widen `safe_zones` in `forge.json` to cover real code, or add file paths to the roadmap/memory signals it reads |
| **Week 2** | `forge run --live`, safe zones only | Review the draft PRs |
| **Week 3+** | Widen `safe_zones` as the ledger justifies — and give each new zone a real check command in `checks.py` first, or the work there lands unverified (see *The leash* above) | Then start P2, the Exchange |
