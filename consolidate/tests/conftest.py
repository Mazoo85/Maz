"""Helpers for the tests that drive real git repositories."""

import json
import subprocess

import pytest

from consolidate import gitops

ENV = {
    "GIT_AUTHOR_NAME": "Test",
    "GIT_AUTHOR_EMAIL": "test@example.com",
    "GIT_COMMITTER_NAME": "Test",
    "GIT_COMMITTER_EMAIL": "test@example.com",
}

needs_git = pytest.mark.skipif(
    not (gitops.git_available() and gitops.subtree_available()),
    reason="needs git with the subtree command",
)

# Long enough to clear the overlap noise floor.
PADDING = "content " * 20


def git(repo, *args):
    proc = subprocess.run(
        ["git", *args], cwd=str(repo), capture_output=True, text=True, env={"PATH": "/usr/bin:/bin", **ENV}
    )
    assert proc.returncode == 0, f"git {' '.join(args)} failed: {proc.stderr}"
    return proc.stdout.strip()


def make_repo(path, files, *, branch="main", message="first commit"):
    """Create a real git repo at ``path`` containing ``files``."""
    path.mkdir(parents=True, exist_ok=True)
    git(path, "init", "-q", "-b", branch)
    return commit(path, files, message)


def commit(path, files, message):
    for name, text in files.items():
        target = path / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text)
    git(path, "add", "-A")
    git(path, "commit", "-qm", message)
    return git(path, "rev-parse", "HEAD")


def repos_file(tmp_path, repos):
    """Write a discovery file pointing at local repos, as `--from-json` reads."""
    path = tmp_path / "repos.json"
    path.write_text(json.dumps({"repos": repos}, indent=2))
    return path
