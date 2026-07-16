"""HTTP fetching with politeness: timeout, retry/backoff, and per-host rate limiting.

:class:`Fetcher` wraps a single :class:`httpx.Client` (connection reuse) and adds
the behaviours a general-purpose scraper needs to be well-behaved:

* a descriptive, configurable User-Agent,
* a per-request timeout,
* retry-with-exponential-backoff on transient network / 5xx errors, and
* a per-host rate limit so we never hammer a single site.

Tests inject a custom ``httpx`` transport (``httpx.MockTransport``) so the whole
suite runs offline.
"""

from __future__ import annotations

import time
from dataclasses import dataclass
from urllib.parse import urlsplit

import httpx

from .config import ScraperConfig


@dataclass
class FetchResult:
    """The outcome of fetching one URL."""

    url: str  # final URL (after redirects)
    status_code: int
    text: str
    ok: bool


# Status codes worth retrying — transient server-side / rate-limit signals.
_RETRY_STATUS = {429, 500, 502, 503, 504}


class Fetcher:
    """A polite HTTP client for a single scrape run.

    Use as a context manager so the underlying connection pool is closed::

        with Fetcher(config) as f:
            result = f.get("https://example.com")
    """

    def __init__(
        self,
        config: ScraperConfig,
        *,
        transport: httpx.BaseTransport | None = None,
        sleep=time.sleep,
        monotonic=time.monotonic,
    ) -> None:
        self._config = config
        self._sleep = sleep
        self._monotonic = monotonic
        # Min seconds between requests to the same host (0 disables throttling).
        rate = config.rate_limit_per_host
        self._min_interval = (1.0 / rate) if rate and rate > 0 else 0.0
        self._last_request_at: dict[str, float] = {}
        self._client = httpx.Client(
            headers={"User-Agent": config.user_agent},
            timeout=config.request_timeout,
            follow_redirects=True,
            transport=transport,
        )

    # -- context manager -----------------------------------------------------
    def __enter__(self) -> "Fetcher":
        return self

    def __exit__(self, *exc) -> None:
        self.close()

    def close(self) -> None:
        self._client.close()

    # -- rate limiting -------------------------------------------------------
    def _throttle(self, url: str) -> None:
        if not self._min_interval:
            return
        host = urlsplit(url).netloc
        last = self._last_request_at.get(host)
        now = self._monotonic()
        if last is not None:
            wait = self._min_interval - (now - last)
            if wait > 0:
                self._sleep(wait)
                now = self._monotonic()
        self._last_request_at[host] = now

    # -- fetching ------------------------------------------------------------
    def get(self, url: str) -> FetchResult:
        """Fetch ``url`` with rate limiting and retry/backoff.

        Returns a :class:`FetchResult`. A request that never succeeds (after
        exhausting retries) comes back with ``ok=False`` rather than raising, so
        a crawl can log it and move on.
        """
        attempts = self._config.max_retries + 1
        backoff = self._config.retry_backoff
        last_status = 0
        for attempt in range(attempts):
            self._throttle(url)
            try:
                response = self._client.get(url)
            except httpx.HTTPError:
                # Network-level failure — retry with backoff if attempts remain.
                if attempt + 1 < attempts:
                    self._sleep(backoff * (2**attempt))
                    continue
                return FetchResult(url=url, status_code=0, text="", ok=False)

            last_status = response.status_code
            if response.status_code in _RETRY_STATUS and attempt + 1 < attempts:
                self._sleep(backoff * (2**attempt))
                continue

            ok = 200 <= response.status_code < 300
            return FetchResult(
                url=str(response.url),
                status_code=response.status_code,
                text=response.text if ok else "",
                ok=ok,
            )

        return FetchResult(url=url, status_code=last_status, text="", ok=False)

    def get_text(self, url: str) -> tuple[int | None, str]:
        """Fetch and return ``(status_code, text)`` — used by the robots cache."""
        result = self.get(url)
        return (result.status_code or None, result.text)
