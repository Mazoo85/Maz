"""CLI surface: run (real crawl + dry-run), fetch, init, config."""

import json

import httpx
import pytest
from typer.testing import CliRunner

import scraper.fetch as fetch_mod
from conftest import make_transport
from scraper.cli import app

runner = CliRunner()


@pytest.fixture
def patch_transport(monkeypatch, fixture_pages):
    """Route every Fetcher through the fixture pages (offline CLI runs)."""
    transport = make_transport(fixture_pages)
    real_init = fetch_mod.Fetcher.__init__

    def init(self, config, *, transport=None, **kw):
        real_init(self, config, transport=transport or make_transport(fixture_pages), **kw)

    monkeypatch.setattr(fetch_mod.Fetcher, "__init__", init)
    return transport


def _recipe_file(tmp_path):
    p = tmp_path / "quotes.yml"
    p.write_text(
        "name: quotes\n"
        "start_urls: [https://quotes.test/page/1/]\n"
        "record_selector: div.quote\n"
        "fields:\n"
        "  text: span.text\n"
        "  href: { selector: 'a.detail', attr: href, absolute: true }\n"
        "follow: { next_page: a.next }\n"
        "output: { formats: [jsonl, csv, sqlite], dedup_key: href }\n"
    )
    return p


def test_help_works():
    result = runner.invoke(app, ["--help"])
    assert result.exit_code == 0
    assert "run" in result.stdout
    assert "fetch" in result.stdout


def test_init_and_config(tmp_path, monkeypatch):
    monkeypatch.chdir(tmp_path)
    r = runner.invoke(app, ["init"])
    assert r.exit_code == 0
    assert (tmp_path / "scraper.json").exists()
    # Second init without --force fails cleanly.
    assert runner.invoke(app, ["init"]).exit_code == 1
    # config reflects a value set in the file.
    (tmp_path / "scraper.json").write_text(json.dumps({"max_pages": 7}))
    out = runner.invoke(app, ["config"])
    assert out.exit_code == 0
    assert "max_pages" in out.stdout
    assert "7" in out.stdout


def test_run_writes_outputs(tmp_path, monkeypatch, patch_transport):
    monkeypatch.chdir(tmp_path)
    recipe = _recipe_file(tmp_path)
    out_dir = tmp_path / "out"
    result = runner.invoke(app, ["run", str(recipe), "--out", str(out_dir)])
    assert result.exit_code == 0, result.stdout
    lines = (out_dir / "quotes.jsonl").read_text().splitlines()
    # 5 quotes across 3 pages, deduped on href to 4 unique.
    hrefs = [json.loads(line)["href"] for line in lines]
    assert len(hrefs) == 4
    assert (out_dir / "quotes.csv").exists()
    assert (out_dir / "quotes.db").exists()


def test_run_dry_run_writes_nothing(tmp_path, monkeypatch, patch_transport):
    monkeypatch.chdir(tmp_path)
    recipe = _recipe_file(tmp_path)
    result = runner.invoke(app, ["run", str(recipe), "--dry-run", "--out", str(tmp_path / "out")])
    assert result.exit_code == 0
    assert "Dry run" in result.stdout
    assert not (tmp_path / "out").exists()  # nothing written


def test_run_limit_pages(tmp_path, monkeypatch, patch_transport):
    monkeypatch.chdir(tmp_path)
    recipe = _recipe_file(tmp_path)
    out_dir = tmp_path / "out"
    result = runner.invoke(app, ["run", str(recipe), "--out", str(out_dir), "--limit-pages", "1"])
    assert result.exit_code == 0
    # Only page 1 fetched => its 2 quotes.
    assert len((out_dir / "quotes.jsonl").read_text().splitlines()) == 2


def test_run_bad_recipe_errors(tmp_path, monkeypatch):
    monkeypatch.chdir(tmp_path)
    bad = tmp_path / "bad.yml"
    bad.write_text("name: x\n")  # no start_urls / fields
    result = runner.invoke(app, ["run", str(bad)])
    assert result.exit_code == 1
    assert "Recipe error" in result.stdout


def test_fetch_with_selector(tmp_path, monkeypatch, patch_transport):
    result = runner.invoke(app, ["fetch", "https://quotes.test/page/1/", "--selector", "small.author"])
    assert result.exit_code == 0
    assert "node(s) match" in result.stdout
    assert "Amy Ant" in result.stdout
