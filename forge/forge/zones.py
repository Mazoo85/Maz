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
empty path, or an element that is not even a `str` — the most unparseable
shape there is) are rejected outright rather than resolved — see
`_is_suspicious`.
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

    Callers must run `_is_suspicious` (which checks `isinstance(path, str)`
    first) before calling this: `.startswith` below assumes a string, and a
    non-string reaching this point crashes instead of failing closed.
    """
    while path.startswith("./"):
        path = path[2:]
    return path


def _is_suspicious(path: object) -> bool:
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
      - not a `str` at all (a `None`, an `int`, a `dict`, a `list`, a
        `bytes` — anything a hand-edited or partially-corrupted pulse.json
        can smuggle into a `paths` list, since JSON decodes cleanly into
        every one of those). This check must run before any of the ones
        below, which all call `str` methods (`.strip`, `.startswith`,
        `.split`) that would themselves raise on a non-string. A type this
        module cannot even parse as a path is the most unparseable shape
        there is, so it belongs in this same fail-closed category rather
        than a separate mechanism — do not "simplify" this check away by
        assuming callers already validated element types upstream; they are
        not required to, and the crash this line prevents is exactly what
        happens when they don't.
      - a `..` path *segment* (not merely two dots in a filename — "a..b/c.py"
        is an ordinary name and must be admitted normally)
      - an absolute path (leading "/")
      - a backslash anywhere (this module only understands "/"-separated
        paths; a backslash means either a Windows-style path or an attempt
        to smuggle a separator past the "/"-based prefix checks)
      - an empty or whitespace-only path

    Real candidate paths come from `git ls-files` / `git diff --name-only`,
    which never emit any of these shapes, so rejecting them costs nothing in
    practice — only a hand-crafted, already-malicious, or corrupted-data path
    pays for it.
    """
    if not isinstance(path, str):
        return True
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

    That includes an element that is not even a `str` — a `None`, an `int`,
    a `dict`, whatever a corrupted `pulse.json` decoded into. It is not a
    new category: it is simply the most unparseable path shape there is, so
    it fails closed exactly like `..` traversal or an absolute path does,
    for the same reason (`_is_suspicious` above says why).

    Matching is a plain `str.startswith` over literal prefixes, so it is
    case-sensitive: this is a Linux / case-sensitive-filesystem guarantee.
    On a case-insensitive filesystem "FORGE/decide.py" would not match the
    "forge/" prefix and this function would (wrongly, for that filesystem)
    say it is safe.
    """
    for path in paths:
        # Suspicion (including the type check) must run on the raw element,
        # before `_normalise` touches it: `_normalise` calls `.startswith`
        # unconditionally and has no fallback for a non-string.
        if _is_suspicious(path):
            return True
        p = _normalise(path)
        if any(p.startswith(prefix) for prefix in config.no_touch):
            return True
    return False


def zone_for(paths: tuple[str, ...], config: ForgeConfig) -> str | None:
    """The safe zone covering EVERY path, or None if any path is outside it.

    Returns None for an empty path tuple: a candidate whose files are unknown
    cannot be proven safe, and the Forge does not work blind. Also returns
    None for any path `is_no_touch` flags as suspicious (see `_is_suspicious`)
    — an unparseable path is never "covered" by a safe zone. That includes a
    non-string element (a `None`, an `int`, ...): `is_no_touch` rejects the
    whole tuple before this function's own loop ever calls a string method
    on one of its elements, so a single bad element here costs the candidate
    its zone, not a crash.
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


def zones_for_files(paths: tuple[str, ...], config: ForgeConfig) -> tuple[str, ...] | None:
    """Every distinct zone that actually covers one of these paths.

    `zone_for` above collapses a multi-zone change down to the single
    *broadest* zone spanning it — the right answer for the ledger's one-line
    `zone` field, wrong for deciding what to verify: VERIFY must run every
    zone's checks a change actually touches, not just the broadest one
    (`docs/` covering both `docs/ARCHITECTURE.md` and `music/js/composer.js`
    would otherwise mean the music change's checks never run at all — see
    orchestrate.py's module docstring for why this function exists). This
    returns the full set instead of picking one.

    Same fail-closed contract as `zone_for`: `None` for an empty tuple, for
    any path `is_no_touch` flags as suspicious, and for any path outside
    every safe zone — a file this module cannot prove safe never silently
    contributes to the answer.
    """
    if not paths:
        return None
    if is_no_touch(paths, config):
        return None

    zones: list[str] = []
    for path in paths:
        p = _normalise(path)
        zone = next((z for z in config.safe_zones if p.startswith(z)), None)
        if zone is None:
            return None
        if zone not in zones:
            zones.append(zone)
    return tuple(sorted(zones))


def risk_keys_for(paths: tuple[str, ...]) -> tuple[str, ...]:
    """Which risk weights apply to a change touching these paths.

    In practice every caller (`decide.score_one`) only reaches this after
    `zone_for` has already accepted the candidate's paths, which means every
    element is already a well-formed string — `zone_for` fails the whole
    candidate closed before this function ever sees a suspicious or
    non-string element. But this function is public and callable on its own
    (directly, in tests, or by a future caller that does not route through
    `zone_for` first), so it must not assume that and must not crash on a
    `None`, an `int`, or any other non-string element: `.startswith` inside
    `_normalise` has no fallback for one. A non-string element simply
    contributes no risk key, the same as a path that matches none of
    `RISK_PATHS` — this function only ever adds risk, so silently skipping
    an element it cannot parse cannot cause it to *under*-count risk.
    """
    keys: list[str] = []
    for path in paths:
        if not isinstance(path, str):
            continue
        p = _normalise(path)
        for prefix, key in RISK_PATHS:
            if p.startswith(prefix) and key not in keys:
                keys.append(key)
    if len(paths) > CROSS_CUTTING_FILES and "risk_cross_cutting" not in keys:
        keys.append("risk_cross_cutting")
    return tuple(keys)
