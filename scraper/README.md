# maz-scrape

A general-purpose, **recipe-driven scraper for static HTML pages**. Point it at a
YAML recipe that maps field names to CSS selectors, and it crawls the site
(following pagination and links), extracts records, and writes them to **JSONL,
CSV, and SQLite** — politely, by default.

It's a sibling to [`crew/`](../crew) and follows the same conventions: a `typer`
CLI, a JSON project config with env overrides, and an offline `pytest` suite.

## Install

```bash
cd scraper
pip install -e .
```

## Quick start

```bash
# Preview what a recipe extracts from its first page — writes nothing:
scrape run recipes/example_quotes.yml --dry-run

# Run the full crawl; results land under ./scraped/quotes.{jsonl,csv,db}:
scrape run recipes/example_quotes.yml

# Probe a page while writing a recipe — print text of nodes matching a selector:
scrape fetch https://quotes.toscrape.com/page/1/ --selector "span.text"
```

## Recipes

A recipe declares *what* to scrape and *how* — no code needed for the common case:

```yaml
name: quotes
start_urls: ["https://quotes.toscrape.com/page/1/"]
record_selector: "div.quote"       # optional: one record per matching node
fields:
  text:   { selector: "span.text" }
  author: { selector: "small.author" }
  href:   { selector: "a", attr: "href", absolute: true }
follow:
  next_page: "li.next > a"         # pagination (href followed)
  links: ["a.tag"]                 # extra link selectors to enqueue
limits: { max_depth: 3, max_pages: 200 }
output: { formats: ["jsonl", "csv", "sqlite"], dedup_key: "text" }
respect_robots: true               # default; set false to override per-site
same_host_only: true               # default; don't wander off the seed host
```

Field spec keys: `selector` (required), `attr` (pull an attribute instead of
text), `absolute` (resolve a URL against the page), `strip` (trim ends, default
`true`). A bare string (`text: span.text`) is shorthand for `{ selector: ... }`.

### Python hooks (for messy pages)

When selectors aren't enough, add a module under `scraper/hooks/` and name it
from the recipe:

```python
# scraper/hooks/my_site.py
def extract(html: str, url: str) -> list[dict]:
    ...  # return a list of record dicts
```

```yaml
name: my_site
start_urls: ["https://example.com/"]
hook: my_site      # runs instead of `fields`
```

## Output formats

- **jsonl** — one JSON object per line.
- **csv** — columns fixed from the first record's keys.
- **sqlite** — a `records` table; with a `dedup_key`, a `UNIQUE` index +
  `INSERT OR IGNORE` make re-runs **incremental** (duplicates are skipped, not
  re-appended).

## Configuration

Politeness and crawl bounds come from (lowest to highest precedence): built-in
defaults < a project `scraper.json` < `SCRAPER_*` env vars.

```bash
scrape init      # write a starter scraper.json
scrape config    # show the effective configuration
```

| Setting | Default | Meaning |
|---|---|---|
| `user_agent` | `maz-scrape/0.1 (…)` | descriptive UA sent with every request |
| `request_timeout` | `20.0` | per-request timeout (seconds) |
| `max_retries` | `3` | retries on transient network / 5xx errors |
| `retry_backoff` | `1.0` | base backoff seconds (doubles each retry) |
| `rate_limit_per_host` | `2.0` | max requests/sec to any one host |
| `respect_robots` | `true` | honour each host's `robots.txt` |
| `max_depth` | `3` | link-following depth from the seeds |
| `max_pages` | `200` | hard cap on pages fetched per run |
| `output_dir` | `scraped` | where output files are written |

## Tests

```bash
cd scraper
pytest -q
```

The suite is fully offline — it drives the fetcher with `httpx.MockTransport`
and local HTML fixtures, so it never touches the network.

## Scope

v1 handles **static HTML** only. JS-rendered sites (Playwright/Chromium) are a
natural next step behind the same recipe interface.
