"""Where the Forge may work, and how dangerous the work is.

Two independent questions, deliberately kept apart:

  zone_for()     may it touch these paths at all? (the leash)
  risk_keys_for() how dangerous is touching them?  (the scoring)

Both are pure functions over path strings so they can be exercised without a
filesystem.
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


def is_no_touch(paths: tuple[str, ...], config: ForgeConfig) -> bool:
    """True if ANY path falls under a no-touch prefix. No-touch always wins."""
    for path in paths:
        p = _normalise(path)
        if any(p.startswith(prefix) for prefix in config.no_touch):
            return True
    return False


def zone_for(paths: tuple[str, ...], config: ForgeConfig) -> str | None:
    """The safe zone covering EVERY path, or None if any path is outside it.

    Returns None for an empty path tuple: a candidate whose files are unknown
    cannot be proven safe, and the Forge does not work blind.
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
