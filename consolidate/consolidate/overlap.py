"""Find the work you are doing twice.

Consolidating repos into one directory tree is only half the job. The half that
actually saves you time is noticing that two of those repos contain the same
file, the same project name, or largely the same program — so you can delete
one instead of maintaining both.

Detection is exact where it can be: git stores every file under a hash of its
contents, so two files sharing a blob sha are byte-for-byte identical. No
guessing and no reading file contents.
"""

from __future__ import annotations

import posixpath
from dataclasses import dataclass

#: Paths that are duplicated across every repo by design and mean nothing.
NOISE_BASENAMES = frozenset(
    {".gitignore", ".gitattributes", ".editorconfig", "LICENSE", "LICENSE.md", "LICENSE.txt", "py.typed"}
)
#: Directories that hold other people's code, not yours.
VENDOR_DIRS = frozenset({"node_modules", "vendor", "third_party", "dist", "build", ".venv", "__pycache__"})

#: A file this small is usually boilerplate; too small to be worth reporting.
MIN_INTERESTING_BYTES = 64

#: Filenames every project is *supposed* to have its own version of. Two projects
#: both having a README.md with different contents is how things should be, not a
#: sign of duplicated work, so these never count as copies that drifted apart.
#: (They are still compared for byte-identical duplicates, which does mean
#: something.)
STRUCTURAL_NAMES = frozenset(
    {
        "readme", "readme.md", "readme.txt", "changelog.md", "contributing.md",
        "index.html", "index.js", "index.ts", "index.css", "main.js", "main.ts",
        "main.py", "main.cpp", "main.c", "app.js", "app.ts", "app.py", "style.css",
        "styles.css", "__init__.py", "__main__.py", "cli.py", "config.py",
        "settings.py", "models.py", "utils.py", "conftest.py", "setup.py",
        "pyproject.toml", "setup.cfg", "requirements.txt", "package.json",
        "package-lock.json", "tsconfig.json", "cmakelists.txt", "makefile",
        "dockerfile", "go.mod", "cargo.toml",
    }
)


def is_structural(basename: str) -> bool:
    """True for a filename that every project legitimately has its own copy of."""
    lowered = basename.lower()
    if lowered in STRUCTURAL_NAMES:
        return True
    # A project's own tests for its own cli/config/etc. are equally expected.
    stem = lowered.rsplit(".", 1)[0]
    if stem.startswith("test_") and f"{stem[5:]}.py" in STRUCTURAL_NAMES:
        return True
    return False

#: A ``Tree`` is what :func:`gitops.ls_tree` returns: path -> (blob sha, size).
Tree = dict[str, tuple[str, int]]


def is_noise(path: str) -> bool:
    """True for files that are duplicated everywhere and carry no signal."""
    parts = path.split("/")
    if any(part in VENDOR_DIRS for part in parts):
        return True
    return posixpath.basename(path) in NOISE_BASENAMES


@dataclass(frozen=True)
class DuplicateGroup:
    """One file's contents, found in more than one project."""

    blob: str
    size: int
    locations: tuple[tuple[str, str], ...]  # (project, path)

    @property
    def projects(self) -> tuple[str, ...]:
        return tuple(sorted({project for project, _ in self.locations}))

    @property
    def wasted_bytes(self) -> int:
        """What you would get back by keeping one copy instead of all of them."""
        return self.size * (len(self.locations) - 1)

    def to_dict(self) -> dict:
        return {
            "blob": self.blob,
            "size": self.size,
            "locations": [list(loc) for loc in self.locations],
            "wasted_bytes": self.wasted_bytes,
        }


@dataclass(frozen=True)
class NameClash:
    """The same name in two projects, holding *different* things.

    This is the dangerous one: two copies that have drifted apart, where a fix
    applied to one never reaches the other.
    """

    name: str
    kind: str  # "file" or "directory"
    locations: tuple[tuple[str, str], ...]

    @property
    def projects(self) -> tuple[str, ...]:
        return tuple(sorted({project for project, _ in self.locations}))

    def to_dict(self) -> dict:
        return {"name": self.name, "kind": self.kind, "locations": [list(loc) for loc in self.locations]}


@dataclass(frozen=True)
class ProjectPair:
    """How much two projects have in common, as a number you can sort by."""

    a: str
    b: str
    identical_files: int
    shared_bytes: int
    similarity: float  # 0.0 .. 1.0, share of the smaller project that is duplicated

    def to_dict(self) -> dict:
        return {
            "a": self.a,
            "b": self.b,
            "identical_files": self.identical_files,
            "shared_bytes": self.shared_bytes,
            "similarity": round(self.similarity, 4),
        }


@dataclass(frozen=True)
class OverlapReport:
    duplicates: tuple[DuplicateGroup, ...] = ()
    name_clashes: tuple[NameClash, ...] = ()
    pairs: tuple[ProjectPair, ...] = ()

    @property
    def wasted_bytes(self) -> int:
        return sum(group.wasted_bytes for group in self.duplicates)

    @property
    def is_clean(self) -> bool:
        return not self.duplicates and not self.name_clashes

    def to_dict(self) -> dict:
        return {
            "duplicates": [d.to_dict() for d in self.duplicates],
            "name_clashes": [n.to_dict() for n in self.name_clashes],
            "pairs": [p.to_dict() for p in self.pairs],
            "wasted_bytes": self.wasted_bytes,
        }


def _clean(tree: Tree, *, min_bytes: int) -> Tree:
    return {
        path: (sha, size)
        for path, (sha, size) in tree.items()
        if size >= min_bytes and not is_noise(path)
    }


def find_duplicates(
    trees: dict[str, Tree], *, min_bytes: int = MIN_INTERESTING_BYTES
) -> tuple[DuplicateGroup, ...]:
    """Files that are byte-identical in two or more projects, biggest waste first."""
    by_blob: dict[str, list[tuple[str, str]]] = {}
    sizes: dict[str, int] = {}
    for project, tree in trees.items():
        for path, (sha, size) in _clean(tree, min_bytes=min_bytes).items():
            by_blob.setdefault(sha, []).append((project, path))
            sizes[sha] = size

    groups = []
    for sha, locations in by_blob.items():
        if len({project for project, _ in locations}) < 2:
            continue
        groups.append(
            DuplicateGroup(blob=sha, size=sizes[sha], locations=tuple(sorted(locations)))
        )
    return tuple(sorted(groups, key=lambda g: (-g.wasted_bytes, g.blob)))


def find_name_clashes(trees: dict[str, Tree]) -> tuple[NameClash, ...]:
    """Same top-level directory name, or same filename with different contents."""
    clashes: list[NameClash] = []

    # Top-level directories sharing a name across projects — two versions of
    # "the same program" usually announce themselves exactly like this.
    dirs: dict[str, set[str]] = {}
    for project, tree in trees.items():
        for path in tree:
            head, sep, _ = path.partition("/")
            if sep:
                dirs.setdefault(head, set()).add(project)
    for name, projects in sorted(dirs.items()):
        if len(projects) > 1:
            clashes.append(
                NameClash(
                    name=name,
                    kind="directory",
                    locations=tuple(sorted((project, name) for project in projects)),
                )
            )

    # Same filename, different contents: copies that have drifted apart.
    by_name: dict[str, list[tuple[str, str, str]]] = {}
    for project, tree in trees.items():
        for path, (sha, size) in _clean(tree, min_bytes=MIN_INTERESTING_BYTES).items():
            by_name.setdefault(posixpath.basename(path), []).append((project, path, sha))
    for name, entries in sorted(by_name.items()):
        if is_structural(name):
            continue
        projects = {project for project, _, _ in entries}
        shas = {sha for _, _, sha in entries}
        if len(projects) > 1 and len(shas) > 1:
            clashes.append(
                NameClash(
                    name=name,
                    kind="file",
                    locations=tuple(sorted((project, path) for project, path, _ in entries)),
                )
            )
    return tuple(clashes)


def find_pairs(
    trees: dict[str, Tree], *, min_bytes: int = MIN_INTERESTING_BYTES
) -> tuple[ProjectPair, ...]:
    """Rank every pair of projects by how much of the smaller one is duplicated."""
    cleaned = {project: _clean(tree, min_bytes=min_bytes) for project, tree in trees.items()}
    blobs = {
        project: {sha for sha, _ in tree.values()} for project, tree in cleaned.items()
    }
    sizes = {
        project: {sha: size for sha, size in tree.values()} for project, tree in cleaned.items()
    }

    pairs = []
    names = sorted(cleaned)
    for i, a in enumerate(names):
        for b in names[i + 1 :]:
            shared = blobs[a] & blobs[b]
            if not shared:
                continue
            smaller = min(len(blobs[a]), len(blobs[b])) or 1
            pairs.append(
                ProjectPair(
                    a=a,
                    b=b,
                    identical_files=len(shared),
                    shared_bytes=sum(sizes[a][sha] for sha in shared),
                    similarity=len(shared) / smaller,
                )
            )
    return tuple(sorted(pairs, key=lambda p: (-p.similarity, -p.identical_files, p.a, p.b)))


def analyse(trees: dict[str, Tree], *, min_bytes: int = MIN_INTERESTING_BYTES) -> OverlapReport:
    """The whole overlap picture for a set of project trees."""
    return OverlapReport(
        duplicates=find_duplicates(trees, min_bytes=min_bytes),
        name_clashes=find_name_clashes(trees),
        pairs=find_pairs(trees, min_bytes=min_bytes),
    )
