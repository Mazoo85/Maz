# The Maz Inventory

One command that answers three questions about this repository:

1. **What exists here?** Every program, demo, engine capability, tool, CI gate and document —
   counted, described and grouped.
2. **What is each one still missing?** Measured against checks that can be answered by looking at
   the files, not by judging the work: does this app build, does it run headless, does a test
   exercise this engine module, does any app demonstrate it, does this browser project have tests.
3. **What could each one lend the others?** The opportunities the scan can prove (a helper written
   out in four projects; 550 engine features nothing demonstrates) alongside a reviewed table of
   pairings that judgement, not a scanner, has to supply.

The output is [`docs/INVENTORY.md`](../../docs/INVENTORY.md) for people and
[`docs/inventory.json`](../../docs/inventory.json) for programs.

## Use

```sh
node tools/inventory/main.mjs            # print the report
node tools/inventory/main.mjs --write    # write docs/INVENTORY.md and docs/inventory.json
node tools/inventory/main.mjs --check    # fail if either file is out of date (this is the CI gate)
node tools/inventory/main.mjs --summary  # just the numbers
node tools/inventory/main.mjs --json     # the machine-readable model on stdout
node tools/inventory/tests/inventory.test.mjs   # the tests
```

No dependencies. It runs on a bare `node`, like `scripts/check-links.mjs` and
`scripts/check-exchange.mjs`, so it works in CI and on a machine with no Vulkan SDK and no Python.
A full scan of the repository takes well under a second.

**Regenerate the report in the same commit as any change that moves its numbers**, or
`--check` fails the build. That is the point of the gate: a catalogue nobody regenerates is worse
than no catalogue, because it is confidently wrong.

## How it decides what an app demonstrates

Apps include `maz/Engine.hpp` and nothing else, and that umbrella header pulls in 150+ modules, so
`#include` says nothing about what an app actually uses. Type *names* do: the header at
`engine/include/maz/game/AStar2D.hpp` declares `game::AStar2D`, and an app demonstrating it has to
write that identifier somewhere. So the scanner tokenises every app and every test once into an
identifier index and asks that index which files name each module.

It is a heuristic and it is worth knowing its shape: a false positive needs a same-named identifier
somewhere unrelated, and a false negative needs a module whose own type is never named — usually
one reached only through another module's API. Both readings are useful, which is why the report
says "no app names this type" rather than "this is unused".

## Layout

| File | What it does |
|---|---|
| `main.mjs` | The CLI: flags, reading and writing the two output files, the `--check` gate. |
| `lib/model.mjs` | Runs every scanner and check, and assembles the one object everything else reads. |
| `lib/scan-cpp.mjs` | Engine headers, `apps/`, `tests/`, and the identifier index that links them. |
| `lib/scan-web.mjs` | The browser projects, `shared/projects.js`, `shared/exchange.json`, duplicate helpers. |
| `lib/scan-repo.mjs` | Python tools, the `scripts/` gates, `tools/` helpers, and `docs/`. |
| `lib/checks.mjs` | What "finished" means per kind of artifact. Each failing check names its own fix. |
| `lib/synergy.mjs` | Cross-pollination: computed opportunities, plus the reviewed pairings table. |
| `lib/queue.mjs` | Collapses every gap into a ranked task list with the paths each one touches. |
| `lib/render.mjs` | Renders `docs/INVENTORY.md`. Deterministic — no clock, or `--check` would cry wolf. |

## Adding a check

A check lives in `lib/checks.mjs` under its artifact's kind, and is four fields: an `id`, a `label`
(what passing looks like), a `missing` phrase (what failing looks like, for the report's gap
column), and a `fix` written as an instruction, because that sentence is what ends up in the work
queue and in front of whoever or whatever does the job.

Keep checks mechanical. "Does PONG run headless so CI can capture it" is a check. "Is PONG a good
game" is not.

## Adding a pairing

`DECLARED_PAIRINGS` in `lib/synergy.mjs` is judgement written down — "SONG FORGE can score the
silent browser games" is not something a scanner concludes. Every entry names real artifact ids
(`web:music`, `app:pong`, `engine:audio/MusicTheory`, `py:forge`), and `--check` fails on one that
names something deleted. That validation is what stops the table quietly becoming fiction as the
repo moves underneath it.

## Who reads the output

- **You**, in `docs/INVENTORY.md`.
- **The Forge**, through `forge/forge/signals/inventory.py`, which turns the queue in
  `docs/inventory.json` into candidates for the nightly run — so gaps found here become work that
  gets done without anyone asking.
