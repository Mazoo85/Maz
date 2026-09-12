"""VERIFY — the tests are the judge, and there is no arguing with them.

Green: push the branch and open a DRAFT pull request. Never a merge, never a
push to main. Red: the caller abandons the branch and the failure goes into the
ledger, where tomorrow night's SENSE will read it.

The pull request body is written for a human skimming it over coffee: what was
picked, where the idea came from, the arithmetic that chose it, and every file
touched.
"""

from __future__ import annotations

import subprocess
from dataclasses import dataclass
from pathlib import Path

from .checks import UnmappedConsumerError, UnmappedZoneError, all_commands
from .config import ForgeConfig
from .do import CrewOutcome
from .exchange import ExchangeError
from .github import api, repo_slug
from .zones import zones_for_files

# Fallback only: a caller with a real ForgeConfig must pass its own
# `base_branch` explicitly (see orchestrate.py) rather than lean on this.
# This module imports `ForgeConfig` below only as a type — for
# `run_checks_for_files` to accept the caller's already-loaded config
# explicitly as a parameter, the same pattern `do.py` uses for its own
# `ForgeConfig` argument — never to reach into it for a default of its own.
# `BASE_BRANCH` stays a bare module constant, not `ForgeConfig.base_branch`
# read here: that would make every `open_draft_pr` call site's honesty about
# which base it targets an accident of import order rather than a fact
# visible in its own call. Kept around only so a caller that genuinely
# doesn't have a config (an ad-hoc script, a REPL, most of this module's own
# unit tests) still gets a sane default instead of a required argument.
BASE_BRANCH = "main"
CHECK_TIMEOUT_S = 900


@dataclass(frozen=True)
class CheckResult:
    ok: bool
    ran: tuple[str, ...]
    output: str


def _default_runner(cmd: tuple[str, ...], root: Path | None) -> tuple[int, str]:
    proc = subprocess.run(list(cmd), cwd=str(root or Path.cwd()),
                          capture_output=True, text=True, timeout=CHECK_TIMEOUT_S)
    return (proc.returncode, (proc.stdout or "") + (proc.stderr or ""))


def _run_commands(commands: tuple[tuple[str, ...], ...], root: Path | None, runner=None) -> CheckResult:
    """Run a fixed, already-decided command list, stopping at the first
    failure. Shared by `run_checks` (one zone) and `run_checks_for_files`
    (the union of zones a real change actually touched) so the two only
    differ in how `commands` was assembled, never in how it is executed.
    """
    run = runner or _default_runner
    ran: list[str] = []
    for cmd in commands:
        ran.append(" ".join(cmd))
        try:
            code, output = run(cmd, root)
        except Exception as exc:  # noqa: BLE001
            return CheckResult(False, tuple(ran), f"{ran[-1]} could not run: {exc}")
        if code != 0:
            return CheckResult(False, tuple(ran), output[-2000:])
    return CheckResult(True, tuple(ran), "")


def run_checks(zone: str, root: Path | None, runner=None) -> CheckResult:
    """Run every command for the zone, stopping at the first failure.

    "Every command" includes the checks of projects downstream of this zone,
    so an unreadable `shared/exchange.json` is a failed check rather than a
    shorter list of commands.

    This is the single-zone answer — the right one for a caller that already
    knows the one zone it means (most of this module's own tests). A real
    live run must not call this with DECIDE's chosen zone: see
    `run_checks_for_files` below for why, and orchestrate.py's module
    docstring for the incident this replaced.

    No production caller reaches this function today: `orchestrate.py`
    calls `run_checks_for_files` exclusively (see its docstring for why).
    This is kept for tests and any future caller that genuinely already
    knows the one zone it means — a test-only entry point in practice, not
    dead code, since deleting it would leave `_run_commands` and the
    single-zone behaviour it exercises unverified.
    """
    try:
        commands = all_commands(zone, root or Path.cwd())
    except UnmappedZoneError as exc:
        return CheckResult(
            False, (),
            f"cannot tell what this change could break: zone {exc} has no "
            f"entry in checks.ZONE_PROJECT",
        )
    except UnmappedConsumerError as exc:
        return CheckResult(
            False, (),
            f"cannot tell what this change could break: consumer project {exc} has no "
            f"entry in checks.PROJECT_CHECKS",
        )
    except ExchangeError as exc:
        return CheckResult(False, (), f"cannot tell what this change could break: {exc}")
    return _run_commands(commands, root, runner)


def run_checks_for_files(files: tuple[str, ...], config: ForgeConfig,
                         root: Path | None, runner=None) -> CheckResult:
    """The real VERIFY entry point: derive what to run from the files Crew
    actually changed, never from the zone DECIDE chose before Crew ran.

    DECIDE's chosen zone describes the *candidate's declared* paths, not
    what Crew actually touched, and `zones.zone_for` reports only the single
    *broadest* zone spanning a change — not a union. A candidate scoped to
    `docs/` whose agent also edits `music/js/composer.js` has `zone_for`
    report `docs/` (the broader of the two), and `run_checks("docs/", ...)`
    would run zero commands and record `checks: green` having verified
    nothing about the music edit at all — the exact defect this function
    exists to remove. `do.py` already refuses to report success for any
    change whose files do not each resolve to *some* safe zone (see
    `zones.zone_for`'s call inside `do.do`), so by the time this function is
    reached that guarantee should already hold for every file in `files` —
    but this function does not lean on that "by luck": it re-derives the
    zones itself via `zones.zones_for_files` and fails closed, exactly like
    an unreadable declaration below, if a file does not resolve to one.

    Runs the union of every touched zone's own checks plus each zone's
    downstream consumers, deduplicated in the order first encountered, so a
    check needed by two zones runs once. Fails closed — a broken
    `shared/exchange.json` reports the same way `run_checks` does, never a
    shorter list of commands.
    """
    zones = zones_for_files(files, config)
    if zones is None:
        return CheckResult(
            False, (),
            "cannot tell what this change could break: a changed file did not "
            "resolve to a known safe zone",
        )
    root = root or Path.cwd()
    commands: list[tuple[str, ...]] = []
    try:
        for zone in zones:
            for cmd in all_commands(zone, root):
                if cmd not in commands:
                    commands.append(cmd)
    except UnmappedZoneError as exc:
        return CheckResult(
            False, (),
            f"cannot tell what this change could break: zone {exc} has no "
            f"entry in checks.ZONE_PROJECT",
        )
    except UnmappedConsumerError as exc:
        return CheckResult(
            False, (),
            f"cannot tell what this change could break: consumer project {exc} has no "
            f"entry in checks.PROJECT_CHECKS",
        )
    except ExchangeError as exc:
        return CheckResult(False, (), f"cannot tell what this change could break: {exc}")
    return _run_commands(tuple(commands), root, runner)


def _pr_body(outcome: CrewOutcome, chosen: dict) -> str:
    c = chosen["candidate"]
    files = "\n".join(f"- `{p}`" for p in outcome.files) or "- (none recorded)"
    cost = f"${outcome.cost_usd:.2f}" if outcome.cost_usd is not None else "not measured"
    return (
        f"Opened by **the Forge** on its nightly run. This is a draft — nothing merges "
        f"without you.\n\n"
        f"**What it picked:** {c['task']}\n\n"
        f"**Where the idea came from:** `{c['source']}`\n\n"
        f"**Why it won tonight:** score **{chosen['score']}** "
        f"= value {chosen['value']} x confidence {chosen['confidence']} "
        f"/ (1 + risk {chosen['risk']}), in zone `{chosen['zone']}`.\n\n"
        f"**Files touched:**\n{files}\n\n"
        f"**Cost:** {cost} over {outcome.duration_min:.1f} minutes.\n\n"
        f"---\n_Generated by [Claude Code](https://claude.ai/code)_\n"
    )


def open_draft_pr(outcome: CrewOutcome, chosen: dict, root: Path | None,
                  slug: str | None = None, poster=None,
                  base_branch: str = BASE_BRANCH) -> int | None:
    """Open a draft PR for the branch. Returns the PR number, or None.

    `base_branch` is a bare string, not a `ForgeConfig`, deliberately: this
    module has no other reason to know about config.py's shape, and a
    string keeps the one real caller (`orchestrate.live_run`, which already
    has a loaded config in hand) honest about passing `config.base_branch`
    explicitly, rather than letting this function silently reach for its
    own default whenever a config happens to be available. The default
    here only serves callers with no config at all.
    """
    slug = slug if slug is not None else repo_slug(root)
    if not slug:
        return None
    post = poster or (lambda path, body: api(path, method="POST", body=body))
    task = chosen["candidate"]["task"]
    payload = {
        "title": f"forge: {task[:70]}",
        "head": outcome.branch,
        "base": base_branch,
        "draft": True,
        "body": _pr_body(outcome, chosen),
    }
    result = post(f"/repos/{slug}/pulls", payload)
    number = result.get("number") if isinstance(result, dict) else None
    return int(number) if number else None
