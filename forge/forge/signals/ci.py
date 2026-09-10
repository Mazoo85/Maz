"""The immune system's alarm: workflows that are currently red on main.

A red check is the highest-value signal the Forge has — something that used to
work has stopped. Only the most recent run per workflow counts; older failures
that a later run turned green are history, not work.

Each workflow is mapped to the paths it actually tests, so a failing job routes
the Forge to the code under test rather than to the workflow file, which is a
no-touch path and could never be fixed anyway.
"""

from __future__ import annotations

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


def from_runs(runs: list[dict]) -> list[Candidate]:
    """Latest run per workflow; a red one becomes a candidate. Never raises."""
    latest: dict[str, dict] = {}
    for run in runs:
        if run.get("head_branch") not in MAIN_BRANCHES:
            continue
        path = run.get("path") or ""
        seen = latest.get(path)
        if seen is None or str(run.get("created_at", "")) > str(seen.get("created_at", "")):
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
    runs = payload.get("workflow_runs") or []
    if not isinstance(runs, list):
        return []
    return from_runs(runs)
