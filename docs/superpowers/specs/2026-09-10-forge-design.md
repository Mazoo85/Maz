# The Forge — design spec

**Date:** 2026-09-10
**Status:** built — see `docs/FORGE.md` for the operator's guide. The "Trusted"
criteria below remain open until thirty nights have run.
**Scope:** P1 of a three-part system (see *Where this fits*)

---

## The problem

The Maz repo holds seven separate things — a C++ game engine, three browser games/apps, a
scraper, an AI agent crew, and a persistent memory server. They grow only when a human sits
down and moves one of them. They do not help each other, and they do not move on their own.

The goal is a system where everything already built and everything built in future feed one
continuous loop that runs and updates itself.

## Where this fits

The full goal decomposes into three sub-projects, each with its own spec and build:

| | Sub-project | Delivers | Status |
|---|---|---|---|
| **P1** | **The Forge** — the spine | A loop that runs nightly, does one useful thing, records what happened | **built** |
| **P2** | **The Exchange** — the bus | Projects publish and consume each other's output | later |
| **P3** | **Taste** — the scoring brain | The loop learns which work actually pays off | later |

The order is deliberate. Autonomy (P1) is cheapest to stand up and becomes the machine that
builds P2. Unification (P2) is worth little without something to drive it. Compounding (P3)
can only learn from a record of outcomes, so it must come last — but **the record it learns
from has to be collected from P1's first night**, which is why the ledger is in this spec.

## What already exists

The Forge is mostly wiring, not invention. Every organ is already in the repo:

| Existing piece | Role in the loop |
|---|---|
| `crew/` — Maz Crew (plan → code → review → test agents) | The hands |
| codebase-memory MCP server | The memory |
| `.github/workflows/` (5 workflows) | The immune system |
| `docs/ROADMAP.md` (14 phases of checkboxes) | The appetite |
| `.claude/skills/` (Superpowers) | The habits |
| `scraper/` — maz-scrape | The senses (P2) |

---

## Design

### The cycle

One program, six steps, runs on a timer, then stops.

```
  +--------------------------------------------------+
  |                                                  |
  v                                                  |
SENSE ---> DECIDE ---> DO ---> VERIFY ---> LEARN -----+
  |           |         |        |           |
reads      scores     hands    tests,      writes
repo       tasks,     it to    opens       ledger +
state      picks 1    Crew     a PR        memory
```

**SENSE** — gather the repo's vital signs into one file:

- unchecked checkboxes in `docs/ROADMAP.md`, with their phase and milestone
- conclusions of the most recent run of each GitHub Actions workflow
- `TODO` / `FIXME` / `HACK` comments in tracked source, with file recency
- observations from `.claude/codebase-memory.json` marking work as partial
- open GitHub issues labelled `forge`

**DECIDE** — score every candidate, pick exactly one, and record why it won over the
runners-up. That written reason is P3's training data.

**DO** — hand the single task to Maz Crew on a fresh branch `forge/YYYY-MM-DD-<slug>`.

**VERIFY** — run the repo's checks. Green: push the branch and open a **draft PR**. Red:
abandon the branch and record the failure.

**LEARN** — append one line to the ledger, tick the roadmap checkbox if the PR merges, and
write observations back to codebase-memory.

### The leash

Non-negotiable for the initial build. Each can be loosened later, on evidence from the ledger.

1. **Never pushes to `main`.** Output is always a draft PR. The worst case for a bad night is
   a PR that gets closed.
2. **One task per run.** Not "as much as it can fit."
3. **A hard budget** in both spend and files touched, enforced before Crew is invoked and
   again before the PR is opened.
4. **Safe zones.** Initially: `docs/`, `*/tests/`, `madlibs/`, `music/`, `shooter/`, and
   generated content. The C++ engine core (`engine/`), CMake files, and `.github/workflows/`
   are human-only until the ledger earns wider access.
5. **No self-modification.** `forge/` and `.github/workflows/` are a hard no-touch zone. The
   thing that decides does not get to rewrite its own rules.
6. **One command to undo.** Every night is its own branch; nothing is entangled.

   The no-touch rule binds the Forge, not the human. Adding `.github/workflows/forge.yml`
   by hand later is expected; the Forge simply may never author a change to that directory
   or to `forge/` itself.

### Files

```
forge.json                     leash: budget, safe zones, no-touch zones, scoring weights
forge/                         the brain (~400 lines of Python, sits beside crew/)
  cli.py                       forge sense | decide | do | verify | learn | run
  sense.py                     gathers signals into pulse.json
  decide.py                    scores candidates, writes tonight.json
  do.py                        invokes Maz Crew on a fresh branch
  verify.py                    runs checks, pushes, opens the draft PR
  learn.py                     appends the ledger line, updates roadmap + memory
  config.py                    loads and validates forge.json
  signals/roadmap.py           parses ROADMAP.md checkboxes
  signals/ci.py                reads recent workflow conclusions
  signals/todos.py             scans tracked source for TODO/FIXME/HACK
  signals/memory.py            reads .claude/codebase-memory.json
  tests/                       unit tests (parsers + scoring)
forge/state/pulse.json         tonight's vital signs — scratch, gitignored
forge/state/tonight.json       the chosen task and why — scratch, gitignored
forge/ledger/YYYY-MM.jsonl     THE RECORD — committed to git, never deleted
forge/stuck.md                 quarantined tasks that failed three nights running
docs/FORGE.md                  how it works, and how to stop it
```

`forge/state/` is added to `.gitignore`. `forge/ledger/` is not — the ledger is the most
important artifact the system produces.

### The ledger

One JSON object per line, appended per run. Fields:

| Field | Meaning |
|---|---|
| `run_id` | date + sequence |
| `chose` | the task, in the words it was given to Crew |
| `source` | where the idea came from (`roadmap:phase-0`, `ci:music-ci`, `todo:js/game.js:412`) |
| `why` | value, confidence, risk, final score, and the top three runners-up with their scores |
| `zone` | which safe zone the task fell in |
| `outcome` | `pr_opened` / `verify_failed` / `crew_failed` / `no_task` / `budget_exceeded` |
| `pr` | PR number, when one was opened |
| `checks` | final CI state |
| `files_touched`, `cost_usd`, `duration_min` | the bill |
| `notes` | free text from the run |

A **follow-up pass**, run on later nights against older ledger lines, fills in two more fields:

| Field | Meaning |
|---|---|
| `merged` | did a human merge the PR |
| `human_edits` | how many lines the human changed before merging |

These two are the real quality signal. `checks: green` only means nothing broke.
`merged: true, human_edits: 0` means the work was actually good. P3 learns from these; P1's
only job is to start collecting them.

### Scoring

Hand-tuned arithmetic, readable at a glance. No model, no training, in P1.

```
score = (value * confidence) / (1 + risk)
```

**Value** (highest first): a red CI check · a roadmap item in the current milestone (M0) · a
`TODO` in a file changed recently · a roadmap item in a later phase.

**Confidence**: raised by small expected file count, existing nearby tests, and sitting well
inside a safe zone.

**Risk**: raised by touching the build system, the C++ engine core, or anything cross-cutting.

Three modifiers on top:

1. **Three strikes** — a task that fails three runs is moved to `forge/stuck.md` and excluded
   until a human clears it.
2. **Variety** — the same project is not chosen three runs in a row.
3. **A score floor** — if nothing clears it, the run records `no_task` and exits clean. A loop
   forced to produce output will produce garbage.

All weights live in `forge.json` so tuning never requires a code change. (JSON
rather than TOML: Python 3.10 has no stdlib TOML reader, and `crew.json` already
set the pattern for a committable, dependency-free config file.)

### Failure handling

| Failure | Behaviour |
|---|---|
| Crew hangs | Wall-clock timeout, kill, record `crew_failed`, no PR |
| Crew crashes | Record `crew_failed` with the error, abandon branch |
| Checks fail after Crew's bounded repair loop | Abandon branch, record `verify_failed`; the failure is a signal SENSE reads tomorrow |
| Budget exceeded mid-run | Stop, abandon branch, record `budget_exceeded` |
| Nothing scores above the floor | Record `no_task`, exit 0 |
| A signal source is unreadable | Log it, continue with the remaining signals — one broken parser must not stop the night |

### Testing

- Unit tests on each parser in `forge/signals/` against fixture files, including malformed input.
- Unit tests on `decide.py` scoring: a fixed `pulse.json` fixture must produce a known ranking.
- `forge run --dry-run` performs SENSE and DECIDE, writes the ledger line, and stops without
  touching a file. This is both a test harness and the week-one operating mode.
- The Forge's own tests run in the existing CI, alongside `crew`'s.

### What runs it

The scheduling and the logic are deliberately separate. The brain lives in the repo; only the
alarm clock differs.

- **Now:** a Claude Code Routine (scheduled trigger) that wakes a fresh session nightly and
  runs `forge run`. No API key to manage, no CI minutes, and it inherits the repo's Superpowers
  skills.
- **Later, optionally:** a `.github/workflows/forge.yml` cron doing the same thing via Maz
  Crew with an `ANTHROPIC_API_KEY` repo secret. Fully self-hosted, portable, and a small swap
  because nothing in `forge/` depends on how it was invoked.

### Rollout

- **Week 1 — dry runs only.** Nightly SENSE + DECIDE + ledger line, nothing touched. The
  scoring gets corrected against real picks before the system has ever written code.
- **Week 2 — live, safe zones, draft PRs only.**
- **Week 3+ — widen the safe zones** as the ledger justifies it, then begin P2.

---

## Success criteria

**Build complete** when all three hold:

1. `forge run --dry-run` produces a sensible, explained pick from the real repo state.
2. A live run produces a draft PR whose checks pass, on a real roadmap or CI item.
3. A deliberately broken task is abandoned cleanly, recorded, and deprioritised the next run.

**Trusted** — the separate bar for widening the safe zones and starting P2:

4. Thirty consecutive nights have run unattended with a readable ledger and no incident
   requiring manual repair of the repo.
5. The ledger contains `merged` / `human_edits` data for every PR the Forge has opened.

## Explicitly out of scope for P1

- Any cross-project wiring, shared artifact formats, or hub page (that is P2).
- Any learned or adaptive scoring (that is P3) — weights are hand-set in `forge.json`.
- Auto-merge of any kind. Every change is a human-merged draft PR.
- Multi-task runs, parallel branches, or the Forge modifying its own configuration.
