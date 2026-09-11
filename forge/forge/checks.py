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
"""

from __future__ import annotations

ZONE_CHECKS: dict[str, tuple[tuple[str, ...], ...]] = {
    "music/": (("node", "music/tests/music-logic.test.js"),),
    "scraper/": (
        ("python", "-m", "pytest", "-q", "scraper/tests"),
        ("python", "-m", "compileall", "-q", "scraper/scraper"),
    ),
    "crew/tests/": (("python", "-m", "pytest", "-q", "crew/tests"),),
    "docs/": (),
    "madlibs/": (),
    "shooter/": (),
}


def commands_for(zone: str) -> tuple[tuple[str, ...], ...]:
    """The commands that verify a zone. Unknown zones have none."""
    return ZONE_CHECKS.get(zone, ())
