"""``scrape`` command-line entry point.

    scrape run recipes/quotes.yml        # crawl + extract per a recipe
    scrape run quotes.yml --dry-run      # preview records from the first page
    scrape fetch URL --selector "h1"     # one-off probe while building a recipe
    scrape init                          # write a starter scraper.json
    scrape config                        # show the effective configuration

The heavy imports (httpx, selectolax) happen inside the commands so
``scrape --help`` and ``scrape --version`` work even before the runtime
dependencies are installed.
"""

from __future__ import annotations

from pathlib import Path

import typer
from rich.console import Console
from rich.table import Table

from .config import CONFIG_FILENAME, effective_values, load_config

app = typer.Typer(
    add_completion=False,
    help="Recipe-driven scraper for static HTML: crawl, extract, and save JSONL/CSV/SQLite.",
)
console = Console()


def _version_callback(value: bool) -> None:
    if value:
        from . import __version__

        # Use typer.echo (not the rich Console) so output is captured under test
        # runners and plain pipes.
        typer.echo(f"scrape {__version__}")
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
    """Recipe-driven scraper for static HTML pages."""


def _load_recipe_or_exit(recipe_path: str):
    from .recipe import RecipeError, load_recipe

    try:
        return load_recipe(recipe_path)
    except RecipeError as exc:
        console.print(f"[red]Recipe error:[/red] {exc}")
        raise typer.Exit(code=1)


@app.command()
def run(
    recipe_path: str = typer.Argument(..., metavar="RECIPE", help="Path to a YAML/JSON recipe file."),
    out: str | None = typer.Option(None, "--out", "-o", help="Output directory (overrides config)."),
    format: str | None = typer.Option(
        None, "--format", "-f", help="Comma-separated formats (jsonl,csv,sqlite); overrides the recipe."
    ),
    limit_pages: int | None = typer.Option(
        None, "--limit-pages", min=1, help="Cap pages fetched this run (overrides recipe/config)."
    ),
    dry_run: bool = typer.Option(
        False, "--dry-run", help="Fetch only the first page and print sample records; write nothing."
    ),
) -> None:
    """Run a scrape defined by RECIPE: crawl, extract, and save records."""
    from dataclasses import replace

    from .runner import preview, run as run_scrape

    recipe = _load_recipe_or_exit(recipe_path)
    config = load_config()
    if limit_pages is not None:
        config = replace(config, max_pages=limit_pages)
    formats = [f.strip() for f in format.split(",")] if format else None

    if dry_run:
        records = preview(recipe, config)
        if not records:
            console.print("[yellow]No records extracted from the first page.[/yellow] Check your selectors.")
            raise typer.Exit(code=1)
        console.print(f"[bold]Dry run:[/bold] {len(records)} sample record(s) from {recipe.start_urls[0]}\n")
        for i, rec in enumerate(records, 1):
            console.print(f"[dim]{i}.[/dim] {rec}")
        console.print("\n[dim]No files written (--dry-run).[/dim]")
        return

    console.print(f"[bold]Scraping:[/bold] {recipe.name}  (seeds: {len(recipe.start_urls)})\n")

    def on_page(event) -> None:
        reason = getattr(event, "reason", None)
        if reason:
            console.print(f"[yellow]skip[/yellow] {event.url} [dim]({reason})[/dim]")
        else:
            console.print(f"[green]ok[/green]   {event.url} [dim]({len(event.records)} rec, d{event.depth})[/dim]")

    stats = run_scrape(recipe, config, output_dir=out, formats=formats, on_page=on_page)

    console.print(
        f"\n[bold]Done.[/bold] {stats.pages_fetched} page(s), "
        f"{stats.records_written} record(s) written "
        f"([dim]{stats.records_found} found, {stats.pages_skipped} skipped[/dim])."
    )
    for path in stats.output_files:
        console.print(f"  [cyan]{path}[/cyan]")


@app.command()
def fetch(
    url: str = typer.Argument(..., help="URL to fetch once."),
    selector: str | None = typer.Option(
        None, "--selector", "-s", help="Print text of nodes matching this CSS selector."
    ),
) -> None:
    """Fetch a single URL — a quick probe for building a recipe."""
    from selectolax.parser import HTMLParser

    from .fetch import Fetcher

    config = load_config()
    with Fetcher(config) as fetcher:
        result = fetcher.get(url)
    if not result.ok:
        console.print(f"[red]Fetch failed[/red] ({result.status_code or 'no response'}): {url}")
        raise typer.Exit(code=1)

    if selector is None:
        console.print(f"[green]{result.status_code}[/green] {result.url}  [dim]({len(result.text)} bytes)[/dim]")
        return

    nodes = HTMLParser(result.text).css(selector)
    console.print(f"[bold]{len(nodes)} node(s)[/bold] match [cyan]{selector}[/cyan]:\n")
    for i, node in enumerate(nodes, 1):
        text = node.text(deep=True, separator=" ").strip()
        console.print(f"[dim]{i}.[/dim] {text}")


@app.command()
def init(
    force: bool = typer.Option(False, "--force", help="Overwrite an existing scraper.json."),
) -> None:
    """Write a starter scraper.json in the current directory."""
    from .config import write_starter_config

    written, path = write_starter_config(force=force)
    if written:
        console.print(
            f"[green]Wrote {path}[/green] — edit it to set politeness and crawl bounds, "
            "then run [bold]scrape config[/bold] to confirm."
        )
    else:
        console.print(f"[yellow]{path} already exists.[/yellow] Use --force to overwrite.")
        raise typer.Exit(code=1)


@app.command()
def config() -> None:
    """Show the effective configuration (defaults + scraper.json + env overrides)."""
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


if __name__ == "__main__":
    app()
