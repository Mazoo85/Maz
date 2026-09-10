"""The appetite: unchecked boxes in docs/ROADMAP.md.

The roadmap is already a machine-readable backlog — 14 phases of `- [ ]` lines
with no nesting. Items marked `[~]` (in progress) count as candidates too, and
the phase containing them is taken to be the milestone currently in flight.

Roadmap candidates carry no `paths`, which means DECIDE will not run them until
a human widens the safe zones or the item is restated with files. That is the
intended conservative default: the roadmap describes work, not locations.
"""

from __future__ import annotations

import re
from pathlib import Path

from ..models import Candidate

ROADMAP_PATH = "docs/ROADMAP.md"

_PHASE_RE = re.compile(r"^##\s+Phase\s+(\d+)\b")
_ITEM_RE = re.compile(r"^-\s+\[([ x~])\]\s+(.+?)\s*$")


def _clean(text: str) -> str:
    """Strip markdown emphasis and code ticks so the task reads as plain English."""
    text = re.sub(r"\*\*(.+?)\*\*", r"\1", text)
    text = re.sub(r"`(.+?)`", r"\1", text)
    return text.strip()


def parse(text: str) -> list[Candidate]:
    """Turn roadmap markdown into candidates. Never raises."""
    phase: str | None = None
    # phase -> list of (state, task)
    per_phase: dict[str, list[tuple[str, str]]] = {}
    order: list[str] = []

    for line in text.splitlines():
        m = _PHASE_RE.match(line)
        if m:
            phase = m.group(1)
            if phase not in per_phase:
                per_phase[phase] = []
                order.append(phase)
            continue
        if phase is None:
            continue
        m = _ITEM_RE.match(line)
        if not m:
            continue
        state, task = m.group(1), _clean(m.group(2))
        if not task:
            continue
        per_phase[phase].append((state, task))

    # A phase carrying an in-progress marker is the milestone in flight.
    current = {p for p, items in per_phase.items() if any(s == "~" for s, _ in items)}

    out: list[Candidate] = []
    for phase in order:
        for state, task in per_phase[phase]:
            if state == "x":
                continue
            out.append(
                Candidate(
                    task=task,
                    source=f"roadmap:phase-{phase}",
                    kind="roadmap",
                    current_milestone=phase in current,
                    detail="in progress" if state == "~" else "",
                )
            )
    return out


def collect(root: Path) -> list[Candidate]:
    """Read docs/ROADMAP.md under ``root``. Missing or unreadable is not an error.

    ``UnicodeDecodeError`` is caught alongside ``OSError``: it is a
    ``ValueError`` subclass, not an ``OSError``, so a roadmap file containing
    invalid UTF-8 would otherwise slip past an ``except OSError`` and break
    the "never raises" contract every collector in this package promises.
    """
    p = root / ROADMAP_PATH
    try:
        return parse(p.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError):
        return []
