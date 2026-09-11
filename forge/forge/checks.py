"""What "green" means, per zone.

Structural, not tunable: which command tests a directory is a fact about the
repo, so it lives in code beside the other path maps (see zones.RISK_PATHS and
signals.ci.WORKFLOW_SUBJECTS) rather than in forge.json, which is reserved for
the numbers a human tunes.

A zone with no commands passes trivially. That is correct for docs and content:
there is nothing to run, and CI on the pull request remains the real gate.

`tests/` has no entry, and that omission is deliberate, not an oversight: in
this repo that directory is C++ (CMakeLists.txt, unit_*.cpp), built and run
only through `cmake`/`ctest` against the Vulkan SDK (see
`.github/workflows/ci.yml`) — a build this sandbox cannot run and no command
this module may honestly invent. An earlier version of this map pointed
`tests/` at `python -m pytest -q forge/tests`, which runs the Forge's own
Python suite and verifies nothing whatsoever about a C++ change — a change
under `tests/` would have been "verified" by a command that never looks at
it. `config.ForgeConfig.safe_zones` matches this by leaving `tests/` out of
its default, so the two files cannot drift back into that mismatch: an
unknown zone here (`commands_for` returning `()`) is indistinguishable from
"nothing to check", same as `docs/`, but a zone that isn't in `safe_zones` to
begin with is never reachable to ask.

**A zone's own tests are not the whole story.** SCRIPT FORGE loads five of
SONG FORGE's files, so a change in `music/` can break `film/` while music's
own tests stay green. `all_commands` reads `shared/exchange.json` and adds
the checks of every project declared to consume this one. `commands_for`
remains the pure own-zone answer, for callers that genuinely want only that.
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
# which is how docs and content get their correct empty command list.
ZONE_PROJECT: dict[str, str | None] = {
    "music/": "music",
    "scraper/": "scraper",
    "crew/tests/": "crew",
    "docs/": None,
    "madlibs/": None,
    "shooter/": None,
}


def commands_for(zone: str) -> tuple[tuple[str, ...], ...]:
    """The commands that verify a zone itself. Unknown zones have none.

    Deliberately pure and root-free: it answers only "what tests this
    directory", which is the question `ZONE_PROJECT` and `PROJECT_CHECKS`
    can answer without reading anything from disk.
    """
    project = ZONE_PROJECT.get(zone)
    if project is None:
        return ()
    return PROJECT_CHECKS.get(project, ())


def all_commands(zone: str, root: Path) -> tuple[tuple[str, ...], ...]:
    """A zone's own checks plus those of every project that consumes it.

    Raises `ExchangeError` when `shared/exchange.json` cannot be read — for
    every zone, including one with nothing downstream. Without that file we
    cannot prove a zone has no consumers, so "no consumers" is not an answer
    we are entitled to give, and returning the own-zone list anyway would
    report `checks: green` having verified less than it claims. That is the
    precise defect this function exists to remove; reintroducing it as the
    error path would be worse than never having written it.

    `load(root)` runs first and unconditionally — even for a zone whose
    project is `None` — so a broken declaration is never silently skipped
    just because this particular zone has nothing downstream to look up.
    """
    exchange = load(root)
    out = list(commands_for(zone))
    project = ZONE_PROJECT.get(zone)
    if project is None:
        return tuple(out)
    for consumer in exchange.consumers_of(project):
        for cmd in PROJECT_CHECKS.get(consumer, ()):
            if cmd not in out:
                out.append(cmd)
    return tuple(out)
