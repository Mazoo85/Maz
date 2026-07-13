"""``crew`` command-line entry point.

    crew do "add retry logic to the http client"   # run a task through the crew
    crew resume                                     # continue the last task here
    crew status                                     # show saved session state
    crew agents                                     # list the crew and their tools

The heavy SDK imports happen inside the commands so ``crew --help`` and
``crew agents`` work even before ``claude-agent-sdk`` is installed.
"""

from __future__ import annotations

import typer
from rich.console import Console
from rich.panel import Panel
from rich.table import Table

from dataclasses import replace

from . import session as session_mod
from .config import CONFIG_FILENAME, effective_values, load_config

app = typer.Typer(
    add_completion=False,
    help="Orchestrate a team of AI coding agents (planner, coder, reviewer, tester).",
)
console = Console()


def _version_callback(value: bool) -> None:
    if value:
        from . import __version__

        # Use typer.echo (not the rich Console) so output is captured under test
        # runners and plain pipes.
        typer.echo(f"crew {__version__}")
        raise typer.Exit()


@app.callback()
def main(
    version: bool = typer.Option(
        False,
        "--version",
        callback=_version_callback,
        is_eager=True,
        help="Show the version and exit.",
    ),
) -> None:
    """Orchestrate a team of AI coding agents (planner, coder, reviewer, tester)."""


def _interactive_confirm(question: str) -> bool:
    return typer.confirm(question, default=True)


def _auto_confirm(question: str) -> bool:
    # Non-interactive: print the checkpoint and approve it automatically.
    console.print(f"[dim]auto-approve:[/dim] {question} [green]yes[/green]")
    return True


def _resolve_config(max_fix_rounds: int | None):
    config = load_config()
    if max_fix_rounds is not None:
        config = replace(config, max_fix_rounds=max_fix_rounds)
    return config


def _maybe_commit(task: str, message: str | None, confirm) -> None:
    """Optionally stage + commit the working tree after a completed run."""
    from pathlib import Path

    from .gitutil import commit_all, has_changes, is_git_repo

    root = Path.cwd()
    if not is_git_repo(root):
        console.print("[yellow]--commit: not a git repository; skipping.[/yellow]")
        return
    if not has_changes(root):
        console.print("[dim]--commit: no changes to commit.[/dim]")
        return
    msg = message or f"crew: {task}"
    if not confirm(f"Commit all changes with message {msg!r}?"):
        console.print("[yellow]Commit skipped.[/yellow]")
        return
    ok, out = commit_all(msg, root)
    if ok:
        console.print(f"[green]Committed[/green] {out}. [dim](not pushed — that's your call)[/dim]")
    else:
        console.print(f"[red]Commit failed:[/red] {out}")


def _drive(
    task,
    config,
    confirm,
    resume_session_id=None,
    commit=False,
    commit_message=None,
    dry_run=False,
) -> None:
    """Run the crew, turning failures into clean, resumable messages.

    Progress is checkpointed to ``.crew/session.json`` after every completed
    phase, so any interruption or error can be picked up with ``crew resume``.
    With ``commit=True``, a completed run offers to commit the working tree.
    """
    from .orchestrator import run_task_sync

    try:
        state = run_task_sync(
            task,
            config,
            confirm=confirm,
            resume_session_id=resume_session_id,
            dry_run=dry_run,
        )
        if commit and getattr(state, "phase", None) == "done":
            _maybe_commit(task, commit_message, confirm)
    except ModuleNotFoundError as exc:  # SDK not installed
        console.print(
            f"[red]Missing dependency:[/red] {exc}.\n"
            "Install with:  pip install -e .   (needs claude-agent-sdk)"
        )
        raise typer.Exit(code=1)
    except KeyboardInterrupt:
        console.print(
            "\n[yellow]Interrupted.[/yellow] Progress was saved — run "
            "[bold]crew resume[/bold] to continue."
        )
        raise typer.Exit(code=130)
    except Exception as exc:  # noqa: BLE001 - surface a clean message, not a traceback
        console.print(
            Panel.fit(
                f"[red]The crew hit an error:[/red] {exc}\n\n"
                "Progress up to the last completed phase was saved. Fix the issue and run "
                "[bold]crew resume[/bold] to continue.",
                title="Error",
                border_style="red",
            )
        )
        raise typer.Exit(code=1)


# Shared option definitions so `do` and `resume` stay consistent.
_YES_OPT = typer.Option(
    False, "--yes", "-y", help="Non-interactive: auto-approve every checkpoint. Use with care."
)
_ROUNDS_OPT = typer.Option(
    None, "--max-fix-rounds", min=0, help="Override the coder<->tester repair-round limit."
)
_COMMIT_OPT = typer.Option(
    False, "--commit", help="After a completed run, offer to commit the working tree (never pushes)."
)
_COMMIT_MSG_OPT = typer.Option(
    None, "--commit-message", "-m", help="Commit message to use with --commit (default: 'crew: <task>')."
)


@app.command()
def do(
    task: str = typer.Argument(..., help="The task to hand to the crew, in plain English."),
    yes: bool = _YES_OPT,
    max_fix_rounds: int | None = _ROUNDS_OPT,
    commit: bool = _COMMIT_OPT,
    commit_message: str | None = _COMMIT_MSG_OPT,
    dry_run: bool = typer.Option(
        False, "--dry-run", help="Run only the planner and stop — preview the plan, change nothing."
    ),
) -> None:
    """Run TASK through the full crew: plan -> code -> review -> test, with checkpoints."""
    config = _resolve_config(max_fix_rounds)
    confirm = _auto_confirm if yes else _interactive_confirm
    console.print(f"[bold]Task:[/bold] {task}\n")
    if yes and not dry_run:
        console.print("[yellow]Running unattended (--yes): all checkpoints auto-approved.[/yellow]\n")
    _drive(task, config, confirm, commit=commit, commit_message=commit_message, dry_run=dry_run)


@app.command()
def resume(
    yes: bool = _YES_OPT,
    max_fix_rounds: int | None = _ROUNDS_OPT,
    commit: bool = _COMMIT_OPT,
    commit_message: str | None = _COMMIT_MSG_OPT,
) -> None:
    """Continue the last task in this directory using its saved session."""
    config = _resolve_config(max_fix_rounds)
    confirm = _auto_confirm if yes else _interactive_confirm
    state = session_mod.load(config)
    if not state.session_id or not state.task:
        console.print("[yellow]No saved session found in this directory. Start one with `crew do`.[/yellow]")
        raise typer.Exit(code=1)
    console.print(f"[bold]Resuming:[/bold] {state.task}  (last phase: {state.phase})\n")
    _drive(
        state.task,
        config,
        confirm,
        resume_session_id=state.session_id,
        commit=commit,
        commit_message=commit_message,
    )


@app.command()
def status() -> None:
    """Show the saved crew session for this directory."""
    config = load_config()
    state = session_mod.load(config)
    if not state.session_id:
        console.print("No saved crew session in this directory.")
        return
    table = Table(show_header=False)
    table.add_row("Task", state.task or "-")
    table.add_row("Last phase", state.phase or "-")
    table.add_row("Session id", state.session_id)
    if state.total_cost_usd:
        table.add_row("Approx. cost", f"${state.total_cost_usd:.4f}")
    console.print(table)


@app.command()
def init(
    force: bool = typer.Option(False, "--force", help="Overwrite an existing crew.json."),
) -> None:
    """Write a starter crew.json in the current directory."""
    from .config import write_starter_config

    written, path = write_starter_config(force=force)
    if written:
        console.print(
            f"[green]Wrote {path}[/green] — edit it to set per-project models and loop bounds, "
            "then run [bold]crew config[/bold] to confirm."
        )
    else:
        console.print(f"[yellow]{path} already exists.[/yellow] Use --force to overwrite.")
        raise typer.Exit(code=1)


@app.command()
def config() -> None:
    """Show the effective configuration (defaults + crew.json + env overrides)."""
    from pathlib import Path

    cfg = load_config()
    table = Table(title="Effective config")
    table.add_column("Setting", style="bold cyan")
    table.add_column("Value")
    for key, value in effective_values(cfg).items():
        table.add_row(key, str(value))
    console.print(table)
    found = Path.cwd() / CONFIG_FILENAME
    where = str(found) if found.exists() else f"none ({CONFIG_FILENAME} not present)"
    console.print(f"[dim]Project config file: {where}[/dim]")


@app.command()
def runs() -> None:
    """List saved run transcripts for this directory (most recent first)."""
    from .transcript import list_runs

    runs_dir = load_config().state_dir() / "runs"
    entries = list_runs(runs_dir)
    if not entries:
        console.print("No saved runs in this directory yet.")
        return
    table = Table(title="Saved runs")
    table.add_column("#", style="dim")
    table.add_column("Task")
    table.add_column("Transcript")
    for i, (path, task) in enumerate(entries, 1):
        table.add_row(str(i), task, str(path))
    console.print(table)


@app.command()
def show(
    target: str | None = typer.Argument(
        None, help="Run index (from `crew runs`) or session id. Defaults to the most recent."
    ),
) -> None:
    """Print a saved run transcript, rendered as Markdown."""
    from rich.markdown import Markdown

    from .transcript import list_runs, resolve_run

    runs_dir = load_config().state_dir() / "runs"
    entries = list_runs(runs_dir)
    if not entries:
        console.print("No saved runs in this directory yet.")
        raise typer.Exit(code=1)
    path = resolve_run(entries, target)
    if path is None:
        console.print(f"[yellow]No run matches {target!r}.[/yellow] Try [bold]crew runs[/bold].")
        raise typer.Exit(code=1)
    console.print(Markdown(path.read_text()))


@app.command()
def agents() -> None:
    """List the crew members and the tools each one is allowed to use."""
    # Derive from ROLES (the single source of truth) so this can never drift from
    # the actual agent definitions. ROLES is plain data — no SDK import needed.
    from .agents import ROLES

    table = Table(title="The crew")
    table.add_column("Agent", style="bold cyan")
    table.add_column("Tools")
    table.add_column("Role")
    for role in ROLES:
        table.add_row(role.name, ", ".join(role.tools), role.description)
    console.print(table)


if __name__ == "__main__":
    app()
