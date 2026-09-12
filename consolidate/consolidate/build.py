"""Run a plan: create the consolidated repo and graft each project into it.

Two promises shape this module.

*Nothing is hidden.* Every git command that runs is recorded as a
:class:`~consolidate.models.Step`, and ``dry_run=True`` produces exactly the
same list of steps without running any of them. What you review is what runs.

*Nothing is destroyed.* The source repositories are only ever read from — the
builder clones and fetches, and never pushes, deletes or rewrites them.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable

from . import gitops, layout, overlap
from .models import Placement, Plan, SourceRepo, Step


@dataclass
class BuildResult:
    plan: Plan
    steps: list[Step] = field(default_factory=list)
    merged: dict[str, str] = field(default_factory=dict)  # dest -> source commit sha
    root: str = ""
    dry_run: bool = False

    @property
    def ok(self) -> bool:
        return all(step.ok is not False for step in self.steps)

    @property
    def failures(self) -> list[Step]:
        return [step for step in self.steps if step.ok is False]

    def to_dict(self) -> dict:
        return {
            "root": self.root,
            "dry_run": self.dry_run,
            "ok": self.ok,
            "merged": self.merged,
            "steps": [s.to_dict() for s in self.steps],
            "plan": self.plan.to_dict(),
        }


def remote_name(placement: Placement) -> str:
    """A stable per-project remote name, so re-runs reuse the same remote."""
    return "src-" + placement.dest.rsplit("/", 1)[-1]


def probe(plan: Plan) -> Plan:
    """Ask each remote for its real default branch, and notice empty repos.

    Planning guesses ``main``; a repo whose trunk is ``master`` would otherwise
    fail at the last moment. One cheap ``ls-remote`` each removes that class of
    failure entirely.
    """
    placements = []
    for placement in plan.placements:
        repo = placement.repo
        if not placement.included:
            placements.append(placement)
            continue
        branches = gitops.remote_branches(repo.url)
        if not branches:
            placements.append(
                Placement(
                    repo=repo,
                    dest=placement.dest,
                    disposition="skip",
                    reason="the remote has no branches — it is an empty repository",
                )
            )
            continue
        branch = repo.default_branch
        if branch not in branches:
            branch = gitops.default_branch(repo.url) or (
                "main" if "main" in branches else "master" if "master" in branches else branches[0]
            )
        from dataclasses import replace

        placements.append(
            Placement(
                repo=replace(repo, default_branch=branch),
                dest=placement.dest,
                disposition=placement.disposition,
                reason=placement.reason,
            )
        )
    return plan.with_placements(placements)


def collect_trees(root: str | Path, plan: Plan) -> dict[str, overlap.Tree]:
    """The file list of every fetched project, keyed by its destination name.

    Read from the fetched remote refs rather than from disk, so the overlap
    report can be produced before anything is merged.
    """
    trees: dict[str, overlap.Tree] = {}
    for placement in plan.included:
        ref = f"{remote_name(placement)}/{placement.repo.default_branch}"
        tree = gitops.ls_tree(root, ref)
        if tree:
            trees[placement.dest.rsplit("/", 1)[-1]] = tree
    return trees


def _step(result: BuildResult, action: str, detail: str, run) -> bool:
    """Record a step, running it unless this is a dry run."""
    if result.dry_run:
        result.steps.append(Step(action=action, detail=detail, ok=None))
        return True
    ok, output = run()
    result.steps.append(Step(action=action, detail=detail, ok=ok, output=output[:2000]))
    return ok


def preflight(root: str | Path, *, update: bool) -> list[str]:
    """Problems that would stop the build, in plain language. Empty means go."""
    problems = []
    if not gitops.git_available():
        problems.append("git is not installed. Install git and run this again.")
    elif not gitops.subtree_available():
        problems.append(
            "your git has no 'subtree' command, which is what keeps the history. "
            "On Debian/Ubuntu: sudo apt install git-subtree"
        )
    root = Path(root)
    if root.exists() and any(root.iterdir()):
        if not gitops.is_repo(root):
            problems.append(f"{root} already exists and is not empty. Choose an empty directory.")
        elif not update:
            problems.append(
                f"{root} is already a repository. Re-run with --update to fold new "
                "projects in and refresh the ones already there."
            )
        elif not gitops.is_clean(root):
            problems.append(f"{root} has uncommitted changes. Commit or stash them first.")
    return problems


def build(
    root: str | Path,
    plan: Plan,
    *,
    dry_run: bool = False,
    update: bool = False,
    squash: bool = False,
    branch: str = "main",
    index_plan: Plan | None = None,
) -> BuildResult:
    """Create the consolidated repo at ``root`` and fold in every project.

    ``plan`` is what we act on; ``index_plan`` is what gets written into
    README.md and consolidate.json. They differ when only one project is being
    updated — acting on one project must never rewrite the index as though the
    others had stopped existing.
    """
    root = Path(root)
    index = index_plan or plan
    result = BuildResult(plan=plan, root=str(root), dry_run=dry_run)

    # Keep the commits recorded for projects we are not touching this run.
    previous = layout.read_manifest_commits(root)
    result.merged.update(previous)

    fresh = not (root.exists() and gitops.is_repo(root))
    if fresh:
        _step(result, "init", f"create an empty git repository at {root} on '{branch}'",
              lambda: gitops.init_repo(root, branch=branch))
        _step(result, "scaffold", "write README.md, CONSOLIDATION.md, consolidate.json, .gitignore",
              lambda: (bool(layout.write_scaffold(root, index)), ""))
        _step(result, "commit", "commit the empty consolidated repo",
              lambda: gitops.commit_all(root, f"consolidate: start {plan.dest_name}"))
    else:
        gitops.ensure_identity(root)

    for placement in plan.included:
        repo = placement.repo
        remote = remote_name(placement)
        already = (root / placement.dest).exists() and not fresh

        if not _step(result, "remote", f"point '{remote}' at {repo.url}",
                     lambda repo=repo, remote=remote: gitops.add_remote(root, remote, repo.url)):
            continue
        if not _step(result, "fetch", f"download {repo.slug} ({repo.default_branch})",
                     lambda repo=repo, remote=remote: gitops.fetch(root, remote, repo.default_branch)):
            continue

        if not dry_run:
            sha = gitops.rev_parse(root, f"{remote}/{repo.default_branch}")
            if sha:
                result.merged[placement.dest] = sha

        if already:
            if not update:
                result.steps.append(
                    Step("skip", f"{placement.dest} is already here", ok=True,
                         output="re-run with --update to pull its latest commits")
                )
                continue
            _step(result, "update", f"pull new commits into {placement.dest}",
                  lambda p=placement, remote=remote: gitops.subtree_pull(
                      root, p.dest, remote, p.repo.default_branch, squash=squash))
        else:
            _step(result, "graft",
                  f"add {repo.slug} at {placement.dest}, keeping its full history",
                  lambda p=placement, remote=remote: gitops.subtree_add(
                      root, p.dest, remote, p.repo.default_branch, squash=squash))

    _step(result, "index", f"refresh {index.index_file} and CONSOLIDATION.md from the result",
          lambda: (bool(layout.write_scaffold(
              root, index, shas=result.merged, existing=existing_dirs(root, index))), ""))
    _step(result, "commit", "commit the generated index",
          lambda: gitops.commit_all(root, "consolidate: refresh the project index"))

    return result


def verify(root: str | Path, result: BuildResult) -> list[str]:
    """Prove the build did what it claimed. Returns the problems found."""
    root = Path(root)
    problems = []
    for placement in result.plan.included:
        if not (root / placement.dest).exists():
            problems.append(f"{placement.dest} is missing from the consolidated repo")
            continue
        sha = result.merged.get(placement.dest)
        if sha and not gitops.contains_commit(root, sha):
            problems.append(
                f"{placement.dest} is present but {placement.repo.slug}'s history "
                f"({sha[:12]}) is not reachable — the merge did not keep it"
            )
    if not gitops.is_clean(root):
        problems.append("the consolidated repo has uncommitted changes left over")
    return problems


def survey(root: str | Path, plan: Plan, *, depth: int = 1) -> dict[str, overlap.Tree]:
    """Fetch just enough of every project to list its files.

    Used by the overlap report on its own: a shallow fetch of one commit per
    repo is all that is needed to compare file trees, and it costs a fraction
    of a full clone.
    """
    root = Path(root)
    if not gitops.is_repo(root):
        gitops.init_repo(root)
    for placement in plan.included:
        remote = remote_name(placement)
        gitops.add_remote(root, remote, placement.repo.url)
        gitops.fetch(root, remote, placement.repo.default_branch, depth=depth)
    return collect_trees(root, plan)


def local_trees(repo: str | Path, *, dirs: list[str] | None = None, ref: str = "HEAD") -> dict[str, overlap.Tree]:
    """Split one repository into its projects, for an inside-out overlap check.

    The same analysis used across repos works just as well *within* a repo that
    has already grown several projects: each directory is treated as a project
    and compared against the others.

    ``dirs`` names the directories to compare; by default every top-level one.
    """
    tree = gitops.ls_tree(repo, ref)
    if not tree:
        return {}
    if dirs:
        wanted = [d.strip("/") for d in dirs]
    else:
        wanted = sorted({path.split("/")[0] for path in tree if "/" in path})

    out: dict[str, overlap.Tree] = {}
    for name in wanted:
        prefix = name + "/"
        sub = {path[len(prefix) :]: value for path, value in tree.items() if path.startswith(prefix)}
        if sub:
            out[name] = sub
    return out


def host_slug(root: str | Path) -> str:
    """``owner/name`` of the repo at ``root``, read from its origin remote."""
    url = gitops.remote_url(root)
    if not url:
        return ""
    from .discover import DiscoveryError, parse_spec

    try:
        owner, name = parse_spec(url)
    except DiscoveryError:
        return ""
    return f"{owner}/{name}"


def adopt_plan(root: str | Path, *, name: str = "", prefix: str = "projects") -> Plan:
    """The plan for making an existing repository the consolidation home.

    The repo keeps everything it already has. Its own README is left alone and
    the generated index goes to PROJECTS.md instead; its existing top-level
    directories are reserved so nothing folded in later can land on top of one.
    """
    root = Path(root)
    from .plan import build_plan as _build_plan

    existing = gitops.top_level_dirs(root)
    readme = root / "README.md"
    index_file = "PROJECTS.md" if readme.exists() and not layout.is_generated(readme) else "README.md"
    return _build_plan(
        [],
        dest_name=name or root.name,
        prefix=prefix,
        reserved=existing,
        index_file=index_file,
        host=host_slug(root),
        adopted=True,
    )


def merge_plans(existing: Plan, newcomers: Iterable[SourceRepo], *, root: str | Path) -> Plan:
    """Add repositories to a plan an existing consolidated repo already has.

    Anything already folded in keeps its directory, the host repo is never
    folded into itself, and new names avoid every directory in use.
    """
    from .plan import build_plan as _build_plan

    known = {p.repo.slug for p in existing.placements}
    wanted = [
        repo
        for repo in newcomers
        if repo.slug not in known and repo.slug.lower() != existing.host.lower()
    ]
    if not wanted:
        return existing

    reserved = set(gitops.top_level_dirs(root))
    reserved |= {p.dest.rsplit("/", 1)[-1] for p in existing.placements}
    fresh = _build_plan(
        wanted,
        dest_name=existing.dest_name,
        prefix=existing.prefix,
        reserved=reserved,
        index_file=existing.index_file,
        host=existing.host,
        adopted=existing.adopted,
    )
    return existing.with_placements(list(existing.placements) + list(fresh.placements))


def existing_dirs(root: str | Path, plan: Plan) -> list[str]:
    """Directories an adopted repo already had, excluding the one we fold into."""
    if not plan.adopted:
        return []
    prefix = (plan.prefix or "").strip("/")
    return [
        name
        for name in gitops.top_level_dirs(root)
        if name != prefix and not name.startswith(".")  # .github, .claude: config, not projects
    ]


def adopt(root: str | Path, plan: Plan, *, dry_run: bool = False) -> BuildResult:
    """Write the consolidation files into an existing repository."""
    root = Path(root)
    result = BuildResult(plan=plan, root=str(root), dry_run=dry_run)
    if not dry_run:
        gitops.ensure_identity(root)
    here = existing_dirs(root, plan)
    _step(result, "scaffold", f"write {plan.index_file}, CONSOLIDATION.md, {layout.MANIFEST_NAME}",
          lambda: (bool(layout.write_scaffold(root, plan, existing=here)), ""))
    _step(result, "commit", f"record {plan.dest_name} as the consolidation home",
          lambda: gitops.commit_all(root, f"consolidate: make {plan.dest_name} the home repository"))
    return result
