"""Central configuration for maz-scrape.

Tunables that control fetching politeness and crawl bounds live here so they are
easy to change in one place. The precedence, lowest to highest, is:

    dataclass defaults  <  scraper.json (project file)  <  SCRAPER_* env vars

A committed ``scraper.json`` sets project defaults while the ``SCRAPER_*`` env
vars stay handy for one-off runs. A missing or malformed config file never
crashes the tool — it just falls back to the defaults.
"""

from __future__ import annotations

import json
import os
from dataclasses import dataclass, field, fields
from pathlib import Path

# A committable project config file (JSON so it works on every supported Python
# with no extra dependency). Lives at the project root next to where you run
# `scrape`, unlike the gitignored `.scraper/` run state.
CONFIG_FILENAME = "scraper.json"

# A descriptive default User-Agent — a well-behaved scraper identifies itself.
DEFAULT_USER_AGENT = "maz-scrape/0.1 (+https://github.com/mazoo85/maz)"

# Fields a project config file is allowed to set. Anything else in the file is
# ignored (so the file can never reach non-tunable internals).
_FILE_FIELDS = (
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
)

_INT_FIELDS = ("max_retries", "concurrency", "max_depth", "max_pages")
_FLOAT_FIELDS = ("request_timeout", "retry_backoff", "rate_limit_per_host")
_BOOL_FIELDS = ("respect_robots",)


@dataclass(frozen=True)
class ScraperConfig:
    """Runtime configuration for a scrape run."""

    # --- Fetching / politeness ---------------------------------------------
    user_agent: str = DEFAULT_USER_AGENT
    request_timeout: float = 20.0  # seconds per request
    max_retries: int = 3  # retries on transient network/5xx errors
    retry_backoff: float = 1.0  # base seconds; doubles each retry (1, 2, 4, ...)
    rate_limit_per_host: float = 2.0  # max requests/sec to any single host
    concurrency: int = 4  # reserved for future parallel fetching

    # Respect robots.txt by default — a general-purpose scraper should be
    # well-behaved out of the box. Recipes can opt out per-site.
    respect_robots: bool = True

    # --- Crawl bounds ------------------------------------------------------
    max_depth: int = 3  # link-following depth from the seed URLs (0 = seeds only)
    max_pages: int = 200  # hard cap on pages fetched per run

    # --- Output ------------------------------------------------------------
    output_dir: str = "scraped"  # where JSONL/CSV/SQLite land (relative to cwd)

    # Directory where per-run state (e.g. robots cache) may be written.
    state_dirname: str = ".scraper"

    # Extra tunables can be threaded through here later.
    extra: dict = field(default_factory=dict)

    def output_path(self, root: Path | None = None) -> Path:
        return (root or Path.cwd()) / self.output_dir

    def state_dir(self, root: Path | None = None) -> Path:
        return (root or Path.cwd()) / self.state_dirname


def _read_config_file(root: Path | None = None) -> dict:
    """Read allowed fields from ``scraper.json`` at ``root`` (cwd by default)."""
    p = (root or Path.cwd()) / CONFIG_FILENAME
    if not p.exists():
        return {}
    try:
        data = json.loads(p.read_text())
    except (json.JSONDecodeError, ValueError, OSError):
        # A broken config file should never crash the tool — ignore it.
        return {}
    if not isinstance(data, dict):
        return {}
    return {k: v for k, v in data.items() if k in _FILE_FIELDS and v is not None}


def load_config(root: Path | None = None) -> ScraperConfig:
    """Build a config from defaults, then ``scraper.json``, then env overrides."""
    kwargs: dict = _read_config_file(root)

    # Env overrides: SCRAPER_<FIELD_NAME_UPPERCASED>.
    for name in _FILE_FIELDS:
        env_key = "SCRAPER_" + name.upper()
        if (v := os.environ.get(env_key)) is not None:
            kwargs[name] = v

    # Coerce values (JSON may give the right type already; env always gives str).
    for key in _INT_FIELDS:
        if key in kwargs:
            kwargs[key] = int(kwargs[key])
    for key in _FLOAT_FIELDS:
        if key in kwargs:
            kwargs[key] = float(kwargs[key])
    for key in _BOOL_FIELDS:
        if key in kwargs:
            kwargs[key] = _as_bool(kwargs[key])

    return ScraperConfig(**kwargs)


def _as_bool(value) -> bool:
    if isinstance(value, bool):
        return value
    return str(value).strip().lower() in ("1", "true", "yes", "on")


def effective_values(config: ScraperConfig) -> dict:
    """The user-facing config fields, for display by `scrape config`."""
    return {f.name: getattr(config, f.name) for f in fields(config) if f.name in _FILE_FIELDS}


def default_config_dict() -> dict:
    """The settable fields at their defaults — the body of a starter scraper.json."""
    return effective_values(ScraperConfig())


def write_starter_config(root: Path | None = None, force: bool = False) -> tuple[bool, Path]:
    """Write a starter ``scraper.json`` at ``root``. Returns (written, path).

    Won't clobber an existing file unless ``force`` is set.
    """
    p = (root or Path.cwd()) / CONFIG_FILENAME
    if p.exists() and not force:
        return (False, p)
    p.write_text(json.dumps(default_config_dict(), indent=2) + "\n")
    return (True, p)
