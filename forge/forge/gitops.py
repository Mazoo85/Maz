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


def is_clean(root: Path | None = None, runner=None) -> bool:
    """True when the working tree has no staged, modified, or untracked-but-
    not-ignored changes against HEAD.

    ``git status --porcelain`` prints one line per such path (staged,
    modified, or untracked and not gitignored) and nothing at all when the
    tree is clean. Used by ``do()`` to refuse to start a run rather than
    begin a branch, and therefore an attribution trail, on top of someone
    else's uncommitted work. A failed status call (bad cwd, no repo) reads
    as "not clean" — fail closed, exactly like a falsy ``head_sha``.
    """
    code, out, _ = _runner_for(root, runner)(["status", "--porcelain"])
    return code == 0 and out.strip() == ""


def changed_files(root: Path | None = None, base: str = "HEAD", runner=None) -> tuple[str, ...]:
    """Paths touched since ``base``: tracked modifications plus untracked,
    non-ignored new files.

    ``git diff --name-only`` alone only ever sees tracked history — a file
    Crew created and never ``git add``ed is invisible to it, and that gap is
    exactly what could let an agent's own output (a new file dropped under a
    no-touch path, say) slip past the leash's file-count, no-touch, and
    safe-zone checks unseen. This function is where every caller — today
    just the leash re-check in ``do()`` — gets protected against that gap at
    once, rather than each caller having to remember to ask for it
    separately. ``git ls-files --others --exclude-standard`` supplies the
    missing half; ``--exclude-standard`` is git's own gitignore-respecting
    flag, so a build artifact the project already ignores is correctly
    left out rather than flagged as a violation.

    Not "committed or not": a change that is committed *and reverted* back
    to ``base`` on the branch does not appear here, because both queries
    compare live state (the diff against ``base``, the untracked listing
    against the index) rather than commit history.
    """
    run = _runner_for(root, runner)
    code, out, _ = run(["diff", "--name-only", base])
    tracked = tuple(p.strip() for p in out.splitlines() if p.strip()) if code == 0 else ()

    code, out, _ = run(["ls-files", "--others", "--exclude-standard"])
    untracked = tuple(p.strip() for p in out.splitlines() if p.strip()) if code == 0 else ()

    # Union, not concatenation: keep the diff's own order first, then any
    # untracked path not already present, so the result is deterministic
    # and never lists the same path twice.
    seen = set(tracked)
    return tracked + tuple(p for p in untracked if p not in seen)


def commit_all(message: str, root: Path | None = None, runner=None) -> bool:
    run = _runner_for(root, runner)
    if run(["add", "-A"])[0] != 0:
        return False
    return run(["commit", "-m", message])[0] == 0


def push_branch(name: str, root: Path | None = None, runner=None) -> bool:
    code, _, _ = _runner_for(root, runner)(["push", "-u", "origin", name])
    return code == 0
