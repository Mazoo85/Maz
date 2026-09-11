"""What "green" means, per zone.

Structural, not tunable: which command tests a directory is a fact about the
repo, so it lives in code beside the other path maps (see zones.RISK_PATHS and
signals.ci.WORKFLOW_SUBJECTS) rather than in forge.json, which is reserved for
the numbers a human tunes.

A zone mapped to a project with no commands of its own passes trivially on
its *own* checks, but every zone still runs the exchange gate (see
`EXCHANGE_CHECK_CMD` below), so no zone in `safe_zones` is ever left with
*nothing at all* running against it. That gate proves the declaration, not
the zone's own code: it never opens a file inside `madlibs/` or `shooter/`
itself, so a change confined to one of those today runs only the exchange
check, and its own code goes unexercised unless a declared consumer's
checks happen to pick it up — see docs/FORGE.md's "A safe zone only means
something if a check backs it up" for the honest version of this.

A zone with no entry in `ZONE_PROJECT` at all is a different thing entirely,
and must not read the same way. `docs/` genuinely has no project — that is
a fact about the repo, stated by mapping it to `None` — but a zone nobody
has ever told this module about is a configuration gap, not an answer.
Collapsing the two (as an earlier version of this module did, via a plain
`.get(zone)` returning `None` either way) meant a zone added to
`safe_zones` and forgotten here — or a bare typo in `forge.json` like
`"music"` for `"music/"` — silently verified nothing while still recording
`checks: green`. `commands_for` and `all_commands` now raise
`UnmappedZoneError` for that case instead: see their docstrings.

`tests/` has no entry, and that omission is deliberate, not an oversight: in
this repo that directory is C++ (CMakeLists.txt, unit_*.cpp), built and run
only through `cmake`/`ctest` against the Vulkan SDK (see
`.github/workflows/ci.yml`) — a build this sandbox cannot run and no command
this module may honestly invent. An earlier version of this map pointed
`tests/` at `python -m pytest -q forge/tests`, which runs the Forge's own
Python suite and verifies nothing whatsoever about a C++ change — a change
under `tests/` would have been "verified" by a command that never looks at
it. `config.ForgeConfig.safe_zones` matches this by leaving `tests/` out of
its default, so a live run never reaches `commands_for("tests/")` at all —
if it ever did, it would now raise `UnmappedZoneError` rather than return
`()`, same as any other zone nobody has classified.

**A zone's own tests are not the whole story.** SCRIPT FORGE loads five of
SONG FORGE's files, so a change in `music/` can break `film/` while music's
own tests stay green. `all_commands` reads `shared/exchange.json` and adds
the checks of every project declared to consume this one. `commands_for`
remains the pure own-zone answer, for callers that genuinely want only that.

**A declared consumer with no checks of its own is the same gap as an
unmapped zone, one map over.** `all_commands` looks each of a project's
consumers up in `PROJECT_CHECKS` and raises `UnmappedConsumerError` for one
with no entry there, rather than silently contributing zero commands for
it. This is the consumer-side mirror of `UnmappedZoneError` above: a zone
`ZONE_PROJECT` has never heard of is a configuration gap, not "nothing to
run", and so is a project named in `consumes` that `PROJECT_CHECKS` has
never heard of — `checks: green` must not claim to have verified a
downstream project it never ran a single command against. Give a new
consumer a `PROJECT_CHECKS` entry before declaring it in
`shared/exchange.json`, or a night touching what it consumes now fails
closed instead of quietly verifying less than the ledger claims.
"""

from __future__ import annotations

from pathlib import Path

from .exchange import load

# What verifies each project, whether or not it is a safe zone. `film` is not
# a safe zone and is not expected to become one — it is here because a change
# in `music/` must run film's tests, not because the Forge may edit film.
PROJECT_CHECKS: dict[str, tuple[tuple[str, ...], ...]] = {
    "music": (("node", "music/tests/music-logic.test.js"),),
    "film": (("node", "film/tests/film-logic.test.js"),),
    "scraper": (
        ("python", "-m", "pytest", "-q", "scraper/tests"),
        ("python", "-m", "compileall", "-q", "scraper/scraper"),
    ),
    "crew": (("python", "-m", "pytest", "-q", "crew/tests"),),
}

# Which project a safe zone belongs to. `None` means "no project owns this",
# which is how docs get their correct empty command list. `madlibs/` and
# `shooter/` are real projects (both have ids in `shared/projects.js`) that
# simply have no tests of their own today — `None` here used to conflate
# "not a project" with "a project with nothing to run", and the second
# reading is wrong: it skipped `consumers_of` entirely (see `all_commands`
# below), so a declared consumer of madlibs or shooter would never have its
# checks run. Mapping them to their real project id costs nothing for their
# own checks (`PROJECT_CHECKS.get` already returns `()` for a project not in
# that map) and lets `all_commands` see their consumers once either is
# declared.
ZONE_PROJECT: dict[str, str | None] = {
    "music/": "music",
    "scraper/": "scraper",
    "crew/tests/": "crew",
    "docs/": None,
    "madlibs/": "madlibs",
    "shooter/": "shooter",
}

# The whole-repo gate this branch depends on: whether every cross-project
# `<script>` coupling is declared in `shared/exchange.json`, in both
# directions (nothing declared that isn't real, nothing real left
# undeclared). It cannot live as one project's *own* entry in
# `PROJECT_CHECKS` — nothing in `music/`'s own files proves the rest of the
# repo still agrees with the declaration. Its result never depends on which
# zone changed — a `docs/`-only night can add an undeclared `<script>` in a
# new page exactly as easily as a `music/`-only night can — so `all_commands`
# runs it for every zone unconditionally, `docs/` included, rather than only
# for zones that happen to own a project.
EXCHANGE_CHECK_CMD: tuple[str, ...] = ("node", "scripts/check-exchange.mjs")


class UnmappedZoneError(Exception):
    """`zone` has no entry in `ZONE_PROJECT` at all.

    Not the same thing as a zone that is deliberately project-less: `docs/`
    says so explicitly by mapping to `None`, and that still answers `()`
    from `commands_for`. This exception is for a zone `ZONE_PROJECT` has
    never heard of — added to `forge.json`'s `safe_zones` (by design, or by
    a bare typo like `"music"` for `"music/"`) without a matching entry
    here. Returning `()` for that case used to be indistinguishable from
    "nothing to check" and let `checks: green` mean "nobody wired this zone
    up to a project" — see the module docstring.
    """


class UnmappedConsumerError(Exception):
    """A project named in `shared/exchange.json`'s `consumes` has no entry
    in `PROJECT_CHECKS`.

    The same shape as `UnmappedZoneError`, one level over: a *zone* with no
    entry in `ZONE_PROJECT` is a configuration gap, not "nothing to check"
    — and so is a *consumer project*, named by a `consumes` entry, with no
    entry in `PROJECT_CHECKS`. `all_commands` used to answer that with
    `PROJECT_CHECKS.get(consumer, ())` — silently zero commands, exactly
    the `ZONE_PROJECT` mistake this module's docstring already describes,
    just one map over. `checks: green` would then claim to have verified a
    downstream project it never ran a single command against. This is
    distinct from a zone's *own* project having no `PROJECT_CHECKS` entry
    (`commands_for`'s `PROJECT_CHECKS.get(project, ())`, unchanged and
    correctly fail-open): a project with no tests of its own but real
    consumers is a legitimate shape (`madlibs/`, `shooter/` today), verified
    entirely through what consumes it. A *consumer* with no entry is not —
    there is nothing left downstream of it to verify it instead.
    """


def commands_for(zone: str) -> tuple[tuple[str, ...], ...]:
    """The commands that verify a zone itself.

    Raises `UnmappedZoneError` when `zone` has no entry in `ZONE_PROJECT` at
    all. A zone that is deliberately project-less (`docs/`, mapped to
    `None`) still answers `()` — only a zone nobody has classified fails
    closed.

    Deliberately pure and root-free otherwise: it answers only "what tests
    this directory", which is the question `ZONE_PROJECT` and
    `PROJECT_CHECKS` can answer without reading anything from disk.
    """
    if zone not in ZONE_PROJECT:
        raise UnmappedZoneError(zone)
    project = ZONE_PROJECT[zone]
    if project is None:
        return ()
    return PROJECT_CHECKS.get(project, ())


def all_commands(zone: str, root: Path) -> tuple[tuple[str, ...], ...]:
    """A zone's own checks, the exchange gate, plus every consumer's checks.

    Raises `ExchangeError` when `shared/exchange.json` cannot be read — for
    every zone, including one with nothing downstream. Without that file we
    cannot prove a zone has no consumers, so "no consumers" is not an answer
    we are entitled to give, and returning the own-zone list anyway would
    report `checks: green` having verified less than it claims. That is the
    precise defect this function exists to remove; reintroducing it as the
    error path would be worse than never having written it.

    Raises `UnmappedZoneError` (via `commands_for`) when `zone` has no entry
    in `ZONE_PROJECT` — see that exception's docstring. This runs *before*
    the exchange gate is appended, so an unmapped zone fails with a plain
    "nobody told me what this is", not a command list that happens to be
    just the exchange gate.

    Raises `UnmappedConsumerError` when a project this zone's project feeds
    (`exchange.consumers_of(project)`) has no entry in `PROJECT_CHECKS` —
    see that exception's docstring for why this is a configuration gap, not
    "nothing to check", the same way an unmapped zone is. This is checked
    *before* appending any of that consumer's commands, so a night never
    partially records a downstream project's checks as run.

    `load(root)` runs first and unconditionally — even for a zone whose
    project is `None` — so a broken declaration is never silently skipped
    just because this particular zone has nothing downstream to look up.

    `EXCHANGE_CHECK_CMD` is appended for every zone unconditionally,
    `docs/` included — see its own docstring for why gating it on "has a
    project" would be the wrong condition.
    """
    exchange = load(root)
    out = list(commands_for(zone))
    if EXCHANGE_CHECK_CMD not in out:
        out.append(EXCHANGE_CHECK_CMD)
    project = ZONE_PROJECT[zone]
    if project is None:
        return tuple(out)
    for consumer in exchange.consumers_of(project):
        if consumer not in PROJECT_CHECKS:
            raise UnmappedConsumerError(consumer)
        for cmd in PROJECT_CHECKS[consumer]:
            if cmd not in out:
                out.append(cmd)
    return tuple(out)
