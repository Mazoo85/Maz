"""robots.txt fetching, caching, and allow/deny checks.

A well-behaved scraper honours ``robots.txt`` by default. This module keeps a
small per-host cache of parsed rules so we fetch each site's policy at most once
per run. A host whose ``robots.txt`` can't be fetched is treated as *allow all*
(the conventional default), so a transient error never silently blocks a whole
crawl.
"""

from __future__ import annotations

from urllib.parse import urlsplit, urlunsplit
from urllib.robotparser import RobotFileParser


def robots_url_for(url: str) -> str:
    """Return the ``robots.txt`` URL for the host serving ``url``."""
    parts = urlsplit(url)
    return urlunsplit((parts.scheme, parts.netloc, "/robots.txt", "", ""))


class RobotsCache:
    """Caches parsed robots policies keyed by ``scheme://host``.

    ``fetch`` is a callable ``(url) -> (status_code, text)`` — injected so the
    cache reuses the run's :class:`~scraper.fetch.Fetcher` (same UA, timeout,
    rate limiting) and so tests can drive it fully offline.
    """

    def __init__(self, fetch, user_agent: str) -> None:
        self._fetch = fetch
        self._user_agent = user_agent
        self._by_host: dict[str, RobotFileParser] = {}

    def _host_key(self, url: str) -> str:
        parts = urlsplit(url)
        return f"{parts.scheme}://{parts.netloc}"

    def _parser_for(self, url: str) -> RobotFileParser:
        key = self._host_key(url)
        parser = self._by_host.get(key)
        if parser is not None:
            return parser

        parser = RobotFileParser()
        try:
            status, text = self._fetch(robots_url_for(url))
        except Exception:
            # Network trouble reaching robots.txt — default to allow-all.
            status, text = None, ""

        if status == 200 and text:
            parser.parse(text.splitlines())
        elif status is not None and 400 <= status < 500:
            # Missing/forbidden robots.txt conventionally means "allow all".
            parser.allow_all = True
        else:
            # 5xx or unknown — be conservative but don't hard-block; allow all.
            parser.allow_all = True

        self._by_host[key] = parser
        return parser

    def allowed(self, url: str) -> bool:
        """Whether the configured User-Agent may fetch ``url``."""
        return self._parser_for(url).can_fetch(self._user_agent, url)
