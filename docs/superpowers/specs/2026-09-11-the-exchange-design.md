# The Exchange — declared dependencies between projects

**Date:** 2026-09-11
**Status:** draft, awaiting approval
**Sub-project:** P2 of 3 in the feedback loop (see
[`2026-09-10-forge-design.md`](2026-09-10-forge-design.md) — P1 the Forge is
built and merged; P3 Taste is later)

## The goal

Make it a build failure for one project in this repo to depend on another
without saying so, and make the Forge's word "verified" cover the projects
its change can actually break.

## What the repo actually looks like

This spec was written after measuring, not after imagining, and the
measurement changed its scope. Every cross-project reference in every app
page:

```
film      ->  music shared
madlibs   ->  shared
music     ->  shared
shooter   ->  shared
zomboid   ->  shared
```

`shared/` is the nav bar and the project list — every page uses it, and it is
not a dependency in any interesting sense. So the real graph is **one edge**:
SCRIPT FORGE consumes SONG FORGE, by loading five of its files in order:

```html
<script src="../music/js/theory.js"></script>
<script src="../music/js/genres.js"></script>
<script src="../music/js/synth.js"></script>
<script src="../music/js/composer.js"></script>
<script src="../music/js/engine.js"></script>
```

That edge is in better shape than expected. `film/tests/film-logic.test.js`
loads SONG FORGE's real files into a VM sandbox and asserts the contract
holds — every film genre maps to a genre and mood SONG FORGE actually has.
Site CI has no path filter, so that test runs on every pull request.

**A one-edge graph does not need a bus.** An artifact store, a wire format
and a runtime broker would be machinery in search of a use. What follows is
deliberately much smaller than the word "Exchange" suggests.

## What is actually broken

**1. The Forge's "green" is not honest.** Its check for the `music/` zone is
`node music/tests/music-logic.test.js` and nothing else
(`forge/forge/checks.py`). It can change `music/js/composer.js`, break
SCRIPT FORGE, record `checks: green`, and open a pull request describing
verified work. Site CI catches the break afterwards, so nothing ships broken
— but the ledger, which is the permanent record of the loop's judgement,
contains a false claim. The Forge is scheduled nightly as of today and
`music/` is one of its safe zones, so this is live, not hypothetical.

**2. Nothing declares the dependency.** That SCRIPT FORGE consumes SONG
FORGE is written down in exactly one place: a `require` inside a test file
belonging to the other project. Someone editing `music/js/composer.js` — or
something, on a schedule, at 08:00 UTC — has no signal that anything
downstream cares.

**3. `music-ci.yml` is path-filtered** to `music/**`, so the contract test
does not run in music's own CI either. Only the unfiltered site CI saves it.

## What this is not

No artifact store. No publish/subscribe at runtime. No new file format, and
nothing new loaded by any browser page. Nothing about how the apps behave
changes. This is declarations plus enforcement.

## Architecture

Three pieces, each usable without the others.

```
shared/exchange.json          who publishes what, who consumes it
        │
        ├── scripts/check-exchange.mjs   proves the declarations are true
        │       run by site-ci, no path filter
        │
        └── forge/forge/checks.py        a zone's checks now include the
                                         checks of everything downstream
```

### 1. `shared/exchange.json` — the declaration

A new plain-JSON file at the repo root's `shared/`. Shape:

```json
{
  "publishes": {
    "music/composer": {
      "project": "music",
      "summary": "Compose and play a song from a genre, mood and length.",
      "files": [
        "music/js/theory.js",
        "music/js/genres.js",
        "music/js/synth.js",
        "music/js/composer.js",
        "music/js/engine.js"
      ]
    }
  },
  "consumes": [
    {
      "project": "film",
      "id": "music/composer",
      "via": "script",
      "page": "film/index.html",
      "contract": "film/tests/film-logic.test.js"
    }
  ]
}
```

**Why a separate JSON file and not `shared/projects.js`,** which I proposed
in conversation. Two reasons found while designing. `projects.js` is a
browser IIFE whose own header says it exists because "the hub and the in-app
nav bar both read this file" — neither needs dependency data, and every
visitor would download it. More decisively, the Forge is Python: parsing a
JavaScript IIFE from Python to learn what depends on what is the kind of
cleverness that breaks quietly. Plain JSON is read by `node` and by Python
with no parsing tricks in either. `check-exchange.mjs` cross-validates that
every `project` named here exists in `shared/projects.js`, so there is still
one source of truth for *what projects exist*; this file only records *how
they depend*.

`files` is ordered, and the order is meaningful: it is the load order the
consumer must use.

### 2. `scripts/check-exchange.mjs` — the enforcement

Run by site-ci beside `check-links.mjs`, which has no path filter, so it runs
on every pull request. It fails the build on any of:

| Rule | Why |
|---|---|
| A `consumes.id` nobody publishes | A dangling dependency |
| A `project` not in `shared/projects.js` | The two manifests have drifted |
| A publisher's `files` entry that does not exist | The declaration is stale |
| A declared `via: "script"` link whose `page` does not load exactly those files, in that order | The declaration does not match reality |
| A cross-project `<script src="../other/...">` in any app page with no matching `consumes` | **Undeclared coupling** — the rule that does the work |
| A `contract` file that does not exist | The contract is claimed but not tested |
| A project consuming its own published id | Nonsense, and cheap to rule out |

The fifth rule is the point of the whole piece: adding a cross-project
`<script>` tag without declaring it turns CI red with a message naming the
file and the fix.

`shared/` is excluded from the undeclared-coupling rule — it is shared
infrastructure every page uses, not a project dependency.

### 3. `forge/forge/checks.py` — the Forge learns the rule

Today: `ZONE_CHECKS` maps a safe zone to its own commands. After: the same
map, plus the checks of every project that declares it consumes something
the zone publishes.

So `commands_for("music/")` returns music's own test **and**
`node film/tests/film-logic.test.js`, because `film` consumes
`music/composer` and `music/composer` is published from `music/`.

This needs a second map — a project's check commands whether or not it is a
safe zone, since `film/` is not one and never will be while the Forge cannot
verify it. `ZONE_CHECKS` becomes derived from it rather than a separate list,
so the two cannot drift.

**Failure behaviour, deliberately harsh.** If `shared/exchange.json` is
missing, unreadable or malformed, the Forge does **not** fall back to
own-zone checks. Falling back would mean running under weaker verification
while still reporting `checks: green`, which is defect #1 wearing a disguise.

*Where* it fails matters. The check is made in **DECIDE**, not VERIFY: a
candidate whose zone's check commands cannot be determined is skipped with
`config_error`, the existing per-candidate skip reason in `decide.py`. Doing
it at verify time would burn the night's Crew run before discovering the
problem; doing it at decide time costs nothing and reads correctly in the
ledger, which already records `skipped: {config_error: N}`.

If that leaves nothing above the score floor the night records `no_task`, as
any quiet night does. The ledger names the reason either way. A quiet night
is already a correct outcome in this design; a night that overstates what it
verified is not.

## Testing

- `scripts/check-exchange.mjs` gets its own Node test with fixture
  manifests: one valid, and one per failure rule, each asserting the checker
  fails **for that reason** rather than merely failing.
- The undeclared-coupling rule is tested against a fixture page carrying a
  real cross-project `<script>` tag, since that rule is the one with teeth.
- `forge/tests/test_checks.py` covers the widened `commands_for`: a zone with
  no consumers is unchanged; `music/` gains film's test; a missing or
  malformed `exchange.json` produces `config_error` and not a silent
  narrowing.
- Every new test is mutation-checked — the guard is removed, the test must
  fail — per the practice that caught fifteen vacuous tests while building
  the Forge.

## Out of scope

Artifact storage, runtime message passing, versioning of published surfaces
(`music/composer@2`), and dependency declarations for Python or C++ code.
Each can be added when something needs it; none is needed by a one-edge
graph.

Removing the `music-ci.yml` path filter is *not* in scope either. Site CI
already runs the contract test unfiltered, so widening the filter would add a
second run of the same test and no safety.

## The growth path

The movie-maker spec
([`2026-09-10-score-to-picture-design.md`](2026-09-10-score-to-picture-design.md))
already plans three more edges: a renderer upgrade, MADLIBS becoming the
story brain, and Maz Engine rendering the reel. Each is one entry in
`consumes` and one contract test. The checker covers it with no change, and
the Forge's verification widens on its own.

That is the test of whether this design is right: the second edge should cost
one line, not a redesign.

## Risks

**The declaration could rot.** Mitigated by rule five: reality is checked
against the declaration on every pull request, in both directions.

**The Forge could get slower.** Each consumer's test is added to the zone's
check run. With one edge this is one extra Node test of under a second. If
the graph grows to where this matters, the answer is a per-run time budget,
not weaker checks.

**`config_error` on a malformed file could silence the loop indefinitely.**
Accepted: the ledger records the reason every night, and a loop that does
nothing is the failure mode this whole design prefers.
