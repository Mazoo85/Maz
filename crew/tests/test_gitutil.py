"""git helpers behind the opt-in --commit step, exercised against a temp repo."""

import subprocess

import pytest

from crew import gitutil


def _git(root, *args):
    return subprocess.run(["git", *args], cwd=str(root), capture_output=True, text=True)


@pytest.fixture
def repo(tmp_path):
    _git(tmp_path, "init")
    _git(tmp_path, "config", "user.email", "crew@example.com")
    _git(tmp_path, "config", "user.name", "Crew Test")
    return tmp_path


def test_is_git_repo(repo, tmp_path):
    assert gitutil.is_git_repo(repo) is True


def test_not_a_git_repo(tmp_path):
    plain = tmp_path / "plain"
    plain.mkdir()
    assert gitutil.is_git_repo(plain) is False


def test_has_changes_detects_untracked(repo):
    assert gitutil.has_changes(repo) is False
    (repo / "new.txt").write_text("hi")
    assert gitutil.has_changes(repo) is True


def test_commit_all_commits_and_clears(repo):
    (repo / "a.txt").write_text("content")
    ok, sha = gitutil.commit_all("crew: add a.txt", repo)
    assert ok is True
    assert sha  # a short sha string
    assert gitutil.has_changes(repo) is False
    # The commit really landed with our message.
    log = _git(repo, "log", "-1", "--pretty=%s").stdout.strip()
    assert log == "crew: add a.txt"


def test_commit_all_with_nothing_to_commit_fails_cleanly(repo):
    ok, msg = gitutil.commit_all("nothing here", repo)
    assert ok is False
    assert msg  # a non-empty explanation, not a crash
