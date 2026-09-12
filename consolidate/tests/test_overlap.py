from consolidate import overlap
from consolidate.overlap import analyse, find_duplicates, find_name_clashes, find_pairs, is_noise

BIG = overlap.MIN_INTERESTING_BYTES + 10


def tree(**files):
    """``tree(**{"a/b.js": "sha1"})`` -> a tree with each file at a readable size."""
    return {path: (sha, BIG) for path, sha in files.items()}


def test_an_identical_file_in_two_projects_is_found():
    trees = {"a": tree(**{"src/x.js": "s1"}), "b": tree(**{"lib/x.js": "s1"})}
    (group,) = find_duplicates(trees)
    assert group.projects == ("a", "b")
    assert group.locations == (("a", "src/x.js"), ("b", "lib/x.js"))
    assert group.wasted_bytes == BIG


def test_a_file_repeated_inside_one_project_is_not_cross_project_duplication():
    trees = {"a": tree(**{"one/x.js": "s1", "two/x.js": "s1"})}
    assert find_duplicates(trees) == ()


def test_duplicates_are_ranked_by_how_much_space_they_waste():
    trees = {
        "a": {"small.js": ("s1", 100), "large.js": ("s2", 9000)},
        "b": {"small.js": ("s1", 100), "large.js": ("s2", 9000)},
    }
    assert [g.blob for g in find_duplicates(trees)] == ["s2", "s1"]


def test_three_copies_waste_two_copies_worth():
    trees = {name: tree(**{"x.js": "s1"}) for name in "abc"}
    (group,) = find_duplicates(trees)
    assert group.wasted_bytes == BIG * 2


def test_tiny_files_are_below_the_noise_floor():
    trees = {"a": {"x.js": ("s1", 3)}, "b": {"x.js": ("s1", 3)}}
    assert find_duplicates(trees) == ()


def test_vendored_and_boilerplate_files_are_ignored():
    assert is_noise("node_modules/left-pad/index.js")
    assert is_noise("a/b/__pycache__/x.pyc")
    assert is_noise("LICENSE")
    assert is_noise(".gitignore")
    assert not is_noise("src/app.js")


def test_vendored_copies_do_not_show_up_as_your_duplication():
    trees = {
        "a": tree(**{"node_modules/dep/index.js": "s1"}),
        "b": tree(**{"node_modules/dep/index.js": "s1"}),
    }
    assert find_duplicates(trees) == ()


def test_a_shared_top_level_directory_name_is_reported():
    trees = {"a": tree(**{"game/x.js": "s1"}), "b": tree(**{"game/y.js": "s2"})}
    clashes = [c for c in find_name_clashes(trees) if c.kind == "directory"]
    assert [c.name for c in clashes] == ["game"]
    assert clashes[0].projects == ("a", "b")


def test_the_same_filename_with_different_contents_is_a_drifted_copy():
    trees = {"a": tree(**{"src/engine.js": "s1"}), "b": tree(**{"other/engine.js": "s2"})}
    files = [c for c in find_name_clashes(trees) if c.kind == "file"]
    assert [c.name for c in files] == ["engine.js"]


def test_the_same_filename_with_the_same_contents_is_not_a_drifted_copy():
    trees = {"a": tree(**{"src/engine.js": "s1"}), "b": tree(**{"other/engine.js": "s1"})}
    assert [c for c in find_name_clashes(trees) if c.kind == "file"] == []


def test_similarity_is_measured_against_the_smaller_project():
    trees = {
        "small": tree(**{"a.js": "s1", "b.js": "s2"}),
        "large": tree(**{"a.js": "s1", "b.js": "s2", "c.js": "s3", "d.js": "s4"}),
    }
    (pair,) = find_pairs(trees)
    assert pair.identical_files == 2
    assert pair.similarity == 1.0


def test_projects_with_nothing_in_common_are_not_paired():
    trees = {"a": tree(**{"a.js": "s1"}), "b": tree(**{"b.js": "s2"})}
    assert find_pairs(trees) == ()


def test_pairs_are_ranked_with_the_worst_overlap_first():
    trees = {
        "a": tree(**{"x.js": "s1", "y.js": "s2"}),
        "b": tree(**{"x.js": "s1", "y.js": "s2"}),
        "c": tree(**{"x.js": "s1", "z.js": "s9"}),
    }
    pairs = find_pairs(trees)
    assert (pairs[0].a, pairs[0].b) == ("a", "b")
    assert pairs[0].similarity > pairs[-1].similarity


def test_a_tidy_set_of_projects_reports_clean():
    trees = {"a": tree(**{"a.js": "s1"}), "b": tree(**{"b.js": "s2"})}
    assert analyse(trees).is_clean


def test_the_full_report_serialises():
    trees = {"a": tree(**{"x.js": "s1"}), "b": tree(**{"x.js": "s1"})}
    data = analyse(trees).to_dict()
    assert data["wasted_bytes"] == BIG
    assert data["duplicates"][0]["locations"] == [["a", "x.js"], ["b", "x.js"]]


# --------------------------------------------------------------------------
# keeping the "drifted apart" signal meaningful
# --------------------------------------------------------------------------

def test_files_every_project_owns_a_copy_of_are_structural():
    for name in ("README.md", "index.html", "__init__.py", "pyproject.toml", "CMakeLists.txt", "style.css"):
        assert overlap.is_structural(name), name


def test_a_projects_own_test_for_a_structural_file_is_also_structural():
    assert overlap.is_structural("test_cli.py")
    assert overlap.is_structural("test_config.py")


def test_a_distinctive_name_is_not_structural():
    for name in ("generator.js", "lexicon.js", "pathfinding.cpp"):
        assert not overlap.is_structural(name), name


def test_two_projects_having_their_own_readme_is_not_reported_as_drift():
    trees = {"a": tree(**{"README.md": "s1"}), "b": tree(**{"README.md": "s2"})}
    assert [c for c in find_name_clashes(trees) if c.kind == "file"] == []


def test_a_distinctive_filename_in_two_projects_is_still_reported():
    trees = {"a": tree(**{"js/generator.js": "s1"}), "b": tree(**{"js/generator.js": "s2"})}
    files = [c for c in find_name_clashes(trees) if c.kind == "file"]
    assert [c.name for c in files] == ["generator.js"]


def test_identical_structural_files_are_still_real_duplicates():
    """A README that is byte-identical in two projects really is one copy too many."""
    trees = {"a": tree(**{"README.md": "s1"}), "b": tree(**{"README.md": "s1"})}
    assert len(find_duplicates(trees)) == 1
