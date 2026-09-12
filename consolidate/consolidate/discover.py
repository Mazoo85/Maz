"""Where the list of repositories comes from.

Three sources, all producing the same :class:`SourceRepo` objects:

* ``from_github``  — every repo on a GitHub account (the usual case)
* ``from_specs``   — an explicit list typed on the command line
* ``from_file``    — a JSON file, so a reviewed list can be replayed exactly

Only ``from_github`` touches the network, and it is the only function here that
can fail for reasons outside your control, so the other two stay usable offline.
"""

from __future__ import annotations

import json
import os
import re
import urllib.error
import urllib.request
from pathlib import Path
from typing import Iterable, Sequence

from .models import SourceRepo

API_ROOT = "https://api.github.com"
TIMEOUT_S = 20
PER_PAGE = 100
MAX_PAGES = 20

#: ``owner/name``, ``https://github.com/owner/name(.git)``, ``git@github.com:owner/name``
_SPEC_RE = re.compile(
    r"^(?:(?:https?://|git@)?github\.com[:/])?([A-Za-z0-9._-]+)/([A-Za-z0-9._-]+?)(?:\.git)?/?$"
)


class DiscoveryError(RuntimeError):
    """Raised when repositories could not be listed, with a human explanation."""


def env_token() -> str | None:
    """The GitHub token, if the environment offers one.

    A token is optional: without it you see public repositories, with it you
    also see private ones.
    """
    for name in ("GITHUB_TOKEN", "GH_TOKEN"):
        value = os.environ.get(name)
        if value:
            return value.strip()
    return None


def _get(path: str, token: str | None, *, opener=None) -> tuple[list, dict]:
    url = path if path.startswith("http") else f"{API_ROOT}{path}"
    request = urllib.request.Request(url, method="GET")
    request.add_header("Accept", "application/vnd.github+json")
    request.add_header("X-GitHub-Api-Version", "2022-11-28")
    request.add_header("User-Agent", "maz-consolidate")
    if token:
        request.add_header("Authorization", f"Bearer {token}")
    open_url = opener or urllib.request.urlopen
    try:
        with open_url(request, timeout=TIMEOUT_S) as response:
            payload = json.loads(response.read().decode("utf-8") or "[]")
            headers = dict(response.headers)
    except urllib.error.HTTPError as exc:  # pragma: no cover - network shape
        raise DiscoveryError(
            f"GitHub said {exc.code} for {url}. "
            + (
                "Set GITHUB_TOKEN to a token with 'repo' scope."
                if exc.code in (401, 403, 404)
                else "Try again in a moment."
            )
        ) from exc
    except urllib.error.URLError as exc:  # pragma: no cover - network shape
        raise DiscoveryError(f"Could not reach GitHub: {exc.reason}") from exc
    if not isinstance(payload, list):
        raise DiscoveryError(f"Expected a list of repositories from {url}, got {type(payload).__name__}.")
    return payload, headers


def _next_link(headers: dict) -> str | None:
    """GitHub paginates with a Link header; pull the ``rel="next"`` url out of it."""
    link = headers.get("Link") or headers.get("link") or ""
    for part in link.split(","):
        if 'rel="next"' in part:
            start, end = part.find("<"), part.find(">")
            if start != -1 and end > start:
                return part[start + 1 : end]
    return None


def _repo_from_api(item: dict) -> SourceRepo:
    owner = (item.get("owner") or {}).get("login", "")
    return SourceRepo(
        owner=owner,
        name=item.get("name", ""),
        url=item.get("clone_url") or item.get("html_url", ""),
        description=(item.get("description") or "").strip(),
        default_branch=item.get("default_branch") or "main",
        is_fork=bool(item.get("fork")),
        archived=bool(item.get("archived")),
        visibility=item.get("visibility") or ("private" if item.get("private") else "public"),
        pushed_at=item.get("pushed_at") or "",
    )


def from_github(user: str | None = None, token: str | None = None, *, opener=None) -> list[SourceRepo]:
    """List the repositories of ``user`` (or of the token's own account).

    With a token we ask ``/user/repos`` so private repositories are included;
    without one we can only see what is public.
    """
    token = token if token is not None else env_token()
    if token:
        path = f"/user/repos?per_page={PER_PAGE}&affiliation=owner&sort=full_name"
    elif user:
        path = f"/users/{user}/repos?per_page={PER_PAGE}&type=owner&sort=full_name"
    else:
        raise DiscoveryError("Give a GitHub username, or set GITHUB_TOKEN to use your own account.")

    repos: list[SourceRepo] = []
    next_path: str | None = path
    for _ in range(MAX_PAGES):
        if not next_path:
            break
        page, headers = _get(next_path, token, opener=opener)
        repos.extend(_repo_from_api(item) for item in page)
        next_path = _next_link(headers)

    if user:
        repos = [r for r in repos if r.owner.lower() == user.lower()]
    repos.sort(key=lambda r: r.name.lower())
    return repos


def parse_spec(spec: str) -> tuple[str, str]:
    """``"Mazoo85/Maz"`` or a github URL -> ``("Mazoo85", "Maz")``."""
    match = _SPEC_RE.match(spec.strip())
    if not match:
        raise DiscoveryError(
            f"{spec!r} is not a repository. Write it as owner/name, "
            "or paste the https://github.com/... address."
        )
    return match.group(1), match.group(2)


def from_specs(specs: Sequence[str], *, default_branch: str = "main") -> list[SourceRepo]:
    """Build repos from an explicit list, with no network access at all."""
    out = []
    for spec in specs:
        owner, name = parse_spec(spec)
        out.append(
            SourceRepo(
                owner=owner,
                name=name,
                url=f"https://github.com/{owner}/{name}.git",
                default_branch=default_branch,
            )
        )
    return out


def from_file(path: str | Path) -> list[SourceRepo]:
    """Read repos from a JSON file: a list of objects, or a ``{"repos": [...]}``."""
    text = Path(path).read_text(encoding="utf-8")
    data = json.loads(text)
    items = data.get("repos", []) if isinstance(data, dict) else data
    if not isinstance(items, list):
        raise DiscoveryError(f"{path} should hold a list of repositories.")
    return [SourceRepo.from_dict(item) for item in items]


def to_file(repos: Iterable[SourceRepo], path: str | Path) -> None:
    """Save a discovered list so the exact same set can be replayed later."""
    payload = {"repos": [r.to_dict() for r in repos]}
    Path(path).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
