"""What "green" means, per zone.

Structural, not tunable: which command tests a directory is a fact about the
repo, so it lives in code beside the other path maps (see zones.RISK_PATHS and
signals.ci.WORKFLOW_SUBJECTS) rather than in forge.json, which is reserved for
the numbers a human tunes.

A zone with no commands passes trivially. That is correct for docs and content:
there is nothing to run, and CI on the pull request remains the real gate.
"""

from __future__ import annotations

ZONE_CHECKS: dict[str, tuple[tuple[str, ...], ...]] = {
    "music/": (("node", "music/tests/music-logic.test.js"),),
    "scraper/": (
        ("python", "-m", "pytest", "-q", "scraper/tests"),
        ("python", "-m", "compileall", "-q", "scraper/scraper"),
    ),
    "crew/tests/": (("python", "-m", "pytest", "-q", "crew/tests"),),
    "tests/": (("python", "-m", "pytest", "-q", "forge/tests"),),
    "docs/": (),
    "madlibs/": (),
    "shooter/": (),
}


def commands_for(zone: str) -> tuple[tuple[str, ...], ...]:
    """The commands that verify a zone. Unknown zones have none."""
    return ZONE_CHECKS.get(zone, ())
