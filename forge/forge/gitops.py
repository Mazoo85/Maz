"""Thin git wrappers over subprocess — no dependency, no cleverness.

Every function takes an optional ``runner`` so the Forge's own tests never
shell out. The runner signature is ``runner(args) -> (returncode, stdout, stderr)``.
"""

from __future__ import annotations

import subprocess
from pathlib import Path


def run_git(args: list[str], root: Path | None = None) -> tuple[int, str, str]:
    r = subprocess.run(["git", *args], cwd=str(root or Path.cwd()),
                       capture_output=True, text=True)
    return (r.returncode, r.stdout, r.stderr)


def _runner_for(root: Path | None, runner):
    return runner or (lambda args: run_git(args, root))


def current_branch(root: Path | None, runner=None) -> str:
    code, out, _ = _runner_for(root, runner)(["rev-parse", "--abbrev-ref", "HEAD"])
    return out.strip() if code == 0 else ""


def create_branch(name: str, root: Path | None = None, runner=None) -> bool:
    code, _, _ = _runner_for(root, runner)(["checkout", "-b", name])
    return code == 0


def head_sha(root: Path | None = None, runner=None) -> str:
    """The current commit, read once right after the branch is cut.

    This is the base the leash re-check diffs against once Crew is done.
    Recording it up front — rather than assuming Crew made exactly one
    commit and diffing against ``HEAD~1`` afterwards — is what makes the
    re-check correct for zero, one, or any number of commits: it no longer
    depends on Crew's commit behaviour at all. Returns "" (never raises) on
    a bad rev-parse, exactly like ``current_branch`` above, so a caller can
    fail closed on a falsy result the same way.
    """
    code, out, _ = _runner_for(root, runner)(["rev-parse", "HEAD"])
    return out.strip() if code == 0 else ""


def checkout(name: str, root: Path | None = None, runner=None) -> bool:
    code, _, _ = _runner_for(root, runner)(["checkout", name])
    return code == 0


def delete_branch(name: str, root: Path | None = None, runner=None) -> bool:
    code, _, _ = _runner_for(root, runner)(["branch", "-D", name])
    return code == 0


def changed_files(root: Path | None = None, base: str = "HEAD", runner=None) -> tuple[str, ...]:
    """Paths changed against ``base``, including untracked-but-added files."""
    code, out, _ = _runner_for(root, runner)(["diff", "--name-only", base])
    if code != 0:
        return ()
    return tuple(p.strip() for p in out.splitlines() if p.strip())


def commit_all(message: str, root: Path | None = None, runner=None) -> bool:
    run = _runner_for(root, runner)
    if run(["add", "-A"])[0] != 0:
        return False
    return run(["commit", "-m", message])[0] == 0


def push_branch(name: str, root: Path | None = None, runner=None) -> bool:
    code, _, _ = _runner_for(root, runner)(["push", "-u", "origin", name])
    return code == 0
