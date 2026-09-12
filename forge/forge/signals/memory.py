"""What the project already knows is unfinished.

codebase-memory stores plain-English observations about each component. Some of
them are quiet admissions that something is half-done — "still to add",
"not yet wired", "partial". Those are candidates nobody wrote on the roadmap.

Like roadmap candidates, these carry no paths: an observation names a state of
affairs, not a set of files.

One entity in that same file is not a statement about the project at all:
``learn.memory_note`` appends one observation per run under the entity named
``FORGE_ENTITY_NAME`` below — "Forge run 2026-09-11: pr_opened (PR #7) —
Resolve the TODO in js/game.js: ...". Every TODO candidate's task text is,
by construction, unfinished-work-shaped, so left unfiltered that entity
regenerates as a fresh memory candidate every single night forever: a
closed loop where the Forge's own log becomes tomorrow's input. Excluding
it by name — rather than by anything about the observation text — is what
keeps this exclusion narrow: it must not (and, matched on name alone,
cannot) swallow a genuine entity that happens to reuse the same wording.
``FORGE_ENTITY_NAME`` is imported by ``learn.py`` rather than redefined
there, so the writer and this reader can never drift onto two different
strings.
"""

from __future__ import annotations

import json
from pathlib import Path

from ..models import Candidate

MEMORY_PATH = ".claude/codebase-memory.json"

# The entity name learn.memory_note writes its per-run observations under.
# Named once, here, and imported by learn.py — never redefined there — so
# the writer and this reader cannot drift onto two different literals.
FORGE_ENTITY_NAME = "The Forge"

# Phrases that mark an observation as unfinished business.
UNFINISHED_MARKERS = (
    "still to",
    "still needs",
    "not yet",
    "to do",
    "todo",
    "partial",
    "in progress",
    "remaining",
    "missing",
    "incomplete",
)

MAX_TASK_CHARS = 200


def _is_unfinished(observation: str) -> bool:
    low = observation.lower()
    return any(marker in low for marker in UNFINISHED_MARKERS)


def parse(text: str) -> list[Candidate]:
    """Turn memory JSONL into candidates. Never raises."""
    out: list[Candidate] = []
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            record = json.loads(line)
        except (json.JSONDecodeError, ValueError):
            continue
        if not isinstance(record, dict) or record.get("type") != "entity":
            continue
        name = str(record.get("name") or "unknown")
        if name == FORGE_ENTITY_NAME:
            # This is a log of what the Forge did, not a statement about the
            # project — see the module docstring for why it must not become
            # tomorrow's candidate.
            continue
        observations = record.get("observations")
        if not isinstance(observations, list):
            continue
        for observation in observations:
            if not isinstance(observation, str) or not _is_unfinished(observation):
                continue
            out.append(
                Candidate(
                    task=observation.strip()[:MAX_TASK_CHARS],
                    source=f"memory:{name}",
                    kind="memory",
                    detail="from codebase-memory",
                )
            )
    return out


def collect(root: Path) -> list[Candidate]:
    """Read .claude/codebase-memory.json under ``root``. Missing is not an error.

    ``UnicodeDecodeError`` is caught alongside ``OSError``: it is a
    ``ValueError`` subclass, not an ``OSError``, so a memory file containing
    invalid UTF-8 would otherwise slip past an ``except OSError`` and break
    the "never raises" contract every collector in this package promises.
    """
    try:
        return parse((root / MEMORY_PATH).read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError):
        return []
