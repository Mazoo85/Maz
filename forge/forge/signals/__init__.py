"""Signal collectors: pure functions that turn repo state into Candidates.

Every collector exposes ``collect(root: Path) -> list[Candidate]`` and never
raises — SENSE runs them all and one broken source must not stop the night.
"""

from __future__ import annotations
