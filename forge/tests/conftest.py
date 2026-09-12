"""Shared fixtures for the Forge's tests.

The scratch-repo helper lives here because three reproduction tests each kept
their own copy of the same list of directories, and that list has to match
`shared/exchange.json` exactly: `scripts/check-exchange.mjs` verifies that every
file the manifest declares really exists, so a project named in the manifest but
missing from the scratch copy makes the checker fail on the FIXTURE, for reasons
that have nothing to do with the breakage the test is about — and the failure
arrives as a confusing assertion about some unrelated message.

Each copy carried a comment warning that publishing a new capability meant
editing the list here too. That warning was correct and was still missed, twice:
once when ZOMBOID began consuming `music/soundtrack`, and again when NAME FORGE
and NEON CELLS published theirs. So the list is no longer written out — it is
derived from the manifest itself, and a new publisher or consumer needs no edit
here at all.
"""

from __future__ import annotations

import json
import shutil
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]

# Always present regardless of the manifest: `shared/` holds the manifest and the
# project list, `scripts/` holds the checkers that read them.
INFRASTRUCTURE = ("shared", "scripts")


def manifest_project_dirs(repo_root: Path = REPO_ROOT) -> tuple[str, ...]:
    """Every top-level directory `shared/exchange.json` refers to, in sorted order.

    Covers both sides of the manifest: the files a capability publishes, and the
    page and contract test of each declared consumer.
    """
    manifest = json.loads((repo_root / "shared" / "exchange.json").read_text(encoding="utf-8"))
    referenced: set[str] = set()

    for entry in manifest.get("publishes", {}).values():
        for rel in entry.get("files", ()):
            referenced.add(Path(rel).parts[0])
    for entry in manifest.get("consumes", ()):
        for key in ("page", "contract"):
            rel = entry.get(key)
            if rel:
                referenced.add(Path(rel).parts[0])

    referenced.difference_update(INFRASTRUCTURE)
    return tuple(sorted(referenced))


def scratch_repo_copy(tmp_path: Path, extra: tuple[str, ...] = ()) -> Path:
    """A scratch copy of the real repo directories the exchange gate needs.

    Copied straight off disk rather than reconstructed by hand, so a test using
    it exercises the real declarations, the real consumer pages and the real
    contract tests. `extra` adds directories a particular test needs beyond the
    manifest's own (film's suites, say, which the manifest names only as a
    contract path).
    """
    root = tmp_path / "repo"
    for rel in INFRASTRUCTURE + manifest_project_dirs() + extra:
        source = REPO_ROOT / rel
        if source.is_dir():
            shutil.copytree(source, root / rel, dirs_exist_ok=True)
    return root


@pytest.fixture
def scratch_repo(tmp_path: Path) -> Path:
    """`scratch_repo_copy` as a fixture, for tests that need no extras."""
    return scratch_repo_copy(tmp_path)
