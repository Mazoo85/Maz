"""Crawl frontier: pagination following, caps, dedup, robots, same-host."""

import httpx

from conftest import make_transport
from scraper.config import ScraperConfig
from scraper.crawl import PageResult, SkippedPage, crawl
from scraper.fetch import Fetcher
from scraper.recipe import parse_recipe

RECIPE = parse_recipe(
    {
        "name": "quotes",
        "start_urls": ["https://quotes.test/page/1/"],
        "record_selector": "div.quote",
        "fields": {
            "text": {"selector": "span.text"},
            "href": {"selector": "a.detail", "attr": "href", "absolute": True},
        },
        "follow": {"next_page": "a.next"},
        "output": {"dedup_key": "href"},
    }
)

CFG = ScraperConfig(rate_limit_per_host=0)


def _crawl(recipe, cfg, pages):
    transport = make_transport(pages)
    with Fetcher(cfg, transport=transport) as f:
        return list(crawl(recipe, cfg, f))


def test_follows_pagination_across_all_pages(fixture_pages):
    events = _crawl(RECIPE, CFG, fixture_pages)
    pages = [e for e in events if isinstance(e, PageResult)]
    urls = [p.url for p in pages]
    assert urls == [
        "https://quotes.test/page/1/",
        "https://quotes.test/page/2/",
        "https://quotes.test/page/3/",
    ]
    # 2 + 2 + 1 quotes across the three fixture pages.
    assert sum(len(p.records) for p in pages) == 5


def test_max_pages_cap(fixture_pages):
    cfg = ScraperConfig(rate_limit_per_host=0, max_pages=2)
    pages = [e for e in _crawl(RECIPE, cfg, fixture_pages) if isinstance(e, PageResult)]
    assert len(pages) == 2


def test_max_depth_cap(fixture_pages):
    # depth 0 => only the seed page is fetched (no pagination followed).
    cfg = ScraperConfig(rate_limit_per_host=0, max_depth=0)
    pages = [e for e in _crawl(RECIPE, cfg, fixture_pages) if isinstance(e, PageResult)]
    assert [p.url for p in pages] == ["https://quotes.test/page/1/"]


def test_no_url_visited_twice(fixture_pages):
    # Add a back-link from page 2 to page 1; page 1 must not be refetched.
    pages = dict(fixture_pages)
    pages["https://quotes.test/page/2/"] = pages["https://quotes.test/page/2/"].replace(
        "</nav>", '<a class="next" href="/page/1/">back</a></nav>'
    )
    fetched = [e.url for e in _crawl(RECIPE, CFG, pages) if isinstance(e, PageResult)]
    assert fetched.count("https://quotes.test/page/1/") == 1


def test_robots_disallow_skips_page():
    pages = {
        "https://quotes.test/page/1/": '<div class="quote"><span class="text">hi</span>'
        '<a class="detail" href="/q/1">d</a></div>',
        "https://quotes.test/robots.txt": "User-agent: *\nDisallow: /\n",
    }
    events = _crawl(RECIPE, ScraperConfig(rate_limit_per_host=0), pages)
    assert len(events) == 1
    assert isinstance(events[0], SkippedPage)
    assert events[0].reason == "robots"


def test_respect_robots_off_ignores_disallow():
    recipe = parse_recipe(
        {
            "name": "q",
            "start_urls": ["https://quotes.test/page/1/"],
            "record_selector": "div.quote",
            "fields": {"text": {"selector": "span.text"}},
            "respect_robots": False,
        }
    )
    pages = {
        "https://quotes.test/page/1/": '<div class="quote"><span class="text">hi</span></div>',
        "https://quotes.test/robots.txt": "User-agent: *\nDisallow: /\n",
    }
    events = _crawl(recipe, ScraperConfig(rate_limit_per_host=0), pages)
    assert any(isinstance(e, PageResult) for e in events)


def test_fetch_error_is_skipped():
    events = _crawl(RECIPE, ScraperConfig(rate_limit_per_host=0, max_retries=0), {
        # robots allows all (missing robots.txt -> 404 -> allow), page 1 404s.
    })
    assert len(events) == 1
    assert isinstance(events[0], SkippedPage)
    assert events[0].reason == "fetch-error"


def test_same_host_only_blocks_offsite_links():
    recipe = parse_recipe(
        {
            "name": "q",
            "start_urls": ["https://quotes.test/page/1/"],
            "record_selector": "div.quote",
            "fields": {"text": {"selector": "span.text"}},
            "follow": {"links": "a.out"},
            "same_host_only": True,
        }
    )
    pages = {
        "https://quotes.test/page/1/": '<div class="quote"><span class="text">hi</span></div>'
        '<a class="out" href="https://other.test/x">off</a>',
        "https://quotes.test/robots.txt": "User-agent: *\nAllow: /\n",
        "https://other.test/x": '<div class="quote"><span class="text">nope</span></div>',
        "https://other.test/robots.txt": "User-agent: *\nAllow: /\n",
    }
    fetched = [e.url for e in _crawl(recipe, ScraperConfig(rate_limit_per_host=0), pages) if isinstance(e, PageResult)]
    assert fetched == ["https://quotes.test/page/1/"]  # offsite link not followed
