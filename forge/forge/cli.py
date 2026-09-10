"""``forge`` command-line entry point.

    forge init                  # write a starter forge.json
    forge config                # show the leash currently in force
    forge sense                 # gather signals into forge/state/pulse.json
    forge decide                # score them into forge/state/tonight.json
    forge run --dry-run         # sense + decide + ledger line, change nothing
    forge run --live            # the full cycle (week 2 onward)
    forge ledger                # read the record back

`--dry-run` is the default and deliberately so: a night that changes nothing is
the safe default, and week one runs in exactly this mode.
"""

from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path

import typer
from rich.console import Console
from rich.table import Table

from . import __version__, ledger as ledger_mod
from .config import default_config_dict, load_config, write_starter_config
from .decide import decide as decide_step
from .decide import read_tonight, write_tonight
from .sense import read_pulse, sense as sense_step, write_pulse

app = typer.Typer(add_completion=False, help="The Forge: the Maz repo's nightly self-improvement loop.")
console = Console()

_ROOT_OPT = typer.Option(None, "--root", help="Repo root to operate on (default: current directory).")


def _root(value: str | None) -> Path:
    return Path(value) if value else Path.cwd()


def _version_callback(value: bool) -> None:
    if value:
        typer.echo(f"forge {__version__}")
        raise typer.Exit()


@app.callback()
def main(
    version: bool = typer.Option(False, "--version", callback=_version_callback, is_eager=True,
                                 help="Show the version and exit."),
) -> None:
    """The Forge: the Maz repo's nightly self-improvement loop."""


def _run_id() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%d")


def dry_run(root: Path, collectors: dict | None = None) -> dict:
    """Sense, decide, write the ledger line — and change nothing else.

    Returned as a plain callable (not only a command) so it can be tested and
    scheduled without going through a terminal.
    """
    root = Path(root)
    config = load_config(root)

    pulse = sense_step(root, config, collectors=collectors)
    write_pulse(pulse, root, config)

    record = decide_step(
        pulse,
        config,
        strikes=ledger_mod.strikes(root, config),
        recent_zones=ledger_mod.recent_zones(root, config),
    )
    write_tonight(record, root, config)

    chosen = record.get("chosen")
    entry = ledger_mod.new_entry(
        _run_id(),
        outcome="dry_run",
        chose=(chosen or {}).get("candidate", {}).get("task", ""),
        source=(chosen or {}).get("candidate", {}).get("source", ""),
        kind=(chosen or {}).get("candidate", {}).get("kind", ""),
        candidate_key=(chosen or {}).get("candidate", {}).get("key", ""),
        zone=(chosen or {}).get("zone", ""),
        why={
            "value": (chosen or {}).get("value"),
            "confidence": (chosen or {}).get("confidence"),
            "risk": (chosen or {}).get("risk"),
            "score": (chosen or {}).get("score", 0),
            "considered": record.get("considered", 0),
            "skipped": record.get("skipped", {}),
            "runners_up": record.get("runners_up", []),
        },
        notes="dry run: nothing was changed",
    )
    ledger_mod.append(entry, root, config)
    return record


@app.command()
def init(root: str | None = _ROOT_OPT) -> None:
    """Write a starter forge.json. Won't overwrite an existing one."""
    written, path = write_starter_config(root=_root(root))
    if written:
        console.print(f"[green]Wrote[/green] {path}")
    else:
        console.print(f"[yellow]{path} already exists — left alone.[/yellow]")


@app.command()
def config(root: str | None = _ROOT_OPT) -> None:
    """Show the leash currently in force."""
    cfg = load_config(_root(root))
    table = Table(show_header=True, header_style="bold")
    table.add_column("setting")
    table.add_column("value")
    for key in default_config_dict():
        value = getattr(cfg, key)
        table.add_row(key, json.dumps(list(value) if isinstance(value, tuple) else value))
    console.print(table)


@app.command()
def sense(root: str | None = _ROOT_OPT) -> None:
    """Gather signals into forge/state/pulse.json."""
    r = _root(root)
    cfg = load_config(r)
    pulse = sense_step(r, cfg)
    path = write_pulse(pulse, r, cfg)
    for name, info in pulse["sources"].items():
        mark = "[green]ok[/green]" if info["ok"] else f"[red]failed[/red] ({info['error']})"
        console.print(f"  {name:<8} {info['count']:>4}  {mark}")
    console.print(f"[dim]{path}[/dim]")


@app.command()
def decide(root: str | None = _ROOT_OPT) -> None:
    """Score the latest pulse into forge/state/tonight.json."""
    r = _root(root)
    cfg = load_config(r)
    pulse = read_pulse(r, cfg)
    if not pulse:
        console.print("[yellow]No pulse found. Run [bold]forge sense[/bold] first.[/yellow]")
        raise typer.Exit(code=1)
    record = decide_step(pulse, cfg, strikes=ledger_mod.strikes(r, cfg),
                         recent_zones=ledger_mod.recent_zones(r, cfg))
    write_tonight(record, r, cfg)
    _print_pick(record)


def _print_pick(record: dict) -> None:
    chosen = record.get("chosen")
    if not chosen:
        console.print(
            f"[yellow]Nothing worth doing.[/yellow] "
            f"considered {record.get('considered', 0)}, skipped {record.get('skipped')}"
        )
        return
    c = chosen["candidate"]
    console.print(f"[bold]{c['task']}[/bold]")
    console.print(
        f"  [dim]{c['source']} · zone {chosen['zone']} · score {chosen['score']} "
        f"(value {chosen['value']} x confidence {chosen['confidence']} / risk {chosen['risk']})[/dim]"
    )
    for r in record.get("runners_up", []):
        console.print(f"  [dim]runner-up: {r['task'][:60]} ({r['score']})[/dim]")


@app.command()
def run(
    root: str | None = _ROOT_OPT,
    live: bool = typer.Option(
        False, "--live/--dry-run",
        help="Live runs hand the pick to Crew and open a draft PR. Default is a dry run.",
    ),
) -> None:
    """Run one cycle. Dry by default — sense, decide, record, change nothing."""
    r = _root(root)
    if not live:
        record = dry_run(r)
        _print_pick(record)
        console.print("[dim]dry run — nothing was changed[/dim]")
        return

    from .orchestrate import live_run  # imported lazily: live needs git and Crew

    entry = live_run(r)
    console.print(f"[bold]{entry['outcome']}[/bold] — {entry['notes']}")


@app.command()
def ledger(root: str | None = _ROOT_OPT, limit: int = typer.Option(10, help="How many entries to show.")) -> None:
    """Read the record back, newest last."""
    r = _root(root)
    cfg = load_config(r)
    entries = ledger_mod.read_all(r, cfg)[-limit:]
    if not entries:
        console.print("[yellow]The ledger is empty.[/yellow]")
        return
    table = Table(show_header=True, header_style="bold")
    for col in ("run", "outcome", "zone", "chose", "score", "merged"):
        table.add_column(col)
    for e in entries:
        table.add_row(
            str(e.get("run_id", "")),
            str(e.get("outcome", "")),
            str(e.get("zone", "")),
            str(e.get("chose", ""))[:44],
            str((e.get("why") or {}).get("score", "")),
            "" if e.get("merged") is None else str(e.get("merged")),
        )
    console.print(table)


if __name__ == "__main__":
    app()
