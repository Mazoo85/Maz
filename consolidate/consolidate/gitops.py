"""Every call to git lives here, so the rest of the package stays pure.

The important one is :func:`subtree_add`. Copying files between repos would
throw away the history; ``git subtree`` grafts the source repo's *whole*
history under a subdirectory, so the consolidated repo keeps every commit,
author and date. That is what "keeping all of their features" has to mean if
it is going to mean anything.
"""

from __future__ import annotations

import shutil
import subprocess
from pathlib import Path

TIMEOUT_S = 900


def run_git(args: list[str], cwd: str | Path | None = None, timeout: int = TIMEOUT_S) -> tuple[int, str, str]:
    """Run one git command. Returns ``(exit_code, stdout, stderr)``, never raises."""
    try:
        proc = subprocess.run(
            ["git", *args],
            cwd=str(cwd) if cwd else None,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
    except FileNotFoundError:
        return 127, "", "git is not installed, or not on PATH"
    except subprocess.TimeoutExpired:
        return 124, "", f"git {' '.join(args)} took longer than {timeout}s"
    return proc.returncode, proc.stdout.strip(), proc.stderr.strip()


def git_available() -> bool:
    return shutil.which("git") is not None


def subtree_available() -> bool:
    """``git subtree`` ships with git but some minimal installs drop it."""
    code, out, err = run_git(["subtree", "--help"])
    return code == 0 or "prefix" in (out + err)


def init_repo(path: str | Path, *, branch: str = "main") -> tuple[bool, str]:
    """Create an empty repo at ``path`` with ``branch`` checked out."""
    path = Path(path)
    path.mkdir(parents=True, exist_ok=True)
    code, _, err = run_git(["init", "-b", branch], cwd=path)
    if code != 0:
        return False, err
    ensure_identity(path)
    return True, ""


def ensure_identity(repo: str | Path) -> None:
    """Give the repo a committer if the machine has no global one configured.

    Without this, the very first merge fails on a fresh machine with git's
    "please tell me who you are" error, which is a confusing way to lose a
    twenty-minute clone.
    """
    for key, fallback in (("user.name", "maz-consolidate"), ("user.email", "consolidate@localhost")):
        code, out, _ = run_git(["config", "--get", key], cwd=repo)
        if code != 0 or not out:
            run_git(["config", key, fallback], cwd=repo)


def is_repo(path: str | Path) -> bool:
    code, out, _ = run_git(["rev-parse", "--is-inside-work-tree"], cwd=path)
    return code == 0 and out == "true"


def is_clean(repo: str | Path) -> bool:
    code, out, _ = run_git(["status", "--porcelain"], cwd=repo)
    return code == 0 and out == ""


def has_commits(repo: str | Path) -> bool:
    code, _, _ = run_git(["rev-parse", "--verify", "HEAD"], cwd=repo)
    return code == 0


def commit_all(repo: str | Path, message: str) -> tuple[bool, str]:
    """Stage everything and commit. Succeeds quietly when there is nothing to do."""
    code, _, err = run_git(["add", "-A"], cwd=repo)
    if code != 0:
        return False, err
    code, out, _ = run_git(["status", "--porcelain"], cwd=repo)
    if code == 0 and not out:
        return True, "nothing to commit"
    code, _, err = run_git(["commit", "-m", message], cwd=repo)
    return code == 0, err


def remote_branches(url: str) -> list[str]:
    """Branch names on a remote, without cloning it. Empty means an empty repo."""
    code, out, _ = run_git(["ls-remote", "--heads", url], timeout=120)
    if code != 0 or not out:
        return []
    names = []
    for line in out.splitlines():
        _, _, ref = line.partition("\t")
        if ref.startswith("refs/heads/"):
            names.append(ref[len("refs/heads/") :])
    return names


def default_branch(url: str) -> str | None:
    """The remote's HEAD branch, asked of the remote rather than guessed."""
    code, out, _ = run_git(["ls-remote", "--symref", url, "HEAD"], timeout=120)
    if code != 0:
        return None
    for line in out.splitlines():
        if line.startswith("ref: refs/heads/"):
            return line[len("ref: refs/heads/") :].split()[0].strip()
    return None


def add_remote(repo: str | Path, name: str, url: str) -> tuple[bool, str]:
    code, _, _ = run_git(["remote", "get-url", name], cwd=repo)
    if code == 0:
        code, _, err = run_git(["remote", "set-url", name, url], cwd=repo)
    else:
        code, _, err = run_git(["remote", "add", name, url], cwd=repo)
    return code == 0, err


def remote_url(repo: str | Path, name: str = "origin") -> str | None:
    """The url of a remote, or None when there isn't one."""
    code, out, _ = run_git(["remote", "get-url", name], cwd=repo)
    return out if code == 0 and out else None


def top_level_dirs(repo: str | Path, ref: str = "HEAD") -> list[str]:
    """Directory names at the root of ``ref`` — the names a new project must avoid."""
    code, out, _ = run_git(["ls-tree", "--name-only", "-d", ref], cwd=repo)
    return sorted(out.splitlines()) if code == 0 and out else []


def fetch(repo: str | Path, remote: str, ref: str, *, depth: int = 0) -> tuple[bool, str]:
    """Download one branch. ``depth=1`` grabs only the latest commit, which is
    all the overlap report needs and far faster on a large repo."""
    args = ["fetch", "--no-tags"]
    if depth:
        args.append(f"--depth={depth}")
    args += [remote, ref]
    code, _, err = run_git(args, cwd=repo)
    return code == 0, err


def ls_tree(repo: str | Path, ref: str) -> dict[str, tuple[str, int]]:
    """Every file at ``ref`` as ``path -> (blob_sha, size_in_bytes)``.

    Git already content-addresses every file, so two files with the same blob
    sha are byte-identical — which makes duplicate detection exact and free.
    """
    code, out, _ = run_git(["ls-tree", "-r", "--long", ref], cwd=repo)
    if code != 0:
        return {}
    tree: dict[str, tuple[str, int]] = {}
    for line in out.splitlines():
        meta, tab, path = line.partition("\t")
        if not tab:
            continue
        parts = meta.split()
        if len(parts) < 4 or parts[1] != "blob":
            continue
        sha, raw_size = parts[2], parts[3]
        tree[path] = (sha, int(raw_size) if raw_size.isdigit() else 0)
    return tree


def subtree_add(
    repo: str | Path, prefix: str, remote: str, branch: str, *, squash: bool = False
) -> tuple[bool, str]:
    """Graft ``remote/branch`` under ``prefix``, keeping its history."""
    args = ["subtree", "add", f"--prefix={prefix}", remote, branch]
    if squash:
        args.append("--squash")
    args += ["-m", f"consolidate: add {prefix} from {remote}/{branch}"]
    code, out, err = run_git(args, cwd=repo)
    return code == 0, (err or out)


def subtree_pull(
    repo: str | Path, prefix: str, remote: str, branch: str, *, squash: bool = False
) -> tuple[bool, str]:
    """Bring a already-folded-in project up to date with its source repo."""
    args = ["subtree", "pull", f"--prefix={prefix}", remote, branch]
    if squash:
        args.append("--squash")
    args += ["-m", f"consolidate: update {prefix} from {remote}/{branch}"]
    code, out, err = run_git(args, cwd=repo)
    return code == 0, (err or out)


def count_commits(repo: str | Path, ref: str = "HEAD") -> int:
    code, out, _ = run_git(["rev-list", "--count", ref], cwd=repo)
    return int(out) if code == 0 and out.isdigit() else 0


def contains_commit(repo: str | Path, sha: str) -> bool:
    """True when ``sha`` is an ancestor of HEAD — the proof a graft really landed."""
    code, _, _ = run_git(["merge-base", "--is-ancestor", sha, "HEAD"], cwd=repo)
    return code == 0


def rev_parse(repo: str | Path, ref: str) -> str | None:
    code, out, _ = run_git(["rev-parse", "--verify", ref], cwd=repo)
    return out if code == 0 and out else None
