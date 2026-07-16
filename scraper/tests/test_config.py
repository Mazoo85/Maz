"""Config precedence: defaults < scraper.json < SCRAPER_* env vars."""

import json

from scraper.config import (
    CONFIG_FILENAME,
    ScraperConfig,
    default_config_dict,
    effective_values,
    load_config,
    write_starter_config,
)

ENV_KEYS = [
    "SCRAPER_USER_AGENT",
    "SCRAPER_REQUEST_TIMEOUT",
    "SCRAPER_MAX_RETRIES",
    "SCRAPER_RATE_LIMIT_PER_HOST",
    "SCRAPER_RESPECT_ROBOTS",
    "SCRAPER_MAX_DEPTH",
    "SCRAPER_MAX_PAGES",
    "SCRAPER_OUTPUT_DIR",
]


def _clear_env(monkeypatch):
    for k in ENV_KEYS:
        monkeypatch.delenv(k, raising=False)


def _write(root, data):
    (root / CONFIG_FILENAME).write_text(json.dumps(data))


def test_defaults(monkeypatch, tmp_path):
    _clear_env(monkeypatch)
    cfg = load_config(root=tmp_path)
    assert cfg.max_depth == 3
    assert cfg.max_pages == 200
    assert cfg.respect_robots is True
    assert cfg.rate_limit_per_host == 2.0


def test_env_overrides(monkeypatch, tmp_path):
    _clear_env(monkeypatch)
    monkeypatch.setenv("SCRAPER_MAX_PAGES", "42")
    monkeypatch.setenv("SCRAPER_RATE_LIMIT_PER_HOST", "0.5")
    cfg = load_config(root=tmp_path)
    assert cfg.max_pages == 42
    assert cfg.rate_limit_per_host == 0.5


def test_file_sets_values(monkeypatch, tmp_path):
    _clear_env(monkeypatch)
    _write(tmp_path, {"max_depth": 1, "user_agent": "custom-ua/1.0"})
    cfg = load_config(root=tmp_path)
    assert cfg.max_depth == 1
    assert cfg.user_agent == "custom-ua/1.0"
    assert cfg.max_pages == 200  # untouched keys keep their defaults


def test_env_beats_file(monkeypatch, tmp_path):
    _clear_env(monkeypatch)
    _write(tmp_path, {"max_pages": 10})
    monkeypatch.setenv("SCRAPER_MAX_PAGES", "99")
    assert load_config(root=tmp_path).max_pages == 99


def test_unknown_keys_ignored(monkeypatch, tmp_path):
    _clear_env(monkeypatch)
    _write(tmp_path, {"max_depth": 2, "not_a_field": "nope", "state_dirname": "hax"})
    cfg = load_config(root=tmp_path)
    assert cfg.max_depth == 2
    assert cfg.state_dirname == ".scraper"  # non-allowlisted field can't be set from file


def test_malformed_file_falls_back(monkeypatch, tmp_path):
    _clear_env(monkeypatch)
    (tmp_path / CONFIG_FILENAME).write_text("{ this is not json")
    cfg = load_config(root=tmp_path)  # must not raise
    assert cfg.max_depth == 3


def test_respect_robots_env_coerces(monkeypatch, tmp_path):
    _clear_env(monkeypatch)
    monkeypatch.setenv("SCRAPER_RESPECT_ROBOTS", "false")
    assert load_config(root=tmp_path).respect_robots is False
    monkeypatch.setenv("SCRAPER_RESPECT_ROBOTS", "1")
    assert load_config(root=tmp_path).respect_robots is True


def test_effective_values_shape(monkeypatch, tmp_path):
    _clear_env(monkeypatch)
    vals = effective_values(load_config(root=tmp_path))
    assert set(vals) == {
        "user_agent",
        "request_timeout",
        "max_retries",
        "retry_backoff",
        "rate_limit_per_host",
        "concurrency",
        "respect_robots",
        "max_depth",
        "max_pages",
        "output_dir",
    }


def test_write_starter_config(tmp_path):
    written, path = write_starter_config(root=tmp_path)
    assert written is True
    assert path.exists()
    # Won't clobber without force.
    again, _ = write_starter_config(root=tmp_path)
    assert again is False
    forced, _ = write_starter_config(root=tmp_path, force=True)
    assert forced is True
    # The file round-trips to the default dict.
    assert json.loads(path.read_text()) == default_config_dict()


def test_default_config_dict_matches_dataclass():
    assert default_config_dict() == effective_values(ScraperConfig())
