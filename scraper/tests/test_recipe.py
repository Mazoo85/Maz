"""Recipe loading and validation."""

import json

import pytest

from scraper.config import ScraperConfig
from scraper.recipe import RecipeError, load_recipe, parse_recipe

BASE = {
    "name": "quotes",
    "start_urls": ["https://quotes.test/page/1/"],
    "record_selector": "div.quote",
    "fields": {
        "text": {"selector": "span.text"},
        "author": {"selector": "small.author"},
        "href": {"selector": "a.detail", "attr": "href", "absolute": True},
    },
    "follow": {"next_page": "a.next", "links": ["a.tag"]},
    "limits": {"max_depth": 2, "max_pages": 50},
    "output": {"formats": ["jsonl", "csv"], "dedup_key": "href"},
}


def test_parse_full_recipe():
    r = parse_recipe(BASE)
    assert r.name == "quotes"
    assert r.start_urls == ("https://quotes.test/page/1/",)
    assert r.record_selector == "div.quote"
    assert len(r.fields) == 3
    assert r.next_page == "a.next"
    assert r.link_selectors == ("a.tag",)
    assert r.max_depth == 2
    assert r.formats == ("jsonl", "csv")
    assert r.dedup_key == "href"


def test_field_shorthand_string_selector():
    r = parse_recipe({"name": "x", "start_urls": ["https://a.test/"], "fields": {"title": "h1"}})
    assert r.fields[0].name == "title"
    assert r.fields[0].selector == "h1"


def test_effective_bounds_fall_back_to_config():
    cfg = ScraperConfig(max_depth=7, max_pages=999)
    r = parse_recipe({"name": "x", "start_urls": ["https://a.test/"], "fields": {"t": "h1"}})
    assert r.effective_max_depth(cfg) == 7
    assert r.effective_max_pages(cfg) == 999
    # A recipe value overrides the config default.
    r2 = parse_recipe({**BASE})
    assert r2.effective_max_depth(cfg) == 2


def test_missing_name_raises():
    with pytest.raises(RecipeError, match="name"):
        parse_recipe({"start_urls": ["https://a.test/"], "fields": {"t": "h1"}})


def test_missing_start_urls_raises():
    with pytest.raises(RecipeError, match="start_urls"):
        parse_recipe({"name": "x", "fields": {"t": "h1"}})


def test_missing_fields_and_hook_raises():
    with pytest.raises(RecipeError, match="fields"):
        parse_recipe({"name": "x", "start_urls": ["https://a.test/"]})


def test_bad_format_raises():
    with pytest.raises(RecipeError, match="format"):
        parse_recipe({**BASE, "output": {"formats": ["xml"]}})


def test_field_without_selector_raises():
    with pytest.raises(RecipeError, match="selector"):
        parse_recipe({"name": "x", "start_urls": ["https://a.test/"], "fields": {"t": {"attr": "href"}}})


def test_load_recipe_yaml(tmp_path):
    p = tmp_path / "r.yml"
    p.write_text(
        "name: y\n"
        "start_urls: [https://a.test/]\n"
        "fields:\n"
        "  title: h1\n"
    )
    r = load_recipe(p)
    assert r.name == "y"
    assert r.fields[0].selector == "h1"


def test_load_recipe_json(tmp_path):
    p = tmp_path / "r.json"
    p.write_text(json.dumps({"name": "j", "start_urls": ["https://a.test/"], "fields": {"t": "h1"}}))
    assert load_recipe(p).name == "j"


def test_load_missing_file_raises(tmp_path):
    with pytest.raises(RecipeError, match="not found"):
        load_recipe(tmp_path / "nope.yml")


def test_hook_allows_missing_fields(tmp_path, monkeypatch):
    # A recipe with a hook needs no 'fields'. Point the hook at a temp module.
    import sys
    import types

    mod = types.ModuleType("scraper.hooks.tmp_hook")
    mod.extract = lambda html, url: [{"ok": True}]
    monkeypatch.setitem(sys.modules, "scraper.hooks.tmp_hook", mod)
    r = parse_recipe({"name": "h", "start_urls": ["https://a.test/"], "hook": "tmp_hook"})
    assert r.hook_name == "tmp_hook"
    assert callable(r.hook)


def test_unknown_hook_raises():
    with pytest.raises(RecipeError, match="hook"):
        parse_recipe({"name": "h", "start_urls": ["https://a.test/"], "hook": "does_not_exist"})
