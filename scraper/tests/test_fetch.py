"""Fetcher behaviour: success, retry/backoff, rate limiting — all offline."""

import httpx

from scraper.config import ScraperConfig
from scraper.fetch import Fetcher


def test_get_ok():
    transport = httpx.MockTransport(lambda req: httpx.Response(200, text="<h1>hi</h1>"))
    with Fetcher(ScraperConfig(rate_limit_per_host=0), transport=transport) as f:
        r = f.get("https://a.test/")
    assert r.ok
    assert r.status_code == 200
    assert "hi" in r.text


def test_get_404_not_ok():
    transport = httpx.MockTransport(lambda req: httpx.Response(404, text="nope"))
    with Fetcher(ScraperConfig(rate_limit_per_host=0), transport=transport) as f:
        r = f.get("https://a.test/")
    assert not r.ok
    assert r.status_code == 404
    assert r.text == ""  # body dropped for non-2xx


def test_retries_then_succeeds():
    calls = {"n": 0}

    def handler(req):
        calls["n"] += 1
        if calls["n"] < 3:
            return httpx.Response(503, text="busy")
        return httpx.Response(200, text="ok")

    sleeps = []
    cfg = ScraperConfig(max_retries=3, retry_backoff=1.0, rate_limit_per_host=0)
    f = Fetcher(cfg, transport=httpx.MockTransport(handler), sleep=sleeps.append)
    r = f.get("https://a.test/")
    f.close()
    assert r.ok
    assert calls["n"] == 3
    assert sleeps == [1.0, 2.0]  # exponential backoff between the 3 attempts


def test_retries_exhausted_returns_not_ok():
    cfg = ScraperConfig(max_retries=1, retry_backoff=0.5, rate_limit_per_host=0)
    f = Fetcher(cfg, transport=httpx.MockTransport(lambda req: httpx.Response(500)), sleep=lambda s: None)
    r = f.get("https://a.test/")
    f.close()
    assert not r.ok
    assert r.status_code == 500


def test_network_error_returns_not_ok():
    def boom(req):
        raise httpx.ConnectError("down")

    cfg = ScraperConfig(max_retries=1, rate_limit_per_host=0)
    f = Fetcher(cfg, transport=httpx.MockTransport(boom), sleep=lambda s: None)
    r = f.get("https://a.test/")
    f.close()
    assert not r.ok
    assert r.status_code == 0


def test_rate_limit_sleeps_between_same_host():
    # Fake clock that doesn't advance on its own: every request must wait.
    clock = {"t": 0.0}
    sleeps = []

    def sleep(s):
        sleeps.append(s)
        clock["t"] += s

    cfg = ScraperConfig(rate_limit_per_host=2.0, max_retries=0)  # 0.5s min interval
    f = Fetcher(
        cfg,
        transport=httpx.MockTransport(lambda req: httpx.Response(200, text="ok")),
        sleep=sleep,
        monotonic=lambda: clock["t"],
    )
    f.get("https://a.test/1")
    f.get("https://a.test/2")  # same host -> must throttle
    f.close()
    assert sleeps == [0.5]


def test_rate_limit_independent_per_host():
    clock = {"t": 0.0}
    sleeps = []
    cfg = ScraperConfig(rate_limit_per_host=2.0, max_retries=0)
    f = Fetcher(
        cfg,
        transport=httpx.MockTransport(lambda req: httpx.Response(200, text="ok")),
        sleep=lambda s: sleeps.append(s),
        monotonic=lambda: clock["t"],
    )
    f.get("https://a.test/")
    f.get("https://b.test/")  # different host -> no throttle
    f.close()
    assert sleeps == []
