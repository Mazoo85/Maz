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
from .gitops import changed_files, create_branch, head_sha, is_clean
from .zones import is_no_touch, zone_for

SLUG_MAX = 40


@dataclass(frozen=True)
class CrewOutcome:
    ok: bool
    branch: str
    files: tuple[str, ...]
    # None means "not reported" — the bundled Crew runner never measures
    # cost, and that has to stay distinguishable from a run that genuinely
    # cost nothing. Never coerce a missing cost to 0.0; see _default_crew.
    cost_usd: float | None
    duration_min: float
    error: str = ""


def branch_name(task: str, when: datetime.date | None = None) -> str:
    """forge/YYYY-MM-DD-<slug> — dated so a night is always identifiable."""
    when = when or datetime.date.today()
    slug = re.sub(r"[^a-z0-9]+", "-", task.lower()).strip("-")[:SLUG_MAX].strip("-")
    return f"forge/{when:%Y-%m-%d}-{slug or 'task'}"


def _default_crew(task: str, root: Path, timeout_s: int) -> tuple[int, str, float | None]:
    """Run `crew do` non-interactively. Returns (returncode, output, cost_usd).

    Crew does not report cost on stdout in a machine-readable form, so cost
    is returned as ``None`` — "not reported" — rather than ``0.0``. ``0.0``
    would be indistinguishable from a run that genuinely cost nothing: every
    live run would silently record itself as free, the budget check would be
    structurally inert in production (it would still fire under test, where
    injected runners report a real number), and a future learned scorer or
    a human auditing spend would read zero variance in cost as "the Forge is
    free to run" instead of "cost is unmeasured". When Crew grows a `--json`
    output this is the one place to change.
    """
    proc = subprocess.run(
        ["crew", "do", task, "--yes", "--commit", "-m", f"forge: {task[:60]}"],
        cwd=str(root), capture_output=True, text=True, timeout=timeout_s,
    )
    return (proc.returncode, (proc.stdout or "") + (proc.stderr or ""), None)


def do(chosen: dict, root: Path, config: ForgeConfig, git=None, crew=None,
       when: datetime.date | None = None) -> CrewOutcome:
    """Branch, run Crew, then re-check the result against the leash."""
    task = chosen["candidate"]["task"]
    name = branch_name(task, when=when)
    started = datetime.datetime.now()

    # A dirty tree must stop the run before anything is created, not after:
    # `git diff --name-only <base>` compares the working tree to the
    # recorded base commit, so a file a human left modified *before* do()
    # ran would show up as though Crew had touched it. Subtracting the
    # pre-existing diff back out would be guesswork (a file dirty before
    # AND legitimately touched by Crew is not separable after the fact);
    # refusing to start is exact instead. It also keeps the branch itself
    # from carrying someone else's uncommitted work under Crew's name.
    try:
        clean = is_clean(root, runner=git)
    except Exception as exc:  # noqa: BLE001 — a crash is an outcome, not a traceback
        return CrewOutcome(False, name, (), None, _minutes(started),
                           f"could not check whether the working tree was clean: {exc}")
    if not clean:
        return CrewOutcome(False, name, (), None, _minutes(started),
                           "working tree is dirty; refusing to start so Crew is never "
                           "blamed for someone else's uncommitted changes")

    # create_branch reaches a real `subprocess.run(["git", ...])` in
    # production, with no guard of its own — git missing from PATH
    # (FileNotFoundError) or an unreadable cwd (PermissionError) must become
    # a CrewOutcome here, not an exception out of a function that promises
    # it never raises.
    try:
        created = create_branch(name, root, runner=git)
    except Exception as exc:  # noqa: BLE001 — a crash is an outcome, not a traceback
        return CrewOutcome(False, name, (), None, _minutes(started),
                           f"could not create the branch: {exc}")
    if not created:
        return CrewOutcome(False, name, (), None, _minutes(started), "could not create the branch")

    # Recorded now, before Crew runs at all, so the leash re-check below can
    # diff against a fixed commit rather than assume Crew made exactly one
    # commit (the `HEAD~1` bug this replaces: an early commit's violation
    # was invisible once later commits piled on top, and a zero-commit run
    # fabricated the pre-existing commit's files as its own). Guarded for
    # the same reason create_branch is above.
    try:
        base_sha = head_sha(root, runner=git)
    except Exception as exc:  # noqa: BLE001 — a crash is an outcome, not a traceback
        return CrewOutcome(False, name, (), None, _minutes(started),
                           f"could not read the branch's starting commit: {exc}")
    if not base_sha:
        return CrewOutcome(False, name, (), None, _minutes(started),
                           "could not read the branch's starting commit")

    runner = crew or _default_crew
    try:
        code, output, cost = runner(task, root, config.crew_timeout_min * 60)
    except subprocess.TimeoutExpired:
        return CrewOutcome(False, name, (), None, _minutes(started),
                           f"Crew timed out after {config.crew_timeout_min} minutes")
    except Exception as exc:  # noqa: BLE001 — a crash is an outcome, not a traceback
        return CrewOutcome(False, name, (), None, _minutes(started), f"Crew crashed: {exc}")

    duration = _minutes(started)
    if code != 0:
        return CrewOutcome(False, name, (), cost, duration, f"Crew failed: {output.strip()[-400:]}")

    # A runner (a test stub today, conceivably a real integration tomorrow)
    # can hand back anything as its third tuple element. `cost` is either
    # already None ("not reported", the documented and expected shape — see
    # _default_crew) or must be coerced explicitly: a right-arity tuple with
    # a wrong-typed cost is a malformed report, not a crash and not a free
    # run. Coercing it to None keeps it out of the `>` comparison below
    # (which would otherwise raise TypeError on, say, a string) while still
    # surfacing the problem — see cost_note below — instead of silently
    # swallowing it.
    cost, cost_note = _coerce_cost(cost)

    # An unknown cost (None) must not silently pass as though it were zero —
    # it passes the check (there is nothing to compare), but the "never
    # measured" fact survives into the record via cost_usd staying None.
    if cost is not None and cost > config.budget_usd:
        return CrewOutcome(False, name, (), cost, duration,
                           f"over budget: ${cost:.2f} against a ${config.budget_usd:.2f} cap")

    try:
        files = changed_files(root, base=base_sha, runner=git)
    except Exception as exc:  # noqa: BLE001 — a crash is an outcome, not a traceback
        return CrewOutcome(False, name, (), cost, duration,
                           _with_note(f"could not determine what Crew changed: {exc}", cost_note))
    if len(files) > config.max_files_touched:
        return CrewOutcome(False, name, files, cost, duration,
                           _with_note(f"touched {len(files)} files, over the "
                                      f"{config.max_files_touched} cap", cost_note))
    if is_no_touch(files, config):
        return CrewOutcome(False, name, files, cost, duration,
                           _with_note("touched a no-touch path", cost_note))
    if zone_for(files, config) is None:
        return CrewOutcome(False, name, files, cost, duration,
                           _with_note("touched a path outside the safe zones", cost_note))

    return CrewOutcome(True, name, files, cost, duration, cost_note)


def _coerce_cost(cost: object) -> tuple[float | None, str]:
    """Normalise a runner's reported cost, and say so when it can't be.

    ``None`` is the documented "not reported" sentinel (see _default_crew)
    and passes through unchanged and silently — that is expected, not an
    error. Anything else that isn't plainly numeric — a string, a list, the
    wrong shape entirely — is a malformed report: coerced to ``None`` (never
    measured) rather than crashing the `>` budget comparison or, worse,
    being silently accepted as some real number. ``bool`` is excluded even
    though it is technically an ``int`` subclass in Python — treating a
    stray ``True``/``False`` as $1.00/$0.00 would be a stranger failure than
    just calling it unmeasured.
    """
    if cost is None:
        return None, ""
    if isinstance(cost, bool) or not isinstance(cost, (int, float)):
        return None, f"cost unusable (crew reported {cost!r}, not a number); treated as unmeasured"
    return float(cost), ""


def _with_note(message: str, note: str) -> str:
    """Append a secondary note (today, only the cost-unusable note) to an
    outcome's primary message without losing either half.
    """
    return f"{message}; {note}" if note else message


def _minutes(started: datetime.datetime) -> float:
    return round((datetime.datetime.now() - started).total_seconds() / 60.0, 2)
