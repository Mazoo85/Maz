"""DO — one branch, one task, one hand-off to Maz Crew.

Crew is invoked as a subprocess, never imported: the Forge must keep working
when the Claude Agent SDK is absent, and the process boundary is also what makes
the wall-clock timeout enforceable.

Everything Crew produces is re-checked against the leash afterwards. Crew is a
capable agent, not a trusted one — the file count, the safe zones and the budget
are verified against the actual diff before anything is offered as a PR.
"""

from __future__ import annotations

import datetime
import re
import subprocess
from dataclasses import dataclass
from pathlib import Path

from .config import ForgeConfig
from .gitops import changed_files, create_branch
from .zones import is_no_touch, zone_for

SLUG_MAX = 40


@dataclass(frozen=True)
class CrewOutcome:
    ok: bool
    branch: str
    files: tuple[str, ...]
    cost_usd: float
    duration_min: float
    error: str = ""


def branch_name(task: str, when: datetime.date | None = None) -> str:
    """forge/YYYY-MM-DD-<slug> — dated so a night is always identifiable."""
    when = when or datetime.date.today()
    slug = re.sub(r"[^a-z0-9]+", "-", task.lower()).strip("-")[:SLUG_MAX].strip("-")
    return f"forge/{when:%Y-%m-%d}-{slug or 'task'}"


def _default_crew(task: str, root: Path, timeout_s: int) -> tuple[int, str, float]:
    """Run `crew do` non-interactively. Returns (returncode, output, cost_usd).

    Crew does not report cost on stdout in a machine-readable form, so cost is
    returned as 0.0 here and the budget check relies on the file-count and
    zone limits. When Crew grows a `--json` output this is the one place to
    change.
    """
    proc = subprocess.run(
        ["crew", "do", task, "--yes", "--commit", "-m", f"forge: {task[:60]}"],
        cwd=str(root), capture_output=True, text=True, timeout=timeout_s,
    )
    return (proc.returncode, (proc.stdout or "") + (proc.stderr or ""), 0.0)


def do(chosen: dict, root: Path, config: ForgeConfig, git=None, crew=None,
       when: datetime.date | None = None) -> CrewOutcome:
    """Branch, run Crew, then re-check the result against the leash."""
    task = chosen["candidate"]["task"]
    name = branch_name(task, when=when)
    started = datetime.datetime.now()

    if not create_branch(name, root, runner=git):
        return CrewOutcome(False, name, (), 0.0, 0.0, "could not create the branch")

    runner = crew or _default_crew
    try:
        code, output, cost = runner(task, root, config.crew_timeout_min * 60)
    except subprocess.TimeoutExpired:
        return CrewOutcome(False, name, (), 0.0, _minutes(started),
                           f"Crew timed out after {config.crew_timeout_min} minutes")
    except Exception as exc:  # noqa: BLE001 — a crash is an outcome, not a traceback
        return CrewOutcome(False, name, (), 0.0, _minutes(started), f"Crew crashed: {exc}")

    duration = _minutes(started)
    if code != 0:
        return CrewOutcome(False, name, (), cost, duration, f"Crew failed: {output.strip()[-400:]}")

    if cost > config.budget_usd:
        return CrewOutcome(False, name, (), cost, duration,
                           f"over budget: ${cost:.2f} against a ${config.budget_usd:.2f} cap")

    files = changed_files(root, base="HEAD~1", runner=git)
    if len(files) > config.max_files_touched:
        return CrewOutcome(False, name, files, cost, duration,
                           f"touched {len(files)} files, over the {config.max_files_touched} cap")
    if is_no_touch(files, config):
        return CrewOutcome(False, name, files, cost, duration,
                           "touched a no-touch path")
    if zone_for(files, config) is None:
        return CrewOutcome(False, name, files, cost, duration,
                           "touched a path outside the safe zones")

    return CrewOutcome(True, name, files, cost, duration, "")


def _minutes(started: datetime.datetime) -> float:
    return round((datetime.datetime.now() - started).total_seconds() / 60.0, 2)
