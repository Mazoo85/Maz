"""Turn a list of repositories into a reviewable plan.

Two jobs live here and nothing else: giving every repo a clean, non-clashing
directory name, and deciding which repos should be folded in at all.

The policy is deliberately conservative — when consolidating is likely to cost
you something (a fork loses its link to upstream), the default is to skip and
say so, and you opt back in with a flag.
"""

from __future__ import annotations

import re
from typing import Iterable

from .models import Placement, Plan, SourceRepo

#: Directory names a project may not take, because the repo itself uses them.
RESERVED = frozenset({".git", ".github", "docs", "scripts", "shared"})

_SEPARATORS = re.compile(r"[^a-z0-9]+")


def slugify(name: str) -> str:
    """``"Codebase Memory MCP"`` -> ``"codebase-memory-mcp"``.

    Lowercase, ASCII, hyphen-separated: one naming rule for every project, which
    is most of what "a very clean format" means in practice.
    """
    slug = _SEPARATORS.sub("-", name.strip().lower()).strip("-")
    return slug or "project"


def _candidates(repo: SourceRepo) -> Iterable[str]:
    """Names to try for a repo, in order, until one is free."""
    base = slugify(repo.name)
    yield base
    yield f"{slugify(repo.owner)}-{base}"
    for n in range(2, 100):
        yield f"{base}-{n}"


def assign_dirs(repos: Iterable[SourceRepo], *, reserved: Iterable[str] = ()) -> dict[str, str]:
    """Map each repo slug to a unique directory name.

    Ordering is the caller's, and it is stable: the first repo to want a name
    keeps it, so re-running against a grown list never renames what already
    landed.
    """
    taken: set[str] = set(RESERVED) | {name.strip("/") for name in reserved}
    out: dict[str, str] = {}
    for repo in repos:
        for candidate in _candidates(repo):
            if candidate not in taken:
                taken.add(candidate)
                out[repo.slug] = candidate
                break
    return out


def classify(repo: SourceRepo, *, include_forks: bool, include_archived: bool) -> tuple[str, str]:
    """Decide include/skip for one repo, with the reason a human needs.

    Returns ``(disposition, reason)``.
    """
    if repo.empty:
        return "skip", "repository has no commits — there is nothing to fold in"
    if repo.is_fork and not include_forks:
        return (
            "skip",
            "a fork of someone else's project: folding it in ends your ability to "
            "pull upstream fixes or send changes back. Use --include-forks to "
            "override, or keep it as a separate repo.",
        )
    if repo.archived and not include_archived:
        return "skip", "archived upstream — use --include-archived to fold it in anyway"
    notes = []
    if repo.archived:
        notes.append("archived upstream")
    if repo.is_fork:
        notes.append("a fork — the link to upstream is lost once folded in")
    return "include", "; ".join(notes)


def build_plan(
    repos: Iterable[SourceRepo],
    *,
    dest_name: str,
    prefix: str = "projects",
    include_forks: bool = False,
    include_archived: bool = True,
    reserved: Iterable[str] = (),
    index_file: str = "README.md",
    host: str = "",
    adopted: bool = False,
) -> Plan:
    """Build the full plan: every repo gets a decision and a destination.

    ``reserved`` names directories that are already spoken for — the top-level
    folders of a repository being adopted as the home, for instance.
    """
    repos = list(repos)
    dirs = assign_dirs(repos, reserved=reserved)
    placements = []
    for repo in repos:
        disposition, reason = classify(
            repo, include_forks=include_forks, include_archived=include_archived
        )
        directory = dirs[repo.slug]
        dest = f"{prefix}/{directory}" if prefix else directory
        placements.append(Placement(repo=repo, dest=dest, disposition=disposition, reason=reason))
    return Plan(
        dest_name=dest_name,
        prefix=prefix,
        placements=tuple(placements),
        index_file=index_file,
        host=host,
        adopted=adopted,
    )
