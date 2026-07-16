"""Declarative extraction and link discovery."""

from scraper.parse import extract_links, extract_records
from scraper.recipe import parse_recipe

HTML = """
<html><body>
  <div class="quote">
    <span class="text">  Hello   world  </span>
    <small class="author">Amy</small>
    <a class="detail" href="/quote/1/">details</a>
  </div>
  <div class="quote">
    <span class="text">Second</span>
    <small class="author">Bob</small>
    <a class="detail" href="/quote/2/">details</a>
  </div>
  <nav><a class="next" href="/page/2/">next</a></nav>
</body></html>
"""

RECIPE = parse_recipe(
    {
        "name": "q",
        "start_urls": ["https://x.test/page/1/"],
        "record_selector": "div.quote",
        "fields": {
            "text": {"selector": "span.text"},
            "author": {"selector": "small.author"},
            "href": {"selector": "a.detail", "attr": "href", "absolute": True},
        },
        "follow": {"next_page": "a.next"},
    }
)


def test_extract_records_with_container():
    records = extract_records(HTML, "https://x.test/page/1/", RECIPE)
    assert len(records) == 2
    assert records[0]["text"] == "Hello   world"  # surrounding whitespace stripped
    assert records[0]["author"] == "Amy"
    assert records[0]["href"] == "https://x.test/quote/1/"  # made absolute
    assert records[1]["text"] == "Second"


def test_missing_selector_yields_none():
    recipe = parse_recipe(
        {
            "name": "q",
            "start_urls": ["https://x.test/"],
            "record_selector": "div.quote",
            "fields": {"missing": {"selector": ".nope"}},
        }
    )
    records = extract_records(HTML, "https://x.test/", recipe)
    assert records[0]["missing"] is None


def test_whole_document_single_record():
    recipe = parse_recipe(
        {"name": "q", "start_urls": ["https://x.test/"], "fields": {"heading": {"selector": "small.author"}}}
    )
    records = extract_records(HTML, "https://x.test/", recipe)
    assert len(records) == 1
    assert records[0]["heading"] == "Amy"  # first match on the page


def test_extract_links_absolute_and_deduped():
    html = '<a class="l" href="/a">a</a><a class="l" href="/b">b</a><a class="l" href="/a">dup</a>'
    links = extract_links(html, "https://x.test/here/", "a.l")
    assert links == ["https://x.test/a", "https://x.test/b"]


def test_extract_links_empty_selectors():
    assert extract_links(HTML, "https://x.test/", []) == []


def test_extract_records_via_hook():
    recipe = parse_recipe({"name": "q", "start_urls": ["https://x.test/"], "fields": {"t": "h1"}})
    # Swap in a hook callable directly (bypassing module import).
    object.__setattr__(recipe, "hook", lambda html, url: [{"from_hook": url}])
    records = extract_records("<html></html>", "https://x.test/p", recipe)
    assert records == [{"from_hook": "https://x.test/p"}]
