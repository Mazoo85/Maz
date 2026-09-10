"""Where the Forge may work, and how dangerous the work is.

Two independent questions, deliberately kept apart:

  zone_for()     may it touch these paths at all? (the leash)
  risk_keys_for() how dangerous is touching them?  (the scoring)

Both are pure functions over path strings so they can be exercised without a
filesystem.

Matching is plain `str.startswith` against literal prefixes: case-sensitive,
with no notion of `..`, symlinks, or a real filesystem underneath it. The
safety guarantee this module provides is therefore a guarantee for a Linux /
case-sensitive-filesystem checkout, and paths shaped in ways this simple
comparison cannot trust (a `..` segment, an absolute path, a backslash, an
empty path) are rejected outright rather than resolved — see `_is_suspicious`.
"""

from __future__ import annotations

from .config import ForgeConfig

# Path prefixes that raise a candidate's risk. Keys name entries in the
# config's `weights` dict so the numbers stay tunable from forge.json.
RISK_PATHS = (
    ("CMakeLists.txt", "risk_build_system"),
    ("cmake/", "risk_build_system"),
    ("engine/CMakeLists.txt", "risk_build_system"),
    ("engine/src/core/", "risk_engine_core"),
    ("engine/include/", "risk_engine_core"),
    ("engine/src/render/", "risk_engine_core"),
)

# Above this many files, a change is cross-cutting by definition.
CROSS_CUTTING_FILES = 6


def _normalise(path: str) -> str:
    """Strip a leading "./" (as a literal prefix, repeated) from a path.

    `str.lstrip("./")` looks tempting here but is wrong: it strips any
    leading run of '.' and '/' characters, so ".github/workflows/ci.yml"
    would lose its leading dot and become "github/workflows/ci.yml" — no
    longer matching the ".github/workflows/" no-touch prefix. That silently
    loosens the leash on exactly the path it must never loosen on.
    """
    while path.startswith("./"):
        path = path[2:]
    return path


def _is_suspicious(path: str) -> bool:
    """True for a path shape this module cannot safely reason about.

    Matching here is pure `str.startswith` against literal prefixes — it has
    no filesystem underneath it and does not resolve `..` the way a real
    filesystem would. A path built to fool that comparison must be rejected
    outright rather than "handled": resolving it (`os.path.normpath`,
    `Path.resolve`) would add filesystem-dependent behaviour, symlink
    semantics, and cwd-relative surprises to a function that is supposed to
    be a pure, offline check — new surface a crafted path could exploit in
    some *other* way. Rejection has no such surface: a path this function
    refuses is never touched, whatever it would have resolved to.

    Caught here:
      - a `..` path *segment* (not merely two dots in a filename — "a..b/c.py"
        is an ordinary name and must be admitted normally)
      - an absolute path (leading "/")
      - a backslash anywhere (this module only understands "/"-separated
        paths; a backslash means either a Windows-style path or an attempt
        to smuggle a separator past the "/"-based prefix checks)
      - an empty or whitespace-only path

    Real candidate paths come from `git ls-files` / `git diff --name-only`,
    which never emit any of these shapes, so rejecting them costs nothing in
    practice — only a hand-crafted or already-malicious path pays for it.
    """
    if not path.strip():
        return True
    if path.startswith("/"):
        return True
    if "\\" in path:
        return True
    if any(segment == ".." for segment in path.split("/")):
        return True
    return False


def is_no_touch(paths: tuple[str, ...], config: ForgeConfig) -> bool:
    """True if ANY path falls under a no-touch prefix. No-touch always wins.

    Also true for any path this module cannot confidently parse (see
    `_is_suspicious`) — deliberately, even though such a path is not
    literally "under a no-touch prefix". `is_no_touch` is read by callers as
    "is this forbidden?", and for a path we cannot parse the honest answer is
    yes: being too cautious here is free, but the alternative — a public
    predicate that says "not forbidden" about a path it does not actually
    understand — is a false negative on the one guarantee this module
    exists to provide. Do not simplify this back to a plain prefix check.

    Matching is a plain `str.startswith` over literal prefixes, so it is
    case-sensitive: this is a Linux / case-sensitive-filesystem guarantee.
    On a case-insensitive filesystem "FORGE/decide.py" would not match the
    "forge/" prefix and this function would (wrongly, for that filesystem)
    say it is safe.
    """
    for path in paths:
        p = _normalise(path)
        if _is_suspicious(p):
            return True
        if any(p.startswith(prefix) for prefix in config.no_touch):
            return True
    return False


def zone_for(paths: tuple[str, ...], config: ForgeConfig) -> str | None:
    """The safe zone covering EVERY path, or None if any path is outside it.

    Returns None for an empty path tuple: a candidate whose files are unknown
    cannot be proven safe, and the Forge does not work blind. Also returns
    None for any path `is_no_touch` flags as suspicious (see `_is_suspicious`)
    — an unparseable path is never "covered" by a safe zone.
    """
    if not paths:
        return None
    if is_no_touch(paths, config):
        return None

    matched: list[str] = []
    for path in paths:
        p = _normalise(path)
        zone = next((z for z in config.safe_zones if p.startswith(z)), None)
        if zone is None:
            return None
        matched.append(zone)
    # Report the broadest zone touched, for the ledger.
    return sorted(matched, key=len)[0]


def risk_keys_for(paths: tuple[str, ...]) -> tuple[str, ...]:
    """Which risk weights apply to a change touching these paths."""
    keys: list[str] = []
    for path in paths:
        p = _normalise(path)
        for prefix, key in RISK_PATHS:
            if p.startswith(prefix) and key not in keys:
                keys.append(key)
    if len(paths) > CROSS_CUTTING_FILES and "risk_cross_cutting" not in keys:
        keys.append("risk_cross_cutting")
    return tuple(keys)
