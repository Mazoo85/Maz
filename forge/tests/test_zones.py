"""Zone enforcement: safe zones admit, no-touch always wins."""

from forge.config import ForgeConfig, load_config
from forge.models import Candidate
from forge.zones import is_no_touch, risk_keys_for, zone_for, zones_for_files


def test_path_inside_safe_zone_returns_zone():
    cfg = ForgeConfig()
    assert zone_for(("docs/ROADMAP.md",), cfg) == "docs/"


def test_path_outside_safe_zones_returns_none():
    cfg = ForgeConfig()
    assert zone_for(("engine/src/render/vk.cpp",), cfg) is None


def test_mixed_paths_reject_when_any_is_outside():
    cfg = ForgeConfig()
    assert zone_for(("docs/a.md", "engine/src/b.cpp"), cfg) is None


def test_empty_paths_returns_none():
    # A candidate with no known paths cannot be proven safe, so it isn't.
    assert zone_for((), ForgeConfig()) is None


def test_no_touch_beats_safe_zone(tmp_path):
    # Even if a user puts forge/ in safe_zones, no_touch wins.
    (tmp_path / "forge.json").write_text('{"safe_zones": ["forge/", "docs/"]}')
    cfg = load_config(root=tmp_path)
    assert is_no_touch(("forge/decide.py",), cfg) is True
    assert zone_for(("forge/decide.py",), cfg) is None
    assert zone_for(("docs/x.md",), cfg) == "docs/"


def test_workflows_are_no_touch():
    assert is_no_touch((".github/workflows/ci.yml",), ForgeConfig()) is True


def test_risk_keys_for_build_system():
    assert "risk_build_system" in risk_keys_for(("CMakeLists.txt",))
    assert "risk_engine_core" in risk_keys_for(("engine/src/core/app.cpp",))
    assert risk_keys_for(("docs/a.md",)) == ()


def test_risk_keys_for_cross_cutting():
    many = tuple(f"music/js/f{i}.js" for i in range(7))
    assert "risk_cross_cutting" in risk_keys_for(many)


def test_candidate_key_is_stable():
    c1 = Candidate(task="Add clang-tidy", source="roadmap:phase-0", kind="roadmap")
    c2 = Candidate(task="Add clang-tidy", source="roadmap:phase-0", kind="roadmap", detail="x")
    assert c1.key() == c2.key()


# --- Critical: ".." traversal must not defeat the leash ------------------


def test_dotdot_traversal_into_forge_is_no_touch_and_no_zone():
    cfg = ForgeConfig()
    path = "docs/../forge/decide.py"
    assert is_no_touch((path,), cfg) is True
    assert zone_for((path,), cfg) is None


def test_dotdot_traversal_into_workflows_is_no_touch_and_no_zone():
    cfg = ForgeConfig()
    path = "docs/../.github/workflows/x.yml"
    assert is_no_touch((path,), cfg) is True
    assert zone_for((path,), cfg) is None


# --- Minor: unparseable path shapes must fail closed too ------------------


def test_absolute_path_is_no_touch_and_no_zone():
    cfg = ForgeConfig()
    assert is_no_touch(("/forge/decide.py",), cfg) is True
    assert zone_for(("/forge/decide.py",), cfg) is None


def test_backslash_path_is_no_touch_and_no_zone():
    cfg = ForgeConfig()
    assert is_no_touch(("forge\\decide.py",), cfg) is True
    assert zone_for(("forge\\decide.py",), cfg) is None


def test_empty_and_whitespace_paths_are_no_touch_and_no_zone():
    cfg = ForgeConfig()
    assert is_no_touch(("",), cfg) is True
    assert zone_for(("",), cfg) is None
    assert is_no_touch(("   ",), cfg) is True
    assert zone_for(("   ",), cfg) is None


def test_dotslash_prefix_is_still_caught_as_no_touch():
    # The existing "./" strip must not regress: this is a no-touch hit, not
    # a suspicious-path rejection.
    cfg = ForgeConfig()
    assert is_no_touch(("./forge/x.py",), cfg) is True
    assert zone_for(("./forge/x.py",), cfg) is None


def test_dots_in_ordinary_filenames_are_not_treated_as_traversal():
    # ".." must only be caught as a whole path *segment*; a filename that
    # merely contains two dots is ordinary and must still be admitted.
    cfg = ForgeConfig()
    assert is_no_touch(("docs/a..b.md",), cfg) is False
    assert zone_for(("docs/a..b.md",), cfg) == "docs/"
    assert is_no_touch(("docs/v1.2..3/notes.md",), cfg) is False
    assert zone_for(("docs/v1.2..3/notes.md",), cfg) == "docs/"


# --- Fifth instance: a non-string element is the most unparseable shape ---
#
# `is_no_touch`/`zone_for` already fail closed on path *shapes* they cannot
# reason about (".." traversal, absolute paths, backslashes, empty strings).
# They never checked that each element *is* a string, so `_normalise`'s
# `.startswith("./")` and `_is_suspicious`'s `.split("/")` crash on anything
# else. This is the guard-on-a-container-not-its-elements bug, fifth time.


NON_STRING_ELEMENTS = (None, 123, {"a": 1}, ["x"], b"docs/x.md")


def test_non_string_elements_are_no_touch_and_no_zone():
    cfg = ForgeConfig()
    for bad in NON_STRING_ELEMENTS:
        assert is_no_touch((bad,), cfg) is True, f"is_no_touch should reject {bad!r}"
        assert zone_for((bad,), cfg) is None, f"zone_for should reject {bad!r}"


def test_mixed_tuple_with_one_valid_path_and_one_non_string_is_rejected():
    # The case that matters most: a guard that only checked the first
    # element would pass this (the valid path leads) and still be wrong.
    cfg = ForgeConfig()
    for bad in NON_STRING_ELEMENTS:
        paths = ("docs/x.md", bad)
        assert is_no_touch(paths, cfg) is True, f"is_no_touch should reject {paths!r}"
        assert zone_for(paths, cfg) is None, f"zone_for should reject {paths!r}"

        # And order must not matter either.
        paths_reversed = (bad, "docs/x.md")
        assert is_no_touch(paths_reversed, cfg) is True
        assert zone_for(paths_reversed, cfg) is None


def test_risk_keys_for_does_not_raise_on_non_string_elements():
    for bad in NON_STRING_ELEMENTS:
        risk_keys_for((bad,))
        risk_keys_for(("docs/x.md", bad))
        risk_keys_for((bad, "CMakeLists.txt"))


def test_valid_path_still_resolves_to_its_zone_after_the_fix():
    # Pin that the non-string guard is not over-broad: an ordinary valid
    # path must behave exactly as before.
    cfg = ForgeConfig()
    assert zone_for(("docs/x.md",), cfg) == "docs/"
    assert is_no_touch(("docs/x.md",), cfg) is False


# --- zones_for_files: the full set of zones touched, not just the broadest -


def test_zones_for_files_reports_every_zone_touched():
    # zone_for collapses this to "docs/" (the broadest single zone) — the
    # right answer for the ledger, the wrong one for deciding what to
    # verify. zones_for_files must report both.
    cfg = ForgeConfig()
    files = ("docs/ARCHITECTURE.md", "music/js/composer.js")
    assert zone_for(files, cfg) == "docs/"
    assert zones_for_files(files, cfg) == ("docs/", "music/")


def test_zones_for_files_dedupes_multiple_files_in_the_same_zone():
    cfg = ForgeConfig()
    files = ("music/js/a.js", "music/js/b.js")
    assert zones_for_files(files, cfg) == ("music/",)


def test_zones_for_files_matches_zone_for_for_a_single_zone_change():
    cfg = ForgeConfig()
    assert zones_for_files(("docs/a.md",), cfg) == ("docs/",)


def test_zones_for_files_none_for_empty_paths():
    assert zones_for_files((), ForgeConfig()) is None


def test_zones_for_files_none_when_any_path_is_outside_every_zone():
    cfg = ForgeConfig()
    assert zones_for_files(("docs/a.md", "engine/src/b.cpp"), cfg) is None


def test_zones_for_files_none_on_no_touch_path(tmp_path):
    # forge/ is not a safe zone by default, so it would fail the plain
    # zone-match check on its own — put it in safe_zones (as
    # test_no_touch_beats_safe_zone does above) so this actually pins
    # is_no_touch's own veto, not just "unmatched zone".
    (tmp_path / "forge.json").write_text('{"safe_zones": ["forge/", "docs/"]}')
    cfg = load_config(root=tmp_path)
    assert zones_for_files(("docs/a.md", "forge/decide.py"), cfg) is None


def test_zones_for_files_none_on_suspicious_path():
    cfg = ForgeConfig()
    assert zones_for_files(("docs/../forge/decide.py",), cfg) is None
    assert zones_for_files((None,), cfg) is None
