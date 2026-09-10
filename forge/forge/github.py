"""A four-function GitHub client over urllib.

The Forge needs three things from GitHub: recent workflow runs, the ability to
open a draft pull request, and the state of a PR it opened earlier. That does
not justify a dependency. Every call returns {} rather than raising, because a
GitHub outage must degrade the night, not break it.
"""

from __future__ import annotations

import http.client
import json
import os
import re
import subprocess
import urllib.error
import urllib.request
from pathlib import Path

API_ROOT = "https://api.github.com"
TIMEOUT_S = 20

_SLUG_RE = re.compile(r"github\.com[:/]([^/]+/[^/\s]+?)(?:\.git)?$")


def env_token() -> str | None:
    """The token from the environment, or None. Named for what it reads."""
    return os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN") or None


def api(path: str, method: str = "GET", body: dict | None = None, token: str | None = None) -> dict:
    """One GitHub REST call. Returns {} on any failure — never raises.

    Everything that can go wrong with a single call — building the request,
    making it, and reading the response — is inside one try block. Building
    the ``Request`` is not the network-safe step it looks like: its URL is
    parsed eagerly, so a malformed ``path`` raises ``ValueError`` (e.g.
    "Invalid IPv6 URL") right there, before any socket opens. Leaving that
    construction outside the guard would let a bad path escape the "never
    raises" contract this module promises its callers; ``json.dumps`` on a
    non-serialisable ``body`` fails the same way, with ``TypeError``. All
    three of those, plus the network errors below, share one except clause.

    ``http.client.HTTPException`` (e.g. ``IncompleteRead``, raised when the
    server closes the connection mid-body) is named separately from
    ``OSError`` because it does not descend from it the way the socket-level
    failures — connection resets, timeouts, DNS errors — do; omitting it
    would let a truncated response escape this "never raises" contract.
    """
    tok = token or env_token()
    if not tok:
        return {}
    try:
        data = json.dumps(body).encode("utf-8") if body is not None else None
        req = urllib.request.Request(
            f"{API_ROOT}{path}",
            data=data,
            method=method,
            headers={
                "Authorization": f"Bearer {tok}",
                "Accept": "application/vnd.github+json",
                "X-GitHub-Api-Version": "2022-11-28",
                "Content-Type": "application/json",
                "User-Agent": "maz-forge",
            },
        )
        with urllib.request.urlopen(req, timeout=TIMEOUT_S) as resp:
            payload = resp.read().decode("utf-8")
    except (urllib.error.URLError, OSError, ValueError, TypeError, http.client.HTTPException):
        # urllib.error.HTTPError (4xx/5xx) is a URLError subclass; DNS
        # failures, connection resets and socket timeouts are OSError
        # subclasses; TLS failures (ssl.SSLError) are OSError subclasses too;
        # a malformed URL or non-UTF-8 body decode raises ValueError.
        # http.client.HTTPException (e.g. IncompleteRead, from a mid-stream
        # read failure) is named explicitly: it subclasses Exception, not
        # OSError, so it would otherwise escape this tuple entirely.
        return {}
    try:
        parsed = json.loads(payload)
    except (json.JSONDecodeError, ValueError):
        return {}
    return parsed if isinstance(parsed, dict) else {"items": parsed}


def repo_slug(root: Path | None, runner=None) -> str | None:
    """"owner/name" from the origin remote, or None."""
    def _run(args: list[str]) -> str:
        r = subprocess.run(["git", *args], cwd=str(root or Path.cwd()),
                           capture_output=True, text=True)
        return r.stdout if r.returncode == 0 else ""

    run = runner or _run
    try:
        url = run(["remote", "get-url", "origin"]).strip()
    except Exception:  # noqa: BLE001
        return None
    if not url:
        return None
    # A remote can carry one or more trailing slashes (e.g. a URL pasted
    # with a stray "/", or ".git/"); _SLUG_RE is anchored on $ with no
    # allowance for one, so strip them first rather than loosen the anchor
    # and risk matching short of the real end of the slug.
    m = _SLUG_RE.search(url.rstrip("/"))
    return m.group(1) if m else None
