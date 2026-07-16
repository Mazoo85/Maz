"""Tie fetch -> crawl -> parse -> store together into one run.

:func:`run` drives a crawl for a recipe and writes every extracted record to the
configured output formats, returning a :class:`RunStats` summary. It is UI-free
so it can be exercised entirely offline in tests (inject a ``transport`` and a
no-op ``on_page`` callback); the CLI supplies a ``rich`` progress callback.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

import httpx

from .config import ScraperConfig
from .crawl import PageResult, SkippedPage, crawl
from .fetch import Fetcher
from .parse import extract_records
from .recipe import Recipe
from .store import open_writers


@dataclass
class RunStats:
    """Summary of a completed (or dry-run) scrape."""

    pages_fetched: int = 0
    pages_skipped: int = 0
    records_found: int = 0
    records_written: int = 0
    output_files: list[str] = field(default_factory=list)
    skipped: list[str] = field(default_factory=list)  # "url (reason)"


def run(
    recipe: Recipe,
    config: ScraperConfig,
    *,
    output_dir: str | Path | None = None,
    formats=None,
    transport: httpx.BaseTransport | None = None,
    on_page=None,
) -> RunStats:
    """Run a full scrape for ``recipe`` and write records to disk.

    ``output_dir`` / ``formats`` override the config + recipe defaults (used by
    the CLI's ``--out`` / ``--format``). ``on_page`` is called with each
    :class:`PageResult` / :class:`SkippedPage` for progress reporting.
    """
    out_dir = Path(output_dir) if output_dir is not None else config.output_path()
    use_formats = tuple(formats) if formats else recipe.formats
    stats = RunStats()

    writers = open_writers(use_formats, out_dir, recipe.name, dedup_key=recipe.dedup_key)
    try:
        with Fetcher(config, transport=transport) as fetcher:
            for event in crawl(recipe, config, fetcher):
                if on_page is not None:
                    on_page(event)
                if isinstance(event, SkippedPage):
                    stats.pages_skipped += 1
                    stats.skipped.append(f"{event.url} ({event.reason})")
                    continue
                assert isinstance(event, PageResult)
                stats.pages_fetched += 1
                stats.records_found += len(event.records)
                for record in event.records:
                    if writers.write(record):
                        stats.records_written += 1
    finally:
        writers.close()

    from .store import _FORMATS  # local import: keep the module surface small

    stats.output_files = [str(out_dir / f"{recipe.name}.{_FORMATS[f][1]}") for f in use_formats]
    return stats


def preview(
    recipe: Recipe,
    config: ScraperConfig,
    *,
    transport: httpx.BaseTransport | None = None,
    limit: int = 5,
) -> list[dict]:
    """Fetch only the first seed URL and return up to ``limit`` sample records.

    Backs ``scrape run --dry-run``: shows what the recipe extracts without
    writing anything or crawling further.
    """
    if not recipe.start_urls:
        return []
    with Fetcher(config, transport=transport) as fetcher:
        result = fetcher.get(recipe.start_urls[0])
        if not result.ok:
            return []
        records = extract_records(result.text, result.url, recipe)
    return records[:limit]
