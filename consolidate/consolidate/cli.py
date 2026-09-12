"""The commands you actually type.

Four of them, in the order you would use them:

    consolidate plan    --user <you>          # what would happen
    consolidate report  --user <you>          # what you are maintaining twice
    consolidate build   <folder> --user <you> # do it (dry run unless --yes)
    consolidate update  <folder>              # pull later changes in

Every command that changes anything is a dry run by default and prints exactly
what it would do. Nothing is ever pushed, and the source repositories are only
ever read.
"""

from __future__ import annotations

import json
import tempfile
from pathlib import Path
from typing import Optional

import typer
from rich.console import Console
from rich.table import Table

from . import build as build_mod
from . import discover, layout, overlap
from .models import Plan
from .plan import build_plan

app = typer.Typer(
    add_completion=False,
    help="Fold many repositories into one clean repository, keeping every commit.",
)
console = Console()


# --------------------------------------------------------------------------
# shared option handling
# --------------------------------------------------------------------------

def _gather(
    user: Optional[str], repos: list[str], from_json: Optional[Path]
) -> list:
    """Work out which repositories we are talking about."""
    if from_json:
        return discover.from_file(from_json)
    if repos:
        return discover.from_specs(repos)
    if user or discover.env_token():
        return discover.from_github(user)
    raise typer.BadParameter(
        "Tell me which repositories to use: --user <github-username>, "
        "one or more --repo owner/name, or --from-json <file>."
    )


def _plan_from_options(
    user: Optional[str],
    repos: list[str],
    from_json: Optional[Path],
    name: str,
    prefix: str,
    include_forks: bool,
    include_archived: bool,
) -> Plan:
    try:
        sources = _gather(user, repos, from_json)
    except discover.DiscoveryError as exc:
        console.print(f"[red]{exc}[/red]")
        raise typer.Exit(code=2)
    if not sources:
        console.print("[yellow]No repositories found.[/yellow]")
        raise typer.Exit(code=1)
    return build_plan(
        sources,
        dest_name=name,
        prefix=prefix,
        include_forks=include_forks,
        include_archived=include_archived,
    )


def _print_plan(plan: Plan) -> None:
    table = Table(title=f"Plan for {plan.dest_name}", show_lines=False)
    table.add_column("Repository", style="cyan", no_wrap=True)
    table.add_column("Goes to", style="green")
    table.add_column("Notes", style="dim")
    for placement in plan.included:
        table.add_row(placement.repo.slug, placement.dest, placement.reason or "")
    for placement in plan.skipped:
        table.add_row(f"[dim]{placement.repo.slug}[/dim]", "[yellow]left out[/yellow]", placement.reason)
    console.print(table)
    console.print(
        f"\n[bold]{len(plan.included)}[/bold] repositories fold in, "
        f"[bold]{len(plan.skipped)}[/bold] left out."
    )


def _print_overlap(report: overlap.OverlapReport, *, limit: int = 15) -> None:
    if report.is_clean:
        console.print("[green]No duplicated files and no clashing names. Nothing to merge away.[/green]")
        return

    if report.pairs:
        table = Table(title="Projects that overlap")
        table.add_column("Project", style="cyan")
        table.add_column("Project", style="cyan")
        table.add_column("Identical files", justify="right")
        table.add_column("Of the smaller one", justify="right", style="magenta")
        for pair in report.pairs[:limit]:
            table.add_row(pair.a, pair.b, str(pair.identical_files), f"{pair.similarity:.0%}")
        console.print(table)

    if report.duplicates:
        table = Table(title="The same file, kept in more than one place")
        table.add_column("Size", justify="right")
        table.add_column("Found in", style="dim")
        for group in report.duplicates[:limit]:
            where = "\n".join(f"{project}/{path}" for project, path in group.locations)
            table.add_row(f"{group.size:,} B", where)
        console.print(table)
        console.print(
            f"[dim]{len(report.duplicates)} duplicated files, "
            f"{report.wasted_bytes:,} bytes held twice or more.[/dim]"
        )

    drifted = [c for c in report.name_clashes if c.kind == "file"]
    if drifted:
        console.print(
            f"\n[yellow]{len(drifted)} filenames exist in several projects with "
            "different contents[/yellow] — copies that have drifted apart, where a fix "
            "to one never reaches the other:"
        )
        for clash in drifted[:limit]:
            console.print(f"  [bold]{clash.name}[/bold] in {', '.join(clash.projects)}")


def _print_steps(result: build_mod.BuildResult) -> None:
    table = Table(title="Dry run — nothing has been changed" if result.dry_run else "What happened")
    table.add_column("", width=3)
    table.add_column("Step", style="cyan", no_wrap=True)
    table.add_column("Detail")
    for step in result.steps:
        mark = "[dim]·[/dim]" if step.ok is None else ("[green]ok[/green]" if step.ok else "[red]!![/red]")
        table.add_row(mark, step.action, step.detail)
    console.print(table)
    for step in result.failures:
        console.print(f"[red]{step.action} failed:[/red] {step.detail}\n  {step.output}")


# --------------------------------------------------------------------------
# commands
# --------------------------------------------------------------------------

USER_OPT = typer.Option(None, "--user", "-u", help="GitHub username whose repositories to fold in.")
REPO_OPT = typer.Option(None, "--repo", "-r", help="A specific repo (owner/name). Repeatable.")
JSON_OPT = typer.Option(None, "--from-json", help="Read the repository list from a JSON file.")
NAME_OPT = typer.Option("consolidated", "--name", "-n", help="Name for the consolidated repository.")
PREFIX_OPT = typer.Option("projects", "--prefix", help="Folder the projects live in. Empty for the top level.")
FORKS_OPT = typer.Option(False, "--include-forks", help="Fold in forks too (you lose the upstream link).")
ARCHIVED_OPT = typer.Option(True, "--include-archived/--skip-archived", help="Fold in archived repos.")


@app.command()
def plan(
    user: Optional[str] = USER_OPT,
    repo: Optional[list[str]] = REPO_OPT,
    from_json: Optional[Path] = JSON_OPT,
    name: str = NAME_OPT,
    prefix: str = PREFIX_OPT,
    include_forks: bool = FORKS_OPT,
    include_archived: bool = ARCHIVED_OPT,
    probe: bool = typer.Option(True, "--probe/--no-probe", help="Ask each repo for its real branch."),
    out: Optional[Path] = typer.Option(None, "--out", help="Save the plan as JSON for later."),
) -> None:
    """Show what would be folded in, where, and what would be left out."""
    result = _plan_from_options(user, repo or [], from_json, name, prefix, include_forks, include_archived)
    if probe:
        with console.status("Checking each repository..."):
            result = build_mod.probe(result)
    _print_plan(result)
    if out:
        out.write_text(result.to_json(), encoding="utf-8")
        console.print(f"\nPlan saved to [bold]{out}[/bold] — run it with `consolidate build <folder> --from-plan {out}`.")


@app.command()
def report(
    user: Optional[str] = USER_OPT,
    repo: Optional[list[str]] = REPO_OPT,
    from_json: Optional[Path] = JSON_OPT,
    name: str = NAME_OPT,
    prefix: str = PREFIX_OPT,
    include_forks: bool = FORKS_OPT,
    include_archived: bool = ARCHIVED_OPT,
    workdir: Optional[Path] = typer.Option(None, "--workdir", help="Where to cache the downloads."),
) -> None:
    """Find the work you are doing twice, before you merge anything."""
    result = _plan_from_options(user, repo or [], from_json, name, prefix, include_forks, include_archived)
    result = build_mod.probe(result)

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(workdir) if workdir else Path(tmp) / "survey"
        with console.status("Downloading a shallow copy of each repository..."):
            trees = build_mod.survey(root, result)
    if not trees:
        console.print("[yellow]Nothing could be downloaded, so there is nothing to compare.[/yellow]")
        raise typer.Exit(code=1)
    console.print(f"Compared [bold]{len(trees)}[/bold] projects.\n")
    _print_overlap(overlap.analyse(trees))


@app.command()
def build(
    folder: Path = typer.Argument(..., help="Folder to create the consolidated repository in."),
    user: Optional[str] = USER_OPT,
    repo: Optional[list[str]] = REPO_OPT,
    from_json: Optional[Path] = JSON_OPT,
    from_plan: Optional[Path] = typer.Option(None, "--from-plan", help="Run a plan saved by `plan --out`."),
    name: Optional[str] = typer.Option(None, "--name", "-n", help="Name for the consolidated repository."),
    prefix: str = PREFIX_OPT,
    include_forks: bool = FORKS_OPT,
    include_archived: bool = ARCHIVED_OPT,
    squash: bool = typer.Option(False, "--squash", help="Keep one commit per project instead of full history."),
    branch: str = typer.Option("main", "--branch", help="Branch name for the new repository."),
    update: bool = typer.Option(False, "--update", help="Work on an existing consolidated repo."),
    yes: bool = typer.Option(False, "--yes", "-y", help="Actually do it. Without this it is a dry run."),
) -> None:
    """Create the consolidated repository. Dry run unless you pass --yes."""
    if from_plan:
        the_plan = Plan.from_json(Path(from_plan).read_text(encoding="utf-8"))
    else:
        the_plan = _plan_from_options(
            user, repo or [], from_json, name or folder.name, prefix, include_forks, include_archived
        )
        with console.status("Checking each repository..."):
            the_plan = build_mod.probe(the_plan)

    _print_plan(the_plan)

    problems = build_mod.preflight(folder, update=update)
    if problems:
        for problem in problems:
            console.print(f"[red]✗[/red] {problem}")
        raise typer.Exit(code=2)

    result = build_mod.build(folder, the_plan, dry_run=not yes, update=update, squash=squash, branch=branch)
    _print_steps(result)

    if not yes:
        console.print(
            "\n[bold]This was a dry run.[/bold] Nothing was created. "
            "Add [bold]--yes[/bold] to the same command to do it for real."
        )
        return

    if not result.ok:
        console.print("\n[red]Some steps failed — see above. The repository is incomplete.[/red]")
        raise typer.Exit(code=1)

    issues = build_mod.verify(folder, result)
    if issues:
        for issue in issues:
            console.print(f"[red]✗[/red] {issue}")
        raise typer.Exit(code=1)

    console.print(f"\n[green]Done.[/green] {folder} holds {len(result.plan.included)} projects with full history.")
    trees = build_mod.collect_trees(folder, result.plan)
    if trees:
        console.print()
        _print_overlap(overlap.analyse(trees))
    console.print(
        "\nNothing was pushed anywhere. When you are happy with it:\n"
        f"  [dim]cd {folder} && git remote add origin <the new empty repo's url> && git push -u origin {branch}[/dim]"
    )


@app.command()
def update(
    folder: Path = typer.Argument(..., help="An existing consolidated repository."),
    project: Optional[str] = typer.Argument(None, help="Just this project, instead of all of them."),
    squash: bool = typer.Option(False, "--squash"),
    yes: bool = typer.Option(False, "--yes", "-y", help="Actually do it. Without this it is a dry run."),
) -> None:
    """Pull later changes from the original repositories into the consolidated one."""
    saved = layout.read_manifest(folder)
    if saved is None:
        console.print(f"[red]{folder} has no {layout.MANIFEST_NAME} — it was not built by this tool.[/red]")
        raise typer.Exit(code=2)
    acting = saved
    if project:
        wanted = [p for p in saved.placements if p.dest.rsplit("/", 1)[-1] == project or p.dest == project]
        if not wanted:
            console.print(f"[red]No project called {project!r} in {folder}.[/red]")
            raise typer.Exit(code=2)
        acting = saved.with_placements(wanted)

    result = build_mod.build(
        folder, acting, dry_run=not yes, update=True, squash=squash, index_plan=saved
    )
    _print_steps(result)
    if not yes:
        console.print("\n[bold]Dry run.[/bold] Add [bold]--yes[/bold] to pull the changes in for real.")
    elif not result.ok:
        raise typer.Exit(code=1)


@app.command()
def check(
    folder: Path = typer.Argument(Path("."), help="A repository that already holds several projects."),
    directory: Optional[list[str]] = typer.Option(
        None, "--dir", "-d", help="Compare only these directories. Repeatable."
    ),
    ref: str = typer.Option("HEAD", "--ref", help="Which commit to look at."),
) -> None:
    """Find duplicated work *inside* one repository you already have.

    The same comparison `report` runs across repositories, run across the
    directories of a single one — for when the several programs doing the same
    thing are already living together.
    """
    if not build_mod.gitops.is_repo(folder):
        console.print(f"[red]{folder} is not a git repository.[/red]")
        raise typer.Exit(code=2)
    trees = build_mod.local_trees(folder, dirs=list(directory) if directory else None, ref=ref)
    if not trees:
        console.print(f"[yellow]No project directories found in {folder} at {ref}.[/yellow]")
        raise typer.Exit(code=1)
    console.print(f"Compared [bold]{len(trees)}[/bold] directories in {folder}.\n")
    _print_overlap(overlap.analyse(trees))


@app.command("show-plan")
def show_plan(path: Path = typer.Argument(..., help="A plan saved by `plan --out`.")) -> None:
    """Print a saved plan without touching the network."""
    _print_plan(Plan.from_json(Path(path).read_text(encoding="utf-8")))


def main() -> None:  # pragma: no cover - entry point
    app()


if __name__ == "__main__":  # pragma: no cover
    main()
