"""Shared test fixtures. The whole suite runs offline — no real network.

``mock_transport`` builds an ``httpx.MockTransport`` from a ``{url: html}`` map
so :class:`~scraper.fetch.Fetcher` can be driven against in-memory fixture pages.
``fixtures_dir`` points at ``tests/fixtures`` for the saved HTML pages.
"""

from __future__ import annotations

from pathlib import Path

import httpx
import pytest

FIXTURES = Path(__file__).parent / "fixtures"


@pytest.fixture
def fixtures_dir() -> Path:
    return FIXTURES


def _load_fixture_pages() -> dict[str, str]:
    """Map the 3 paginated quote fixtures to their served URLs."""
    return {
        "https://quotes.test/page/1/": (FIXTURES / "page1.html").read_text(),
        "https://quotes.test/page/2/": (FIXTURES / "page2.html").read_text(),
        "https://quotes.test/page/3/": (FIXTURES / "page3.html").read_text(),
        "https://quotes.test/robots.txt": "User-agent: *\nAllow: /\n",
    }


@pytest.fixture
def fixture_pages() -> dict[str, str]:
    return _load_fixture_pages()


def make_transport(pages: dict[str, str], *, status: int = 200) -> httpx.MockTransport:
    """Build a MockTransport that serves ``pages`` and 404s everything else."""

    def handler(request: httpx.Request) -> httpx.Response:
        url = str(request.url)
        if url in pages:
            return httpx.Response(status, text=pages[url])
        return httpx.Response(404, text="not found")

    return httpx.MockTransport(handler)


@pytest.fixture
def mock_transport(fixture_pages) -> httpx.MockTransport:
    return make_transport(fixture_pages)
