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
from rich.table import Table

from dataclasses import replace

from . import session as session_mod
from .config import load_config

app = typer.Typer(
    add_completion=False,
    help="Orchestrate a team of AI coding agents (planner, coder, reviewer, tester).",
)
console = Console()


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


# Shared option definitions so `do` and `resume` stay consistent.
_YES_OPT = typer.Option(
    False, "--yes", "-y", help="Non-interactive: auto-approve every checkpoint. Use with care."
)
_ROUNDS_OPT = typer.Option(
    None, "--max-fix-rounds", min=0, help="Override the coder<->tester repair-round limit."
)


@app.command()
def do(
    task: str = typer.Argument(..., help="The task to hand to the crew, in plain English."),
    yes: bool = _YES_OPT,
    max_fix_rounds: int | None = _ROUNDS_OPT,
) -> None:
    """Run TASK through the full crew: plan -> code -> review -> test, with checkpoints."""
    from .orchestrator import run_task_sync

    config = _resolve_config(max_fix_rounds)
    confirm = _auto_confirm if yes else _interactive_confirm
    console.print(f"[bold]Task:[/bold] {task}\n")
    if yes:
        console.print("[yellow]Running unattended (--yes): all checkpoints auto-approved.[/yellow]\n")
    try:
        run_task_sync(task, config, confirm=confirm)
    except ModuleNotFoundError as exc:  # SDK not installed
        console.print(
            f"[red]Missing dependency:[/red] {exc}.\n"
            "Install with:  pip install -e .   (needs claude-agent-sdk)"
        )
        raise typer.Exit(code=1)


@app.command()
def resume(
    yes: bool = _YES_OPT,
    max_fix_rounds: int | None = _ROUNDS_OPT,
) -> None:
    """Continue the last task in this directory using its saved session."""
    from .orchestrator import run_task_sync

    config = _resolve_config(max_fix_rounds)
    confirm = _auto_confirm if yes else _interactive_confirm
    state = session_mod.load(config)
    if not state.session_id or not state.task:
        console.print("[yellow]No saved session found in this directory. Start one with `crew do`.[/yellow]")
        raise typer.Exit(code=1)
    console.print(f"[bold]Resuming:[/bold] {state.task}  (last phase: {state.phase})\n")
    run_task_sync(state.task, config, confirm=confirm, resume_session_id=state.session_id)


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
    console.print(table)


@app.command()
def agents() -> None:
    """List the crew members and the tools each one is allowed to use."""
    # Static description so this works without the SDK installed.
    rows = [
        ("planner", "Read, Glob, Grep", "Reads the codebase, writes the plan (read-only)"),
        ("coder", "Read, Edit, Write, Bash, Grep", "Implements the plan (only agent that edits)"),
        ("reviewer", "Read, Glob, Grep", "Reviews the diff for bugs/security (read-only)"),
        ("tester", "Bash, Read, Grep, Glob", "Runs tests and linters (read-only on code)"),
    ]
    table = Table(title="The crew")
    table.add_column("Agent", style="bold cyan")
    table.add_column("Tools")
    table.add_column("Role")
    for name, tools, role in rows:
        table.add_row(name, tools, role)
    console.print(table)


if __name__ == "__main__":
    app()
