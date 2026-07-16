"""Loading and validating scrape *recipes*.

A recipe is a small YAML (or JSON) file that declares what to scrape and how,
without writing any code for the common case. Example::

    name: quotes
    start_urls: ["https://quotes.toscrape.com/page/1/"]
    record_selector: "div.quote"      # optional repeated container
    fields:
      text:   { selector: "span.text" }
      author: { selector: "small.author" }
      href:   { selector: "a", attr: "href", absolute: true }
    follow:
      next_page: "li.next > a"        # pagination (href followed)
      links: "div.quote a.tag"        # extra links to enqueue
    limits: { max_depth: 3, max_pages: 200 }
    output: { formats: ["jsonl", "csv", "sqlite"], dedup_key: "href" }
    respect_robots: true
    same_host_only: true
    hook: "quotes_custom"             # optional: scraper/hooks/quotes_custom.py

Anything omitted falls back to the run's :class:`~scraper.config.ScraperConfig`.
A malformed recipe raises :class:`RecipeError` with a clear message rather than
letting a raw traceback surface.
"""

from __future__ import annotations

import importlib
from dataclasses import dataclass, field
from pathlib import Path

import yaml

from .config import ScraperConfig

VALID_FORMATS = ("jsonl", "csv", "sqlite")


class RecipeError(ValueError):
    """Raised when a recipe file is missing required keys or is malformed."""


@dataclass(frozen=True)
class FieldSpec:
    """How to extract one field from a record's HTML node."""

    name: str
    selector: str
    attr: str | None = None  # pull this attribute; None => text content
    absolute: bool = False  # resolve the value against the page URL (for links)
    strip: bool = True  # trim surrounding whitespace on text values


@dataclass(frozen=True)
class Recipe:
    """A validated, ready-to-run scrape recipe."""

    name: str
    start_urls: tuple[str, ...]
    fields: tuple[FieldSpec, ...]
    record_selector: str | None = None
    next_page: str | None = None
    link_selectors: tuple[str, ...] = ()
    max_depth: int | None = None
    max_pages: int | None = None
    formats: tuple[str, ...] = ("jsonl",)
    dedup_key: str | None = None
    respect_robots: bool | None = None
    same_host_only: bool = True
    hook_name: str | None = None
    hook: object = field(default=None, compare=False)  # resolved callable

    # Bounds/politeness resolved against the run config (config supplies the
    # default; the recipe overrides when it sets a value).
    def effective_max_depth(self, config: ScraperConfig) -> int:
        return self.max_depth if self.max_depth is not None else config.max_depth

    def effective_max_pages(self, config: ScraperConfig) -> int:
        return self.max_pages if self.max_pages is not None else config.max_pages

    def effective_respect_robots(self, config: ScraperConfig) -> bool:
        return self.respect_robots if self.respect_robots is not None else config.respect_robots


def _as_list(value) -> list:
    if value is None:
        return []
    if isinstance(value, (list, tuple)):
        return list(value)
    return [value]


def _parse_fields(raw: dict) -> tuple[FieldSpec, ...]:
    if not isinstance(raw, dict) or not raw:
        raise RecipeError("recipe 'fields' must be a non-empty mapping of field -> spec")
    specs: list[FieldSpec] = []
    for name, spec in raw.items():
        if isinstance(spec, str):
            spec = {"selector": spec}
        if not isinstance(spec, dict) or "selector" not in spec:
            raise RecipeError(f"field {name!r} needs a 'selector' (got {spec!r})")
        specs.append(
            FieldSpec(
                name=str(name),
                selector=str(spec["selector"]),
                attr=spec.get("attr"),
                absolute=bool(spec.get("absolute", False)),
                strip=bool(spec.get("strip", True)),
            )
        )
    return tuple(specs)


def _resolve_hook(hook_name: str):
    """Import ``scraper.hooks.<hook_name>`` and return its ``extract`` callable."""
    try:
        module = importlib.import_module(f"scraper.hooks.{hook_name}")
    except ImportError as exc:
        raise RecipeError(f"hook {hook_name!r} could not be imported: {exc}") from exc
    extract = getattr(module, "extract", None)
    if not callable(extract):
        raise RecipeError(f"hook {hook_name!r} must define a callable extract(html, url)")
    return extract


def _load_raw(path: Path) -> dict:
    if not path.exists():
        raise RecipeError(f"recipe file not found: {path}")
    text = path.read_text()
    try:
        # YAML is a JSON superset, so this parses both .yml and .json recipes.
        data = yaml.safe_load(text)
    except yaml.YAMLError as exc:
        raise RecipeError(f"could not parse recipe {path.name}: {exc}") from exc
    if not isinstance(data, dict):
        raise RecipeError(f"recipe {path.name} must be a mapping at the top level")
    return data


def parse_recipe(data: dict) -> Recipe:
    """Build a :class:`Recipe` from an already-loaded mapping (validates it)."""
    name = data.get("name")
    if not name:
        raise RecipeError("recipe is missing a 'name'")

    start_urls = tuple(str(u) for u in _as_list(data.get("start_urls")))
    if not start_urls:
        raise RecipeError("recipe needs at least one entry in 'start_urls'")

    hook_name = data.get("hook")
    hook = _resolve_hook(str(hook_name)) if hook_name else None

    # With a hook, fields are optional (the hook does the extraction).
    fields_raw = data.get("fields")
    fields = _parse_fields(fields_raw) if fields_raw else ()
    if not fields and not hook:
        raise RecipeError("recipe needs 'fields' (or a 'hook' that extracts records)")

    follow = data.get("follow") or {}
    if not isinstance(follow, dict):
        raise RecipeError("recipe 'follow' must be a mapping")

    limits = data.get("limits") or {}
    output = data.get("output") or {}

    formats = tuple(str(f).lower() for f in _as_list(output.get("formats")) ) or ("jsonl",)
    bad = [f for f in formats if f not in VALID_FORMATS]
    if bad:
        raise RecipeError(f"unknown output format(s) {bad}; valid: {list(VALID_FORMATS)}")

    return Recipe(
        name=str(name),
        start_urls=start_urls,
        fields=fields,
        record_selector=data.get("record_selector"),
        next_page=follow.get("next_page"),
        link_selectors=tuple(str(s) for s in _as_list(follow.get("links"))),
        max_depth=_opt_int(limits.get("max_depth"), "limits.max_depth"),
        max_pages=_opt_int(limits.get("max_pages"), "limits.max_pages"),
        formats=formats,
        dedup_key=output.get("dedup_key"),
        respect_robots=data.get("respect_robots"),
        same_host_only=bool(data.get("same_host_only", True)),
        hook_name=str(hook_name) if hook_name else None,
        hook=hook,
    )


def _opt_int(value, label: str) -> int | None:
    if value is None:
        return None
    try:
        return int(value)
    except (TypeError, ValueError):
        raise RecipeError(f"{label} must be an integer (got {value!r})") from None


def load_recipe(path: str | Path) -> Recipe:
    """Load and validate a recipe from a YAML/JSON file."""
    return parse_recipe(_load_raw(Path(path)))
