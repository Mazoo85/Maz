"""Minimal git helpers for the opt-in `--commit` step.

Thin wrappers over the git CLI via subprocess — no third-party dependency. The
crew only ever *commits* (never pushes); pushing stays a deliberate human action.
"""

from __future__ import annotations

import subprocess
from pathlib import Path


def _git(args: list[str], root: Path) -> subprocess.CompletedProcess:
    return subprocess.run(
        ["git", *args],
        cwd=str(root),
        capture_output=True,
        text=True,
    )


def is_git_repo(root: Path | None = None) -> bool:
    root = root or Path.cwd()
    r = _git(["rev-parse", "--is-inside-work-tree"], root)
    return r.returncode == 0 and r.stdout.strip() == "true"


def has_changes(root: Path | None = None) -> bool:
    """True if there is anything to commit (staged, unstaged, or untracked)."""
    root = root or Path.cwd()
    r = _git(["status", "--porcelain"], root)
    return bool(r.stdout.strip())


def commit_all(message: str, root: Path | None = None) -> tuple[bool, str]:
    """Stage everything and commit. Returns (success, short_sha or error text)."""
    root = root or Path.cwd()
    add = _git(["add", "-A"], root)
    if add.returncode != 0:
        return (False, add.stderr.strip() or "git add failed")
    commit = _git(["commit", "-m", message], root)
    if commit.returncode != 0:
        return (False, commit.stderr.strip() or commit.stdout.strip() or "git commit failed")
    sha = _git(["rev-parse", "--short", "HEAD"], root)
    return (True, sha.stdout.strip())
