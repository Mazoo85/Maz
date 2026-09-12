"""The vocabulary shared by every step of the loop.

A `Candidate` is one thing the Forge could do tonight, whatever signal proposed
it. A `Scored` is a candidate after DECIDE has weighed it. Keeping both frozen
and free of behaviour means signals, scoring and the ledger can be reasoned
about — and tested — independently.
"""

from __future__ import annotations

import hashlib
from dataclasses import dataclass


@dataclass(frozen=True)
class Candidate:
    """One thing the Forge could do tonight."""

    # The words that will be handed to Crew, verbatim.
    task: str
    # Where the idea came from: "roadmap:phase-0", "ci:music-ci", "todo:js/game.js:412",
    # "inventory:app:headless:app:area2d".
    source: str
    # One of: "ci", "roadmap", "todo", "memory", "inventory".
    kind: str
    # Files the work is expected to touch. Empty means "unknown", which zone
    # checking treats as unsafe — the Forge does not work blind.
    paths: tuple[str, ...] = ()
    # True when the roadmap item belongs to the milestone currently in progress.
    current_milestone: bool = False
    # True when a test file sits alongside the paths above.
    tests_nearby: bool = False
    # Free text for the ledger and for the Crew prompt.
    detail: str = ""

    def key(self) -> str:
        """Stable identity for strike counting, independent of detail text.

        Two candidates naming the same task from the same source are the same
        piece of work even if the surrounding prose changed between nights.
        """
        raw = f"{self.kind}|{self.source}|{self.task}".encode("utf-8")
        return hashlib.sha1(raw).hexdigest()[:12]


@dataclass(frozen=True)
class Scored:
    """A candidate after DECIDE has weighed it."""

    candidate: Candidate
    value: float
    confidence: float
    risk: float
    score: float
    zone: str
