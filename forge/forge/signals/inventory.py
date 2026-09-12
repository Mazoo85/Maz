"""The repository's own catalogue, read as work.

`tools/inventory/main.mjs` walks every artifact in the repo — apps, engine
headers, browser projects, Python tools, CI gates, docs — measures each against
a short list of mechanical completeness checks, and writes the gaps to
`docs/inventory.json` as a ranked queue. That file is regenerated on every push
and CI fails when it drifts, so it is always a true statement about the repo.

This collector turns it into candidates. Two shapes come out of the queue and
they are handled differently:

* An **itemised** task names one artifact and one file ("MADLIBS has no logic
  test"). It becomes one candidate, as written.

* A **grouped** task names a check that many artifacts fail ("38 apps have no
  headless mode"). That is not one night's work and offering it as such would
  give Crew an instruction it cannot finish. It is fanned out instead: one
  candidate per member, each with that member's own fix sentence and its own
  path, so the Forge picks off a single app on a single night.

Tasks the Forge could never act on are dropped here rather than scored and
rejected later: an opportunity with no paths (`zone_for` treats unknown scope as
unsafe, correctly — the Forge does not work blind) and any task whose scope is
wider than `max_files_touched`.

Like every collector, this one never raises. A missing or malformed
`docs/inventory.json` costs the night this signal, not the night.
"""

from __future__ import annotations

import json
from pathlib import Path

from ..models import Candidate

# Where the inventory writes its machine-readable queue, relative to the repo root.
QUEUE_PATH = "docs/inventory.json"

# The schema this collector understands. The inventory stamps every file with
# `schema`; a bump means the shape changed underneath us, and reading it anyway
# would produce confident nonsense. Better to contribute nothing that night.
SUPPORTED_SCHEMA = 1

# Cap the task text handed to Crew, matching the todo collector's cap.
MAX_TASK_CHARS = 300

# Most members to fan a single grouped task out over. The Forge picks one task a
# night, so a longer list only slows scoring; 12 is enough that the choice is
# real and the pulse stays readable.
MAX_FANOUT = 12


def _read_queue(root: Path, path: str = QUEUE_PATH) -> list[dict]:
    """The queue, or [] for anything wrong with the file. Never raises."""
    try:
        data = json.loads((root / path).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError, ValueError):
        return []
    if not isinstance(data, dict) or data.get("schema") != SUPPORTED_SCHEMA:
        return []
    queue = data.get("queue")
    return queue if isinstance(queue, list) else []


def _tests_nearby(root: Path, paths: tuple[str, ...]) -> bool:
    """True when something that looks like a test sits beside the work.

    Feeds `confidence_tests_nearby` in scoring: a change with a test next to it
    is one the Forge can verify it did not break, which is worth real weight.
    """
    for p in paths:
        here = root / p
        directory = here if here.is_dir() else here.parent
        if not directory.is_dir():
            continue
        if (directory / "tests").is_dir():
            return True
        try:
            siblings = list(directory.iterdir())
        except OSError:
            continue
        if any("test" in s.name.lower() for s in siblings):
            return True
    return False


def _candidate(root: Path, task: str, source: str, paths: tuple[str, ...], priority: int) -> Candidate:
    return Candidate(
        task=task[:MAX_TASK_CHARS],
        source=source,
        kind="inventory",
        paths=paths,
        tests_nearby=_tests_nearby(root, paths),
        # Scoring reads this to separate "something is broken or unprotected"
        # from "this is polish" — see decide._value.
        detail=f"p{priority}",
    )


def collect(root: Path, max_files: int = 12, path: str = QUEUE_PATH) -> list[Candidate]:
    """Turn the inventory's queue into candidates. Never raises."""
    out: list[Candidate] = []

    for entry in _read_queue(root, path):
        if not isinstance(entry, dict):
            continue
        priority = entry.get("priority")
        if priority not in (1, 2, 3):
            continue
        title = str(entry.get("title") or "").strip()
        detail = str(entry.get("detail") or "").strip()
        source = str(entry.get("source") or "").strip()
        if not title or not source:
            continue

        members = entry.get("members")
        if isinstance(members, list) and members:
            # Grouped: one candidate per member, each scoped to its own file.
            for member in members[:MAX_FANOUT]:
                if not isinstance(member, dict):
                    continue
                fix = str(member.get("fix") or "").strip()
                member_path = str(member.get("path") or "").strip()
                member_id = str(member.get("id") or "").strip()
                if not fix or not member_path or not member_id:
                    continue
                out.append(
                    _candidate(root, fix, f"{source}:{member_id}", (member_path,), priority)
                )
            continue

        # Itemised: take the task as the inventory wrote it.
        paths = tuple(p for p in (entry.get("paths") or []) if isinstance(p, str) and p)
        if not paths or len(paths) > max_files:
            # No scope, or more scope than a single run is allowed to touch.
            # Both are real proposals; neither is something the Forge can do
            # tonight, and a candidate it must reject is noise in the pulse.
            continue
        out.append(
            _candidate(root, f"{title}. {detail}" if detail else title, source, paths, priority)
        )

    return out
