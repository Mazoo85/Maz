"""Breadth-first crawl frontier with depth/page caps, dedup, and politeness.

:func:`crawl` walks the site starting from the recipe's ``start_urls``. For each
page it fetches (respecting robots.txt and rate limits), it yields a
:class:`PageResult` carrying the extracted records; the caller writes those out.
New URLs are discovered from the recipe's ``next_page`` (pagination) and ``links``
selectors, enqueued once, and bounded by ``max_depth`` / ``max_pages``.

The crawl is a generator so the runner stays in control of output and progress.
"""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass
from urllib.parse import urlsplit

from .config import ScraperConfig
from .fetch import Fetcher
from .parse import extract_links, extract_records
from .recipe import Recipe
from .robots import RobotsCache


@dataclass
class PageResult:
    """One fetched-and-parsed page."""

    url: str
    depth: int
    status_code: int
    records: list[dict]


@dataclass
class SkippedPage:
    """A page that was not fetched, with the reason (for logging)."""

    url: str
    depth: int
    reason: str  # "robots", "fetch-error"


def _same_host(a: str, b: str) -> bool:
    return urlsplit(a).netloc == urlsplit(b).netloc


def crawl(recipe: Recipe, config: ScraperConfig, fetcher: Fetcher):
    """Yield :class:`PageResult` / :class:`SkippedPage` for each visited URL.

    The generator honours ``max_depth`` and ``max_pages`` from the recipe (with
    config defaults) and never visits the same URL twice.
    """
    max_depth = recipe.effective_max_depth(config)
    max_pages = recipe.effective_max_pages(config)
    respect_robots = recipe.effective_respect_robots(config)
    robots = RobotsCache(fetcher.get_text, config.user_agent) if respect_robots else None

    seen: set[str] = set()
    queue: deque[tuple[str, int]] = deque()
    for url in recipe.start_urls:
        if url not in seen:
            seen.add(url)
            queue.append((url, 0))

    pages_fetched = 0
    while queue and pages_fetched < max_pages:
        url, depth = queue.popleft()

        if robots is not None and not robots.allowed(url):
            yield SkippedPage(url=url, depth=depth, reason="robots")
            continue

        result = fetcher.get(url)
        pages_fetched += 1
        if not result.ok:
            yield SkippedPage(url=url, depth=depth, reason="fetch-error")
            continue

        records = extract_records(result.text, result.url, recipe)
        yield PageResult(url=result.url, depth=depth, status_code=result.status_code, records=records)

        if depth >= max_depth:
            continue

        # Discover next URLs: pagination first, then general links.
        next_selectors = list(recipe.link_selectors)
        if recipe.next_page:
            next_selectors = [recipe.next_page, *next_selectors]
        for link in extract_links(result.text, result.url, next_selectors):
            if link in seen:
                continue
            if recipe.same_host_only and not _same_host(link, result.url):
                continue
            seen.add(link)
            queue.append((link, depth + 1))
