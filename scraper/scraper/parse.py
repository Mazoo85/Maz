"""Turn HTML into records, either declaratively (CSS selectors) or via a hook.

The declarative path uses :mod:`selectolax` (fast CSS-selector parsing):

* If the recipe sets ``record_selector``, each matching node becomes one record
  and every field selector is evaluated *within* that node.
* Otherwise the whole document is one record and field selectors run against it.

If the recipe carries a resolved ``hook`` callable, that runs instead and its
returned list of dicts is used verbatim.
"""

from __future__ import annotations

from urllib.parse import urljoin

from selectolax.parser import HTMLParser, Node

from .recipe import FieldSpec, Recipe


def _extract_field(node: Node, spec: FieldSpec, base_url: str):
    """Evaluate one field selector against ``node`` — returns str or None."""
    found = node.css_first(spec.selector)
    if found is None:
        return None
    if spec.attr is not None:
        value = found.attributes.get(spec.attr)
        if value is None:
            return None
    else:
        value = found.text(deep=True, separator=" ")
    if value is not None and spec.strip:
        value = value.strip()
    if value and spec.absolute:
        value = urljoin(base_url, value)
    return value


def _record_from_node(node: Node, recipe: Recipe, base_url: str) -> dict:
    return {spec.name: _extract_field(node, spec, base_url) for spec in recipe.fields}


def extract_records(html: str, url: str, recipe: Recipe) -> list[dict]:
    """Extract all records from ``html`` (fetched from ``url``) per ``recipe``."""
    if recipe.hook is not None:
        records = recipe.hook(html, url)
        return list(records) if records else []

    tree = HTMLParser(html)
    if recipe.record_selector:
        nodes = tree.css(recipe.record_selector)
        return [_record_from_node(node, recipe, url) for node in nodes]
    # No repeated container: the document itself is a single record.
    return [_record_from_node(tree.root, recipe, url)]


def extract_links(html: str, url: str, selectors) -> list[str]:
    """Collect absolute hrefs for every ``<a>`` matched by ``selectors``.

    Used for both pagination (a single ``next_page`` selector) and general link
    following. Returns absolute URLs, de-duplicated in document order.
    """
    if not selectors:
        return []
    if isinstance(selectors, str):
        selectors = [selectors]

    tree = HTMLParser(html)
    seen: set[str] = set()
    out: list[str] = []
    for selector in selectors:
        for node in tree.css(selector):
            href = node.attributes.get("href")
            if not href:
                continue
            absolute = urljoin(url, href.strip())
            if absolute not in seen:
                seen.add(absolute)
                out.append(absolute)
    return out
