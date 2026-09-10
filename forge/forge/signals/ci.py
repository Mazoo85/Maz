"""The immune system's alarm: workflows that are currently red on main.

A red check is the highest-value signal the Forge has — something that used to
work has stopped. Only the most recent run per workflow counts; older failures
that a later run turned green are history, not work.

Each workflow is mapped to the paths it actually tests, so a failing job routes
the Forge to the code under test rather than to the workflow file, which is a
no-touch path and could never be fixed anyway.
"""

from __future__ import annotations

import re
from pathlib import Path

from ..github import api, repo_slug
from ..models import Candidate

# Which directory each workflow is really about.
WORKFLOW_SUBJECTS = {
    ".github/workflows/music-ci.yml": ("music/",),
    ".github/workflows/scraper-ci.yml": ("scraper/",),
    ".github/workflows/crew-ci.yml": ("crew/",),
    ".github/workflows/forge-ci.yml": (),  # forge/ is no-touch: never actionable
    ".github/workflows/ci.yml": ("engine/",),
    ".github/workflows/pages.yml": (),
}

MAIN_BRANCHES = ("main", "master")
RUNS_TO_SCAN = 40


def _slug_from_path(path: str) -> str:
    name = path.rsplit("/", 1)[-1]
    return name.rsplit(".", 1)[0]


_ISO_8601_PREFIX = re.compile(r"^\d{4}-\d{2}-\d{2}[T ]\d{2}:\d{2}:\d{2}")
"""Matches the shape of an ISO-8601 timestamp, not its calendar validity.
A digit-shaped but impossible timestamp like "2026-13-45T99:99:99" is treated
as valid and sorted by raw string value."""


def _created_at_sort_key(run: dict) -> tuple[bool, str]:
    """Sort key for "latest wins" that a malformed created_at can never win.

    created_at compares correctly as a plain string when it is a real
    ISO-8601 timestamp. But GitHub is not the only source of a run dict any
    more (from_runs is exercised directly by tests, and this module's own
    `fetch` injection point is documented as replaceable) — a missing,
    non-string, or non-ISO-8601-shaped value must never be able to
    masquerade as "latest". Comparing bare strings lets it: the string
    "None" beats every real timestamp in ASCII ("N" > "2"), so a red run
    could lose "latest" to a malformed record and vanish, silencing the
    exact alarm this module exists to raise. The leading bool sorts any
    valid timestamp ahead of any malformed one regardless of the second
    element's ASCII value; among malformed records it is a no-op tie.
    """
    created = run.get("created_at")
    if isinstance(created, str) and _ISO_8601_PREFIX.match(created):
        return (True, created)
    return (False, "")


def from_runs(runs: list[dict]) -> list[Candidate]:
    """Latest run per workflow; a red one becomes a candidate. Never raises."""
    latest: dict[str, dict] = {}
    for run in runs:
        # A malformed element that is not a dict must be skipped: the payload
        # being a dict says nothing about its contents, and this is the same
        # failure class one level deeper as the guard already in collect().
        if not isinstance(run, dict):
            continue
        if run.get("head_branch") not in MAIN_BRANCHES:
            continue
        path = run.get("path") or ""
        seen = latest.get(path)
        if seen is None or _created_at_sort_key(run) > _created_at_sort_key(seen):
            latest[path] = run

    out: list[Candidate] = []
    for path, run in sorted(latest.items()):
        if run.get("conclusion") != "failure":
            continue
        name = run.get("name") or _slug_from_path(path)
        out.append(
            Candidate(
                task=f"Fix the failing {name} workflow on main",
                source=f"ci:{_slug_from_path(path)}",
                kind="ci",
                paths=WORKFLOW_SUBJECTS.get(path, ()),
                detail=f"latest run: {run.get('html_url', 'unknown')}",
            )
        )
    return out


def collect(root: Path, fetch=None, slug: str | None = None) -> list[Candidate]:
    """Read recent workflow runs from GitHub. No token means no CI signals."""
    slug = slug or repo_slug(root)
    if not slug:
        return []
    getter = fetch or (lambda path: api(path))
    try:
        payload = getter(f"/repos/{slug}/actions/runs?per_page={RUNS_TO_SCAN}")
    except Exception:  # noqa: BLE001
        return []
    # `fetch` is an explicitly documented injection point, so a payload that
    # is not a dict (None, a list, ...) is reachable through this module's
    # own public interface, not just a hypothetical. Guard it rather than
    # assume `.get` exists.
    if not isinstance(payload, dict):
        return []
    runs = payload.get("workflow_runs") or []
    if not isinstance(runs, list):
        return []
    return from_runs(runs)
