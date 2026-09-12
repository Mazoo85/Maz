"""The generated files that make a consolidated repo readable.

A merged repo that is just N directories in a row is not "clean" — you still
have to open each one to find out what it is. So the build writes three things
and keeps them true:

* ``README.md``        one table: every project, what it is, where it came from
* ``CONSOLIDATION.md`` provenance — original repo, branch and commit per project
* ``consolidate.json`` the machine-readable version, so the repo can update
                       itself later without you retyping anything

All three are pure functions of the plan, so they can never drift from it.
"""

from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path

from .models import Plan

MANIFEST_NAME = "consolidate.json"

GITIGNORE = """\
# Build output
build/
dist/
out/

# Python
__pycache__/
*.py[cod]
.venv/
venv/
.pytest_cache/

# Node
node_modules/

# Editors and OS
.DS_Store
.idea/
.vscode/
*.swp
"""


def _timestamp() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%d")


def render_readme(plan: Plan, *, shas: dict[str, str] | None = None) -> str:
    """The front page: what this repo is and what is in it."""
    shas = shas or {}
    lines = [
        f"# {plan.dest_name}",
        "",
        "One repository holding every project, each in its own directory, each with",
        "its full history intact. Nothing was rewritten or flattened on the way in —",
        f"the folder `{plan.prefix or '.'}/` is where the projects live.",
        "",
        "## Projects",
        "",
        "| Project | What it is | Came from |",
        "| --- | --- | --- |",
    ]
    for placement in plan.included:
        repo = placement.repo
        what = repo.description or "_no description yet_"
        origin = f"[{repo.slug}]({repo.url.removesuffix('.git')})"
        lines.append(f"| [`{placement.dest}`]({placement.dest}) | {what} | {origin} |")

    if not plan.included:
        lines.append("| _none yet_ | | |")

    lines += [
        "",
        "## Working in here",
        "",
        "Each project is self-contained: its build, its tests and its README are",
        "inside its own directory, and they work exactly as they did before.",
        "",
        "```sh",
        f"cd {plan.included[0].dest if plan.included else plan.prefix + '/<project>'}",
        "```",
        "",
        "## Where this came from",
        "",
        "See [CONSOLIDATION.md](CONSOLIDATION.md) for the original repository, branch",
        "and commit behind every directory, and for how to pull in later changes.",
        "",
    ]

    if plan.skipped:
        lines += [
            "## Deliberately left out",
            "",
            "| Repository | Why |",
            "| --- | --- |",
        ]
        for placement in plan.skipped:
            reason = placement.reason.replace("\n", " ") or "skipped"
            lines.append(f"| `{placement.repo.slug}` | {reason} |")
        lines.append("")

    return "\n".join(lines)


def render_provenance(plan: Plan, *, shas: dict[str, str] | None = None) -> str:
    """Exactly what was folded in, from where, at which commit."""
    shas = shas or {}
    lines = [
        "# Consolidation record",
        "",
        f"Built on {_timestamp()} by `maz-consolidate`.",
        "",
        "Every directory below was added with `git subtree`, which keeps the source",
        "repository's entire history — every commit, author and date is still in",
        "`git log`. No files were copied and no history was squashed.",
        "",
        "| Directory | Source repository | Branch | Commit at merge |",
        "| --- | --- | --- | --- |",
    ]
    for placement in plan.included:
        repo = placement.repo
        sha = shas.get(placement.dest, "")
        short = sha[:12] if sha else "—"
        lines.append(
            f"| `{placement.dest}` | {repo.url.removesuffix('.git')} | `{repo.default_branch}` | `{short}` |"
        )
    if not plan.included:
        lines.append("| _none_ | | | |")

    lines += [
        "",
        "## Reading the history",
        "",
        "Every commit from every project is here. `git log` with no path shows them",
        "all interleaved:",
        "",
        "```sh",
        "git log --oneline --graph",
        "```",
        "",
        "To read **one project's** history on its own, use the commit from the table",
        "above — it is that project's last commit as it stood in its own repository,",
        "so everything reachable from it is exactly that project's history and",
        "nothing else:",
        "",
        "```sh",
        "git log --oneline <commit from the table>",
        "```",
        "",
        "One file within it, using the path the file had **in the original repo**",
        "(not the new prefixed one):",
        "",
        "```sh",
        "git log --follow <commit from the table> -- path/inside/the/old/repo.js",
        "```",
        "",
        "Plain `git log projects/<project>/file.js` shows only the merge commit. That",
        "is normal: the old commits record the file under its original path, which is",
        "why the two commands above take that path instead.",
        "",
        "## Pulling in later changes",
        "",
        "If you keep working in one of the original repositories, bring those commits",
        "across with:",
        "",
        "```sh",
        "consolidate update            # every project",
        "consolidate update <project>  # just one",
        "```",
        "",
        "That runs `git subtree pull` under the hood, so the update is a normal merge",
        "and your local edits are preserved.",
        "",
        "## Notes recorded at build time",
        "",
    ]
    noted = [p for p in plan.placements if p.reason]
    if noted:
        for placement in noted:
            mark = "folded in" if placement.included else "left out"
            lines.append(f"- **{placement.repo.slug}** ({mark}): {placement.reason}")
    else:
        lines.append("_None._")
    lines.append("")
    return "\n".join(lines)


def render_manifest(plan: Plan, *, shas: dict[str, str] | None = None) -> str:
    """The plan as JSON, stored in the repo so it can update itself later."""
    payload = plan.to_dict()
    payload["generated"] = _timestamp()
    payload["merged_commits"] = dict(sorted((shas or {}).items()))
    return json.dumps(payload, indent=2) + "\n"


def write_scaffold(root: str | Path, plan: Plan, *, shas: dict[str, str] | None = None) -> list[str]:
    """Write (or refresh) the generated files. Returns the paths written."""
    root = Path(root)
    root.mkdir(parents=True, exist_ok=True)
    files = {
        "README.md": render_readme(plan, shas=shas),
        "CONSOLIDATION.md": render_provenance(plan, shas=shas),
        MANIFEST_NAME: render_manifest(plan, shas=shas),
    }
    gitignore = root / ".gitignore"
    if not gitignore.exists():
        files[".gitignore"] = GITIGNORE
    for name, text in files.items():
        (root / name).write_text(text, encoding="utf-8")
    return sorted(files)


def read_manifest(root: str | Path) -> Plan | None:
    """Load the plan a previous build left behind, if there is one."""
    path = Path(root) / MANIFEST_NAME
    if not path.exists():
        return None
    return Plan.from_dict(json.loads(path.read_text(encoding="utf-8")))


def read_manifest_commits(root: str | Path) -> dict[str, str]:
    """The source commit recorded for each project by a previous build.

    Read back so that updating one project does not wipe the record of the
    others out of the index.
    """
    path = Path(root) / MANIFEST_NAME
    if not path.exists():
        return {}
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}
    commits = data.get("merged_commits", {})
    return {str(k): str(v) for k, v in commits.items()} if isinstance(commits, dict) else {}
